#include "StandardGameSetup.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <vector>

#include "Platform.h"

namespace {
struct MapTemplate {
	const char* id;
	const char* path;
};

constexpr std::array<MapTemplate, 6> MapTemplates{ {
	{ "lefty", "res/AWBW/MapSources/Lefty.json" },
	{ "mcts", "res/AWBW/MapSources/MCTS.json" },
	{ "tiny-capture-5x5", "res/AWBW/MapSources/TinyCapture5x5.json" },
	{ "tiny-skirmish-5x5", "res/AWBW/MapSources/TinySkirmish5x5.json" },
	{ "small-capture-7x7", "res/AWBW/MapSources/SmallCapture7x7.json" },
	{ "small-production-7x7", "res/AWBW/MapSources/SmallProduction7x7.json" },
} };

void SetError(StandardGameSetupError& error, StandardGameSetupErrorType type, const std::string& code, const std::string& message, json details = json::object()) {
	error.type = type;
	error.code = code;
	error.message = message;
	error.details = std::move(details);
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

const MapTemplate* FindMapTemplate(const std::string& mapId) {
	for (const MapTemplate& mapTemplate : MapTemplates) {
		if (mapId == mapTemplate.id) {
			return &mapTemplate;
		}
	}
	return nullptr;
}

Result LoadMapTemplateJson(const std::string& mapId, json& templateJson, StandardGameSetupError& error) {
	const MapTemplate* mapTemplate = FindMapTemplate(mapId);
	if (mapTemplate == nullptr) {
		SetError(error, StandardGameSetupErrorType::UnprocessableEntity, "unknown-map-id", "Map id is not supported.", {
			{ "field", "mapId" },
			{ "value", mapId },
			{ "supportedMapIds", SupportedStandardMapIds() },
		});
		return Result::Failed;
	}

	std::fstream filestream(mapTemplate->path, std::ios::in);
	if (filestream.fail() || filestream.eof()) {
		SetError(error, StandardGameSetupErrorType::Internal, "map-template-unavailable", "Map template could not be loaded.", { { "mapId", mapId } });
		return Result::Failed;
	}

	try {
		filestream >> templateJson;
	}
	catch (const std::exception& err) {
		SetError(error, StandardGameSetupErrorType::Internal, "map-template-invalid", err.what(), { { "mapId", mapId } });
		return Result::Failed;
	}

	return Result::Succeeded;
}

Result ValidateSettings(const json& request, json& resolvedSettings, StandardGameSetupError& error) {
	if (!request.contains("settings")) {
		resolvedSettings = StandardSettingsJson();
		return Result::Succeeded;
	}

	const json& settings = request.at("settings");
	if (!settings.is_object()) {
		SetError(error, StandardGameSetupErrorType::BadRequest, "invalid-field", "settings must be an object.", { { "field", "settings" } });
		return Result::Failed;
	}

	std::string invalidField;
	if (!ContainsOnlyFields(settings, { "mode", "fog", "weather", "coPowers", "tags", "startingFunds", "incomePerProperty", "unitCap", "captureLimit", "dayLimit", "bannedUnits", "heuristicAutoResign" }, invalidField)) {
		SetError(error, StandardGameSetupErrorType::BadRequest, "invalid-field", "Unknown settings field.", { { "field", "settings." + invalidField } });
		return Result::Failed;
	}

	int unitCap = 50;
	int captureLimit = 21;
	std::optional<int> dayLimit;
	try {
		if (settings.contains("mode") && (!settings.at("mode").is_string() || settings.at("mode").get<std::string>() != "standard")) {
			SetError(error, StandardGameSetupErrorType::UnprocessableEntity, "unsupported-setting", "Only standard mode is supported.", { { "field", "settings.mode" }, { "value", settings.at("mode") } });
			return Result::Failed;
		}
		if (settings.contains("fog") && (!settings.at("fog").is_boolean() || settings.at("fog").get<bool>() != false)) {
			SetError(error, StandardGameSetupErrorType::UnprocessableEntity, "unsupported-setting", "Fog of War is not supported for standard games yet.", { { "field", "settings.fog" }, { "value", settings.at("fog") } });
			return Result::Failed;
		}
		if (settings.contains("weather") && (!settings.at("weather").is_string() || settings.at("weather").get<std::string>() != "clear")) {
			SetError(error, StandardGameSetupErrorType::UnprocessableEntity, "unsupported-setting", "Only clear weather is supported for standard games.", { { "field", "settings.weather" }, { "value", settings.at("weather") } });
			return Result::Failed;
		}
		if (settings.contains("coPowers") && (!settings.at("coPowers").is_boolean() || settings.at("coPowers").get<bool>() != true)) {
			SetError(error, StandardGameSetupErrorType::UnprocessableEntity, "unsupported-setting", "CO powers must remain enabled for standard games.", { { "field", "settings.coPowers" }, { "value", settings.at("coPowers") } });
			return Result::Failed;
		}
		if (settings.contains("tags") && (!settings.at("tags").is_boolean() || settings.at("tags").get<bool>() != false)) {
			SetError(error, StandardGameSetupErrorType::UnprocessableEntity, "unsupported-setting", "Tag powers are not supported for standard games yet.", { { "field", "settings.tags" }, { "value", settings.at("tags") } });
			return Result::Failed;
		}
		if (settings.contains("startingFunds") && (!settings.at("startingFunds").is_number_integer() || settings.at("startingFunds").get<int>() != 0)) {
			SetError(error, StandardGameSetupErrorType::UnprocessableEntity, "unsupported-setting", "Only 0 starting funds are supported for standard games.", { { "field", "settings.startingFunds" }, { "value", settings.at("startingFunds") } });
			return Result::Failed;
		}
		if (settings.contains("incomePerProperty") && (!settings.at("incomePerProperty").is_number_integer() || settings.at("incomePerProperty").get<int>() != 1000)) {
			SetError(error, StandardGameSetupErrorType::UnprocessableEntity, "unsupported-setting", "Only 1000 income per property is supported.", { { "field", "settings.incomePerProperty" }, { "value", settings.at("incomePerProperty") } });
			return Result::Failed;
		}
		if (settings.contains("unitCap")) {
			if (!settings.at("unitCap").is_number_integer()) {
				SetError(error, StandardGameSetupErrorType::BadRequest, "invalid-field", "unitCap must be an integer.", { { "field", "settings.unitCap" } });
				return Result::Failed;
			}
			unitCap = settings.at("unitCap").get<int>();
			if (unitCap <= 0) {
				SetError(error, StandardGameSetupErrorType::UnprocessableEntity, "unsupported-setting", "unitCap must be positive for standard games.", { { "field", "settings.unitCap" }, { "value", settings.at("unitCap") } });
				return Result::Failed;
			}
		}
		if (settings.contains("captureLimit")) {
			if (!settings.at("captureLimit").is_number_integer()) {
				SetError(error, StandardGameSetupErrorType::BadRequest, "invalid-field", "captureLimit must be an integer.", { { "field", "settings.captureLimit" } });
				return Result::Failed;
			}
			captureLimit = settings.at("captureLimit").get<int>();
			if (captureLimit <= 0) {
				SetError(error, StandardGameSetupErrorType::UnprocessableEntity, "unsupported-setting", "captureLimit must be positive for standard games.", { { "field", "settings.captureLimit" }, { "value", settings.at("captureLimit") } });
				return Result::Failed;
			}
		}
		if (settings.contains("dayLimit")) {
			if (settings.at("dayLimit").is_null()) {
				dayLimit.reset();
			}
			else if (!settings.at("dayLimit").is_number_integer()) {
				SetError(error, StandardGameSetupErrorType::BadRequest, "invalid-field", "dayLimit must be an integer or null.", { { "field", "settings.dayLimit" } });
				return Result::Failed;
			}
			else {
				const int parsedDayLimit = settings.at("dayLimit").get<int>();
				if (parsedDayLimit <= 0) {
					SetError(error, StandardGameSetupErrorType::UnprocessableEntity, "unsupported-setting", "dayLimit must be positive for standard games.", { { "field", "settings.dayLimit" }, { "value", settings.at("dayLimit") } });
					return Result::Failed;
				}
				dayLimit = parsedDayLimit;
			}
		}
		if (settings.contains("bannedUnits") && !IsStandardBannedUnits(settings.at("bannedUnits"))) {
			SetError(error, StandardGameSetupErrorType::UnprocessableEntity, "unsupported-setting", "Only the standard blackbomb ban is supported.", { { "field", "settings.bannedUnits" }, { "value", settings.at("bannedUnits") } });
			return Result::Failed;
		}
		if (settings.contains("heuristicAutoResign") && !settings.at("heuristicAutoResign").is_boolean()) {
			SetError(error, StandardGameSetupErrorType::BadRequest, "invalid-field", "heuristicAutoResign must be a boolean.", { { "field", "settings.heuristicAutoResign" } });
			return Result::Failed;
		}
	}
	catch (const std::exception&) {
		SetError(error, StandardGameSetupErrorType::BadRequest, "invalid-field", "settings contains a value with the wrong type.", { { "field", "settings" } });
		return Result::Failed;
	}

	const bool heuristicAutoResign = settings.contains("heuristicAutoResign") && settings.at("heuristicAutoResign").get<bool>();
	resolvedSettings = StandardSettingsJson(unitCap, captureLimit, dayLimit, heuristicAutoResign);
	return Result::Succeeded;
}

bool IsValidSeedJson(const json& value) {
	return value.is_number_unsigned() ||
		(value.is_number_integer() && value.get<std::int64_t>() >= 0);
}
}

json StandardSettingsJson(int unitCap, int captureLimit, std::optional<int> dayLimit, bool heuristicAutoResign) {
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

json SupportedStandardMapIds() {
	json supported = json::array();
	for (const MapTemplate& mapTemplate : MapTemplates) {
		supported.push_back(mapTemplate.id);
	}
	return supported;
}

Result BuildStandardGameRequestFromSlots(
	const std::string& mapId,
	const std::string& player0CoId,
	const std::string& player1CoId,
	const json& settings,
	std::optional<std::uint32_t> seed,
	json& request,
	StandardGameSetupError& error) {
	json templateJson;
	IfFailedReturn(LoadMapTemplateJson(mapId, templateJson, error));
	if (!templateJson.contains("players") || !templateJson.at("players").is_array() || templateJson.at("players").size() != 2) {
		SetError(error, StandardGameSetupErrorType::Internal, "map-template-invalid", "Map template must contain exactly two players.", { { "mapId", mapId } });
		return Result::Failed;
	}

	request = {
		{ "mapId", mapId },
		{ "players", json::array({
			{
				{ "co", player0CoId },
				{ "armyType", templateJson.at("players").at(0).at("armyType") },
			},
			{
				{ "co", player1CoId },
				{ "armyType", templateJson.at("players").at(1).at("armyType") },
			},
		}) },
		{ "settings", settings },
	};
	if (seed.has_value()) {
		request["seed"] = seed.value();
	}

	return Result::Succeeded;
}

Result CreateStandardGameFromRequest(const json& request, StandardGameSetupResult& result, StandardGameSetupError& error) {
	if (!request.is_object()) {
		SetError(error, StandardGameSetupErrorType::BadRequest, "invalid-field", "Create game payload must be an object.");
		return Result::Failed;
	}

	std::string invalidField;
	if (!ContainsOnlyFields(request, { "mapId", "players", "settings", "seed" }, invalidField)) {
		SetError(error, StandardGameSetupErrorType::BadRequest, "invalid-field", "Unknown create-game field.", { { "field", invalidField } });
		return Result::Failed;
	}

	if (!request.contains("mapId") || !request.at("mapId").is_string()) {
		SetError(error, StandardGameSetupErrorType::BadRequest, "missing-required-field", "mapId is required.", { { "field", "mapId" } });
		return Result::Failed;
	}

	const std::string mapId = request.at("mapId").get<std::string>();
	json templateJson;
	IfFailedReturn(LoadMapTemplateJson(mapId, templateJson, error));

	if (!request.contains("players") || !request.at("players").is_array() || request.at("players").size() != 2) {
		SetError(error, StandardGameSetupErrorType::BadRequest, "missing-required-field", "players must contain exactly two players.", { { "field", "players" } });
		return Result::Failed;
	}

	json resolvedSettings;
	IfFailedReturn(ValidateSettings(request, resolvedSettings, error));
	const bool heuristicAutoResign = resolvedSettings.at("heuristicAutoResign").get<bool>();

	templateJson["gameId"] = Platform::createUuid();
	templateJson["settings"] = resolvedSettings;
	templateJson["unit-cap"] = resolvedSettings.at("unitCap").get<int>();
	templateJson["cap-limit"] = resolvedSettings.at("captureLimit").get<int>();
	templateJson["activePlayer"] = 0;
	templateJson["turn-count"] = 0;
	templateJson.erase("combat-rng-seed");

	std::array<std::string, 2> coIds;
	std::array<std::string, 2> armyTypes;
	for (int i = 0; i < 2; ++i) {
		const json& requestedPlayer = request.at("players").at(i);
		if (!requestedPlayer.is_object()) {
			SetError(error, StandardGameSetupErrorType::BadRequest, "invalid-field", "Player setup must be an object.", { { "field", "players[" + std::to_string(i) + "]" } });
			return Result::Failed;
		}

		if (!ContainsOnlyFields(requestedPlayer, { "co", "armyType" }, invalidField)) {
			SetError(error, StandardGameSetupErrorType::BadRequest, "invalid-field", "Unknown player field.", { { "field", "players[" + std::to_string(i) + "]." + invalidField } });
			return Result::Failed;
		}

		if (!requestedPlayer.contains("co") || !requestedPlayer.at("co").is_string()) {
			SetError(error, StandardGameSetupErrorType::BadRequest, "missing-required-field", "Player co is required.", { { "field", "players[" + std::to_string(i) + "].co" } });
			return Result::Failed;
		}

		if (!requestedPlayer.contains("armyType") || !requestedPlayer.at("armyType").is_string()) {
			SetError(error, StandardGameSetupErrorType::BadRequest, "missing-required-field", "Player armyType is required.", { { "field", "players[" + std::to_string(i) + "].armyType" } });
			return Result::Failed;
		}

		const std::string armyType = requestedPlayer.at("armyType").get<std::string>();
		const std::string templateArmyType = templateJson.at("players").at(i).at("armyType").get<std::string>();
		if (armyType != templateArmyType || Player::armyTypefromString(armyType) == Player::ArmyType::Invalid) {
			SetError(error, StandardGameSetupErrorType::UnprocessableEntity, "unsupported-army", "Requested armyType must match the map template player slot.", {
				{ "field", "players[" + std::to_string(i) + "].armyType" },
				{ "value", armyType },
				{ "templateArmyType", templateArmyType },
			});
			return Result::Failed;
		}

		CommandingOfficier co;
		std::string coJsonName;
		coIds[i] = requestedPlayer.at("co").get<std::string>();
		if (!TryParseCoId(coIds[i], co, coJsonName)) {
			SetError(error, StandardGameSetupErrorType::UnprocessableEntity, "unsupported-co", "Commanding officer is not supported.", { { "field", "players[" + std::to_string(i) + "].co" }, { "value", requestedPlayer.at("co") } });
			return Result::Failed;
		}

		Player player(co.m_type, Player::armyTypefromString(armyType));
		player.m_funds = resolvedSettings.at("startingFunds").get<int>();
		json playerJson;
		to_json(playerJson, player);
		templateJson["players"][i] = std::move(playerJson);
		armyTypes[i] = armyType;
	}

	GameState gameState;
	try {
		GameState::from_json(templateJson, gameState);
	}
	catch (const std::exception& err) {
		SetError(error, StandardGameSetupErrorType::Internal, "map-template-invalid", err.what(), { { "mapId", mapId } });
		return Result::Failed;
	}

	std::optional<std::uint32_t> seed;
	if (request.contains("seed")) {
		if (!IsValidSeedJson(request.at("seed"))) {
			SetError(error, StandardGameSetupErrorType::BadRequest, "invalid-field", "seed must be an integer.", { { "field", "seed" } });
			return Result::Failed;
		}
		seed = request.at("seed").get<std::uint32_t>();
		gameState.SetCombatRngSeed(seed.value());
	}
	gameState.SetHeuristicAutoResign(heuristicAutoResign);

	if (gameState.StartFirstTurn() == Result::Failed) {
		SetError(error, StandardGameSetupErrorType::Internal, "game-initialization-failed", "Game could not run the opening turn start.", { { "mapId", mapId } });
		return Result::Failed;
	}

	result.gameState = std::move(gameState);
	result.resolvedSettings = std::move(resolvedSettings);
	result.mapId = mapId;
	result.playerCoIds = coIds;
	result.playerArmyTypes = armyTypes;
	result.seed = seed;
	return Result::Succeeded;
}

json SerializeStandardGameForApi(const GameState& gameState) {
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

json SerializeGameStateForReplay(const GameState& gameState) {
	json game;
	GameState::to_json(game, gameState);
	if (gameState.GetTerminalReason().has_value()) {
		game["terminalReason"] = gameState.GetTerminalReason().value();
	}
	else {
		game["terminalReason"] = nullptr;
	}
	return game;
}
