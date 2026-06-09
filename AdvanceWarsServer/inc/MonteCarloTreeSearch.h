#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
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
	int legalActionGenerationCalls{ 0 };
	double legalActionGenerationTimeMs{ 0.0 };
};

// StateType must provide:
// - std::vector<ActionType> getLegalActions() const
// - optional void/result getLegalActions(std::vector<ActionType>&) const for caller-owned action storage
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

template <typename StateType, typename ActionType, typename = void>
struct HasFillLegalActions : std::false_type {
};

template <typename StateType, typename ActionType>
struct HasFillLegalActions<
	StateType,
	ActionType,
	std::void_t<decltype(std::declval<const StateType&>().getLegalActions(std::declval<std::vector<ActionType>&>()))>> : std::true_type {
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
		InitializeNode(root, context, true);
		MCTSSearchResult<ActionType> result;

		if (root.state.isTerminal() || root.legalActions.empty()) {
			result.rootActionStats = BuildRootActionStats(root);
			FinalizeSearchResult(result, context);
			return result;
		}

		const int maxSimulations = std::max(0, options.maxSimulations);
		for (int i = 0; i < maxSimulations; ++i) {
			Node* selected = Select(root, options.explorationConstant, context.nodesCreated < context.maxNodes);
			Node* simulationStart = selected;

			if (!selected->state.isTerminal() &&
				!selected->unexpandedActionIndices.empty() &&
				context.nodesCreated < context.maxNodes) {
				simulationStart = ExpandOne(*selected, context, rng);
				++context.nodesCreated;
			}

			bool reachedTerminal = false;
			StateType rolloutState = Rollout(simulationStart->state, std::max(0, options.maxRolloutActions), context, rng, reachedTerminal);
			Backpropagate(simulationStart, rolloutState, reachedTerminal);
			++result.simulationsRun;
		}

		result.rootActionStats = BuildRootActionStats(root);
		result.selectedAction = SelectAction(result.rootActionStats, options.temperature, rng);
		FinalizeSearchResult(result, context);
		return result;
	}

	template <typename EvaluatorType>
	MCTSSearchResult<ActionType> searchNeural(const StateType& rootState, EvaluatorType& evaluator, const MCTSOptions& options = MCTSOptions()) {
		std::mt19937 rng(options.seed);
		SearchContext context{ std::max(1, options.maxNodes), 1 };

		Node root(rootState, std::nullopt, nullptr);
		InitializeNode(root, context, false);
		MCTSSearchResult<ActionType> result;

		if (root.state.isTerminal() || root.legalActions.empty()) {
			result.rootActionStats = BuildRootActionStats(root);
			FinalizeSearchResult(result, context);
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

		result.rootActionStats = BuildRootActionStats(root);
		result.selectedAction = SelectAction(result.rootActionStats, options.temperature, rng);
		FinalizeSearchResult(result, context);
		return result;
	}

private:
	struct SearchContext {
		int maxNodes{ 1 };
		int nodesCreated{ 1 };
		int legalActionGenerationCalls{ 0 };
		double legalActionGenerationTimeMs{ 0.0 };
	};

	struct Node {
		StateType state;
		std::optional<ActionType> action;
		Node* parent{ nullptr };
		double prior{ 0.0 };
		std::vector<ActionType> legalActions;
		std::vector<double> legalActionPriors;
		std::vector<std::size_t> unexpandedActionIndices;
		std::vector<std::unique_ptr<Node>> children;
		int visits{ 0 };
		double totalValue{ 0.0 };
		bool legalActionsLoaded{ false };
		bool neuralExpanded{ false };
		double evaluatedValue{ 0.0 };

		Node(StateType state, std::optional<ActionType> action, Node* parent, double prior = 0.0) :
			state(std::move(state)),
			action(std::move(action)),
			parent(parent),
			prior(prior) {
		}
	};

	void LoadLegalActions(const StateType& state, std::vector<ActionType>& legalActions, SearchContext& context) const {
		const auto start = std::chrono::steady_clock::now();
		legalActions.clear();
		if constexpr (MctsDetail::HasFillLegalActions<StateType, ActionType>::value) {
			state.getLegalActions(legalActions);
		}
		else {
			legalActions = state.getLegalActions();
		}
		const auto end = std::chrono::steady_clock::now();
		++context.legalActionGenerationCalls;
		context.legalActionGenerationTimeMs += std::chrono::duration<double, std::milli>(end - start).count();
	}

	void InitializeNode(Node& node, SearchContext& context, bool trackUnexpandedActions) const {
		if (node.legalActionsLoaded) {
			return;
		}

		if (node.state.isTerminal()) {
			node.legalActionsLoaded = true;
			return;
		}

		LoadLegalActions(node.state, node.legalActions, context);
		node.legalActionsLoaded = true;
		if (!trackUnexpandedActions) {
			return;
		}

		node.unexpandedActionIndices.clear();
		node.unexpandedActionIndices.reserve(node.legalActions.size());
		for (std::size_t i = 0; i < node.legalActions.size(); ++i) {
			node.unexpandedActionIndices.push_back(i);
		}
	}

	void FinalizeSearchResult(MCTSSearchResult<ActionType>& result, const SearchContext& context) const {
		result.nodesCreated = context.nodesCreated;
		result.legalActionGenerationCalls = context.legalActionGenerationCalls;
		result.legalActionGenerationTimeMs = context.legalActionGenerationTimeMs;
	}

	Node* Select(Node& root, double explorationConstant, bool canExpand) {
		Node* node = &root;
		while (!node->state.isTerminal()) {
			if (!node->unexpandedActionIndices.empty() && (canExpand || node->children.empty())) {
				return node;
			}

			if (node->children.empty()) {
				return node;
			}

			node = BestChild(*node, explorationConstant);
		}

		return node;
	}

	Node* ExpandOne(Node& node, SearchContext& context, std::mt19937& rng) {
		std::uniform_int_distribution<int> distribution(0, static_cast<int>(node.unexpandedActionIndices.size() - 1));
		const std::size_t unexpandedIndex = static_cast<std::size_t>(distribution(rng));
		const std::size_t actionIndex = node.unexpandedActionIndices[unexpandedIndex];
		node.unexpandedActionIndices[unexpandedIndex] = node.unexpandedActionIndices.back();
		node.unexpandedActionIndices.pop_back();

		const ActionType& action = node.legalActions[actionIndex];
		StateType newState = MctsDetail::ApplyKnownLegalAction(node.state, action);
		auto child = std::make_unique<Node>(std::move(newState), action, &node);
		InitializeNode(*child, context, true);
		node.children.push_back(std::move(child));
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

		InitializeNode(node, context, false);
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

		const int remainingNodes = context.maxNodes - context.nodesCreated;
		if (remainingNodes > 0) {
			const std::size_t childCapacity = std::min(node.legalActions.size(), static_cast<std::size_t>(remainingNodes));
			node.children.reserve(node.children.size() + childCapacity);
		}

		for (std::size_t i = 0; i < node.legalActions.size() && context.nodesCreated < context.maxNodes; ++i) {
			StateType newState = MctsDetail::ApplyKnownLegalAction(node.state, node.legalActions[i]);
			auto child = std::make_unique<Node>(std::move(newState), node.legalActions[i], &node, node.legalActionPriors[i]);
			node.children.push_back(std::move(child));
			++context.nodesCreated;
		}
	}

	StateType Rollout(const StateType& startState, int maxRolloutActions, SearchContext& context, std::mt19937& rng, bool& reachedTerminal) {
		StateType state(startState);
		std::vector<ActionType> actions;
		for (int i = 0; i < maxRolloutActions && !state.isTerminal(); ++i) {
			LoadLegalActions(state, actions, context);
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
			MCTSActionStats<ActionType> actionStats{ action };
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

			stats.push_back(std::move(actionStats));
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
