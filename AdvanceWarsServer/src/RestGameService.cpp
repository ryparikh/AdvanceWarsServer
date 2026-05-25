#include "RestGameService.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <vector>

#include "Platform.h"

namespace {
constexpr int HttpOk = 200;
constexpr int HttpCreated = 201;
constexpr int HttpBadRequest = 400;
constexpr int HttpNotFound = 404;
constexpr int HttpConflict = 409;
constexpr int HttpUnprocessableEntity = 422;
constexpr int HttpInternalServerError = 500;

struct MapTemplate {
	const char* id;
	const char* path;
};

constexpr std::array<MapTemplate, 2> MapTemplates{ {
	{ "lefty", "res/AWBW/MapSources/Lefty.json" },
	{ "mcts", "res/AWBW/MapSources/MCTS.json" },
} };

json StandardSettingsJson(int unitCap = 50, int captureLimit = 21, std::optional<int> dayLimit = std::nullopt, bool heuristicAutoResign = false) {
	json dayLimitJson = nullptr;
	if (dayLimit.has_value()) {
		dayLimitJson = dayLimit.value();
	}

	return {
		{ "mode", "standard" },
		{ "fog", false },
		{ "weather", "clear" },
		{ "coPowers", true },
		{ "tags", false },
		{ "startingFunds", 0 },
		{ "incomePerProperty", 1000 },
		{ "unitCap", unitCap },
		{ "captureLimit", captureLimit },
		{ "dayLimit", dayLimitJson },
		{ "bannedUnits", json::array({ "blackbomb" }) },
		{ "heuristicAutoResign", heuristicAutoResign },
	};
}

json SupportedMapIds() {
	json supported = json::array();
	for (const MapTemplate& mapTemplate : MapTemplates) {
		supported.push_back(mapTemplate.id);
	}
	return supported;
}

const MapTemplate* FindMapTemplate(const std::string& mapId) {
	for (const MapTemplate& mapTemplate : MapTemplates) {
		if (mapId == mapTemplate.id) {
			return &mapTemplate;
		}
	}
	return nullptr;
}

ApiResponse JsonResponse(int status, json body) {
	ApiResponse response;
	response.status = status;
	response.body = std::move(body);
	response.headers.emplace("Content-Type", "application/json");
	return response;
}

ApiResponse ErrorResponse(int status, const std::string& code, const std::string& message, json details = json::object()) {
	json body = {
		{ "error", {
			{ "code", code },
			{ "message", message },
		} },
	};
	if (!details.empty()) {
		body["error"]["details"] = std::move(details);
	}
	return JsonResponse(status, std::move(body));
}

ApiResponse ErrorResponseWithGame(int status, const std::string& code, const std::string& message, const json& game) {
	json body = {
		{ "error", {
			{ "code", code },
			{ "message", message },
		} },
		{ "game", game },
	};
	return JsonResponse(status, std::move(body));
}

bool ContainsOnlyFields(const json& object, const std::vector<std::string>& allowedFields, std::string& invalidField) {
	if (!object.is_object()) {
		invalidField.clear();
		return false;
	}

	for (auto it = object.begin(); it != object.end(); ++it) {
		if (std::find(allowedFields.begin(), allowedFields.end(), it.key()) == allowedFields.end()) {
			invalidField = it.key();
			return false;
		}
	}

	return true;
}

std::string ToPascalNoHyphen(std::string value) {
	std::string converted;
	bool capitalizeNext = true;
	for (char ch : value) {
		if (ch == '-') {
			capitalizeNext = true;
			continue;
		}

		if (capitalizeNext) {
			converted.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(ch))));
			capitalizeNext = false;
		}
		else {
			converted.push_back(ch);
		}
	}
	return converted;
}

bool TryParseCoId(const std::string& coId, CommandingOfficier& co, std::string& coJsonName) {
	coJsonName = ToPascalNoHyphen(coId);
	json coJson = coJsonName;
	try {
		from_json(coJson, co);
		return co.m_type != CommandingOfficier::Type::Invalid;
	}
	catch (const std::exception&) {
		return false;
	}
}

bool IsStandardBannedUnits(const json& bannedUnits) {
	return bannedUnits.is_array() &&
		bannedUnits.size() == 1 &&
		bannedUnits.at(0).is_string() &&
		bannedUnits.at(0).get<std::string>() == "blackbomb";
}

