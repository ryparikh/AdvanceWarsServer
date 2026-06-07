#include "Training.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <set>
#include <sstream>
#include <vector>

#include <torch/torch.h>

#include "ActionSpace.h"
#include "PolicyValueModel.h"
#include "SelfPlayReplay.h"
#include "StateTensor.h"
#include "nlohmann/json.hpp"

namespace {
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

struct ReplayGameRecord {
	std::filesystem::path path;
	int line{ 0 };
	int gameIndex{ -1 };
	json initialState;
	json actions;
};

struct ReplaySampleRecord {
	int gameRecordIndex{ -1 };
	int ply{ -1 };
	int currentPlayer{ -1 };
	std::string stateTensorChecksum;
	std::vector<int> legalActionIndices;
	std::vector<std::pair<int, int>> visitCounts;
	int selectedActionIndex{ -1 };
	int outcome{ 0 };
};

struct ReplayDataset {
	std::vector<std::filesystem::path> files;
	std::vector<ReplayGameRecord> games;
	std::vector<ReplaySampleRecord> samples;
};

struct BatchData {
	std::vector<float> flatStates;
	std::vector<std::vector<int>> legalActionIndices;
	std::vector<std::vector<std::pair<int, int>>> visitCounts;
	std::vector<float> outcomes;
};

void SetError(TrainingError& error, const std::string& code, const std::string& message, const std::filesystem::path& path = {}, int line = 0, int gameIndex = -1, int ply = -1, int epoch = 0, int batch = 0) {
	error.code = code;
	error.message = message;
	error.replayPath = path;
	error.line = line;
	error.gameIndex = gameIndex;
	error.ply = ply;
	error.epoch = epoch;
	error.batch = batch;
}

std::string ChecksumToHex(std::uint64_t checksum) {
	std::ostringstream stream;
	stream << std::hex << std::setw(16) << std::setfill('0') << checksum;
	return stream.str();
}

std::string CurrentUtcTimestamp() {
	const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm utc{};
	gmtime_s(&utc, &now);
	std::ostringstream stream;
	stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
	return stream.str();
}

double ElapsedSeconds(const std::chrono::steady_clock::time_point& start, const std::chrono::steady_clock::time_point& end) {
	return std::chrono::duration<double>(end - start).count();
}

std::filesystem::path NormalizePath(const std::filesystem::path& path) {
	return std::filesystem::absolute(path).lexically_normal();
}

bool IsKnownTrainingBundleFile(const std::filesystem::path& path) {
	const std::filesystem::path filename = path.filename();
	return filename == "metadata.json" || filename == "model.pt" || filename == "training.json";
}

std::string EpochCheckpointName(int epoch) {
	std::ostringstream stream;
	stream << "epoch-" << std::setw(6) << std::setfill('0') << epoch;
	return stream.str();
}

Result CheckCheckpointOutputAvailable(const std::filesystem::path& path, bool force, TrainingError& error, const std::string& label) {
	if (!std::filesystem::exists(path)) {
		return Result::Succeeded;
	}
	if (!std::filesystem::is_directory(path)) {
		SetError(error, "checkpoint-path-not-directory", label + " exists but is not a directory");
		return Result::Failed;
	}
	if (!force) {
		SetError(error, "checkpoint-exists", label + " already exists; pass --force to overwrite known bundle files");
		return Result::Failed;
	}

	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(path)) {
		if (!IsKnownTrainingBundleFile(entry.path())) {
			SetError(error, "checkpoint-has-unexpected-file", label + " contains unexpected file: " + entry.path().filename().string());
			return Result::Failed;
		}
	}
	return Result::Succeeded;
}

