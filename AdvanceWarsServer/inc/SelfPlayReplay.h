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
const char* SelfPlayReplayTraceFormatVersion() noexcept;
json SelfPlayVersionsJson();
Result ValidateSelfPlayReplay(const std::filesystem::path& path, SelfPlayReplayValidationSummary& summary, SelfPlayError& error);
Result ValidateSelfPlayReplayForAppend(const std::filesystem::path& path, const json& expectedHeader, SelfPlayReplayValidationSummary& summary, SelfPlayError& error);
Result ExportSelfPlayReplayTrace(const std::filesystem::path& replayPath, int gameIndex, json& trace, SelfPlayError& error);
Result ExportSelfPlayReplayTraceFile(const std::filesystem::path& replayPath, const std::filesystem::path& outputPath, int gameIndex, SelfPlayError& error);
int RunExportReplayTraceCommand(int argc, char* argv[]) noexcept;
