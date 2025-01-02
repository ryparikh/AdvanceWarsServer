#pragma once
#include "CommandingOfficier.h"
class Player final {
public:
	enum class ArmyType : int {
		Invalid = -1,
		OrangeStar = 1,
		BlueMoon = 2,
	};

	Player(CommandingOfficier::Type type, ArmyType army) : m_co{ type }, m_armyType(army) {}
	int PowerStatus() const {
		return m_powerStatus;
	}
	int m_funds{ 0 };
	CommandingOfficier m_co{ CommandingOfficier::Type::Invalid };
	// This indexes into the damage charts during damage calculations
	int m_powerStatus{ 0 };
	ArmyType m_armyType{ ArmyType::Invalid };
	std::string getArmyTypeJson() const;
};

void to_json(json& j, const Player& gameState);
