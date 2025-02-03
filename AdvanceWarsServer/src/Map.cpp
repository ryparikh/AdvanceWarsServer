#include "Map.h"
#include "MapTile.h"
#include "Result.h"
#include "Terrain.h"
#include "TerrainInfo.h"

#include <iostream>
Result Map::TryAddTile(unsigned int x, unsigned int y, Terrain::Type terrainType, int nTileId) noexcept
{
	std::size_t nRows = GetRows();

	if (nRows < y)
	{
		return Result::Failed;
	}
	else if (nRows == y)
	{
		m_vecvecMapTile.emplace_back();
	}

	std::vector<MapTile>& vecpTerrainRow = m_vecvecMapTile[y];
	std::size_t nColumns = vecpTerrainRow.size();
	if (nColumns < x)
	{
		return Result::Failed;
	}
	else if (nColumns == x)
	{
		vecpTerrainRow.emplace_back(GetTerrainInfo(terrainType), nTileId);
	}
	else
	{
		return Result::Failed;
	}

	if (terrainType == Terrain::Type::Headquarters) {
		m_fHasHeadquarters = true;
	}

	return Result::Succeeded;
}

std::size_t Map::GetRows() const noexcept
{
	return m_vecvecMapTile.size();
}

std::size_t Map::GetCols() const noexcept
{
	return m_vecvecMapTile[0].size();
}

Result Map::TryGetTile(unsigned int x, unsigned int y, MapTile** pTile) noexcept {
	if (y >= GetRows())
	{
		return Result::Failed;
	}

	std::vector<MapTile>& vecMapTileRow = m_vecvecMapTile[y];
	if (x >= vecMapTileRow.size())
	{
		return Result::Failed;
	}

	*pTile = &vecMapTileRow[x];
	return Result::Succeeded;
}

Result Map::TryGetTile(unsigned int x, unsigned int y, const MapTile** pTile) const noexcept {
	return const_cast<Map*>(this)->TryGetTile(x, y, const_cast<MapTile**>(pTile));
}

Result Map::TryGetTerrain(unsigned int x, unsigned int y, Terrain& terrain) const noexcept
{
	const MapTile* pTile;
	if (TryGetTile(x, y, &pTile) != Result::Succeeded) {
		return Result::Failed;
	}

	terrain = pTile->GetTerrain();
	return Result::Succeeded;
}

Result Map::TryGetUnit(unsigned int x, unsigned int y, Unit** pUnit) noexcept {
const MapTile* pTile;
	if (TryGetTile(x, y, &pTile) != Result::Succeeded) {
		return Result::Failed;
	}

	*pUnit = pTile->m_spUnit.get();
	return Result::Succeeded;
}

Result Map::TryGetUnit(unsigned int x, unsigned int y, const Unit** pUnit) const noexcept
{
	const MapTile* pTile;
	if (TryGetTile(x, y, &pTile) != Result::Succeeded) {
		return Result::Failed;
	}

	*pUnit = pTile->m_spUnit.get();
	return Result::Succeeded;
}

Result Map::Capture(unsigned int x, unsigned int y, const Player* player) noexcept {
	MapTile* pTile;
	if (TryGetTile(x, y, &pTile) != Result::Succeeded) {
		return Result::Failed;
	}

	pTile->Capture(player);
	return Result::Succeeded;
}

Result Map::TryAddUnit(unsigned int x, unsigned int y, const UnitProperties::Type& type, const Player* owner) noexcept
{
	if (y >= GetRows())
	{
		return Result::Failed;
	}

	std::vector<MapTile>& vecMapTileRow = m_vecvecMapTile[y];
	if (x >= vecMapTileRow.size())
	{
		return Result::Failed;
	}

	vecMapTileRow[x].TryAddUnit(type, owner);
	return Result::Succeeded;
}


Result Map::TryDestroyUnit(unsigned int x, unsigned int y) noexcept {
	MapTile* pTile;
	IfFailedReturn(TryGetTile(x, y, &pTile));
	IfFailedReturn(pTile->TryDestroyUnit());
	return Result::Succeeded;
}

Map* Map::Clone(const std::array<Player, 2>& arrPlayers) const {
	std::unique_ptr<Map> spNewMap{ new Map() };
	spNewMap->m_vecvecMapTile.reserve(GetRows());
	for (int y = 0; y < GetRows(); ++y) {
		std::vector<MapTile> vecCloneTiles;
		vecCloneTiles.reserve(GetCols());
		for (int x = 0; x < GetCols(); ++x) {
			const MapTile* pTile;
			TryGetTile(x, y, &pTile);
			const Player* pNewPropertyOwner = nullptr;
			if (pTile->m_spPropertyInfo != nullptr && pTile->m_spPropertyInfo->m_owner != nullptr) {
				if (pTile->m_spPropertyInfo->m_owner->m_armyType == arrPlayers[0].m_armyType) {
					pNewPropertyOwner = &arrPlayers[0];
				}
				else if (pTile->m_spPropertyInfo->m_owner->m_armyType == arrPlayers[1].m_armyType) {
					pNewPropertyOwner = &arrPlayers[1];
				}
			}

			const Unit* pUnit = pTile->TryGetUnit();
			const Player* pNewUnitOwner = nullptr;
			if (pUnit != nullptr) {
				if (pUnit->m_owner->m_armyType == arrPlayers[0].m_armyType) {
					pNewUnitOwner = &arrPlayers[0];
				}
				else
					pNewUnitOwner = &arrPlayers[1];
			}

			vecCloneTiles.emplace_back(std::move(pTile->Clone(pNewPropertyOwner, pNewUnitOwner)));
		}
		spNewMap->m_vecvecMapTile.emplace_back(std::move(vecCloneTiles));
	}

	return spNewMap.release();
}

void to_json(json& j, const Map& map) {

	json jsonMap = json::array();
	for (int y = 0; y < map.GetRows(); ++y) {
		json jsonRow = json::array();
		for (int x = 0; x < map.GetCols(); ++x) {
			const MapTile* pTile;
			map.TryGetTile(x, y, &pTile);
			jsonRow.push_back(*pTile);
		}
		jsonMap.push_back(jsonRow);
	}

	j = jsonMap;
}

/*static*/ void Map::from_test_json(const std::array<Player, 2>& arrPlayers, json& j, Map& map) {
	int x = 0;
	int y = 0;
	for (auto& jrow: j) {
		map.m_vecvecMapTile.emplace_back();
		for (auto& jmaptile : jrow) {
			MapTile maptile;
			from_json(arrPlayers, jmaptile, maptile);
			if (maptile.GetTerrain().m_type == Terrain::Type::Headquarters) {
				map.m_fHasHeadquarters = true;
			}
			map.m_vecvecMapTile[y].emplace_back(std::move(maptile));
			++x;
		}
		++y;
	}
}
