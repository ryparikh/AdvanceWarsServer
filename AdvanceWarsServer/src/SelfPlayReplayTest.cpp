#include "SelfPlayReplayTest.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "PolicyValueModel.h"
#include "SelfPlayReplay.h"
#include "SelfPlayRunner.h"

namespace {
bool Expect(bool condition, const std::string& message) {
	if (!condition) {
		std::cerr << "Self-play replay test failed: " << message << std::endl;
		return false;
	}

	return true;
}

std::filesystem::path MakeTempReplayPath(const std::string& name) {
	return std::filesystem::temp_directory_path() / ("advance-wars-" + name + ".jsonl");
}

json ReadJsonLine(const std::filesystem::path& path, int lineNumber) {
	std::ifstream input(path);
	std::string line;
	for (int i = 1; i <= lineNumber && std::getline(input, line); ++i) {
		if (i == lineNumber) {
			return json::parse(line);
		}
	}

	return json();
}

bool WriteCorruptedReplay(const std::filesystem::path& source, const std::filesystem::path& target) {
	std::ifstream input(source);
	std::ofstream output(target, std::ios::trunc);
	if (!input.is_open() || !output.is_open()) {
		return false;
	}

	std::string line;
	int lineNumber = 0;
	while (std::getline(input, line)) {
		++lineNumber;
		if (lineNumber == 2) {
			json game = json::parse(line);
			game["samples"][0]["selectedActionIndex"] = -1;
			output << game.dump() << "\n";
		}
		else {
			output << line << "\n";
		}
	}

	return true;
}

bool WriteReplayMissingSampleField(const std::filesystem::path& source, const std::filesystem::path& target) {
	std::ifstream input(source);
	std::ofstream output(target, std::ios::trunc);
	if (!input.is_open() || !output.is_open()) {
		return false;
	}

	std::string line;
	int lineNumber = 0;
	while (std::getline(input, line)) {
		++lineNumber;
		if (lineNumber == 2) {
			json game = json::parse(line);
			game["samples"][0].erase("selectedActionIndex");
			output << game.dump() << "\n";
		}
		else {
			output << line << "\n";
		}
	}

	return true;
}

bool GeneratedReplayValidatesAndUsesSparseSamples() {
	const std::filesystem::path replayPath = MakeTempReplayPath("self-play-replay-test");
	std::filesystem::remove(replayPath);

	SelfPlayRunnerOptions options;
	options.outputPath = replayPath;
	options.mapId = "mcts";
	options.player0CoId = "andy";
	options.player1CoId = "adder";
	options.games = 1;
	options.maxActions = 2;
	options.baseSeed = 17;
	options.mctsOptions.maxSimulations = 1;
	options.mctsOptions.maxNodes = 16;
	options.mctsOptions.maxRolloutActions = 4;
	options.mctsOptions.temperature = 0.0;
	options.quiet = true;

	SelfPlayRunSummary runSummary;
	SelfPlayError error;
	if (!Expect(RunSelfPlay(options, runSummary, error) == Result::Succeeded, "self-play run should write and validate a replay: " + error.message)) {
		return false;
	}

	if (!Expect(std::filesystem::exists(replayPath), "replay file should exist after generation")) {
		return false;
	}

	SelfPlayReplayValidationSummary validationSummary;
	if (!Expect(ValidateSelfPlayReplay(replayPath, validationSummary, error) == Result::Succeeded, "written replay should validate: " + error.message)) {
		return false;
	}

	if (!Expect(runSummary.gamesWritten == 1, "run should write one game")) {
		return false;
	}
	if (!Expect(validationSummary.games == 1, "validator should read one game")) {
		return false;
	}
	if (!Expect(validationSummary.samples == 2, "action-limit smoke run should contain two samples")) {
		return false;
	}

	const json header = ReadJsonLine(replayPath, 1);
	if (!Expect(header.at("recordType") == "header", "first JSONL line should be a header record")) {
		return false;
	}
	if (!Expect(header.at("replayFormatVersion") == SelfPlayReplayFormatVersion(), "header should record replay format version")) {
		return false;
	}

	const json game = ReadJsonLine(replayPath, 2);
	if (!Expect(game.at("recordType") == "game", "second JSONL line should be a game record")) {
		return false;
	}
	if (!Expect(game.at("terminalReason") == "action-limit", "short smoke run should stop by action limit")) {
		return false;
	}
	if (!Expect(game.at("winner").is_null(), "action-limit game should not invent a winner")) {
		return false;
	}
	if (!Expect(game.at("actions").size() == game.at("samples").size(), "actions and samples should stay one-to-one")) {
		return false;
	}
	if (!Expect(game.at("samples")[0].contains("legalActionIndices"), "sample should contain sparse legal action indices")) {
		return false;
	}
	if (!Expect(!game.at("samples")[0].contains("legalActionMask"), "sample should not store a dense legal action mask")) {
		return false;
	}

	std::filesystem::remove(replayPath);
	return true;
}

bool WriteLegacyReplayWithoutSearchMetadata(const std::filesystem::path& source, const std::filesystem::path& target) {
	std::ifstream input(source);
	std::ofstream output(target, std::ios::trunc);
	if (!input.is_open() || !output.is_open()) {
		return false;
	}

	std::string line;
	int lineNumber = 0;
	while (std::getline(input, line)) {
		++lineNumber;
		json record = json::parse(line);
		if (lineNumber == 1) {
			record["config"].erase("mctsMode");
			record["config"].erase("policyValueCheckpoint");
			record["config"].erase("device");
		}
		else if (record.contains("config")) {
			record["config"].erase("mctsMode");
			record["config"].erase("policyValueCheckpoint");
			record["config"].erase("device");
		}
		output << record.dump() << "\n";
	}

	return true;
}

bool GeneratedNeuralReplayUsesPolicyValueEvaluator() {
	const std::filesystem::path replayPath = MakeTempReplayPath("self-play-neural-replay-test");
	const std::filesystem::path checkpointPath = std::filesystem::temp_directory_path() / "advance-wars-self-play-neural-checkpoint";
	std::filesystem::remove(replayPath);
	std::filesystem::remove_all(checkpointPath);

	PolicyValueModelConfig config;
	PolicyValueNetwork model = CreatePolicyValueNetwork(config, 31);
	PolicyValueModelError modelError;
	if (!Expect(SavePolicyValueCheckpoint(checkpointPath, model, config, 31, "cpu", false, modelError) == Result::Succeeded, "neural checkpoint should save: " + modelError.message)) {
		std::filesystem::remove_all(checkpointPath);
		return false;
	}

	SelfPlayRunnerOptions options;
	options.outputPath = replayPath;
	options.mapId = "mcts";
	options.player0CoId = "andy";
	options.player1CoId = "adder";
	options.games = 1;
	options.maxActions = 1;
	options.baseSeed = 37;
	options.mctsMode = SelfPlayMctsMode::NeuralPuct;
	options.policyValueCheckpointPath = checkpointPath;
	options.deviceName = "cpu";
	options.mctsOptions.maxSimulations = 2;
	options.mctsOptions.maxNodes = 10000;
	options.mctsOptions.maxRolloutActions = 0;
	options.mctsOptions.temperature = 0.0;
	options.quiet = true;

	SelfPlayRunSummary runSummary;
	SelfPlayError error;
	if (!Expect(RunSelfPlay(options, runSummary, error) == Result::Succeeded, "neural self-play run should write and validate a replay: " + error.message)) {
		std::filesystem::remove(replayPath);
		std::filesystem::remove_all(checkpointPath);
		return false;
	}

	const json header = ReadJsonLine(replayPath, 1);
	const json game = ReadJsonLine(replayPath, 2);
	const bool passed =
		Expect(header.at("config").at("mctsMode") == "neural-puct", "header should record neural MCTS mode") &&
		Expect(game.at("config").at("mctsMode") == "neural-puct", "game config should record neural MCTS mode") &&
		Expect(game.at("samples")[0].at("mcts").at("simulationsRun") == 2, "neural sample should record requested simulations") &&
		Expect(runSummary.samplesWritten == 1, "neural smoke run should write one sample");

	std::filesystem::remove(replayPath);
	std::filesystem::remove_all(checkpointPath);
	return passed;
}

bool CorruptedReplayIsRejected() {
	const std::filesystem::path replayPath = MakeTempReplayPath("self-play-replay-corrupt-source");
	const std::filesystem::path corruptPath = MakeTempReplayPath("self-play-replay-corrupt");
	std::filesystem::remove(replayPath);
	std::filesystem::remove(corruptPath);

	SelfPlayRunnerOptions options;
	options.outputPath = replayPath;
	options.mapId = "mcts";
	options.player0CoId = "andy";
	options.player1CoId = "adder";
	options.games = 1;
	options.maxActions = 1;
	options.baseSeed = 23;
	options.mctsOptions.maxSimulations = 1;
	options.mctsOptions.maxNodes = 16;
	options.mctsOptions.maxRolloutActions = 4;
	options.mctsOptions.temperature = 0.0;
	options.quiet = true;

	SelfPlayRunSummary runSummary;
	SelfPlayError error;
	if (!Expect(RunSelfPlay(options, runSummary, error) == Result::Succeeded, "self-play run should create corrupt-source replay: " + error.message)) {
		return false;
	}
	if (!Expect(WriteCorruptedReplay(replayPath, corruptPath), "test should write a corrupted replay")) {
		return false;
	}

	SelfPlayReplayValidationSummary validationSummary;
	const Result result = ValidateSelfPlayReplay(corruptPath, validationSummary, error);
	std::filesystem::remove(replayPath);
	std::filesystem::remove(corruptPath);

	return Expect(result == Result::Failed, "validator should reject selected action index corruption");
}

bool MissingRequiredReplayFieldIsRejected() {
	const std::filesystem::path replayPath = MakeTempReplayPath("self-play-replay-missing-source");
	const std::filesystem::path corruptPath = MakeTempReplayPath("self-play-replay-missing-field");
	std::filesystem::remove(replayPath);
	std::filesystem::remove(corruptPath);

	SelfPlayRunnerOptions options;
	options.outputPath = replayPath;
	options.mapId = "mcts";
	options.player0CoId = "andy";
	options.player1CoId = "adder";
	options.games = 1;
	options.maxActions = 1;
	options.baseSeed = 29;
	options.mctsOptions.maxSimulations = 1;
	options.mctsOptions.maxNodes = 16;
	options.mctsOptions.maxRolloutActions = 4;
	options.mctsOptions.temperature = 0.0;
	options.quiet = true;

	SelfPlayRunSummary runSummary;
	SelfPlayError error;
	if (!Expect(RunSelfPlay(options, runSummary, error) == Result::Succeeded, "self-play run should create missing-field source replay: " + error.message)) {
		return false;
	}
	if (!Expect(WriteReplayMissingSampleField(replayPath, corruptPath), "test should write a replay missing a required sample field")) {
		return false;
	}

	SelfPlayReplayValidationSummary validationSummary;
	const Result result = ValidateSelfPlayReplay(corruptPath, validationSummary, error);
	std::filesystem::remove(replayPath);
	std::filesystem::remove(corruptPath);

	if (!Expect(result == Result::Failed, "validator should reject missing required replay fields")) {
		return false;
	}
	return Expect(error.code == "schema-error", "missing field rejection should report a schema error");
}

bool LegacyReplayHeaderCanAppendRolloutDefaults() {
	const std::filesystem::path sourcePath = MakeTempReplayPath("self-play-replay-legacy-source");
	const std::filesystem::path legacyPath = MakeTempReplayPath("self-play-replay-legacy-append");
	std::filesystem::remove(sourcePath);
	std::filesystem::remove(legacyPath);

	SelfPlayRunnerOptions options;
	options.outputPath = sourcePath;
	options.mapId = "mcts";
	options.player0CoId = "andy";
	options.player1CoId = "adder";
	options.games = 1;
	options.maxActions = 1;
	options.baseSeed = 41;
	options.mctsOptions.maxSimulations = 1;
	options.mctsOptions.maxNodes = 16;
	options.mctsOptions.maxRolloutActions = 4;
	options.mctsOptions.temperature = 0.0;
	options.quiet = true;

	SelfPlayRunSummary runSummary;
	SelfPlayError error;
	if (!Expect(RunSelfPlay(options, runSummary, error) == Result::Succeeded, "self-play run should create legacy source replay: " + error.message)) {
		std::filesystem::remove(sourcePath);
		return false;
	}
	if (!Expect(WriteLegacyReplayWithoutSearchMetadata(sourcePath, legacyPath), "test should write a legacy-shaped replay")) {
		std::filesystem::remove(sourcePath);
		std::filesystem::remove(legacyPath);
		return false;
	}

	options.outputPath = legacyPath;
	options.append = true;
	options.baseSeed = 41;
	const Result appendResult = RunSelfPlay(options, runSummary, error);

	SelfPlayReplayValidationSummary validationSummary;
	const Result validationResult = ValidateSelfPlayReplay(legacyPath, validationSummary, error);
	std::filesystem::remove(sourcePath);
	std::filesystem::remove(legacyPath);

	return Expect(appendResult == Result::Succeeded, "append should accept legacy rollout header defaults: " + error.message) &&
		Expect(validationResult == Result::Succeeded, "legacy-plus-new replay should validate after append: " + error.message) &&
		Expect(validationSummary.games == 2, "legacy append test should contain two games");
}

bool InvalidSelfPlaySetupReportsError() {
	SelfPlayRunnerOptions options;
	options.outputPath = MakeTempReplayPath("self-play-invalid-setup");
	options.mapId = "not-a-map";
	options.player0CoId = "andy";
	options.player1CoId = "adder";
	options.games = 1;
	options.maxActions = 1;
	options.mctsOptions.maxSimulations = 1;
	options.mctsOptions.maxNodes = 16;
	options.mctsOptions.maxRolloutActions = 4;
	options.quiet = true;
	std::filesystem::remove(options.outputPath);

	SelfPlayRunSummary runSummary;
	SelfPlayError error;
	const Result result = RunSelfPlay(options, runSummary, error);
	std::filesystem::remove(options.outputPath);

	if (!Expect(result == Result::Failed, "invalid self-play setup should fail")) {
		return false;
	}
	return Expect(error.code == "unknown-map-id", "invalid setup should surface the setup error code");
}
}

int RunSelfPlayReplayTests() {
	if (!GeneratedReplayValidatesAndUsesSparseSamples()) {
		return 1;
	}

	if (!GeneratedNeuralReplayUsesPolicyValueEvaluator()) {
		return 1;
	}

	if (!CorruptedReplayIsRejected()) {
		return 1;
	}

	if (!MissingRequiredReplayFieldIsRejected()) {
		return 1;
	}

	if (!LegacyReplayHeaderCanAppendRolloutDefaults()) {
		return 1;
	}

	if (!InvalidSelfPlaySetupReportsError()) {
		return 1;
	}

	std::cout << "Self-play replay tests passed" << std::endl;
	return 0;
}
