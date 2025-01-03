#include <iostream>
#include <fstream>
#include <random>
#include <thread>
#include "AdvanceWarsServer.h"

int main() noexcept {
	try {
		AdvanceWarsServer& awaiServer = AdvanceWarsServer::getInstance();
		std::thread thread([&]() {
			awaiServer.run();
		});

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

		//testRunner1.join();


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
		thread.join();
	}
	catch (...) {
		std::cout << "exception was thrown" << std::endl;
	}
	return 0;
}
