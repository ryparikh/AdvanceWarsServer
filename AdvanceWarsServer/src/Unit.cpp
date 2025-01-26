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

Unit* Unit::Clone(const Player* pNewOwner) const {
	std::unique_ptr<Unit> spClone(new Unit(m_properties.m_type, pNewOwner));
	spClone->health = health;
	spClone->m_moved = m_moved;
	spClone->m_hidden = m_hidden;
	for (const Unit* pLoadedUnit : m_vecLanderUnits) {
		spClone->m_vecLanderUnits.emplace_back(pLoadedUnit->Clone(pNewOwner));
	}
	return spClone.release();
}

bool Unit::IsVehicle() const noexcept {
	return UnitProperties::IsVehicle(m_properties.m_type);
}

bool Unit::IsTransport() const noexcept {
	return m_properties.m_type == UnitProperties::Type::Apc ||
		m_properties.m_type == UnitProperties::Type::BlackBoat ||
		m_properties.m_type == UnitProperties::Type::Crusier ||
		m_properties.m_type == UnitProperties::Type::Carrier ||
		m_properties.m_type == UnitProperties::Type::Lander ||
		m_properties.m_type == UnitProperties::Type::TCopter;
}

bool Unit::IsFootsoldier() const noexcept {
	return UnitProperties::IsFootsoldier(m_properties.m_type);
}

/*static*/ bool UnitProperties::IsFootsoldier(const UnitProperties::Type& type) noexcept {
	return type == UnitProperties::Type::Infantry || type == UnitProperties::Type::Mech;
}

/*static*/ bool UnitProperties::IsSeaUnit(const UnitProperties::Type& type) noexcept {
	switch (type) {
	default:
		return false;
	case UnitProperties::Type::Battleship:
	case UnitProperties::Type::BlackBoat:
	case UnitProperties::Type::Carrier:
	case UnitProperties::Type::Crusier:
	case UnitProperties::Type::Lander:
	case UnitProperties::Type::Sub:
		return true;
	}
}

/*static*/ bool UnitProperties::IsVehicle(const UnitProperties::Type& type) noexcept {
	switch (type) {
	default:
		return false;
	case UnitProperties::Type::AntiAir:
	case UnitProperties::Type::Apc:
	case UnitProperties::Type::Artillery:
	case UnitProperties::Type::MediumTank:
	case UnitProperties::Type::MegaTank:
	case UnitProperties::Type::Missile:
	case UnitProperties::Type::Neotank:
	case UnitProperties::Type::Piperunner:
	case UnitProperties::Type::Recon:
	case UnitProperties::Type::Rocket:
	case UnitProperties::Type::Tank:
		return true;
	}
}

/*static*/ bool UnitProperties::IsDirectAttack(const UnitProperties::Type& type) noexcept {
	switch (type) {
	default:
		return false;
	case UnitProperties::Type::AntiAir:
	case UnitProperties::Type::BCopter:
	case UnitProperties::Type::Bomber:
	case UnitProperties::Type::Crusier:
	case UnitProperties::Type::Fighter:
	case UnitProperties::Type::Infantry:
	case UnitProperties::Type::Mech:
	case UnitProperties::Type::MediumTank:
	case UnitProperties::Type::MegaTank:
	case UnitProperties::Type::Neotank:
	case UnitProperties::Type::Recon:
	case UnitProperties::Type::Stealth:
	case UnitProperties::Type::Sub:
	case UnitProperties::Type::Tank:
		return true;
	}
}

/*static*/ bool UnitProperties::IsIndirectAttack(const UnitProperties::Type& type) noexcept {
	switch (type) {
	default:
		return false;
	case UnitProperties::Type::Artillery:
	case UnitProperties::Type::Battleship:
	case UnitProperties::Type::Carrier:
	case UnitProperties::Type::Missile:
	case UnitProperties::Type::Piperunner:
	case UnitProperties::Type::Rocket:
		return true;
	}
}

/*static*/ bool UnitProperties::IsAirUnit(const UnitProperties::Type& type) noexcept {
	switch (type)
	{
	default:
		return false;
	case UnitProperties::Type::BCopter:
	case UnitProperties::Type::BlackBomb:
	case UnitProperties::Type::Bomber:
	case UnitProperties::Type::Fighter:
	case UnitProperties::Type::Stealth:
	case UnitProperties::Type::TCopter:
		return true;
	}
}

