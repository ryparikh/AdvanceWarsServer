#pragma once
#include <utility>
#include "MovementTypes.h"
#include "nlohmann/json.hpp"
using json = nlohmann::json;

class Player;

struct UnitProperties final
{
	enum class Type {
		Invalid = -1,
		AntiAir,
		Apc,
		Artillery,
		BCopter,
		Battleship,
		BlackBoat,
		BlackBomb,
		Bomber,
		Carrier,
		Crusier,
		Fighter,
		Infantry,
		Lander,
		MediumTank,
		Mech,
		MegaTank,
		Missile,
		Neotank,
		Piperunner,
		Recon,
		Rocket,
		Stealth,
		Sub,
		TCopter,
		Tank,
		Size
	};

	enum class Weapon {
		Invalid = -1,
		AntiSubMissiles,
		AirToAirMissiles,
		AirToSurfaceMissiles,
		AntiAirGun,
		AntiAirMissiles,
		Bazooka,
		Bombs,
		Cannon,
		LightCannon,
		MachineGun,
		MegaCannon,
		MediumCannon,
		NeoCannon,
		OmniMissile,
		PipeCannon,
		Rockets,
		Torpedoes,
		VulcanCannon,
		Size
	};

	static const char* getTypename(UnitProperties::Type type);
	static const UnitProperties::Type unitTypeFromString(const std::string& strTypename);
	static bool IsGroundUnit(const UnitProperties::Type& type) noexcept;
	static bool IsAirUnit(const UnitProperties::Type& type) noexcept;
	static bool IsSeaUnit(const UnitProperties::Type& type) noexcept;
	static bool IsVehicle(const UnitProperties::Type& type) noexcept;
	static bool IsDirectAttack(const UnitProperties::Type& type) noexcept;
	static bool IsIndirectAttack(const UnitProperties::Type& type) noexcept;
	static bool IsFootsoldier(const UnitProperties::Type& type) noexcept;
	const char* getTypename() const;

	Type m_type;
	MovementTypes m_movementType;
	int m_cost;
	int m_movement;
	int m_ammo;
	int m_fuel;
	std::pair<int, int> m_fuelCostPerDay;
	int m_vision;
	std::pair<int, int> m_range;
	Weapon m_primaryWeapon;
	Weapon m_secondaryWeapon;
};

class Unit {
public:
	static int GetUnitCost(const UnitProperties::Type& type);
	Unit() {}
	Unit(const UnitProperties::Type& type, const Player* owner);
	Unit* Clone(const Player* pNewOwner) const;
	bool IsTransport() const noexcept;
	bool IsFootsoldier() const noexcept;
	bool IsAirUnit() const noexcept;
	bool IsSeaUnit() const noexcept;
	bool IsVehicle() const noexcept;
	bool IsHidden() const noexcept;
	bool CanLoad(UnitProperties::Type type) const noexcept;
	//TODO leaks memory should be unique_ptr
	void Load(Unit* pUnit);
	int CLoadedUnits() const noexcept;
	Unit* GetLoadedUnit(int i) const noexcept;
	Unit* Unload(int i);
	UnitProperties m_properties;
	const Player* m_owner;
	int health = 100;
	bool m_moved = false;
	bool m_hidden = false;
	std::vector<Unit*> m_vecLanderUnits;
};

void to_json(json& j, const Unit& unit);
void from_json(const std::array<Player, 2>& arrPlayers, json& j, Unit& unit);
void from_json(json& j, UnitProperties& unit);