ApiResponse ValidateSettings(const json& request) {
	if (!request.contains("settings")) {
		return JsonResponse(HttpOk, StandardSettingsJson());
	}

	const json& settings = request.at("settings");
	if (!settings.is_object()) {
		return ErrorResponse(HttpBadRequest, "invalid-field", "settings must be an object.", { { "field", "settings" } });
	}

	std::string invalidField;
	if (!ContainsOnlyFields(settings, { "mode", "fog", "weather", "coPowers", "tags", "startingFunds", "incomePerProperty", "unitCap", "captureLimit", "dayLimit", "bannedUnits", "heuristicAutoResign" }, invalidField)) {
		return ErrorResponse(HttpBadRequest, "invalid-field", "Unknown settings field.", { { "field", "settings." + invalidField } });
	}

	int unitCap = 50;
	int captureLimit = 21;
	std::optional<int> dayLimit;
	try {
		if (settings.contains("mode") && (!settings.at("mode").is_string() || settings.at("mode").get<std::string>() != "standard")) {
			return ErrorResponse(HttpUnprocessableEntity, "unsupported-setting", "Only standard mode is supported.", { { "field", "settings.mode" }, { "value", settings.at("mode") } });
		}
		if (settings.contains("fog") && (!settings.at("fog").is_boolean() || settings.at("fog").get<bool>() != false)) {
			return ErrorResponse(HttpUnprocessableEntity, "unsupported-setting", "Fog of War is not supported for standard games yet.", { { "field", "settings.fog" }, { "value", settings.at("fog") } });
		}
		if (settings.contains("weather") && (!settings.at("weather").is_string() || settings.at("weather").get<std::string>() != "clear")) {
			return ErrorResponse(HttpUnprocessableEntity, "unsupported-setting", "Only clear weather is supported for standard games.", { { "field", "settings.weather" }, { "value", settings.at("weather") } });
		}
		if (settings.contains("coPowers") && (!settings.at("coPowers").is_boolean() || settings.at("coPowers").get<bool>() != true)) {
			return ErrorResponse(HttpUnprocessableEntity, "unsupported-setting", "CO powers must remain enabled for standard games.", { { "field", "settings.coPowers" }, { "value", settings.at("coPowers") } });
		}
		if (settings.contains("tags") && (!settings.at("tags").is_boolean() || settings.at("tags").get<bool>() != false)) {
			return ErrorResponse(HttpUnprocessableEntity, "unsupported-setting", "Tag powers are not supported for standard games yet.", { { "field", "settings.tags" }, { "value", settings.at("tags") } });
		}
		if (settings.contains("startingFunds") && (!settings.at("startingFunds").is_number_integer() || settings.at("startingFunds").get<int>() != 0)) {
			return ErrorResponse(HttpUnprocessableEntity, "unsupported-setting", "Only 0 starting funds are supported for standard games.", { { "field", "settings.startingFunds" }, { "value", settings.at("startingFunds") } });
		}
		if (settings.contains("incomePerProperty") && (!settings.at("incomePerProperty").is_number_integer() || settings.at("incomePerProperty").get<int>() != 1000)) {
			return ErrorResponse(HttpUnprocessableEntity, "unsupported-setting", "Only 1000 income per property is supported for standard games.", { { "field", "settings.incomePerProperty" }, { "value", settings.at("incomePerProperty") } });
		}
		if (settings.contains("unitCap")) {
			if (!settings.at("unitCap").is_number_integer()) {
				return ErrorResponse(HttpBadRequest, "invalid-field", "unitCap must be an integer.", { { "field", "settings.unitCap" } });
			}
			unitCap = settings.at("unitCap").get<int>();
			if (unitCap <= 0) {
				return ErrorResponse(HttpUnprocessableEntity, "unsupported-setting", "unitCap must be positive for standard games.", { { "field", "settings.unitCap" }, { "value", settings.at("unitCap") } });
			}
		}
		if (settings.contains("captureLimit")) {
			if (!settings.at("captureLimit").is_number_integer()) {
				return ErrorResponse(HttpBadRequest, "invalid-field", "captureLimit must be an integer.", { { "field", "settings.captureLimit" } });
			}
			captureLimit = settings.at("captureLimit").get<int>();
			if (captureLimit <= 0) {
				return ErrorResponse(HttpUnprocessableEntity, "unsupported-setting", "captureLimit must be positive for standard games.", { { "field", "settings.captureLimit" }, { "value", settings.at("captureLimit") } });
			}
		}
		if (settings.contains("dayLimit")) {
			if (settings.at("dayLimit").is_null()) {
				dayLimit.reset();
			}
			else if (!settings.at("dayLimit").is_number_integer()) {
				return ErrorResponse(HttpBadRequest, "invalid-field", "dayLimit must be an integer or null.", { { "field", "settings.dayLimit" } });
			}
			else {
				const int parsedDayLimit = settings.at("dayLimit").get<int>();
				if (parsedDayLimit <= 0) {
					return ErrorResponse(HttpUnprocessableEntity, "unsupported-setting", "dayLimit must be positive for standard games.", { { "field", "settings.dayLimit" }, { "value", settings.at("dayLimit") } });
				}
				dayLimit = parsedDayLimit;
			}
		}
		if (settings.contains("bannedUnits") && !IsStandardBannedUnits(settings.at("bannedUnits"))) {
			return ErrorResponse(HttpUnprocessableEntity, "unsupported-setting", "Only the standard blackbomb ban is supported.", { { "field", "settings.bannedUnits" }, { "value", settings.at("bannedUnits") } });
		}
		if (settings.contains("heuristicAutoResign") && !settings.at("heuristicAutoResign").is_boolean()) {
			return ErrorResponse(HttpBadRequest, "invalid-field", "heuristicAutoResign must be a boolean.", { { "field", "settings.heuristicAutoResign" } });
		}
	}
	catch (const std::exception&) {
		return ErrorResponse(HttpBadRequest, "invalid-field", "settings contains a value with the wrong type.", { { "field", "settings" } });
	}

	const bool heuristicAutoResign = settings.contains("heuristicAutoResign") && settings.at("heuristicAutoResign").get<bool>();
	return JsonResponse(HttpOk, StandardSettingsJson(unitCap, captureLimit, dayLimit, heuristicAutoResign));
}

