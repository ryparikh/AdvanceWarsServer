#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <random>
#include <type_traits>
#include <utility>
#include <vector>

struct MCTSOptions {
	int maxSimulations{ 1000 };
	int maxNodes{ 10000 };
	int maxRolloutActions{ 512 };
	double explorationConstant{ std::sqrt(2.0) };
	double temperature{ 0.0 };
	std::uint32_t seed{ 0 };
};

template <typename ActionType>
struct MCTSActionStats {
	ActionType action;
	int visits{ 0 };
	double totalValue{ 0.0 };
	double averageValue{ 0.0 };
	double prior{ 0.0 };
};

template <typename ActionType>
struct MCTSActionPrior {
	ActionType action;
	double prior{ 0.0 };
};

template <typename ActionType>
struct MCTSNeuralEvaluation {
	double value{ 0.0 };
	std::vector<MCTSActionPrior<ActionType>> actionPriors;
};

template <typename ActionType>
struct MCTSSearchResult {
	std::optional<ActionType> selectedAction;
	std::vector<MCTSActionStats<ActionType>> rootActionStats;
	int simulationsRun{ 0 };
	int nodesCreated{ 0 };
};

// StateType must provide:
// - std::vector<ActionType> getLegalActions() const
// - StateType applyAction(const ActionType&) const
// - optional StateType applyKnownLegalAction(const ActionType&) for known-legal MCTS actions
// - bool isTerminal() const
// - int getCurrentPlayer() const
// - double-compatible evaluate(int player) const
// ActionType must be equality-comparable.
namespace MctsDetail {
template <typename StateType, typename ActionType, typename = void>
struct HasKnownLegalApply : std::false_type {
};

template <typename StateType, typename ActionType>
struct HasKnownLegalApply<
	StateType,
	ActionType,
	std::void_t<decltype(std::declval<StateType&>().applyKnownLegalAction(std::declval<const ActionType&>()))>> : std::true_type {
};

template <typename StateType, typename ActionType>
StateType ApplyKnownLegalAction(StateType& state, const ActionType& action) {
	if constexpr (HasKnownLegalApply<StateType, ActionType>::value) {
		return state.applyKnownLegalAction(action);
	}
	else {
		return state.applyAction(action);
	}
}
}

template <typename StateType, typename ActionType>
class MCTS {
public:
	MCTSSearchResult<ActionType> search(const StateType& rootState, const MCTSOptions& options = MCTSOptions()) {
		std::mt19937 rng(options.seed);
		SearchContext context{ std::max(1, options.maxNodes), 1 };

		Node root(rootState, std::nullopt, nullptr);
		MCTSSearchResult<ActionType> result;
		result.nodesCreated = context.nodesCreated;

		if (root.state.isTerminal() || root.legalActions.empty()) {
			result.rootActionStats = BuildRootActionStats(root);
			return result;
		}

		const int maxSimulations = std::max(0, options.maxSimulations);
		for (int i = 0; i < maxSimulations; ++i) {
			Node* selected = Select(root, options.explorationConstant, context.nodesCreated < context.maxNodes);
			Node* simulationStart = selected;

			if (!selected->state.isTerminal() &&
				!selected->unexpandedActions.empty() &&
				context.nodesCreated < context.maxNodes) {
				simulationStart = ExpandOne(*selected, rng);
				++context.nodesCreated;
			}

			bool reachedTerminal = false;
			StateType rolloutState = Rollout(simulationStart->state, std::max(0, options.maxRolloutActions), rng, reachedTerminal);
			Backpropagate(simulationStart, rolloutState, reachedTerminal);
			++result.simulationsRun;
		}

		result.nodesCreated = context.nodesCreated;
		result.rootActionStats = BuildRootActionStats(root);
		result.selectedAction = SelectAction(result.rootActionStats, options.temperature, rng);
		return result;
	}

