#pragma once

#include <filesystem>

#include "GameState.h"

struct JsonSubsystemTest {
	GameState initialGameState;
	GameState finalGameState;
	std::vector<Action> vecActions;
	std::vector<Action> vecFailedActions;
	std::vector<std::pair<std::pair<int, int>, std::vector<Action>>> vecValidActions;
	bool fHasFinalGameState{ false };
	bool fDeterministicReplay{ false };
	json actionSpace;
	json legalActionMask;
	bool fHasActionSpace{ false };
	bool fHasLegalActionMask{ false };
	bool fLegalActionMaskShouldFail{ false };
};

void from_json(json& j, JsonSubsystemTest& test);

class JsonTestRunner {
public:
	JsonTestRunner(const std::filesystem::path& filePath);
	Result run();
	const wchar_t* GetFilepath() const;
	const std::string& GetActual() { return m_strActual; }
	const std::string& GetExpected() { return m_strExpected; }
private:
	std::filesystem::path m_filePath;
	std::string m_strActual;
	std::string m_strExpected;
};

class JsonTestSuite {
public:
	JsonTestSuite(const std::filesystem::path& path);
	void run();
private:
	std::filesystem::path m_filePath;
};
