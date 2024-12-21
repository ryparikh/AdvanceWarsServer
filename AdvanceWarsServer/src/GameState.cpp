#include "GameState.h"
#include "UnitInfo.h"

#include <chrono>
#include <iostream>
#include <queue>

#include "MapParser.h"
const std::string mapChoiceFilePath = R"(.\res\AWBW\MapSources\Lefty.txt)";

using time_point = std::chrono::time_point<std::chrono::steady_clock>;
GameState::GameState(std::string guid, std::array<Player, 2>&& arrPlayers) noexcept :
	m_guid(guid),
	m_arrPlayers(std::move(arrPlayers)) {
}

Result GameState::InitializeGame() noexcept {
	MapParser parser;
	time_point fileStartTime = std::chrono::steady_clock::now();
	IfFailedReturn(parser.TryCreateFromFile(std::filesystem::path(mapChoiceFilePath), m_arrPlayers, m_spmap));

	time_point fileEndTime = std::chrono::steady_clock::now();
	std::clog << "File load time: " << std::chrono::duration<double, std::milli>(fileEndTime - fileStartTime).count() << "\n";

	// Headquarters
	m_spmap->Capture(7, 13, &GetPlayers()[0]);
	m_spmap->Capture(6, 5, &GetPlayers()[1]);
	// Bases
	m_spmap->Capture(11, 10, &GetPlayers()[0]);
	m_spmap->Capture(12, 15, &GetPlayers()[0]);
	m_spmap->Capture(5, 0, &GetPlayers()[1]);
	m_spmap->Capture(10, 2, &GetPlayers()[1]);
	m_spmap->TryAddUnit(7, 13, UnitProperties::Type::Infantry, &GetPlayers()[0]);
	m_spmap->TryAddUnit(6, 13, UnitProperties::Type::Infantry, &GetPlayers()[1]);
	m_spmap->TryAddUnit(1, 2, UnitProperties::Type::Artillery, &GetPlayers()[0]);
	m_spmap->TryAddUnit(1, 4, UnitProperties::Type::Infantry, &GetPlayers()[0]);
	m_spmap->TryAddUnit(1, 5, UnitProperties::Type::Infantry, &GetPlayers()[1]);

	BeginTurn();
	return Result::Succeeded;
}

Result GameState::BeginTurn() noexcept {
	// Add Funds
	int player = IsFirstPlayerTurn() ? 0 : 1;
	int newFunds = 0;
	int repairFunds = 0;
	for (int x = 0; x < m_spmap->GetCols(); ++x) {
		for (int y = 0; y < m_spmap->GetRows(); ++y) {
			const MapTile* pTile = nullptr;
			m_spmap->TryGetTile(x, y, &pTile);
			const Terrain& terrain = pTile->GetTerrain();
			if (MapTile::IsProperty(terrain.m_type) && (terrain.m_type != Terrain::Type::ComTower || terrain.m_type != Terrain::Type::Lab) && pTile->m_spPropertyInfo->m_owner == &m_arrPlayers[player]) {
				newFunds += 1000;
			}
		}
	}

	m_arrPlayers[player].m_funds += newFunds;
	for (int x = 0; x < m_spmap->GetCols(); ++x) {
		for (int y = 0; y < m_spmap->GetRows(); ++y) {
			MapTile* pTile = nullptr;
			m_spmap->TryGetTile(x, y, &pTile);
			const Terrain& terrain = pTile->GetTerrain();
			const Unit* pUnit = pTile->m_spUnit.get();
			if (pUnit != nullptr && MapTile::IsProperty(terrain.m_type) && (terrain.m_type != Terrain::Type::ComTower || terrain.m_type != Terrain::Type::Lab) && pUnit->m_owner == &m_arrPlayers[player]) {
				Unit* pUnit = pTile->TryGetUnit();
				if (pUnit != nullptr && pUnit->health < 10) {
					int repairAmount = std::min(10 - pUnit->health, 2);
					repairFunds += Unit::GetUnitCost(pUnit->m_properties.m_type) * repairAmount;
					if (repairFunds <= m_arrPlayers[player].m_funds) {
						m_arrPlayers[player].m_funds -= repairFunds;
						pUnit->health += repairAmount;
					}
				}
			}
		}
	}
	return Result::Succeeded;
}

