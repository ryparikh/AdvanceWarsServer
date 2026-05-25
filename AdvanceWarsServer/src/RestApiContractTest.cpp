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

json SonjaHiddenHpGameState() {
	return {
		{ "activePlayer", 0 },
		{ "cap-limit", 21 },
		{ "game-over", false },
		{ "gameId", "rest-sonja-hidden-hp" },
		{ "map", json::array({
			json::array({
				{
					{ "terrain", 15 },
					{ "unit", {
						{ "ammo", 9 },
						{ "fuel", 70 },
						{ "health", 88 },
						{ "hidden", false },
						{ "moved", false },
						{ "owner", "orange-star" },
						{ "type", "tank" },
					} },
				},
				{
					{ "terrain", 15 },
					{ "unit", {
						{ "ammo", 9 },
						{ "fuel", 70 },
						{ "health", 73 },
						{ "hidden", false },
						{ "moved", false },
						{ "owner", "blue-moon" },
						{ "type", "tank" },
					} },
				},
			}),
		}) },
		{ "players", json::array({
			{
				{ "armyType", "orange-star" },
				{ "co", "Andy" },
				{ "funds", 0 },
				{ "power-meter", {
					{ "charge", 0 },
					{ "cop-stars", 3 },
					{ "scop-stars", 3 },
					{ "star-value", 9000 },
				} },
				{ "power-status", 0 },
				{ "luck-policy", 1 },
			},
			{
				{ "armyType", "blue-moon" },
				{ "co", "Sonja" },
				{ "funds", 0 },
				{ "power-meter", {
					{ "charge", 0 },
					{ "cop-stars", 3 },
					{ "scop-stars", 2 },
					{ "star-value", 9000 },
				} },
				{ "power-status", 0 },
				{ "luck-policy", 3 },
			},
		}) },
		{ "turn-count", 1 },
		{ "unit-cap", 50 },
		{ "winner", -1 },
	};
}

json OneActionBeforeEndTurnGameState() {
	json row = json::array();
	for (int x = 0; x < 8; ++x) {
		row.push_back({ { "terrain", 1 } });
	}

	row.at(0)["unit"] = {
		{ "health", 100 },
		{ "hidden", false },
		{ "moved", false },
		{ "owner", "blue-moon" },
		{ "type", "infantry" },
	};
	row.at(7)["unit"] = {
		{ "health", 100 },
		{ "hidden", false },
		{ "moved", true },
		{ "owner", "orange-star" },
		{ "type", "infantry" },
	};

	return {
		{ "activePlayer", 1 },
		{ "cap-limit", 21 },
		{ "game-over", false },
		{ "gameId", "rest-explicit-end-turn" },
		{ "map", json::array({ row }) },
		{ "players", json::array({
			{
				{ "armyType", "orange-star" },
				{ "co", "Andy" },
				{ "funds", 0 },
				{ "power-meter", {
					{ "charge", 0 },
					{ "cop-stars", 3 },
					{ "scop-stars", 3 },
					{ "star-value", 9000 },
				} },
				{ "power-status", 0 },
				{ "luck-policy", 1 },
			},
			{
				{ "armyType", "blue-moon" },
				{ "co", "Adder" },
				{ "funds", 0 },
				{ "power-meter", {
					{ "charge", 0 },
					{ "cop-stars", 2 },
					{ "scop-stars", 3 },
					{ "star-value", 9000 },
				} },
				{ "power-status", 0 },
				{ "luck-policy", 1 },
			},
		}) },
		{ "turn-count", 1 },
		{ "unit-cap", 50 },
		{ "winner", -1 },
	};
}

