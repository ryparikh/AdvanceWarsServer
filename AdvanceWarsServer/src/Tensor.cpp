#include "StateTensor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cctype>
#include <cstring>
#include <iterator>
#include <limits>
#include <string>
#include <vector>

#include "MapTile.h"
#include "TerrainInfo.h"
#include "UnitInfo.h"

namespace {
constexpr int kBoardWidth = 27;
constexpr int kBoardHeight = 23;
constexpr int kCellCount = kBoardWidth * kBoardHeight;
constexpr int kTerrainCount = static_cast<int>(Terrain::Type::Size);
constexpr int kUnitTypeCount = static_cast<int>(UnitProperties::Type::Size);
constexpr int kCoCount = static_cast<int>(CommandingOfficier::Type::Size);

constexpr int kInBoundsChannel = 0;
constexpr int kTerrainStart = kInBoundsChannel + 1;
constexpr int kPropertyStart = kTerrainStart + kTerrainCount;
constexpr int kSelfUnitStart = kPropertyStart + 4;
constexpr int kOpponentUnitStart = kSelfUnitStart + kUnitTypeCount;
constexpr int kUnitAttributeStart = kOpponentUnitStart + kUnitTypeCount;
constexpr int kLoadedStart = kUnitAttributeStart + 13;
constexpr int kSelfLoadedTypeStart = kLoadedStart + 2;
constexpr int kOpponentLoadedTypeStart = kSelfLoadedTypeStart + kUnitTypeCount;
constexpr int kSelfLoadedHealth = kOpponentLoadedTypeStart + kUnitTypeCount;
constexpr int kOpponentLoadedHealth = kSelfLoadedHealth + 1;
constexpr int kEconomyStart = kOpponentLoadedHealth + 1;
constexpr int kSelfCoStart = kEconomyStart + 10;
constexpr int kOpponentCoStart = kSelfCoStart + kCoCount;
constexpr int kPowerStart = kOpponentCoStart + kCoCount;
constexpr int kTurnWeatherStart = kPowerStart + 8;
constexpr int kChannelCount = kTurnWeatherStart + 5;
static_assert(kChannelCount == 219, "Unexpected state tensor channel count");

enum class RelativeOwner {
	Self,
	Opponent,
	Neutral,
	Invalid,
};

struct PlayerContext {
	int activePlayer = 0;
	int opponentPlayer = 1;
	const Player* self = nullptr;
	const Player* opponent = nullptr;
};

struct UnitAggregate {
	int count = 0;
	float armyValue = 0.0f;
};

float Clamp01(float value) noexcept {
	if (!std::isfinite(value)) {
		return 0.0f;
	}

	return std::max(0.0f, std::min(1.0f, value));
}

float Normalize(float value, float denominator) noexcept {
	if (denominator <= 0.0f) {
		return 0.0f;
	}

	return Clamp01(value / denominator);
}

int DisplayHpPips(int health) noexcept {
	if (health <= 0) {
		return 0;
	}

	return std::min(10, (health + 9) / 10);
}

float DisplayHealthValue(const Unit& unit) noexcept {
	return Normalize(static_cast<float>(DisplayHpPips(unit.health)), 10.0f);
}

float UnitArmyValue(const Unit& unit) noexcept {
	return static_cast<float>(DisplayHpPips(unit.health)) * static_cast<float>(Unit::GetUnitCost(unit.m_properties.m_type)) / 10.0f;
}

bool IsValidTerrainType(Terrain::Type type) noexcept {
	const int terrainIndex = static_cast<int>(type);
	return terrainIndex >= 0 && terrainIndex < kTerrainCount;
}

bool IsValidUnitType(UnitProperties::Type type) noexcept {
	const int unitIndex = static_cast<int>(type);
	return unitIndex >= 0 && unitIndex < kUnitTypeCount;
}

bool IsValidCoType(CommandingOfficier::Type type) noexcept {
	const int coIndex = static_cast<int>(type);
	return coIndex >= 0 && coIndex < kCoCount;
}

const char* TerrainName(Terrain::Type type) noexcept {
	switch (type) {
	case Terrain::Type::Plain:
		return "plain";
	case Terrain::Type::Mountain:
		return "mountain";
	case Terrain::Type::Forest:
		return "forest";
	case Terrain::Type::River:
		return "river";
	case Terrain::Type::Road:
		return "road";
	case Terrain::Type::Bridge:
		return "bridge";
	case Terrain::Type::Sea:
		return "sea";
	case Terrain::Type::Shoal:
		return "shoal";
	case Terrain::Type::Reef:
		return "reef";
	case Terrain::Type::City:
		return "city";
	case Terrain::Type::Base:
		return "base";
	case Terrain::Type::Airport:
		return "airport";
	case Terrain::Type::Port:
		return "port";
	case Terrain::Type::Headquarters:
		return "headquarters";
	case Terrain::Type::Pipe:
		return "pipe";
	case Terrain::Type::MissleSilo:
		return "missile-silo";
	case Terrain::Type::ComTower:
		return "com-tower";
	case Terrain::Type::Lab:
		return "lab";
	default:
		return "";
	}
}

std::string CoName(CommandingOfficier::Type type) {
	CommandingOfficier co;
	co.m_type = type;
	std::string name = co.to_string();
	std::transform(name.begin(), name.end(), name.begin(), [](unsigned char ch) {
		return static_cast<char>(std::tolower(ch));
	});
	return name;
}

std::vector<std::string> BuildChannelNames() {
	std::vector<std::string> channels;
	channels.reserve(kChannelCount);

	channels.emplace_back("in-bounds");

	for (int i = 0; i < kTerrainCount; ++i) {
		channels.emplace_back(std::string("terrain/") + TerrainName(static_cast<Terrain::Type>(i)));
	}

	channels.emplace_back("property-self");
	channels.emplace_back("property-opponent");
	channels.emplace_back("property-neutral");
	channels.emplace_back("property-capture-points");

	for (int i = 0; i < kUnitTypeCount; ++i) {
		channels.emplace_back(std::string("self-unit/") + UnitProperties::getTypename(static_cast<UnitProperties::Type>(i)));
	}

	for (int i = 0; i < kUnitTypeCount; ++i) {
		channels.emplace_back(std::string("opponent-unit/") + UnitProperties::getTypename(static_cast<UnitProperties::Type>(i)));
	}

	const std::array<const char*, 13> unitAttributes{ {
		"self-health",
		"self-ammo",
		"self-fuel",
		"self-moved",
		"self-hidden",
		"self-stunned",
		"opponent-health",
		"opponent-ammo",
		"opponent-fuel",
		"opponent-moved",
		"opponent-hidden",
		"opponent-stunned",
		"opponent-health-known",
	} };

	for (const char* attribute : unitAttributes) {
		channels.emplace_back(attribute);
	}

	channels.emplace_back("self-loaded-count");
	channels.emplace_back("opponent-loaded-count");

	for (int i = 0; i < kUnitTypeCount; ++i) {
		channels.emplace_back(std::string("self-loaded/") + UnitProperties::getTypename(static_cast<UnitProperties::Type>(i)));
	}

	for (int i = 0; i < kUnitTypeCount; ++i) {
		channels.emplace_back(std::string("opponent-loaded/") + UnitProperties::getTypename(static_cast<UnitProperties::Type>(i)));
	}

	channels.emplace_back("self-loaded-health");
	channels.emplace_back("opponent-loaded-health");

	const std::array<const char*, 10> economyChannels{ {
		"self-funds",
		"opponent-funds",
		"self-income",
		"opponent-income",
		"self-unit-count",
		"opponent-unit-count",
		"self-army-value",
		"opponent-army-value",
		"self-capture-progress",
		"opponent-capture-progress",
	} };

	for (const char* channel : economyChannels) {
		channels.emplace_back(channel);
	}

	for (int i = 0; i < kCoCount; ++i) {
		channels.emplace_back(std::string("self-co/") + CoName(static_cast<CommandingOfficier::Type>(i)));
	}

	for (int i = 0; i < kCoCount; ++i) {
		channels.emplace_back(std::string("opponent-co/") + CoName(static_cast<CommandingOfficier::Type>(i)));
	}

	const std::array<const char*, 13> finalChannels{ {
		"self-power-normal",
		"self-power-cop-active",
		"self-power-scop-active",
		"self-power-charge",
		"opponent-power-normal",
		"opponent-power-cop-active",
		"opponent-power-scop-active",
		"opponent-power-charge",
		"turn-count",
		"weather-clear",
		"weather-rain",
		"weather-snow",
		"weather-turns-remaining",
	} };

	for (const char* channel : finalChannels) {
		channels.emplace_back(channel);
	}

	return channels;
}

RelativeOwner GetRelativeOwner(const Player* owner, const PlayerContext& context, bool neutralAllowed) noexcept {
	if (owner == nullptr) {
		return neutralAllowed ? RelativeOwner::Neutral : RelativeOwner::Invalid;
	}

	if (owner == context.self) {
		return RelativeOwner::Self;
	}

	if (owner == context.opponent) {
		return RelativeOwner::Opponent;
	}

	return RelativeOwner::Invalid;
}

bool TryGetTile(const Map& map, int x, int y, const MapTile** tile) noexcept {
	if (x < 0 || y < 0 || x >= static_cast<int>(map.GetCols()) || y >= static_cast<int>(map.GetRows())) {
		return false;
	}

	return map.TryGetTile(static_cast<unsigned int>(x), static_cast<unsigned int>(y), tile) == Result::Succeeded && *tile != nullptr;
}

bool OwnUnitAdjacent(const Map& map, int x, int y, const PlayerContext& context) noexcept {
	const std::array<std::pair<int, int>, 4> offsets{ {
		{ 0, -1 },
		{ 1, 0 },
		{ 0, 1 },
		{ -1, 0 },
	} };

	for (const auto& offset : offsets) {
		const MapTile* tile = nullptr;
		if (!TryGetTile(map, x + offset.first, y + offset.second, &tile)) {
			continue;
		}

		const Unit* unit = tile->TryGetUnit();
		if (unit != nullptr && unit->m_owner == context.self) {
			return true;
		}
	}

	return false;
}

bool OnOwnProperty(const MapTile& tile, const PlayerContext& context) noexcept {
	return tile.m_spPropertyInfo != nullptr && tile.m_spPropertyInfo->m_owner == context.self;
}

bool IsRedactedHiddenUnit(const Map& map, const MapTile& tile, const Unit& unit, int x, int y, RelativeOwner owner, const PlayerContext& context) noexcept {
	if (owner != RelativeOwner::Opponent || !unit.m_hidden) {
		return false;
	}

	const UnitProperties::Type type = unit.m_properties.m_type;
	if (type != UnitProperties::Type::Stealth && type != UnitProperties::Type::Sub) {
		return false;
	}

	return !OnOwnProperty(tile, context) && !OwnUnitAdjacent(map, x, y, context);
}

bool HealthKnown(RelativeOwner owner, const PlayerContext& context) noexcept {
	return owner != RelativeOwner::Opponent || context.opponent->m_co.m_type != CommandingOfficier::Type::Sonja;
}

int CellIndex(int x, int y) noexcept {
	return y * kBoardWidth + x;
}

void SetValue(std::vector<float>& values, int channel, int x, int y, float value) noexcept {
	values[channel * kCellCount + CellIndex(x, y)] = Clamp01(value);
}

void Broadcast(std::vector<float>& values, const Map& map, int channel, float value) noexcept {
	for (int y = 0; y < static_cast<int>(map.GetRows()); ++y) {
		for (int x = 0; x < static_cast<int>(map.GetCols()); ++x) {
			SetValue(values, channel, x, y, value);
		}
	}
}

void AddUnitAggregate(UnitAggregate& aggregate, const Unit& unit, bool healthKnown) noexcept {
	++aggregate.count;
	if (healthKnown) {
		aggregate.armyValue += UnitArmyValue(unit);
	}
}

int UnitTypeChannel(RelativeOwner owner, UnitProperties::Type type) noexcept {
	const int unitIndex = static_cast<int>(type);
	return (owner == RelativeOwner::Self ? kSelfUnitStart : kOpponentUnitStart) + unitIndex;
}

int LoadedTypeChannel(RelativeOwner owner, UnitProperties::Type type) noexcept {
	const int unitIndex = static_cast<int>(type);
	return (owner == RelativeOwner::Self ? kSelfLoadedTypeStart : kOpponentLoadedTypeStart) + unitIndex;
}

void SetUnitAttributes(std::vector<float>& values, const Unit& unit, RelativeOwner owner, bool healthKnown, int x, int y) noexcept {
	const UnitProperties& maxProperties = GetUnitInfo(unit.m_properties.m_type);
	const int start = kUnitAttributeStart + (owner == RelativeOwner::Self ? 0 : 6);

	SetValue(values, start + 0, x, y, healthKnown ? DisplayHealthValue(unit) : 0.0f);
	SetValue(values, start + 1, x, y, Normalize(static_cast<float>(unit.m_properties.m_ammo), static_cast<float>(maxProperties.m_ammo)));
	SetValue(values, start + 2, x, y, Normalize(static_cast<float>(unit.m_properties.m_fuel), static_cast<float>(maxProperties.m_fuel)));
	SetValue(values, start + 3, x, y, unit.m_moved ? 1.0f : 0.0f);
	SetValue(values, start + 4, x, y, unit.m_hidden ? 1.0f : 0.0f);
	SetValue(values, start + 5, x, y, unit.m_stunned ? 1.0f : 0.0f);

	if (owner == RelativeOwner::Opponent) {
		SetValue(values, kUnitAttributeStart + 12, x, y, healthKnown ? 1.0f : 0.0f);
	}
}

Result EncodeLoadedUnits(std::vector<float>& values, const Unit& transport, const PlayerContext& context, UnitAggregate& selfAggregate, UnitAggregate& opponentAggregate, int x, int y) noexcept {
	std::array<int, kUnitTypeCount> selfLoadedTypes{};
	std::array<int, kUnitTypeCount> opponentLoadedTypes{};
	int selfLoadedCount = 0;
	int opponentLoadedCount = 0;
	float selfLoadedHealth = 0.0f;
	float opponentLoadedHealth = 0.0f;

	for (int i = 0; i < transport.CLoadedUnits(); ++i) {
		const Unit* loadedUnit = transport.GetLoadedUnit(i);
		if (loadedUnit == nullptr || !IsValidUnitType(loadedUnit->m_properties.m_type)) {
			return Result::Failed;
		}

		const RelativeOwner owner = GetRelativeOwner(loadedUnit->m_owner, context, false);
		if (owner != RelativeOwner::Self && owner != RelativeOwner::Opponent) {
			return Result::Failed;
		}

		const bool knownHealth = HealthKnown(owner, context);
		if (owner == RelativeOwner::Self) {
			++selfLoadedCount;
			++selfLoadedTypes[static_cast<int>(loadedUnit->m_properties.m_type)];
			if (knownHealth) {
				selfLoadedHealth += static_cast<float>(DisplayHpPips(loadedUnit->health));
			}
			AddUnitAggregate(selfAggregate, *loadedUnit, knownHealth);
		}
		else {
			++opponentLoadedCount;
			++opponentLoadedTypes[static_cast<int>(loadedUnit->m_properties.m_type)];
			if (knownHealth) {
				opponentLoadedHealth += static_cast<float>(DisplayHpPips(loadedUnit->health));
			}
			AddUnitAggregate(opponentAggregate, *loadedUnit, knownHealth);
		}
	}

	if (selfLoadedCount > 0) {
		SetValue(values, kLoadedStart, x, y, Normalize(static_cast<float>(selfLoadedCount), 2.0f));
		SetValue(values, kSelfLoadedHealth, x, y, Normalize(selfLoadedHealth, 20.0f));
		for (int unitType = 0; unitType < kUnitTypeCount; ++unitType) {
			if (selfLoadedTypes[unitType] > 0) {
				SetValue(values, kSelfLoadedTypeStart + unitType, x, y, Normalize(static_cast<float>(selfLoadedTypes[unitType]), 2.0f));
			}
		}
	}

	if (opponentLoadedCount > 0) {
		SetValue(values, kLoadedStart + 1, x, y, Normalize(static_cast<float>(opponentLoadedCount), 2.0f));
		SetValue(values, kOpponentLoadedHealth, x, y, Normalize(opponentLoadedHealth, 20.0f));
		for (int unitType = 0; unitType < kUnitTypeCount; ++unitType) {
			if (opponentLoadedTypes[unitType] > 0) {
				SetValue(values, kOpponentLoadedTypeStart + unitType, x, y, Normalize(static_cast<float>(opponentLoadedTypes[unitType]), 2.0f));
			}
		}
	}

	return Result::Succeeded;
}

Result ValidateTensorValues(const std::vector<float>& values) noexcept {
	for (float value : values) {
		if (!std::isfinite(value) || value < 0.0f || value > 1.0f) {
			return Result::Failed;
		}
	}

	return Result::Succeeded;
}

void HashBytes(std::uint64_t& hash, const void* data, std::size_t size) noexcept {
	const unsigned char* bytes = static_cast<const unsigned char*>(data);
	for (std::size_t i = 0; i < size; ++i) {
		hash ^= static_cast<std::uint64_t>(bytes[i]);
		hash *= 1099511628211ULL;
	}
}

void HashString(std::uint64_t& hash, const std::string& value) noexcept {
	HashBytes(hash, value.data(), value.size());
	const char separator = '\0';
	HashBytes(hash, &separator, sizeof(separator));
}
}