std::pair<int, int> GameState::movementRemainingAfterStep(int x, int y, MovementTypes movementType, int maxMovement, int maxFuel) const noexcept {
	const MapTile* pSearchTile = nullptr;
	Result result = m_spmap->TryGetTile(x, y, &pSearchTile);
	if (result == Result::Failed) {
		return { -1, -1 };
	}
	const Unit* punit = pSearchTile->TryGetUnit();
	if (punit != nullptr && punit->m_owner != &GetCurrentPlayer()) {
		return { -1, -1 };
	}

	int terrainCost = pSearchTile->GetTerrain().m_movementCostMap.find(movementType)->second;
	if (terrainCost < 1) {
		return { -1, -1 };
	}

	int fuelRemaining = maxFuel - terrainCost;

	int movementRemaining = maxMovement - terrainCost;
	return { movementRemaining, fuelRemaining };
}

MoveNode* GameState::AddNewNodeToGraph(std::vector<Action>& vecActions, std::vector<std::unique_ptr<MoveNode>>& vecMoves, MoveNode** pMoveUpdate, int movement, int fuel, int x, int y) const noexcept {
	auto find = [&](int x, int y) -> MoveNode* {
		for (auto& node : vecMoves) {
			if (node->m_x == x && node->m_y == y)
				return node.get();
		}

		return nullptr;
	};

	MoveNode* pNode = find(x, y);
	if (pNode == nullptr) {
		vecMoves.emplace_back(new MoveNode(movement, fuel, x, y));
		pNode = vecMoves.back().get();
	}
	else if (pNode->m_movement < movement) {
		pNode->m_movement = movement;
		pNode->m_fuel = fuel;
	}

	pNode->m_pnorth = find(x, y - 1);
	if (pNode->m_pnorth != nullptr) {
		pNode->m_pnorth->m_psouth = pNode;
	}

	pNode->m_peast = find(x + 1, y);
	if (pNode->m_peast != nullptr) {
		pNode->m_peast->m_pwest = pNode;
	}

	pNode->m_psouth = find(x, y + 1);
	if (pNode->m_psouth != nullptr) {
		pNode->m_psouth->m_pnorth = pNode;
	}

	pNode->m_pwest = find(x - 1, y);
	if (pNode->m_pwest != nullptr) {
		pNode->m_pwest->m_peast = pNode;
	}

	return pNode;
}

bool GameState::CanUnitAttack(const Unit& attacker, const Unit& defender) const noexcept {
	if (attacker.m_properties.m_primaryWeapon == UnitProperties::Weapon::MachineGun) {
		return vrgPrimaryWeaponDamage[static_cast<int>(attacker.m_properties.m_type)][static_cast<int>(defender.m_properties.m_type)] > -1;
	}
	else {
		if (attacker.m_properties.m_ammo > 0) {
			return vrgPrimaryWeaponDamage[static_cast<int>(attacker.m_properties.m_type)][static_cast<int>(defender.m_properties.m_type)] > -1;
		} else if (attacker.m_properties.m_secondaryWeapon != UnitProperties::Weapon::Invalid) {
			return vrgSecondaryWeaponDamage[static_cast<int>(attacker.m_properties.m_type)][static_cast<int>(defender.m_properties.m_type)] > -1;
		}
	}

	return false;
}

