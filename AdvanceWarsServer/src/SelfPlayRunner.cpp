#include "SelfPlayRunner.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

#include "ActionSpace.h"
#include "StandardGameSetup.h"
#include "StateTensor.h"

namespace {
void SetError(SelfPlayError& error, const std::string& code, const std::string& message, int gameIndex = -1, int ply = -1) {
	error.code = code;
	error.message = message;
	error.gameIndex = gameIndex;
	error.ply = ply;
}

std::string ChecksumToHex(std::uint64_t checksum) {
	std::ostringstream stream;
	stream << std::hex << std::setw(16) << std::setfill('0') << checksum;
	return stream.str();
}

std::string CreatedAtUtc() {
	const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
	std::tm utc{};
	gmtime_s(&utc, &now);
	std::ostringstream stream;
	stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
	return stream.str();
}

std::uint32_t AddSeed(std::uint32_t seed, std::uint32_t delta) {
	return static_cast<std::uint32_t>(seed + delta);
}

std::uint32_t DeriveActionSeed(std::uint32_t gameMctsSeed, int ply) {
	std::uint32_t value = gameMctsSeed ^ (0x9e3779b9u + static_cast<std::uint32_t>(ply) * 0x85ebca6bu);
	value ^= value >> 16;
	value *= 0x7feb352du;
	value ^= value >> 15;
	value *= 0x846ca68bu;
	value ^= value >> 16;
	return value;
}

json MctsOptionsJson(const MCTSOptions& options) {
	return {
		{ "maxSimulations", options.maxSimulations },
		{ "maxNodes", options.maxNodes },
		{ "maxRolloutActions", options.maxRolloutActions },
		{ "explorationConstant", options.explorationConstant },
		{ "temperature", options.temperature },
	};
}

json SettingsJsonFromOptions(const SelfPlayRunnerOptions& options) {
	return StandardSettingsJson(
		options.unitCap,
		options.captureLimit,
		options.hasDayLimit ? std::optional<int>(options.dayLimit) : std::nullopt,
		options.heuristicAutoResign);
}

json BuildHeader(const SelfPlayRunnerOptions& options) {
	return {
		{ "recordType", "header" },
		{ "replayFormatVersion", SelfPlayReplayFormatVersion() },
		{ "createdAt", CreatedAtUtc() },
		{ "versions", SelfPlayVersionsJson() },
		{ "config", {
			{ "mapId", options.mapId },
			{ "player0Co", options.player0CoId },
			{ "player1Co", options.player1CoId },
			{ "baseSeed", options.baseSeed },
			{ "maxActions", options.maxActions },
			{ "settings", SettingsJsonFromOptions(options) },
			{ "mctsOptions", MctsOptionsJson(options.mctsOptions) },
		} },
	};
}

Result StateTensorChecksumHex(const GameState& gameState, std::string& checksum, SelfPlayError& error, int gameIndex, int ply) {
	std::vector<float> values;
	if (StateTensor::Encode(gameState, values) == Result::Failed) {
		SetError(error, "state-tensor-failed", "state tensor encoding failed; map may exceed standard-gl-v1 shape", gameIndex, ply);
		return Result::Failed;
	}

	std::uint64_t rawChecksum = 0;
	if (StateTensor::Checksum(values, rawChecksum) == Result::Failed) {
		SetError(error, "state-tensor-checksum-failed", "state tensor checksum failed", gameIndex, ply);
		return Result::Failed;
	}

	checksum = ChecksumToHex(rawChecksum);
	return Result::Succeeded;
}

Result EncodeLegalActionIndices(const GameState& gameState, std::vector<Action>& legalActions, std::vector<int>& legalActionIndices, SelfPlayError& error, int gameIndex, int ply) {
	legalActions.clear();
	legalActionIndices.clear();
	if (gameState.GetValidActions(legalActions) == Result::Failed) {
		SetError(error, "legal-actions-failed", "legal action generation failed", gameIndex, ply);
		return Result::Failed;
	}

	for (const Action& action : legalActions) {
		int actionIndex = -1;
		if (ActionSpace::EncodeAction(action, actionIndex) == Result::Failed) {
			SetError(error, "action-encoding-failed", "legal action failed to encode", gameIndex, ply);
			return Result::Failed;
		}
		legalActionIndices.push_back(actionIndex);
	}

	std::sort(legalActionIndices.begin(), legalActionIndices.end());
	legalActionIndices.erase(std::unique(legalActionIndices.begin(), legalActionIndices.end()), legalActionIndices.end());
	return Result::Succeeded;
}

json IntVectorJson(const std::vector<int>& values) {
	json array = json::array();
	for (int value : values) {
		array.push_back(value);
	}
	return array;
}

Result BuildVisitCounts(const MCTSSearchResult<Action>& searchResult, int selectedActionIndex, json& visitCounts, SelfPlayError& error, int gameIndex, int ply) {
	std::map<int, int> visitsByAction;
	for (const MCTSActionStats<Action>& stats : searchResult.rootActionStats) {
		if (stats.visits <= 0) {
			continue;
		}

		int actionIndex = -1;
		if (ActionSpace::EncodeAction(stats.action, actionIndex) == Result::Failed) {
			SetError(error, "action-encoding-failed", "visited MCTS action failed to encode", gameIndex, ply);
			return Result::Failed;
		}
		visitsByAction[actionIndex] += stats.visits;
	}

	int visitSum = 0;
	bool selectedVisited = false;
	visitCounts = json::array();
	for (const auto& visits : visitsByAction) {
		visitCounts.push_back({
			{ "actionIndex", visits.first },
			{ "visits", visits.second },
		});
		visitSum += visits.second;
		if (visits.first == selectedActionIndex) {
			selectedVisited = true;
		}
	}

	if (!selectedVisited) {
		SetError(error, "selected-action-unvisited", "selected MCTS action must have positive visits", gameIndex, ply);
		return Result::Failed;
	}
	if (visitSum != searchResult.simulationsRun) {
		SetError(error, "visit-sum-mismatch", "sum of root visits does not match simulationsRun", gameIndex, ply);
		return Result::Failed;
	}

	return Result::Succeeded;
}

int OutcomeForPlayer(const json& winner, int player) {
	if (winner.is_null()) {
		return 0;
	}

	const int winningPlayer = winner.get<int>();
	if (winningPlayer < 0 || winningPlayer == 2) {
		return 0;
	}

	return winningPlayer == player ? 1 : -1;
}

json PlayersMetadata(const StandardGameSetupResult& setup) {
	return json::array({
		{
			{ "slot", 0 },
			{ "co", setup.playerCoIds[0] },
			{ "armyType", setup.playerArmyTypes[0] },
		},
		{
			{ "slot", 1 },
			{ "co", setup.playerCoIds[1] },
			{ "armyType", setup.playerArmyTypes[1] },
		},
	});
}

json GameConfigJson(const SelfPlayRunnerOptions& options, int gameIndex, std::uint32_t combatSeed, std::uint32_t mctsSeed) {
	return {
		{ "mapId", options.mapId },
		{ "baseSeed", options.baseSeed },
		{ "combatSeed", combatSeed },
		{ "mctsSeed", mctsSeed },
		{ "maxActions", options.maxActions },
		{ "mctsOptions", MctsOptionsJson(options.mctsOptions) },
	};
}

Result CreateGame(const SelfPlayRunnerOptions& options, int gameIndex, std::uint32_t combatSeed, StandardGameSetupResult& setup, SelfPlayError& error) {
	json request;
	StandardGameSetupError setupError;
	if (BuildStandardGameRequestFromSlots(options.mapId, options.player0CoId, options.player1CoId, SettingsJsonFromOptions(options), combatSeed, request, setupError) == Result::Failed) {
		SetError(error, setupError.code, setupError.message, gameIndex);
		return Result::Failed;
	}
	if (CreateStandardGameFromRequest(request, setup, setupError) == Result::Failed) {
		SetError(error, setupError.code, setupError.message, gameIndex);
		return Result::Failed;
	}

	return Result::Succeeded;
}

Result BuildGameRecord(const SelfPlayRunnerOptions& options, int gameIndex, int outputGameIndex, json& gameRecord, SelfPlayRunSummary& summary, SelfPlayError& error) {
	const std::uint32_t combatSeed = AddSeed(options.baseSeed, static_cast<std::uint32_t>(gameIndex));
	const std::uint32_t mctsGameSeed = AddSeed(options.baseSeed, static_cast<std::uint32_t>(1000003u * (static_cast<std::uint32_t>(gameIndex) + 1u)));

	StandardGameSetupResult setup;
	IfFailedReturn(CreateGame(options, gameIndex, combatSeed, setup, error));

	GameState gameState = setup.gameState;
	const json initialState = SerializeGameStateForReplay(gameState);

	json actions = json::array();
	json samples = json::array();
	int invalidActionCount = 0;
	int legalActionTotal = 0;
	double totalSearchTimeMs = 0.0;

	const auto gameStart = std::chrono::steady_clock::now();
	for (int ply = 0; ply < options.maxActions && !gameState.FGameOver(); ++ply) {
		const int currentPlayer = gameState.getCurrentPlayer();

		std::string checksum;
		IfFailedReturn(StateTensorChecksumHex(gameState, checksum, error, gameIndex, ply));

		std::vector<Action> legalActions;
		std::vector<int> legalActionIndices;
		IfFailedReturn(EncodeLegalActionIndices(gameState, legalActions, legalActionIndices, error, gameIndex, ply));
		if (legalActions.empty()) {
			SetError(error, "no-legal-actions", "non-terminal state has no legal actions", gameIndex, ply);
			return Result::Failed;
		}
		legalActionTotal += static_cast<int>(legalActionIndices.size());

		MCTS<GameState, Action> mcts;
		MCTSOptions searchOptions = options.mctsOptions;
		const std::uint32_t actionMctsSeed = DeriveActionSeed(mctsGameSeed, ply);
		searchOptions.seed = actionMctsSeed;

		const auto searchStart = std::chrono::steady_clock::now();
		MCTSSearchResult<Action> searchResult = mcts.search(gameState, searchOptions);
		const auto searchEnd = std::chrono::steady_clock::now();
		const double searchTimeMs = std::chrono::duration<double, std::milli>(searchEnd - searchStart).count();
		totalSearchTimeMs += searchTimeMs;

		if (!searchResult.selectedAction.has_value()) {
			SetError(error, "mcts-no-action", "MCTS did not select an action", gameIndex, ply);
			return Result::Failed;
		}

		const Action selectedAction = searchResult.selectedAction.value();
		if (std::find(legalActions.begin(), legalActions.end(), selectedAction) == legalActions.end()) {
			++invalidActionCount;
			SetError(error, "selected-action-illegal", "MCTS selected action is not legal", gameIndex, ply);
			return Result::Failed;
		}

		int selectedActionIndex = -1;
		if (ActionSpace::EncodeAction(selectedAction, selectedActionIndex) == Result::Failed) {
			SetError(error, "action-encoding-failed", "selected action failed to encode", gameIndex, ply);
			return Result::Failed;
		}

		json visitCounts;
		IfFailedReturn(BuildVisitCounts(searchResult, selectedActionIndex, visitCounts, error, gameIndex, ply));

		json actionJson;
		to_json(actionJson, selectedAction);
		actions.push_back({
			{ "ply", ply },
			{ "player", currentPlayer },
			{ "actionIndex", selectedActionIndex },
			{ "action", std::move(actionJson) },
		});

		samples.push_back({
			{ "ply", ply },
			{ "currentPlayer", currentPlayer },
			{ "stateTensorChecksum", checksum },
			{ "legalActionCount", static_cast<int>(legalActionIndices.size()) },
			{ "legalActionIndices", IntVectorJson(legalActionIndices) },
			{ "visitCounts", std::move(visitCounts) },
			{ "selectedActionIndex", selectedActionIndex },
			{ "outcome", 0 },
			{ "mcts", {
				{ "mctsSeed", actionMctsSeed },
				{ "simulationsRun", searchResult.simulationsRun },
				{ "nodesCreated", searchResult.nodesCreated },
				{ "searchTimeMs", searchTimeMs },
			} },
		});

		if (gameState.DoAction(selectedAction) == Result::Failed) {
			++invalidActionCount;
			SetError(error, "action-apply-failed", "selected action failed during execution", gameIndex, ply);
			return Result::Failed;
		}
		if (gameState.FHeuristicAutoResign()) {
			gameState.CheckPlayerResigns();
		}
	}

	json terminalReason;
	json winner;
	if (gameState.FGameOver()) {
		terminalReason = gameState.GetTerminalReason().has_value() ? json(gameState.GetTerminalReason().value()) : json(nullptr);
		winner = gameState.getWinningPlayer();
	}
	else {
		terminalReason = "action-limit";
		winner = nullptr;
	}

	for (json& sample : samples) {
		sample["outcome"] = OutcomeForPlayer(winner, sample.at("currentPlayer").get<int>());
	}

	const auto gameEnd = std::chrono::steady_clock::now();
	const double totalElapsedMs = std::chrono::duration<double, std::milli>(gameEnd - gameStart).count();
	const int sampleCount = static_cast<int>(samples.size());
	const double averageBranchingFactor = sampleCount == 0 ? 0.0 : static_cast<double>(legalActionTotal) / sampleCount;
	const double averageSearchTimeMs = sampleCount == 0 ? 0.0 : totalSearchTimeMs / sampleCount;
	const int resignations = terminalReason.is_string() && terminalReason.get<std::string>() == "heuristic-resign" ? 1 : 0;

	gameRecord = {
		{ "recordType", "game" },
		{ "replayFormatVersion", SelfPlayReplayFormatVersion() },
		{ "versions", SelfPlayVersionsJson() },
		{ "gameIndex", outputGameIndex },
		{ "config", GameConfigJson(options, gameIndex, combatSeed, mctsGameSeed) },
		{ "players", PlayersMetadata(setup) },
		{ "settings", setup.resolvedSettings },
		{ "initialState", initialState },
		{ "actions", actions },
		{ "samples", samples },
		{ "finalState", SerializeGameStateForReplay(gameState) },
		{ "terminalReason", terminalReason },
		{ "winner", winner },
		{ "metrics", {
			{ "actionCount", static_cast<int>(actions.size()) },
			{ "sampleCount", sampleCount },
			{ "turnCount", gameState.GetTurnCount() },
			{ "resignations", resignations },
			{ "invalidActionCount", invalidActionCount },
			{ "averageBranchingFactor", averageBranchingFactor },
			{ "totalSearchTimeMs", totalSearchTimeMs },
			{ "averageSearchTimeMs", averageSearchTimeMs },
			{ "totalElapsedMs", totalElapsedMs },
		} },
	};

	++summary.gamesWritten;
	summary.samplesWritten += sampleCount;
	summary.actionsWritten += static_cast<int>(actions.size());
	return Result::Succeeded;
}

bool IsFileEmpty(const std::filesystem::path& path) {
	if (!std::filesystem::exists(path)) {
		return true;
	}
	return std::filesystem::file_size(path) == 0;
}

Result PrepareOutputFile(const SelfPlayRunnerOptions& options, const json& header, int& existingGames, SelfPlayError& error) {
	existingGames = 0;
	const std::filesystem::path parent = options.outputPath.parent_path();
	if (!parent.empty()) {
		std::error_code ec;
		std::filesystem::create_directories(parent, ec);
		if (ec) {
			SetError(error, "mkdir-failed", "could not create output directory: " + parent.string());
			return Result::Failed;
		}
	}

	if (std::filesystem::exists(options.outputPath)) {
		if (!options.append) {
			SetError(error, "output-exists", "output file already exists; pass --append to add games");
			return Result::Failed;
		}

		if (!IsFileEmpty(options.outputPath)) {
			SelfPlayReplayValidationSummary appendSummary;
			IfFailedReturn(ValidateSelfPlayReplayForAppend(options.outputPath, header, appendSummary, error));
			existingGames = appendSummary.games;
		}
	}

	return Result::Succeeded;
}

Result ValidateOptions(const SelfPlayRunnerOptions& options, SelfPlayError& error) {
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
	if (options.games < 1) {
		SetError(error, "invalid-games", "--games must be at least 1");
		return Result::Failed;
	}
	if (options.maxActions < 1) {
		SetError(error, "invalid-max-actions", "--max-actions must be at least 1");
		return Result::Failed;
	}
	if (options.mctsOptions.maxSimulations < 1) {
		SetError(error, "invalid-mcts-simulations", "--mcts-simulations must be at least 1");
		return Result::Failed;
	}
	if (options.mctsOptions.maxNodes < 1 || options.mctsOptions.maxRolloutActions < 0 || options.mctsOptions.temperature < 0.0) {
		SetError(error, "invalid-mcts-options", "MCTS limits must be nonnegative, and max nodes must be positive");
		return Result::Failed;
	}
	return Result::Succeeded;
}

bool ParseInt(const std::string& text, int& value) {
	try {
		std::size_t processed = 0;
		value = std::stoi(text, &processed);
		return processed == text.size();
	}
	catch (const std::exception&) {
		return false;
	}
}

bool ParseUInt32(const std::string& text, std::uint32_t& value) {
	try {
		std::size_t processed = 0;
		const unsigned long parsed = std::stoul(text, &processed);
		if (processed != text.size() || parsed > 0xffffffffUL) {
			return false;
		}
		value = static_cast<std::uint32_t>(parsed);
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

bool ParseDouble(const std::string& text, double& value) {
	try {
		std::size_t processed = 0;
		value = std::stod(text, &processed);
		return processed == text.size();
	}
	catch (const std::exception&) {
		return false;
	}
}

int PrintError(const SelfPlayError& error) {
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

Result RunSelfPlay(const SelfPlayRunnerOptions& options, SelfPlayRunSummary& summary, SelfPlayError& error) {
	summary = SelfPlayRunSummary{};
	IfFailedReturn(ValidateOptions(options, error));

	const json header = BuildHeader(options);
	int existingGames = 0;
	IfFailedReturn(PrepareOutputFile(options, header, existingGames, error));

	const bool writeHeader = !std::filesystem::exists(options.outputPath) || IsFileEmpty(options.outputPath);
	std::ofstream output(options.outputPath, std::ios::app);
	if (!output.is_open()) {
		SetError(error, "open-failed", "could not open output file: " + options.outputPath.string());
		return Result::Failed;
	}

	if (writeHeader) {
		output << header.dump() << "\n";
	}

	for (int gameIndex = 0; gameIndex < options.games; ++gameIndex) {
		json gameRecord;
		IfFailedReturn(BuildGameRecord(options, existingGames + gameIndex, existingGames + gameIndex, gameRecord, summary, error));
		output << gameRecord.dump() << "\n";
		if (!options.quiet) {
			std::cout << "self-play game " << (existingGames + gameIndex) <<
				" terminalReason=" << gameRecord.at("terminalReason").dump() <<
				" winner=" << gameRecord.at("winner").dump() <<
				" actions=" << gameRecord.at("metrics").at("actionCount") <<
				" samples=" << gameRecord.at("metrics").at("sampleCount") <<
				" elapsedMs=" << gameRecord.at("metrics").at("totalElapsedMs") <<
				" out=" << options.outputPath.string() << std::endl;
		}
	}
	output.close();

	SelfPlayReplayValidationSummary validationSummary;
	IfFailedReturn(ValidateSelfPlayReplay(options.outputPath, validationSummary, error));
	return Result::Succeeded;
}

int RunSelfPlayCommand(int argc, char* argv[]) noexcept {
	SelfPlayRunnerOptions options;
	options.mctsOptions.maxSimulations = 128;
	options.mctsOptions.temperature = 1.0;

	for (int i = 0; i < argc; ++i) {
		const std::string arg = argv[i];
		auto requireValue = [&](std::string& value) -> bool {
			if (i + 1 >= argc) {
				return false;
			}
			value = argv[++i];
			return true;
		};

		std::string value;
		if (arg == "--out" && requireValue(value)) {
			options.outputPath = value;
		}
		else if (arg == "--map" && requireValue(value)) {
			options.mapId = value;
		}
		else if (arg == "--player0-co" && requireValue(value)) {
			options.player0CoId = value;
		}
		else if (arg == "--player1-co" && requireValue(value)) {
			options.player1CoId = value;
		}
		else if (arg == "--games" && requireValue(value) && ParseInt(value, options.games)) {}
		else if (arg == "--max-actions" && requireValue(value) && ParseInt(value, options.maxActions)) {}
		else if (arg == "--seed" && requireValue(value) && ParseUInt32(value, options.baseSeed)) {}
		else if (arg == "--unit-cap" && requireValue(value) && ParseInt(value, options.unitCap)) {}
		else if (arg == "--capture-limit" && requireValue(value) && ParseInt(value, options.captureLimit)) {}
		else if (arg == "--day-limit" && requireValue(value) && ParseInt(value, options.dayLimit)) {
			options.hasDayLimit = true;
		}
		else if (arg == "--mcts-simulations" && requireValue(value) && ParseInt(value, options.mctsOptions.maxSimulations)) {}
		else if (arg == "--mcts-max-nodes" && requireValue(value) && ParseInt(value, options.mctsOptions.maxNodes)) {}
		else if (arg == "--mcts-max-rollout-actions" && requireValue(value) && ParseInt(value, options.mctsOptions.maxRolloutActions)) {}
		else if (arg == "--mcts-exploration" && requireValue(value) && ParseDouble(value, options.mctsOptions.explorationConstant)) {}
		else if (arg == "--temperature" && requireValue(value) && ParseDouble(value, options.mctsOptions.temperature)) {}
		else if (arg == "--heuristic-auto-resign") {
			options.heuristicAutoResign = true;
		}
		else if (arg == "--append") {
			options.append = true;
		}
		else if (arg == "--quiet") {
			options.quiet = true;
		}
		else {
			std::cerr << "invalid self-play argument: " << arg << std::endl;
			return 1;
		}
	}

	SelfPlayRunSummary summary;
	SelfPlayError error;
	if (RunSelfPlay(options, summary, error) == Result::Failed) {
		return PrintError(error);
	}

	if (!options.quiet) {
		std::cout << "self-play replay validated: games=" << summary.gamesWritten <<
			" samples=" << summary.samplesWritten <<
			" actions=" << summary.actionsWritten << std::endl;
	}
	return 0;
}

int RunValidateReplayCommand(int argc, char* argv[]) noexcept {
	if (argc != 1) {
		std::cerr << "usage: -validate-replay <path>" << std::endl;
		return 1;
	}

	SelfPlayReplayValidationSummary summary;
	SelfPlayError error;
	if (ValidateSelfPlayReplay(argv[0], summary, error) == Result::Failed) {
		return PrintError(error);
	}

	std::cout << "Replay valid: games=" << summary.games <<
		" samples=" << summary.samples <<
		" actions=" << summary.actions <<
		" invalidActions=" << summary.invalidActions << std::endl;
	return 0;
}
