#pragma once

#include "nlohmann/json.hpp"
using json = nlohmann::json;

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
