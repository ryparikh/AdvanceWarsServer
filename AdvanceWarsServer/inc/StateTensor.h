#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "GameState.h"
#include "Result.h"

class StateTensor final {
public:
	static const char* Version() noexcept;
	static int BoardWidth() noexcept;
	static int BoardHeight() noexcept;
	static int ChannelCount() noexcept;
	static const std::vector<std::string>& ChannelNames();
	static Result ChannelIndex(const std::string& name, int& index) noexcept;
	static Result Encode(const GameState& gameState, std::vector<float>& values) noexcept;
	static Result Checksum(const std::vector<float>& values, std::uint64_t& checksum) noexcept;
};