Result ValidateTrainingOptions(const TrainingOptions& options, TrainingError& error) {
	if (options.replayPath.empty()) {
		SetError(error, "missing-replay", "--replay is required");
		return Result::Failed;
	}
	if (options.checkpointIn.empty()) {
		SetError(error, "missing-checkpoint-in", "--checkpoint-in is required");
		return Result::Failed;
	}
	if (options.checkpointOut.empty()) {
		SetError(error, "missing-checkpoint-out", "--checkpoint-out is required");
		return Result::Failed;
	}
	if (options.epochs <= 0) {
		SetError(error, "invalid-epochs", "--epochs must be greater than 0");
		return Result::Failed;
	}
	if (options.batchSize <= 0) {
		SetError(error, "invalid-batch-size", "--batch-size must be greater than 0");
		return Result::Failed;
	}
	if (options.maxSamples.has_value() && options.maxSamples.value() <= 0) {
		SetError(error, "invalid-max-samples", "--max-samples must be greater than 0");
		return Result::Failed;
	}
	if (!std::isfinite(options.learningRate) || options.learningRate <= 0.0) {
		SetError(error, "invalid-learning-rate", "--learning-rate must be finite and greater than 0");
		return Result::Failed;
	}
	if (!std::isfinite(options.weightDecay) || options.weightDecay < 0.0) {
		SetError(error, "invalid-weight-decay", "--weight-decay must be finite and greater than or equal to 0");
		return Result::Failed;
	}
	if (!std::isfinite(options.policyLossWeight) || !std::isfinite(options.valueLossWeight) ||
		options.policyLossWeight < 0.0 || options.valueLossWeight < 0.0 || (options.policyLossWeight == 0.0 && options.valueLossWeight == 0.0)) {
		SetError(error, "invalid-loss-weights", "loss weights must be finite, nonnegative, and at least one must be positive");
		return Result::Failed;
	}
	if (!std::isfinite(options.maxGradNorm) || options.maxGradNorm < 0.0) {
		SetError(error, "invalid-max-grad-norm", "--max-grad-norm must be finite and greater than or equal to 0");
		return Result::Failed;
	}
	if (options.seed < 0) {
		SetError(error, "invalid-seed", "--seed must be nonnegative");
		return Result::Failed;
	}
	if (options.logEveryBatches < 0) {
		SetError(error, "invalid-log-every-batches", "--log-every-batches must be greater than or equal to 0");
		return Result::Failed;
	}
	if (options.checkpointEveryEpoch < 0) {
		SetError(error, "invalid-checkpoint-every-epochs", "--checkpoint-every-epochs must be greater than or equal to 0");
		return Result::Failed;
	}
	if (options.checkpointEveryEpoch > 0 && options.checkpointDir.empty()) {
		SetError(error, "missing-checkpoint-dir", "--checkpoint-dir is required with --checkpoint-every-epochs");
		return Result::Failed;
	}
	if (NormalizePath(options.checkpointIn) == NormalizePath(options.checkpointOut)) {
		SetError(error, "checkpoint-path-conflict", "--checkpoint-in and --checkpoint-out must be different paths");
		return Result::Failed;
	}
	if (!std::filesystem::exists(options.checkpointIn)) {
		SetError(error, "checkpoint-in-missing", "--checkpoint-in does not exist");
		return Result::Failed;
	}
	IfFailedReturn(CheckCheckpointOutputAvailable(options.checkpointOut, options.force, error, "--checkpoint-out"));
	if (options.checkpointEveryEpoch > 0) {
		for (int epoch = options.checkpointEveryEpoch; epoch <= options.epochs; epoch += options.checkpointEveryEpoch) {
			const std::filesystem::path checkpointPath = options.checkpointDir / EpochCheckpointName(epoch);
			IfFailedReturn(CheckCheckpointOutputAvailable(checkpointPath, options.force, error, "periodic checkpoint"));
		}
	}
	return Result::Succeeded;
}

