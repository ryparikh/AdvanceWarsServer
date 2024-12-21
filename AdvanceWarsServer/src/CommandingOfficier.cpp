#include "CommandingOfficier.h"
std::string CommandingOfficier::to_string() const {
		switch (m_type) {
		case Type::Adder:
			return "Adder";
		case Type::Andy:
			return "Andy";
		case Type::Colin:
			return "Colin";
		case Type::Drake:
			return "Drake";
		case Type::Eagle:
			return "Eagle";
		case Type::Flak:
			return "Flak";
		case Type::Grimm:
			return "Grimm";
		case Type::Grit:
			return "Grit";
		case Type::Hachi:
			return "Hachi";
		case Type::Hawke:
			return "Hawke";
		case Type::Jake:
			return "Jake";
		case Type::Javier:
			return "Javier";
		case Type::Jess:
			return "Jess";
		case Type::Jugger:
			return "Jugger";
		case Type::Kanbei:
			return "Kanbei";
		case Type::Kindle:
			return "Kindle";
		case Type::Koal:
			return "Koal";
		case Type::Lash:
			return "Lash";
		case Type::Max:
			return "Max";
		case Type::Nell:
			return "Nell";
		case Type::Olaf:
			return "Olaf";
		case Type::Rachel:
			return "Rachel";
		case Type::Sami:
			return "Sami";
		case Type::Sasha:
			return "Sasha";
		case Type::Sensei:
			return "Sensei";
		case Type::Sonja:
			return "Sonja";
		case Type::Sturm:
			return "Sturm";
		case Type::VonBolt:
			return "VonBolt";
		default:
			return "";
		}
	}
void to_json(json& j, const CommandingOfficier& co) {
	j = co.to_string();
}