Result GameState::GetValidActions(int x, int y, std::vector<Action>& vecActions) const noexcept {
	const MapTile* pmaptile;
	m_spmap->TryGetTile(x, y, &pmaptile);
	const Unit* pUnit = pmaptile->TryGetUnit();
	const Terrain& terrain = pmaptile->GetTerrain();

	// If current player unit, moves
	if (pUnit != nullptr) {
		int cLoadedUnits = pUnit->CLoadedUnits();
		if (pUnit->IsLander() && cLoadedUnits > 0) {
			for (int i = 0; i < cLoadedUnits; ++i) {
				const Unit* pUnloadUnit = pUnit->GetLoadedUnit(i);
				const MapTile* pUnloadDestination = nullptr;
				// north
				Result result = m_spmap->TryGetTile(x, y - 1, &pUnloadDestination);
				if (result == Result::Succeeded) {
					if (pUnloadDestination->m_spUnit == nullptr && pUnloadDestination->GetTerrain().m_movementCostMap.find(pUnloadUnit->m_properties.m_movementType)->second > 0) {
						vecActions.emplace_back(Action::Type::Unload, Action::Direction::North, i);
					}

					if (pUnit->m_properties.m_type == UnitProperties::Type::BlackBoat && pUnloadDestination->m_spUnit != nullptr && pUnloadDestination->m_spUnit->m_owner == &GetCurrentPlayer()) {
						vecActions.emplace_back(Action::Type::Repair, Action::Direction::North);
					}
				}
				// east
				result = m_spmap->TryGetTile(x + 1, y, &pUnloadDestination);
				if (result == Result::Succeeded) {
					if (pUnloadDestination->m_spUnit == nullptr && pUnloadDestination->GetTerrain().m_movementCostMap.find(pUnloadUnit->m_properties.m_movementType)->second > 0) {
						vecActions.emplace_back(Action::Type::Unload, Action::Direction::East, i);
					}

					if (pUnit->m_properties.m_type == UnitProperties::Type::BlackBoat && pUnloadDestination->m_spUnit != nullptr && pUnloadDestination->m_spUnit->m_owner == &GetCurrentPlayer()) {
						vecActions.emplace_back(Action::Type::Repair, Action::Direction::East);
					}
				}
				// south 
				result = m_spmap->TryGetTile(x, y + 1, &pUnloadDestination);
				if (result == Result::Succeeded) {
					if (pUnloadDestination->m_spUnit == nullptr && pUnloadDestination->GetTerrain().m_movementCostMap.find(pUnloadUnit->m_properties.m_movementType)->second > 0) {
						vecActions.emplace_back(Action::Type::Unload, Action::Direction::South, i);
					}

					if (pUnit->m_properties.m_type == UnitProperties::Type::BlackBoat && pUnloadDestination->m_spUnit != nullptr && pUnloadDestination->m_spUnit->m_owner == &GetCurrentPlayer()) {
						vecActions.emplace_back(Action::Type::Repair, Action::Direction::South);
					}
				}
				// west 
				result = m_spmap->TryGetTile(x - 1, y, &pUnloadDestination);
				if (result == Result::Succeeded) {
					if (pUnloadDestination->m_spUnit == nullptr && pUnloadDestination->GetTerrain().m_movementCostMap.find(pUnloadUnit->m_properties.m_movementType)->second > 0) {
						vecActions.emplace_back(Action::Type::Unload, Action::Direction::West, i);
					}

					if (pUnit->m_properties.m_type == UnitProperties::Type::BlackBoat && pUnloadDestination->m_spUnit != nullptr && pUnloadDestination->m_spUnit->m_owner == &GetCurrentPlayer()) {
						vecActions.emplace_back(Action::Type::Repair, Action::Direction::West);
					}
				}
			}
		}

		if (pUnit->m_moved) {
			return Result::Succeeded;
		}

		MovementTypes movementType = pUnit->m_properties.m_movementType;
		std::vector<std::pair<int, int>> vecTargets;
		std::vector<std::unique_ptr<MoveNode>> vecNodes;
		vecNodes.emplace_back(new MoveNode(pUnit->m_properties.m_movement, pUnit->m_properties.m_fuel, x, y));
		std::queue<MoveNode*> queueMoveNodes;
		queueMoveNodes.push(vecNodes.back().get());
		while (!queueMoveNodes.empty()) {
			MoveNode* pTop = queueMoveNodes.front();
			queueMoveNodes.pop();

			if (pTop->m_visited) {
				continue;
			}

			const int xCurr = pTop->m_x;
			const int yCurr = pTop->m_y;
			// Add moves for current position
			MapTile* pCurrentTile = nullptr;
			m_spmap->TryGetTile(xCurr, yCurr, &pCurrentTile);
			const Unit* pCurrentTileUnit = pCurrentTile->TryGetUnit();
			if (pCurrentTileUnit == nullptr || pCurrentTileUnit == pUnit) {
				vecActions.emplace_back(Action::Type::MoveWait, xCurr, yCurr);
				// Check for enemy attacks
				std::pair<int, int> attackRange = GetUnitInfo(pUnit->m_properties.m_type).m_range;

				// Direct combat actions
				if (attackRange.first == 1) {
					// north
					// TODO: Refactor
					MapTile* pAttackTile = nullptr;
					m_spmap->TryGetTile(xCurr, yCurr - 1, &pAttackTile);
					const Unit* pAttackUnit = pAttackTile->TryGetUnit();
					if (pAttackUnit != nullptr && pAttackUnit->m_owner != &GetCurrentPlayer() && CanUnitAttack(*pCurrentTileUnit, *pAttackUnit)) {
						vecActions.emplace_back(Action::Type::MoveAttack, Action::Direction::North, xCurr, yCurr);
					}
					// east
					m_spmap->TryGetTile(xCurr + 1, yCurr, &pAttackTile);
					pAttackUnit = pAttackTile->TryGetUnit();
					if (pAttackUnit != nullptr && pAttackUnit->m_owner != &GetCurrentPlayer() && CanUnitAttack(*pCurrentTileUnit, *pAttackUnit)) {
						vecActions.emplace_back(Action::Type::MoveAttack, Action::Direction::East, xCurr, yCurr);
					}
					// south
					m_spmap->TryGetTile(xCurr, yCurr + 1, &pAttackTile);
					pAttackUnit = pAttackTile->TryGetUnit();
					if (pAttackUnit != nullptr && pAttackUnit->m_owner != &GetCurrentPlayer() && CanUnitAttack(*pCurrentTileUnit, *pAttackUnit)) {
						vecActions.emplace_back(Action::Type::MoveAttack, Action::Direction::South, xCurr, yCurr);
					}
					// west
					m_spmap->TryGetTile(xCurr - 1, yCurr, &pAttackTile);
					pAttackUnit = pAttackTile->TryGetUnit();
					if (pAttackUnit != nullptr && pAttackUnit->m_owner != &GetCurrentPlayer() && CanUnitAttack(*pCurrentTileUnit, *pAttackUnit)) {
						vecActions.emplace_back(Action::Type::MoveAttack, Action::Direction::West, xCurr, yCurr);
					}
				}
				// Indirect combat can only happen from the same square
				else if (attackRange.first > 1 && x == xCurr && y == yCurr) {
					AddIndirectAttackActions(xCurr, yCurr, *pCurrentTileUnit, attackRange.first, attackRange.second, vecActions);
				}

				// Check for property and capture
				if (MapTile::IsProperty(pCurrentTile->GetTerrain().m_type) && pCurrentTile->m_spPropertyInfo->m_owner != &GetCurrentPlayer() && (pUnit->m_properties.m_type == UnitProperties::Type::Infantry || pUnit->m_properties.m_type == UnitProperties::Type::Mech)) {
					vecActions.emplace_back(Action::Type::MoveCapture, pTop->m_x, pTop->m_y);
				}
			}
			else {
				// this should be true always.  If the unit wasn't owned by the same player we don't expect to visit
				if (pCurrentTileUnit->m_owner == &GetCurrentPlayer()) {
					if (pCurrentTileUnit->IsLander() && pCurrentTileUnit->CanLoad(pUnit->m_properties.m_type)) {
						vecActions.emplace_back(Action::Type::MoveLoad, pTop->m_x, pTop->m_y);
					}
					// Can combine if the unit you move to is not at full health. Can't combine with itself
					else if (pCurrentTileUnit->m_properties.m_type == pUnit->m_properties.m_type && pCurrentTileUnit->health < 90 && x != xCurr && y != yCurr) {
						vecActions.emplace_back(Action::Type::MoveCombine, pTop->m_x, pTop->m_y);
					}
				}
			}

			pTop->m_visited = true;
			if (pTop->m_movement == 0) {
				continue;
			}

			// TODO: Refactor
			// Search north
			if (pTop->m_pnorth == nullptr || pTop->m_pnorth->m_visited == false) {
				auto [movementRemaining, fuelRemaining] = movementRemainingAfterStep(pTop->m_x, pTop->m_y - 1, movementType, pTop->m_movement, pTop->m_fuel);
				if (movementRemaining >= 0 && fuelRemaining >= 0) {
					queueMoveNodes.push(AddNewNodeToGraph(vecActions, vecNodes, &pTop->m_pnorth, movementRemaining, fuelRemaining, pTop->m_x, pTop->m_y - 1));
				}
			}

			// Search east
			if (pTop->m_peast == nullptr || pTop->m_peast->m_visited == false) {
				auto [movementRemaining, fuelRemaining] = movementRemainingAfterStep(pTop->m_x + 1, pTop->m_y, movementType, pTop->m_movement, pTop->m_fuel);
				if (movementRemaining >= 0 && fuelRemaining >= 0) {
					queueMoveNodes.push(AddNewNodeToGraph(vecActions, vecNodes, &pTop->m_peast, movementRemaining, fuelRemaining, pTop->m_x + 1, pTop->m_y));
				}
			}
			// Search south 
			if (pTop->m_psouth == nullptr || pTop->m_psouth->m_visited == false) {
				auto [movementRemaining, fuelRemaining] = movementRemainingAfterStep(pTop->m_x, pTop->m_y + 1, movementType, pTop->m_movement, pTop->m_fuel);
				if (movementRemaining >= 0 && fuelRemaining >= 0) {
					queueMoveNodes.push(AddNewNodeToGraph(vecActions, vecNodes, &pTop->m_psouth, movementRemaining, fuelRemaining, pTop->m_x, pTop->m_y + 1));
				}
			}
			// Search west
			if (pTop->m_pwest == nullptr || pTop->m_pwest->m_visited == false) {
				auto [movementRemaining, fuelRemaining] = movementRemainingAfterStep(pTop->m_x - 1, pTop->m_y, movementType, pTop->m_movement, pTop->m_fuel);
				if (movementRemaining >= 0 && fuelRemaining >= 0) {
					queueMoveNodes.push(AddNewNodeToGraph(vecActions, vecNodes, &pTop->m_pwest, movementRemaining, fuelRemaining, pTop->m_x - 1, pTop->m_y));
				}
			}
		}
		return Result::Succeeded;
	}
	else {
		switch (terrain.m_type) {
		case Terrain::Type::Airport:
			for (auto unit : vrgUnits) {
				int funds = GetCurrentPlayer().m_funds;
				if (unit.m_movementType == MovementTypes::Air && Unit::GetUnitCost(unit.m_type) * 10 <= funds) {
					vecActions.emplace_back(Action::Type::Buy, unit.m_type);
				}
			}
			break;
		case Terrain::Type::Base:
			for (auto unit : vrgUnits) {
				int funds = GetCurrentPlayer().m_funds;
				if ((unit.m_movementType == MovementTypes::Boots ||
					unit.m_movementType == MovementTypes::Foot ||
					unit.m_movementType == MovementTypes::Pipe ||
					unit.m_movementType == MovementTypes::Tires ||
					unit.m_movementType == MovementTypes::Treads) &&
					Unit::GetUnitCost(unit.m_type) * 10 <= funds) {
					vecActions.emplace_back(Action::Type::Buy, unit.m_type);
				}
			}
			break;
		case Terrain::Type::Port:
			for (auto unit : vrgUnits) {
				int funds = GetCurrentPlayer().m_funds;
				if ((unit.m_movementType == MovementTypes::Sea || unit.m_movementType == MovementTypes::Lander) && Unit::GetUnitCost(unit.m_type) * 10 <= funds) {
					vecActions.emplace_back(Action::Type::Buy, unit.m_type);
				}
			}
			break;
		}
	}

	// This square has no unit or property.  No specific tile actions to return
	return Result::Succeeded;
}