Result ResolveReplayFiles(const std::filesystem::path& replayPath, std::vector<std::filesystem::path>& files, TrainingError& error) {
	files.clear();
	if (!std::filesystem::exists(replayPath)) {
		SetError(error, "replay-missing", "replay path does not exist: " + replayPath.string(), replayPath);
		return Result::Failed;
	}
	if (std::filesystem::is_regular_file(replayPath)) {
		files.push_back(replayPath);
		return Result::Succeeded;
	}
	if (!std::filesystem::is_directory(replayPath)) {
		SetError(error, "replay-path-invalid", "replay path is neither a file nor directory: " + replayPath.string(), replayPath);
		return Result::Failed;
	}

	for (const std::filesystem::directory_entry& entry : std::filesystem::directory_iterator(replayPath)) {
		if (entry.is_regular_file() && entry.path().extension() == ".jsonl") {
			files.push_back(entry.path());
		}
	}
	std::sort(files.begin(), files.end());
	if (files.empty()) {
		SetError(error, "replay-empty", "replay directory contains no .jsonl shards: " + replayPath.string(), replayPath);
		return Result::Failed;
	}
	return Result::Succeeded;
}

bool ReadIntArray(const json& value, std::vector<int>& output) {
	if (!value.is_array()) {
		return false;
	}
	output.clear();
	for (const json& item : value) {
		if (!item.is_number_integer()) {
			return false;
		}
		output.push_back(item.get<int>());
	}
	return true;
}

bool ReadVisitCounts(const json& value, std::vector<std::pair<int, int>>& output) {
	if (!value.is_array()) {
		return false;
	}
	output.clear();
	for (const json& item : value) {
		if (!item.is_object() || !item.contains("actionIndex") || !item.contains("visits") ||
			!item.at("actionIndex").is_number_integer() || !item.at("visits").is_number_integer()) {
			return false;
		}
		output.push_back({ item.at("actionIndex").get<int>(), item.at("visits").get<int>() });
	}
	return true;
}

Result AppendGameSamples(const std::filesystem::path& path, int line, const json& game, const std::optional<int>& maxSamples, ReplayDataset& dataset, TrainingError& error) {
	if (!game.is_object() || !game.contains("initialState") || !game.contains("actions") || !game.contains("samples")) {
		SetError(error, "replay-schema-error", "game record is missing required training fields", path, line);
		return Result::Failed;
	}

	ReplayGameRecord gameRecord;
	gameRecord.path = path;
	gameRecord.line = line;
	gameRecord.gameIndex = game.value("gameIndex", -1);
	gameRecord.initialState = game.at("initialState");
	gameRecord.actions = game.at("actions");
	const int gameRecordIndex = static_cast<int>(dataset.games.size());
	dataset.games.push_back(std::move(gameRecord));

	const json& samples = game.at("samples");
	for (const json& sample : samples) {
		if (maxSamples.has_value() && static_cast<int>(dataset.samples.size()) >= maxSamples.value()) {
			break;
		}
		ReplaySampleRecord record;
		record.gameRecordIndex = gameRecordIndex;
		record.ply = sample.at("ply").get<int>();
		record.currentPlayer = sample.at("currentPlayer").get<int>();
		record.stateTensorChecksum = sample.at("stateTensorChecksum").get<std::string>();
		record.selectedActionIndex = sample.at("selectedActionIndex").get<int>();
		record.outcome = sample.at("outcome").get<int>();
		if (!ReadIntArray(sample.at("legalActionIndices"), record.legalActionIndices) ||
			!ReadVisitCounts(sample.at("visitCounts"), record.visitCounts)) {
			SetError(error, "replay-schema-error", "sample has malformed legal action or visit-count arrays", path, line, gameRecord.gameIndex, record.ply);
			return Result::Failed;
		}
		dataset.samples.push_back(std::move(record));
	}
	return Result::Succeeded;
}

Result ParseReplayFile(const std::filesystem::path& path, const std::optional<int>& maxSamples, ReplayDataset& dataset, TrainingError& error) {
	std::ifstream input(path);
	if (!input.is_open()) {
		SetError(error, "replay-open-failed", "could not open replay file: " + path.string(), path);
		return Result::Failed;
	}

	std::string line;
	int lineNumber = 0;
	while (std::getline(input, line)) {
		++lineNumber;
		if (lineNumber == 1) {
			continue;
		}
		if (maxSamples.has_value() && static_cast<int>(dataset.samples.size()) >= maxSamples.value()) {
			break;
		}
		json record;
		try {
			record = json::parse(line);
		}
		catch (const std::exception& err) {
			SetError(error, "replay-json-error", err.what(), path, lineNumber);
			return Result::Failed;
		}
		if (record.value("recordType", "") == "game") {
			IfFailedReturn(AppendGameSamples(path, lineNumber, record, maxSamples, dataset, error));
		}
	}
	return Result::Succeeded;
}