json SerializeGameForApi(const GameState& gameState) {
	json game;
	GameState::to_json(game, gameState);
	game.erase("combat-rng-seed");
	game.erase("heuristic-auto-resign");
	if (gameState.GetTerminalReason().has_value()) {
		game["terminalReason"] = gameState.GetTerminalReason().value();
	}
	else {
		game["terminalReason"] = nullptr;
	}
	return game;
}

void RedactSonjaHiddenHp(json& unit, const std::string& sonjaArmyType) {
	if (!unit.is_object()) {
		return;
	}

	if (unit.contains("owner") && unit.at("owner").is_string() && unit.at("owner").get<std::string>() == sonjaArmyType) {
		unit["health"] = nullptr;
		unit["hidden-health"] = true;
	}

	if (unit.contains("loaded-units") && unit.at("loaded-units").is_array()) {
		for (json& loadedUnit : unit["loaded-units"]) {
			RedactSonjaHiddenHp(loadedUnit, sonjaArmyType);
		}
	}
}

void RedactHiddenHpForPerspective(json& game, int playerIndex) {
	const int opponentIndex = playerIndex == 0 ? 1 : 0;
	const json& opponent = game.at("players").at(opponentIndex);
	if (opponent.at("co") != "Sonja") {
		return;
	}

	const std::string sonjaArmyType = opponent.at("armyType").get<std::string>();
	for (json& row : game["map"]) {
		for (json& tile : row) {
			if (tile.contains("unit")) {
				RedactSonjaHiddenHp(tile["unit"], sonjaArmyType);
			}
		}
	}
}

ApiResponse UnknownGameResponse() {
	return ErrorResponse(HttpNotFound, "unknown-game-id", "Game id was not found.");
}

bool TryGetInteger(const std::string& value, int& parsed) {
	try {
		size_t processed = 0;
		parsed = std::stoi(value, &processed);
		return processed == value.size();
	}
	catch (const std::exception&) {
		return false;
	}
}

bool ContainsQueryField(std::initializer_list<const char*> allowedFields, const std::string& key) {
	for (const char* allowedField : allowedFields) {
		if (key == allowedField) {
			return true;
		}
	}

	return false;
}

