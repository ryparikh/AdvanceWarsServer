#include "Player.h"

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
			{ "co", player.m_co},
			{"funds", player.m_funds},
			{"armyType", player.getArmyTypeJson()} 
	};
}
