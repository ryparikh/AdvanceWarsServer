#include "TrainingTest.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "PolicyValueModel.h"
#include "SelfPlayRunner.h"
#include "Training.h"

namespace {
bool Expect(bool condition, const std::string& message) {
	if (!condition) {
		std::cerr << "Training test failed: " << message << std::endl;
		return false;
	}

	return true;
}

std::filesystem::path MakeTempPath(const std::string& name) {
	return std::filesystem::temp_directory_path() / ("advance-wars-" + name);
}

bool AnyParameterChanged(const std::filesystem::path& beforePath, const std::filesystem::path& afterPath) {
	PolicyValueNetwork before(nullptr);
	PolicyValueNetwork after(nullptr);
	PolicyValueModelConfig beforeConfig;
	PolicyValueModelConfig afterConfig;
	std::int64_t beforeSeed = -1;
	std::int64_t afterSeed = -1;
	PolicyValueModelError error;
	if (LoadPolicyValueCheckpoint(beforePath, before, beforeConfig, beforeSeed, error) == Result::Failed) {
		std::cerr << "Training test failed: input checkpoint reload failed: " << error.message << std::endl;
		return false;
	}
	if (LoadPolicyValueCheckpoint(afterPath, after, afterConfig, afterSeed, error) == Result::Failed) {
		std::cerr << "Training test failed: output checkpoint reload failed: " << error.message << std::endl;
		return false;
	}

	const std::vector<torch::Tensor> beforeParams = before->parameters();
	const std::vector<torch::Tensor> afterParams = after->parameters();
	if (beforeParams.size() != afterParams.size()) {
		return true;
	}

	for (std::size_t i = 0; i < beforeParams.size(); ++i) {
		if (!torch::allclose(beforeParams[i].cpu(), afterParams[i].cpu(), 1e-6, 1e-6)) {
			return true;
		}
	}
	return false;
}

bool TinyReplayTrainingWritesCheckpointAndMetrics() {
	const std::filesystem::path replayPath = MakeTempPath("train-smoke.jsonl");
	const std::filesystem::path inputCheckpoint = MakeTempPath("train-input-checkpoint");
	const std::filesystem::path outputCheckpoint = MakeTempPath("train-output-checkpoint");
	std::filesystem::remove(replayPath);
	std::filesystem::remove_all(inputCheckpoint);
	std::filesystem::remove_all(outputCheckpoint);

	SelfPlayRunnerOptions replayOptions;
	replayOptions.outputPath = replayPath;
	replayOptions.mapId = "mcts";
	replayOptions.player0CoId = "andy";
	replayOptions.player1CoId = "adder";
	replayOptions.games = 1;
	replayOptions.maxActions = 2;
	replayOptions.baseSeed = 41;
	replayOptions.mctsOptions.maxSimulations = 1;
	replayOptions.mctsOptions.maxNodes = 16;
	replayOptions.mctsOptions.maxRolloutActions = 4;
	replayOptions.mctsOptions.temperature = 0.0;
	replayOptions.quiet = true;

	SelfPlayRunSummary replaySummary;
	SelfPlayError replayError;
	if (!Expect(RunSelfPlay(replayOptions, replaySummary, replayError) == Result::Succeeded, "self-play replay should generate: " + replayError.message)) {
		return false;
	}

	PolicyValueModelConfig config;
	config.hiddenChannels = 8;
	config.residualBlocks = 0;
	config.normGroups = 1;
	PolicyValueModelError modelError;
	PolicyValueNetwork model = CreatePolicyValueNetwork(config, 7);
	if (!Expect(SavePolicyValueCheckpoint(inputCheckpoint, model, config, 7, "cpu", false, modelError) == Result::Succeeded, "input checkpoint should save: " + modelError.message)) {
		std::filesystem::remove(replayPath);
		return false;
	}

	TrainingOptions options;
	options.replayPath = replayPath;
	options.checkpointIn = inputCheckpoint;
	options.checkpointOut = outputCheckpoint;
	options.epochs = 1;
	options.batchSize = 3;
	options.learningRate = 0.01;
	options.weightDecay = 0.0;
	options.seed = 5;
	options.deviceName = "cpu";
	options.quiet = true;

	TrainingSummary summary;
	TrainingError error;
	const Result result = RunTraining(options, summary, error);

	bool passed = true;
	passed = Expect(result == Result::Succeeded, "training should succeed: " + error.code + " " + error.message) && passed;
	passed = Expect(std::filesystem::exists(outputCheckpoint / "metadata.json"), "output metadata.json should exist") && passed;
	passed = Expect(std::filesystem::exists(outputCheckpoint / "model.pt"), "output model.pt should exist") && passed;
	passed = Expect(std::filesystem::exists(outputCheckpoint / "training.json"), "output training.json should exist") && passed;
	passed = Expect(summary.samplesLoaded == 2, "two replay samples should load") && passed;
	passed = Expect(summary.samplesTrained == 2, "two replay samples should train") && passed;
	passed = Expect(summary.batchesCompleted == 1, "partial final batch should be trained") && passed;
	passed = Expect(AnyParameterChanged(inputCheckpoint, outputCheckpoint), "training should change at least one model parameter") && passed;

	std::ifstream trainingMetadata(outputCheckpoint / "training.json");
	if (passed && Expect(trainingMetadata.is_open(), "training.json should open")) {
		json metadata;
		trainingMetadata >> metadata;
		passed = Expect(metadata.at("trainingMetadataVersion") == 1, "training metadata version should be 1") && passed;
		passed = Expect(metadata.at("sampleCount") == 2, "training metadata should record sample count") && passed;
		passed = Expect(metadata.at("epochsCompleted") == 1, "training metadata should record completed epochs") && passed;
		passed = Expect(metadata.at("epochHistory").is_array() && metadata.at("epochHistory").size() == 1, "training metadata should record one epoch") && passed;
	}
	trainingMetadata.close();

	std::filesystem::remove(replayPath);
	std::filesystem::remove_all(inputCheckpoint);
	std::filesystem::remove_all(outputCheckpoint);
	return passed;
}
}

int RunTrainingTests() noexcept {
	try {
		if (!TinyReplayTrainingWritesCheckpointAndMetrics()) {
			return 1;
		}

		std::cout << "Training tests passed" << std::endl;
		return 0;
	}
	catch (const std::exception& err) {
		std::cerr << "Training test exception: " << err.what() << std::endl;
		return 1;
	}
}