Result GameState::AddIndirectAttackActions(int x, int y, const Unit& attacker, int minAttackRange, int maxAttackRange, std::vector<Action>& vecActions) const noexcept {
	for (int xAttack = maxAttackRange * -1; xAttack <= maxAttackRange; ++xAttack) {
		for (int yAttack = maxAttackRange * -1; yAttack <= maxAttackRange; ++yAttack) {
			const int attackRange = std::abs(xAttack) + std::abs(yAttack);
			const int xLocation = x + xAttack;
			const int yLocation = y + yAttack;
			if (attackRange <= maxAttackRange && attackRange >= minAttackRange && xLocation >= 0 && xLocation < m_spmap->GetCols() && yLocation >= 0 && yLocation < m_spmap->GetRows()) {
				const MapTile* pDefendingTile = nullptr;
				Result result = m_spmap->TryGetTile(xLocation, yLocation, &pDefendingTile);
				if (result == Result::Succeeded) {
					const Unit* pDefender = pDefendingTile->TryGetUnit();
					if (pDefender != nullptr && pDefender->m_owner != &GetCurrentPlayer() && CanUnitAttack(attacker, *pDefender)) {
						vecActions.emplace_back(Action::Type::Attack, xLocation, yLocation);
					}
				}
			}
		}
	}

	return Result::Succeeded;
}

