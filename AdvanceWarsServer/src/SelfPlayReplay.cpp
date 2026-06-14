#include "SelfPlayReplay.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <vector>

#include "ActionSpace.h"
#include "StandardGameSetup.h"
#include "StateTensor.h"

namespace {
constexpr double kMetricTolerance = 0.001;

void SetError(SelfPlayError& error, const std::string& code, const std::string& message, int line = 0, int gameIndex = -1, int ply = -1) {
	error.code = code;
	error.message = message;
	error.line = line;
	error.gameIndex = gameIndex;
	error.ply = ply;
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

Result RequireObjectWithFields(const json& value, const std::vector<std::string>& allowedFields, const std::string& label, SelfPlayError& error, int line, int gameIndex = -1, int ply = -1) {
	if (!value.is_object()) {
		SetError(error, "schema-error", label + " must be an object", line, gameIndex, ply);
		return Result::Failed;
	}

	std::string invalidField;
	if (!ContainsOnlyFields(value, allowedFields, invalidField)) {
		SetError(error, "schema-error", label + " has unknown or invalid field: " + invalidField, line, gameIndex, ply);
		return Result::Failed;
	}

	for (const std::string& requiredField : allowedFields) {
		if (!value.contains(requiredField)) {
			SetError(error, "schema-error", label + " is missing required field: " + requiredField, line, gameIndex, ply);
			return Result::Failed;
		}
	}

	return Result::Succeeded;
}

Result RequireObjectWithRequiredAndOptionalFields(
	const json& value,
	const std::vector<std::string>& requiredFields,
	const std::vector<std::string>& optionalFields,
	const std::string& label,
	SelfPlayError& error,
	int line,
	int gameIndex = -1,
	int ply = -1) {
	if (!value.is_object()) {
		SetError(error, "schema-error", label + " must be an object", line, gameIndex, ply);
		return Result::Failed;
	}

	std::vector<std::string> allowedFields = requiredFields;
	allowedFields.insert(allowedFields.end(), optionalFields.begin(), optionalFields.end());
	std::string invalidField;
	if (!ContainsOnlyFields(value, allowedFields, invalidField)) {
		SetError(error, "schema-error", label + " has unknown or invalid field: " + invalidField, line, gameIndex, ply);
		return Result::Failed;
	}

	for (const std::string& requiredField : requiredFields) {
		if (!value.contains(requiredField)) {
			SetError(error, "schema-error", label + " is missing required field: " + requiredField, line, gameIndex, ply);
			return Result::Failed;
		}
	}

	return Result::Succeeded;
}

std::string ChecksumToHex(std::uint64_t checksum) {
	std::ostringstream stream;
	stream << std::hex << std::setw(16) << std::setfill('0') << checksum;
	return stream.str();
}

Result StateTensorChecksumHex(const GameState& gameState, std::string& checksum, SelfPlayError& error, int line, int gameIndex, int ply) {
	std::vector<float> values;
	if (StateTensor::Encode(gameState, values) == Result::Failed) {
		SetError(error, "state-tensor-failed", "state tensor encoding failed", line, gameIndex, ply);
		return Result::Failed;
	}

	std::uint64_t rawChecksum = 0;
	if (StateTensor::Checksum(values, rawChecksum) == Result::Failed) {
		SetError(error, "state-tensor-checksum-failed", "state tensor checksum failed", line, gameIndex, ply);
		return Result::Failed;
	}

	checksum = ChecksumToHex(rawChecksum);
	return Result::Succeeded;
}

Result EncodeLegalActionIndices(const GameState& gameState, std::vector<Action>& legalActions, std::vector<int>& legalActionIndices, SelfPlayError& error, int line, int gameIndex, int ply) {
	legalActions.clear();
	legalActionIndices.clear();
	if (gameState.GetValidActions(legalActions) == Result::Failed) {
		SetError(error, "legal-actions-failed", "legal action generation failed", line, gameIndex, ply);
		return Result::Failed;
	}

	legalActionIndices.reserve(legalActions.size());
	for (const Action& action : legalActions) {
		int encodedAction = -1;
		if (ActionSpace::EncodeAction(action, encodedAction) == Result::Failed) {
			SetError(error, "action-encoding-failed", "legal action failed to encode", line, gameIndex, ply);
			return Result::Failed;
		}
		legalActionIndices.push_back(encodedAction);
	}

	std::sort(legalActionIndices.begin(), legalActionIndices.end());
	legalActionIndices.erase(std::unique(legalActionIndices.begin(), legalActionIndices.end()), legalActionIndices.end());
	return Result::Succeeded;
}

bool JsonIntArrayEquals(const json& value, const std::vector<int>& expected) {
	if (!value.is_array() || value.size() != expected.size()) {
		return false;
	}

	for (std::size_t i = 0; i < expected.size(); ++i) {
		if (!value.at(i).is_number_integer() || value.at(i).get<int>() != expected[i]) {
			return false;
		}
	}

	return true;
}

bool IsSortedUniqueInts(const json& value) {
	if (!value.is_array()) {
		return false;
	}

	int previous = -1;
	bool hasPrevious = false;
	for (const json& item : value) {
		if (!item.is_number_integer()) {
			return false;
		}

		const int current = item.get<int>();
		if (hasPrevious && current <= previous) {
			return false;
		}
		previous = current;
		hasPrevious = true;
	}

	return true;
}

void NormalizeHeaderConfigDefaults(json& header) {
	if (!header.contains("config") || !header.at("config").is_object()) {
		return;
	}

	json& config = header["config"];
	if (!config.contains("mctsMode")) {
		config["mctsMode"] = "rollout";
	}
	if (!config.contains("policyValueCheckpoint")) {
		config["policyValueCheckpoint"] = nullptr;
	}
	if (!config.contains("device")) {
		config["device"] = "auto";
	}
}

Result ValidateHeader(const json& header, const json* expectedHeader, SelfPlayError& error, int line) {
	IfFailedReturn(RequireObjectWithFields(header, { "recordType", "replayFormatVersion", "createdAt", "versions", "config" }, "header", error, line));
	if (header.at("recordType") != "header") {
		SetError(error, "schema-error", "first record must be a header", line);
		return Result::Failed;
	}
	if (header.at("replayFormatVersion") != SelfPlayReplayFormatVersion()) {
		SetError(error, "version-mismatch", "replay format version is not supported", line);
		return Result::Failed;
	}
	if (header.at("versions") != SelfPlayVersionsJson()) {
		SetError(error, "version-mismatch", "state/action encoding versions do not match this binary", line);
		return Result::Failed;
	}
	if (!header.at("createdAt").is_string()) {
		SetError(error, "schema-error", "header createdAt must be a string", line);
		return Result::Failed;
	}
	if (!header.at("config").is_object()) {
		SetError(error, "schema-error", "header config must be an object", line);
		return Result::Failed;
	}

	if (expectedHeader != nullptr) {
		json actualComparable = header;
		json expectedComparable = *expectedHeader;
		actualComparable.erase("createdAt");
		expectedComparable.erase("createdAt");
		NormalizeHeaderConfigDefaults(actualComparable);
		NormalizeHeaderConfigDefaults(expectedComparable);
		if (actualComparable != expectedComparable) {
			SetError(error, "append-header-mismatch", "existing replay header is not compatible with requested append", line);
			return Result::Failed;
		}
	}

	return Result::Succeeded;
}

bool IsNonnegativeNumber(const json& value) {
	return (value.is_number_integer() && value.get<double>() >= 0.0) ||
		(value.is_number_float() && value.get<double>() >= 0.0);
}

Result ValidateMctsBlock(const json& mcts, int& simulationsRun, int& nodesCreated, double& searchTimeMs, int& legalActionGenerationCalls, double& legalActionGenerationTimeMs, SelfPlayError& error, int line, int gameIndex, int ply) {
	IfFailedReturn(RequireObjectWithRequiredAndOptionalFields(
		mcts,
		{ "mctsSeed", "simulationsRun", "nodesCreated", "searchTimeMs" },
		{ "legalActionGenerationCalls", "legalActionGenerationTimeMs" },
		"sample.mcts",
		error,
		line,
		gameIndex,
		ply));
	if (!mcts.at("mctsSeed").is_number_unsigned() && !mcts.at("mctsSeed").is_number_integer()) {
		SetError(error, "schema-error", "mctsSeed must be an integer", line, gameIndex, ply);
		return Result::Failed;
	}
	if (!mcts.at("simulationsRun").is_number_integer() || mcts.at("simulationsRun").get<int>() < 1) {
		SetError(error, "schema-error", "simulationsRun must be at least 1", line, gameIndex, ply);
		return Result::Failed;
	}
	if (!mcts.at("nodesCreated").is_number_integer() || mcts.at("nodesCreated").get<int>() < 1) {
		SetError(error, "schema-error", "nodesCreated must be at least 1", line, gameIndex, ply);
		return Result::Failed;
	}
	if (!IsNonnegativeNumber(mcts.at("searchTimeMs"))) {
		SetError(error, "schema-error", "searchTimeMs must be nonnegative", line, gameIndex, ply);
		return Result::Failed;
	}
	if (mcts.contains("legalActionGenerationCalls") &&
		(!mcts.at("legalActionGenerationCalls").is_number_integer() || mcts.at("legalActionGenerationCalls").get<int>() < 0)) {
		SetError(error, "schema-error", "legalActionGenerationCalls must be nonnegative", line, gameIndex, ply);
		return Result::Failed;
	}
	if (mcts.contains("legalActionGenerationTimeMs") && !IsNonnegativeNumber(mcts.at("legalActionGenerationTimeMs"))) {
		SetError(error, "schema-error", "legalActionGenerationTimeMs must be nonnegative", line, gameIndex, ply);
		return Result::Failed;
	}

	simulationsRun = mcts.at("simulationsRun").get<int>();
	nodesCreated = mcts.at("nodesCreated").get<int>();
	searchTimeMs = mcts.at("searchTimeMs").get<double>();
	legalActionGenerationCalls = mcts.contains("legalActionGenerationCalls") ? mcts.at("legalActionGenerationCalls").get<int>() : 0;
	legalActionGenerationTimeMs = mcts.contains("legalActionGenerationTimeMs") ? mcts.at("legalActionGenerationTimeMs").get<double>() : 0.0;
	return Result::Succeeded;
}

Result ValidateVisitCounts(const json& visitCounts, const std::vector<int>& legalActionIndices, int selectedActionIndex, int simulationsRun, SelfPlayError& error, int line, int gameIndex, int ply) {
	if (!visitCounts.is_array()) {
		SetError(error, "schema-error", "visitCounts must be an array", line, gameIndex, ply);
		return Result::Failed;
	}

	int previousActionIndex = -1;
	int visitSum = 0;
	bool selectedVisited = false;
	for (const json& visit : visitCounts) {
		IfFailedReturn(RequireObjectWithFields(visit, { "actionIndex", "visits" }, "visitCounts[]", error, line, gameIndex, ply));
		if (!visit.at("actionIndex").is_number_integer() || !visit.at("visits").is_number_integer()) {
			SetError(error, "schema-error", "visit count fields must be integers", line, gameIndex, ply);
			return Result::Failed;
		}

		const int actionIndex = visit.at("actionIndex").get<int>();
		const int visits = visit.at("visits").get<int>();
		if (actionIndex <= previousActionIndex) {
			SetError(error, "schema-error", "visitCounts must be sorted by actionIndex", line, gameIndex, ply);
			return Result::Failed;
		}
		if (visits <= 0) {
			SetError(error, "schema-error", "visitCounts stores positive visits only", line, gameIndex, ply);
			return Result::Failed;
		}
		if (!std::binary_search(legalActionIndices.begin(), legalActionIndices.end(), actionIndex)) {
			SetError(error, "visit-not-legal", "visited action is not legal", line, gameIndex, ply);
			return Result::Failed;
		}
		if (actionIndex == selectedActionIndex) {
			selectedVisited = true;
		}

		previousActionIndex = actionIndex;
		visitSum += visits;
	}

	if (!selectedVisited) {
		SetError(error, "selected-action-unvisited", "selected action must have positive visits", line, gameIndex, ply);
		return Result::Failed;
	}
	if (visitSum != simulationsRun) {
		SetError(error, "visit-sum-mismatch", "sum of visit counts must equal simulationsRun", line, gameIndex, ply);
		return Result::Failed;
	}

	return Result::Succeeded;
}

int ExpectedOutcome(const json& winner, int currentPlayer) {
	if (winner.is_null()) {
		return 0;
	}

	const int winningPlayer = winner.get<int>();
	if (winningPlayer < 0 || winningPlayer == 2) {
		return 0;
	}

	return winningPlayer == currentPlayer ? 1 : -1;
}

Result ValidateMetrics(const json& game, int actionCount, int sampleCount, int turnCount, double totalSearchTimeMs, int totalLegalActionGenerationCalls, double totalLegalActionGenerationTimeMs, double averageBranchingFactor, SelfPlayReplayValidationSummary& summary, SelfPlayError& error, int line, int gameIndex) {
	const json& metrics = game.at("metrics");
	IfFailedReturn(RequireObjectWithRequiredAndOptionalFields(
		metrics,
		{ "actionCount", "sampleCount", "turnCount", "resignations", "invalidActionCount", "averageBranchingFactor", "totalSearchTimeMs", "averageSearchTimeMs", "totalElapsedMs" },
		{ "totalLegalActionGenerationCalls", "totalLegalActionGenerationTimeMs", "averageLegalActionGenerationTimeMs" },
		"metrics",
		error,
		line,
		gameIndex));

	if (!metrics.at("actionCount").is_number_integer() || metrics.at("actionCount").get<int>() != actionCount ||
		!metrics.at("sampleCount").is_number_integer() || metrics.at("sampleCount").get<int>() != sampleCount ||
		!metrics.at("turnCount").is_number_integer() || metrics.at("turnCount").get<int>() != turnCount) {
		SetError(error, "metrics-mismatch", "action/sample/turn metrics do not match replay", line, gameIndex);
		return Result::Failed;
	}
	if (!metrics.at("invalidActionCount").is_number_integer() || metrics.at("invalidActionCount").get<int>() != 0) {
		SetError(error, "metrics-mismatch", "invalidActionCount must be 0", line, gameIndex);
		return Result::Failed;
	}
	if (!metrics.at("resignations").is_number_integer() || metrics.at("resignations").get<int>() < 0) {
		SetError(error, "schema-error", "resignations must be nonnegative", line, gameIndex);
		return Result::Failed;
	}
	if (!IsNonnegativeNumber(metrics.at("averageBranchingFactor")) ||
		!IsNonnegativeNumber(metrics.at("totalSearchTimeMs")) ||
		!IsNonnegativeNumber(metrics.at("averageSearchTimeMs")) ||
		!IsNonnegativeNumber(metrics.at("totalElapsedMs"))) {
		SetError(error, "schema-error", "timing and average metrics must be nonnegative", line, gameIndex);
		return Result::Failed;
	}
	if (std::fabs(metrics.at("averageBranchingFactor").get<double>() - averageBranchingFactor) > kMetricTolerance) {
		SetError(error, "metrics-mismatch", "averageBranchingFactor does not match samples", line, gameIndex);
		return Result::Failed;
	}
	if (std::fabs(metrics.at("totalSearchTimeMs").get<double>() - totalSearchTimeMs) > kMetricTolerance) {
		SetError(error, "metrics-mismatch", "totalSearchTimeMs does not match samples", line, gameIndex);
		return Result::Failed;
	}
	const bool hasLegalActionMetrics =
		metrics.contains("totalLegalActionGenerationCalls") ||
		metrics.contains("totalLegalActionGenerationTimeMs") ||
		metrics.contains("averageLegalActionGenerationTimeMs");
	if (hasLegalActionMetrics) {
		if (!metrics.contains("totalLegalActionGenerationCalls") ||
			!metrics.contains("totalLegalActionGenerationTimeMs") ||
			!metrics.contains("averageLegalActionGenerationTimeMs")) {
			SetError(error, "schema-error", "legal-action generation metrics must be present together", line, gameIndex);
			return Result::Failed;
		}
		if (!metrics.at("totalLegalActionGenerationCalls").is_number_integer() ||
			metrics.at("totalLegalActionGenerationCalls").get<int>() < 0 ||
			!IsNonnegativeNumber(metrics.at("totalLegalActionGenerationTimeMs")) ||
			!IsNonnegativeNumber(metrics.at("averageLegalActionGenerationTimeMs"))) {
			SetError(error, "schema-error", "legal-action generation metrics must be nonnegative", line, gameIndex);
			return Result::Failed;
		}
		const double expectedAverage = totalLegalActionGenerationCalls == 0 ? 0.0 : totalLegalActionGenerationTimeMs / totalLegalActionGenerationCalls;
		if (metrics.at("totalLegalActionGenerationCalls").get<int>() != totalLegalActionGenerationCalls ||
			std::fabs(metrics.at("totalLegalActionGenerationTimeMs").get<double>() - totalLegalActionGenerationTimeMs) > kMetricTolerance ||
			std::fabs(metrics.at("averageLegalActionGenerationTimeMs").get<double>() - expectedAverage) > kMetricTolerance) {
			SetError(error, "metrics-mismatch", "legal-action generation metrics do not match samples", line, gameIndex);
			return Result::Failed;
		}
	}

	summary.invalidActions += metrics.at("invalidActionCount").get<int>();
	return Result::Succeeded;
}

Result ValidateGameRecord(const json& game, SelfPlayReplayValidationSummary& summary, SelfPlayError& error, int line) {
	IfFailedReturn(RequireObjectWithFields(game, { "recordType", "replayFormatVersion", "versions", "gameIndex", "config", "players", "settings", "initialState", "actions", "samples", "finalState", "terminalReason", "winner", "metrics" }, "game", error, line));
	if (game.at("recordType") != "game") {
		SetError(error, "schema-error", "non-header records must be game records", line);
		return Result::Failed;
	}
	if (game.at("replayFormatVersion") != SelfPlayReplayFormatVersion() || game.at("versions") != SelfPlayVersionsJson()) {
		SetError(error, "version-mismatch", "game record versions do not match this binary", line);
		return Result::Failed;
	}
	if (!game.at("gameIndex").is_number_integer()) {
		SetError(error, "schema-error", "gameIndex must be an integer", line);
		return Result::Failed;
	}

	const int gameIndex = game.at("gameIndex").get<int>();
	IfFailedReturn(RequireObjectWithRequiredAndOptionalFields(
		game.at("config"),
		{ "mapId", "baseSeed", "combatSeed", "mctsSeed", "maxActions", "mctsOptions" },
		{ "mctsMode", "policyValueCheckpoint", "device" },
		"game.config",
		error,
		line,
		gameIndex));
	IfFailedReturn(RequireObjectWithFields(game.at("config").at("mctsOptions"), { "maxSimulations", "maxNodes", "maxRolloutActions", "explorationConstant", "temperature" }, "game.config.mctsOptions", error, line, gameIndex));
	if (game.at("config").contains("mctsMode")) {
		const json& mode = game.at("config").at("mctsMode");
		if (!mode.is_string() || (mode != "rollout" && mode != "neural-puct")) {
			SetError(error, "schema-error", "game.config.mctsMode must be rollout or neural-puct", line, gameIndex);
			return Result::Failed;
		}
	}
	if (game.at("config").contains("policyValueCheckpoint") &&
		!game.at("config").at("policyValueCheckpoint").is_null() &&
		!game.at("config").at("policyValueCheckpoint").is_string()) {
		SetError(error, "schema-error", "game.config.policyValueCheckpoint must be a string or null", line, gameIndex);
		return Result::Failed;
	}
	if (game.at("config").contains("device") &&
		!game.at("config").at("device").is_string()) {
		SetError(error, "schema-error", "game.config.device must be a string", line, gameIndex);
		return Result::Failed;
	}
	if (!game.at("players").is_array() || game.at("players").size() != 2) {
		SetError(error, "schema-error", "players must contain two entries", line, gameIndex);
		return Result::Failed;
	}
	for (const json& player : game.at("players")) {
		IfFailedReturn(RequireObjectWithFields(player, { "slot", "co", "armyType" }, "players[]", error, line, gameIndex));
	}

	if (!game.at("actions").is_array() || !game.at("samples").is_array()) {
		SetError(error, "schema-error", "actions and samples must be arrays", line, gameIndex);
		return Result::Failed;
	}
	if (!game.at("terminalReason").is_null() && !game.at("terminalReason").is_string()) {
		SetError(error, "schema-error", "terminalReason must be a string or null", line, gameIndex);
		return Result::Failed;
	}
	if (!game.at("winner").is_null() && !game.at("winner").is_number_integer()) {
		SetError(error, "schema-error", "winner must be an integer or null", line, gameIndex);
		return Result::Failed;
	}
	if (game.at("actions").size() != game.at("samples").size()) {
		SetError(error, "schema-error", "actions and samples must have the same length", line, gameIndex);
		return Result::Failed;
	}

	GameState replayState;
	json initialState = game.at("initialState");
	try {
		GameState::from_json(initialState, replayState);
	}
	catch (const std::exception& err) {
		SetError(error, "initial-state-invalid", err.what(), line, gameIndex);
		return Result::Failed;
	}

	int legalActionTotal = 0;
	double totalSearchTimeMs = 0.0;
	int totalLegalActionGenerationCalls = 0;
	double totalLegalActionGenerationTimeMs = 0.0;
	for (std::size_t i = 0; i < game.at("samples").size(); ++i) {
		const int ply = static_cast<int>(i);
		const json& actionEntry = game.at("actions").at(i);
		const json& sample = game.at("samples").at(i);

		IfFailedReturn(RequireObjectWithFields(actionEntry, { "ply", "player", "actionIndex", "action" }, "actions[]", error, line, gameIndex, ply));
		IfFailedReturn(RequireObjectWithFields(sample, { "ply", "currentPlayer", "stateTensorChecksum", "legalActionCount", "legalActionIndices", "visitCounts", "selectedActionIndex", "outcome", "mcts" }, "samples[]", error, line, gameIndex, ply));

		if (actionEntry.at("ply") != ply || sample.at("ply") != ply) {
			SetError(error, "ply-mismatch", "ply must match action/sample index", line, gameIndex, ply);
			return Result::Failed;
		}

		const int currentPlayer = replayState.getCurrentPlayer();
		if (actionEntry.at("player") != currentPlayer || sample.at("currentPlayer") != currentPlayer) {
			SetError(error, "player-mismatch", "recorded player does not match replayed state", line, gameIndex, ply);
			return Result::Failed;
		}

		std::string actualChecksum;
		IfFailedReturn(StateTensorChecksumHex(replayState, actualChecksum, error, line, gameIndex, ply));
		if (!sample.at("stateTensorChecksum").is_string() || sample.at("stateTensorChecksum").get<std::string>() != actualChecksum) {
			SetError(error, "state-checksum-mismatch", "state tensor checksum does not match replayed state", line, gameIndex, ply);
			return Result::Failed;
		}

		std::vector<Action> legalActions;
		std::vector<int> legalActionIndices;
		IfFailedReturn(EncodeLegalActionIndices(replayState, legalActions, legalActionIndices, error, line, gameIndex, ply));
		if (!sample.at("legalActionCount").is_number_integer() || sample.at("legalActionCount").get<int>() != static_cast<int>(legalActionIndices.size())) {
			SetError(error, "legal-action-count-mismatch", "legalActionCount does not match legalActionIndices", line, gameIndex, ply);
			return Result::Failed;
		}
		if (!IsSortedUniqueInts(sample.at("legalActionIndices")) || !JsonIntArrayEquals(sample.at("legalActionIndices"), legalActionIndices)) {
			SetError(error, "legal-actions-mismatch", "legalActionIndices do not match engine legal actions", line, gameIndex, ply);
			return Result::Failed;
		}
		legalActionTotal += static_cast<int>(legalActionIndices.size());

		Action action;
		json actionJson = actionEntry.at("action");
		try {
			from_json(actionJson, action);
		}
		catch (const std::exception& err) {
			SetError(error, "action-invalid", err.what(), line, gameIndex, ply);
			return Result::Failed;
		}

		int actionIndex = -1;
		if (ActionSpace::EncodeAction(action, actionIndex) == Result::Failed) {
			SetError(error, "action-encoding-failed", "recorded action failed to encode", line, gameIndex, ply);
			return Result::Failed;
		}
		if (actionEntry.at("actionIndex") != actionIndex || sample.at("selectedActionIndex") != actionIndex) {
			SetError(error, "selected-action-index-mismatch", "selected action index does not match action history", line, gameIndex, ply);
			return Result::Failed;
		}
		if (std::find(legalActions.begin(), legalActions.end(), action) == legalActions.end()) {
			SetError(error, "selected-action-illegal", "selected action is not legal in replayed state", line, gameIndex, ply);
			return Result::Failed;
		}

		int simulationsRun = 0;
		int nodesCreated = 0;
		double searchTimeMs = 0.0;
		int legalActionGenerationCalls = 0;
		double legalActionGenerationTimeMs = 0.0;
		IfFailedReturn(ValidateMctsBlock(sample.at("mcts"), simulationsRun, nodesCreated, searchTimeMs, legalActionGenerationCalls, legalActionGenerationTimeMs, error, line, gameIndex, ply));
		IfFailedReturn(ValidateVisitCounts(sample.at("visitCounts"), legalActionIndices, actionIndex, simulationsRun, error, line, gameIndex, ply));
		totalSearchTimeMs += searchTimeMs;
		totalLegalActionGenerationCalls += legalActionGenerationCalls;
		totalLegalActionGenerationTimeMs += legalActionGenerationTimeMs;

		if (!sample.at("outcome").is_number_integer() || sample.at("outcome").get<int>() != ExpectedOutcome(game.at("winner"), currentPlayer)) {
			SetError(error, "outcome-mismatch", "sample outcome does not match winner/currentPlayer", line, gameIndex, ply);
			return Result::Failed;
		}

		if (replayState.DoAction(action) == Result::Failed) {
			SetError(error, "action-apply-failed", "recorded action failed during replay", line, gameIndex, ply);
			return Result::Failed;
		}
		if (replayState.FHeuristicAutoResign()) {
			replayState.CheckPlayerResigns();
		}
	}

	json replayedFinalState = SerializeGameStateForReplay(replayState);
	if (replayedFinalState != game.at("finalState")) {
		SetError(error, "final-state-mismatch", "replayed final state does not match finalState", line, gameIndex);
		return Result::Failed;
	}

	const std::string terminalReason = game.at("terminalReason").is_null() ? "" : game.at("terminalReason").get<std::string>();
	if (terminalReason == "action-limit") {
		if (!game.at("winner").is_null()) {
			SetError(error, "winner-mismatch", "action-limit games must have null winner", line, gameIndex);
			return Result::Failed;
		}
	}
	else {
		if (!replayState.FGameOver()) {
			SetError(error, "terminal-mismatch", "non action-limit game must end in an engine terminal state", line, gameIndex);
			return Result::Failed;
		}
		if (!game.at("winner").is_number_integer() || game.at("winner").get<int>() != replayState.getWinningPlayer()) {
			SetError(error, "winner-mismatch", "winner does not match replayed final state", line, gameIndex);
			return Result::Failed;
		}
		if (replayState.GetTerminalReason().has_value() && game.at("terminalReason") != replayState.GetTerminalReason().value()) {
			SetError(error, "terminal-mismatch", "terminal reason does not match replayed final state", line, gameIndex);
			return Result::Failed;
		}
	}

	const int sampleCount = static_cast<int>(game.at("samples").size());
	const double averageBranchingFactor = sampleCount == 0 ? 0.0 : static_cast<double>(legalActionTotal) / sampleCount;
	IfFailedReturn(ValidateMetrics(game, static_cast<int>(game.at("actions").size()), sampleCount, replayState.GetTurnCount(), totalSearchTimeMs, totalLegalActionGenerationCalls, totalLegalActionGenerationTimeMs, averageBranchingFactor, summary, error, line, gameIndex));

	++summary.games;
	summary.samples += sampleCount;
	summary.actions += static_cast<int>(game.at("actions").size());
	return Result::Succeeded;
}

json TraceSourceFromReplay(const json& header, const json& game, int selectedGameIndex) {
	json source = {
		{ "kind", "self-play-replay" },
		{ "replayFormatVersion", game.at("replayFormatVersion") },
		{ "gameIndex", game.at("gameIndex") },
		{ "selectedGameIndex", selectedGameIndex },
		{ "config", game.at("config") },
		{ "players", game.at("players") },
		{ "settings", game.at("settings") },
	};

	if (header.contains("createdAt")) {
		source["createdAt"] = header.at("createdAt");
	}
	return source;
}

Result MaterializeReplayTrace(const json& header, const json& game, int selectedGameIndex, int line, json& trace, SelfPlayError& error) {
	SelfPlayReplayValidationSummary validationSummary;
	IfFailedReturn(ValidateGameRecord(game, validationSummary, error, line));

	GameState replayState;
	json initialState = game.at("initialState");
	try {
		GameState::from_json(initialState, replayState);
	}
	catch (const std::exception& err) {
		SetError(error, "initial-state-invalid", err.what(), line, game.at("gameIndex").get<int>());
		return Result::Failed;
	}

	json steps = json::array();
	for (std::size_t i = 0; i < game.at("actions").size(); ++i) {
		const int ply = static_cast<int>(i);
		const json& actionEntry = game.at("actions").at(i);
		const json& sample = game.at("samples").at(i);

		Action action;
		json actionJson = actionEntry.at("action");
		try {
			from_json(actionJson, action);
		}
		catch (const std::exception& err) {
			SetError(error, "action-invalid", err.what(), line, game.at("gameIndex").get<int>(), ply);
			return Result::Failed;
		}

		if (replayState.DoAction(action) == Result::Failed) {
			SetError(error, "action-apply-failed", "recorded action failed during trace materialization", line, game.at("gameIndex").get<int>(), ply);
			return Result::Failed;
		}
		if (replayState.FHeuristicAutoResign()) {
			replayState.CheckPlayerResigns();
		}

		json step = {
			{ "ply", actionEntry.at("ply") },
			{ "player", actionEntry.at("player") },
			{ "actionIndex", actionEntry.at("actionIndex") },
			{ "action", actionEntry.at("action") },
			{ "legalActionCount", sample.at("legalActionCount") },
			{ "selectedActionIndex", sample.at("selectedActionIndex") },
			{ "stateTensorChecksum", sample.at("stateTensorChecksum") },
			{ "resultingState", SerializeGameStateForReplay(replayState) },
		};
		if (sample.contains("visitCounts")) {
			step["visitCounts"] = sample.at("visitCounts");
		}
		if (sample.contains("mcts")) {
			step["mcts"] = sample.at("mcts");
		}
		if (sample.contains("outcome")) {
			step["outcome"] = sample.at("outcome");
		}
		steps.push_back(std::move(step));
	}

	trace = {
		{ "traceFormatVersion", SelfPlayReplayTraceFormatVersion() },
		{ "source", TraceSourceFromReplay(header, game, selectedGameIndex) },
		{ "initialState", game.at("initialState") },
		{ "steps", std::move(steps) },
		{ "finalState", game.at("finalState") },
		{ "terminalReason", game.at("terminalReason") },
		{ "winner", game.at("winner") },
		{ "metrics", game.at("metrics") },
	};
	return Result::Succeeded;
}

Result ValidateReplayStream(std::istream& input, const json* expectedHeader, SelfPlayReplayValidationSummary& summary, SelfPlayError& error) {
	summary = SelfPlayReplayValidationSummary{};
	std::string line;
	int lineNumber = 0;
	bool sawHeader = false;
	while (std::getline(input, line)) {
		++lineNumber;
		if (line.empty()) {
			SetError(error, "schema-error", "JSONL records must not be blank", lineNumber);
			return Result::Failed;
		}

		json record;
		try {
			record = json::parse(line);
		}
		catch (const std::exception& err) {
			SetError(error, "malformed-json", err.what(), lineNumber);
			return Result::Failed;
		}

		if (!sawHeader) {
			IfFailedReturn(ValidateHeader(record, expectedHeader, error, lineNumber));
			sawHeader = true;
			continue;
		}

		IfFailedReturn(ValidateGameRecord(record, summary, error, lineNumber));
	}

	if (!sawHeader) {
		SetError(error, "empty-replay", "replay file does not contain a header");
		return Result::Failed;
	}

	if (summary.games == 0) {
		SetError(error, "empty-replay", "replay file does not contain any games");
		return Result::Failed;
	}

	return Result::Succeeded;
}

Result ExportTraceFromReplayStream(std::istream& input, int selectedGameIndex, json& trace, SelfPlayError& error) {
	if (selectedGameIndex < 0) {
		SetError(error, "invalid-game-index", "game index must be at least 0");
		return Result::Failed;
	}

	std::string line;
	int lineNumber = 0;
	int gameOrdinal = 0;
	bool sawHeader = false;
	json header;
	while (std::getline(input, line)) {
		++lineNumber;
		if (line.empty()) {
			SetError(error, "schema-error", "JSONL records must not be blank", lineNumber);
			return Result::Failed;
		}

		json record;
		try {
			record = json::parse(line);
		}
		catch (const std::exception& err) {
			SetError(error, "malformed-json", err.what(), lineNumber);
			return Result::Failed;
		}

		if (!sawHeader) {
			IfFailedReturn(ValidateHeader(record, nullptr, error, lineNumber));
			header = record;
			sawHeader = true;
			continue;
		}

		if (gameOrdinal == selectedGameIndex) {
			return MaterializeReplayTrace(header, record, selectedGameIndex, lineNumber, trace, error);
		}
		++gameOrdinal;
	}

	if (!sawHeader) {
		SetError(error, "empty-replay", "replay file does not contain a header");
		return Result::Failed;
	}
	if (gameOrdinal == 0) {
		SetError(error, "empty-replay", "replay file does not contain any games");
		return Result::Failed;
	}

	SetError(error, "game-index-not-found", "replay file does not contain requested game index");
	return Result::Failed;
}

bool ParseNonnegativeInt(const std::string& text, int& value) {
	try {
		size_t processed = 0;
		value = std::stoi(text, &processed);
		return processed == text.size() && value >= 0;
	}
	catch (const std::exception&) {
		return false;
	}
}

int PrintReplayError(const SelfPlayError& error) {
	std::cerr << error.code << ": " << error.message;
	if (error.line > 0) {
		std::cerr << " (line " << error.line << ")";
	}
	if (error.gameIndex >= 0) {
		std::cerr << " (game " << error.gameIndex << ")";
	}
	if (error.ply >= 0) {
		std::cerr << " (ply " << error.ply << ")";
	}
	std::cerr << std::endl;
	return 1;
}
}

const char* SelfPlayReplayFormatVersion() noexcept {
	return "standard-gl-self-play-replay-v1";
}

const char* SelfPlayReplayTraceFormatVersion() noexcept {
	return "standard-gl-visualizer-trace-v1";
}

json SelfPlayVersionsJson() {
	return {
		{ "stateTensor", StateTensor::Version() },
		{ "actionSpace", ActionSpace::Version() },
	};
}

Result ValidateSelfPlayReplay(const std::filesystem::path& path, SelfPlayReplayValidationSummary& summary, SelfPlayError& error) {
	std::ifstream input(path);
	if (!input.is_open()) {
		SetError(error, "open-failed", "could not open replay file: " + path.string());
		return Result::Failed;
	}

	return ValidateReplayStream(input, nullptr, summary, error);
}

Result ValidateSelfPlayReplayForAppend(const std::filesystem::path& path, const json& expectedHeader, SelfPlayReplayValidationSummary& summary, SelfPlayError& error) {
	std::ifstream input(path);
	if (!input.is_open()) {
		SetError(error, "open-failed", "could not open replay file for append validation: " + path.string());
		return Result::Failed;
	}

	if (input.peek() == std::ifstream::traits_type::eof()) {
		summary = SelfPlayReplayValidationSummary{};
		return Result::Succeeded;
	}

	return ValidateReplayStream(input, &expectedHeader, summary, error);
}

Result ExportSelfPlayReplayTrace(const std::filesystem::path& replayPath, int gameIndex, json& trace, SelfPlayError& error) {
	std::ifstream input(replayPath);
	if (!input.is_open()) {
		SetError(error, "open-failed", "could not open replay file: " + replayPath.string());
		return Result::Failed;
	}

	return ExportTraceFromReplayStream(input, gameIndex, trace, error);
}

Result ExportSelfPlayReplayTraceFile(const std::filesystem::path& replayPath, const std::filesystem::path& outputPath, int gameIndex, SelfPlayError& error) {
	json trace;
	IfFailedReturn(ExportSelfPlayReplayTrace(replayPath, gameIndex, trace, error));

	const std::filesystem::path parent = outputPath.parent_path();
	if (!parent.empty()) {
		std::error_code ec;
		std::filesystem::create_directories(parent, ec);
		if (ec) {
			SetError(error, "mkdir-failed", "could not create output directory: " + parent.string());
			return Result::Failed;
		}
	}

	std::ofstream output(outputPath, std::ios::trunc);
	if (!output.is_open()) {
		SetError(error, "open-failed", "could not open trace output file: " + outputPath.string());
		return Result::Failed;
	}

	output << trace.dump(2) << "\n";
	return Result::Succeeded;
}

int RunExportReplayTraceCommand(int argc, char* argv[]) noexcept {
	if (argc < 2) {
		std::cerr << "usage: -export-replay-trace <replay.jsonl> <trace.json> [--game-index <n>]" << std::endl;
		return 1;
	}

	std::filesystem::path replayPath = argv[0];
	std::filesystem::path outputPath = argv[1];
	int gameIndex = 0;
	for (int i = 2; i < argc; ++i) {
		const std::string arg = argv[i];
		if (arg == "--game-index" && i + 1 < argc) {
			if (!ParseNonnegativeInt(argv[++i], gameIndex)) {
				std::cerr << "invalid --game-index value" << std::endl;
				return 1;
			}
		}
		else {
			std::cerr << "invalid export-replay-trace argument: " << arg << std::endl;
			return 1;
		}
	}

	SelfPlayError error;
	if (ExportSelfPlayReplayTraceFile(replayPath, outputPath, gameIndex, error) == Result::Failed) {
		return PrintReplayError(error);
	}

	json trace;
	if (ExportSelfPlayReplayTrace(replayPath, gameIndex, trace, error) == Result::Succeeded) {
		std::cout << "Replay trace exported: gameIndex=" << gameIndex <<
			" steps=" << trace.at("steps").size() <<
			" out=" << outputPath.string() << std::endl;
	}
	else {
		std::cout << "Replay trace exported: gameIndex=" << gameIndex <<
			" out=" << outputPath.string() << std::endl;
	}
	return 0;
}