/*static*/ bool UnitProperties::IsGroundUnit(const UnitProperties::Type& type) noexcept {
	switch (type)
	{
	default:
		return false;
	case UnitProperties::Type::AntiAir:
	case UnitProperties::Type::Apc:
	case UnitProperties::Type::Artillery:
	case UnitProperties::Type::Infantry:
	case UnitProperties::Type::MediumTank:
	case UnitProperties::Type::Mech:
	case UnitProperties::Type::MegaTank:
	case UnitProperties::Type::Missile:
	case UnitProperties::Type::Neotank:
	case UnitProperties::Type::Piperunner:
	case UnitProperties::Type::Recon:
	case UnitProperties::Type::Rocket:
	case UnitProperties::Type::Tank:
		return true;
	}
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
		unitTypeIsOk = UnitProperties::IsAirUnit(type);
	}

	if (m_properties.m_type == UnitProperties::Type::Lander) {
		unitTypeIsOk = UnitProperties::IsGroundUnit(type);
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
	m_vecLanderUnits.erase(m_vecLanderUnits.begin() + i);
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
		return "recon";
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

bool Unit::IsAirUnit() const noexcept {
	return UnitProperties::IsAirUnit(m_properties.m_type);
}

bool Unit::IsSeaUnit() const noexcept {
	return UnitProperties::IsSeaUnit(m_properties.m_type);
}

bool Unit::IsHidden() const noexcept {
	return m_hidden;
}

/*static*/ const UnitProperties::Type UnitProperties::unitTypeFromString(const std::string& strTypename) {
	if (strTypename == "antiair") {
		return UnitProperties::Type::AntiAir;
	} else if (strTypename == "apc") {
		return UnitProperties::Type::Apc;
	} else if (strTypename == "artillery") {
		return UnitProperties::Type::Artillery;
	} else if (strTypename == "bcopter") {
		return UnitProperties::Type::BCopter;
	} else if (strTypename == "battleship") {
		return UnitProperties::Type::Battleship;
	}
	else if (strTypename == "blackboat") {
		return UnitProperties::Type::BlackBoat;
	}
	else if (strTypename == "blackbomb") {
		return UnitProperties::Type::BlackBomb;
	}
	else if (strTypename == "bomber") {
		return UnitProperties::Type::Bomber;
	}
	else if (strTypename == "carrier") {
		return UnitProperties::Type::Carrier;
	}
	else if (strTypename == "crusier") {
		return UnitProperties::Type::Crusier;
	}
	else if (strTypename == "fighter") {
		return UnitProperties::Type::Fighter;
	}
	else if (strTypename == "infantry") {
		return UnitProperties::Type::Infantry;
	}
	else if (strTypename == "lander") {
		return UnitProperties::Type::Lander;
	}
	else if (strTypename == "medium-tank") {
		return UnitProperties::Type::MediumTank;
	}
	else if (strTypename == "mech") {
		return UnitProperties::Type::Mech;
	}
	else if (strTypename == "megatank") {
		return UnitProperties::Type::MegaTank;
	}
	else if (strTypename == "missile") {
		return UnitProperties::Type::Missile;
	}
	else if (strTypename == "neotank") {
		return UnitProperties::Type::Neotank;
	}
	else if (strTypename == "piperunner") {
		return UnitProperties::Type::Piperunner;
	}
	else if (strTypename == "recon") {
		return UnitProperties::Type::Recon;
	}
	else if (strTypename == "rocket") {
		return UnitProperties::Type::Rocket;
	}
	else if (strTypename == "stealth") {
		return UnitProperties::Type::Stealth;
	}
	else if (strTypename == "sub") {
		return UnitProperties::Type::Sub;
	}
	else if (strTypename == "tcopter") {
		return UnitProperties::Type::TCopter;
	}
	else if (strTypename == "tank") {
		return UnitProperties::Type::Tank;
	}

	return UnitProperties::Type::Invalid;
}

const char* UnitProperties::getTypename() const {
	return UnitProperties::getTypename(m_type);
}

void to_json(json& j, const UnitProperties& unitproperties) {
	j = { {"type", unitproperties.getTypename() },
		  {"ammo", unitproperties.m_ammo },
		  {"fuel", unitproperties.m_fuel}
	};
}

void to_json(json& j, const Unit& unit) {
	to_json(j, unit.m_properties);
	j["owner"] = unit.m_owner->getArmyTypeJson();
	j["health"] = unit.health;
	j["moved"] = unit.m_moved;
	j["hidden"] = unit.m_hidden;
	if (unit.m_vecLanderUnits.size() == 1) {
		json loadedUnit;
		to_json(loadedUnit, *unit.m_vecLanderUnits[0]);
		j["loaded-units"] = { loadedUnit };
	}
	else if (unit.m_vecLanderUnits.size() == 2) {
		json loadedUnit;
		to_json(loadedUnit, *unit.m_vecLanderUnits[0]);
		json loadedUnit2;
		to_json(loadedUnit2, *unit.m_vecLanderUnits[1]);
		j["loaded-units"] = { loadedUnit, loadedUnit2 };
	}
}

void from_json(const std::array<Player, 2>& arrPlayers, json& j, Unit& unit) {
	from_json(j, unit.m_properties);
	j.at("health").get_to(unit.health);

	std::string armyType;
	j.at("owner").get_to(armyType);
	if (arrPlayers[0].m_armyType == Player::armyTypefromString(armyType)) {
		unit.m_owner = &arrPlayers[0];
	}
	else if (arrPlayers[1].m_armyType == Player::armyTypefromString(armyType)) {
		unit.m_owner = &arrPlayers[1];
	}

	j.at("moved").get_to(unit.m_moved);
	j.at("hidden").get_to(unit.m_hidden);

	if (j.contains("loaded-units")) {
		for (auto& jUnit : j.at("loaded-units")) {
			Unit* ploadedUnit = new Unit();
			from_json(arrPlayers, jUnit, *ploadedUnit);
			unit.m_vecLanderUnits.emplace_back(ploadedUnit);
		}
	}
}

void from_json(json& j, UnitProperties& unitProperties) {
	if (j.contains("type")) {
		std::string typeName;
		j.at("type").get_to(typeName);
		UnitProperties::Type type = UnitProperties::unitTypeFromString(typeName);
		unitProperties = GetUnitInfo(type);
	}

	if (j.contains("ammo")) {
		j.at("ammo").get_to(unitProperties.m_ammo);
	}

	if (j.contains("fuel")) {
		j.at("fuel").get_to(unitProperties.m_fuel);
	}
}