std::map<std::string, std::string> ParseQueryString(const std::string& query, std::initializer_list<const char*> allowedFields, bool& valid) {
	valid = true;
	std::map<std::string, std::string> parsed;
	if (query.empty()) {
		return parsed;
	}

	std::stringstream stream(query);
	std::string part;
	while (std::getline(stream, part, '&')) {
		const size_t equals = part.find('=');
		if (equals == std::string::npos) {
			valid = false;
			return parsed;
		}

		std::string key = part.substr(0, equals);
		std::string value = part.substr(equals + 1);
		if (!ContainsQueryField(allowedFields, key) || parsed.find(key) != parsed.end()) {
			valid = false;
			return parsed;
		}

		parsed.emplace(std::move(key), std::move(value));
	}

	return parsed;
}

json LegalActionsEnvelope(const GameState& gameState, const std::string& gameId, const std::vector<Action>& actions) {
	json envelope = {
		{ "gameId", gameId },
		{ "activePlayer", gameState.getCurrentPlayer() },
		{ "actions", actions },
	};
	return envelope;
}

ApiResponse ParseActionBody(const std::string& requestBody, Action& action) {
	json request;
	try {
		request = json::parse(requestBody);
	}
	catch (const std::exception&) {
		return ErrorResponse(HttpBadRequest, "malformed-json", "Request body is not valid JSON.");
	}

	if (!request.is_object()) {
		return ErrorResponse(HttpBadRequest, "invalid-field", "Action payload must be an object.");
	}

	std::string invalidField;
	if (!ContainsOnlyFields(request, { "type", "source", "target", "direction", "unit", "unloadIndex" }, invalidField)) {
		return ErrorResponse(HttpBadRequest, "invalid-field", "Unknown action field.", { { "field", invalidField } });
	}

	if (!request.contains("type") || !request.at("type").is_string()) {
		return ErrorResponse(HttpBadRequest, "invalid-field", "Action type is required.", { { "field", "type" } });
	}

	try {
		from_json(request, action);
	}
	catch (const std::exception&) {
		return ErrorResponse(HttpBadRequest, "invalid-field", "Action payload has an invalid field shape.");
	}

	if (action.m_type == Action::Type::Invalid) {
		return ErrorResponse(HttpBadRequest, "unknown-action-id", "Action type is not recognized.", { { "field", "type" }, { "value", request.at("type") } });
	}

	if (request.contains("unit") && (!action.m_optUnitType.has_value() || action.m_optUnitType.value() == UnitProperties::Type::Invalid)) {
		return ErrorResponse(HttpBadRequest, "unknown-unit-id", "Unit id is not recognized.", { { "field", "unit" }, { "value", request.at("unit") } });
	}

	if (request.contains("direction") && !action.m_optDirection.has_value()) {
		return ErrorResponse(HttpBadRequest, "unknown-direction", "Direction is not recognized.", { { "field", "direction" }, { "value", request.at("direction") } });
	}

	return JsonResponse(HttpOk, json::object());
}
}

RestGameService::GameCache::iterator RestGameService::FindGame(const std::string& gameId) {
	return m_games.find(gameId);
}

RestGameService::GameCache::const_iterator RestGameService::FindGame(const std::string& gameId) const {
	return m_games.find(gameId);
}

