#include "EvaluationTest.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "Evaluation.h"
#include "PolicyValueModel.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

namespace {
bool Expect(bool condition, const std::string& message) {
	if (!condition) {
		std::cerr << "Evaluation test failed: " << message << std::endl;
		return false;
	}

	return true;
}

std::filesystem::path MakeTempPath(const std::string& name) {
	return std::filesystem::temp_directory_path() / ("advance-wars-" + name);
}

bool SaveTinyCheckpoint(const std::filesystem::path& checkpointPath) {
	std::filesystem::remove_all(checkpointPath);
	PolicyValueModelConfig config;
	config.hiddenChannels = 8;
	config.residualBlocks = 0;
	config.normGroups = 1;

	PolicyValueNetwork model = CreatePolicyValueNetwork(config, 17);
	PolicyValueModelError error;
	return Expect(SavePolicyValueCheckpoint(checkpointPath, model, config, 17, "cpu", false, error) == Result::Succeeded, "checkpoint should save: " + error.message);
}

bool LoadJsonFile(const std::filesystem::path& path, json& value) {
	std::ifstream input(path);
	if (!Expect(input.is_open(), "JSON report should open: " + path.string())) {
		return false;
	}

	input >> value;
	return true;
}

EvaluationOptions BaseEvaluationOptions(const std::filesystem::path& outPath) {
	EvaluationOptions options;
	options.outputPath = outPath;
	options.mapId = "mcts";
	options.player0CoId = "andy";
	options.player1CoId = "adder";
	options.rounds = 1;
	options.baseSeed = 123;
	options.maxActions = 1;
	options.deviceName = "cpu";
	options.quiet = true;
	options.force = false;
	options.agents[0].type = "random";
	options.agents[1].type = "random";
	return options;
}

bool CheckpointPolicyVersusRandomWritesReport() {
	const std::filesystem::path checkpoint = MakeTempPath("eval-checkpoint");
	const std::filesystem::path reportPath = MakeTempPath("eval-report.json");
	std::filesystem::remove(reportPath);

	if (!SaveTinyCheckpoint(checkpoint)) {
		return false;
	}

	EvaluationOptions options = BaseEvaluationOptions(reportPath);
	options.agents[0].type = "checkpoint-policy";
	options.agents[0].checkpointPath = checkpoint;
	options.agents[1].type = "random";
	options.maxActions = 1;
	options.minPromotionRounds = 1;

	EvaluationSummary summary;
	EvaluationError error;
	const Result result = RunEvaluation(options, summary, error);

	bool passed = true;
	passed = Expect(result == Result::Succeeded, "evaluation should succeed: " + error.code + " " + error.message) && passed;
	passed = Expect(std::filesystem::exists(reportPath), "evaluation report should exist") && passed;
	passed = Expect(summary.games == 1, "one game should run") && passed;
	passed = Expect(summary.noResults == 1, "one-action smoke game should be a no-result") && passed;
	passed = Expect(summary.promotionDecision == "insufficient-data", "no-result smoke should not recommend promotion") && passed;

	json report;
	if (passed && LoadJsonFile(reportPath, report)) {
		passed = Expect(report.at("evaluationMetadataVersion") == 1, "report metadata version should be 1") && passed;
		passed = Expect(report.at("agents").is_array() && report.at("agents").size() == 2, "report should include two agents") && passed;
		passed = Expect(report.at("agents").at(0).at("type") == "checkpoint-policy", "agent 0 should be checkpoint-policy") && passed;
		passed = Expect(report.at("agents").at(0).contains("checkpoint"), "checkpoint agent should include checkpoint metadata") && passed;
		passed = Expect(report.at("summary").at("noResult") == 1, "summary should separate no-results") && passed;
		passed = Expect(report.at("summary").at("draws") == 0, "summary should separate true draws") && passed;
		passed = Expect(report.at("promotionRecommendation").at("decision") == "insufficient-data", "report should include recommendation") && passed;
		passed = Expect(report.at("games").is_array() && report.at("games").size() == 1, "report should include one compact game") && passed;
		passed = Expect(report.at("games").at(0).at("terminalReason") == "action-limit", "action limit should be reported") && passed;
		passed = Expect(report.at("games").at(0).at("winner").is_null(), "action-limit winner should be null") && passed;
	}

	std::filesystem::remove_all(checkpoint);
	std::filesystem::remove(reportPath);
	return passed;
}

bool SwapSidesRunsBothOrientationsAndSwapsAgentCoPairs() {
	const std::filesystem::path reportPath = MakeTempPath("eval-swap-report.json");
	std::filesystem::remove(reportPath);

	EvaluationOptions options = BaseEvaluationOptions(reportPath);
	options.swapSides = true;
	options.force = false;

	EvaluationSummary summary;
	EvaluationError error;
	const Result result = RunEvaluation(options, summary, error);

	bool passed = true;
	passed = Expect(result == Result::Succeeded, "swap-side evaluation should succeed: " + error.code + " " + error.message) && passed;
	passed = Expect(summary.games == 2, "swap-side one-round evaluation should run two games") && passed;

	json report;
	if (passed && LoadJsonFile(reportPath, report)) {
		const json& games = report.at("games");
		passed = Expect(games.size() == 2, "report should include two games") && passed;
		passed = Expect(games.at(0).at("orientation") == "normal", "first game should be normal orientation") && passed;
		passed = Expect(games.at(1).at("orientation") == "swapped", "second game should be swapped orientation") && passed;
		passed = Expect(games.at(0).at("players").at(0).at("agentIndex") == 0, "normal slot 0 should use agent 0") && passed;
		passed = Expect(games.at(0).at("players").at(0).at("co") == "andy", "normal slot 0 should use player0 CO") && passed;
		passed = Expect(games.at(1).at("players").at(0).at("agentIndex") == 1, "swapped slot 0 should use agent 1") && passed;
		passed = Expect(games.at(1).at("players").at(0).at("co") == "adder", "swapped slot 0 should use player1 CO") && passed;
	}

	std::filesystem::remove(reportPath);
	return passed;
}

bool OutputExistingRequiresForce() {
	const std::filesystem::path reportPath = MakeTempPath("eval-existing-report.json");
	{
		std::ofstream output(reportPath, std::ios::trunc);
		output << "{}\n";
	}

	EvaluationOptions options = BaseEvaluationOptions(reportPath);
	EvaluationSummary summary;
	EvaluationError error;
	const Result result = RunEvaluation(options, summary, error);

	const bool passed = Expect(result == Result::Failed, "existing output should fail without force") &&
		Expect(error.code == "output-exists", "existing output should fail with output-exists, got " + error.code);

	std::filesystem::remove(reportPath);
	return passed;
}

bool CheckpointAgentRequiresCheckpointPath() {
	EvaluationOptions options = BaseEvaluationOptions(MakeTempPath("eval-missing-checkpoint-report.json"));
	std::filesystem::remove(options.outputPath);
	options.agents[0].type = "checkpoint-policy";

	EvaluationSummary summary;
	EvaluationError error;
	const Result result = RunEvaluation(options, summary, error);

	return Expect(result == Result::Failed, "checkpoint-policy without checkpoint should fail") &&
		Expect(error.code == "missing-checkpoint", "missing checkpoint should fail with missing-checkpoint, got " + error.code);
}
}

int RunEvaluationTests() noexcept {
	try {
		if (!CheckpointPolicyVersusRandomWritesReport()) {
			return 1;
		}
		if (!SwapSidesRunsBothOrientationsAndSwapsAgentCoPairs()) {
			return 1;
		}
		if (!OutputExistingRequiresForce()) {
			return 1;
		}
		if (!CheckpointAgentRequiresCheckpointPath()) {
			return 1;
		}

		std::cout << "Evaluation tests passed" << std::endl;
		return 0;
	}
	catch (const std::exception& err) {
		std::cerr << "Evaluation test exception: " << err.what() << std::endl;
		return 1;
	}
}
