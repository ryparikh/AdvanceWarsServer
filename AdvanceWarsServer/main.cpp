#include <iostream>
#include <fstream>
#include <random>
#include <thread>
#include <future>
#include "AdvanceWarsServer.h"
#include "MonteCarloTreeSearch.h"
#include "SubsystemTest.h"
#include "Platform.h"

int main(int argc, char* argv[]) noexcept {
	try {


		if (argc < 2) { // Check if the user provided exactly one argument
			std::cerr << "Usage: " << argv[0] << " <option>\n";
			std::cerr << "Options:\n";
			std::cerr << "  -sim-random-move-game : Simulate a random move game.\n";
			std::cerr << "  -sim-mcts-game        : Simulate an MCTS game.\n";
			std::cerr << "  -server               : Act as game server.\n";
			return 1; // Return an error code
		}

		std::string argument = argv[1];

		if (argument == "-sim-random-move-game") {
			// Determine the number of available CPU cores.
			unsigned int maxConcurrentGames = std::thread::hardware_concurrency();
			if (maxConcurrentGames == 0) {
				maxConcurrentGames = 2; // Fallback if hardware_concurrency() is not able to detect the core count.
			}

			std::vector<std::thread> threads;
			threads.reserve(maxConcurrentGames);

			std::atomic<int> simulationMoves = 0;
			auto runGameSimulation = [&]() {
				std::fstream filestream("./res/AWBW/MapSources/MCTS.json", std::ios::in);
				if (filestream.fail() || filestream.eof())
				{
					return;
				}

				json jsonState;
				filestream >> jsonState;
				GameState rootState;
				GameState::from_json(jsonState, rootState);

				std::ofstream outFile("D:/awai/random-move-game/" + Platform::createUuid() + ".awai");

				time_point startTime = std::chrono::steady_clock::now();
				std::cout << "GameState: " << std::endl << jsonState.dump() << std::endl;
				outFile << "GameState: " << std::endl << jsonState.dump() << std::endl;
				std::random_device rd;
				std::mt19937 luckGen(rd());
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
					//GameState::to_json(jsonState, rootState);
					//outFile << "GameState: " << std::endl << jsonState.dump() << std::endl;
					if (totalActions % 1000 == 0) {
						time_point endTime = std::chrono::steady_clock::now();
						std::cout << "Processed Actions: " << totalActions << ", Processed Time (ms): " << std::chrono::duration<double, std::milli>(endTime - startTime).count() << std::endl;
					}

					if (totalActions == 300000) {
						break;
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
			for (auto &fut : futures) {
				fut.wait();
			}			

			time_point endTimeTotalSim = std::chrono::steady_clock::now();
			std::clog << "\n\n\n\nTook to simulate: " << std::chrono::duration<double, std::milli>(endTimeTotalSim - startTimeTotalSim).count() << std::endl;
			std::cout << "TotalActions: " << simulationMoves << std::endl;
		}
		else if (argument == "-sim-mcts-game") {
			std::fstream filestream("./res/AWBW/MapSources/MCTS.json", std::ios::in);
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
			auto root = std::make_shared<MCTSNode<GameState, Action>>(rootState, Action());
			while (!rootState.isTerminal()) {
				int player = rootState.IsFirstPlayerTurn() ? 0 : 1;
				MCTS<GameState, Action> mcts;
				auto bestNode = mcts.run(root, 10000);
				json jaction;
				to_json(jaction, bestNode->action);
				outFile << "Player: " << player << ", Action picked: " << jaction.dump() << std::endl;
				std::cout << "Player: " << player << ", Action picked: " << jaction.dump() << std::endl;
				rootState = rootState.applyAction(bestNode->action);
				GameState::to_json(jstate, rootState);
				outFile << "Game State: " << jstate.dump() << std::endl;
				std::cout << "Game State: " << jstate.dump() << std::endl;
				bestNode->parent->detachNode(bestNode);
				bestNode->parent->freeMemory();
				bestNode->parent.reset();
				root = bestNode;
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
	}
	catch (...) {
		std::cout << "exception was thrown" << std::endl;
	}
	return 0;
}
