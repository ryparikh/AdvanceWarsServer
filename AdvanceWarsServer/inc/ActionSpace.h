#pragma once

#include <cstdint>
#include <vector>

#include "GameState.h"
#include "Result.h"

class ActionSpace final {
public:
	static const char* Version() noexcept;
	static int BoardWidth() noexcept;
	static int BoardHeight() noexcept;
	static int ActionCount() noexcept;

	static Result EncodeAction(const Action& action, int& encodedAction) noexcept;
	static Result DecodeAction(int encodedAction, Action& action) noexcept;
	static Result GenerateLegalActionMask(const GameState& gameState, std::vector<std::uint8_t>& mask) noexcept;
};
