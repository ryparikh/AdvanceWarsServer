#include "SubsystemTest.h"

#include <fstream>
#include <iostream>

#include "GameState.h"

void from_json(json& j, JsonSubsystemTest& test) {
	GameState::from_json(j.at("initial-game-state"), test.initialGameState);
	GameState::from_json(j.at("final-game-state"), test.finalGameState);
	for (auto& jAction : j.at("actions")) {
		Action action;
		from_json(jAction, action);
		test.vecActions.emplace_back(action);
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
	JsonSubsystemTest test;
	from_json(j, test);

	for (const Action& action : test.vecActions) {
		test.initialGameState.DoAction(action);
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
