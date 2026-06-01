#include "SubsystemTest.h"

#include <fstream>
#include <cmath>
#include <iostream>
#include <sstream>
#include <unordered_set>

#include "ActionSpace.h"
#include "GameState.h"
#include "StateTensor.h"

namespace {
void SetMismatch(std::string& expected, std::string& actual, const std::string& message, const std::string& expectedValue, const std::string& actualValue) {
	expected = message + ": " + expectedValue;
	actual = actualValue;
}

std::string StatToString(const std::pair<int, int>& stat) {
	std::ostringstream oss;
	oss << stat.first << "/" << stat.second;
	return oss.str();
}

Result RunCommandingOfficierContract(json& jContract, std::string& expected, std::string& actual) {
	std::string coName;
	jContract.at("name").get_to(coName);

	json coJson = coName;
	CommandingOfficier co;
	from_json(coJson, co);
	if (co.m_type == CommandingOfficier::Type::Invalid) {
		SetMismatch(expected, actual, "CO parser should recognize", coName, "Invalid");
		return Result::Failed;
	}

	json roundTrip;
	to_json(roundTrip, co);
	if (roundTrip != coName) {
		SetMismatch(expected, actual, "CO should round trip", coName, roundTrip.dump());
		return Result::Failed;
	}

	PowerMeter powerMeter(co.m_type);
	json actualPowerMeter;
	PowerMeter::to_json(actualPowerMeter, powerMeter);
	const json& expectedPowerMeter = jContract.at("power-meter");
	for (const auto& expectedField : expectedPowerMeter.items()) {
		const std::string& key = expectedField.key();
		if (!actualPowerMeter.contains(key) || actualPowerMeter.at(key) != expectedField.value()) {
			expected = expectedPowerMeter.dump();
			actual = actualPowerMeter.dump();
			return Result::Failed;
		}
	}

	const DamageCharts& charts = rgCharts[static_cast<int>(co.m_type)];
	for (auto& statContract : jContract.at("stats")) {
		std::string unitName;
		statContract.at("unit").get_to(unitName);
		UnitProperties::Type unitType = UnitProperties::unitTypeFromString(unitName);
		if (unitType == UnitProperties::Type::Invalid) {
			SetMismatch(expected, actual, "Unit parser should recognize", unitName, "Invalid");
			return Result::Failed;
		}

		const std::array<std::pair<const char*, int>, 3> powerStatuses{ {
			{ "normal", 0 },
			{ "cop", 1 },
			{ "scop", 2 },
		} };
		for (const auto& [statusName, statusIndex] : powerStatuses) {
			std::pair<int, int> expectedStat;
			statContract.at(statusName).at(0).get_to(expectedStat.first);
			statContract.at(statusName).at(1).get_to(expectedStat.second);
			std::pair<int, int> actualStat = charts[statusIndex][static_cast<int>(unitType)];
			if (actualStat != expectedStat) {
				SetMismatch(expected, actual, coName + " " + unitName + " " + statusName, StatToString(expectedStat), StatToString(actualStat));
				return Result::Failed;
			}
		}
	}

	return Result::Succeeded;
}

Result RunInvalidCommandingOfficierContract(json& jInvalidCo, std::string& expected, std::string& actual) {
	std::string coName;
	jInvalidCo.get_to(coName);

	json coJson = coName;
	CommandingOfficier co;
	try {
		from_json(coJson, co);
	}
	catch (const std::exception&) {
		return Result::Succeeded;
	}

	SetMismatch(expected, actual, "Unknown CO should throw", coName, co.to_string());
	return Result::Failed;
}

Result BuildActionTrace(GameState gameState, const std::vector<Action>& actions, std::string& traceDump) {
	json trace = json::array();
	json initialState;
	GameState::to_json(initialState, gameState);
	trace.push_back(std::move(initialState));

	for (const Action& action : actions) {
		IfFailedReturn(gameState.DoAction(action));

		json actedGameState;
		GameState::to_json(actedGameState, gameState);
		trace.push_back(std::move(actedGameState));
	}

	traceDump = trace.dump();
	return Result::Succeeded;
}

Result ExpectJsonValue(const json& actualJson, const json& expectedJson, const std::string& label, std::string& expected, std::string& actual) {
	expected = label + ": " + expectedJson.dump();
	actual = actualJson.dump();
	if (actualJson != expectedJson) {
		return Result::Failed;
	}

	return Result::Succeeded;
}

Result RunActionSpaceAssertions(const json& actionSpace, std::string& expected, std::string& actual) {
	if (actionSpace.contains("version")) {
		json actualVersion = ActionSpace::Version();
		IfFailedReturn(ExpectJsonValue(actualVersion, actionSpace.at("version"), "action-space version", expected, actual));
	}

	if (actionSpace.contains("width")) {
		json actualWidth = ActionSpace::BoardWidth();
		IfFailedReturn(ExpectJsonValue(actualWidth, actionSpace.at("width"), "action-space width", expected, actual));
	}

	if (actionSpace.contains("height")) {
		json actualHeight = ActionSpace::BoardHeight();
		IfFailedReturn(ExpectJsonValue(actualHeight, actionSpace.at("height"), "action-space height", expected, actual));
	}

	if (actionSpace.contains("action-count")) {
		json actualActionCount = ActionSpace::ActionCount();
		IfFailedReturn(ExpectJsonValue(actualActionCount, actionSpace.at("action-count"), "action-space action-count", expected, actual));
	}

	if (actionSpace.contains("encodedActions")) {
		for (const json& encodedActionJson : actionSpace.at("encodedActions")) {
			Action action;
			json actionJson = encodedActionJson.at("action");
			from_json(actionJson, action);

			int encodedAction = -1;
			if (ActionSpace::EncodeAction(action, encodedAction) == Result::Failed) {
				json jAction;
				to_json(jAction, action);
				SetMismatch(expected, actual, "action should encode", jAction.dump(), "encode failed");
				return Result::Failed;
			}

			if (encodedActionJson.contains("index") && encodedAction != encodedActionJson.at("index").get<int>()) {
				json jAction;
				to_json(jAction, action);
				SetMismatch(expected, actual, "encoded index for " + jAction.dump(), encodedActionJson.at("index").dump(), std::to_string(encodedAction));
				return Result::Failed;
			}

			Action decodedAction;
			if (ActionSpace::DecodeAction(encodedAction, decodedAction) == Result::Failed) {
				SetMismatch(expected, actual, "encoded action should decode", std::to_string(encodedAction), "decode failed");
				return Result::Failed;
			}

			if (!(decodedAction == action)) {
				json jExpected;
				json jActual;
				to_json(jExpected, action);
				to_json(jActual, decodedAction);
				expected = jExpected.dump();
				actual = jActual.dump();
				return Result::Failed;
			}
		}
	}

	if (actionSpace.contains("invalidEncodedActions")) {
		for (const json& actionJsonSource : actionSpace.at("invalidEncodedActions")) {
			Action action;
			json actionJson = actionJsonSource;
			from_json(actionJson, action);

			int encodedAction = -1;
			if (ActionSpace::EncodeAction(action, encodedAction) == Result::Succeeded) {
				json jAction;
				to_json(jAction, action);
				SetMismatch(expected, actual, "action should fail to encode", jAction.dump(), std::to_string(encodedAction));
				return Result::Failed;
			}
		}
	}

	if (actionSpace.contains("decodedActions")) {
		for (const json& decodedActionJson : actionSpace.at("decodedActions")) {
			Action actualAction;
			int encodedAction = decodedActionJson.at("index").get<int>();
			if (ActionSpace::DecodeAction(encodedAction, actualAction) == Result::Failed) {
				SetMismatch(expected, actual, "index should decode", std::to_string(encodedAction), "decode failed");
				return Result::Failed;
			}

			Action expectedAction;
			json expectedActionJson = decodedActionJson.at("action");
			from_json(expectedActionJson, expectedAction);
			if (!(actualAction == expectedAction)) {
				json jExpected;
				json jActual;
				to_json(jExpected, expectedAction);
				to_json(jActual, actualAction);
				expected = jExpected.dump();
				actual = jActual.dump();
				return Result::Failed;
			}
		}
	}

	if (actionSpace.contains("invalidIndices")) {
		for (const json& indexJson : actionSpace.at("invalidIndices")) {
			Action action;
			int encodedAction = indexJson.get<int>();
			if (ActionSpace::DecodeAction(encodedAction, action) == Result::Succeeded) {
				json jAction;
				to_json(jAction, action);
				SetMismatch(expected, actual, "index should fail to decode", std::to_string(encodedAction), jAction.dump());
				return Result::Failed;
			}
		}
	}

	return Result::Succeeded;
}

Result RunLegalActionMaskAssertions(const GameState& gameState, const json& legalActionMask, bool shouldFail, std::string& expected, std::string& actual) {
	std::vector<std::uint8_t> mask;
	Result result = ActionSpace::GenerateLegalActionMask(gameState, mask);
	if (shouldFail) {
		if (result == Result::Succeeded) {
			SetMismatch(expected, actual, "legal action mask generation should fail", "failed", "succeeded");
			return Result::Failed;
		}

		return Result::Succeeded;
	}

	if (result == Result::Failed) {
		SetMismatch(expected, actual, "legal action mask generation should succeed", "succeeded", "failed");
		return Result::Failed;
	}

	if (mask.size() != static_cast<std::size_t>(ActionSpace::ActionCount())) {
		SetMismatch(expected, actual, "mask size", std::to_string(ActionSpace::ActionCount()), std::to_string(mask.size()));
		return Result::Failed;
	}

	std::vector<Action> legalActions;
	IfFailedReturn(gameState.GetValidActions(legalActions));

	std::unordered_set<int> uniqueLegalActionIndices;
	for (const Action& legalAction : legalActions) {
		int encodedAction = -1;
		IfFailedReturn(ActionSpace::EncodeAction(legalAction, encodedAction));
		uniqueLegalActionIndices.insert(encodedAction);
	}

	std::size_t actualMaskCount = 0;
	for (std::uint8_t maskValue : mask) {
		if (maskValue != 0U) {
			++actualMaskCount;
		}
	}

	if (actualMaskCount != uniqueLegalActionIndices.size()) {
		SetMismatch(expected, actual, "mask legal action count", std::to_string(uniqueLegalActionIndices.size()), std::to_string(actualMaskCount));
		return Result::Failed;
	}

	if (legalActionMask.contains("expectedLegalActions")) {
		for (const json& actionJsonSource : legalActionMask.at("expectedLegalActions")) {
			Action action;
			json actionJson = actionJsonSource;
			from_json(actionJson, action);
			int encodedAction = -1;
			IfFailedReturn(ActionSpace::EncodeAction(action, encodedAction));
			if (mask[encodedAction] == 0U) {
				json jAction;
				to_json(jAction, action);
				SetMismatch(expected, actual, "expected legal action mask bit", jAction.dump(), "0");
				return Result::Failed;
			}
		}
	}

	if (legalActionMask.contains("expectedIllegalActions")) {
		for (const json& actionJsonSource : legalActionMask.at("expectedIllegalActions")) {
			Action action;
			json actionJson = actionJsonSource;
			from_json(actionJson, action);
			int encodedAction = -1;
			IfFailedReturn(ActionSpace::EncodeAction(action, encodedAction));
			if (mask[encodedAction] != 0U) {
				json jAction;
				to_json(jAction, action);
				SetMismatch(expected, actual, "expected illegal action mask bit", jAction.dump(), "1");
				return Result::Failed;
			}
		}
	}

	return Result::Succeeded;
}

Result RunStateTensorAssertions(const GameState& gameState, const json& stateTensor, bool shouldFail, std::string& expected, std::string& actual) {
	std::vector<float> values;
	Result result = StateTensor::Encode(gameState, values);
	if (shouldFail) {
		if (result == Result::Succeeded) {
			SetMismatch(expected, actual, "state tensor encoding should fail", "failed", "succeeded");
			return Result::Failed;
		}

		return Result::Succeeded;
	}

	if (result == Result::Failed) {
		SetMismatch(expected, actual, "state tensor encoding should succeed", "succeeded", "failed");
		return Result::Failed;
	}

	if (stateTensor.contains("version")) {
		json actualVersion = StateTensor::Version();
		IfFailedReturn(ExpectJsonValue(actualVersion, stateTensor.at("version"), "state tensor version", expected, actual));
	}

	if (stateTensor.contains("width")) {
		json actualWidth = StateTensor::BoardWidth();
		IfFailedReturn(ExpectJsonValue(actualWidth, stateTensor.at("width"), "state tensor width", expected, actual));
	}

	if (stateTensor.contains("height")) {
		json actualHeight = StateTensor::BoardHeight();
		IfFailedReturn(ExpectJsonValue(actualHeight, stateTensor.at("height"), "state tensor height", expected, actual));
	}

	if (stateTensor.contains("channels")) {
		json actualChannels = StateTensor::ChannelCount();
		IfFailedReturn(ExpectJsonValue(actualChannels, stateTensor.at("channels"), "state tensor channels", expected, actual));
	}

	const std::size_t expectedValueCount = static_cast<std::size_t>(StateTensor::ChannelCount() * StateTensor::BoardWidth() * StateTensor::BoardHeight());
	if (values.size() != expectedValueCount) {
		SetMismatch(expected, actual, "state tensor value count", std::to_string(expectedValueCount), std::to_string(values.size()));
		return Result::Failed;
	}

	if (stateTensor.contains("channelIndices")) {
		for (const json& channelIndexJson : stateTensor.at("channelIndices")) {
			const std::string channel = channelIndexJson.at("channel").get<std::string>();
			int channelIndex = -1;
			if (StateTensor::ChannelIndex(channel, channelIndex) == Result::Failed) {
				SetMismatch(expected, actual, "state tensor channel should exist", channel, "missing");
				return Result::Failed;
			}

			const int expectedIndex = channelIndexJson.at("index").get<int>();
			if (channelIndex != expectedIndex) {
				SetMismatch(expected, actual, "state tensor channel index for " + channel, std::to_string(expectedIndex), std::to_string(channelIndex));
				return Result::Failed;
			}
		}
	}

	const double tolerance = stateTensor.value("tolerance", 0.000001);
	if (stateTensor.contains("values")) {
		for (const json& valueJson : stateTensor.at("values")) {
			const std::string channel = valueJson.at("channel").get<std::string>();
			const int x = valueJson.at("at").at(0).get<int>();
			const int y = valueJson.at("at").at(1).get<int>();
			const double expectedValue = valueJson.at("value").get<double>();

			int channelIndex = -1;
			if (StateTensor::ChannelIndex(channel, channelIndex) == Result::Failed) {
				SetMismatch(expected, actual, "state tensor channel should exist", channel, "missing");
				return Result::Failed;
			}

			const int valueIndex = channelIndex * StateTensor::BoardWidth() * StateTensor::BoardHeight() + y * StateTensor::BoardWidth() + x;
			if (valueIndex < 0 || valueIndex >= static_cast<int>(values.size())) {
				SetMismatch(expected, actual, "state tensor coordinate should be in bounds", channel + " at " + std::to_string(x) + "," + std::to_string(y), "out of bounds");
				return Result::Failed;
			}

			const double actualValue = values[valueIndex];
			if (std::abs(actualValue - expectedValue) > tolerance) {
				SetMismatch(expected, actual, "state tensor value for " + channel + " at " + std::to_string(x) + "," + std::to_string(y), std::to_string(expectedValue), std::to_string(actualValue));
				return Result::Failed;
			}
		}
	}

	return Result::Succeeded;
}
}

