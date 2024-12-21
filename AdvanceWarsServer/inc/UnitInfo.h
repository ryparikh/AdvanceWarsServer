#pragma once

#include <array>

#include "Unit.h"

// Collection of terrain information for different terrain types.
using UnitInfo = std::array<const UnitProperties, static_cast<int>(UnitProperties::Type::Size)>;

extern const UnitInfo vrgUnits;

static const UnitProperties& GetUnitInfo(UnitProperties::Type unitType) noexcept
{
	return vrgUnits[static_cast<int>(unitType)];
}

extern const std::array<std::array<int, static_cast<int>(UnitProperties::Type::Size)>, static_cast<int>(UnitProperties::Type::Size)> vrgPrimaryWeaponDamage;
extern const std::array<std::array<int, static_cast<int>(UnitProperties::Type::Size)>, static_cast<int>(UnitProperties::Type::Size)> vrgSecondaryWeaponDamage;
