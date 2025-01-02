#pragma once

#include "UnitInfo.h"
#include "nlohmann/json.hpp"
using json = nlohmann::json;


// TODO: CO Power bonuses
// CO Ready for Play:
// Adder, Andy
struct CommandingOfficier {
	enum class Type {
		Invalid = -1,
		Adder,
		Andy,
		Colin,
		Drake,
		Eagle,
		Flak,
		Grimm,
		Grit,
		Hachi,
		Hawke,
		Jake,
		Javier,
		Jess,
		Jugger,
		Kanbei,
		Kindle,
		Koal,
		Lash,
		Max,
		Nell,
		Olaf,
		Rachel,
		Sami,
		Sasha,
		Sensei,
		Sonja,
		Sturm,
		VonBolt,
		Size
	};

	std::string to_string() const;
	Type m_type;
};

void to_json(json& j, const CommandingOfficier& co);

using DamageChart = std::array<std::pair<int, int>, static_cast<int>(UnitProperties::Type::Size)>;
using DamageCharts = std::array<DamageChart, 3>;
extern const std::array<DamageCharts, static_cast<int>(CommandingOfficier::Type::Size)> rgCharts;

