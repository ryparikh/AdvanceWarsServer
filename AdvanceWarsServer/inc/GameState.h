#pragma once

#include <array>
#include <memory>

#include "Map.h"
#include "Player.h"
#include "Result.h"
#include "Reward.h"

class Map;

#include <optional>

struct Action {
	enum class Type {
		// TODO: Need action for "hide"/"unhide" unit
		Invalid = -1,
		Attack,
		EndTurn,
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
		case Type::EndTurn:
			return "end-turn";
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

	static Type fromTypeString(std::string strType);

	Action() {}
	// EndTurn
	Action(Type type) : m_type(type) {}
	// Used for Indirect Attacks, Capture, Load, Combine, Wait
	Action(Type type, int xSource, int ySource, int xTarget, int yTarget) : m_type(type), m_optSource({ xSource, ySource }), m_optTarget({ xTarget, yTarget }) {}
	// Used for Move Attack
	Action(Type type, int xSource, int ySource, Direction direction, int xTarget, int yTarget) : m_type(type), m_optSource({ xSource, ySource }), m_optTarget({ xTarget, yTarget }), m_optDirection(direction) {}
	// Used for Repair, Unload
	Action(Type type, int xSource, int ySource, Direction direction) : m_type(type), m_optSource({ xSource, ySource }), m_optDirection(direction) {}
	Action(Type type, int xSource, int ySource, Direction direction, int i) : m_type(type), m_optSource({ xSource, ySource }), m_optDirection(direction), m_optUnloadIndex(i) {}
	// Used for Buy Actions
	Action(Type type, int xSource, int ySource, UnitProperties::Type unitType) : m_type(type), m_optSource({ xSource, ySource }), m_optUnitType(unitType) {}

	bool operator==(const Action& other) const {
		if (m_type != other.m_type) {
			return false;
		}

		if (m_optDirection.has_value()) {
			if (!other.m_optDirection.has_value() ||
				(m_optDirection.value() != other.m_optDirection.value())) {
				return false;
			}
		}

		if (m_optSource.has_value()) {
			if (!other.m_optSource.has_value() ||
				(m_optSource.value() != other.m_optSource.value())) {
				return false;
			}
		}

		if (m_optTarget.has_value()) {
			if (!other.m_optTarget.has_value() ||
				(m_optTarget.value() != other.m_optTarget.value())) {
				return false;
			}
		}

		if (m_optUnitType.has_value()) {
			if (!other.m_optUnitType.has_value() ||
				(m_optUnitType.value() != other.m_optUnitType.value())) {
				return false;
			}
		}

		return true;
	}

	Type m_type{ Type::Invalid };

	std::optional<std::pair<int, int>> m_optSource;

	// Where are we moving or indirectly attacking
	std::optional<std::pair<int, int>> m_optTarget;

	// Where are we repairing, unloading, attacking
	std::optional<Direction> m_optDirection;

	// Which unit are we buying
	std::optional<UnitProperties::Type> m_optUnitType;

	std::optional<int> m_optUnloadIndex;
};


void to_json(json& j, const Action& action);
void from_json(json& j, Action& action);

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

#include <iostream>
class GameState {
public:
	GameState() noexcept;
	GameState(std::string guid, std::array<Player, 2>&& arrPlayers) noexcept;
	GameState(const GameState& other) noexcept;
	GameState& operator=(const GameState& other) noexcept;

	bool isTerminal() const {
		return m_fGameOver;
	}

	std::vector<Action> getLegalActions() const {
		std::vector<Action> vecActions;
		GetValidActions(vecActions);
		return vecActions;
	}

	GameState applyAction(const Action& action) {
		GameState actedGameState{Clone()};
		if (Result::Failed == actedGameState.DoAction(action)) 			{
			json jaction;
			::to_json(jaction, action);
			json jstate;
			to_json(jstate, *this);
			std::cout << "Chose an invalid action:" << jaction.dump() << "\n" << jstate.dump() << std::endl;
		}
		//actedGameState.CheckPlayerResigns();
		return actedGameState;
	}

	double evaluate(int playerPerspective) const {
		if (!m_fGameOver || m_winningPlayer == 2) {
			return 0;
		}

		if (m_winningPlayer == playerPerspective) {
			return 1;
		}

		return -1;
	}

	int getWinningPlayer() const {
		return m_winningPlayer;
	}

	int getEvaluationForCurrentPlayer() const {
		if (!m_fGameOver || m_winningPlayer == 2) {
			return 0;
		}

		int player = m_isFirstPlayerTurn ? 0 : 1;
		if (m_winningPlayer == player) {
			return 1;
		}
		else {
			return -1;
		}
	}