Result LoadReplayDataset(const TrainingOptions& options, ReplayDataset& dataset, TrainingError& error) {
	dataset = ReplayDataset{};
	IfFailedReturn(ResolveReplayFiles(options.replayPath, dataset.files, error));

	for (const std::filesystem::path& path : dataset.files) {
		SelfPlayReplayValidationSummary validationSummary;
		SelfPlayError replayError;
		if (ValidateSelfPlayReplay(path, validationSummary, replayError) == Result::Failed) {
			SetError(error, replayError.code, replayError.message, path, replayError.line, replayError.gameIndex, replayError.ply);
			return Result::Failed;
		}
		IfFailedReturn(ParseReplayFile(path, options.maxSamples, dataset, error));
		if (options.maxSamples.has_value() && static_cast<int>(dataset.samples.size()) >= options.maxSamples.value()) {
			break;
		}
	}

	if (dataset.samples.empty()) {
		SetError(error, "replay-empty", "replay input contains no training samples", options.replayPath);
		return Result::Failed;
	}
	return Result::Succeeded;
}

Result EncodeLegalActionIndices(const GameState& gameState, std::vector<int>& legalActionIndices, TrainingError& error, const ReplayGameRecord& game, int ply) {
	std::vector<Action> legalActions;
	if (gameState.GetValidActions(legalActions) == Result::Failed) {
		SetError(error, "legal-actions-failed", "legal action generation failed", game.path, game.line, game.gameIndex, ply);
		return Result::Failed;
	}
	legalActionIndices.clear();
	for (const Action& action : legalActions) {
		int actionIndex = -1;
		if (ActionSpace::EncodeAction(action, actionIndex) == Result::Failed) {
			SetError(error, "action-encoding-failed", "legal action failed to encode", game.path, game.line, game.gameIndex, ply);
			return Result::Failed;
		}
		legalActionIndices.push_back(actionIndex);
	}
	std::sort(legalActionIndices.begin(), legalActionIndices.end());
	legalActionIndices.erase(std::unique(legalActionIndices.begin(), legalActionIndices.end()), legalActionIndices.end());
	return Result::Succeeded;
}

Result ReconstructStateForSample(const ReplayGameRecord& game, const ReplaySampleRecord& sample, GameState& gameState, TrainingError& error) {
	try {
		json initialState = game.initialState;
		GameState::from_json(initialState, gameState);
		for (int i = 0; i < sample.ply; ++i) {
			json actionJson = game.actions.at(i).at("action");
			Action action;
			from_json(actionJson, action);
			if (gameState.DoAction(action) == Result::Failed) {
				SetError(error, "action-apply-failed", "recorded action failed during state reconstruction", game.path, game.line, game.gameIndex, i);
				return Result::Failed;
			}
			if (gameState.FHeuristicAutoResign()) {
				gameState.CheckPlayerResigns();
			}
		}
		return Result::Succeeded;
	}
	catch (const std::exception& err) {
		SetError(error, "state-reconstruction-failed", err.what(), game.path, game.line, game.gameIndex, sample.ply);
		return Result::Failed;
	}
}

