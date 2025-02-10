#pragma once

#include <string>
#include <vector>
#include <memory>
#include "Reward.h"

// Node class for MCTS
template <typename StateType, typename ActionType>
class MCTSNode {
public:
	std::shared_ptr<MCTSNode> parent;
	std::vector<std::shared_ptr<MCTSNode>> children;
	std::vector<ActionType> legalActions;
	ActionType action;
	StateType state;
	int visits;
	double totalValue;

	MCTSNode(const StateType& state, const ActionType& action, std::shared_ptr<MCTSNode> parent = nullptr)
		: state(state), action(action), parent(parent), visits(0), totalValue(0.0) {
		legalActions = state.getLegalActions();
	}

	bool isFullyExpanded() const {
		return !children.empty() && children.size() == state.getLegalActions().size();
	}

	bool isLeaf() const {
		return children.empty();
	}

	void detachNode(std::shared_ptr<MCTSNode> spNode) {
		for (auto it = children.begin(); it != children.end(); ++it) {
			if (it->get() == spNode.get()) {
				children.erase(it);
				return;
			}
		}
	}

	void freeMemory() {
		if (children.empty()) {
			parent.reset();
			return;
		}

		for (auto& child : children) {
			child->freeMemory();
			child.reset();
		}
	}
};

// MCTS algorithm class
template <typename StateType, typename ActionType>
class MCTS {
public:
	std::shared_ptr<MCTSNode<StateType, ActionType>> run(std::shared_ptr<MCTSNode<StateType, ActionType>> root, int iterations) {
		expand(root);
		for (int i = 0; i < iterations; ++i) {
			if ((i + 1) % 500 == 0) {
				std::cout << "simmed iterations: " << i << std::endl;
			}

			auto node = select(root);
			if (!node->state.isTerminal()) {
				expand(node);
			}

			Reward reward = simulate(node);
			backpropagate(node, reward);
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
		if (node->isFullyExpanded()) {
			return;
		}

		for (const auto& action : node->legalActions) {
			auto newState = node->state.applyAction(action);
			node->children.push_back(std::make_shared<MCTSNode<StateType, ActionType>>(newState, action, node));
		}
	}

	Reward simulate(std::shared_ptr<MCTSNode<StateType, ActionType>> node) {
		StateType state{ node->state };
		int totalSimStates = 0;
		time_point startTime = std::chrono::steady_clock::now();
		while (!state.isTerminal()) {
			auto actions = state.getLegalActions();
			if (actions.empty()) break;
			ActionType action = actions[rand() % actions.size()];
			++totalSimStates;
			state = state.applyAction(action);
			if (totalSimStates > 100000) {
				break;
			}
		}
		time_point endTime = std::chrono::steady_clock::now();
		Reward reward = { state.getCurrentPlayer(), state.getEvaluationForCurrentPlayer() };
		return reward;
	}

	void backpropagate(std::shared_ptr<MCTSNode<StateType, ActionType>> node, Reward reward) {
		while (node != nullptr) {
			node->visits++;
			bool invertReward = node->state.getCurrentPlayer() != reward.player;
			if (node->action.m_type == Action::Type::EndTurn) {
				invertReward = !invertReward;
			}

			if (!invertReward) {
				node->totalValue += reward.value;
			}
			else {
				node->totalValue += reward.value * -1;
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
