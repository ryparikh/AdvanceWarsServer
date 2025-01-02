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