Result AddSampleToBatch(const ReplayDataset& dataset, int sampleIndex, BatchData& batch, TrainingError& error) {
	const ReplaySampleRecord& sample = dataset.samples.at(sampleIndex);
	const ReplayGameRecord& game = dataset.games.at(sample.gameRecordIndex);
	GameState gameState;
	IfFailedReturn(ReconstructStateForSample(game, sample, gameState, error));

	std::vector<int> legalActionIndices;
	IfFailedReturn(EncodeLegalActionIndices(gameState, legalActionIndices, error, game, sample.ply));
	if (legalActionIndices != sample.legalActionIndices) {
		SetError(error, "legal-actions-mismatch", "legalActionIndices do not match reconstructed state", game.path, game.line, game.gameIndex, sample.ply);
		return Result::Failed;
	}

	std::vector<float> values;
	if (StateTensor::Encode(gameState, values) == Result::Failed) {
		SetError(error, "state-tensor-failed", "state tensor encoding failed", game.path, game.line, game.gameIndex, sample.ply);
		return Result::Failed;
	}
	std::uint64_t rawChecksum = 0;
	if (StateTensor::Checksum(values, rawChecksum) == Result::Failed) {
		SetError(error, "state-tensor-checksum-failed", "state tensor checksum failed", game.path, game.line, game.gameIndex, sample.ply);
		return Result::Failed;
	}
	if (ChecksumToHex(rawChecksum) != sample.stateTensorChecksum) {
		SetError(error, "state-checksum-mismatch", "state tensor checksum does not match replay sample", game.path, game.line, game.gameIndex, sample.ply);
		return Result::Failed;
	}

	batch.flatStates.insert(batch.flatStates.end(), values.begin(), values.end());
	batch.legalActionIndices.push_back(sample.legalActionIndices);
	batch.visitCounts.push_back(sample.visitCounts);
	batch.outcomes.push_back(static_cast<float>(sample.outcome));
	return Result::Succeeded;
}

Result BuildBatch(const ReplayDataset& dataset, const std::vector<int>& order, int start, int count, BatchData& batch, TrainingError& error) {
	batch = BatchData{};
	for (int i = 0; i < count; ++i) {
		IfFailedReturn(AddSampleToBatch(dataset, order.at(start + i), batch, error));
	}
	return Result::Succeeded;
}

torch::Tensor MakeInputTensor(const BatchData& batch, int batchSize, const torch::Device& device) {
	return torch::from_blob(
		const_cast<float*>(batch.flatStates.data()),
		{ batchSize, StateTensor::ChannelCount(), StateTensor::BoardHeight(), StateTensor::BoardWidth() },
		torch::TensorOptions().dtype(torch::kFloat32)).clone().contiguous().to(device);
}

torch::Tensor ComputePolicyLoss(const torch::Tensor& policyLogits, const BatchData& batch, const torch::Device& device) {
	std::vector<torch::Tensor> losses;
	for (std::size_t i = 0; i < batch.legalActionIndices.size(); ++i) {
		const std::vector<int>& legal = batch.legalActionIndices[i];
		torch::Tensor legalIndices = torch::tensor(legal, torch::TensorOptions().dtype(torch::kInt64)).to(device);
		torch::Tensor legalLogProbs = torch::log_softmax(policyLogits[static_cast<std::int64_t>(i)].index_select(0, legalIndices), 0);

		std::vector<float> targets(legal.size(), 0.0f);
		int visitSum = 0;
		for (const auto& visit : batch.visitCounts[i]) {
			visitSum += visit.second;
		}
		if (visitSum > 0) {
			for (const auto& visit : batch.visitCounts[i]) {
				auto it = std::lower_bound(legal.begin(), legal.end(), visit.first);
				if (it != legal.end() && *it == visit.first) {
					targets[static_cast<std::size_t>(std::distance(legal.begin(), it))] = static_cast<float>(visit.second) / static_cast<float>(visitSum);
				}
			}
		}

		torch::Tensor targetTensor = torch::from_blob(targets.data(), { static_cast<std::int64_t>(targets.size()) }, torch::TensorOptions().dtype(torch::kFloat32)).clone().to(device);
		losses.push_back(-(targetTensor * legalLogProbs).sum());
	}
	return torch::stack(losses).mean();
}

