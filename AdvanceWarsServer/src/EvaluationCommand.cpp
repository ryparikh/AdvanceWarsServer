#include "EvaluationCommand.h"

#include <cstdint>
#include <iostream>
#include <limits>
#include <string>

#include "Evaluation.h"

namespace {
bool ParseInt(const std::string& text, int& value) {
	try {
		std::size_t processed = 0;
		const long long parsed = std::stoll(text, &processed, 10);
		if (processed != text.size() || parsed < std::numeric_limits<int>::min() || parsed > std::numeric_limits<int>::max()) {
			return false;
		}
		value = static_cast<int>(parsed);
		return true;
	}
	catch (const std::exception&) {
		return false;
	}
}

bool ParseUInt32(const std::string& text, std::uint32_t& value) {
	try {
		std::size_t processed = 0;
		const unsigned long parsed = std::stoul(text, &processed, 10);
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

int PrintEvaluationError(const EvaluationError& error) {
	std::cerr << error.code << ": " << error.message;
	if (!error.path.empty()) {
		std::cerr << " path=" << error.path.string();
	}
	if (error.gameIndex >= 0) {
		std::cerr << " gameIndex=" << error.gameIndex;
	}
	if (error.ply >= 0) {
		std::cerr << " ply=" << error.ply;
	}
	std::cerr << std::endl;
	return 1;
}
}

int RunEvaluationCommand(int argc, char* argv[]) noexcept {
	try {
		EvaluationOptions options;
		options.agents[0].type = "";
		options.agents[1].type = "";

		for (int i = 0; i < argc; ++i) {
			const std::string argument = argv[i];
			auto requireValue = [&](const std::string& option, std::string& value) -> bool {
				if (i + 1 >= argc) {
					std::cerr << "Missing value for " << option << std::endl;
					return false;
				}
				value = argv[++i];
				return true;
			};

			std::string value;
			if (argument == "--agent0" && requireValue(argument, value)) {
				options.agents[0].type = value;
			}
			else if (argument == "--agent1" && requireValue(argument, value)) {
				options.agents[1].type = value;
			}
			else if (argument == "--checkpoint0" && requireValue(argument, value)) {
				options.agents[0].checkpointPath = value;
			}
			else if (argument == "--checkpoint1" && requireValue(argument, value)) {
				options.agents[1].checkpointPath = value;
			}
			else if (argument == "--out" && requireValue(argument, value)) {
				options.outputPath = value;
			}
			else if (argument == "--map" && requireValue(argument, value)) {
				options.mapId = value;
			}
			else if (argument == "--player0-co" && requireValue(argument, value)) {
				options.player0CoId = value;
			}
			else if (argument == "--player1-co" && requireValue(argument, value)) {
				options.player1CoId = value;
			}
			else if (argument == "--rounds" && requireValue(argument, value)) {
				if (!ParseInt(value, options.rounds)) {
					std::cerr << "Invalid --rounds value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--seed" && requireValue(argument, value)) {
				if (!ParseUInt32(value, options.baseSeed)) {
					std::cerr << "Invalid --seed value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--max-actions" && requireValue(argument, value)) {
				if (!ParseInt(value, options.maxActions)) {
					std::cerr << "Invalid --max-actions value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--unit-cap" && requireValue(argument, value)) {
				if (!ParseInt(value, options.unitCap)) {
					std::cerr << "Invalid --unit-cap value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--capture-limit" && requireValue(argument, value)) {
				if (!ParseInt(value, options.captureLimit)) {
					std::cerr << "Invalid --capture-limit value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--day-limit" && requireValue(argument, value)) {
				if (!ParseInt(value, options.dayLimit)) {
					std::cerr << "Invalid --day-limit value" << std::endl;
					return 1;
				}
				options.hasDayLimit = true;
			}
			else if (argument == "--device" && requireValue(argument, value)) {
				options.deviceName = value;
			}
			else if (argument == "--promotion-score-threshold" && requireValue(argument, value)) {
				if (!ParseDouble(value, options.promotionScoreThreshold)) {
					std::cerr << "Invalid --promotion-score-threshold value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--min-promotion-rounds" && requireValue(argument, value)) {
				if (!ParseInt(value, options.minPromotionRounds)) {
					std::cerr << "Invalid --min-promotion-rounds value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--swap-sides") {
				options.swapSides = true;
			}
			else if (argument == "--heuristic-auto-resign") {
				options.heuristicAutoResign = true;
			}
			else if (argument == "--force") {
				options.force = true;
			}
			else if (argument == "--quiet") {
				options.quiet = true;
			}
			else {
				std::cerr << "Unknown -evaluate option: " << argument << std::endl;
				return 1;
			}
		}

		EvaluationSummary summary;
		EvaluationError error;
		if (RunEvaluation(options, summary, error) == Result::Failed) {
			return PrintEvaluationError(error);
		}

		if (!options.quiet) {
			std::cout << "Evaluation complete: games=" << summary.games <<
				" agent0ScoreRate=" << summary.agent0OverallScoreRate <<
				" agent1ScoreRate=" << summary.agent1OverallScoreRate <<
				" noResult=" << summary.noResults <<
				" recommendation=" << summary.promotionDecision <<
				" out=" << summary.reportPath.string() << std::endl;
		}
		return 0;
	}
	catch (const std::exception& err) {
		std::cerr << "evaluation command exception: " << err.what() << std::endl;
		return 1;
	}
}
