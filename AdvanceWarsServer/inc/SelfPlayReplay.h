#pragma once

#include <filesystem>
#include <string>

#include "GameState.h"
#include "Result.h"

struct SelfPlayError {
	std::string code;
	std::string message;
	int line{ 0 };
	int gameIndex{ -1 };
	int ply{ -1 };
};

struct SelfPlayReplayValidationSummary {
	int games{ 0 };
	int samples{ 0 };
	int actions{ 0 };
	int invalidActions{ 0 };
};

const char* SelfPlayReplayFormatVersion() noexcept;
json SelfPlayVersionsJson();
Result ValidateSelfPlayReplay(const std::filesystem::path& path, SelfPlayReplayValidationSummary& summary, SelfPlayError& error);
Result ValidateSelfPlayReplayForAppend(const std::filesystem::path& path, const json& expectedHeader, SelfPlayReplayValidationSummary& summary, SelfPlayError& error);
