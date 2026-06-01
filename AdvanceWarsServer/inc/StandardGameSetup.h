#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "GameState.h"

enum class StandardGameSetupErrorType {
	BadRequest,
	UnprocessableEntity,
	Internal,
};

struct StandardGameSetupError {
	StandardGameSetupErrorType type{ StandardGameSetupErrorType::BadRequest };
	std::string code;
	std::string message;
	json details{ json::object() };
};

struct StandardGameSetupResult {
	GameState gameState;
	json resolvedSettings;
	std::string mapId;
	std::array<std::string, 2> playerCoIds;
	std::array<std::string, 2> playerArmyTypes;
	std::optional<std::uint32_t> seed;
};

json StandardSettingsJson(int unitCap = 50, int captureLimit = 21, std::optional<int> dayLimit = std::nullopt, bool heuristicAutoResign = false);
json SupportedStandardMapIds();
Result BuildStandardGameRequestFromSlots(
	const std::string& mapId,
	const std::string& player0CoId,
	const std::string& player1CoId,
	const json& settings,
	std::optional<std::uint32_t> seed,
	json& request,
	StandardGameSetupError& error);
Result CreateStandardGameFromRequest(const json& request, StandardGameSetupResult& result, StandardGameSetupError& error);
json SerializeStandardGameForApi(const GameState& gameState);
json SerializeGameStateForReplay(const GameState& gameState);