ordered_json EpochSummaryJson(const TrainingEpochSummary& epoch) {
	ordered_json jsonEpoch;
	jsonEpoch["epoch"] = epoch.epoch;
	jsonEpoch["samples"] = epoch.samples;
	jsonEpoch["batches"] = epoch.batches;
	jsonEpoch["averagePolicyLoss"] = epoch.averagePolicyLoss;
	jsonEpoch["averageValueLoss"] = epoch.averageValueLoss;
	jsonEpoch["averageTotalLoss"] = epoch.averageTotalLoss;
	jsonEpoch["elapsedSeconds"] = epoch.elapsedSeconds;
	if (!epoch.checkpointPath.empty()) {
		jsonEpoch["checkpointPath"] = epoch.checkpointPath.string();
	}
	return jsonEpoch;
}

ordered_json BuildTrainingMetadata(
	const TrainingOptions& options,
	const TrainingSummary& summary,
	const std::vector<TrainingEpochSummary>& epochHistory,
	const std::string& checkpointRole,
	int completedEpoch,
	const std::filesystem::path& checkpointPath) {
	ordered_json metadata;
	metadata["trainingMetadataVersion"] = 1;
	metadata["createdAt"] = CurrentUtcTimestamp();
	metadata["checkpointRole"] = checkpointRole;
	metadata["completedEpoch"] = completedEpoch;
	metadata["replayPath"] = options.replayPath.string();
	metadata["checkpointIn"] = options.checkpointIn.string();
	metadata["checkpointPath"] = checkpointPath.string();
	metadata["sampleCount"] = summary.samplesLoaded;
	metadata["samplesTrained"] = summary.samplesTrained;
	metadata["epochsCompleted"] = summary.epochsCompleted;
	metadata["batchesCompleted"] = summary.batchesCompleted;
	metadata["device"] = summary.resolvedDevice;
	metadata["elapsedSeconds"] = summary.elapsedSeconds;
	metadata["options"] = ordered_json{
		{ "epochs", options.epochs },
		{ "batchSize", options.batchSize },
		{ "maxSamples", options.maxSamples.has_value() ? json(options.maxSamples.value()) : json(nullptr) },
		{ "learningRate", options.learningRate },
		{ "weightDecay", options.weightDecay },
		{ "policyLossWeight", options.policyLossWeight },
		{ "valueLossWeight", options.valueLossWeight },
		{ "maxGradNorm", options.maxGradNorm },
		{ "seed", options.seed },
	};
	metadata["averagePolicyLoss"] = summary.averagePolicyLoss;
	metadata["averageValueLoss"] = summary.averageValueLoss;
	metadata["averageTotalLoss"] = summary.averageTotalLoss;
	metadata["epochHistory"] = ordered_json::array();
	for (const TrainingEpochSummary& epoch : epochHistory) {
		metadata["epochHistory"].push_back(EpochSummaryJson(epoch));
	}
	return metadata;
}

Result WriteTrainingMetadata(
	const std::filesystem::path& checkpointPath,
	const TrainingOptions& options,
	const TrainingSummary& summary,
	const std::vector<TrainingEpochSummary>& epochHistory,
	const std::string& checkpointRole,
	int completedEpoch,
	TrainingError& error) {
	std::ofstream output(checkpointPath / "training.json", std::ios::trunc);
	if (!output.is_open()) {
		SetError(error, "training-metadata-write-failed", "could not write training.json: " + (checkpointPath / "training.json").string());
		return Result::Failed;
	}
	output << BuildTrainingMetadata(options, summary, epochHistory, checkpointRole, completedEpoch, checkpointPath).dump(2) << "\n";
	return Result::Succeeded;
}

Result SaveTrainingCheckpoint(
	const std::filesystem::path& checkpointPath,
	PolicyValueNetwork& model,
	const PolicyValueModelConfig& config,
	std::int64_t seed,
	const std::string& resolvedDevice,
	const TrainingOptions& options,
	const TrainingSummary& summary,
	const std::vector<TrainingEpochSummary>& epochHistory,
	const std::string& role,
	int completedEpoch,
	bool force,
	TrainingError& error) {
	PolicyValueModelError modelError;
	if (SavePolicyValueCheckpoint(checkpointPath, model, config, seed, resolvedDevice, force, modelError) == Result::Failed) {
		SetError(error, modelError.code, modelError.message);
		return Result::Failed;
	}
	return WriteTrainingMetadata(checkpointPath, options, summary, epochHistory, role, completedEpoch, error);
}