Result GameState::DoAction(int x, int y, const Action& action) noexcept {
	switch (action.m_type) {
	default:
		return Result::Failed;
	case Action::Type::Attack:
		return DoAttackAction(x, y, action);
	case Action::Type::Buy:
		return DoBuyAction(x, y, action);
	case Action::Type::COPower:
		return DoCOPowerAction(action);
	case Action::Type::MoveAttack:
	case Action::Type::MoveCapture:
	case Action::Type::MoveCombine:
	case Action::Type::MoveLoad:
	case Action::Type::MoveWait:
		return DoMoveAction(x, y, action);
	case Action::Type::Repair:
	case Action::Type::SCOPower:
	case Action::Type::Unload:
		return Result::Succeeded;
	}
	return Result::Succeeded;
}

Result GameState::DoBuyAction(int x, int y, const Action& action) {
	MapTile* ptile = nullptr;
	IfFailedReturn(m_spmap->TryGetTile(x, y, &ptile));

	Player* currentPlayer = &GetCurrentPlayer();
	if (ptile->m_spPropertyInfo->m_owner != currentPlayer) {
		return Result::Failed;
	}

	if (!action.m_optUnitType.has_value()) {
		return Result::Failed;
	}

	UnitProperties::Type unitType = action.m_optUnitType.value();
	int cost = Unit::GetUnitCost(unitType) * 10;
	if (currentPlayer->m_funds > cost) {
		currentPlayer->m_funds -= cost;
		return ptile->TryAddUnit(unitType, currentPlayer);
	}

	return Result::Succeeded;
}