void from_json(json& j, JsonSubsystemTest& test) {
	GameState::from_json(j.at("initial-game-state"), test.initialGameState);
	if (j.contains("final-game-state")) {
		GameState::from_json(j.at("final-game-state"), test.finalGameState);
		test.fHasFinalGameState = true;
	}

	if (j.contains("actions")) {
		for (auto& jAction : j.at("actions")) {
			Action action;
			from_json(jAction, action);
			test.vecActions.emplace_back(action);
		}
	}

	if (j.contains("failedActions")) {
		for (auto& jAction : j.at("failedActions")) {
			Action action;
			from_json(jAction, action);
			test.vecFailedActions.emplace_back(action);
		}
	}

	if (j.contains("validActions")) {
		for (auto& jValidActions : j.at("validActions")) {
			std::pair<int, int> source;
			jValidActions.at("source").get_to(source);

			std::vector<Action> actions;
			for (auto& jAction : jValidActions.at("expected")) {
				Action action;
				from_json(jAction, action);
				actions.emplace_back(action);
			}

			test.vecValidActions.emplace_back(std::move(source), std::move(actions));
		}
	}

	if (j.contains("deterministic-replay")) {
		j.at("deterministic-replay").get_to(test.fDeterministicReplay);
	}

	if (j.contains("actionSpace")) {
		test.actionSpace = j.at("actionSpace");
		test.fHasActionSpace = true;
	}

	if (j.contains("legalActionMask")) {
		test.legalActionMask = j.at("legalActionMask");
		test.fHasLegalActionMask = true;
	}

	if (j.contains("legalActionMaskShouldFail")) {
		j.at("legalActionMaskShouldFail").get_to(test.fLegalActionMaskShouldFail);
		test.fHasLegalActionMask = true;
	}

	if (j.contains("stateTensor")) {
		test.stateTensor = j.at("stateTensor");
		test.fHasStateTensor = true;
	}

	if (j.contains("stateTensorShouldFail")) {
		j.at("stateTensorShouldFail").get_to(test.fStateTensorShouldFail);
		test.fHasStateTensor = true;
	}
}

