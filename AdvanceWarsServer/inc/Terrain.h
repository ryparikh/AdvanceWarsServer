#pragma once

#include <map>
#include "MovementTypes.h"
#include "TerrainFileId.h"

struct Terrain {
	enum class Type : int {
		Plain,
		Mountain,
		Forest,
		River,
		Road,
		Bridge,
		Sea,
		Shoal,
		Reef,
		City,
		Base,
		Airport,
		Port,
		Headquarters,
		Pipe,
		MissleSilo,
		ComTower,
		Lab,
		Size,
	};

	std::map<MovementTypes, int> m_movementCostMap;
	Type m_type{ Type::Plain };
	int m_defense{ 0 };
};