Result GameState::DoAttackAction(int x, int y, const Action& action) {
	MapTile* pAttackerTile = nullptr;
	IfFailedReturn(m_spmap->TryGetTile(x, y, &pAttackerTile));

	MapTile* pDefenderTile = nullptr;
	IfFailedReturn(m_spmap->TryGetTile(x, y, &pDefenderTile));

	Unit* pattacker = pAttackerTile->TryGetUnit();
	Unit* pdefender = pDefenderTile->TryGetUnit();
	if (pattacker == nullptr ||
		pattacker->m_owner != &GetCurrentPlayer() ||
		pattacker->m_moved ||
		pdefender == nullptr ||
		!CanUnitAttack(*pattacker, *pdefender)) {
		return Result::Failed;
	}

	int attackDamage = calculateDamage(*pattacker, *pdefender, pDefenderTile->GetTerrain().m_defense);
	if (attackDamage < -1) {
		return Result::Failed;
	}

	pdefender->health -= attackDamage;
	attackDamage = calculateDamage(*pdefender, *pattacker, pAttackerTile->GetTerrain().m_defense);
	if (attackDamage > 0) {
		pattacker->health -= attackDamage;
	}

	return Result::Succeeded;
}

int GameState::calculateDamage(const Unit& attacker, const Unit& defender, int defenderTerrainStars) {
	int baseDamage = -1;
	if (defender.IsFootsoldier()) {
		if (attacker.m_properties.m_primaryWeapon == UnitProperties::Weapon::MachineGun) {
			baseDamage = vrgPrimaryWeaponDamage[static_cast<int>(attacker.m_properties.m_type)][static_cast<int>(defender.m_properties.m_type)];
		}
		else if (attacker.m_properties.m_secondaryWeapon == UnitProperties::Weapon::MachineGun) {
			baseDamage = vrgSecondaryWeaponDamage[static_cast<int>(attacker.m_properties.m_type)][static_cast<int>(defender.m_properties.m_type)];
		}
	}
	else if (attacker.m_properties.m_ammo > 0) {
		baseDamage = vrgPrimaryWeaponDamage[static_cast<int>(attacker.m_properties.m_type)][static_cast<int>(defender.m_properties.m_type)];
	}

	if (baseDamage == -1) {
		return -1;
	}

	int attackValue = 100;
	int luck = 0;
	int badluck = 0;
	int defenceValue = 0;
	double damage = ((baseDamage * attackValue / 100.0) + luck - badluck) * (attacker.health / 10) / 10.0 * ((200 - (defenceValue + defenderTerrainStars * defender.health / 10)) / 100.0);
	if (damage <= 0) {
		return 0;
	}

	damage = std::ceil(damage * 20.0) / 20.0;
	return std::floor(damage);
}

