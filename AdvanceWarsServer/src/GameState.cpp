#include "GameState.h"
#include "UnitInfo.h"

#include <chrono>
#include <iostream>
#include <queue>
#include <random>

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
	m_spmap->Capture(6, 5, &GetPlayers()[0]);
	m_spmap->Capture(7, 13, &GetPlayers()[1]);
	// Bases
	m_spmap->Capture(5, 0, &GetPlayers()[0]);
	m_spmap->Capture(10, 2, &GetPlayers()[0]);
	m_spmap->Capture(11, 10, &GetPlayers()[1]);
	m_spmap->Capture(12, 15, &GetPlayers()[1]);
	m_spmap->TryAddUnit(7, 13, UnitProperties::Type::Infantry, &GetPlayers()[1]);

	BeginTurn();
	return Result::Succeeded;
}

Result GameState::EndTurn() noexcept {
	m_isFirstPlayerTurn = !m_isFirstPlayerTurn;
	IfFailedReturn(BeginTurn());
	return Result::Succeeded;
}

Result GameState::BeginTurn() noexcept {
	// TODO Refactor to get rid of 4O(n^2)
	if (m_isFirstPlayerTurn) {
		++m_nTurnCount;
	}

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

	// Repair Units
	// Resupply by APC Units
	m_arrPlayers[player].m_funds += newFunds;
	for (int x = 0; x < m_spmap->GetCols(); ++x) {
		for (int y = 0; y < m_spmap->GetRows(); ++y) {
			MapTile* pTile = nullptr;
			m_spmap->TryGetTile(x, y, &pTile);
			const Terrain& terrain = pTile->GetTerrain();
			const Unit* pUnit = pTile->m_spUnit.get();
			if (pUnit != nullptr)
			{
				if (pUnit->m_properties.m_type == UnitProperties::Type::Apc && pUnit->m_owner == &GetCurrentPlayer()) {
					ResupplyApcUnits(x, y);
				}

				// This isn't using correct units for health
				if (MapTile::IsProperty(terrain.m_type) && (terrain.m_type != Terrain::Type::ComTower || terrain.m_type != Terrain::Type::Lab) && pUnit->m_owner == &m_arrPlayers[player]) {
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
	}

	// Subtract Fuel/Day
	// Destroy Air/Sea Units
	for (int x = 0; x < m_spmap->GetCols(); ++x) {
		for (int y = 0; y < m_spmap->GetRows(); ++y) {
			MapTile* pTile = nullptr;
			m_spmap->TryGetTile(x, y, &pTile);
			const Terrain& terrain = pTile->GetTerrain();
			Unit* pUnit = pTile->m_spUnit.get();
			if (pUnit != nullptr &&
				pUnit->m_owner == &GetCurrentPlayer() &&
				(pUnit->IsAirUnit() || pUnit->IsSeaUnit())) {
				bool isHidden = pUnit->IsHidden();
				pUnit->m_properties.m_fuel -= isHidden ? pUnit->m_properties.m_fuelCostPerDay.second : pUnit->m_properties.m_fuelCostPerDay.first;
				if (pUnit->m_properties.m_fuel <= 0) {
					m_spmap->TryDestroyUnit(x, y);
				}
			}
		}
	}

	// Unwait
	for (int x = 0; x < m_spmap->GetCols(); ++x) {
		for (int y = 0; y < m_spmap->GetRows(); ++y) {
			MapTile* pTile = nullptr;
			m_spmap->TryGetTile(x, y, &pTile);
			Unit* pUnit = pTile->m_spUnit.get();
			if (pUnit != nullptr &&
				pUnit->m_owner == &GetCurrentPlayer()) {
				pUnit->m_moved = false;
			}
		}
	}
	return Result::Succeeded;
}

Result GameState::ResupplyApcUnits(int x, int y) noexcept {
	const MapTile* pResupplyTile = nullptr;
	IfFailedReturn(m_spmap->TryGetTile(x, y, &pResupplyTile));

	auto resupplyUnit = [&](int xResupply, int yResupply) -> Result {
		MapTile* pTile = nullptr;
		IfFailedReturn(m_spmap->TryGetTile(x, y - 1, &pTile));
		if (pTile != nullptr) {
			Unit* pUnit = pTile->TryGetUnit();
			if (pUnit != nullptr && pUnit->m_owner == pResupplyTile->m_spUnit->m_owner) {
				pUnit->m_properties.m_fuel = GetUnitInfo(pUnit->m_properties.m_type).m_fuel;
			}
		}
		return Result::Succeeded;
	};

	// North
	IfFailedReturn(resupplyUnit(x, y - 1));
	// East 
	IfFailedReturn(resupplyUnit(x + 1, y));
	// South 
	IfFailedReturn(resupplyUnit(x, y + 1));
	// West 
	IfFailedReturn(resupplyUnit(x - 1, y));

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

int GameState::GetCOMovementBonus(const CommandingOfficier::Type& co, const Unit& unit) const noexcept {
	const Player& player = GetCurrentPlayer();
	switch (co) {
	case CommandingOfficier::Type::Adder:
		if (player.PowerStatus() == 1) {
			return 1;
		}
		else if (player.PowerStatus() == 2) {
			return 2;
		}
		return 0;
	case CommandingOfficier::Type::Andy:
		if (player.PowerStatus() == 2) {
			return 1;
		}
		return 0;
	case CommandingOfficier::Type::Drake:
		if (unit.IsSeaUnit()) {
			return 1;
		}
		return 0;
	case CommandingOfficier::Type::Jake:
		if (player.PowerStatus() == 2) {
			return 2;
		}
		return 0;
	case CommandingOfficier::Type::Jess:
		if (unit.IsVehicle()) {
			if (player.PowerStatus() == 1) {
				return 1;
			}
			else if (player.PowerStatus() == 2) {
				return 2;
			}
		}
		return 0;
	case CommandingOfficier::Type::Koal:
		if (player.PowerStatus() == 1) {
			return 1;
		}
		else if (player.PowerStatus() == 2) {
			return 2;
		}
		return 0;
	case CommandingOfficier::Type::Max:
		if (unit.m_properties.m_range.first == 1) {
			if (player.PowerStatus() == 1) {
				return 1;
			}
			else if (player.PowerStatus() == 2) {
				return 2;
			}
		}
		return 0;
	case CommandingOfficier::Type::Sami:
		if (unit.IsTransport()) {
			return 1;
		}
		if (player.PowerStatus() == 1) {
			if (unit.IsFootsoldier()) {
				return 1;
			}
		}
		else if (player.PowerStatus() == 2) {
			if (unit.IsFootsoldier()) {
				return 2;
			}
		}
		return 0;
	case CommandingOfficier::Type::Sensei:
		if (unit.IsTransport()) {
			return 1;
		}
		return 0;
	default:
		return 0;
	}
}

Result GameState::GetValidActions(std::vector<Action>& vecActions) const noexcept {
	for (int x = 0; x < m_spmap->GetCols(); ++x) {
		for (int y = 0; y < m_spmap->GetRows(); ++y) {
			IfFailedReturn(GetValidActions(x, y, vecActions));
		}
	}

	vecActions.emplace_back(Action::Type::EndTurn);
	return Result::Succeeded;
}

bool GameState::FAttackUsesAmmo(const Unit& attacker, const Unit& defender) const noexcept {
	return attacker.m_properties.m_ammo > 0 && vrgPrimaryWeaponDamage[static_cast<int>(attacker.m_properties.m_type)][static_cast<int>(defender.m_properties.m_type)] > -1;
}

bool GameState::FAtUnitCap() const noexcept {
	int totalUnits = 0;
	for (int x = 0; x < m_spmap->GetCols(); ++x) {
		for (int y = 0; y < m_spmap->GetRows(); ++y) {
			const MapTile* pTile = nullptr;
			m_spmap->TryGetTile(x, y, &pTile);
			const Unit* pUnit = pTile->TryGetUnit();
			if (pUnit != nullptr && pUnit->m_owner == &GetCurrentPlayer()) {
				++totalUnits;
			}
		}
	}

	return totalUnits >= m_nUnitCap;
}

Result GameState::GetValidActions(int x, int y, std::vector<Action>& vecActions) const noexcept {
	const MapTile* pmaptile;
	m_spmap->TryGetTile(x, y, &pmaptile);
	const Unit* pUnit = pmaptile->TryGetUnit();
	const Terrain& terrain = pmaptile->GetTerrain();

	// If current player unit, moves
	if (pUnit != nullptr) {
		if (pUnit->m_owner != &GetCurrentPlayer()) {
			return Result::Succeeded;
		}

		int cLoadedUnits = pUnit->CLoadedUnits();
		if (pUnit->IsTransport() && cLoadedUnits > 0) {
			for (int i = 0; i < cLoadedUnits; ++i) {
				const Unit* pUnloadUnit = pUnit->GetLoadedUnit(i);
				const MapTile* pUnloadDestination = nullptr;
				// north
				Result result = m_spmap->TryGetTile(x, y - 1, &pUnloadDestination);
				if (result == Result::Succeeded) {
					if (pUnloadDestination->m_spUnit == nullptr && pUnloadDestination->GetTerrain().m_movementCostMap.find(pUnloadUnit->m_properties.m_movementType)->second > 0) {
						vecActions.emplace_back(Action::Type::Unload, x, y, Action::Direction::North, i);
					}

					if (pUnit->m_properties.m_type == UnitProperties::Type::BlackBoat && pUnloadDestination->m_spUnit != nullptr && pUnloadDestination->m_spUnit->m_owner == &GetCurrentPlayer()) {
						vecActions.emplace_back(Action::Type::Repair, x, y, Action::Direction::North);
					}
				}
				// east
				result = m_spmap->TryGetTile(x + 1, y, &pUnloadDestination);
				if (result == Result::Succeeded) {
					if (pUnloadDestination->m_spUnit == nullptr && pUnloadDestination->GetTerrain().m_movementCostMap.find(pUnloadUnit->m_properties.m_movementType)->second > 0) {
						vecActions.emplace_back(Action::Type::Unload, x, y, Action::Direction::East, i);
					}

					if (pUnit->m_properties.m_type == UnitProperties::Type::BlackBoat && pUnloadDestination->m_spUnit != nullptr && pUnloadDestination->m_spUnit->m_owner == &GetCurrentPlayer()) {
						vecActions.emplace_back(Action::Type::Repair, x, y, Action::Direction::East);
					}
				}
				// south 
				result = m_spmap->TryGetTile(x, y + 1, &pUnloadDestination);
				if (result == Result::Succeeded) {
					if (pUnloadDestination->m_spUnit == nullptr && pUnloadDestination->GetTerrain().m_movementCostMap.find(pUnloadUnit->m_properties.m_movementType)->second > 0) {
						vecActions.emplace_back(Action::Type::Unload, x, y, Action::Direction::South, i);
					}

					if (pUnit->m_properties.m_type == UnitProperties::Type::BlackBoat && pUnloadDestination->m_spUnit != nullptr && pUnloadDestination->m_spUnit->m_owner == &GetCurrentPlayer()) {
						vecActions.emplace_back(Action::Type::Repair, x, y, Action::Direction::South);
					}
				}
				// west 
				result = m_spmap->TryGetTile(x - 1, y, &pUnloadDestination);
				if (result == Result::Succeeded) {
					if (pUnloadDestination->m_spUnit == nullptr && pUnloadDestination->GetTerrain().m_movementCostMap.find(pUnloadUnit->m_properties.m_movementType)->second > 0) {
						vecActions.emplace_back(Action::Type::Unload, x, y, Action::Direction::West, i);
					}

					if (pUnit->m_properties.m_type == UnitProperties::Type::BlackBoat && pUnloadDestination->m_spUnit != nullptr && pUnloadDestination->m_spUnit->m_owner == &GetCurrentPlayer()) {
						vecActions.emplace_back(Action::Type::Repair, x, y, Action::Direction::West);
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
		int maxMovement = pUnit->m_properties.m_movement + GetCOMovementBonus(GetCurrentPlayer().m_co.m_type, *pUnit);
		vecNodes.emplace_back(new MoveNode(maxMovement, pUnit->m_properties.m_fuel, x, y));
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
				vecActions.emplace_back(Action::Type::MoveWait, x, y, xCurr, yCurr);

				// Check for enemy attacks
				std::pair<int, int> attackRange = GetUnitInfo(pUnit->m_properties.m_type).m_range;

				// Direct combat actions
				if (attackRange.first == 1) {
					auto checkAttack = [&](int xAtt, int yAtt, Action::Direction direction) {
						MapTile* pAttackTile = nullptr;
						if (m_spmap->TryGetTile(xAtt, yAtt, &pAttackTile) == Result::Succeeded) {
							const Unit* pAttackUnit = pAttackTile->TryGetUnit();
							if (pAttackUnit != nullptr && pAttackUnit->m_owner != &GetCurrentPlayer() && CanUnitAttack(*pUnit, *pAttackUnit)) {
								vecActions.emplace_back(Action::Type::MoveAttack, x, y, direction, xCurr, yCurr);
							}
						}
					};

					checkAttack(xCurr, yCurr - 1, Action::Direction::North);
					checkAttack(xCurr + 1, yCurr, Action::Direction::East);
					checkAttack(xCurr, yCurr + 1, Action::Direction::South);
					checkAttack(xCurr - 1, yCurr, Action::Direction::West);
				}
				// Indirect combat can only happen from the same square
				else if (attackRange.first > 1 && x == xCurr && y == yCurr) {
					AddIndirectAttackActions(xCurr, yCurr, *pUnit, attackRange.first, attackRange.second, vecActions);
				}

				// Check for property and capture
				if (MapTile::IsProperty(pCurrentTile->GetTerrain().m_type) && pCurrentTile->m_spPropertyInfo->m_owner != &GetCurrentPlayer() && (pUnit->m_properties.m_type == UnitProperties::Type::Infantry || pUnit->m_properties.m_type == UnitProperties::Type::Mech)) {
					vecActions.emplace_back(Action::Type::MoveCapture, x, y, pTop->m_x, pTop->m_y);
				}
			}
			else {
				// this should be true always.  If the unit wasn't owned by the same player we don't expect to visit
				if (pCurrentTileUnit->m_owner == &GetCurrentPlayer()) {
					if (pCurrentTileUnit->IsTransport() && pCurrentTileUnit->CanLoad(pUnit->m_properties.m_type)) {
						vecActions.emplace_back(Action::Type::MoveLoad, x, y, pTop->m_x, pTop->m_y);
					}
					// Can combine if the unit you are combining with is not full health. Can't combine with itself
					else if (pCurrentTileUnit->m_properties.m_type == pUnit->m_properties.m_type && x != xCurr && y != yCurr) {
						if (((pUnit->IsTransport() && pCurrentTileUnit->CLoadedUnits() == 0 && pUnit->CLoadedUnits() == 0) || !pUnit->IsTransport()) &&
							(pCurrentTileUnit->health <= 90)){
							vecActions.emplace_back(Action::Type::MoveCombine, x, y, pTop->m_x, pTop->m_y);
						}
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
		if (MapTile::IsProperty(pmaptile->m_terrain.m_type) && pmaptile->m_spPropertyInfo->m_owner != &GetCurrentPlayer()) {
			return Result::Succeeded;
		}

		if (FAtUnitCap()) {
			return Result::Succeeded;
		}

		switch (terrain.m_type) {
		case Terrain::Type::Airport:
			for (auto unit : vrgUnits) {
				int funds = GetCurrentPlayer().m_funds;
				if (unit.m_type != UnitProperties::Type::BlackBomb && unit.m_movementType == MovementTypes::Air && Unit::GetUnitCost(unit.m_type) * 10 <= funds) {
					vecActions.emplace_back(Action::Type::Buy, x, y, unit.m_type);
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
					vecActions.emplace_back(Action::Type::Buy, x, y, unit.m_type);
				}
			}
			break;
		case Terrain::Type::Port:
			for (auto unit : vrgUnits) {
				int funds = GetCurrentPlayer().m_funds;
				if ((unit.m_movementType == MovementTypes::Sea || unit.m_movementType == MovementTypes::Lander) && Unit::GetUnitCost(unit.m_type) * 10 <= funds) {
					vecActions.emplace_back(Action::Type::Buy, x, y, unit.m_type);
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
						vecActions.emplace_back(Action::Type::Attack, x, y, xLocation, yLocation);
					}
				}
			}
		}
	}

	return Result::Succeeded;
}

bool GameState::AnyValidActions() const noexcept {
	for (int x = 0; x < m_spmap->GetCols(); ++x) {
		for (int y = 0; y < m_spmap->GetRows(); ++y) {
			std::vector<Action> vecActions;
			GetValidActions(x, y, vecActions);
			if (vecActions.size() > 0) {
				return true;
			}
		}
	}

	return false;
}

Result GameState::DoAction(const Action& action) noexcept {
	if (!action.m_optSource.has_value()) {
		switch (action.m_type) {
		case Action::Type::EndTurn:
			return EndTurn();
		case Action::Type::COPower:
			return DoCOPowerAction();
		case Action::Type::SCOPower:
			return DoSCOPowerAction();
		default:
			return Result::Failed;
		}
	}

	const std::pair<int, int>& source = action.m_optSource.value();
	std::vector<Action> vecActions;
	int x = source.first;
	int y = source.second;
	GetValidActions(x, y, vecActions);
	bool fValid = false;
	for (const Action& validAction : vecActions) {
		if (validAction == action) {
			fValid = true;
		}
	}

	if (!fValid) {
		return Result::Failed;
	}

	switch (action.m_type) {
	default:
		return Result::Failed;
	case Action::Type::Attack:
		return DoAttackAction(x, y, action);
	case Action::Type::Buy:
		return DoBuyAction(x, y, action);
	case Action::Type::MoveAttack:
		DoMoveAction(x, y, action);
		return DoAttackAction(x, y, action);
	case Action::Type::MoveCapture:
		DoMoveAction(x, y, action);
		return DoCaptureAction(x, y, action);
	case Action::Type::MoveCombine:
		return DoMoveCombineAction(x, y, action);
	case Action::Type::MoveLoad:
		return Result::Failed;
	case Action::Type::MoveWait:
		return DoMoveAction(x, y, action);
	case Action::Type::Repair:
	case Action::Type::Unload:
		return Result::Failed;
	}
	return Result::Succeeded;
}

Result GameState::DoMoveCombineAction(int x, int y, const Action& action) {
	if (!action.m_optTarget.has_value()) {
		return Result::Failed;
	}

	MapTile* ptile = nullptr;
	IfFailedReturn(m_spmap->TryGetTile(x, y, &ptile));

	MapTile* pDest = nullptr;
	const std::pair<int, int>& pairDestination = action.m_optTarget.value();

	std::unique_ptr<Unit> spunitSource(ptile->SpDetachUnit());
	x = pairDestination.first;
	y = pairDestination.second;
	IfFailedReturn(m_spmap->TryGetTile(x, y, &pDest));
	Unit* punitDest = pDest->TryGetUnit();

	// Combine ammo, fuel, health
	punitDest->m_properties.m_ammo = std::min(GetUnitInfo(spunitSource->m_properties.m_type).m_ammo, punitDest->m_properties.m_ammo + spunitSource->m_properties.m_ammo);
	punitDest->m_properties.m_fuel = std::min(GetUnitInfo(spunitSource->m_properties.m_type).m_fuel, punitDest->m_properties.m_fuel + spunitSource->m_properties.m_fuel);
	
	int healthTotal = punitDest->health + spunitSource->health;
	if (healthTotal > 100) {
		int refundUnits = (healthTotal + 9) / 10 - 10;
		GetCurrentPlayer().m_funds += refundUnits * Unit::GetUnitCost(spunitSource->m_properties.m_type);
		punitDest->health = 100;
	}
	else {
		punitDest->health = healthTotal;
	}

	punitDest->m_moved = true;
	return Result::Succeeded;
}

Result GameState::DoMoveLoadAction(int x, int y, const Action& action) {
	return Result::Failed;
}

Result GameState::DoCaptureAction(int x, int y, const Action& action) {
	MapTile* ptile = nullptr;
	IfFailedReturn(m_spmap->TryGetTile(x, y, &ptile));
	const Unit* pUnit = ptile->TryGetUnit();
	if (pUnit == nullptr) {
		return Result::Failed;
	}

	if (!MapTile::IsProperty(ptile->m_terrain.m_type) || ptile->m_spPropertyInfo->m_owner == &GetCurrentPlayer()) {
		return Result::Failed;
	}

	int& cp = ptile->m_spPropertyInfo->m_capturePoints;
	cp -= ((pUnit->health + 9)/ 10);
	if (cp <= 0) {
		bool fHqCapture = ptile->m_terrain.m_type == Terrain::Type::Headquarters;
		ptile->Capture(&GetCurrentPlayer());
		if (fHqCapture) {
			m_fGameOver = true;
		}

		int totalProperties = 0;
		for (int x = 0; x < m_spmap->GetCols(); ++x) {
			for (int y = 0; y < m_spmap->GetRows(); ++y) {
				const MapTile* pTile = nullptr;
				m_spmap->TryGetTile(x, y, &pTile);
				if (pTile->m_spPropertyInfo != nullptr &&
					pTile->m_spPropertyInfo->m_owner == &GetCurrentPlayer() &&
					(pTile->GetTerrain().m_type != Terrain::Type::Lab || pTile->GetTerrain().m_type != Terrain::Type::ComTower)) {
					++totalProperties;
				}
			}
		}
		if (totalProperties >= m_nCaptureLimit) {
			m_fGameOver = true;
		}
	}

	return Result::Succeeded;
}

Result GameState::DoBuyAction(int x, int y, const Action& action) {
	MapTile* ptile = nullptr;
	IfFailedReturn(m_spmap->TryGetTile(x, y, &ptile));

	if (ptile->TryGetUnit() != nullptr) {
		return Result::Failed;
	}

	Player* currentPlayer = &GetCurrentPlayer();
	if (ptile->m_spPropertyInfo->m_owner != currentPlayer) {
		return Result::Failed;
	}

	if (!action.m_optUnitType.has_value()) {
		return Result::Failed;
	}

	UnitProperties::Type unitType = action.m_optUnitType.value();

	if ((ptile->GetTerrain().m_type == Terrain::Type::Airport && !UnitProperties::IsAirUnit(unitType)) ||
		(ptile->GetTerrain().m_type == Terrain::Type::Base && !UnitProperties::IsGroundUnit(unitType)) ||
		(ptile->GetTerrain().m_type == Terrain::Type::Port && !UnitProperties::IsSeaUnit(unitType))) {
		return Result::Failed;
	}

	int cost = Unit::GetUnitCost(unitType) * 10;
	if (currentPlayer->m_funds >= cost) {
		currentPlayer->m_funds -= cost;
		IfFailedReturn(ptile->TryAddUnit(unitType, currentPlayer));
		Unit* punit = ptile->TryGetUnit();
		punit->m_moved = true;
	}

	return Result::Succeeded;
}

Result GameState::DoAttackAction(int x, int y, const Action& action) {
	MapTile* pAttackerTile = nullptr;
	IfFailedReturn(m_spmap->TryGetTile(x, y, &pAttackerTile));

	MapTile* pDefenderTile = nullptr;
	if (action.m_optDirection.has_value()) {
		switch (action.m_optDirection.value()) {
		case Action::Direction::North:
			IfFailedReturn(m_spmap->TryGetTile(x, y - 1, &pDefenderTile));
			break;
		case Action::Direction::East:
			IfFailedReturn(m_spmap->TryGetTile(x + 1, y, &pDefenderTile));
			break;
		case Action::Direction::South:
			IfFailedReturn(m_spmap->TryGetTile(x, y + 1, &pDefenderTile));
			break;
		case Action::Direction::West:
			IfFailedReturn(m_spmap->TryGetTile(x - 1, y, &pDefenderTile));
			break;
		}
	}
	else if (action.m_optTarget.has_value()) {
		const std::pair<int, int>& target = action.m_optTarget.value();
		IfFailedReturn(m_spmap->TryGetTile(target.first, target.second, &pDefenderTile));
	}
	else {
		return Result::Failed;
	}

	Unit* pattacker = pAttackerTile->TryGetUnit();
	Unit* pdefender = pDefenderTile->TryGetUnit();
	if (pattacker == nullptr ||
		pattacker->m_owner != &GetCurrentPlayer() ||
		pdefender == nullptr ||
		!CanUnitAttack(*pattacker, *pdefender)) {
		return Result::Failed;
	}

	Player* pattackingplayer = &GetCurrentPlayer();
	Player* pdefendingplayer = &GetEnemyPlayer();
	bool fSonjaPower = pdefendingplayer->m_co.m_type == CommandingOfficier::Type::Sonja && pdefendingplayer->PowerStatus() == 2;

	if (fSonjaPower) {
		Player* playerswap = pattackingplayer;
		pattackingplayer = pdefendingplayer;
		pdefendingplayer = playerswap;
		Unit* unitswap = pattacker;
		pattacker = pdefender;
		pdefender = unitswap;
	}

	int attackDamage = calculateDamage(pattackingplayer, pattackingplayer->m_co.m_type, pdefendingplayer->m_co.m_type, *pattacker, *pdefender, pDefenderTile->GetTerrain().m_defense);
	if (attackDamage <= -1) {
		if (!fSonjaPower) {
			return Result::Failed;
		}
	}
	else {
		pdefender->health -= attackDamage;
		if (FAttackUsesAmmo(*pattacker, *pdefender)) {
			--pattacker->m_properties.m_ammo;
		}
	}

	if (pdefender->health <= 0) {
		std::cout << "Defending Unit Destroyed" << std::endl;
		pDefenderTile->TryDestroyUnit();
		return Result::Succeeded;
	}

	// Calculate counter attack only for direct combat
	if (pattacker->m_properties.m_range.first != 1 || pdefender->m_properties.m_range.first != 1) {
		return Result::Succeeded;
	}

	attackDamage = calculateDamage(pdefendingplayer, pdefendingplayer->m_co.m_type, pattackingplayer->m_co.m_type, *pdefender, *pattacker, pAttackerTile->GetTerrain().m_defense);
	if (attackDamage > 0) {
		pattacker->health -= attackDamage;
		if (FAttackUsesAmmo(*pdefender, *pattacker)) {
			--pdefender->m_properties.m_ammo;
		}
	}
	else 		{
		return Result::Failed;
	}

	if (pattacker->health <= 0) {
		std::cout << "Attacking Unit Destroyed by counterattack" << std::endl;
		pAttackerTile->TryDestroyUnit();
		return Result::Succeeded;
	}

	return Result::Succeeded;
}

int GameState::calculateDamage(const Player* pattackingplayer, const CommandingOfficier::Type& attackerCO, const CommandingOfficier::Type& defenderCO, const Unit& attacker, const Unit& defender, int defenderTerrainStars) {
	int baseDamage = -1;
	// Use machine gun against footsolider
	if (defender.IsFootsoldier()) {
		if (attacker.m_properties.m_primaryWeapon == UnitProperties::Weapon::MachineGun) {
			baseDamage = vrgPrimaryWeaponDamage[static_cast<int>(attacker.m_properties.m_type)][static_cast<int>(defender.m_properties.m_type)];
		}
		else if (attacker.m_properties.m_secondaryWeapon == UnitProperties::Weapon::MachineGun) {
			baseDamage = vrgSecondaryWeaponDamage[static_cast<int>(attacker.m_properties.m_type)][static_cast<int>(defender.m_properties.m_type)];
		}
		else if (attacker.m_properties.m_ammo > 0) {
			baseDamage = vrgPrimaryWeaponDamage[static_cast<int>(attacker.m_properties.m_type)][static_cast<int>(defender.m_properties.m_type)];
		}
	}
	else {
		if (attacker.m_properties.m_primaryWeapon == UnitProperties::Weapon::MachineGun) {
			baseDamage = vrgPrimaryWeaponDamage[static_cast<int>(attacker.m_properties.m_type)][static_cast<int>(defender.m_properties.m_type)];
		}
		else if (attacker.m_properties.m_ammo > 0) {
			baseDamage = vrgPrimaryWeaponDamage[static_cast<int>(attacker.m_properties.m_type)][static_cast<int>(defender.m_properties.m_type)];
		}
		else if (attacker.m_properties.m_ammo == 0 && attacker.m_properties.m_secondaryWeapon != UnitProperties::Weapon::Invalid) {
			baseDamage = vrgSecondaryWeaponDamage[static_cast<int>(attacker.m_properties.m_type)][static_cast<int>(defender.m_properties.m_type)];
		}
	}

	if (baseDamage == -1) {
		return -1;
	}

	std::random_device rd;
	std::mt19937 luckGen(rd());
	std::uniform_int_distribution<int> goodLuckDistribution(0, GetMaxGoodLuck(GetCurrentPlayer()));
	// Generate a random number
	int goodLuckRoll = goodLuckDistribution(luckGen);

	std::uniform_int_distribution<int> badLuckDistribution(0, GetMaxBadLuck(GetCurrentPlayer()));
	int badLuckRoll = badLuckDistribution(luckGen);

	int nComTowers = 0;
	for (int x = 0; x < m_spmap->GetCols(); ++x) {
		for (int y = 0; y < m_spmap->GetRows(); ++y) {
			const MapTile* pTile = nullptr;
			m_spmap->TryGetTile(x, y, &pTile);
			const Terrain& terrain = pTile->GetTerrain();
			if (MapTile::IsProperty(terrain.m_type) && terrain.m_type == Terrain::Type::ComTower  && pTile->m_spPropertyInfo->m_owner == pattackingplayer) {
				++nComTowers;
			}
		}
	}

	int attackValue = rgCharts[static_cast<int>(attackerCO)][GetCurrentPlayer().PowerStatus()][static_cast<int>(attacker.m_properties.m_type)].first + 10 * nComTowers;
	int defenceValue = rgCharts[static_cast<int>(defenderCO)][GetEnemyPlayer().PowerStatus()][static_cast<int>(defender.m_properties.m_type)].second;
	int attackerHealth = (attacker.health + 9) / 10;
	int defenderHealth = (defender.health + 9) / 10;
	double damage = ((baseDamage * attackValue / 100.0) + goodLuckRoll - badLuckRoll) * attackerHealth / 10.0 * ((200 - (defenceValue + defenderTerrainStars * defenderHealth)) / 100.0);
	if (damage <= 0) {
		return 0;
	}

	damage = std::ceil(damage * 20.0) / 20.0;
	return std::floor(damage);
}

int GameState::GetMaxGoodLuck(const Player& player) noexcept {
	int status = player.PowerStatus();
	switch (player.m_co.m_type) {
	case CommandingOfficier::Type::Rachel:
		if (status == 1) {
			return 39;
		}
	case CommandingOfficier::Type::Nell:
		if (status == 0) {
			return 19;
		}
		else if (status == 1) {
			return 59;
		}
		else if (status == 2) {
			return 99;
		}
	case CommandingOfficier::Type::Flak:
		if (status == 0) {
			return 24;
		}
		else if (status == 1) {
			return 49;
		}
		else if (status == 2) {
			return 89;
		}
	case CommandingOfficier::Type::Jugger:
		if (status == 0) {
			return 29;
		}
		else if (status == 1) {
			return 54;
		}
		else if (status == 2) {
			return 94;
		}
	default:
		return 9;
	}
}

int GameState::GetMaxBadLuck(const Player& player) noexcept {
	int status = player.PowerStatus();
	switch (player.m_co.m_type) {
	case CommandingOfficier::Type::Sonja:
		return 9;
	case CommandingOfficier::Type::Flak:
		if (status == 0) {
			return 9;
		}
		else if (status == 1) {
			return 19;
		}
		else if (status == 2) {
			return 39;
		}
	case CommandingOfficier::Type::Jugger:
		if (status == 0) {
			return 14;
		}
		else if (status == 1) {
			return 24;
		}
		else if (status == 2) {
			return 44;
		}
	default:
		return 0;
	}
}
Result GameState::DoMoveAction(int& x, int& y, const Action& action) {
	if (!action.m_optTarget.has_value()) 		{
		return Result::Failed;
	}

	MapTile* ptile = nullptr;
	IfFailedReturn(m_spmap->TryGetTile(x, y, &ptile));

	MapTile* pDest = nullptr;
	const std::pair<int, int>& pairDestination = action.m_optTarget.value();

	Unit* punit = ptile->TryGetUnit();
	punit->m_moved = true;
	if (x == pairDestination.first && y == pairDestination.second) {
		if (punit->m_properties.m_type == UnitProperties::Type::Apc) {
			ResupplyApcUnits(x, y);
		}
		return Result::Succeeded;
	}

	punit = ptile->SpDetachUnit();
	x = pairDestination.first;
	y = pairDestination.second;
	IfFailedReturn(m_spmap->TryGetTile(x, y, &pDest));
	pDest->TryAddUnit(punit);
	//TODO: Need to account for fuel usage

	// Reset capture points when moving off of property.
	if (ptile->m_spPropertyInfo != nullptr && ptile->m_spPropertyInfo->m_capturePoints != 20) {
		ptile->m_spPropertyInfo->m_capturePoints = 20;
	}

	// if APC Resupply Units
	if (punit->m_properties.m_type == UnitProperties::Type::Apc) {
		ResupplyApcUnits(x, y);
	}
	return Result::Succeeded;
}

// This isn't quite right.  We should always add 10*health and not round to nearest multiple of ten
void GameState::HealUnits(int health) {
	for (int x = 0; x < m_spmap->GetCols(); ++x) {
		for (int y = 0; y < m_spmap->GetRows(); ++y) {
			MapTile* pTile = nullptr;
			m_spmap->TryGetTile(x, y, &pTile);
			Unit* punit = pTile->TryGetUnit();
			punit->health = (punit->health + health * 10) / 10 * 10;
			punit->health = std::max(punit->health, 100);
		}
	}
}

Result GameState::DoCOPowerAction() {
	CommandingOfficier::Type type = GetCurrentPlayer().m_co.m_type;
	GetCurrentPlayer().SetPowerStatus(1);
	switch (type) {
		case CommandingOfficier::Type::Andy:
			HealUnits(2);
			return Result::Succeeded;
	}
	return Result::Succeeded;
}

Result GameState::DoSCOPowerAction() {
	CommandingOfficier::Type type = GetCurrentPlayer().m_co.m_type;
	GetCurrentPlayer().SetPowerStatus(2);
	switch (type) {
		case CommandingOfficier::Type::Andy:
			HealUnits(5);
			return Result::Succeeded;
	}
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

bool GameState::CheckPlayerResigns() noexcept {
	if (m_nTurnCount < 14) {
		return false;
	}

	int nCurrentPlayerArmyValue = 0;
	int nCurrentPlayerUnits = 0;
	int nEnemyPlayerArmyValue = 0;
	int nEnemyPlayerUnits = 0;
	for (int x = 0; x < m_spmap->GetCols(); ++x) {
		for (int y = 0; y < m_spmap->GetRows(); ++y) {
			const MapTile* pTile = nullptr;
			m_spmap->TryGetTile(x, y, &pTile);
			const Unit* punit = pTile->TryGetUnit();
			if (punit != nullptr) {
				if (punit->m_owner == &GetCurrentPlayer()) {
					++nCurrentPlayerUnits;
					nCurrentPlayerArmyValue += (((punit->health + 9) / 10) * Unit::GetUnitCost(punit->m_properties.m_type));
				}
				else {
					++nEnemyPlayerUnits;
					nEnemyPlayerArmyValue += (((punit->health + 9) / 10) * Unit::GetUnitCost(punit->m_properties.m_type));
				}
			}
		}
	}

	// if army value is less than half opponent value
	// if army units is less than half opponent units
	// forfeit
	if ((nCurrentPlayerArmyValue < (nEnemyPlayerArmyValue / 3)) || (nCurrentPlayerUnits < (nEnemyPlayerUnits / 3))) {
		std::cout << "CAM: " << nCurrentPlayerArmyValue << std::endl;
		std::cout << "EAM: " << nEnemyPlayerArmyValue << std::endl;
		std::cout << "CU: " << nCurrentPlayerUnits << std::endl;
		std::cout << "EU: " << nEnemyPlayerUnits << std::endl;
		m_fGameOver = true;
		m_isFirstPlayerTurn = !m_isFirstPlayerTurn;
	}
	return false;
}

void to_json(json& j, const GameState& gameState) {
	j = { { "gameId", gameState.GetId()},
		  { "map", *gameState.TryGetMap()},
		  { "players", gameState.GetPlayers()},
		  { "activePlayer", gameState.IsFirstPlayerTurn() ? 0 : 1 } };
}

void to_json(json& j, const Action& action) {
	j = { {"type", action.getTypeString()} };

	if (action.m_optSource.has_value()) {
		j["source"] = *action.m_optSource;
	}

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

/*static*/ Action::Type Action::fromTypeString(std::string strType) {
	if (strType == "attack") {
		return Action::Type::Attack;
	}

	if (strType == "buy") {
		return Action::Type::Buy;
	}

	if (strType == "co-power") {
		return Action::Type::COPower;
	}

	if (strType == "end-turn") {
		return Action::Type::EndTurn;
	}

	if (strType == "move-wait") {
		return Action::Type::MoveWait;
	}

	if (strType == "move-attack") {
		return Action::Type::MoveAttack;
	}

	if (strType == "move-capture") {
		return Action::Type::MoveCapture;
	}

	if (strType == "move-combine") {
		return Action::Type::MoveCombine;
	}

	if (strType == "move-load") {
		return Action::Type::MoveLoad;
	}

	if (strType == "repair") {
		return Action::Type::Repair;
	}

	if (strType == "super-co-power") {
		return Action::Type::SCOPower;
	}

	if (strType == "unload") {
		return Action::Type::Unload;
	}

	return Action::Type::Invalid;
}

void from_json(json& j, Action& action) {
	std::string strType;
	j.at("type").get_to(strType);
	action.m_type = Action::fromTypeString(strType);

	if (j.contains("source")) {
		std::pair<int, int> source;
		j.at("source").get_to(source);
		action.m_optSource = std::move(source);
	}

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
