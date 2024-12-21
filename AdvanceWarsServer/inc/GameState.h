#pragma once

#include <array>
#include <memory>

#include "Map.h"
#include "Player.h"
#include "Result.h"

class Map;

#include <optional>

struct Action {
	enum class Type {
		Invalid = -1,
		Attack,
		MoveWait,
		MoveAttack, // Requires direction
		Unload, // Requires direction
		MoveLoad,
		MoveCapture,
		MoveCombine,
		Repair, // Requires direction
		Buy, // size 25
		COPower,
		SCOPower,
		Size
	};

	enum Direction : int {
		Invalid = -1,
		North,
		East,
		South,
		West,
		Size
	};

	std::string getDirectionString() const {
		if (!m_optDirection.has_value()) {
			return "";
		}

		switch (*m_optDirection) {
		case Direction::North:
			return "north";
		case Direction::East:
			return "east";
		case Direction::South:
			return "south";
		case Direction::West:
			return "west";
		default:
			return "";
		}
	}

	std::string getTypeString() const {
		switch (m_type) {
		case Type::Attack:
			return "attack";
		case Type::Buy:
			return "buy";
		case Type::COPower:
			return "co-power";
		case Type::MoveAttack:
			return "move-attack";
		case Type::MoveCapture:
			return "move-capture";
		case Type::MoveCombine:
			return "move-combine";
		case Type::MoveLoad:
			return "move-load";
		case Type::Repair:
			return "repair";
		case Type::Unload:
			return "unload";
		case Type::MoveWait: 
			return "move-wait";
		case Type::SCOPower:
			return "super-co-power";
		default:
			return "";
		}
	}

	Action() {}
	// Used for Indirect Attacks, Capture, Load, Combine, Wait
	Action(Type type, int x, int y) : m_type(type), m_optTarget({ x, y }) {}
	// Used for Move Attack
	Action(Type type, Direction direction, int x, int y) : m_type(type), m_optTarget({ x, y }), m_optDirection(direction) {}
	// Used for Repair, Unload
	Action(Type type, Direction direction) : m_type(type), m_optDirection(direction) {}
	// Used for Buy Actions
	Action(Type type, UnitProperties::Type unitType) : m_type(type), m_optUnitType(unitType) {}
	Type m_type{ Type::Invalid };

	// Where are we moving or indirectly attacking
	std::optional<std::pair<int, int>> m_optTarget;

	// Where are we repairing, unloading, attacking
	std::optional<Direction> m_optDirection;

	// Which unit are we buying
	std::optional<UnitProperties::Type> m_optUnitType;
};

class MoveNode {
public:
	MoveNode(bool visited) : m_visited(visited) {}
	MoveNode(int movement, int fuel, int x, int y) : m_movement(movement), m_fuel(fuel), m_x(x), m_y(y) {}
	MoveNode* m_pnorth{ nullptr };
	MoveNode* m_peast{ nullptr };
	MoveNode* m_psouth{ nullptr };
	MoveNode* m_pwest{ nullptr };
	int m_movement;
	int m_fuel;
	int m_x;
	int m_y;
	bool m_visited = false;
};

class GameState {
public:
	GameState(std::string guid, std::array<Player, 2>&& arrPlayers) noexcept;

	Result InitializeGame() noexcept;
	Result DoAction(int x, int y, const Action& action) noexcept;
	Map* TryGetMap() const noexcept;
	const std::string& GetId() const noexcept;
	const std::array<Player, 2>& GetPlayers() const noexcept;
	bool IsFirstPlayerTurn() const noexcept;
	Result GetValidActions(int x, int y, std::vector<Action>& vecActions) const noexcept;

private:
	Result BeginTurn() noexcept;
	Player& GetCurrentPlayer() noexcept { return m_isFirstPlayerTurn ? m_arrPlayers[0] : m_arrPlayers[1]; }
	const Player& GetCurrentPlayer() const noexcept { return m_isFirstPlayerTurn ? m_arrPlayers[0] : m_arrPlayers[1]; }
	std::pair<int, int> movementRemainingAfterStep(int x, int y, MovementTypes movementType, int maxMovement, int maxFuel) const noexcept;
	Result AddIndirectAttackActions(int x, int y, const Unit& attacker, int minAttackRange, int maxAttackRange, std::vector<Action>& vecActions) const noexcept;
	bool CanUnitAttack(const Unit& attacker, const Unit& defender) const noexcept;

	MoveNode* AddNewNodeToGraph(std::vector<Action>& vecActions, std::vector<std::unique_ptr<MoveNode>>& vecMoves, MoveNode** pMoveUpdate, int movement, int fuel, int x, int y) const noexcept;

	// Actions
	Result DoAttackAction(int x, int y, const Action& action);
	Result DoBuyAction(int x, int y, const Action& action);
	Result DoMoveAction(int x, int y, const Action& action);
	Result DoCOPowerAction(const Action& action);
	int calculateDamage(const Unit& attacker, const Unit& defender, int defenderTerrainStars);
private:
	std::string m_guid;
	std::unique_ptr<Map> m_spmap;
	std::array<Player, 2> m_arrPlayers;
	bool m_isFirstPlayerTurn{ true };
};

void to_json(json& j, const GameState& gameState);
void to_json(json& j, const Action& action);
void from_json(json& j, Action& action);
