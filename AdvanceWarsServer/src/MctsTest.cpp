#include "MctsTest.h"

#include "MonteCarloTreeSearch.h"

#include <cmath>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {
struct TestAction {
	int id{ 0 };

	bool operator==(const TestAction& other) const noexcept {
		return id == other.id;
	}
};

struct TestNodeDef {
	int player{ 0 };
	std::optional<int> winner;
	std::vector<TestAction> legalActions;
	std::vector<std::pair<TestAction, int>> transitions;
};

struct TestGraph {
	std::vector<TestNodeDef> nodes;
	mutable int applyActionCalls{ 0 };
	mutable int applyKnownLegalActionCalls{ 0 };
};

class TestState {
public:
	TestState(std::shared_ptr<const TestGraph> graph, int nodeId) :
		m_graph(std::move(graph)),
		m_nodeId(nodeId) {
	}

	std::vector<TestAction> getLegalActions() const {
		const TestNodeDef& node = GetNode();
		if (node.winner.has_value()) {
			return {};
		}

		return node.legalActions;
	}

	TestState applyAction(const TestAction& action) const {
		++m_graph->applyActionCalls;
		return ApplyTransition(action);
	}

	TestState applyKnownLegalAction(const TestAction& action) const {
		++m_graph->applyKnownLegalActionCalls;
		return ApplyTransition(action);
	}

	bool isTerminal() const {
		return GetNode().winner.has_value();
	}

	int getCurrentPlayer() const {
		return GetNode().player;
	}

	double evaluate(int player) const {
		const TestNodeDef& node = GetNode();
		if (!node.winner.has_value() || *node.winner < 0) {
			return 0.0;
		}

		return *node.winner == player ? 1.0 : -1.0;
	}

	int getApplyActionCalls() const {
		return m_graph->applyActionCalls;
	}

	int getApplyKnownLegalActionCalls() const {
		return m_graph->applyKnownLegalActionCalls;
	}

private:
	const TestNodeDef& GetNode() const {
		return m_graph->nodes.at(static_cast<std::size_t>(m_nodeId));
	}

	TestState ApplyTransition(const TestAction& action) const {
		for (const auto& transition : GetNode().transitions) {
			if (transition.first == action) {
				return TestState(m_graph, transition.second);
			}
		}

		throw std::runtime_error("test action was not legal in scripted state");
	}

	std::shared_ptr<const TestGraph> m_graph;
	int m_nodeId{ 0 };
};

TestState MakeState(std::vector<TestNodeDef> nodes, int nodeId = 0) {
	auto graph = std::make_shared<TestGraph>();
	graph->nodes = std::move(nodes);
	return TestState(graph, nodeId);
}

bool Expect(bool condition, const std::string& message) {
	if (!condition) {
		std::cerr << "MCTS test failed: " << message << std::endl;
	}

	return condition;
}

const MCTSActionStats<TestAction>* FindStats(const MCTSSearchResult<TestAction>& result, int actionId) {
	for (const MCTSActionStats<TestAction>& stats : result.rootActionStats) {
		if (stats.action.id == actionId) {
			return &stats;
		}
	}

	return nullptr;
}

int VisitsFor(const MCTSSearchResult<TestAction>& result, int actionId) {
	const MCTSActionStats<TestAction>* stats = FindStats(result, actionId);
	return stats == nullptr ? -1 : stats->visits;
}

double AverageFor(const MCTSSearchResult<TestAction>& result, int actionId) {
	const MCTSActionStats<TestAction>* stats = FindStats(result, actionId);
	return stats == nullptr ? 999.0 : stats->averageValue;
}

bool SelectedActionIs(const MCTSSearchResult<TestAction>& result, int actionId) {
	return result.selectedAction.has_value() && result.selectedAction->id == actionId;
}

int CountVisitedRootActions(const MCTSSearchResult<TestAction>& result) {
	int count = 0;
	for (const MCTSActionStats<TestAction>& stats : result.rootActionStats) {
		if (stats.visits > 0) {
			++count;
		}
	}

	return count;
}

