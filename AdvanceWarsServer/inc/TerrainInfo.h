#pragma once

#include <array>

#include "Terrain.h"

// Collection of terrain information for different terrain types.
using TerrainInfo = std::array<const Terrain, static_cast<int>(Terrain::Type::Size)>;

extern const TerrainInfo vrgTerrain;

static const Terrain& GetTerrainInfo(Terrain::Type terrainType) noexcept
{
	return vrgTerrain[static_cast<int>(terrainType)];
}
