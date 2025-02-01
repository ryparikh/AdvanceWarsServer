#include "Player.h"

PowerMeter::PowerMeter(const CommandingOfficier::Type& type) noexcept {
	switch (type) {
	case CommandingOfficier::Type::Adder:
		m_nCopStars = 2;
		m_nScopStars = 3;
		break;
	case CommandingOfficier::Type::Andy:
		m_nCopStars = 3;
		m_nScopStars = 3;
		break;
	default:
		throw;
	}
}

void PowerMeter::AddCharge(int charge) noexcept {
	m_nCharge = std::min(charge + m_nCharge, GetTotalCharge());
}

void PowerMeter::UseScop() noexcept {
	m_nCharge -= m_nScopStars * m_nStarValue;
	IncreaseStarCost();
}

void PowerMeter::UseCop() noexcept {
	m_nCharge -= m_nCopStars * m_nStarValue;
	IncreaseStarCost();
}

void PowerMeter::IncreaseStarCost() noexcept {
	m_nStarValue = std::min(m_nStarValue * 1.2, 55720.0);
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
