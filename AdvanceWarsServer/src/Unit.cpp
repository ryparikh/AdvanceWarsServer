#include "Unit.h"
#include "UnitInfo.h"
#include "Player.h"

/*static*/ int Unit::GetUnitCost(const UnitProperties::Type& type) {
	switch (type)
	{
	case UnitProperties::Type::AntiAir:
		return 800;
	case UnitProperties::Type::Apc:
		return 500;
	case UnitProperties::Type::Artillery:
		return 600;
	case UnitProperties::Type::BCopter:
		return 900;
	case UnitProperties::Type::Battleship:
		return 2800;
	case UnitProperties::Type::BlackBoat:
		return 750;
	case UnitProperties::Type::BlackBomb:
		return 2500;
	case UnitProperties::Type::Bomber:
		return 2200;
	case UnitProperties::Type::Carrier:
		return 3000;
	case UnitProperties::Type::Crusier:
		return 1800;
	case UnitProperties::Type::Fighter:
		return 2000;
	case UnitProperties::Type::Infantry:
		return 100;
	case UnitProperties::Type::Lander:
		return 1200;
	case UnitProperties::Type::MediumTank:
		return 1600;
	case UnitProperties::Type::Mech:
		return 300;
	case UnitProperties::Type::MegaTank:
		return 2800;
	case UnitProperties::Type::Missile:
		return 1200;
	case UnitProperties::Type::Neotank:
		return 2200;
	case UnitProperties::Type::Piperunner:
		return 2000;
	case UnitProperties::Type::Recon:
		return 400;
	case UnitProperties::Type::Rocket:
		return 1500;
	case UnitProperties::Type::Stealth:
		return 2400;
	case UnitProperties::Type::Sub:
		return 2000;
	case UnitProperties::Type::TCopter:
		return 500;
	case UnitProperties::Type::Tank:
		return 700;
	}

	return -1;
}

Unit::Unit(const UnitProperties::Type& type, const Player* owner): m_owner(owner) {
	m_properties = GetUnitInfo(type);
}


bool Unit::IsLander() const noexcept {
	return m_properties.m_type == UnitProperties::Type::Apc ||
		m_properties.m_type == UnitProperties::Type::BlackBoat ||
		m_properties.m_type == UnitProperties::Type::Crusier ||
		m_properties.m_type == UnitProperties::Type::Carrier ||
		m_properties.m_type == UnitProperties::Type::Lander ||
		m_properties.m_type == UnitProperties::Type::TCopter;
}

bool IsAirUnit(UnitProperties::Type type) {
	switch (type)
	{
	case UnitProperties::Type::AntiAir:
		return false;
	case UnitProperties::Type::Apc:
		return false;
	case UnitProperties::Type::Artillery:
		return false;
	case UnitProperties::Type::BCopter:
		return true;
	case UnitProperties::Type::Battleship:
		return false;
	case UnitProperties::Type::BlackBoat:
		return false;
	case UnitProperties::Type::BlackBomb:
		return true;
	case UnitProperties::Type::Bomber:
		return true;
	case UnitProperties::Type::Carrier:
		return false;
	case UnitProperties::Type::Crusier:
		return false;
	case UnitProperties::Type::Fighter:
		return true;
	case UnitProperties::Type::Infantry:
		return false;
	case UnitProperties::Type::Lander:
		return false;
	case UnitProperties::Type::MediumTank:
		return false;
	case UnitProperties::Type::Mech:
		return false;
	case UnitProperties::Type::MegaTank:
		return false;
	case UnitProperties::Type::Missile:
		return false;
	case UnitProperties::Type::Neotank:
		return false;
	case UnitProperties::Type::Piperunner:
		return false;
	case UnitProperties::Type::Recon:
		return false;
	case UnitProperties::Type::Rocket:
		return false;
	case UnitProperties::Type::Stealth:
		return true;
	case UnitProperties::Type::Sub:
		return false;
	case UnitProperties::Type::TCopter:
		return true;
	case UnitProperties::Type::Tank:
		return false;
	}

	return false;
}