json LowArmyValueNonTerminalGameState() {
	json row = json::array();
	for (int x = 0; x < 7; ++x) {
		row.push_back({ { "terrain", 1 } });
	}

	row.at(0)["unit"] = {
		{ "health", 100 },
		{ "hidden", false },
		{ "moved", false },
		{ "owner", "orange-star" },
		{ "type", "infantry" },
	};
	for (int x = 1; x < 5; ++x) {
		row.at(x)["unit"] = {
			{ "health", 100 },
			{ "hidden", false },
			{ "moved", false },
			{ "owner", "blue-moon" },
			{ "type", "tank" },
		};
	}
	row.at(6) = {
		{ "terrain", 47 },
		{ "property", {
			{ "capture-points", 20 },
			{ "owner", "blue-moon" },
		} },
	};

	return {
		{ "activePlayer", 0 },
		{ "cap-limit", 21 },
		{ "game-over", false },
		{ "gameId", "rest-no-heuristic-resign" },
		{ "map", json::array({ row }) },
		{ "players", json::array({
			{
				{ "armyType", "orange-star" },
				{ "co", "Andy" },
				{ "funds", 0 },
				{ "power-meter", {
					{ "charge", 0 },
					{ "cop-stars", 3 },
					{ "scop-stars", 3 },
					{ "star-value", 9000 },
				} },
				{ "power-status", 0 },
				{ "luck-policy", 1 },
			},
			{
				{ "armyType", "blue-moon" },
				{ "co", "Adder" },
				{ "funds", 0 },
				{ "power-meter", {
					{ "charge", 0 },
					{ "cop-stars", 2 },
					{ "scop-stars", 3 },
					{ "star-value", 9000 },
				} },
				{ "power-status", 0 },
				{ "luck-policy", 1 },
			},
		}) },
		{ "turn-count", 5 },
		{ "unit-cap", 50 },
		{ "winner", -1 },
	};
}

json InvalidActionRejectionGameState() {
	json map = json::array({
		json::array({
			{
				{ "terrain", 15 },
				{ "unit", {
					{ "ammo", 0 },
					{ "fuel", 99 },
					{ "health", 100 },
					{ "hidden", false },
					{ "moved", false },
					{ "owner", "orange-star" },
					{ "type", "infantry" },
				} },
			},
			{ { "terrain", 15 } },
			{ { "terrain", 15 } },
			{ { "terrain", 15 } },
			{ { "terrain", 15 } },
		}),
		json::array({
			{
				{ "terrain", 15 },
				{ "unit", {
					{ "ammo", 9 },
					{ "fuel", 50 },
					{ "health", 100 },
					{ "hidden", false },
					{ "moved", false },
					{ "owner", "orange-star" },
					{ "type", "artillery" },
				} },
			},
			{ { "terrain", 15 } },
			{
				{ "terrain", 15 },
				{ "unit", {
					{ "ammo", 0 },
					{ "fuel", 60 },
					{ "health", 100 },
					{ "hidden", false },
					{ "loaded-units", json::array({
						{
							{ "ammo", 0 },
							{ "fuel", 99 },
							{ "health", 100 },
							{ "hidden", false },
							{ "moved", true },
							{ "owner", "orange-star" },
							{ "type", "infantry" },
						},
					}) },
					{ "moved", false },
					{ "owner", "orange-star" },
					{ "type", "apc" },
				} },
			},
			{
				{ "terrain", 15 },
				{ "unit", {
					{ "ammo", 0 },
					{ "fuel", 99 },
					{ "health", 100 },
					{ "hidden", false },
					{ "moved", false },
					{ "owner", "orange-star" },
					{ "type", "infantry" },
				} },
			},
			{ { "terrain", 15 } },
		}),
		json::array({
			{
				{ "terrain", 34 },
				{ "property", {
					{ "capture-points", 20 },
					{ "owner", "blue-moon" },
				} },
				{ "unit", {
					{ "ammo", 9 },
					{ "fuel", 70 },
					{ "health", 100 },
					{ "hidden", false },
					{ "moved", false },
					{ "owner", "orange-star" },
					{ "type", "tank" },
				} },
			},
			{ { "terrain", 15 } },
			{ { "terrain", 15 } },
			{ { "terrain", 15 } },
			{
				{ "terrain", 35 },
				{ "property", {
					{ "capture-points", 20 },
					{ "owner", "orange-star" },
				} },
			},
		}),
	});

	return {
		{ "activePlayer", 0 },
		{ "cap-limit", 21 },
		{ "game-over", false },
		{ "gameId", "rest-invalid-action-rejection" },
		{ "map", map },
		{ "players", json::array({
			{
				{ "armyType", "orange-star" },
				{ "co", "Andy" },
				{ "funds", 5000 },
				{ "power-meter", {
					{ "charge", 0 },
					{ "cop-stars", 3 },
					{ "scop-stars", 3 },
					{ "star-value", 9000 },
				} },
				{ "power-status", 0 },
				{ "luck-policy", 1 },
			},
			{
				{ "armyType", "blue-moon" },
				{ "co", "Adder" },
				{ "funds", 0 },
				{ "power-meter", {
					{ "charge", 0 },
					{ "cop-stars", 2 },
					{ "scop-stars", 3 },
					{ "star-value", 9000 },
				} },
				{ "power-status", 0 },
				{ "luck-policy", 1 },
			},
		}) },
		{ "turn-count", 1 },
		{ "unit-cap", 50 },
		{ "winner", -1 },
	};
}