	template <typename EvaluatorType>
	MCTSSearchResult<ActionType> searchNeural(const StateType& rootState, EvaluatorType& evaluator, const MCTSOptions& options = MCTSOptions()) {
		std::mt19937 rng(options.seed);
		SearchContext context{ std::max(1, options.maxNodes), 1 };

		Node root(rootState, std::nullopt, nullptr);
		MCTSSearchResult<ActionType> result;
		result.nodesCreated = context.nodesCreated;

		if (root.state.isTerminal() || root.legalActions.empty()) {
			result.rootActionStats = BuildRootActionStats(root);
			return result;
		}

		double rootValue = 0.0;
		EvaluateAndExpand(root, evaluator, context, rootValue);

		const int maxSimulations = std::max(0, options.maxSimulations);
		for (int i = 0; i < maxSimulations; ++i) {
			Node* leaf = SelectNeural(root, options.explorationConstant);
			double value = 0.0;
			if (leaf->state.isTerminal()) {
				value = static_cast<double>(leaf->state.evaluate(leaf->state.getCurrentPlayer()));
			}
			else if (!leaf->neuralExpanded) {
				EvaluateAndExpand(*leaf, evaluator, context, value);
			}
			else {
				value = leaf->evaluatedValue;
			}

			BackpropagateNeural(leaf, value);
			++result.simulationsRun;
		}

		result.nodesCreated = context.nodesCreated;
		result.rootActionStats = BuildRootActionStats(root);
		result.selectedAction = SelectAction(result.rootActionStats, options.temperature, rng);
		return result;
	}

private:
	struct Node {
		StateType state;
		std::optional<ActionType> action;
		Node* parent{ nullptr };
		double prior{ 0.0 };
		std::vector<ActionType> legalActions;
		std::vector<double> legalActionPriors;
		std::vector<ActionType> unexpandedActions;
		std::vector<std::unique_ptr<Node>> children;
		int visits{ 0 };
		double totalValue{ 0.0 };
		bool neuralExpanded{ false };
		double evaluatedValue{ 0.0 };

		Node(const StateType& state, std::optional<ActionType> action, Node* parent, double prior = 0.0) :
			state(state),
			action(std::move(action)),
			parent(parent),
			prior(prior) {
			if (!this->state.isTerminal()) {
				legalActions = this->state.getLegalActions();
				legalActionPriors.assign(legalActions.size(), 0.0);
				unexpandedActions = legalActions;
			}
		}
	};

	struct SearchContext {
		int maxNodes{ 1 };
		int nodesCreated{ 1 };
	};

	Node* Select(Node& root, double explorationConstant, bool canExpand) {
		Node* node = &root;
		while (!node->state.isTerminal()) {
			if (!node->unexpandedActions.empty() && (canExpand || node->children.empty())) {
				return node;
			}

			if (node->children.empty()) {
				return node;
			}

			node = BestChild(*node, explorationConstant);
		}

		return node;
	}

	Node* ExpandOne(Node& node, std::mt19937& rng) {
		std::uniform_int_distribution<int> distribution(0, static_cast<int>(node.unexpandedActions.size() - 1));
		const int actionIndex = distribution(rng);
		ActionType action = node.unexpandedActions[static_cast<std::size_t>(actionIndex)];
		node.unexpandedActions.erase(node.unexpandedActions.begin() + actionIndex);

		StateType newState = MctsDetail::ApplyKnownLegalAction(node.state, action);
		node.children.push_back(std::make_unique<Node>(newState, action, &node));
		return node.children.back().get();
	}