const char* StateTensor::Version() noexcept {
	return "standard-gl-v1-state";
}

int StateTensor::BoardWidth() noexcept {
	return kBoardWidth;
}

int StateTensor::BoardHeight() noexcept {
	return kBoardHeight;
}

int StateTensor::ChannelCount() noexcept {
	return kChannelCount;
}

const std::vector<std::string>& StateTensor::ChannelNames() {
	static const std::vector<std::string> channels = BuildChannelNames();
	return channels;
}

Result StateTensor::ChannelIndex(const std::string& name, int& index) noexcept {
	const std::vector<std::string>& channels = ChannelNames();
	const auto it = std::find(channels.begin(), channels.end(), name);
	if (it == channels.end()) {
		return Result::Failed;
	}

	index = static_cast<int>(std::distance(channels.begin(), it));
	return Result::Succeeded;
}

Result StateTensor::Encode(const GameState& gameState, std::vector<float>& values) noexcept {
	const Map* map = gameState.TryGetMap();
	if (map == nullptr || map->GetCols() > kBoardWidth || map->GetRows() > kBoardHeight) {
		return Result::Failed;
	}

	const int activePlayer = gameState.IsFirstPlayerTurn() ? 0 : 1;
	const int opponentPlayer = activePlayer == 0 ? 1 : 0;
	const std::array<Player, 2>& players = gameState.GetPlayers();
	if (!IsValidCoType(players[activePlayer].m_co.m_type) || !IsValidCoType(players[opponentPlayer].m_co.m_type)) {
		return Result::Failed;
	}

	const PlayerContext context{ activePlayer, opponentPlayer, &players[activePlayer], &players[opponentPlayer] };
	values.assign(static_cast<std::size_t>(kChannelCount * kCellCount), 0.0f);

	UnitAggregate selfAggregate;
	UnitAggregate opponentAggregate;

	for (int y = 0; y < static_cast<int>(map->GetRows()); ++y) {
		for (int x = 0; x < static_cast<int>(map->GetCols()); ++x) {
			const MapTile* tile = nullptr;
			if (!TryGetTile(*map, x, y, &tile)) {
				return Result::Failed;
			}

			SetValue(values, kInBoundsChannel, x, y, 1.0f);

			const Terrain::Type terrainType = tile->GetTerrain().m_type;
			if (!IsValidTerrainType(terrainType)) {
				return Result::Failed;
			}
			SetValue(values, kTerrainStart + static_cast<int>(terrainType), x, y, 1.0f);

			if (MapTile::IsProperty(terrainType)) {
				if (tile->m_spPropertyInfo == nullptr) {
					return Result::Failed;
				}

				const RelativeOwner propertyOwner = GetRelativeOwner(tile->m_spPropertyInfo->m_owner, context, true);
				switch (propertyOwner) {
				case RelativeOwner::Self:
					SetValue(values, kPropertyStart, x, y, 1.0f);
					break;
				case RelativeOwner::Opponent:
					SetValue(values, kPropertyStart + 1, x, y, 1.0f);
					break;
				case RelativeOwner::Neutral:
					SetValue(values, kPropertyStart + 2, x, y, 1.0f);
					break;
				default:
					return Result::Failed;
				}
				SetValue(values, kPropertyStart + 3, x, y, Normalize(static_cast<float>(tile->m_spPropertyInfo->m_capturePoints), 20.0f));
			}

			const Unit* unit = tile->TryGetUnit();
			if (unit == nullptr) {
				continue;
			}

			if (!IsValidUnitType(unit->m_properties.m_type)) {
				return Result::Failed;
			}

			const RelativeOwner unitOwner = GetRelativeOwner(unit->m_owner, context, false);
			if (unitOwner != RelativeOwner::Self && unitOwner != RelativeOwner::Opponent) {
				return Result::Failed;
			}

			if (IsRedactedHiddenUnit(*map, *tile, *unit, x, y, unitOwner, context)) {
				continue;
			}

			const bool knownHealth = HealthKnown(unitOwner, context);
			SetValue(values, UnitTypeChannel(unitOwner, unit->m_properties.m_type), x, y, 1.0f);
			SetUnitAttributes(values, *unit, unitOwner, knownHealth, x, y);
			if (unitOwner == RelativeOwner::Self) {
				AddUnitAggregate(selfAggregate, *unit, knownHealth);
			}
			else {
				AddUnitAggregate(opponentAggregate, *unit, knownHealth);
			}

			IfFailedReturn(EncodeLoadedUnits(values, *unit, context, selfAggregate, opponentAggregate, x, y));
		}
	}

	Broadcast(values, *map, kEconomyStart, Normalize(static_cast<float>(context.self->m_funds), 50000.0f));
	Broadcast(values, *map, kEconomyStart + 1, Normalize(static_cast<float>(context.opponent->m_funds), 50000.0f));
	Broadcast(values, *map, kEconomyStart + 2, Normalize(static_cast<float>(gameState.GetIncomeForPlayer(activePlayer)), 50000.0f));
	Broadcast(values, *map, kEconomyStart + 3, Normalize(static_cast<float>(gameState.GetIncomeForPlayer(opponentPlayer)), 50000.0f));
	Broadcast(values, *map, kEconomyStart + 4, Normalize(static_cast<float>(selfAggregate.count), static_cast<float>(gameState.GetSettings().m_unitCap)));
	Broadcast(values, *map, kEconomyStart + 5, Normalize(static_cast<float>(opponentAggregate.count), static_cast<float>(gameState.GetSettings().m_unitCap)));
	Broadcast(values, *map, kEconomyStart + 6, Normalize(selfAggregate.armyValue, 200000.0f));
	Broadcast(values, *map, kEconomyStart + 7, Normalize(opponentAggregate.armyValue, 200000.0f));
	Broadcast(values, *map, kEconomyStart + 8, Normalize(static_cast<float>(gameState.CountCaptureLimitPropertiesForPlayer(activePlayer)), static_cast<float>(gameState.GetSettings().m_captureLimit)));
	Broadcast(values, *map, kEconomyStart + 9, Normalize(static_cast<float>(gameState.CountCaptureLimitPropertiesForPlayer(opponentPlayer)), static_cast<float>(gameState.GetSettings().m_captureLimit)));

	Broadcast(values, *map, kSelfCoStart + static_cast<int>(context.self->m_co.m_type), 1.0f);
	Broadcast(values, *map, kOpponentCoStart + static_cast<int>(context.opponent->m_co.m_type), 1.0f);

	const int selfPowerStatus = context.self->PowerStatus();
	const int opponentPowerStatus = context.opponent->PowerStatus();
	if (selfPowerStatus < 0 || selfPowerStatus > 2 || opponentPowerStatus < 0 || opponentPowerStatus > 2) {
		return Result::Failed;
	}

	Broadcast(values, *map, kPowerStart + selfPowerStatus, 1.0f);
	Broadcast(values, *map, kPowerStart + 3, Normalize(static_cast<float>(context.self->m_powerMeter.GetCharge()), static_cast<float>(context.self->m_powerMeter.GetScopThreshold())));
	Broadcast(values, *map, kPowerStart + 4 + opponentPowerStatus, 1.0f);
	Broadcast(values, *map, kPowerStart + 7, Normalize(static_cast<float>(context.opponent->m_powerMeter.GetCharge()), static_cast<float>(context.opponent->m_powerMeter.GetScopThreshold())));

	Broadcast(values, *map, kTurnWeatherStart, Normalize(static_cast<float>(gameState.GetTurnCount()), 30.0f));
	switch (gameState.GetWeather()) {
	case GameState::WeatherType::Clear:
		Broadcast(values, *map, kTurnWeatherStart + 1, 1.0f);
		break;
	case GameState::WeatherType::Rain:
		Broadcast(values, *map, kTurnWeatherStart + 2, 1.0f);
		break;
	case GameState::WeatherType::Snow:
		Broadcast(values, *map, kTurnWeatherStart + 3, 1.0f);
		break;
	default:
		return Result::Failed;
	}

	if (gameState.GetWeatherTurnsRemaining().has_value()) {
		Broadcast(values, *map, kTurnWeatherStart + 4, Normalize(static_cast<float>(gameState.GetWeatherTurnsRemaining().value()), 3.0f));
	}

	IfFailedReturn(ValidateTensorValues(values));
	return Result::Succeeded;
}

Result StateTensor::Checksum(const std::vector<float>& values, std::uint64_t& checksum) noexcept {
	if (values.size() != static_cast<std::size_t>(kChannelCount * kCellCount)) {
		return Result::Failed;
	}

	std::uint64_t hash = 14695981039346656037ULL;
	const std::string version = Version();
	HashString(hash, version);

	const std::array<int, 3> shape{ { kChannelCount, kBoardHeight, kBoardWidth } };
	HashBytes(hash, shape.data(), sizeof(int) * shape.size());

	for (const std::string& channel : ChannelNames()) {
		HashString(hash, channel);
	}

	HashBytes(hash, values.data(), sizeof(float) * values.size());
	checksum = hash;
	return Result::Succeeded;
}