bool ExpectIllegalActionRejectedAtomically(RestGameService& service, const std::string& gameId, const json& action, const std::string& description) {
	ApiResponse before = service.GetGame(gameId);
	if (!Expect(before.status == 200, description + " setup should be retrievable")) {
		return false;
	}

	const std::string beforeDump = before.body.dump();
	ApiResponse illegal = service.SubmitAction(gameId, action.dump());
	if (!Expect(illegal.status == 422, description + " should return 422")) {
		return false;
	}
	if (!Expect(illegal.body.at("error").at("code") == "illegal-action", description + " should use illegal-action code")) {
		return false;
	}
	if (!Expect(illegal.body.contains("game"), description + " should return the current game")) {
		return false;
	}
	if (!Expect(illegal.body.at("game").dump() == beforeDump, description + " response game should be unchanged")) {
		return false;
	}

	ApiResponse after = service.GetGame(gameId);
	if (!Expect(after.status == 200, description + " post-check should be retrievable")) {
		return false;
	}
	if (!Expect(after.body.dump() == beforeDump, description + " should not mutate stored game state")) {
		return false;
	}

	return true;
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
	if (!Expect(create.body.at("settings").at("heuristicAutoResign") == false, "heuristic auto-resign should default off")) {
		return false;
	}
	if (!Expect(!create.body.contains("combat-rng-seed"), "create should not expose combat RNG seed")) {
		return false;
	}
	if (!Expect(create.body.contains("terminalReason") && create.body.at("terminalReason").is_null(), "active game should have null terminalReason")) {
		return false;
	}
	if (!Expect(create.body.at("turn-count") == 1, "create should run the opening turn start")) {
		return false;
	}
	if (!Expect(create.body.at("players").at(0).at("funds") == 2000, "opening turn start should pay player 1 income")) {
		return false;
	}
	if (!Expect(create.body.at("players").at(1).at("funds") == 0, "opening turn start should not pay player 2 income yet")) {
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
	bool hasEndTurn = false;
	for (const json& action : allActions.body.at("actions")) {
		if (action.at("type") == "end-turn") {
			hasEndTurn = true;
			break;
		}
	}
	if (!Expect(hasEndTurn, "all legal actions should include end-turn")) {
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

bool RunSonjaHiddenHpPerspectiveContract() {
	RestGameService service;
	json fixture = SonjaHiddenHpGameState();
	GameState gameState;
	GameState::from_json(fixture, gameState);
	service.StoreGameForTesting(std::move(gameState));

	const std::string gameId = fixture.at("gameId").get<std::string>();
	ApiResponse full = service.GetGame(gameId);
	if (!Expect(full.status == 200, "full get should return 200")) {
		return false;
	}
	const json& fullSonjaUnit = full.body.at("map").at(0).at(1).at("unit");
	if (!Expect(fullSonjaUnit.at("health") == 73, "full get should expose authoritative Sonja HP")) {
		return false;
	}
	if (!Expect(!fullSonjaUnit.contains("hidden-health"), "full get should not mark hidden HP")) {
		return false;
	}

	ApiResponse attackerView = service.GetGame(gameId, "player=0");
	if (!Expect(attackerView.status == 200, "player 0 get should return 200")) {
		return false;
	}
	const json& visibleAndyUnit = attackerView.body.at("map").at(0).at(0).at("unit");
	if (!Expect(visibleAndyUnit.at("health") == 88, "player perspective should keep own HP exact")) {
		return false;
	}
	const json& hiddenSonjaUnit = attackerView.body.at("map").at(0).at(1).at("unit");
	if (!Expect(hiddenSonjaUnit.at("health").is_null(), "opponent perspective should hide Sonja HP")) {
		return false;
	}
	if (!Expect(hiddenSonjaUnit.at("hidden-health") == true, "opponent perspective should flag hidden Sonja HP")) {
		return false;
	}

	ApiResponse defenderView = service.GetGame(gameId, "player=1");
	if (!Expect(defenderView.status == 200, "player 1 get should return 200")) {
		return false;
	}
	const json& ownSonjaUnit = defenderView.body.at("map").at(0).at(1).at("unit");
	if (!Expect(ownSonjaUnit.at("health") == 73, "Sonja player should see own exact HP")) {
		return false;
	}
	if (!Expect(!ownSonjaUnit.contains("hidden-health"), "Sonja player should not see own HP as hidden")) {
		return false;
	}

	ApiResponse unsupportedPlayer = service.GetGame(gameId, "player=2");
	if (!Expect(unsupportedPlayer.status == 422, "unsupported perspective player should return 422")) {
		return false;
	}
	if (!Expect(unsupportedPlayer.body.at("error").at("code") == "invalid-player", "unsupported perspective should use invalid-player code")) {
		return false;
	}

	ApiResponse invalidQuery = service.GetGame(gameId, "viewer=0");
	if (!Expect(invalidQuery.status == 400, "unknown get query field should return 400")) {
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

bool RunInvalidActionAtomicRejectionContract() {
	RestGameService service;
	json fixture = InvalidActionRejectionGameState();
	GameState gameState;
	GameState::from_json(fixture, gameState);
	service.StoreGameForTesting(std::move(gameState));

	const std::string gameId = fixture.at("gameId").get<std::string>();

	if (!ExpectIllegalActionRejectedAtomically(service, gameId, {
		{ "type", "move-wait" },
		{ "source", json::array({ 0, 0 }) },
		{ "target", json::array({ 4, 0 }) },
	}, "invalid move")) {
		return false;
	}

	if (!ExpectIllegalActionRejectedAtomically(service, gameId, {
		{ "type", "attack" },
		{ "source", json::array({ 0, 1 }) },
		{ "target", json::array({ 1, 1 }) },
	}, "invalid attack")) {
		return false;
	}

	if (!ExpectIllegalActionRejectedAtomically(service, gameId, {
		{ "type", "unload" },
		{ "source", json::array({ 2, 1 }) },
		{ "direction", "east" },
		{ "unloadIndex", 0 },
	}, "invalid unload")) {
		return false;
	}

	if (!ExpectIllegalActionRejectedAtomically(service, gameId, {
		{ "type", "buy" },
		{ "source", json::array({ 4, 2 }) },
		{ "unit", "tank" },
	}, "invalid buy")) {
		return false;
	}

	if (!ExpectIllegalActionRejectedAtomically(service, gameId, {
		{ "type", "move-capture" },
		{ "source", json::array({ 0, 2 }) },
		{ "target", json::array({ 0, 2 }) },
	}, "invalid capture")) {
		return false;
	}

	if (!ExpectIllegalActionRejectedAtomically(service, gameId, {
		{ "type", "co-power" },
	}, "invalid power")) {
		return false;
	}

	return true;
}

bool RunExplicitEndTurnStepContract() {
	RestGameService service;
	json fixture = OneActionBeforeEndTurnGameState();
	GameState gameState;
	GameState::from_json(fixture, gameState);
	service.StoreGameForTesting(std::move(gameState));

	const std::string gameId = fixture.at("gameId").get<std::string>();
	json lastUnitAction = {
		{ "type", "move-wait" },
		{ "source", json::array({ 0, 0 }) },
		{ "target", json::array({ 0, 0 }) },
	};
	ApiResponse lastUnitStep = service.SubmitAction(gameId, lastUnitAction.dump());
	if (!Expect(lastUnitStep.status == 200, "last unit step should return 200")) {
		return false;
	}
	if (!Expect(lastUnitStep.body.at("activePlayer") == 1, "last unit step should not implicitly end player 2 turn")) {
		return false;
	}
	if (!Expect(lastUnitStep.body.at("turn-count") == 1, "last unit step should not implicitly advance the day")) {
		return false;
	}

	ApiResponse onlyEndTurnActions = service.ListActions(gameId, "");
	if (!Expect(onlyEndTurnActions.status == 200, "end-turn-only legal actions should return 200")) {
		return false;
	}
	if (!Expect(onlyEndTurnActions.body.at("activePlayer") == 1, "end-turn-only legal actions should keep active player")) {
		return false;
	}
	if (!Expect(onlyEndTurnActions.body.at("actions").is_array() && onlyEndTurnActions.body.at("actions").size() == 1, "legal actions should contain only end-turn")) {
		return false;
	}
	if (!Expect(onlyEndTurnActions.body.at("actions").front().at("type") == "end-turn", "only legal action should be end-turn")) {
		return false;
	}

	ApiResponse afterList = service.GetGame(gameId);
	if (!Expect(afterList.status == 200, "get after end-turn-only legal actions should return 200")) {
		return false;
	}
	if (!Expect(afterList.body.at("activePlayer") == 1, "listing end-turn-only actions should not advance active player")) {
		return false;
	}
	if (!Expect(afterList.body.at("turn-count") == 1, "listing end-turn-only actions should not advance the day")) {
		return false;
	}

	ApiResponse explicitEndTurn = service.SubmitAction(gameId, R"({"type":"end-turn"})");
	if (!Expect(explicitEndTurn.status == 200, "explicit end-turn should return 200")) {
		return false;
	}
	if (!Expect(explicitEndTurn.body.at("activePlayer") == 0, "explicit end-turn should advance to player 1")) {
		return false;
	}
	if (!Expect(explicitEndTurn.body.at("turn-count") == 2, "explicit player 2 end-turn should advance the day")) {
		return false;
	}

	return true;
}

bool RunNoHeuristicResignContract() {
	json waitAction = {
		{ "type", "move-wait" },
		{ "source", json::array({ 0, 0 }) },
		{ "target", json::array({ 0, 0 }) },
	};

	RestGameService service;
	json fixture = LowArmyValueNonTerminalGameState();
	GameState gameState;
	GameState::from_json(fixture, gameState);
	service.StoreGameForTesting(std::move(gameState));

	const std::string gameId = fixture.at("gameId").get<std::string>();
	ApiResponse step = service.SubmitAction(gameId, waitAction.dump());
	if (!Expect(step.status == 200, "low army value legal step should return 200")) {
		return false;
	}
	if (!Expect(step.body.at("game-over") == false, "low army value alone should not end a standard game")) {
		return false;
	}
	if (!Expect(step.body.at("winner") == -1, "low army value alone should not choose a winner")) {
		return false;
	}
	if (!Expect(step.body.contains("terminalReason") && step.body.at("terminalReason").is_null(), "low army value alone should keep null terminalReason")) {
		return false;
	}

	RestGameService optInService;
	json optInFixture = LowArmyValueNonTerminalGameState();
	GameState optInGameState;
	GameState::from_json(optInFixture, optInGameState);
	optInGameState.SetHeuristicAutoResign(true);
	optInService.StoreGameForTesting(std::move(optInGameState));

	const std::string optInGameId = optInFixture.at("gameId").get<std::string>();
	ApiResponse optInGet = optInService.GetGame(optInGameId);
	if (!Expect(optInGet.status == 200, "opt-in heuristic game should be retrievable")) {
		return false;
	}
	if (!Expect(optInGet.body.at("settings").at("heuristicAutoResign") == true, "opt-in heuristic setting should serialize as enabled")) {
		return false;
	}

	ApiResponse optInStep = optInService.SubmitAction(optInGameId, waitAction.dump());
	if (!Expect(optInStep.status == 200, "opt-in heuristic legal step should return 200")) {
		return false;
	}
	if (!Expect(optInStep.body.at("game-over") == true, "opt-in heuristic should be able to end a game")) {
		return false;
	}
	if (!Expect(optInStep.body.at("winner") == 1, "opt-in heuristic should choose the stronger player")) {
		return false;
	}
	if (!Expect(optInStep.body.at("terminalReason") == "heuristic-resign", "opt-in heuristic should report heuristic-resign")) {
		return false;
	}

	json createPayload = StandardCreatePayload("mcts");
	createPayload["settings"] = { { "heuristicAutoResign", true } };
	ApiResponse create = optInService.CreateGame(createPayload.dump());
	if (!Expect(create.status == 201, "create should accept heuristicAutoResign setting")) {
		return false;
	}
	if (!Expect(create.body.at("settings").at("heuristicAutoResign") == true, "create should return resolved opt-in heuristic setting")) {
		return false;
	}

	return true;
}

bool RunCreateConfiguredStandardSettingsContract() {
	RestGameService service;
	json createPayload = StandardCreatePayload("mcts");
	createPayload["settings"] = {
		{ "unitCap", 7 },
		{ "captureLimit", 4 },
		{ "dayLimit", 2 },
	};

	ApiResponse create = service.CreateGame(createPayload.dump());
	if (!Expect(create.status == 201, "create should accept configured Standard limits")) {
		return false;
	}
	if (!Expect(create.body.at("settings").at("unitCap") == 7, "create should return configured unit cap")) {
		return false;
	}
	if (!Expect(create.body.at("settings").at("captureLimit") == 4, "create should return configured capture limit")) {
		return false;
	}
	if (!Expect(create.body.at("settings").at("dayLimit") == 2, "create should return configured day limit")) {
		return false;
	}
	if (!Expect(create.body.at("unit-cap") == 7, "create should mirror configured unit cap in legacy field")) {
		return false;
	}
	if (!Expect(create.body.at("cap-limit") == 4, "create should mirror configured capture limit in legacy field")) {
		return false;
	}

	return true;
}

bool RunGameSettingsModelContract() {
	json fixture = OneActionBeforeEndTurnGameState();
	GameState gameState;
	GameState::from_json(fixture, gameState);

	json serialized;
	GameState::to_json(serialized, gameState);
	if (!Expect(serialized.contains("settings"), "serialized GameState should include resolved settings")) {
		return false;
	}

	const json& settings = serialized.at("settings");
	if (!Expect(settings.at("mode") == "standard", "default settings mode should be standard")) {
		return false;
	}
	if (!Expect(settings.at("fog") == false, "default settings should disable fog")) {
		return false;
	}
	if (!Expect(settings.at("weather") == "clear", "default settings weather should be clear")) {
		return false;
	}
	if (!Expect(settings.at("coPowers") == true, "default settings should enable CO powers")) {
		return false;
	}
	if (!Expect(settings.at("tags") == false, "default settings should disable tags")) {
		return false;
	}
	if (!Expect(settings.at("startingFunds") == 0, "default settings should use 0 starting funds")) {
		return false;
	}
	if (!Expect(settings.at("incomePerProperty") == 1000, "default settings should use 1000 income per property")) {
		return false;
	}
	if (!Expect(settings.at("unitCap") == 50, "default settings should use unit cap 50")) {
		return false;
	}
	if (!Expect(settings.at("captureLimit") == 21, "default settings should use capture limit 21")) {
		return false;
	}
	if (!Expect(settings.contains("dayLimit") && settings.at("dayLimit").is_null(), "default settings should not configure a day limit")) {
		return false;
	}
	if (!Expect(settings.at("bannedUnits") == json::array({ "blackbomb" }), "default settings should ban Black Bomb production")) {
		return false;
	}

	json settingsOnlyFixture = OneActionBeforeEndTurnGameState();
	settingsOnlyFixture.erase("unit-cap");
	settingsOnlyFixture.erase("cap-limit");
	settingsOnlyFixture["settings"] = {
		{ "mode", "standard" },
		{ "fog", false },
		{ "weather", "clear" },
		{ "coPowers", true },
		{ "tags", false },
		{ "startingFunds", 0 },
		{ "incomePerProperty", 1000 },
		{ "unitCap", 7 },
		{ "captureLimit", 4 },
		{ "dayLimit", 3 },
		{ "bannedUnits", json::array() },
		{ "heuristicAutoResign", false },
	};

	GameState settingsOnlyState;
	try {
		GameState::from_json(settingsOnlyFixture, settingsOnlyState);
	}
	catch (const std::exception& err) {
		return Expect(false, std::string("settings-only fixture should parse: ") + err.what());
	}

	json settingsOnlySerialized;
	GameState::to_json(settingsOnlySerialized, settingsOnlyState);
	if (!Expect(settingsOnlySerialized.at("settings").at("unitCap") == 7, "settings-only fixture should set unit cap")) {
		return false;
	}
	if (!Expect(settingsOnlySerialized.at("settings").at("captureLimit") == 4, "settings-only fixture should set capture limit")) {
		return false;
	}
	if (!Expect(settingsOnlySerialized.at("settings").at("dayLimit") == 3, "settings-only fixture should set day limit")) {
		return false;
	}
	if (!Expect(settingsOnlySerialized.at("settings").at("bannedUnits").empty(), "settings-only fixture should allow explicit empty unit bans")) {
		return false;
	}
	if (!Expect(settingsOnlySerialized.at("unit-cap") == 7, "legacy unit-cap should mirror settings")) {
		return false;
	}
	if (!Expect(settingsOnlySerialized.at("cap-limit") == 4, "legacy cap-limit should mirror settings")) {
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
	if (!RunSonjaHiddenHpPerspectiveContract()) {
		return 1;
	}
	if (!RunStepAndErrorContract()) {
		return 1;
	}
	if (!RunInvalidActionAtomicRejectionContract()) {
		return 1;
	}
	if (!RunExplicitEndTurnStepContract()) {
		return 1;
	}
	if (!RunNoHeuristicResignContract()) {
		return 1;
	}
	if (!RunCreateConfiguredStandardSettingsContract()) {
		return 1;
	}
	if (!RunGameSettingsModelContract()) {
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
