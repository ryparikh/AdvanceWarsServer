#include "TrainingCommand.h"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>

#include "Training.h"

namespace {
bool ParseInt(const std::string& text, int& value) {
	try {
		std::size_t processed = 0;
		const long long parsed = std::stoll(text, &processed);
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

bool ParseInt64(const std::string& text, std::int64_t& value) {
	try {
		std::size_t processed = 0;
		const long long parsed = std::stoll(text, &processed);
		if (processed != text.size()) {
			return false;
		}
		value = static_cast<std::int64_t>(parsed);
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
		return processed == text.size() && std::isfinite(value);
	}
	catch (const std::exception&) {
		return false;
	}
}

int PrintTrainingError(const TrainingError& error, const TrainingSummary& summary) {
	std::cerr << error.code << ": " << error.message;
	if (!error.replayPath.empty()) {
		std::cerr << " replay=" << error.replayPath.string();
	}
	if (error.line > 0) {
		std::cerr << " line=" << error.line;
	}
	if (error.gameIndex >= 0) {
		std::cerr << " game=" << error.gameIndex;
	}
	if (error.ply >= 0) {
		std::cerr << " ply=" << error.ply;
	}
	if (error.epoch > 0) {
		std::cerr << " epoch=" << error.epoch;
	}
	if (error.batch > 0) {
		std::cerr << " batch=" << error.batch;
	}
	std::cerr << std::endl;
	std::cerr << "Training failed after samplesLoaded=" << summary.samplesLoaded <<
		" epochsCompleted=" << summary.epochsCompleted <<
		" batchesCompleted=" << summary.batchesCompleted;
	if (!summary.lastPeriodicCheckpoint.empty()) {
		std::cerr << " lastPeriodicCheckpoint=" << summary.lastPeriodicCheckpoint.string();
	}
	std::cerr << std::endl;
	return 1;
}
}

int RunTrainingCommand(int argc, char* argv[]) noexcept {
	try {
		TrainingOptions options;
		for (int i = 0; i < argc; ++i) {
			const std::string argument = argv[i];
			if (argument == "--replay" && i + 1 < argc) {
				options.replayPath = argv[++i];
			}
			else if (argument == "--checkpoint-in" && i + 1 < argc) {
				options.checkpointIn = argv[++i];
			}
			else if (argument == "--checkpoint-out" && i + 1 < argc) {
				options.checkpointOut = argv[++i];
			}
			else if (argument == "--checkpoint-dir" && i + 1 < argc) {
				options.checkpointDir = argv[++i];
			}
			else if (argument == "--epochs" && i + 1 < argc) {
				if (!ParseInt(argv[++i], options.epochs)) {
					std::cerr << "Invalid --epochs value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--batch-size" && i + 1 < argc) {
				if (!ParseInt(argv[++i], options.batchSize)) {
					std::cerr << "Invalid --batch-size value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--max-samples" && i + 1 < argc) {
				int maxSamples = 0;
				if (!ParseInt(argv[++i], maxSamples)) {
					std::cerr << "Invalid --max-samples value" << std::endl;
					return 1;
				}
				options.maxSamples = maxSamples;
			}
			else if (argument == "--learning-rate" && i + 1 < argc) {
				if (!ParseDouble(argv[++i], options.learningRate)) {
					std::cerr << "Invalid --learning-rate value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--weight-decay" && i + 1 < argc) {
				if (!ParseDouble(argv[++i], options.weightDecay)) {
					std::cerr << "Invalid --weight-decay value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--policy-loss-weight" && i + 1 < argc) {
				if (!ParseDouble(argv[++i], options.policyLossWeight)) {
					std::cerr << "Invalid --policy-loss-weight value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--value-loss-weight" && i + 1 < argc) {
				if (!ParseDouble(argv[++i], options.valueLossWeight)) {
					std::cerr << "Invalid --value-loss-weight value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--max-grad-norm" && i + 1 < argc) {
				if (!ParseDouble(argv[++i], options.maxGradNorm)) {
					std::cerr << "Invalid --max-grad-norm value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--seed" && i + 1 < argc) {
				if (!ParseInt64(argv[++i], options.seed)) {
					std::cerr << "Invalid --seed value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--device" && i + 1 < argc) {
				options.deviceName = argv[++i];
			}
			else if (argument == "--log-every-batches" && i + 1 < argc) {
				if (!ParseInt(argv[++i], options.logEveryBatches)) {
					std::cerr << "Invalid --log-every-batches value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--checkpoint-every-epochs" && i + 1 < argc) {
				if (!ParseInt(argv[++i], options.checkpointEveryEpoch)) {
					std::cerr << "Invalid --checkpoint-every-epochs value" << std::endl;
					return 1;
				}
			}
			else if (argument == "--force") {
				options.force = true;
			}
			else {
				std::cerr << "Unknown -train option: " << argument << std::endl;
				return 1;
			}
		}

		TrainingSummary summary;
		TrainingError error;
		if (RunTraining(options, summary, error) == Result::Failed) {
			return PrintTrainingError(error, summary);
		}

		std::cout << "Training complete: samples=" << summary.samplesTrained <<
			" epochs=" << summary.epochsCompleted <<
			" batches=" << summary.batchesCompleted <<
			" policyLoss=" << summary.averagePolicyLoss <<
			" valueLoss=" << summary.averageValueLoss <<
			" totalLoss=" << summary.averageTotalLoss <<
			" elapsedSeconds=" << summary.elapsedSeconds <<
			" checkpointOut=" << options.checkpointOut.string() << std::endl;
		return 0;
	}
	catch (const std::exception& err) {
		std::cerr << "training command exception: " << err.what() << std::endl;
		return 1;
	}
}