void PrintEpochProgress(const TrainingEpochSummary& epoch) {
	std::cout << "Epoch " << epoch.epoch <<
		" samples=" << epoch.samples <<
		" batches=" << epoch.batches <<
		" policyLoss=" << epoch.averagePolicyLoss <<
		" valueLoss=" << epoch.averageValueLoss <<
		" totalLoss=" << epoch.averageTotalLoss <<
		" elapsedSeconds=" << epoch.elapsedSeconds << std::endl;
}
}

Result RunTraining(const TrainingOptions& options, TrainingSummary& summary, TrainingError& error) noexcept {
	try {
		summary = TrainingSummary{};
		error = TrainingError{};
		const auto runStart = std::chrono::steady_clock::now();

		IfFailedReturn(ValidateTrainingOptions(options, error));

		PolicyValueNetwork model(nullptr);
		PolicyValueModelConfig config;
		std::int64_t checkpointSeed = -1;
		PolicyValueModelError modelError;
		if (LoadPolicyValueCheckpoint(options.checkpointIn, model, config, checkpointSeed, modelError) == Result::Failed) {
			SetError(error, modelError.code, modelError.message);
			return Result::Failed;
		}

		const torch::Device device = ResolvePolicyValueDevice(options.deviceName, modelError);
		if (!modelError.code.empty()) {
			SetError(error, modelError.code, modelError.message);
			return Result::Failed;
		}
		summary.resolvedDevice = PolicyValueDeviceName(device);

		ReplayDataset dataset;
		IfFailedReturn(LoadReplayDataset(options, dataset, error));
		summary.replayFiles = static_cast<int>(dataset.files.size());
		summary.samplesLoaded = static_cast<int>(dataset.samples.size());

		model->to(device);
		model->train();
		torch::optim::AdamW optimizer(model->parameters(), torch::optim::AdamWOptions(options.learningRate).weight_decay(options.weightDecay));

		if (!options.quiet) {
			std::cout << "Training start: samples=" << summary.samplesLoaded <<
				" epochs=" << options.epochs <<
				" batchSize=" << options.batchSize <<
				" device=" << summary.resolvedDevice <<
				" checkpointIn=" << options.checkpointIn.string() <<
				" checkpointOut=" << options.checkpointOut.string() << std::endl;
		}

		std::vector<int> baseOrder(dataset.samples.size());
		std::iota(baseOrder.begin(), baseOrder.end(), 0);
		std::vector<TrainingEpochSummary> epochHistory;
		epochHistory.reserve(static_cast<std::size_t>(options.epochs));

		double totalPolicyLossWeighted = 0.0;
		double totalValueLossWeighted = 0.0;
		double totalLossWeighted = 0.0;
		int totalSamplesTrained = 0;

		for (int epoch = 1; epoch <= options.epochs; ++epoch) {
			const auto epochStart = std::chrono::steady_clock::now();
			std::vector<int> order = baseOrder;
			std::mt19937_64 rng(static_cast<std::uint64_t>(options.seed) + static_cast<std::uint64_t>(epoch) * 0x9e3779b97f4a7c15ULL);
			std::shuffle(order.begin(), order.end(), rng);

			TrainingEpochSummary epochSummary;
			epochSummary.epoch = epoch;
			double epochPolicyLossWeighted = 0.0;
			double epochValueLossWeighted = 0.0;
			double epochTotalLossWeighted = 0.0;

			for (int start = 0; start < static_cast<int>(order.size()); start += options.batchSize) {
				const int batchNumber = epochSummary.batches + 1;
				const int batchCount = std::min(options.batchSize, static_cast<int>(order.size()) - start);
				BatchData batch;
				if (BuildBatch(dataset, order, start, batchCount, batch, error) == Result::Failed) {
					error.epoch = epoch;
					error.batch = batchNumber;
					return Result::Failed;
				}

				torch::Tensor input = MakeInputTensor(batch, batchCount, device);
				torch::Tensor outcomes = torch::from_blob(batch.outcomes.data(), { batchCount }, torch::TensorOptions().dtype(torch::kFloat32)).clone().to(device);

				model->train();
				PolicyValueNetworkOutput output = model->forward(input);
				torch::Tensor policyLoss = ComputePolicyLoss(output.policyLogits, batch, device);
				torch::Tensor valueLoss = torch::mse_loss(output.value.view({ batchCount }), outcomes);
				torch::Tensor totalLoss = policyLoss * options.policyLossWeight + valueLoss * options.valueLossWeight;

				optimizer.zero_grad();
				totalLoss.backward();
				if (options.maxGradNorm > 0.0) {
					torch::nn::utils::clip_grad_norm_(model->parameters(), options.maxGradNorm);
				}
				optimizer.step();

				const double policyLossValue = policyLoss.detach().cpu().item<double>();
				const double valueLossValue = valueLoss.detach().cpu().item<double>();
				const double totalLossValue = totalLoss.detach().cpu().item<double>();
				epochPolicyLossWeighted += policyLossValue * batchCount;
				epochValueLossWeighted += valueLossValue * batchCount;
				epochTotalLossWeighted += totalLossValue * batchCount;
				totalPolicyLossWeighted += policyLossValue * batchCount;
				totalValueLossWeighted += valueLossValue * batchCount;
				totalLossWeighted += totalLossValue * batchCount;
				totalSamplesTrained += batchCount;
				epochSummary.samples += batchCount;
				++epochSummary.batches;
				++summary.batchesCompleted;

				if (!options.quiet && options.logEveryBatches > 0 && batchNumber % options.logEveryBatches == 0) {
					std::cout << "Batch epoch=" << epoch << " batch=" << batchNumber << " totalLoss=" << totalLossValue << std::endl;
				}
			}

			const auto epochEnd = std::chrono::steady_clock::now();
			epochSummary.elapsedSeconds = ElapsedSeconds(epochStart, epochEnd);
			epochSummary.averagePolicyLoss = epochPolicyLossWeighted / epochSummary.samples;
			epochSummary.averageValueLoss = epochValueLossWeighted / epochSummary.samples;
			epochSummary.averageTotalLoss = epochTotalLossWeighted / epochSummary.samples;
			summary.samplesTrained += epochSummary.samples;
			summary.epochsCompleted = epoch;
			summary.averagePolicyLoss = totalPolicyLossWeighted / totalSamplesTrained;
			summary.averageValueLoss = totalValueLossWeighted / totalSamplesTrained;
			summary.averageTotalLoss = totalLossWeighted / totalSamplesTrained;
			summary.elapsedSeconds = ElapsedSeconds(runStart, epochEnd);

			if (options.checkpointEveryEpoch > 0 && epoch % options.checkpointEveryEpoch == 0) {
				const std::filesystem::path periodicPath = options.checkpointDir / EpochCheckpointName(epoch);
				epochSummary.checkpointPath = periodicPath;
				epochHistory.push_back(epochSummary);
				summary.lastPeriodicCheckpoint = periodicPath;
				IfFailedReturn(SaveTrainingCheckpoint(periodicPath, model, config, checkpointSeed, summary.resolvedDevice, options, summary, epochHistory, "periodic", epoch, options.force, error));
				model->to(device);
				model->train();
			}
			else {
				epochHistory.push_back(epochSummary);
			}

			if (!options.quiet) {
				PrintEpochProgress(epochSummary);
			}
		}

		summary.elapsedSeconds = ElapsedSeconds(runStart, std::chrono::steady_clock::now());
		IfFailedReturn(SaveTrainingCheckpoint(options.checkpointOut, model, config, checkpointSeed, summary.resolvedDevice, options, summary, epochHistory, "final", summary.epochsCompleted, options.force, error));
		return Result::Succeeded;
	}
	catch (const std::exception& err) {
		SetError(error, "training-exception", err.what());
		return Result::Failed;
	}
}