int SumRootVisits(const MCTSSearchResult<TestAction>& result) {
	int visits = 0;
	for (const MCTSActionStats<TestAction>& stats : result.rootActionStats) {
		visits += stats.visits;
	}

	return visits;
}

bool NearlyEqual(double left, double right) {
	return std::fabs(left - right) < 1e-9;
}

bool ResultsEqual(const MCTSSearchResult<TestAction>& left, const MCTSSearchResult<TestAction>& right) {
	if (left.selectedAction.has_value() != right.selectedAction.has_value() ||
		left.simulationsRun != right.simulationsRun ||
		left.nodesCreated != right.nodesCreated ||
		left.rootActionStats.size() != right.rootActionStats.size()) {
		return false;
	}

	if (left.selectedAction.has_value() && !(left.selectedAction.value() == right.selectedAction.value())) {
		return false;
	}

	for (std::size_t i = 0; i < left.rootActionStats.size(); ++i) {
		if (!(left.rootActionStats[i].action == right.rootActionStats[i].action) ||
			left.rootActionStats[i].visits != right.rootActionStats[i].visits ||
			!NearlyEqual(left.rootActionStats[i].totalValue, right.rootActionStats[i].totalValue) ||
			!NearlyEqual(left.rootActionStats[i].averageValue, right.rootActionStats[i].averageValue)) {
			return false;
		}
	}

	return true;
}

TestState MakeThreeTerminalActionState() {
	return MakeState({
		{ 0, std::nullopt, { { 1 }, { 2 }, { 3 } }, { { { 1 }, 1 }, { { 2 }, 2 }, { { 3 }, 3 } } },
		{ 0, 0, {}, {} },
		{ 0, 0, {}, {} },
		{ 0, 0, {}, {} },
	});
}

bool TestOneActionExpansionAndRootStats() {
	MCTS<TestState, TestAction> mcts;
	MCTSOptions options;
	options.maxSimulations = 1;
	options.maxNodes = 10;
	options.maxRolloutActions = 0;
	options.temperature = 0.0;
	options.seed = 7;

	MCTSSearchResult<TestAction> result = mcts.search(MakeThreeTerminalActionState(), options);

	return Expect(result.simulationsRun == 1, "one simulation should run") &&
		Expect(result.nodesCreated == 2, "root plus one child should be created") &&
		Expect(result.rootActionStats.size() == 3, "all root legal actions should be present") &&
		Expect(result.rootActionStats[0].action.id == 1 && result.rootActionStats[1].action.id == 2 && result.rootActionStats[2].action.id == 3, "root stats should preserve legal action order") &&
		Expect(SumRootVisits(result) == 1, "only one root action should be visited") &&
		Expect(CountVisitedRootActions(result) == 1, "only one root child should be expanded") &&
		Expect(result.selectedAction.has_value(), "a selected action should be returned for non-empty root");
}

bool TestSearchExpansionUsesKnownLegalApplyPath() {
	TestState root = MakeThreeTerminalActionState();

	MCTS<TestState, TestAction> mcts;
	MCTSOptions options;
	options.maxSimulations = 1;
	options.maxNodes = 10;
	options.maxRolloutActions = 0;
	options.temperature = 0.0;
	options.seed = 7;

	MCTSSearchResult<TestAction> result = mcts.search(root, options);

	return Expect(result.nodesCreated == 2, "test setup should create one expanded child") &&
		Expect(root.getApplyKnownLegalActionCalls() == 1, "MCTS expansion should use the known-legal apply path") &&
		Expect(root.getApplyActionCalls() == 0, "MCTS expansion should not use the validating apply path for known-legal actions");
}

bool TestSeededSearchIsReproducible() {
	MCTS<TestState, TestAction> mcts;
	MCTSOptions options;
	options.maxSimulations = 12;
	options.maxNodes = 20;
	options.maxRolloutActions = 1;
	options.temperature = 1.0;
	options.seed = 42;

	MCTSSearchResult<TestAction> first = mcts.search(MakeThreeTerminalActionState(), options);
	MCTSSearchResult<TestAction> second = mcts.search(MakeThreeTerminalActionState(), options);

	return Expect(ResultsEqual(first, second), "same seed and state should produce identical search results");
}

