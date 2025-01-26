#pragma once
#include "CommandingOfficier.h"

class PowerMeter final {
public:
	PowerMeter() {}
	PowerMeter(const CommandingOfficier::Type& type) noexcept;
	void AddCharge(int charge) noexcept;
	bool FCopCharged() const noexcept {
		return m_nCharge > m_nCopStars * m_nStarValue;
	}

	bool FScopCharged() const noexcept {
		return m_nCharge == GetTotalCharge();
	}

	void UseCop() noexcept;
	void UseScop() noexcept;
	inline int GetCharge() const noexcept {
		return m_nCharge;
	}

	inline int GetTotalCharge() const noexcept {
		return (m_nCopStars + m_nScopStars) * m_nStarValue;
	}

	static void to_json(json& j, const PowerMeter& powerMeter);
	static void from_json(json& j, PowerMeter& powerMeter);

private:
	void IncreaseStarCost() noexcept;

private:
	int m_nCopStars{ -1 };
	int m_nScopStars{ -1 };
	int m_nCharge{ 0 };
	int m_nStarValue{ 9000 };
};

class Player final {
public:
	enum class ArmyType : int {
		Invalid = -1,
		OrangeStar = 1,
		BlueMoon = 2,
	};

	Player() {}
	Player(CommandingOfficier::Type type, ArmyType army) : m_co{ type }, m_armyType(army), m_powerMeter{ type } {}
	int PowerStatus() const {
		return m_powerStatus;
	}
	void SetPowerStatus(int status) {
		m_powerStatus = status;
	}

	int m_funds{ 0 };
	CommandingOfficier m_co{ CommandingOfficier::Type::Invalid };
	PowerMeter m_powerMeter;
	// This indexes into the damage charts during damage calculations
	int m_powerStatus{ 0 };
	ArmyType m_armyType{ ArmyType::Invalid };
	std::string getArmyTypeJson() const;
	static ArmyType armyTypefromString(const std::string& strTypename);
	mutable int m_unitsCached = 0;
};

void to_json(json& j, const Player& player);
void from_json(json& j, Player& player);

