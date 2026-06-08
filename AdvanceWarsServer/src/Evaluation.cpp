#include "Evaluation.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "ActionSpace.h"
#include "PolicyValueModel.h"
#include "StandardGameSetup.h"
#include "StateTensor.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

namespace {
constexpr int kMetadataVersion = 1;

struct LegalActionChoice {
	int actionIndex{ -1 };
	Action action;
};

struct AgentRuntime {
	EvaluationAgentOptions options;
	PolicyValueNetwork model{ nullptr };
	json checkpointMetadata{ json::object() };
	json trainingMetadata{ nullptr };
};

struct GameEvaluationRecord {
	int round{ 0 };
	std::string orientation;
	std::uint32_t seed{ 0 };
	std::array<int, 2> slotAgentIndices{ { 0, 1 } };
	std::array<std::string, 2> slotCoIds;
	std::array<std::string, 2> slotArmyTypes;
	json winner{ nullptr };
	json winningAgent{ nullptr };
	json terminalReason{ nullptr };
	int actions{ 0 };
	int turns{ 0 };
	double elapsedMs{ 0.0 };
	double totalActionSelectionMs{ 0.0 };
	int invalidActionCount{ 0 };
	std::array<double, 2> valuePredictionTotals{ { 0.0, 0.0 } };
	std::array<int, 2> valuePredictionCounts{ { 0, 0 } };
};

void SetError(
	EvaluationError& error,
	const std::string& code,
	const std::string& message,
	const std::filesystem::path& path = {},
	int gameIndex = -1,
	int ply = -1) {
	error.code = code;
	error.message = message;
	error.path = path;
	error.gameIndex = gameIndex;
	error.ply = ply;
}

std::string CurrentUtcTimestamp() {
	const std::time_t now = std::time(nullptr);
	std::tm utc{};
	gmtime_s(&utc, &now);
	char buffer[32]{};
	std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%SZ", &utc);
	return buffer;
}

bool IsCheckpointPolicy(const std::string& type) {
	return type == "checkpoint-policy";
}

bool IsRandomAgent(const std::string& type) {
	return type == "random";
}

bool IsKnownAgentType(const std::string& type) {
	return IsCheckpointPolicy(type) || IsRandomAgent(type);
}

std::uint32_t MixSeed(std::uint32_t seed, std::uint32_t value) {
	seed ^= value + 0x9e3779b9u + (seed << 6) + (seed >> 2);
	seed ^= seed >> 16;
	seed *= 0x7feb352du;
	seed ^= seed >> 15;
	seed *= 0x846ca68bu;
	seed ^= seed >> 16;
	return seed;
}

std::uint32_t DeriveGameSeed(const EvaluationOptions& options, int round, bool swapped) {
	std::uint32_t seed = options.baseSeed;
	seed = MixSeed(seed, static_cast<std::uint32_t>(round));
	seed = MixSeed(seed, swapped ? 0x51f15eedu : 0x0u);
	return seed;
}

std::uint32_t DeriveActionSeed(std::uint32_t gameSeed, int ply, int agentIndex) {
	std::uint32_t seed = MixSeed(gameSeed, 0xace00000u);
	seed = MixSeed(seed, static_cast<std::uint32_t>(ply));
	seed = MixSeed(seed, static_cast<std::uint32_t>(agentIndex));
	return seed;
}

json SettingsJsonFromOptions(const EvaluationOptions& options) {
	return StandardSettingsJson(
		options.unitCap,
		options.captureLimit,
		options.hasDayLimit ? std::optional<int>(options.dayLimit) : std::nullopt,
		options.heuristicAutoResign);
}

Result ReadJsonFile(const std::filesystem::path& path, json& value, EvaluationError& error) {
	std::ifstream input(path);
	if (!input.is_open()) {
		SetError(error, "json-read-failed", "could not open JSON file: " + path.string(), path);
		return Result::Failed;
	}

	try {
		input >> value;
	}
	catch (const std::exception& err) {
		SetError(error, "json-parse-failed", err.what(), path);
		return Result::Failed;
	}
	return Result::Succeeded;
}

json OptionalTrainingMetadata(const std::filesystem::path& checkpointPath) {
	const std::filesystem::path trainingPath = checkpointPath / "training.json";
	if (!std::filesystem::exists(trainingPath)) {
		return nullptr;
	}

	std::ifstream input(trainingPath);
	if (!input.is_open()) {
		return nullptr;
	}

	try {
		json training;
		input >> training;
		ordered_json selected;
		for (const std::string& field : {
			"trainingMetadataVersion",
			"checkpointRole",
			"completedEpoch",
			"replayPath",
			"samplesTrained",
			"averagePolicyLoss",
			"averageValueLoss",
			"averageTotalLoss",
		}) {
			if (training.contains(field)) {
				selected[field] = training.at(field);
			}
		}
		return selected;
	}
	catch (const std::exception&) {
		return nullptr;
	}
}

json SelectedCheckpointMetadata(const json& metadata) {
	ordered_json selected;
	for (const std::string& field : {
		"metadataVersion",
		"modelVersion",
		"createdAt",
		"validatedDevice",
		"seed",
		"architecture",
		"parameterCount",
	}) {
		if (metadata.contains(field)) {
			selected[field] = metadata.at(field);
		}
	}
	return selected;
}

Result ValidateOptions(const EvaluationOptions& options, EvaluationError& error) {
	if (options.outputPath.empty()) {
		SetError(error, "missing-out", "--out is required");
		return Result::Failed;
	}
	if (options.mapId.empty()) {
		SetError(error, "missing-map", "--map is required");
		return Result::Failed;
	}
	if (options.player0CoId.empty() || options.player1CoId.empty()) {
		SetError(error, "missing-co", "--player0-co and --player1-co are required");
		return Result::Failed;
	}
	if (options.rounds < 1) {
		SetError(error, "invalid-rounds", "--rounds must be at least 1");
		return Result::Failed;
	}
	if (options.maxActions < 1) {
		SetError(error, "invalid-max-actions", "--max-actions must be at least 1");
		return Result::Failed;
	}
	if (options.unitCap < 1) {
		SetError(error, "invalid-unit-cap", "--unit-cap must be at least 1");
		return Result::Failed;
	}
	if (options.captureLimit < 1) {
		SetError(error, "invalid-capture-limit", "--capture-limit must be at least 1");
		return Result::Failed;
	}
	if (options.hasDayLimit && options.dayLimit < 1) {
		SetError(error, "invalid-day-limit", "--day-limit must be at least 1");
		return Result::Failed;
	}
	if (!std::isfinite(options.promotionScoreThreshold) || options.promotionScoreThreshold < 0.0 || options.promotionScoreThreshold > 1.0) {
		SetError(error, "invalid-promotion-threshold", "--promotion-score-threshold must be between 0 and 1");
		return Result::Failed;
	}
	if (options.minPromotionRounds < 1) {
		SetError(error, "invalid-min-promotion-rounds", "--min-promotion-rounds must be at least 1");
		return Result::Failed;
	}
	for (int i = 0; i < 2; ++i) {
		const EvaluationAgentOptions& agent = options.agents[static_cast<std::size_t>(i)];
		if (!IsKnownAgentType(agent.type)) {
			SetError(error, "invalid-agent-type", "agent type must be checkpoint-policy or random");
			return Result::Failed;
		}
		if (IsCheckpointPolicy(agent.type) && agent.checkpointPath.empty()) {
			SetError(error, "missing-checkpoint", "--checkpoint" + std::to_string(i) + " is required for checkpoint-policy");
			return Result::Failed;
		}
		if (IsRandomAgent(agent.type) && !agent.checkpointPath.empty()) {
			SetError(error, "unexpected-checkpoint", "random agents must not receive checkpoint paths");
			return Result::Failed;
		}
	}
	if (std::filesystem::exists(options.outputPath) && !options.force) {
		SetError(error, "output-exists", "output file already exists; pass --force to overwrite", options.outputPath);
		return Result::Failed;
	}
	return Result::Succeeded;
}

Result PrepareOutputPath(const EvaluationOptions& options, EvaluationError& error) {
	const std::filesystem::path parent = options.outputPath.parent_path();
	if (!parent.empty()) {
		std::error_code ec;
		std::filesystem::create_directories(parent, ec);
		if (ec) {
			SetError(error, "mkdir-failed", "could not create output directory: " + parent.string(), parent);
			return Result::Failed;
		}
	}
	if (std::filesystem::exists(options.outputPath)) {
		std::error_code ec;
		std::filesystem::remove(options.outputPath, ec);
		if (ec) {
			SetError(error, "output-remove-failed", "could not remove existing output file: " + options.outputPath.string(), options.outputPath);
			return Result::Failed;
		}
	}
	return Result::Succeeded;
}

Result LoadAgents(
	const EvaluationOptions& options,
	std::array<AgentRuntime, 2>& agents,
	const torch::Device& device,
	EvaluationError& error) {
	for (int i = 0; i < 2; ++i) {
		AgentRuntime& runtime = agents[static_cast<std::size_t>(i)];
		runtime.options = options.agents[static_cast<std::size_t>(i)];
		if (!IsCheckpointPolicy(runtime.options.type)) {
			continue;
		}

		PolicyValueModelConfig config;
		std::int64_t seed = -1;
		PolicyValueModelError modelError;
		if (LoadPolicyValueCheckpoint(runtime.options.checkpointPath, runtime.model, config, seed, modelError) == Result::Failed) {
			SetError(error, modelError.code, modelError.message, runtime.options.checkpointPath);
			return Result::Failed;
		}
		runtime.model->to(device);
		runtime.model->eval();

		json metadata;
		IfFailedReturn(ReadJsonFile(runtime.options.checkpointPath / "metadata.json", metadata, error));
		runtime.checkpointMetadata = SelectedCheckpointMetadata(metadata);
		runtime.trainingMetadata = OptionalTrainingMetadata(runtime.options.checkpointPath);
	}
	return Result::Succeeded;
}

Result BuildLegalActionChoices(const GameState& gameState, std::vector<LegalActionChoice>& choices, EvaluationError& error, int gameIndex, int ply) {
	choices.clear();
	std::vector<Action> legalActions;
	if (gameState.GetValidActions(legalActions) == Result::Failed) {
		SetError(error, "legal-actions-failed", "legal action generation failed", {}, gameIndex, ply);
		return Result::Failed;
	}
	for (const Action& action : legalActions) {
		LegalActionChoice choice;
		choice.action = action;
		if (ActionSpace::EncodeAction(action, choice.actionIndex) == Result::Failed) {
			SetError(error, "action-encoding-failed", "legal action failed to encode", {}, gameIndex, ply);
			return Result::Failed;
		}
		choices.push_back(choice);
	}
	std::sort(choices.begin(), choices.end(), [](const LegalActionChoice& left, const LegalActionChoice& right) {
		return left.actionIndex < right.actionIndex;
	});
	return Result::Succeeded;
}

Result SelectRandomAction(
	const std::vector<LegalActionChoice>& choices,
	std::uint32_t actionSeed,
	Action& selectedAction,
	EvaluationError& error,
	int gameIndex,
	int ply) {
	if (choices.empty()) {
		SetError(error, "no-legal-actions", "non-terminal state has no legal actions", {}, gameIndex, ply);
		return Result::Failed;
	}
	std::mt19937 rng(actionSeed);
	std::uniform_int_distribution<int> distribution(0, static_cast<int>(choices.size() - 1));
	selectedAction = choices[static_cast<std::size_t>(distribution(rng))].action;
	return Result::Succeeded;
}

Result SelectCheckpointPolicyAction(
	AgentRuntime& agent,
	const GameState& gameState,
	const std::vector<LegalActionChoice>& choices,
	const torch::Device& device,
	Action& selectedAction,
	std::optional<double>& valuePrediction,
	EvaluationError& error,
	int gameIndex,
	int ply) {
	if (choices.empty()) {
		SetError(error, "no-legal-actions", "non-terminal state has no legal actions", {}, gameIndex, ply);
		return Result::Failed;
	}

	std::vector<float> values;
	if (StateTensor::Encode(gameState, values) == Result::Failed) {
		SetError(error, "state-tensor-failed", "state tensor encoding failed", {}, gameIndex, ply);
		return Result::Failed;
	}

	torch::Tensor input = torch::from_blob(
		values.data(),
		{ 1, StateTensor::ChannelCount(), StateTensor::BoardHeight(), StateTensor::BoardWidth() },
		torch::TensorOptions().dtype(torch::kFloat32)).clone().contiguous().to(device);

	PolicyValueNetworkOutput output;
	PolicyValueModelError modelError;
	if (RunPolicyValueInference(agent.model, input, output, modelError) == Result::Failed) {
		SetError(error, modelError.code, modelError.message, agent.options.checkpointPath, gameIndex, ply);
		return Result::Failed;
	}

	const torch::Tensor policyLogits = output.policyLogits.index({ 0 }).to(torch::kCPU);
	valuePrediction = output.value.view({ -1 }).index({ 0 }).to(torch::kCPU).item<double>();

	double bestLogit = -std::numeric_limits<double>::infinity();
	const LegalActionChoice* bestChoice = nullptr;
	for (const LegalActionChoice& choice : choices) {
		const double logit = policyLogits.index({ choice.actionIndex }).item<double>();
		if (bestChoice == nullptr || logit > bestLogit) {
			bestChoice = &choice;
			bestLogit = logit;
		}
	}

	if (bestChoice == nullptr) {
		SetError(error, "no-legal-actions", "non-terminal state has no legal actions", {}, gameIndex, ply);
		return Result::Failed;
	}

	selectedAction = bestChoice->action;
	return Result::Succeeded;
}

json AgentJson(const AgentRuntime& agent, int agentIndex) {
	ordered_json item;
	item["agentIndex"] = agentIndex;
	item["type"] = agent.options.type;
	if (IsCheckpointPolicy(agent.options.type)) {
		ordered_json checkpoint;
		checkpoint["checkpointPath"] = agent.options.checkpointPath.string();
		checkpoint["metadata"] = agent.checkpointMetadata;
		if (!agent.trainingMetadata.is_null()) {
			checkpoint["training"] = agent.trainingMetadata;
		}
		item["checkpoint"] = checkpoint;
	}
	return item;
}

json PlayersJson(const GameEvaluationRecord& record, const std::array<AgentRuntime, 2>& agents) {
	json players = json::array();
	for (int slot = 0; slot < 2; ++slot) {
		const int agentIndex = record.slotAgentIndices[static_cast<std::size_t>(slot)];
		players.push_back({
			{ "slot", slot },
			{ "agentIndex", agentIndex },
			{ "agentType", agents[static_cast<std::size_t>(agentIndex)].options.type },
			{ "co", record.slotCoIds[static_cast<std::size_t>(slot)] },
			{ "armyType", record.slotArmyTypes[static_cast<std::size_t>(slot)] },
		});
	}
	return players;
}

json AverageValuePredictionsJson(const GameEvaluationRecord& record) {
	ordered_json predictions;
	for (int agentIndex = 0; agentIndex < 2; ++agentIndex) {
		const int count = record.valuePredictionCounts[static_cast<std::size_t>(agentIndex)];
		const std::string key = agentIndex == 0 ? "agent0" : "agent1";
		if (count == 0) {
			predictions[key] = nullptr;
		}
		else {
			predictions[key] = record.valuePredictionTotals[static_cast<std::size_t>(agentIndex)] / count;
		}
	}
	return predictions;
}

json GameRecordJson(const GameEvaluationRecord& record, const std::array<AgentRuntime, 2>& agents, const std::string& mapId) {
	ordered_json item;
	item["round"] = record.round;
	item["orientation"] = record.orientation;
	item["seed"] = record.seed;
	item["mapId"] = mapId;
	item["players"] = PlayersJson(record, agents);
	item["winner"] = record.winner;
	item["winningAgent"] = record.winningAgent;
	item["terminalReason"] = record.terminalReason;
	item["actions"] = record.actions;
	item["turns"] = record.turns;
	item["elapsedMs"] = record.elapsedMs;
	item["averageActionSelectionMs"] = record.actions == 0 ? 0.0 : record.totalActionSelectionMs / record.actions;
	item["invalidActionCount"] = record.invalidActionCount;
	item["averageValuePrediction"] = AverageValuePredictionsJson(record);
	return item;
}

Result CreateEvaluationGame(
	const EvaluationOptions& options,
	const GameEvaluationRecord& planned,
	StandardGameSetupResult& setup,
	EvaluationError& error,
	int gameIndex) {
	json request;
	StandardGameSetupError setupError;
	if (BuildStandardGameRequestFromSlots(
		options.mapId,
		planned.slotCoIds[0],
		planned.slotCoIds[1],
		SettingsJsonFromOptions(options),
		planned.seed,
		request,
		setupError) == Result::Failed) {
		SetError(error, setupError.code, setupError.message, {}, gameIndex);
		return Result::Failed;
	}
	if (CreateStandardGameFromRequest(request, setup, setupError) == Result::Failed) {
		SetError(error, setupError.code, setupError.message, {}, gameIndex);
		return Result::Failed;
	}
	return Result::Succeeded;
}

Result RunOneGame(
	const EvaluationOptions& options,
	std::array<AgentRuntime, 2>& agents,
	const torch::Device& device,
	GameEvaluationRecord& record,
	EvaluationError& error,
	int gameIndex) {
	StandardGameSetupResult setup;
	IfFailedReturn(CreateEvaluationGame(options, record, setup, error, gameIndex));
	record.slotArmyTypes = setup.playerArmyTypes;

	GameState gameState = setup.gameState;
	const auto gameStart = std::chrono::steady_clock::now();
	for (int ply = 0; ply < options.maxActions && !gameState.FGameOver(); ++ply) {
		const int activeSlot = gameState.getCurrentPlayer();
		const int agentIndex = record.slotAgentIndices[static_cast<std::size_t>(activeSlot)];
		AgentRuntime& agent = agents[static_cast<std::size_t>(agentIndex)];

		std::vector<LegalActionChoice> choices;
		IfFailedReturn(BuildLegalActionChoices(gameState, choices, error, gameIndex, ply));

		Action selectedAction;
		std::optional<double> valuePrediction;
		const std::uint32_t actionSeed = DeriveActionSeed(record.seed, ply, agentIndex);
		const auto selectionStart = std::chrono::steady_clock::now();
		if (IsRandomAgent(agent.options.type)) {
			IfFailedReturn(SelectRandomAction(choices, actionSeed, selectedAction, error, gameIndex, ply));
		}
		else {
			IfFailedReturn(SelectCheckpointPolicyAction(agent, gameState, choices, device, selectedAction, valuePrediction, error, gameIndex, ply));
		}
		const auto selectionEnd = std::chrono::steady_clock::now();
		record.totalActionSelectionMs += std::chrono::duration<double, std::milli>(selectionEnd - selectionStart).count();
		if (valuePrediction.has_value()) {
			record.valuePredictionTotals[static_cast<std::size_t>(agentIndex)] += valuePrediction.value();
			++record.valuePredictionCounts[static_cast<std::size_t>(agentIndex)];
		}

		if (gameState.DoAction(selectedAction) == Result::Failed) {
			++record.invalidActionCount;
			SetError(error, "action-apply-failed", "selected action failed during execution", {}, gameIndex, ply);
			return Result::Failed;
		}
		++record.actions;
		if (gameState.FHeuristicAutoResign()) {
			gameState.CheckPlayerResigns();
		}
	}

	const auto gameEnd = std::chrono::steady_clock::now();
	record.elapsedMs = std::chrono::duration<double, std::milli>(gameEnd - gameStart).count();
	record.turns = gameState.GetTurnCount();

	if (gameState.FGameOver()) {
		const int winner = gameState.getWinningPlayer();
		record.winner = winner;
		record.terminalReason = gameState.GetTerminalReason().has_value() ? json(gameState.GetTerminalReason().value()) : json(nullptr);
		if (winner == 0 || winner == 1) {
			record.winningAgent = record.slotAgentIndices[static_cast<std::size_t>(winner)];
		}
		else {
			record.winningAgent = nullptr;
		}
	}
	else {
		record.winner = nullptr;
		record.winningAgent = nullptr;
		record.terminalReason = "action-limit";
	}
	return Result::Succeeded;
}

std::vector<GameEvaluationRecord> BuildSchedule(const EvaluationOptions& options) {
	std::vector<GameEvaluationRecord> schedule;
	schedule.reserve(static_cast<std::size_t>(options.rounds * (options.swapSides ? 2 : 1)));
	for (int round = 0; round < options.rounds; ++round) {
		GameEvaluationRecord normal;
		normal.round = round;
		normal.orientation = "normal";
		normal.seed = DeriveGameSeed(options, round, false);
		normal.slotAgentIndices = { { 0, 1 } };
		normal.slotCoIds = { { options.player0CoId, options.player1CoId } };
		schedule.push_back(normal);

		if (options.swapSides) {
			GameEvaluationRecord swapped;
			swapped.round = round;
			swapped.orientation = "swapped";
			swapped.seed = DeriveGameSeed(options, round, true);
			swapped.slotAgentIndices = { { 1, 0 } };
			swapped.slotCoIds = { { options.player1CoId, options.player0CoId } };
			schedule.push_back(swapped);
		}
	}
	return schedule;
}

std::string PromotionDecision(const EvaluationOptions& options, const EvaluationSummary& summary) {
	if (summary.rounds < options.minPromotionRounds) {
		return "insufficient-data";
	}
	if (summary.games == 0 || summary.noResults == summary.games) {
		return "insufficient-data";
	}
	if (summary.agent0OverallScoreRate >= options.promotionScoreThreshold &&
		(summary.decisiveGames == 0 || summary.agent0DecisiveWinRate >= 0.50)) {
		return "promote";
	}
	return "reject";
}

std::string PromotionReason(const EvaluationOptions& options, const EvaluationSummary& summary) {
	if (summary.rounds < options.minPromotionRounds) {
		return "completed rounds below minPromotionRounds";
	}
	if (summary.games == 0 || summary.noResults == summary.games) {
		return "all games ended without a result";
	}
	if (summary.promotionDecision == "promote") {
		return "agent0 met score and decisive win-rate thresholds";
	}
	return "agent0 did not meet score or decisive win-rate thresholds";
}

void AccumulateSummary(const std::vector<GameEvaluationRecord>& records, EvaluationSummary& summary) {
	summary = EvaluationSummary{};
	summary.games = static_cast<int>(records.size());
	double agentScores[2]{ 0.0, 0.0 };
	double actionsTotal = 0.0;
	double turnsTotal = 0.0;
	double actionSelectionTotal = 0.0;
	int actionSelectionDenominator = 0;

	for (const GameEvaluationRecord& record : records) {
		actionsTotal += record.actions;
		turnsTotal += record.turns;
		actionSelectionTotal += record.totalActionSelectionMs;
		actionSelectionDenominator += record.actions;

		if (record.winner.is_null()) {
			++summary.noResults;
			agentScores[0] += 0.5;
			agentScores[1] += 0.5;
		}
		else if (record.winner.is_number_integer() && record.winner.get<int>() == 2) {
			++summary.draws;
			agentScores[0] += 0.5;
			agentScores[1] += 0.5;
		}
		else if (record.winningAgent.is_number_integer()) {
			const int winningAgent = record.winningAgent.get<int>();
			++summary.wins[static_cast<std::size_t>(winningAgent)];
			agentScores[static_cast<std::size_t>(winningAgent)] += 1.0;
			++summary.decisiveGames;
		}
	}

	if (summary.games > 0) {
		summary.agent0OverallScoreRate = agentScores[0] / summary.games;
		summary.agent1OverallScoreRate = agentScores[1] / summary.games;
		summary.averageActions = actionsTotal / summary.games;
		summary.averageTurns = turnsTotal / summary.games;
	}
	if (summary.decisiveGames > 0) {
		summary.agent0DecisiveWinRate = static_cast<double>(summary.wins[0]) / summary.decisiveGames;
		summary.agent1DecisiveWinRate = static_cast<double>(summary.wins[1]) / summary.decisiveGames;
	}
	if (actionSelectionDenominator > 0) {
		summary.averageActionSelectionMs = actionSelectionTotal / actionSelectionDenominator;
	}
}

json TerminalReasonsJson(const std::vector<GameEvaluationRecord>& records) {
	std::map<std::string, int> counts;
	for (const GameEvaluationRecord& record : records) {
		const std::string reason = record.terminalReason.is_string() ? record.terminalReason.get<std::string>() : "null";
		++counts[reason];
	}

	ordered_json result;
	for (const auto& count : counts) {
		result[count.first] = count.second;
	}
	return result;
}

json SummaryJson(const EvaluationSummary& summary, const std::vector<GameEvaluationRecord>& records) {
	ordered_json result;
	result["rounds"] = summary.rounds;
	result["games"] = summary.games;
	result["wins"] = ordered_json{
		{ "agent0", summary.wins[0] },
		{ "agent1", summary.wins[1] },
	};
	result["draws"] = summary.draws;
	result["noResult"] = summary.noResults;
	result["decisiveGames"] = summary.decisiveGames;
	result["decisiveWinRate"] = ordered_json{
		{ "agent0", summary.agent0DecisiveWinRate },
		{ "agent1", summary.agent1DecisiveWinRate },
	};
	result["overallScoreRate"] = ordered_json{
		{ "agent0", summary.agent0OverallScoreRate },
		{ "agent1", summary.agent1OverallScoreRate },
	};
	result["terminalReasons"] = TerminalReasonsJson(records);
	result["averageActions"] = summary.averageActions;
	result["averageTurns"] = summary.averageTurns;
	result["averageActionSelectionMs"] = summary.averageActionSelectionMs;
	return result;
}

json ConfigJson(const EvaluationOptions& options, const std::string& resolvedDevice) {
	ordered_json config;
	config["mapId"] = options.mapId;
	config["player0Co"] = options.player0CoId;
	config["player1Co"] = options.player1CoId;
	config["rounds"] = options.rounds;
	config["seed"] = options.baseSeed;
	config["swapSides"] = options.swapSides;
	config["maxActions"] = options.maxActions;
	config["settings"] = SettingsJsonFromOptions(options);
	config["device"] = resolvedDevice;
	config["promotionScoreThreshold"] = options.promotionScoreThreshold;
	config["minPromotionRounds"] = options.minPromotionRounds;
	return config;
}

json PromotionRecommendationJson(const EvaluationOptions& options, const EvaluationSummary& summary) {
	ordered_json recommendation;
	recommendation["decision"] = summary.promotionDecision;
	recommendation["reason"] = PromotionReason(options, summary);
	recommendation["scoreThreshold"] = options.promotionScoreThreshold;
	recommendation["minPromotionRounds"] = options.minPromotionRounds;
	recommendation["agent0OverallScoreRate"] = summary.agent0OverallScoreRate;
	recommendation["agent0DecisiveWinRate"] = summary.agent0DecisiveWinRate;
	return recommendation;
}

json BuildReport(
	const EvaluationOptions& options,
	const EvaluationSummary& summary,
	const std::array<AgentRuntime, 2>& agents,
	const std::vector<GameEvaluationRecord>& records,
	const std::string& resolvedDevice) {
	ordered_json report;
	report["evaluationMetadataVersion"] = kMetadataVersion;
	report["createdAt"] = CurrentUtcTimestamp();
	report["versions"] = ordered_json{
		{ "model", PolicyValueModelVersion() },
		{ "stateTensor", StateTensor::Version() },
		{ "actionSpace", ActionSpace::Version() },
	};
	report["config"] = ConfigJson(options, resolvedDevice);
	report["agents"] = json::array({
		AgentJson(agents[0], 0),
		AgentJson(agents[1], 1),
	});
	report["summary"] = SummaryJson(summary, records);
	report["promotionRecommendation"] = PromotionRecommendationJson(options, summary);
	report["games"] = json::array();
	for (const GameEvaluationRecord& record : records) {
		report["games"].push_back(GameRecordJson(record, agents, options.mapId));
	}
	return report;
}

Result WriteReport(const EvaluationOptions& options, const json& report, EvaluationError& error) {
	std::ofstream output(options.outputPath, std::ios::trunc);
	if (!output.is_open()) {
		SetError(error, "report-write-failed", "could not open evaluation report for writing: " + options.outputPath.string(), options.outputPath);
		return Result::Failed;
	}
	output << report.dump(2) << "\n";
	return Result::Succeeded;
}
}

