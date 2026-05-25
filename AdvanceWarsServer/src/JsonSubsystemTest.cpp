#include "SubsystemTest.h"

#include <fstream>
#include <iostream>
#include <sstream>

#include "GameState.h"

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