	int getCurrentPlayer() const {
		return m_isFirstPlayerTurn ? 0 : 1;
	}

	GameState Clone();
	Result InitializeGame() noexcept;
	Result DoAction(const Action& action) noexcept;
	Map* TryGetMap() const noexcept;
	const std::string& GetId() const noexcept;
	const std::array<Player, 2>& GetPlayers() const noexcept;
	std::array<Player, 2>& GetPlayers() noexcept { return m_arrPlayers; }
	bool IsFirstPlayerTurn() const noexcept;
	Result GetValidActions(std::vector<Action>& vecActions) const noexcept;
	Result GetValidActions(int x, int y, std::vector<Action>& vecActions) const noexcept;
	bool AnyValidActions() const noexcept;
	Result EndTurn() noexcept;
	bool CheckPlayerResigns() noexcept;
	bool FGameOver() const noexcept {
		return m_fGameOver;
	}

	bool FEnemyHasLabs() const noexcept;
	static void to_json(json& j, const GameState& gameState);
	static void from_json(json& j, GameState& gameState);
private:
	Result BeginTurn() noexcept;
	Result ResupplyApcUnits(int x, int y) noexcept;
	Player& GetCurrentPlayer() noexcept { return m_isFirstPlayerTurn ? m_arrPlayers[0] : m_arrPlayers[1]; }
	Player& GetEnemyPlayer() noexcept { return m_isFirstPlayerTurn ? m_arrPlayers[1] : m_arrPlayers[0]; }
	const Player& GetEnemyPlayer() const noexcept { return m_isFirstPlayerTurn ? m_arrPlayers[1] : m_arrPlayers[0]; }
	const Player& GetCurrentPlayer() const noexcept { return m_isFirstPlayerTurn ? m_arrPlayers[0] : m_arrPlayers[1]; }
	std::pair<int, int> movementRemainingAfterStep(int x, int y, MovementTypes movementType, int maxMovement, int maxFuel) const noexcept;
	Result AddIndirectAttackActions(int x, int y, const Unit& attacker, int minAttackRange, int maxAttackRange, std::vector<Action>& vecActions) const noexcept;
	bool CanUnitAttack(const Unit& attacker, const Unit& defender) const noexcept;
	bool FAttackUsesAmmo(const Unit& attacker, const Unit& defender) const noexcept;

	MoveNode* AddNewNodeToGraph(std::vector<Action>& vecActions, std::vector<std::unique_ptr<MoveNode>>& vecMoves, MoveNode** pMoveUpdate, int movement, int fuel, int x, int y) const noexcept;

	int GetFuelAfterMove(int xSrc, int ySrc, int xDest, int yDest);

	// Actions
	Result DoAttackAction(int x, int y, const Action& action);
	Result DoBuyAction(int x, int y, const Action& action);
	Result DoMoveAction(int& x, int& y, const Action& action);
	Result DoMoveCombineAction(int x, int y, const Action& action);
	Result DoMoveLoadAction(int x, int y, const Action& action);
	Result DoCaptureAction(int x, int y, const Action& action);
	Result DoUnloadAction(int x, int y, const Action& action);
	Result DoCOPowerAction();
	Result DoSCOPowerAction();
	Result ResupplyPlayersUnits(const Player* player);
	int calculateDamage(const Player* pattackingplayer, const Player* pdefendingplayer, const CommandingOfficier::Type& attackerCO, const CommandingOfficier::Type& defenderCO, const Unit& attacker, const Unit& defender, const Terrain& attackerTerrain, const Terrain& defenderTerrain);
	int GetCOTerrainModifier(const Player& player, const CommandingOfficier::Type& co, const Terrain::Type& terrainType) const noexcept;
	int GetCOIndirectRangeModifier(const Player& player, const CommandingOfficier::Type& co, const Unit& unit) const noexcept;

	int GetMaxGoodLuck(const Player& player) noexcept;
	int GetMaxBadLuck(const Player& player) noexcept;
	int GetCOMovementBonus(const CommandingOfficier::Type& co, const Unit& unit) const noexcept;
	void HealUnits(int health);
	bool FAtUnitCap() const noexcept;
	bool FPlayerRouted(const Player& player) const noexcept;
private:
	std::string m_guid;
	std::unique_ptr<Map> m_spmap;
	std::array<Player, 2> m_arrPlayers;
	int m_nUnitCap = 50;
	int m_nCaptureLimit = 21;
	int m_nTurnCount = 0;
	bool m_isFirstPlayerTurn{ true };
	bool m_fGameOver = false;
	int m_winningPlayer = -1;
};

