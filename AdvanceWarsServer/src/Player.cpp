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

void to_json(json& j, const Player& player) {
	j = { 
			{"co", player.m_co},
			{"power-meter", {player.m_powerMeter.GetCharge(), player.m_powerMeter.GetTotalCharge()}},
			{"power-status", player.m_powerStatus},
			{"funds", player.m_funds},
			{"armyType", player.getArmyTypeJson()}
	};
}
