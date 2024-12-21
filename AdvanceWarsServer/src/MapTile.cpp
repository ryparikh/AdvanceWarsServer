#include <MapTile.h>

/*static*/ bool MapTile::IsProperty(Terrain::Type type) {
	return type == Terrain::Type::Airport ||
		type == Terrain::Type::Base ||
		type == Terrain::Type::City ||
		type == Terrain::Type::ComTower ||
		type == Terrain::Type::Headquarters ||
		type == Terrain::Type::Lab ||
		type == Terrain::Type::Port;
}

MapTile::MapTile(const Terrain& terrain)
	: m_terrain(terrain) {
	if (IsProperty(terrain.m_type)) {
		m_spPropertyInfo.reset(new PropertyInfo());
	}
}

MapTile::MapTile(Terrain&& terrain)
	: m_terrain(std::move(terrain)) {
}

Result MapTile::Capture(const Player* owner) {
	if (!IsProperty(m_terrain.m_type)) {
		return Result::Failed;
	}

	// Should I just reallocate instead of failing?
	if (m_spPropertyInfo == nullptr) {
		return Result::Failed;
	}

	m_spPropertyInfo->m_owner = owner;
	return Result::Succeeded;
}

Result MapTile::TryAddUnit(const UnitProperties::Type& type, const Player* player) noexcept {
	m_spUnit.reset(new Unit(type, player));
	return Result::Succeeded;
}

const Unit* MapTile::TryGetUnit() const noexcept {
	return m_spUnit.get();
}

Unit* MapTile::TryGetUnit() noexcept {
	return m_spUnit.get();
}

TerrainFileID toTerrainFileId(const Terrain::Type& type, const PropertyInfo* pproperty) {
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
	case Terrain::Type::Bridge:
		return TerrainFileID::HBridge;
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
	case Terrain::Type::Forest:
		return TerrainFileID::Forest;
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
	case Terrain::Type::Mountain:
		return TerrainFileID::Mountain;
	case Terrain::Type::MissleSilo:
		return TerrainFileID::MissileSiloEmpty;
	case Terrain::Type::Pipe:
		return TerrainFileID::VPipe;
	case Terrain::Type::Plain:
		return TerrainFileID::Plain;
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
	case Terrain::Type::Reef:
		return TerrainFileID::Reef;
	case Terrain::Type::River:
		return TerrainFileID::HRiver;
	case Terrain::Type::Road:
		return TerrainFileID::HRoad;
	case Terrain::Type::Sea:
		return TerrainFileID::Sea;
	case Terrain::Type::Shoal:
		return TerrainFileID::HShoal;
	}

	return TerrainFileID::Invalid;
}


void to_json(json& j, const MapTile& maptile) {
	j = { {"terrain", toTerrainFileId(maptile.m_terrain.m_type, maptile.m_spPropertyInfo.get())} };

	if (maptile.m_spUnit) {
		j["unit"] = *maptile.m_spUnit;
	}
}
