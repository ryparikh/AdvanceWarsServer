#include "TerrainInfo.h"

const TerrainInfo vrgTerrain{{
	{ {{MovementTypes::Foot, 1},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, 1},
		{MovementTypes::Tires, 2},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, -1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::Plain,
		1
	},
	{ {{MovementTypes::Foot, 2},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, -1},
		{MovementTypes::Tires, -1},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, -1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::Mountain,
		4
	},
	{ {{MovementTypes::Foot, 1},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, 2},
		{MovementTypes::Tires, 3},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, -1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::Forest,
		2
	},
	{ {{MovementTypes::Foot, 2},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, -1},
		{MovementTypes::Tires, -1},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, -1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::River,
		0
	},
	{ {{MovementTypes::Foot, 1},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, 1},
		{MovementTypes::Tires, 1},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, -1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::Road,
		0
	},
	{ {{MovementTypes::Foot, 1},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, 1},
		{MovementTypes::Tires, 1},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, -1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::Bridge,
		0
	},
	{ {{MovementTypes::Foot, -1},
		{MovementTypes::Boots, -1},
		{MovementTypes::Treads, -1},
		{MovementTypes::Tires, -1},
		{MovementTypes::Sea, 1},
		{MovementTypes::Lander, 1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::Sea,
		0
	},
	{ {{MovementTypes::Foot, 1},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, 1},
		{MovementTypes::Tires, 1},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, 1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::Shoal,
		0
	},
	{ {{MovementTypes::Foot, -1},
		{MovementTypes::Boots, -1},
		{MovementTypes::Treads, -1},
		{MovementTypes::Tires, -1},
		{MovementTypes::Sea, 2},
		{MovementTypes::Lander, 2},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::Reef,
		1
	},
	{ {{MovementTypes::Foot, 1},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, 1},
		{MovementTypes::Tires, 1},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, -1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::City,
		3
	},
	{ {{MovementTypes::Foot, 1},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, 1},
		{MovementTypes::Tires, 1},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, -1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::Base,
		3
	},
	{ {{MovementTypes::Foot, 1},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, 1},
		{MovementTypes::Tires, 1},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, -1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::Airport,
		3
	},
	{ {{MovementTypes::Foot, 1},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, 1},
		{MovementTypes::Tires, 1},
		{MovementTypes::Sea, 1},
		{MovementTypes::Lander, 1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::Port,
		3
	},
	{ {{MovementTypes::Foot, 1},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, 1},
		{MovementTypes::Tires, 1},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, -1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::Headquarters,
		4
	},
	{ {{MovementTypes::Foot, -1},
		{MovementTypes::Boots, -1},
		{MovementTypes::Treads, -1},
		{MovementTypes::Tires, -1},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, -1},
		{MovementTypes::Air, -1},
		{MovementTypes::Pipe, 1}},
		Terrain::Type::Pipe,
		0
	},
	{ {{MovementTypes::Foot, 1},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, 1},
		{MovementTypes::Tires, 1},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, -1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::MissleSilo,
		3
	},
	{ {{MovementTypes::Foot, 1},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, 1},
		{MovementTypes::Tires, 1},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, -1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::ComTower,
		3
	},
	{ {{MovementTypes::Foot, 1},
		{MovementTypes::Boots, 1},
		{MovementTypes::Treads, 1},
		{MovementTypes::Tires, 1},
		{MovementTypes::Sea, -1},
		{MovementTypes::Lander, -1},
		{MovementTypes::Air, 1},
		{MovementTypes::Pipe, -1}},
		Terrain::Type::Lab,
		3
	},
}};