ApiResponse RestGameService::CreateGame(const std::string& requestBody) {
	json request;
	try {
		request = json::parse(requestBody.empty() ? "{}" : requestBody);
	}
	catch (const std::exception&) {
		return ErrorResponse(HttpBadRequest, "malformed-json", "Request body is not valid JSON.");
	}

	if (!request.is_object()) {
		return ErrorResponse(HttpBadRequest, "invalid-field", "Create game payload must be an object.");
	}

	std::string invalidField;
	if (!ContainsOnlyFields(request, { "mapId", "players", "settings", "seed" }, invalidField)) {
		return ErrorResponse(HttpBadRequest, "invalid-field", "Unknown create-game field.", { { "field", invalidField } });
	}

	if (!request.contains("mapId") || !request.at("mapId").is_string()) {
		return ErrorResponse(HttpBadRequest, "missing-required-field", "mapId is required.", { { "field", "mapId" } });
	}

	const std::string mapId = request.at("mapId").get<std::string>();
	const MapTemplate* mapTemplate = FindMapTemplate(mapId);
	if (mapTemplate == nullptr) {
		return ErrorResponse(HttpUnprocessableEntity, "unknown-map-id", "Map id is not supported.", { { "field", "mapId" }, { "value", mapId }, { "supportedMapIds", SupportedMapIds() } });
	}

	if (!request.contains("players") || !request.at("players").is_array() || request.at("players").size() != 2) {
		return ErrorResponse(HttpBadRequest, "missing-required-field", "players must contain exactly two players.", { { "field", "players" } });
	}

	ApiResponse settingsValidation = ValidateSettings(request);
	if (settingsValidation.status != HttpOk) {
		return settingsValidation;
	}
	const bool heuristicAutoResign = settingsValidation.body.at("heuristicAutoResign").get<bool>();

	std::fstream filestream(mapTemplate->path, std::ios::in);
	if (filestream.fail() || filestream.eof()) {
		return ErrorResponse(HttpInternalServerError, "map-template-unavailable", "Map template could not be loaded.", { { "mapId", mapId } });
	}

	json templateJson;
	filestream >> templateJson;
	templateJson["gameId"] = Platform::createUuid();
	templateJson["settings"] = settingsValidation.body;
	templateJson["unit-cap"] = settingsValidation.body.at("unitCap").get<int>();
	templateJson["cap-limit"] = settingsValidation.body.at("captureLimit").get<int>();
	templateJson["activePlayer"] = 0;
	templateJson["turn-count"] = 0;
	templateJson.erase("combat-rng-seed");

	for (int i = 0; i < 2; ++i) {
		const json& requestedPlayer = request.at("players").at(i);
		if (!requestedPlayer.is_object()) {
			return ErrorResponse(HttpBadRequest, "invalid-field", "Player setup must be an object.", { { "field", "players[" + std::to_string(i) + "]" } });
		}

		if (!ContainsOnlyFields(requestedPlayer, { "co", "armyType" }, invalidField)) {
			return ErrorResponse(HttpBadRequest, "invalid-field", "Unknown player field.", { { "field", "players[" + std::to_string(i) + "]." + invalidField } });
		}

		if (!requestedPlayer.contains("co") || !requestedPlayer.at("co").is_string()) {
			return ErrorResponse(HttpBadRequest, "missing-required-field", "Player co is required.", { { "field", "players[" + std::to_string(i) + "].co" } });
		}

		if (!requestedPlayer.contains("armyType") || !requestedPlayer.at("armyType").is_string()) {
			return ErrorResponse(HttpBadRequest, "missing-required-field", "Player armyType is required.", { { "field", "players[" + std::to_string(i) + "].armyType" } });
		}

		const std::string armyType = requestedPlayer.at("armyType").get<std::string>();
		const std::string templateArmyType = templateJson.at("players").at(i).at("armyType").get<std::string>();
		if (armyType != templateArmyType || Player::armyTypefromString(armyType) == Player::ArmyType::Invalid) {
			return ErrorResponse(HttpUnprocessableEntity, "unsupported-army", "Requested armyType must match the map template player slot.", {
				{ "field", "players[" + std::to_string(i) + "].armyType" },
				{ "value", armyType },
				{ "templateArmyType", templateArmyType },
			});
		}

		CommandingOfficier co;
		std::string coJsonName;
		if (!TryParseCoId(requestedPlayer.at("co").get<std::string>(), co, coJsonName)) {
			return ErrorResponse(HttpUnprocessableEntity, "unsupported-co", "Commanding officer is not supported.", { { "field", "players[" + std::to_string(i) + "].co" }, { "value", requestedPlayer.at("co") } });
		}

		Player player(co.m_type, Player::armyTypefromString(armyType));
		player.m_funds = settingsValidation.body.at("startingFunds").get<int>();
		json playerJson;
		to_json(playerJson, player);
		templateJson["players"][i] = std::move(playerJson);
	}

	GameState gameState;
	try {
		GameState::from_json(templateJson, gameState);
	}
	catch (const std::exception& err) {
		return ErrorResponse(HttpInternalServerError, "map-template-invalid", err.what(), { { "mapId", mapId } });
	}

	if (request.contains("seed")) {
		if (!request.at("seed").is_number_unsigned() &&
			(!request.at("seed").is_number_integer() || request.at("seed").get<std::int64_t>() < 0)) {
			return ErrorResponse(HttpBadRequest, "invalid-field", "seed must be an integer.", { { "field", "seed" } });
		}
		gameState.SetCombatRngSeed(request.at("seed").get<std::uint32_t>());
	}
	gameState.SetHeuristicAutoResign(heuristicAutoResign);

	if (gameState.StartFirstTurn() == Result::Failed) {
		return ErrorResponse(HttpInternalServerError, "game-initialization-failed", "Game could not run the opening turn start.", { { "mapId", mapId } });
	}

	const std::string gameId = gameState.GetId();
	m_games.emplace(gameId, std::unique_ptr<GameState>(new GameState(gameState)));

	ApiResponse response = JsonResponse(HttpCreated, SerializeGameForApi(*m_games.at(gameId)));
	response.headers.emplace("Location", "/games/" + gameId);
	return response;
}

