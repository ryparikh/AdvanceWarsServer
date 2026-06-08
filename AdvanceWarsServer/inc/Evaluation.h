#pragma once

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

#include "Result.h"

struct EvaluationAgentOptions {
	std::string type;
	std::filesystem::path checkpointPath;
};

struct EvaluationOptions {
	std::array<EvaluationAgentOptions, 2> agents;
	std::filesystem::path outputPath;
	bool force{ false };
	bool quiet{ false };
	std::string mapId;
	std::string player0CoId;
	std::string player1CoId;
	int rounds{ 1 };
	std::uint32_t baseSeed{ 0 };
	bool swapSides{ false };
	int maxActions{ 1000 };
	int unitCap{ 50 };
	int captureLimit{ 21 };
	bool hasDayLimit{ false };
	int dayLimit{ 0 };
	bool heuristicAutoResign{ false };
	std::string deviceName{ "auto" };
	double promotionScoreThreshold{ 0.55 };
	int minPromotionRounds{ 20 };
};

struct EvaluationSummary {
	int rounds{ 0 };
	int games{ 0 };
	std::array<int, 2> wins{ { 0, 0 } };
	int draws{ 0 };
	int noResults{ 0 };
	int decisiveGames{ 0 };
	double agent0OverallScoreRate{ 0.0 };
	double agent1OverallScoreRate{ 0.0 };
	double agent0DecisiveWinRate{ 0.0 };
	double agent1DecisiveWinRate{ 0.0 };
	double averageActions{ 0.0 };
	double averageTurns{ 0.0 };
	double averageActionSelectionMs{ 0.0 };
	std::string promotionDecision;
	std::filesystem::path reportPath;
};

struct EvaluationError {
	std::string code;
	std::string message;
	std::filesystem::path path;
	int gameIndex{ -1 };
	int ply{ -1 };
};

Result RunEvaluation(const EvaluationOptions& options, EvaluationSummary& summary, EvaluationError& error) noexcept;
