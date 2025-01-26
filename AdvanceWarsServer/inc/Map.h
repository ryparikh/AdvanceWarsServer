#pragma once

#include <vector>

#include "MapTile.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

enum class Terrain::Type : int;
enum class Result : int;

class Map final
{
public:
	std::size_t GetRows() const noexcept;
	std::size_t GetCols() const noexcept;
	Result TryGetTerrain(unsigned int x, unsigned int y, Terrain& terrain) const noexcept;
	Result TryGetUnit(unsigned int x, unsigned int y, const Unit** pUnit) const noexcept;
	Result TryGetUnit(unsigned int x, unsigned int y, Unit** pUnit) noexcept;
	Result TryGetTile(unsigned int x, unsigned int y, const MapTile** pTile) const noexcept;
	Result TryGetTile(unsigned int x, unsigned int y, MapTile** pTile) noexcept;
	Result TryAddTile(unsigned int x, unsigned int y, Terrain::Type terrainType, int nTileId) noexcept;
	Result TryAddUnit(unsigned int x, unsigned int y, const UnitProperties::Type& type, const Player* owner) noexcept;
	Result TryDestroyUnit(unsigned int x, unsigned int y) noexcept;
	Result Capture(unsigned int x, unsigned int y, const Player* player) noexcept;
	Map* Clone(const std::array<Player, 2>& arrPlayers) const;
	static void from_test_json(const std::array<Player, 2>& arrPlayers, json& j, Map& map);
private:
	std::vector<std::vector<MapTile>> m_vecvecMapTile;
};

void to_json(json& j, const Map& map);