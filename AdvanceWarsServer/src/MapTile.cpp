#include <MapTile.h>
#include <MapParser.h>
#include <TerrainInfo.h>
#include <TerrainFileId.h>

/*static*/ bool MapTile::IsProperty(Terrain::Type type) {
	return type == Terrain::Type::Airport ||
		type == Terrain::Type::Base ||
		type == Terrain::Type::City ||
		type == Terrain::Type::ComTower ||
		type == Terrain::Type::Headquarters ||
		type == Terrain::Type::Lab ||
		type == Terrain::Type::Port;
}

MapTile::MapTile(const Terrain& terrain, int nFileID) :
	m_pterrain(&terrain),
	m_nFileID(nFileID) {
	if (IsProperty(terrain.m_type)) {
		m_spPropertyInfo.reset(new PropertyInfo());
	}
}

Result MapTile::Capture(const Player* owner) {
	// Should I just reallocate instead of failing?
	if (m_spPropertyInfo == nullptr) {
		return Result::Failed;
	}

	m_spPropertyInfo->m_owner = owner;
	m_spPropertyInfo->m_capturePoints = 20;
	return Result::Succeeded;
}

Result MapTile::TryAddUnit(const UnitProperties::Type& type, const Player* player) noexcept {
	++player->m_unitsCached;
	m_spUnit.reset(new Unit(type, player));
	return Result::Succeeded;
}

Result MapTile::TryAddUnit(Unit* pUnit) noexcept {
	m_spUnit.reset(pUnit);
	return Result::Succeeded;
}

const Unit* MapTile::TryGetUnit() const noexcept {
	return m_spUnit.get();
}

Unit* MapTile::TryGetUnit() noexcept {
	return m_spUnit.get();
}

Unit* MapTile::SpDetachUnit() noexcept {
	return m_spUnit.release();
}

Result MapTile::TryDestroyUnit() noexcept {
	--m_spUnit->m_owner->m_unitsCached;
	m_spUnit.reset();
	return Result::Succeeded;
}
//
//MapTile::MapTile(): m_nFileID(-1), m_terrain(GetTerrainInfo(Terrain::Type::Plain)){
//}

MapTile::MapTile(MapTile&& maptile):m_pterrain(maptile.m_pterrain), m_nFileID(maptile.m_nFileID){
	m_spUnit = std::move(maptile.m_spUnit);
	m_spPropertyInfo = std::move(maptile.m_spPropertyInfo);
}

MapTile MapTile::Clone(const Player* pNewPropertyOwner, const Player* pNewUnitOwner) const {
	MapTile clone(*m_pterrain, m_nFileID);
	if (m_spUnit != nullptr) {
		clone.m_spUnit.reset(m_spUnit->Clone(pNewUnitOwner));
	}

	if (m_spPropertyInfo != nullptr) {
		clone.m_spPropertyInfo.reset(new PropertyInfo());
		clone.m_spPropertyInfo->m_capturePoints = m_spPropertyInfo->m_capturePoints;
		clone.m_spPropertyInfo->m_owner = pNewPropertyOwner;
	}

	return clone;
}

