#include "Player.h"

#include <stdexcept>

PowerMeter::PowerMeter(const CommandingOfficier::Type& type) {
	switch (type) {
	case CommandingOfficier::Type::Adder:
		m_nCopStars = 2;
		m_nScopStars = 3;
		break;
	case CommandingOfficier::Type::Andy:
		m_nCopStars = 3;
		m_nScopStars = 3;
		break;
	case CommandingOfficier::Type::Colin:
		m_nCopStars = 2;
		m_nScopStars = 4;
		break;
	case CommandingOfficier::Type::Drake:
		m_nCopStars = 4;
		m_nScopStars = 3;
		break;
	case CommandingOfficier::Type::Eagle:
		m_nCopStars = 3;
		m_nScopStars = 6;
		break;
	case CommandingOfficier::Type::Flak:
	case CommandingOfficier::Type::Grimm:
	case CommandingOfficier::Type::Grit:
	case CommandingOfficier::Type::Jake:
	case CommandingOfficier::Type::Javier:
	case CommandingOfficier::Type::Jess:
	case CommandingOfficier::Type::Kindle:
	case CommandingOfficier::Type::Max:
	case CommandingOfficier::Type::Nell:
	case CommandingOfficier::Type::Rachel:
		m_nCopStars = 3;
		m_nScopStars = 3;
		break;
	case CommandingOfficier::Type::Hachi:
	case CommandingOfficier::Type::Koal:
	case CommandingOfficier::Type::Sonja:
		m_nCopStars = 3;
		m_nScopStars = 2;
		break;
	case CommandingOfficier::Type::Hawke:
		m_nCopStars = 5;
		m_nScopStars = 4;
		break;
	case CommandingOfficier::Type::Jugger:
	case CommandingOfficier::Type::Olaf:
		m_nCopStars = 3;
		m_nScopStars = 4;
		break;
	case CommandingOfficier::Type::Kanbei:
		m_nCopStars = 4;
		m_nScopStars = 3;
		break;
	case CommandingOfficier::Type::Lash:
		m_nCopStars = 4;
		m_nScopStars = 3;
		break;
	case CommandingOfficier::Type::Sami:
		m_nCopStars = 3;
		m_nScopStars = 5;
		break;
	case CommandingOfficier::Type::Sasha:
	case CommandingOfficier::Type::Sensei:
		m_nCopStars = 2;
		m_nScopStars = 4;
		break;
	case CommandingOfficier::Type::Sturm:
		m_nCopStars = 5;
		m_nScopStars = 5;
		break;
	case CommandingOfficier::Type::VonBolt:
		m_nCopStars = 0;
		m_nScopStars = 10;
		break;
	default:
		throw std::invalid_argument("Unknown commanding officer power meter");
	}
}

void PowerMeter::AddCharge(int charge) noexcept {
	m_nCharge = std::min(charge + m_nCharge, GetTotalCharge());
}

void PowerMeter::UseScop() noexcept {
	m_nCharge = 0;
	IncreaseStarCost();
}

void PowerMeter::UseCop() noexcept {
	m_nCharge -= m_nCopStars * m_nStarValue;
	IncreaseStarCost();
}

void PowerMeter::IncreaseStarCost() noexcept {
	m_nStarValue = std::min(static_cast<int>(m_nStarValue * 1.2), 55720);
}

std::string Player::getArmyTypeJson() const {
	switch (m_armyType) {
	case ArmyType::OrangeStar:
		return "orange-star";
	case ArmyType::BlueMoon:
		return "blue-moon";
	default:
		return "";
	}
}

/*static*/ Player::ArmyType Player::armyTypefromString(const std::string& strTypename) {
	if (strTypename == "orange-star") {
		return ArmyType::OrangeStar;
	}

	if (strTypename == "blue-moon") {
		return ArmyType::BlueMoon;
	}

	return ArmyType::Invalid;
}

void to_json(json& j, const Player& player) {
	json power_meter;
	PowerMeter::to_json(power_meter, player.m_powerMeter);
	j = { 
			{"co", player.m_co},
			{"power-meter", power_meter},
			{"power-status", player.m_powerStatus},
			{"funds", player.m_funds},
			{"armyType", player.getArmyTypeJson()},
			{"luck-policy", player.m_luckPolicy }
	};
}

void from_json(json& j, Player& player) {
	from_json(j.at("co"), player.m_co);
	j.at("funds").get_to(player.m_funds);

	std::string armyType;
	j.at("armyType").get_to(armyType);
	player.m_armyType = Player::armyTypefromString(armyType);

	j.at("power-status").get_to(player.m_powerStatus);

	PowerMeter::from_json(j.at("power-meter"), player.m_powerMeter);

	j.at("luck-policy").get_to(player.m_luckPolicy);
}

/*static*/ void PowerMeter::to_json(json& j, const PowerMeter& powerMeter) {
	j = {
		{"cop-stars", powerMeter.m_nCopStars},
		{"scop-stars", powerMeter.m_nScopStars},
		{"charge", powerMeter.m_nCharge},
		{"star-value", powerMeter.m_nStarValue},
	};
}

/*static*/ void PowerMeter::from_json(json& j, PowerMeter& powerMeter) {
	j.at("cop-stars").get_to(powerMeter.m_nCopStars);
	j.at("scop-stars").get_to(powerMeter.m_nScopStars);
	j.at("charge").get_to(powerMeter.m_nCharge);
	j.at("star-value").get_to(powerMeter.m_nStarValue);
}