bool TestPlayerPerspectiveBackupHandlesSamePlayerAndSwitches() {
	TestState root = MakeState({
		{ 0, std::nullopt, { { 10 }, { 20 } }, { { { 10 }, 1 }, { { 20 }, 2 } } },
		{ 0, std::nullopt, { { 11 } }, { { { 11 }, 3 } } },
		{ 1, std::nullopt, { { 21 } }, { { { 21 }, 4 } } },
		{ 0, 0, {}, {} },
		{ 1, 1, {}, {} },
	});

	MCTS<TestState, TestAction> mcts;
	MCTSOptions options;
	options.maxSimulations = 80;
	options.maxNodes = 10;
	options.maxRolloutActions = 4;
	options.temperature = 0.0;
	options.seed = 5;

	MCTSSearchResult<TestAction> result = mcts.search(root, options);

	return Expect(NearlyEqual(AverageFor(result, 10), 1.0), "same-player winning chain should average +1 for root player") &&
		Expect(NearlyEqual(AverageFor(result, 20), -1.0), "turn-switch opponent win should average -1 for root player") &&
		Expect(VisitsFor(result, 10) > VisitsFor(result, 20), "UCT should prefer the action that wins for the root player") &&
		Expect(SelectedActionIs(result, 10), "temperature zero should select the most visited winning root action");
}

bool TestRolloutCutoffBacksUpZero() {
	TestState root = MakeState({
		{ 0, std::nullopt, { { 1 } }, { { { 1 }, 1 } } },
		{ 0, std::nullopt, { { 2 } }, { { { 2 }, 2 } } },
		{ 0, 0, {}, {} },
	});

	MCTS<TestState, TestAction> mcts;
	MCTSOptions options;
	options.maxSimulations = 1;
	options.maxNodes = 3;
	options.maxRolloutActions = 0;
	options.temperature = 0.0;
	options.seed = 0;

	MCTSSearchResult<TestAction> result = mcts.search(root, options);

	return Expect(VisitsFor(result, 1) == 1, "expanded action should receive one visit") &&
		Expect(NearlyEqual(AverageFor(result, 1), 0.0), "non-terminal rollout cutoff should back up zero");
}

bool TestNodeLimitPreventsExpansion() {
	MCTS<TestState, TestAction> mcts;
	MCTSOptions options;
	options.maxSimulations = 3;
	options.maxNodes = 0;
	options.maxRolloutActions = 2;
	options.temperature = 0.0;
	options.seed = 0;

	MCTSSearchResult<TestAction> result = mcts.search(MakeThreeTerminalActionState(), options);

	return Expect(result.simulationsRun == 3, "simulations can still run from existing nodes") &&
		Expect(result.nodesCreated == 1, "maxNodes should normalize to the root node") &&
		Expect(SumRootVisits(result) == 0, "no root children should be visited when expansion is blocked") &&
		Expect(SelectedActionIs(result, 1), "temperature zero fallback should select first legal action when all visits are zero");
}

bool TestNodeLimitReusesExistingChildrenWhenExpansionIsBlocked() {
	MCTS<TestState, TestAction> mcts;
	MCTSOptions options;
	options.maxSimulations = 5;
	options.maxNodes = 2;
	options.maxRolloutActions = 0;
	options.temperature = 0.0;
	options.seed = 0;

	MCTSSearchResult<TestAction> result = mcts.search(MakeThreeTerminalActionState(), options);

	return Expect(result.nodesCreated == 2, "node limit should allow only root plus one child") &&
		Expect(result.simulationsRun == 5, "all requested simulations should still run") &&
		Expect(CountVisitedRootActions(result) == 1, "only one root child should exist under maxNodes=2") &&
		Expect(SumRootVisits(result) == 5, "existing child should keep receiving visits after expansion is blocked");
}

