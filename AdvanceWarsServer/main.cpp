#include <iostream>
#include <fstream>
#include <cstdint>
#include <exception>
#include <random>
#include <thread>
#include <future>
#include "AdvanceWarsServer.h"
#include "EvaluationCommand.h"
#include "EvaluationTest.h"
#include "MonteCarloTreeSearch.h"
#include "MctsTest.h"
#include "PolicyValueModelCommands.h"
#include "PolicyValueModelTest.h"
#include "SelfPlayReplayTest.h"
#include "SelfPlayRunner.h"
#include "SubsystemTest.h"
#include "Platform.h"
#include "TrainingCommand.h"
#include "TrainingTest.h"

int RunRestApiContractTests();

int main(int argc, char* argv[]) noexcept {
	try {
		if (argc < 2) { // Check if the user provided exactly one argument
			std::cerr << "Usage: " << argv[0] << " <option>\n";
			std::cerr << "Options:\n";
			std::cerr << "  -sim-random-move-game [seed] : Simulate a random move game.\n";
			std::cerr << "  -sim-mcts-game        : Simulate an MCTS game.\n";
			std::cerr << "  -self-play [options]  : Generate validated self-play replay JSONL.\n";
			std::cerr << "  -validate-replay <path> : Validate a self-play replay JSONL file.\n";
			std::cerr << "  -export-replay-trace <replay.jsonl> <trace.json> [--game-index <n>] : Export a materialized visualizer trace.\n";
			std::cerr << "  -test-mcts            : Run focused MCTS tests.\n";
			std::cerr << "  -test-model           : Run focused policy/value model tests.\n";
			std::cerr << "  -test-evaluate        : Run focused checkpoint evaluation tests.\n";
			std::cerr << "  -test-replay          : Run focused self-play replay tests.\n";
			std::cerr << "  -test-train           : Run focused replay training tests.\n";
			std::cerr << "  -model-init --out <dir> : Initialize and validate a policy/value checkpoint bundle.\n";
			std::cerr << "  -train [options]      : Train a policy/value checkpoint from replay data.\n";
			std::cerr << "  -evaluate [options]   : Evaluate checkpoint agents and write an evaluation report.\n";
			std::cerr << "  -torchlib [--mnist-path <path>] : Run a LibTorch smoke check and optional MNIST experiment.\n";
			std::cerr << "  -server               : Act as game server.\n";
			return 1; // Return an error code
		}

		std::string argument = argv[1];
		if (argument == "-torchlib") {
			return RunTorchLibCommand(argc - 2, argv + 2);
		}
		else if (argument == "-sim-random-move-game") {
			std::uint32_t randomMoveSeed = std::random_device{}();
			if (argc >= 3) {
				randomMoveSeed = static_cast<std::uint32_t>(std::stoul(argv[2]));
			}
			std::cout << "Random move game seed: " << randomMoveSeed << std::endl;

			// Determine the number of available CPU cores.
			unsigned int maxConcurrentGames = std::thread::hardware_concurrency();
			if (maxConcurrentGames == 0) {
				maxConcurrentGames = 2; // Fallback if hardware_concurrency() is not able to detect the core count.
			}

			std::vector<std::thread> threads;
			threads.reserve(maxConcurrentGames);

			std::atomic<int> simulationMoves = 0;
			auto runGameSimulation = [&]() {
				std::fstream filestream("./res/AWBW/MapSources/lefty.json", std::ios::in);
				if (filestream.fail() || filestream.eof())
				{
					return;
				}

				json jsonState;
				filestream >> jsonState;
				GameState rootState;
				GameState::from_json(jsonState, rootState);
				rootState.SetCombatRngSeed(randomMoveSeed);

				std::ofstream outFile("D:/awai/random-move-game/" + Platform::createUuid() + ".awai");

				time_point startTime = std::chrono::steady_clock::now();
				std::cout << "GameState: " << std::endl << jsonState.dump() << std::endl;
				outFile << "GameState: " << std::endl << jsonState.dump() << std::endl;
				std::mt19937 luckGen(randomMoveSeed);
				int totalActions = 0;
				while (!rootState.isTerminal()) {
					std::vector<Action> vecActions;
					vecActions = rootState.getLegalActions();
					// Generate a random number
					std::uniform_int_distribution<int> actionDistribution(0, static_cast<int>(vecActions.size() - 1));
					int action = actionDistribution(luckGen);
					json jaction;
					to_json(jaction, vecActions[action]);
					//std::cout << "Action: " << jaction.dump() << std::endl;
					++totalActions;
					outFile << "Action: " << jaction.dump() << std::endl;
					rootState.DoAction(vecActions[action]);
					rootState.CheckPlayerResigns();
					//GameState::to_json(jsonState, rootState);
					//outFile << "GameState: " << std::endl << jsonState.dump() << std::endl;
					if (totalActions % 1000 == 0) {
						time_point endTime = std::chrono::steady_clock::now();
						std::cout << "Processed Actions: " << totalActions << ", Processed Time (ms): " << std::chrono::duration<double, std::milli>(endTime - startTime).count() << std::endl;
					}

					if (totalActions % 500000 == 0) {
						GameState::to_json(jsonState, rootState);
						std::cout << "GameState: " << std::endl << jsonState.dump() << std::endl;
					}
				}
				simulationMoves += totalActions;
				GameState::to_json(jsonState, rootState);
				std::cout << "GameState: " << std::endl << jsonState.dump() << std::endl;
				outFile.close();
				time_point endTime = std::chrono::steady_clock::now();
				std::clog << "Game took to simulate: " << std::chrono::duration<double, std::milli>(endTime - startTime).count() << "\n";
				std::cout << "TotalActions: " << totalActions << std::endl;
				};

			// This vector will hold the futures for the running games.
			std::vector<std::future<void>> futures;
			futures.reserve(maxConcurrentGames);

			time_point startTimeTotalSim = std::chrono::steady_clock::now();
			for (int i = 0; i < 1; ++i) {
				// Launch a game simulation asynchronously.
				futures.push_back(std::async(std::launch::async, runGameSimulation));

				// If we've reached the concurrency limit, check for finished games.
				while (futures.size() >= maxConcurrentGames) {
					// Iterate over the futures and remove the ones that are done.
					for (auto it = futures.begin(); it != futures.end(); ) {
						if (it->wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) {
							it = futures.erase(it);
						}
						else {
							++it;
						}
					}
					// Sleep a little to avoid busy-waiting.
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
				}
			}

			// Wait for any remaining games to finish.
			for (auto& fut : futures) {
				fut.wait();
			}

			time_point endTimeTotalSim = std::chrono::steady_clock::now();
			std::clog << "\n\n\n\nTook to simulate: " << std::chrono::duration<double, std::milli>(endTimeTotalSim - startTimeTotalSim).count() << std::endl;
			std::cout << "TotalActions: " << simulationMoves << std::endl;
		}
		else if (argument == "-sim-mcts-game") {
			std::fstream filestream("./res/AWBW/MapSources/Lefty.json", std::ios::in);
			if (filestream.fail() || filestream.eof())
			{
				return -1;
			}

			json j;
			filestream >> j;
			GameState rootState;
			GameState::from_json(j, rootState);

			std::ofstream outFile("D:/awai/mcts_" + Platform::createUuid() + ".awai");

			json jstate;
			GameState::to_json(jstate, rootState);
			outFile << "Game State: " << jstate.dump() << std::endl;
			while (!rootState.isTerminal()) {
				int player = rootState.IsFirstPlayerTurn() ? 0 : 1;
				MCTS<GameState, Action> mcts;
				MCTSOptions options;
				options.maxSimulations = 50000;
				options.maxNodes = 100000;
				options.maxRolloutActions = 100000;
				time_point startTimeTotalSim = std::chrono::steady_clock::now();
				MCTSSearchResult<Action> result = mcts.search(rootState, options);
				time_point endTimeTotalSim = std::chrono::steady_clock::now();
				if (!result.selectedAction.has_value()) {
					std::cerr << "MCTS did not select an action." << std::endl;
					return -1;
				}

				std::cout << "\n\n\n\nTook to simulate: " << std::chrono::duration<double, std::milli>(endTimeTotalSim - startTimeTotalSim).count() << std::endl;
				json jaction;
				to_json(jaction, result.selectedAction.value());
				outFile << "Player: " << player << ", Action picked: " << jaction.dump() << std::endl;
				std::cout << "Player: " << player << ", Action picked: " << jaction.dump() << std::endl;
				rootState = rootState.applyAction(result.selectedAction.value());
				GameState::to_json(jstate, rootState);
				outFile << "Game State: " << jstate.dump() << std::endl;
				std::cout << "Game State: " << jstate.dump() << std::endl;
			}

			outFile.close();

			return 0;
		}
		else if (argument == "-test") {
			std::string testFilePath = "test/json";
			if (argc == 3) {
				testFilePath = argv[2];
			}

			time_point startTime = std::chrono::steady_clock::now();

			JsonTestSuite suite(testFilePath);
			suite.run();

			time_point endTime = std::chrono::steady_clock::now();
			std::cout << "Tests took to simulate: " << std::chrono::duration<double>(endTime - startTime).count() << "s" << std::endl;
		}
		else if (argument == "-test-api-contract") {
			return RunRestApiContractTests();
		}
		else if (argument == "-test-mcts") {
			return RunMctsTests();
		}
		else if (argument == "-test-model") {
			return RunPolicyValueModelTests(argc - 2, argv + 2);
		}
		else if (argument == "-model-init") {
			return RunModelInitCommand(argc - 2, argv + 2);
		}
		else if (argument == "-test-evaluate") {
			return RunEvaluationTests();
		}
		else if (argument == "-test-replay") {
			return RunSelfPlayReplayTests();
		}
		else if (argument == "-test-train") {
			return RunTrainingTests();
		}
		else if (argument == "-self-play") {
			return RunSelfPlayCommand(argc - 2, argv + 2);
		}
		else if (argument == "-validate-replay") {
			return RunValidateReplayCommand(argc - 2, argv + 2);
		}
		else if (argument == "-export-replay-trace") {
			return RunExportReplayTraceCommand(argc - 2, argv + 2);
		}
		else if (argument == "-train") {
			return RunTrainingCommand(argc - 2, argv + 2);
		}
		else if (argument == "-evaluate") {
			return RunEvaluationCommand(argc - 2, argv + 2);
		}
		else if (argument == "-server") {
			return AdvanceWarsServer::getInstance().run();
		}
		else if (argument == "-converter") {

			Player player1(CommandingOfficier::Type::Andy, Player::ArmyType::OrangeStar);
			Player player2(CommandingOfficier::Type::Adder, Player::ArmyType::BlueMoon);
			std::array<Player, 2> arrPlayers{ std::move(player1), std::move(player2) };
			GameState gameState("game", std::move(arrPlayers));
			gameState.InitializeGame();
			std::ofstream outFile("./res/AWBW/MapSources/Lefty.json");
			json j;
			GameState::to_json(j, gameState);
			outFile << j.dump();
			outFile.close();
		}
	}
	catch (const std::exception& err) {
		std::cerr << "exception was thrown: " << err.what() << std::endl;
		return 1;
	}
	catch (...) {
		std::cerr << "exception was thrown" << std::endl;
		return 1;
	}
	return 0;
}