ApiResponse RestGameService::GetGame(const std::string& gameId, const std::string& query) const {
	const auto game = FindGame(gameId);
	if (game == m_games.end()) {
		return UnknownGameResponse();
	}

	bool validQuery = true;
	std::map<std::string, std::string> queryParams = ParseQueryString(query, { "player" }, validQuery);
	if (!validQuery) {
		return ErrorResponse(HttpBadRequest, "invalid-query", "Game query may only include player.");
	}

	int playerIndex = -1;
	if (!queryParams.empty()) {
		if (!TryGetInteger(queryParams["player"], playerIndex) || playerIndex < 0 || playerIndex > 1) {
			return ErrorResponse(HttpUnprocessableEntity, "invalid-player", "Player perspective must be 0 or 1.", { { "field", "player" }, { "value", queryParams["player"] } });
		}
	}

	json response = SerializeGameForApi(*game->second);
	if (playerIndex != -1) {
		RedactHiddenHpForPerspective(response, playerIndex);
	}

	return JsonResponse(HttpOk, std::move(response));
}

ApiResponse RestGameService::ListActions(const std::string& gameId, const std::string& query) const {
	const auto game = FindGame(gameId);
	if (game == m_games.end()) {
		return UnknownGameResponse();
	}

	bool validQuery = true;
	std::map<std::string, std::string> queryParams = ParseQueryString(query, { "x", "y" }, validQuery);
	if (!validQuery || queryParams.size() == 1) {
		return ErrorResponse(HttpBadRequest, "invalid-query", "Legal action query must include both x and y, or neither.");
	}

	std::vector<Action> actions;
	json envelope;
	if (queryParams.empty()) {
		game->second->GetValidActions(actions);
		envelope = LegalActionsEnvelope(*game->second, gameId, actions);
	}
	else {
		int x = 0;
		int y = 0;
		if (!TryGetInteger(queryParams["x"], x) || !TryGetInteger(queryParams["y"], y)) {
			return ErrorResponse(HttpUnprocessableEntity, "invalid-coordinate", "x and y must be integer board coordinates.", { { "x", queryParams["x"] }, { "y", queryParams["y"] } });
		}

		if (game->second->GetValidActions(x, y, actions) == Result::Failed) {
			return ErrorResponse(HttpUnprocessableEntity, "invalid-coordinate", "x and y must be in bounds.", { { "x", x }, { "y", y } });
		}

		envelope = LegalActionsEnvelope(*game->second, gameId, actions);
		envelope["source"] = json::array({ x, y });
	}

	return JsonResponse(HttpOk, std::move(envelope));
}

ApiResponse RestGameService::SubmitAction(const std::string& gameId, const std::string& requestBody) {
	const auto game = FindGame(gameId);
	if (game == m_games.end()) {
		return UnknownGameResponse();
	}

	json currentGame = SerializeGameForApi(*game->second);
	if (game->second->FGameOver()) {
		return ErrorResponseWithGame(HttpConflict, "terminal-state", "Cannot apply an action to a terminal game.", currentGame);
	}

	Action action;
	ApiResponse actionParse = ParseActionBody(requestBody, action);
	if (actionParse.status != HttpOk) {
		return actionParse;
	}

	std::vector<Action> legalActions;
	game->second->GetValidActions(legalActions);
	if (std::find(legalActions.begin(), legalActions.end(), action) == legalActions.end()) {
		return ErrorResponseWithGame(HttpUnprocessableEntity, "illegal-action", "Action is not legal for the current player.", currentGame);
	}

	if (game->second->DoAction(action) == Result::Failed) {
		return ErrorResponseWithGame(HttpUnprocessableEntity, "illegal-action", "Action failed during execution.", currentGame);
	}

	if (game->second->FHeuristicAutoResign()) {
		game->second->CheckPlayerResigns();
	}

	return JsonResponse(HttpOk, SerializeGameForApi(*game->second));
}

void RestGameService::StoreGameForTesting(GameState gameState) {
	const std::string gameId = gameState.GetId();
	m_games[gameId] = std::unique_ptr<GameState>(new GameState(gameState));
}