Result GameState::DoMoveAction(int x, int y, const Action& action) {
	MapTile* ptile = nullptr;
	IfFailedReturn(m_spmap->TryGetTile(x, y, &ptile));

	const Unit* punit = ptile->TryGetUnit();
	// search to see if unit can move to location
	return Result::Succeeded;
}

Result GameState::DoCOPowerAction(const Action& action) {
	return Result::Succeeded;
}

Map* GameState::TryGetMap() const noexcept {
	return m_spmap.get();
}

const std::string& GameState::GetId() const noexcept {
	return m_guid;
}

const std::array<Player, 2>& GameState::GetPlayers() const noexcept {
	return m_arrPlayers;
}

bool GameState::IsFirstPlayerTurn() const noexcept {
	return m_isFirstPlayerTurn;
}

void to_json(json& j, const GameState& gameState) {
	j = { { "gameId", gameState.GetId()},
		  { "map", *gameState.TryGetMap()},
		  { "players", gameState.GetPlayers()},
		  { "activePlayer", gameState.IsFirstPlayerTurn() ? 0 : 1 } };
}

void to_json(json& j, const Action& action) {
	j = { {"type", action.getTypeString()} };

	if (action.m_optTarget.has_value()) {
		j["target"] = *action.m_optTarget;
	}

	if (action.m_optDirection.has_value()) {
		j["direction"] = action.getDirectionString();
	}

	if (action.m_optUnitType.has_value()) {
		j["unit"] = UnitProperties::getTypename(*action.m_optUnitType);
	}
}

void from_json(json& j, Action& action) {
	j.at("type").get_to(action.m_type);

	if (j.contains("target")) {
		std::pair<int, int> target;
		j.at("target").get_to(target);
		action.m_optTarget = std::move(target);
	}

	if (j.contains("unit")) {
		std::string strUnit;
		j.at("unit").get_to(strUnit);
		action.m_optUnitType = UnitProperties::unitTypeFromString(strUnit);
	}

	if (j.contains("direction")) {
		std::string strDirection;
		j.at("direction").get_to(strDirection);

		if (strDirection == "north") {
			action.m_optDirection = Action::Direction::North;
		}
		else if (strDirection == "east") {
			action.m_optDirection = Action::Direction::East;
		}
		else if (strDirection == "south") {
			action.m_optDirection = Action::Direction::South;
		}
		else if (strDirection == "west") {
			action.m_optDirection = Action::Direction::West;
		}
	}
}
