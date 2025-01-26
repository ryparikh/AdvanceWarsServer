#pragma once

#include <memory>
#include <filesystem>

#include "Terrain.h"

enum class Result : int;
enum class TerrainFileID : int;
class Map;
class Player;

class MapParser
{
public:
	Result TryCreateFromFile(std::filesystem::path filePath, const std::array<Player, 2>& players, std::unique_ptr<Map>& spMap) noexcept;
	static Terrain::Type ToTerrainType(TerrainFileID terrainID) noexcept;
};
