#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "Result.h"

struct TrainingOptions {
	std::filesystem::path replayPath;
	std::filesystem::path checkpointIn;
	std::filesystem::path checkpointOut;
	std::filesystem::path checkpointDir;
	bool force{ false };
	int epochs{ 1 };
	int batchSize{ 32 };
	std::optional<int> maxSamples;
	double learningRate{ 0.001 };
	double weightDecay{ 0.0001 };
	double policyLossWeight{ 1.0 };
	double valueLossWeight{ 1.0 };
	double maxGradNorm{ 0.0 };
	std::int64_t seed{ 0 };
	int logEveryBatches{ 0 };
	int checkpointEveryEpoch{ 0 };
	std::string deviceName{ "auto" };
	bool quiet{ false };
};

struct TrainingEpochSummary {
	int epoch{ 0 };
	int samples{ 0 };
	int batches{ 0 };
	double averagePolicyLoss{ 0.0 };
	double averageValueLoss{ 0.0 };
	double averageTotalLoss{ 0.0 };
	double elapsedSeconds{ 0.0 };
	std::filesystem::path checkpointPath;
};

struct TrainingSummary {
	int replayFiles{ 0 };
	int samplesLoaded{ 0 };
	int samplesTrained{ 0 };
	int epochsCompleted{ 0 };
	int batchesCompleted{ 0 };
	double averagePolicyLoss{ 0.0 };
	double averageValueLoss{ 0.0 };
	double averageTotalLoss{ 0.0 };
	double elapsedSeconds{ 0.0 };
	std::string resolvedDevice;
	std::filesystem::path lastPeriodicCheckpoint;
};

struct TrainingError {
	std::string code;
	std::string message;
	std::filesystem::path replayPath;
	int line{ 0 };
	int gameIndex{ -1 };
	int ply{ -1 };
	int epoch{ 0 };
	int batch{ 0 };
};

Result RunTraining(const TrainingOptions& options, TrainingSummary& summary, TrainingError& error) noexcept;