TerrainFileID toTerrainFileId(const MapTile& maptile, const PropertyInfo* pproperty) {
	const Terrain::Type& type = maptile.m_pterrain->m_type;
	if (MapTile::IsProperty(type) && pproperty == nullptr) {
		throw;
	}

	switch (type) {
	case Terrain::Type::Airport:
		if (pproperty->m_owner == nullptr) {
			return TerrainFileID::NeutralAirport;
		}

		switch (pproperty->m_owner->m_armyType) {
		case Player::ArmyType::BlueMoon:
			return TerrainFileID::BlueMoonAirport;
		case Player::ArmyType::OrangeStar:
			return TerrainFileID::OrangeStarAirport;
		case Player::ArmyType::Invalid:
			throw;
		}
	case Terrain::Type::Base:
		if (pproperty->m_owner == nullptr) {
			return TerrainFileID::NeutralBase;
		}

		switch (pproperty->m_owner->m_armyType) {
		case Player::ArmyType::BlueMoon:
			return TerrainFileID::BlueMoonBase;
		case Player::ArmyType::OrangeStar:
			return TerrainFileID::OrangeStarBase;
		case Player::ArmyType::Invalid:
			throw;
		}
	case Terrain::Type::City:
		if (pproperty->m_owner == nullptr) {
			return TerrainFileID::NeutralCity;
		}

		switch (pproperty->m_owner->m_armyType) {
		case Player::ArmyType::BlueMoon:
			return TerrainFileID::BlueMoonCity;
		case Player::ArmyType::OrangeStar:
			return TerrainFileID::OrangeStarCity;
		case Player::ArmyType::Invalid:
			throw;
		}
	case Terrain::Type::ComTower:
		if (pproperty->m_owner == nullptr) {
			return TerrainFileID::NeutralComTower;
		}
		switch (pproperty->m_owner->m_armyType) {
		case Player::ArmyType::BlueMoon:
			return TerrainFileID::BlueMoonComTower;
		case Player::ArmyType::OrangeStar:
			return TerrainFileID::OrangeStarComTower;
		case Player::ArmyType::Invalid:
			throw;
		}
	case Terrain::Type::Headquarters:
		switch (pproperty->m_owner->m_armyType) {
		case Player::ArmyType::BlueMoon:
			return TerrainFileID::BlueMoonHeadquarters;
		case Player::ArmyType::OrangeStar:
			return TerrainFileID::OrangeStarHeadquarters;
		case Player::ArmyType::Invalid:
			throw;
		}
	case Terrain::Type::Lab:
		if (pproperty->m_owner == nullptr) {
			return TerrainFileID::NeutralLab;
		}
		switch (pproperty->m_owner->m_armyType) {
		case Player::ArmyType::BlueMoon:
			return TerrainFileID::BlueMoonLab;
		case Player::ArmyType::OrangeStar:
			return TerrainFileID::OrangeStarLab;
		case Player::ArmyType::Invalid:
			throw;
		}
	case Terrain::Type::Port:
		if (pproperty->m_owner == nullptr) {
			return TerrainFileID::NeutralPort;
		}
		switch (pproperty->m_owner->m_armyType) {
		case Player::ArmyType::BlueMoon:
			return TerrainFileID::BlueMoonPort;
		case Player::ArmyType::OrangeStar:
			return TerrainFileID::OrangeStarPort;
		case Player::ArmyType::Invalid:
			throw;
		}
	default:
		return static_cast<TerrainFileID>(maptile.m_nFileID);
	}
}


void to_json(json& j, const MapTile& maptile) {
	j = { {"terrain", toTerrainFileId(maptile, maptile.m_spPropertyInfo.get())} };

	if (maptile.m_spUnit) {
		j["unit"] = *maptile.m_spUnit;
	}

	if (maptile.m_spPropertyInfo.get() != nullptr) {
		j["property"] = *maptile.m_spPropertyInfo;
	}
}

void from_json(const std::array<Player, 2>& arrPlayers, json& j, MapTile& maptile) {
	j.at("terrain").get_to(const_cast<int&>(maptile.m_nFileID));
	maptile.m_pterrain = &GetTerrainInfo(MapParser::ToTerrainType(static_cast<TerrainFileID>(maptile.m_nFileID)));

	if (j.contains("unit")) {
		maptile.m_spUnit.reset(new Unit());
		from_json(arrPlayers, j.at("unit"), *maptile.m_spUnit);
	}

	if (j.contains("property")) {
		maptile.m_spPropertyInfo.reset(new PropertyInfo());
		from_json(arrPlayers, j.at("property"), *maptile.m_spPropertyInfo);
	}
}

void to_json(json& j, const PropertyInfo& propertyInfo) {
	j = {
			{"capture-points", propertyInfo.m_capturePoints},
			{"owner", propertyInfo.m_owner != nullptr ? propertyInfo.m_owner->getArmyTypeJson() : "neutral"}
		};
}

void from_json(const std::array<Player, 2>& arrPlayers, json& j, PropertyInfo& propertyInfo) {
	j.at("capture-points").get_to(propertyInfo.m_capturePoints);
	std::string armyType;
	j.at("owner").get_to(armyType);

	if (arrPlayers[0].m_armyType == Player::armyTypefromString(armyType)) {
		propertyInfo.m_owner = &arrPlayers[0];
	}
	else if (arrPlayers[1].m_armyType == Player::armyTypefromString(armyType)) {
		propertyInfo.m_owner = &arrPlayers[1];
	}
}