	template <typename EvaluatorType>
	void EvaluateAndExpand(Node& node, EvaluatorType& evaluator, SearchContext& context, double& value) {
		if (node.state.isTerminal()) {
			value = static_cast<double>(node.state.evaluate(node.state.getCurrentPlayer()));
			node.evaluatedValue = value;
			node.neuralExpanded = true;
			return;
		}

		if (node.legalActions.empty()) {
			value = 0.0;
			node.evaluatedValue = value;
			node.neuralExpanded = true;
			return;
		}

		MCTSNeuralEvaluation<ActionType> evaluation = evaluator.evaluate(node.state, node.legalActions);
		value = std::isfinite(evaluation.value) ? evaluation.value : 0.0;
		node.evaluatedValue = value;
		node.legalActionPriors = NormalizePriors(node.legalActions, evaluation.actionPriors);
		node.neuralExpanded = true;

		for (std::size_t i = 0; i < node.legalActions.size() && context.nodesCreated < context.maxNodes; ++i) {
			if (FindChild(node, node.legalActions[i]) != nullptr) {
				continue;
			}
			StateType newState = MctsDetail::ApplyKnownLegalAction(node.state, node.legalActions[i]);
			node.children.push_back(std::make_unique<Node>(newState, node.legalActions[i], &node, node.legalActionPriors[i]));
			++context.nodesCreated;
		}
	}

	StateType Rollout(const StateType& startState, int maxRolloutActions, std::mt19937& rng, bool& reachedTerminal) {
		StateType state(startState);
		for (int i = 0; i < maxRolloutActions && !state.isTerminal(); ++i) {
			std::vector<ActionType> actions = state.getLegalActions();
			if (actions.empty()) {
				break;
			}

			std::uniform_int_distribution<int> distribution(0, static_cast<int>(actions.size() - 1));
			state = MctsDetail::ApplyKnownLegalAction(state, actions[static_cast<std::size_t>(distribution(rng))]);
		}

		reachedTerminal = state.isTerminal();
		return state;
	}

	void Backpropagate(Node* node, const StateType& rolloutState, bool reachedTerminal) {
		while (node != nullptr) {
			++node->visits;
			const double value = reachedTerminal ? static_cast<double>(rolloutState.evaluate(node->state.getCurrentPlayer())) : 0.0;
			node->totalValue += value;
			node = node->parent;
		}
	}

	Node* SelectNeural(Node& root, double explorationConstant) {
		Node* node = &root;
		while (!node->state.isTerminal() && node->neuralExpanded && !node->children.empty()) {
			node = BestPuctChild(*node, explorationConstant);
			if (!node->neuralExpanded) {
				break;
			}
		}
		return node;
	}

	Node* BestChild(Node& node, double explorationConstant) {
		return std::max_element(node.children.begin(), node.children.end(), [&](const std::unique_ptr<Node>& left, const std::unique_ptr<Node>& right) {
			return UctValue(node, *left, explorationConstant) < UctValue(node, *right, explorationConstant);
		})->get();
	}

	Node* BestPuctChild(Node& node, double explorationConstant) {
		return std::max_element(node.children.begin(), node.children.end(), [&](const std::unique_ptr<Node>& left, const std::unique_ptr<Node>& right) {
			return PuctValue(node, *left, explorationConstant) < PuctValue(node, *right, explorationConstant);
		})->get();
	}

	double UctValue(const Node& parent, const Node& child, double explorationConstant) const {
		if (child.visits == 0) {
			return std::numeric_limits<double>::infinity();
		}

		double averageValue = child.totalValue / child.visits;
		if (child.state.getCurrentPlayer() != parent.state.getCurrentPlayer()) {
			averageValue *= -1.0;
		}

		const double exploration = explorationConstant * std::sqrt(std::log(parent.visits + 1.0) / child.visits);
		return averageValue + exploration;
	}

	double PuctValue(const Node& parent, const Node& child, double explorationConstant) const {
		double averageValue = 0.0;
		if (child.visits > 0) {
			averageValue = child.totalValue / child.visits;
			if (child.state.getCurrentPlayer() != parent.state.getCurrentPlayer()) {
				averageValue *= -1.0;
			}
		}

		const double exploration = explorationConstant * child.prior * std::sqrt(parent.visits + 1.0) / (1.0 + child.visits);
		return averageValue + exploration;
	}

