#pragma once

#include <string>
#include <vector>
#include <memory>

// Node class for MCTS
template <typename StateType, typename ActionType>
class MCTSNode {
public:
	std::shared_ptr<MCTSNode> parent;
	std::vector<std::shared_ptr<MCTSNode>> children;
	ActionType action;
	StateType state;
	int visits;
	double totalValue;

	MCTSNode(const StateType& state, const ActionType& action, std::shared_ptr<MCTSNode> parent = nullptr)
		: state(state), action(action), parent(parent), visits(0), totalValue(0.0) {
	}

	bool isFullyExpanded() const {
		return !children.empty() && children.size() == state.getLegalActions().size();
	}

	bool isLeaf() const {
		return children.empty();
	}
};

// MCTS algorithm class
template <typename StateType, typename ActionType>
class MCTS {
public:
	std::shared_ptr<MCTSNode<StateType, ActionType>> run(const StateType& rootState, int iterations, int playerPerspective) {
		auto root = std::make_shared<MCTSNode<StateType, ActionType>>(rootState, ActionType());

		std::cout << "Start Run" << std::endl;
		for (int i = 1; i <= iterations; ++i) {
			auto node = select(root);
			if (!node->state.isTerminal()) {
				expand(node);
			}
			double reward = simulate(node, playerPerspective);
			backpropagate(node, reward, playerPerspective);

			if (i % 10 == 0) {
				std::cout << "Iterations: " << i << std::endl;
			}
		}

		return bestChild(root, 0.0);
	}

	std::shared_ptr<MCTSNode<StateType, ActionType>> run(std::shared_ptr<MCTSNode<StateType, ActionType>> root, int iterations, int playerPerspective) {
		std::cout << "Start Run" << std::endl;
		for (int i = 1; i <= iterations; ++i) {
			auto node = select(root);
			if (!node->state.isTerminal()) {
				expand(node);
			}
			double reward = simulate(node, playerPerspective);
			backpropagate(node, reward, playerPerspective);

			if (i % 10 == 0) {
				std::cout << "Iterations: " << i << std::endl;
			}
		}

		return bestChild(root, 0.0);
	}

private:
	std::shared_ptr<MCTSNode<StateType, ActionType>> select(std::shared_ptr<MCTSNode<StateType, ActionType>> node) {
		while (!node->isLeaf() && !node->state.isTerminal()) {
			node = bestChild(node, sqrt(2.0));
		}
		return node;
	}

	void expand(std::shared_ptr<MCTSNode<StateType, ActionType>> node) {
		for (const auto& action : node->state.getLegalActions()) {
			auto newState = node->state.applyAction(action);
			node->children.push_back(std::make_shared<MCTSNode<StateType, ActionType>>(newState, action, node));
		}
	}

	double simulate(std::shared_ptr<MCTSNode<StateType, ActionType>> node, int playerPerspective) {
		StateType state{ node->state };
		int totalSimStates = 0;
		//std::wcout << "Start Rollout" << std::endl;
		time_point startTime = std::chrono::steady_clock::now();
		while (!state.isTerminal()) {
			auto actions = state.getLegalActions();
			if (actions.empty()) break;
			ActionType action = actions[rand() % actions.size()];
			++totalSimStates;
			state = state.applyAction(action);
		}
		time_point endTime = std::chrono::steady_clock::now();
		std::cout << "Processed Actions: " << totalSimStates << ", Processed Time (ms): " << std::chrono::duration<double, std::milli>(endTime - startTime).count() << std::endl;
		double eval = state.evaluate(playerPerspective);
		//std::cout << "Player: " << playerPerspective << ", Evaluation: " << eval << std::endl;
		return eval;
	}

	void backpropagate(std::shared_ptr<MCTSNode<StateType, ActionType>> node, double reward, int rootPlayerPerspective) {
		while (node != nullptr) {
			node->visits++;
			int playerTurn = node->state.IsFirstPlayerTurn() ? 0 : 1;
			if (rootPlayerPerspective == playerTurn) {
				node->totalValue += reward;
			}
			else {
				node->totalValue += (reward * -1);
			}
			node = node->parent;
		}
	}

	std::shared_ptr<MCTSNode<StateType, ActionType>> bestChild(std::shared_ptr<MCTSNode<StateType, ActionType>> node, double explorationFactor) {
		return *std::max_element(node->children.begin(), node->children.end(), [explorationFactor](const auto& a, const auto& b) {
			double uctA = (a->totalValue / (a->visits + 1e-6)) + explorationFactor * sqrt(log(a->parent->visits + 1) / (a->visits + 1e-6));
			double uctB = (b->totalValue / (b->visits + 1e-6)) + explorationFactor * sqrt(log(b->parent->visits + 1) / (b->visits + 1e-6));
			return uctA < uctB;
		});
	}
};