JsonTestRunner::JsonTestRunner(const std::filesystem::path& filePath) :
	m_filePath(filePath)
{
}

const wchar_t* JsonTestRunner::GetFilepath() const {
	return m_filePath.c_str();
}

Result JsonTestRunner::run() {
	std::fstream filestream(m_filePath, std::ios::in);
	if (filestream.fail() || filestream.eof())
	{
		return Result::Failed;
	}

	json j;
	filestream >> j;

	if (j.contains("co-contract")) {
		return RunCommandingOfficierContract(j.at("co-contract"), m_strExpected, m_strActual);
	}

	if (j.contains("invalid-co")) {
		return RunInvalidCommandingOfficierContract(j.at("invalid-co"), m_strExpected, m_strActual);
	}

	JsonSubsystemTest test;
	from_json(j, test);

	if (!test.vecActions.empty() && test.fHasFinalGameState) {
		for (const Action& action : test.vecActions) {
			IfFailedReturn(test.initialGameState.DoAction(action));
		}

		json actedGameState;
		GameState::to_json(actedGameState, test.initialGameState);
		json expectedGameState;
		GameState::to_json(expectedGameState, test.finalGameState);
		m_strActual = actedGameState.dump();
		m_strExpected = expectedGameState.dump();

		if (m_strActual != m_strExpected) {
			return Result::Failed;
		}
	}

	if (test.fDeterministicReplay) {
		if (test.vecActions.empty()) {
			return Result::Failed;
		}

		std::string firstTrace;
		std::string secondTrace;
		IfFailedReturn(BuildActionTrace(test.initialGameState, test.vecActions, firstTrace));
		IfFailedReturn(BuildActionTrace(test.initialGameState, test.vecActions, secondTrace));

		m_strExpected = firstTrace;
		m_strActual = secondTrace;
		if (firstTrace != secondTrace) {
			return Result::Failed;
		}
	}

	if (!test.vecFailedActions.empty()) {
		for (const Action& action : test.vecFailedActions) {
			json beforeState;
			GameState::to_json(beforeState, test.initialGameState);
			if (test.initialGameState.DoAction(action) == Result::Succeeded) {
				return Result::Failed;
			}
			json afterState;
			GameState::to_json(afterState, test.initialGameState);
			m_strExpected = beforeState.dump();
			m_strActual = afterState.dump();
			if (m_strActual != m_strExpected) {
				return Result::Failed;
			}
		}
	}

	if (!test.vecValidActions.empty()) {
		for (const auto& validActions : test.vecValidActions) {
			std::vector<Action> actualActions;
			IfFailedReturn(test.initialGameState.GetValidActions(validActions.first.first, validActions.first.second, actualActions));

			json expectedActions = json::array();
			for (const Action& action : validActions.second) {
				json jAction;
				to_json(jAction, action);
				expectedActions.push_back(std::move(jAction));
			}

			json actualActionsJson = json::array();
			for (const Action& action : actualActions) {
				json jAction;
				to_json(jAction, action);
				actualActionsJson.push_back(std::move(jAction));
			}

			m_strExpected = expectedActions.dump();
			m_strActual = actualActionsJson.dump();
			if (m_strActual != m_strExpected) {
				return Result::Failed;
			}
		}
	}

	if (test.fHasActionSpace) {
		IfFailedReturn(RunActionSpaceAssertions(test.actionSpace, m_strExpected, m_strActual));
	}

	if (test.fHasLegalActionMask) {
		IfFailedReturn(RunLegalActionMaskAssertions(test.initialGameState, test.legalActionMask, test.fLegalActionMaskShouldFail, m_strExpected, m_strActual));
	}

	if (test.fHasStateTensor) {
		IfFailedReturn(RunStateTensorAssertions(test.initialGameState, test.stateTensor, test.fStateTensorShouldFail, m_strExpected, m_strActual));
	}

	return Result::Succeeded;
}

JsonTestSuite::JsonTestSuite(const std::filesystem::path& filePath) :
	m_filePath(filePath) {
}

void JsonTestSuite::run() {
	int failures = 0;
	for (const std::filesystem::directory_entry& dir_entry : std::filesystem::recursive_directory_iterator(m_filePath)) {
		if (!dir_entry.is_regular_file()) {
			continue;
		}
		if (dir_entry.path().extension() != ".json") {
			continue;
		}

		JsonTestRunner runner(dir_entry);
		Result result = runner.run();
		if (result == Result::Failed) {
			++failures;
			std::wcout << "Failure: " << runner.GetFilepath() << "\n" << std::endl;
			std::cout << "Expected output:\n" << runner.GetExpected() << "\n\n" << "Actual output:\n" << runner.GetActual() << "\n" << std::endl;;
		}
	}

	if (failures == 0) {
		std::cout << "Tests Passed" << std::endl;
	}
}
