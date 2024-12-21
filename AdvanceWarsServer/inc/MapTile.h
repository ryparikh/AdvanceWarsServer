#pragma once
#include <memory>
#include "Result.h"
#include "Player.h"
#include <Terrain.h>
#include <Unit.h>

struct PropertyInfo{
	int m_capturePoints = 20;
	const Player* m_owner;
};

class MapTile final
{
public:
	MapTile(const Terrain& terrain);
	MapTile(Terrain&& terrain);

	const Terrain& GetTerrain() const {
		return m_terrain;
	}

	static bool IsProperty(Terrain::Type type);
	Unit* TryGetUnit() noexcept;
	const Unit* TryGetUnit() const noexcept;
	Result TryAddUnit(const UnitProperties::Type& type, const Player* player) noexcept;
	Result Capture(const Player* owner);

	const Terrain& m_terrain;
	std::unique_ptr<Unit> m_spUnit{ nullptr };
	std::unique_ptr<PropertyInfo> m_spPropertyInfo{ nullptr };
};

void to_json(json& j, const MapTile& maptile);
