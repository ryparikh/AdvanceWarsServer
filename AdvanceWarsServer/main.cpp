#include <iostream>
#include <fstream>
#include <cstdint>
#include <random>
#include <thread>
#include <future>
#include "AdvanceWarsServer.h"
#include "MonteCarloTreeSearch.h"
#include "SubsystemTest.h"
#include "Platform.h"
#include <torch/torch.h>

int main(int argc, char* argv[]) noexcept {
	try {
		if (argc < 2) { // Check if the user provided exactly one argument
			std::cerr << "Usage: " << argv[0] << " <option>\n";
			std::cerr << "Options:\n";
			std::cerr << "  -sim-random-move-game [seed] : Simulate a random move game.\n";
			std::cerr << "  -sim-mcts-game        : Simulate an MCTS game.\n";
			std::cerr << "  -server               : Act as game server.\n";
			return 1; // Return an error code
		}

		std::string argument = argv[1];
		if (argument == "-torchlib") {

			std::cout << "CUDA support: " << (torch::cuda::is_available() ? "true" : "false") << std::endl;
			std::cout << "CUDA devices: " << (torch::cuda::device_count()) << std::endl;

			torch::Tensor tensor = torch::rand({ 2, 3 });
			std::cout << tensor << std::endl;

			struct Net : torch::nn::Module {
				Net()
					: conv1(torch::nn::Conv2dOptions(1, 32, 5)),
					conv2(torch::nn::Conv2dOptions(32, 64, 5)),
					fc1(1024, 128),
					fc2(128, 10) {
					register_module("conv1", conv1);
					register_module("conv2", conv2);
					register_module("fc1", fc1);
					register_module("fc2", fc2);
				}

				torch::Tensor forward(torch::Tensor x) {
					x = torch::relu(conv1->forward(x));
					x = torch::max_pool2d(x, 2);
					x = torch::relu(conv2->forward(x));
					x = torch::max_pool2d(x, 2);
					x = x.view({ x.size(0), -1 });
					x = torch::relu(fc1->forward(x));
					x = fc2->forward(x);
					return torch::log_softmax(x, 1);
				}

				torch::nn::Conv2d conv1, conv2;
				torch::nn::Linear fc1, fc2;
			};
			torch::Device device(torch::cuda::is_available() ? torch::kCUDA : torch::kCPU);
			std::cout << "Using device: " << (device.is_cuda() ? "CUDA" : "CPU") << "\n";

			// Load MNIST dataset
			auto train_dataset = torch::data::datasets::MNIST("D:/MNIST/MNIST/raw").map(
				torch::data::transforms::Stack<>());
			auto train_loader = torch::data::make_data_loader(std::move(train_dataset),
				torch::data::DataLoaderOptions().batch_size(64));

			// Initialize model, optimizer, and loss function
			Net model;
			model.to(device);
			torch::optim::SGD optimizer(model.parameters(), torch::optim::SGDOptions(0.01).momentum(0.9));

			// Training loop
			for (size_t epoch = 1; epoch <= 5; ++epoch) {
				size_t batch_idx = 0;
				for (auto& batch : *train_loader) {
					model.train();
					auto data = batch.data.to(device), targets = batch.target.to(device);
					optimizer.zero_grad();
					auto output = model.forward(data);
					auto loss = torch::nll_loss(output, targets);
					loss.backward();
					optimizer.step();

					if (batch_idx++ % 100 == 0) {
						std::cout << "Train Epoch: " << epoch << " [" << batch_idx * 64
							<< "/60000] Loss: " << loss.item<float>() << "\n";
					}
				}
			}
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
			auto root = std::make_shared<MCTSNode<GameState, Action>>(rootState, Action());
			while (!rootState.isTerminal()) {
				int player = rootState.IsFirstPlayerTurn() ? 0 : 1;
				MCTS<GameState, Action> mcts;
				time_point startTimeTotalSim = std::chrono::steady_clock::now();
				auto bestNode = mcts.run(root, 50000);
				time_point endTimeTotalSim = std::chrono::steady_clock::now();
				std::cout << "\n\n\n\nTook to simulate: " << std::chrono::duration<double, std::milli>(endTimeTotalSim - startTimeTotalSim).count() << std::endl;
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
	catch (...) {
		std::cout << "exception was thrown" << std::endl;
	}
	return 0;
}