bool TestTerminalAndActionlessRootsReturnNoSelection() {
	MCTS<TestState, TestAction> mcts;
	MCTSOptions options;
	options.maxSimulations = 10;

	MCTSSearchResult<TestAction> terminal = mcts.search(MakeState({
		{ 0, 0, {}, {} },
	}), options);

	MCTSSearchResult<TestAction> actionless = mcts.search(MakeState({
		{ 0, std::nullopt, {}, {} },
	}), options);

	return Expect(!terminal.selectedAction.has_value(), "terminal root should not select an action") &&
		Expect(terminal.rootActionStats.empty(), "terminal root should have no action stats") &&
		Expect(terminal.simulationsRun == 0, "terminal root should run no simulations") &&
		Expect(terminal.nodesCreated == 1, "terminal root should still count the root node") &&
		Expect(!actionless.selectedAction.has_value(), "actionless root should not select an action") &&
		Expect(actionless.rootActionStats.empty(), "actionless root should have no action stats") &&
		Expect(actionless.simulationsRun == 0, "actionless root should run no simulations") &&
		Expect(actionless.nodesCreated == 1, "actionless root should still count the root node");
}

bool TestTemperatureZeroTieAndPositiveTemperatureSampling() {
	MCTS<TestState, TestAction> mcts;

	MCTSOptions tieOptions;
	tieOptions.maxSimulations = 0;
	tieOptions.temperature = 0.0;
	tieOptions.seed = 3;

	MCTSSearchResult<TestAction> tieResult = mcts.search(MakeThreeTerminalActionState(), tieOptions);

	MCTSOptions sampledOptions;
	sampledOptions.maxSimulations = 1;
	sampledOptions.maxNodes = 10;
	sampledOptions.maxRolloutActions = 0;
	sampledOptions.temperature = 1.0;
	sampledOptions.seed = 11;

	MCTSSearchResult<TestAction> sampledResult = mcts.search(MakeThreeTerminalActionState(), sampledOptions);
	int positiveVisitAction = -1;
	for (const MCTSActionStats<TestAction>& stats : sampledResult.rootActionStats) {
		if (stats.visits > 0) {
			positiveVisitAction = stats.action.id;
		}
	}

	return Expect(SelectedActionIs(tieResult, 1), "temperature zero should tie-break by legal action order") &&
		Expect(sampledResult.selectedAction.has_value(), "positive temperature should select an action") &&
		Expect(sampledResult.selectedAction->id == positiveVisitAction, "positive temperature should sample only positive-visit actions when any exist");
}

class TestPolicyValueEvaluator {
public:
	MCTSNeuralEvaluation<TestAction> evaluate(const TestState&, const std::vector<TestAction>& legalActions) {
		++calls;
		MCTSNeuralEvaluation<TestAction> evaluation;
		evaluation.value = 0.0;
		for (const TestAction& action : legalActions) {
			double prior = 1.0;
			if (action.id == 1) {
				prior = 0.05;
			}
			else if (action.id == 2) {
				prior = 0.15;
			}
			else if (action.id == 3) {
				prior = 0.80;
			}
			evaluation.actionPriors.push_back({ action, prior });
		}
		return evaluation;
	}

	int calls{ 0 };
};

class LeafValueEvaluator {
public:
	MCTSNeuralEvaluation<TestAction> evaluate(const TestState&, const std::vector<TestAction>& legalActions) {
		++calls;
		MCTSNeuralEvaluation<TestAction> evaluation;
		evaluation.value = 0.0;
		if (!legalActions.empty() && legalActions.front().id == 11) {
			evaluation.value = 0.9;
		}
		for (const TestAction& action : legalActions) {
			evaluation.actionPriors.push_back({ action, 1.0 });
		}
		return evaluation;
	}

	int calls{ 0 };
};

