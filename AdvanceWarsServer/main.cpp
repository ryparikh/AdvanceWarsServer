#include <iostream>
#include <fstream>
#include <random>
#include <thread>
#include "AdvanceWarsServer.h"
#include "MonteCarloTreeSearch.h"

int main(int argc, char* argv[]) noexcept {
	try {
		AdvanceWarsServer& awaiServer = AdvanceWarsServer::getInstance();
		std::thread thread([&]() {
			awaiServer.run();
		});

		if (argc != 2) { // Check if the user provided exactly one argument
			std::cerr << "Usage: " << argv[0] << " <option>\n";
			std::cerr << "Options:\n";
			std::cerr << "  -sim-random-move-game : Simulate a random move game.\n";
			std::cerr << "  -sim-mcts-game        : Simulate an MCTS game.\n";
			std::cerr << "  -server               : Act as game server.\n";
			return 1; // Return an error code
		}

		std::string argument = argv[1];

		if (argument == "-sim-random-move-game") {

			// Simulate Game
			std::thread gameRunner([&]() {
				std::string gameId;
				json newGameState = awaiServer.create_new_game(gameId);
				std::ofstream outFile("./games/" + gameId + ".awai");

				time_point startTime = std::chrono::steady_clock::now();
				std::cout << "GameState: " << std::endl << newGameState.dump() << std::endl;
				outFile << "GameState: " << std::endl << newGameState.dump() << std::endl;
				std::random_device rd;
				std::mt19937 luckGen(rd());
				int totalActions = 0;
				while (!awaiServer.game_over(gameId)) {
					std::vector<Action> vecActions;
					awaiServer.get_valid_actions(gameId, vecActions);
					// Generate a random number
					std::uniform_int_distribution<int> actionDistribution(0, vecActions.size() - 1);
					int action = actionDistribution(luckGen);
					json jaction;
					to_json(jaction, vecActions[action]);
					//std::cout << "Action: " << jaction.dump() << std::endl;
					++totalActions;
					outFile << "Action: " << jaction.dump() << std::endl;
					newGameState = awaiServer.do_action(gameId, vecActions[action]);
					outFile << "GameState: " << std::endl << newGameState.dump() << std::endl;
					if (totalActions % 1000 == 0) {
						time_point endTime = std::chrono::steady_clock::now();
						std::cout << "Processed Actions: " << totalActions << ", Processed Time (ms): " << std::chrono::duration<double, std::milli>(endTime - startTime).count() << std::endl;
					}
				}
				std::cout << "GameState: " << std::endl << newGameState.dump() << std::endl;
				outFile.close();
				time_point endTime = std::chrono::steady_clock::now();
				std::clog << "Game took to simulate: " << std::chrono::duration<double, std::milli>(endTime - startTime).count() << "\n";
				std::cout << "TotalActions: " << totalActions << std::endl;
			});

			gameRunner.join();

		}
		else if (argument == "-sim-mcts-game") {
			std::thread gameRunner([&]() {
				std::string gameId;
				json j = awaiServer.create_new_game(gameId);
				GameState rootState = awaiServer.CloneGameState(gameId);
				std::ofstream outFile("D:/awai/mcts_" + gameId + ".awai");

				json jstate;
				to_json(jstate, rootState);
				outFile << "Game State: " << jstate.dump() << std::endl;
				int moves = 1;
				auto root = std::make_shared<MCTSNode<GameState, Action>>(rootState, Action());
				//while (!rootState.isTerminal()) {
				while (moves > 0) {
					int player = rootState.IsFirstPlayerTurn() ? 0 : 1;
					MCTS<GameState, Action> mcts;
					auto bestNode = mcts.run(root, 1, player); // Run MCTS with 100000iterations
					json jaction;
					to_json(jaction, bestNode->action);
					std::cout << "Player: " << player << ", Action picked: " << jaction.dump() << std::endl;
					std::cout << "Visits: " << bestNode->visits << ", Total value: " << bestNode->totalValue << std::endl;
					outFile << "Player: " << player << ", Action picked: " << jaction.dump() << std::endl;
					rootState = rootState.applyAction(bestNode->action);
					to_json(jstate, rootState);
					std::cout << "Game State: " << jstate.dump() << std::endl;
					outFile << "Game State: " << jstate.dump() << std::endl;
					--moves;
					bestNode->parent.reset();
					root = bestNode;
				}

				outFile.close();
			});

			gameRunner.join();
			thread.detach();
			return -1;
		}
		else if (argument == "-test") {

#include "Test.h"

			Player player1(CommandingOfficier::Type::Andy, Player::ArmyType::OrangeStar);
			Player player2(CommandingOfficier::Type::Adder, Player::ArmyType::BlueMoon);
			std::array<Player, 2> arrPlayers{ std::move(player1), std::move(player2) };
			GameState state("", std::move(arrPlayers));

			json j;
			std::stringstream(test) >> j;
			from_json(j, state);
			// Run test
			//std::thread testRunner1([&]() {
			//	awaiServer.create_new_game("test-game1");
			//	// P1 T1
			//	awaiServer.do_action("test-game1", Action(Action::Type::Buy, 10, 2, UnitProperties::Type::Infantry));
			//	awaiServer.do_action("test-game1", Action(Action::Type::EndTurn));
			//	// P2 T1
			//	awaiServer.do_action("test-game1", Action(Action::Type::MoveWait, 7, 13, 7, 11));
			//	awaiServer.do_action("test-game1", Action(Action::Type::EndTurn));
			//	// P1 T2
			//	awaiServer.do_action("test-game1", Action(Action::Type::MoveWait, 10, 2, 10, 4));
			//	awaiServer.do_action("test-game1", Action(Action::Type::EndTurn));
			//	// P2 T2
			//	awaiServer.do_action("test-game1", Action(Action::Type::MoveWait, 7, 11, 7, 8));
			//	awaiServer.do_action("test-game1", Action(Action::Type::EndTurn));
			//	// P1 T3
			//	awaiServer.do_action("test-game1", Action(Action::Type::MoveWait, 10, 4, 10, 7));
			//	awaiServer.do_action("test-game1", Action(Action::Type::EndTurn));
			//	// P2 T3
			//	awaiServer.do_action("test-game1", Action(Action::Type::MoveAttack, 7, 8, Action::Direction::North, 10, 8));
			//	awaiServer.do_action("test-game1", Action(Action::Type::EndTurn));
			//});
		}

		//testRunner1.join();
		thread.join();
	}
	catch (...) {
		std::cout << "exception was thrown" << std::endl;
	}
	return 0;
}
