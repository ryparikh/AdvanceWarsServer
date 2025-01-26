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
	MapTile() {}
	MapTile(const Terrain& terrain, int nFileID);
	MapTile(MapTile&& maptile);

	const Terrain& GetTerrain() const {
		return *m_pterrain;
	}

	static bool IsProperty(Terrain::Type type);
	Unit* TryGetUnit() noexcept;
	const Unit* TryGetUnit() const noexcept;
	Unit* SpDetachUnit() noexcept;
	Result TryAddUnit(const UnitProperties::Type& type, const Player* player) noexcept;
	Result TryAddUnit(Unit* pUnit) noexcept;
	Result TryDestroyUnit() noexcept;
	Result Capture(const Player* owner);
	MapTile Clone(const Player* pNewPropertOwner, const Player* pNewUnitOwner) const;

	const Terrain* m_pterrain = nullptr;
	const int m_nFileID = -1;
	// TODO: std::optional?
	std::unique_ptr<Unit> m_spUnit{ nullptr };
	std::unique_ptr<PropertyInfo> m_spPropertyInfo{ nullptr };
};

void to_json(json& j, const MapTile& maptile);
void from_json(const std::array<Player, 2>& arrPlayers, json& j, MapTile& maptile);
void to_json(json& j, const PropertyInfo& propertyInfo);
void from_json(const std::array<Player, 2>& arrPlayers, json& j, PropertyInfo& propertyInfo);