bool TestNeuralSearchUsesPuctPriorsForRootVisits() {
	MCTS<TestState, TestAction> mcts;
	MCTSOptions options;
	options.maxSimulations = 24;
	options.maxNodes = 10;
	options.maxRolloutActions = 0;
	options.explorationConstant = 2.0;
	options.temperature = 0.0;

	TestPolicyValueEvaluator evaluator;
	MCTSSearchResult<TestAction> result = mcts.searchNeural(MakeThreeTerminalActionState(), evaluator, options);

	return Expect(evaluator.calls == 1, "terminal children should only require root neural evaluation") &&
		Expect(result.simulationsRun == 24, "neural search should run requested simulations") &&
		Expect(result.nodesCreated == 4, "neural search should create root plus all root children") &&
		Expect(SumRootVisits(result) == 24, "neural root visits should sum to simulations") &&
		Expect(NearlyEqual(FindStats(result, 1)->prior, 0.05), "root stats should expose normalized prior for action 1") &&
		Expect(NearlyEqual(FindStats(result, 2)->prior, 0.15), "root stats should expose normalized prior for action 2") &&
		Expect(NearlyEqual(FindStats(result, 3)->prior, 0.80), "root stats should expose normalized prior for action 3") &&
		Expect(VisitsFor(result, 3) > VisitsFor(result, 2), "PUCT should visit the highest-prior root action most") &&
		Expect(VisitsFor(result, 2) > VisitsFor(result, 1), "PUCT should reflect lower prior actions in visit counts") &&
		Expect(SelectedActionIs(result, 3), "temperature zero should select the highest-visit neural action");
}

bool TestNeuralSearchBacksUpLeafValueWithoutRollout() {
	TestState root = MakeState({
		{ 0, std::nullopt, { { 1 }, { 2 } }, { { { 1 }, 1 }, { { 2 }, 2 } } },
		{ 0, std::nullopt, { { 11 } }, { { { 11 }, 3 } } },
		{ 0, std::nullopt, { { 21 } }, { { { 21 }, 4 } } },
		{ 0, 1, {}, {} },
		{ 0, 0, {}, {} },
	});

	MCTS<TestState, TestAction> mcts;
	MCTSOptions options;
	options.maxSimulations = 1;
	options.maxNodes = 10;
	options.maxRolloutActions = 100;
	options.temperature = 0.0;

	LeafValueEvaluator evaluator;
	MCTSSearchResult<TestAction> result = mcts.searchNeural(root, evaluator, options);

	return Expect(evaluator.calls == 2, "neural search should evaluate the root and selected leaf") &&
		Expect(VisitsFor(result, 1) == 1, "first legal action should receive the single PUCT visit") &&
		Expect(NearlyEqual(AverageFor(result, 1), 0.9), "leaf value should back up without rolling out to the scripted loss") &&
		Expect(VisitsFor(result, 2) == 0, "unvisited neural root actions should remain in stats with zero visits") &&
		Expect(SelectedActionIs(result, 1), "temperature zero should select the visited neural action");
}
}

int RunMctsTests() {
	bool passed = true;
	passed = TestOneActionExpansionAndRootStats() && passed;
	passed = TestSearchExpansionUsesKnownLegalApplyPath() && passed;
	passed = TestSeededSearchIsReproducible() && passed;
	passed = TestPlayerPerspectiveBackupHandlesSamePlayerAndSwitches() && passed;
	passed = TestRolloutCutoffBacksUpZero() && passed;
	passed = TestNodeLimitPreventsExpansion() && passed;
	passed = TestNodeLimitReusesExistingChildrenWhenExpansionIsBlocked() && passed;
	passed = TestTerminalAndActionlessRootsReturnNoSelection() && passed;
	passed = TestTemperatureZeroTieAndPositiveTemperatureSampling() && passed;
	passed = TestNeuralSearchUsesPuctPriorsForRootVisits() && passed;
	passed = TestNeuralSearchBacksUpLeafValueWithoutRollout() && passed;

	if (!passed) {
		return 1;
	}

	std::cout << "MCTS tests passed" << std::endl;
	return 0;
}
