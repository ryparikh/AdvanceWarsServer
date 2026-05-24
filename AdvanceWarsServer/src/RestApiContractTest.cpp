#include "RestGameService.h"

#include <fstream>
#include <iostream>
#include <string>

namespace {
bool Expect(bool condition, const std::string& message) {
	if (!condition) {
		std::cerr << "REST API contract test failed: " << message << std::endl;
	}
	return condition;
}

json StandardCreatePayload(const std::string& mapId = "lefty") {
	return {
		{ "mapId", mapId },
		{ "players", json::array({
			{
				{ "co", "andy" },
				{ "armyType", "orange-star" },
			},
			{
				{ "co", "adder" },
				{ "armyType", "blue-moon" },
			},
		}) },
		{ "seed", 8675309 },
	};
}

bool RunCreateGetAndLegalActionContract() {
	RestGameService service;
	ApiResponse create = service.CreateGame(StandardCreatePayload("mcts").dump());
	if (!Expect(create.status == 201, "create should return 201")) {
		return false;
	}
	if (!Expect(create.headers.at("Location") == "/games/" + create.body.at("gameId").get<std::string>(), "create should set Location")) {
		return false;
	}
	if (!Expect(create.body.contains("settings"), "create should return resolved settings")) {
		return false;
	}
	if (!Expect(create.body.at("settings").at("mode") == "standard", "settings mode should be standard")) {
		return false;
	}
	if (!Expect(!create.body.contains("combat-rng-seed"), "create should not expose combat RNG seed")) {
		return false;
	}
	if (!Expect(create.body.contains("terminalReason") && create.body.at("terminalReason").is_null(), "active game should have null terminalReason")) {
		return false;
	}

	const std::string gameId = create.body.at("gameId").get<std::string>();
	ApiResponse get = service.GetGame(gameId);
	if (!Expect(get.status == 200, "get should return 200")) {
		return false;
	}
	if (!Expect(get.body.at("gameId") == gameId, "get should return requested game")) {
		return false;
	}

	ApiResponse allActions = service.ListActions(gameId, "");
	if (!Expect(allActions.status == 200, "all legal actions should return 200")) {
		return false;
	}
	if (!Expect(allActions.body.at("gameId") == gameId, "legal action envelope should include gameId")) {
		return false;
	}
	if (!Expect(allActions.body.contains("activePlayer"), "legal action envelope should include activePlayer")) {
		return false;
	}
	if (!Expect(!allActions.body.contains("source"), "all legal actions should not include source")) {
		return false;
	}
	if (!Expect(allActions.body.at("actions").is_array() && !allActions.body.at("actions").empty(), "all legal actions should include actions")) {
		return false;
	}

	json sourceAction;
	for (const json& action : allActions.body.at("actions")) {
		if (action.contains("source")) {
			sourceAction = action;
			break;
		}
	}
	if (!Expect(!sourceAction.is_null(), "expected at least one source action")) {
		return false;
	}

	const int x = sourceAction.at("source").at(0).get<int>();
	const int y = sourceAction.at("source").at(1).get<int>();
	ApiResponse tileActions = service.ListActions(gameId, "x=" + std::to_string(x) + "&y=" + std::to_string(y));
	if (!Expect(tileActions.status == 200, "tile legal actions should return 200")) {
		return false;
	}
	if (!Expect(tileActions.body.at("source").at(0) == x && tileActions.body.at("source").at(1) == y, "tile legal action envelope should include source")) {
		return false;
	}

	return true;
}

bool RunStepAndErrorContract() {
	RestGameService service;
	ApiResponse create = service.CreateGame(StandardCreatePayload("mcts").dump());
	if (!Expect(create.status == 201, "create for step contract should return 201")) {
		return false;
	}
	const std::string gameId = create.body.at("gameId").get<std::string>();

	ApiResponse allActions = service.ListActions(gameId, "");
	json legalAction = allActions.body.at("actions").front();
	ApiResponse step = service.SubmitAction(gameId, legalAction.dump());
	if (!Expect(step.status == 200, "legal step should return 200")) {
		return false;
	}
	if (!Expect(step.body.at("gameId") == gameId, "legal step should return updated game")) {
		return false;
	}

	json illegalAction = {
		{ "type", "move-wait" },
		{ "source", json::array({ 999, 999 }) },
		{ "target", json::array({ 999, 999 }) },
	};
	ApiResponse illegal = service.SubmitAction(gameId, illegalAction.dump());
	if (!Expect(illegal.status == 422, "illegal action should return 422")) {
		return false;
	}
	if (!Expect(illegal.body.at("error").at("code") == "illegal-action", "illegal action should use stable error code")) {
		return false;
	}
	if (!Expect(illegal.body.contains("game"), "illegal action should return current game")) {
		return false;
	}

	ApiResponse unknown = service.GetGame("missing-game");
	if (!Expect(unknown.status == 404, "unknown game should return 404")) {
		return false;
	}

	ApiResponse invalidPayload = service.SubmitAction(gameId, R"({"type":"end-turn","unexpected":true})");
	if (!Expect(invalidPayload.status == 400, "unknown action field should return 400")) {
		return false;
	}

	return true;
}

bool RunTerminalContract() {
	RestGameService service;

	std::fstream filestream("test/json/misc/end-game-hqcap-player0.json", std::ios::in);
	if (!Expect(!filestream.fail(), "terminal fixture should be readable")) {
		return false;
	}

	json fixture;
	filestream >> fixture;
	GameState gameState;
	GameState::from_json(fixture.at("initial-game-state"), gameState);
	service.StoreGameForTesting(std::move(gameState));

	const std::string gameId = fixture.at("initial-game-state").at("gameId").get<std::string>();
	ApiResponse terminalStep = service.SubmitAction(gameId, fixture.at("actions").front().dump());
	if (!Expect(terminalStep.status == 200, "terminal-producing legal step should return 200")) {
		return false;
	}
	if (!Expect(terminalStep.body.at("game-over") == true, "terminal step should return terminal game")) {
		return false;
	}
	if (!Expect(terminalStep.body.at("terminalReason") == "hq-capture", "terminal step should include terminal reason")) {
		return false;
	}

	ApiResponse afterTerminal = service.SubmitAction(gameId, R"({"type":"end-turn"})");
	if (!Expect(afterTerminal.status == 409, "post-terminal action should return 409")) {
		return false;
	}
	if (!Expect(afterTerminal.body.at("error").at("code") == "terminal-state", "post-terminal action should use terminal-state code")) {
		return false;
	}
	if (!Expect(afterTerminal.body.contains("game"), "post-terminal action should return current game")) {
		return false;
	}

	return true;
}

bool RunCreateValidationContract() {
	RestGameService service;

	json unsupported = StandardCreatePayload();
	unsupported["settings"] = { { "fog", true } };
	ApiResponse unsupportedResponse = service.CreateGame(unsupported.dump());
	if (!Expect(unsupportedResponse.status == 422, "unsupported setting should return 422")) {
		return false;
	}
	if (!Expect(unsupportedResponse.body.at("error").at("code") == "unsupported-setting", "unsupported setting should use stable error code")) {
		return false;
	}

	json unknownMap = StandardCreatePayload();
	unknownMap["mapId"] = "not-a-map";
	ApiResponse unknownMapResponse = service.CreateGame(unknownMap.dump());
	if (!Expect(unknownMapResponse.status == 422, "unknown map should return 422")) {
		return false;
	}
	if (!Expect(unknownMapResponse.body.at("error").at("details").at("supportedMapIds").is_array(), "unknown map should list supported map ids")) {
		return false;
	}

	json unknownField = StandardCreatePayload();
	unknownField["mode"] = "standard";
	ApiResponse unknownFieldResponse = service.CreateGame(unknownField.dump());
	if (!Expect(unknownFieldResponse.status == 400, "unknown top-level field should return 400")) {
		return false;
	}

	return true;
}
}

int RunRestApiContractTests() {
	if (!RunCreateGetAndLegalActionContract()) {
		return 1;
	}
	if (!RunStepAndErrorContract()) {
		return 1;
	}
	if (!RunTerminalContract()) {
		return 1;
	}
	if (!RunCreateValidationContract()) {
		return 1;
	}

	std::cout << "REST API contract tests passed" << std::endl;
	return 0;
}