Result RunEvaluation(const EvaluationOptions& options, EvaluationSummary& summary, EvaluationError& error) noexcept {
	try {
		summary = EvaluationSummary{};
		error = EvaluationError{};
		IfFailedReturn(ValidateOptions(options, error));

		PolicyValueModelError modelError;
		const torch::Device device = ResolvePolicyValueDevice(options.deviceName, modelError);
		if (!modelError.code.empty()) {
			SetError(error, modelError.code, modelError.message);
			return Result::Failed;
		}
		const std::string resolvedDevice = PolicyValueDeviceName(device);

		std::array<AgentRuntime, 2> agents;
		IfFailedReturn(LoadAgents(options, agents, device, error));

		std::vector<GameEvaluationRecord> records = BuildSchedule(options);
		for (std::size_t i = 0; i < records.size(); ++i) {
			IfFailedReturn(RunOneGame(options, agents, device, records[i], error, static_cast<int>(i)));
		}

		AccumulateSummary(records, summary);
		summary.rounds = options.rounds;
		summary.promotionDecision = PromotionDecision(options, summary);
		summary.reportPath = options.outputPath;

		IfFailedReturn(PrepareOutputPath(options, error));
		const json report = BuildReport(options, summary, agents, records, resolvedDevice);
		IfFailedReturn(WriteReport(options, report, error));
		return Result::Succeeded;
	}
	catch (const std::exception& err) {
		SetError(error, "evaluation-exception", err.what());
		return Result::Failed;
	}
}
