#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "MonteCarloTreeSearch.h"
#include "Result.h"
#include "SelfPlayReplay.h"

enum class SelfPlayMctsMode {
	Rollout,
	NeuralPuct,
};

struct SelfPlayRunnerOptions {
	std::filesystem::path outputPath;
	bool append{ false };
	bool quiet{ false };
	SelfPlayMctsMode mctsMode{ SelfPlayMctsMode::Rollout };
	std::filesystem::path policyValueCheckpointPath;
	std::string deviceName{ "auto" };
	std::string mapId;
	std::string player0CoId;
	std::string player1CoId;
	int games{ 1 };
	int maxActions{ 1000 };
	std::uint32_t baseSeed{ 0 };
	int unitCap{ 50 };
	int captureLimit{ 21 };
	bool hasDayLimit{ false };
	int dayLimit{ 0 };
	bool heuristicAutoResign{ false };
	MCTSOptions mctsOptions;
};

struct SelfPlayRunSummary {
	int gamesWritten{ 0 };
	int samplesWritten{ 0 };
	int actionsWritten{ 0 };
};

Result RunSelfPlay(const SelfPlayRunnerOptions& options, SelfPlayRunSummary& summary, SelfPlayError& error);
int RunSelfPlayCommand(int argc, char* argv[]) noexcept;
int RunValidateReplayCommand(int argc, char* argv[]) noexcept;