	void BackpropagateNeural(Node* node, double leafValue) {
		const int leafPlayer = node->state.getCurrentPlayer();
		while (node != nullptr) {
			++node->visits;
			double value = leafValue;
			if (node->state.getCurrentPlayer() != leafPlayer) {
				value *= -1.0;
			}
			node->totalValue += value;
			node = node->parent;
		}
	}

	std::vector<MCTSActionStats<ActionType>> BuildRootActionStats(const Node& root) const {
		std::vector<MCTSActionStats<ActionType>> stats;
		stats.reserve(root.legalActions.size());

		for (std::size_t i = 0; i < root.legalActions.size(); ++i) {
			const ActionType& action = root.legalActions[i];
			MCTSActionStats<ActionType> actionStats;
			actionStats.action = action;
			if (i < root.legalActionPriors.size()) {
				actionStats.prior = root.legalActionPriors[i];
			}

			const Node* child = FindChild(root, action);
			if (child != nullptr) {
				actionStats.prior = child->prior;
				actionStats.visits = child->visits;
				actionStats.totalValue = child->totalValue;
				if (child->state.getCurrentPlayer() != root.state.getCurrentPlayer()) {
					actionStats.totalValue *= -1.0;
				}
				actionStats.averageValue = actionStats.visits == 0 ? 0.0 : actionStats.totalValue / actionStats.visits;
			}

			stats.push_back(actionStats);
		}

		return stats;
	}

	const Node* FindChild(const Node& node, const ActionType& action) const {
		for (const std::unique_ptr<Node>& child : node.children) {
			if (child->action.has_value() && child->action.value() == action) {
				return child.get();
			}
		}

		return nullptr;
	}

	std::vector<double> NormalizePriors(const std::vector<ActionType>& legalActions, const std::vector<MCTSActionPrior<ActionType>>& actionPriors) const {
		std::vector<double> priors(legalActions.size(), 0.0);
		double total = 0.0;
		for (std::size_t i = 0; i < legalActions.size(); ++i) {
			for (const MCTSActionPrior<ActionType>& actionPrior : actionPriors) {
				if (actionPrior.action == legalActions[i] && std::isfinite(actionPrior.prior) && actionPrior.prior > 0.0) {
					priors[i] += actionPrior.prior;
				}
			}
			total += priors[i];
		}

		if (total <= 0.0) {
			const double uniform = legalActions.empty() ? 0.0 : 1.0 / static_cast<double>(legalActions.size());
			std::fill(priors.begin(), priors.end(), uniform);
			return priors;
		}

		for (double& prior : priors) {
			prior /= total;
		}
		return priors;
	}

	std::optional<ActionType> SelectAction(const std::vector<MCTSActionStats<ActionType>>& stats, double temperature, std::mt19937& rng) const {
		if (stats.empty()) {
			return std::nullopt;
		}

		if (temperature <= 0.0) {
			const MCTSActionStats<ActionType>* best = &stats.front();
			for (const MCTSActionStats<ActionType>& actionStats : stats) {
				if (actionStats.visits > best->visits) {
					best = &actionStats;
				}
			}

			return best->action;
		}

		bool hasPositiveVisits = false;
		for (const MCTSActionStats<ActionType>& actionStats : stats) {
			if (actionStats.visits > 0) {
				hasPositiveVisits = true;
				break;
			}
		}

		std::vector<double> weights;
		weights.reserve(stats.size());
		for (const MCTSActionStats<ActionType>& actionStats : stats) {
			if (!hasPositiveVisits) {
				weights.push_back(1.0);
			}
			else if (actionStats.visits > 0) {
				weights.push_back(std::pow(static_cast<double>(actionStats.visits), 1.0 / temperature));
			}
			else {
				weights.push_back(0.0);
			}
		}

		std::discrete_distribution<int> distribution(weights.begin(), weights.end());
		return stats[static_cast<std::size_t>(distribution(rng))].action;
	}
};
