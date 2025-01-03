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
	MapTile(const Terrain& terrain, int nFileID);

	const Terrain& GetTerrain() const {
		return m_terrain;
	}

	static bool IsProperty(Terrain::Type type);
	Unit* TryGetUnit() noexcept;
	const Unit* TryGetUnit() const noexcept;
	Unit* SpDetachUnit() noexcept;
	Result TryAddUnit(const UnitProperties::Type& type, const Player* player) noexcept;
	Result TryAddUnit(Unit* pUnit) noexcept;
	Result TryDestroyUnit() noexcept;
	Result Capture(const Player* owner);

	const Terrain& m_terrain;
	const int m_nFileID;
	std::unique_ptr<Unit> m_spUnit{ nullptr };
	std::unique_ptr<PropertyInfo> m_spPropertyInfo{ nullptr };
};

void to_json(json& j, const MapTile& maptile);
void to_json(json& j, const PropertyInfo& propertyInfo);