bool IsGroundUnit(UnitProperties::Type type) {
	switch (type)
	{
	case UnitProperties::Type::AntiAir:
		return true;
	case UnitProperties::Type::Apc:
		return true;
	case UnitProperties::Type::Artillery:
		return true;
	case UnitProperties::Type::BCopter:
		return false;
	case UnitProperties::Type::Battleship:
		return false;
	case UnitProperties::Type::BlackBoat:
		return false;
	case UnitProperties::Type::BlackBomb:
		return false;
	case UnitProperties::Type::Bomber:
		return false;
	case UnitProperties::Type::Carrier:
		return false;
	case UnitProperties::Type::Crusier:
		return false;
	case UnitProperties::Type::Fighter:
		return false;
	case UnitProperties::Type::Infantry:
		return true;
	case UnitProperties::Type::Lander:
		return false;
	case UnitProperties::Type::MediumTank:
		return true;
	case UnitProperties::Type::Mech:
		return true;
	case UnitProperties::Type::MegaTank:
		return true;
	case UnitProperties::Type::Missile:
		return true;
	case UnitProperties::Type::Neotank:
		return true;
	case UnitProperties::Type::Piperunner:
		return true;
	case UnitProperties::Type::Recon:
		return true;
	case UnitProperties::Type::Rocket:
		return true;
	case UnitProperties::Type::Stealth:
		return false;
	case UnitProperties::Type::Sub:
		return false;
	case UnitProperties::Type::TCopter:
		return false;
	case UnitProperties::Type::Tank:
		return true;
	}

	return false;

}

bool Unit::CanLoad(UnitProperties::Type type) const noexcept {
	bool unitTypeIsOk = false;
	if (m_properties.m_type == UnitProperties::Type::Apc || m_properties.m_type == UnitProperties::Type::TCopter) {
		unitTypeIsOk = (type == UnitProperties::Type::Infantry || type == UnitProperties::Type::Mech);
		return unitTypeIsOk && (m_vecLanderUnits.size() < (1U));
	}

	if (m_properties.m_type == UnitProperties::Type::BlackBoat) {
		unitTypeIsOk = (type == UnitProperties::Type::Infantry || type == UnitProperties::Type::Mech);
	}

	if (m_properties.m_type == UnitProperties::Type::Crusier || m_properties.m_type == UnitProperties::Type::Carrier) {
		unitTypeIsOk = IsAirUnit(type);
	}

	if (m_properties.m_type == UnitProperties::Type::Lander) {
		unitTypeIsOk = IsGroundUnit(type);
	}

	return unitTypeIsOk && (m_vecLanderUnits.size() < (2U));
}

void Unit::Load(Unit* pUnit) {
	if (CanLoad(pUnit->m_properties.m_type)) {
		m_vecLanderUnits.push_back(pUnit);
	}
}

Unit* Unit::GetLoadedUnit(int i) const noexcept {
	return m_vecLanderUnits[i];
}

int Unit::CLoadedUnits() const noexcept {
	return static_cast<int>(m_vecLanderUnits.size());
}

Unit* Unit::Unload(int i) {
	Unit* punit = m_vecLanderUnits[i];
	m_vecLanderUnits[i] = nullptr;
	return punit;
}

/*static*/ const char* UnitProperties::getTypename(UnitProperties::Type type) {
	switch (type)
	{
	case UnitProperties::Type::AntiAir:
		return "antiair";
	case UnitProperties::Type::Apc:
		return "apc";
	case UnitProperties::Type::Artillery:
		return "artillery";
	case UnitProperties::Type::BCopter:
		return "bcopter";
	case UnitProperties::Type::Battleship:
		return "battleship";
	case UnitProperties::Type::BlackBoat:
		return "blackboat";
	case UnitProperties::Type::BlackBomb:
		return "blackbomb";
	case UnitProperties::Type::Bomber:
		return "bomber";
	case UnitProperties::Type::Carrier:
		return "carrier";
	case UnitProperties::Type::Crusier:
		return "crusier";
	case UnitProperties::Type::Fighter:
		return "fighter";
	case UnitProperties::Type::Infantry:
		return "infantry";
	case UnitProperties::Type::Lander:
		return "lander";
	case UnitProperties::Type::MediumTank:
		return "medium-tank";
	case UnitProperties::Type::Mech:
		return "mech";
	case UnitProperties::Type::MegaTank:
		return "megatank";
	case UnitProperties::Type::Missile:
		return "missile";
	case UnitProperties::Type::Neotank:
		return "neotank";
	case UnitProperties::Type::Piperunner:
		return "piperunner";
	case UnitProperties::Type::Recon:
		return "reacon";
	case UnitProperties::Type::Rocket:
		return "rocket";
	case UnitProperties::Type::Stealth:
		return "stealth";
	case UnitProperties::Type::Sub:
		return "sub";
	case UnitProperties::Type::TCopter:
		return "tcopter";
	case UnitProperties::Type::Tank:
		return "tank";
	default:
		return "";
	}

}

const char* UnitProperties::getTypename() const {
	return UnitProperties::getTypename(m_type);
}

void to_json(json& j, const UnitProperties& unitproperties) {
	j = { {"type", unitproperties.getTypename() },
		  {"ammo", unitproperties.m_ammo },
		  {"fuel", unitproperties.m_fuel} };
}

void to_json(json& j, const Unit& unit) {
	to_json(j, unit.m_properties);
	j["owner"] = unit.m_owner->getArmyTypeJson();
}