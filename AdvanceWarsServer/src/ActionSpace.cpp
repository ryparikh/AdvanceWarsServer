#include "ActionSpace.h"

namespace {
constexpr int kBoardWidth = 27;
constexpr int kBoardHeight = 23;
constexpr int kCellCount = kBoardWidth * kBoardHeight;

constexpr int DiamondCount(int radius) {
	return 2 * radius * (radius + 1) + 1;
}

constexpr int OffsetCount(int minDistance, int maxDistance) {
	return DiamondCount(maxDistance) - (minDistance <= 0 ? 0 : DiamondCount(minDistance - 1));
}

constexpr int kMoveOffsetRadius = 11;
constexpr int kAttackOffsetMinDistance = 2;
constexpr int kAttackOffsetRadius = 11;
constexpr int kCaptureOffsetRadius = 5;
constexpr int kHideOffsetRadius = 8;

constexpr int kMoveOffsetCount = OffsetCount(0, kMoveOffsetRadius);
constexpr int kAttackOffsetCount = OffsetCount(kAttackOffsetMinDistance, kAttackOffsetRadius);
constexpr int kCaptureOffsetCount = OffsetCount(0, kCaptureOffsetRadius);
constexpr int kHideOffsetCount = OffsetCount(0, kHideOffsetRadius);
constexpr int kDirectionCount = 4;
constexpr int kUnloadIndexCount = 2;
constexpr int kUnitTypeCount = static_cast<int>(UnitProperties::Type::Size);

constexpr int kMoveWaitPlaneStart = 0;
constexpr int kMoveAttackPlaneStart = kMoveWaitPlaneStart + kMoveOffsetCount;
constexpr int kAttackPlaneStart = kMoveAttackPlaneStart + kMoveOffsetCount * kDirectionCount;
constexpr int kMoveCapturePlaneStart = kAttackPlaneStart + kAttackOffsetCount;
constexpr int kMoveCombinePlaneStart = kMoveCapturePlaneStart + kCaptureOffsetCount;
constexpr int kMoveLoadPlaneStart = kMoveCombinePlaneStart + kMoveOffsetCount;
constexpr int kMoveHidePlaneStart = kMoveLoadPlaneStart + kMoveOffsetCount;
constexpr int kMoveUnhidePlaneStart = kMoveHidePlaneStart + kHideOffsetCount;
constexpr int kRepairPlaneStart = kMoveUnhidePlaneStart + kHideOffsetCount;
constexpr int kUnloadPlaneStart = kRepairPlaneStart + kDirectionCount;
constexpr int kBuyPlaneStart = kUnloadPlaneStart + kDirectionCount * kUnloadIndexCount;
constexpr int kCellPlaneCount = kBuyPlaneStart + kUnitTypeCount;
constexpr int kGlobalActionStart = kCellPlaneCount * kCellCount;
constexpr int kActionCount = kGlobalActionStart + 3;

static_assert(kMoveOffsetCount == 265, "Unexpected move offset count");
static_assert(kAttackOffsetCount == 260, "Unexpected attack offset count");
static_assert(kCaptureOffsetCount == 61, "Unexpected capture offset count");
static_assert(kHideOffsetCount == 145, "Unexpected hide offset count");
static_assert(kCellPlaneCount == 2503, "Unexpected action plane count");
static_assert(kActionCount == 1554366, "Unexpected action count");

int Abs(int value) noexcept {
	return value < 0 ? -value : value;
}

bool IsInBounds(int x, int y) noexcept {
	return x >= 0 && x < kBoardWidth && y >= 0 && y < kBoardHeight;
}

bool TryGetSourceCell(const Action& action, int& x, int& y, int& cellIndex) noexcept {
	if (!action.m_optSource.has_value()) {
		return false;
	}

	x = action.m_optSource->first;
	y = action.m_optSource->second;
	if (!IsInBounds(x, y)) {
		return false;
	}

	cellIndex = y * kBoardWidth + x;
	return true;
}

bool TryGetTargetOffset(const Action& action, int sourceX, int sourceY, int& dx, int& dy) noexcept {
	if (!action.m_optTarget.has_value()) {
		return false;
	}

	const int targetX = action.m_optTarget->first;
	const int targetY = action.m_optTarget->second;
	if (!IsInBounds(targetX, targetY)) {
		return false;
	}

	dx = targetX - sourceX;
	dy = targetY - sourceY;
	return true;
}

bool TryGetOffsetIndex(int dx, int dy, int minDistance, int maxDistance, int& offsetIndex) noexcept {
	int index = 0;
	for (int currentDy = -maxDistance; currentDy <= maxDistance; ++currentDy) {
		for (int currentDx = -maxDistance; currentDx <= maxDistance; ++currentDx) {
			const int distance = Abs(currentDx) + Abs(currentDy);
			if (distance < minDistance || distance > maxDistance) {
				continue;
			}

			if (currentDx == dx && currentDy == dy) {
				offsetIndex = index;
				return true;
			}

			++index;
		}
	}

	return false;
}

bool TryGetOffsetAt(int offsetIndex, int minDistance, int maxDistance, int& dx, int& dy) noexcept {
	if (offsetIndex < 0 || offsetIndex >= OffsetCount(minDistance, maxDistance)) {
		return false;
	}

	int index = 0;
	for (int currentDy = -maxDistance; currentDy <= maxDistance; ++currentDy) {
		for (int currentDx = -maxDistance; currentDx <= maxDistance; ++currentDx) {
			const int distance = Abs(currentDx) + Abs(currentDy);
			if (distance < minDistance || distance > maxDistance) {
				continue;
			}

			if (index == offsetIndex) {
				dx = currentDx;
				dy = currentDy;
				return true;
			}

			++index;
		}
	}

	return false;
}

bool TryGetDirectionIndex(Action::Direction direction, int& directionIndex) noexcept {
	switch (direction) {
	case Action::Direction::North:
		directionIndex = 0;
		return true;
	case Action::Direction::East:
		directionIndex = 1;
		return true;
	case Action::Direction::South:
		directionIndex = 2;
		return true;
	case Action::Direction::West:
		directionIndex = 3;
		return true;
	default:
		return false;
	}
}

bool TryGetDirectionAt(int directionIndex, Action::Direction& direction) noexcept {
	switch (directionIndex) {
	case 0:
		direction = Action::Direction::North;
		return true;
	case 1:
		direction = Action::Direction::East;
		return true;
	case 2:
		direction = Action::Direction::South;
		return true;
	case 3:
		direction = Action::Direction::West;
		return true;
	default:
		return false;
	}
}

bool HasNoExtraGlobalFields(const Action& action) noexcept {
	return !action.m_optSource.has_value() &&
		!action.m_optTarget.has_value() &&
		!action.m_optDirection.has_value() &&
		!action.m_optUnitType.has_value() &&
		!action.m_optUnloadIndex.has_value();
}

Result EncodeTargetOffsetAction(const Action& action, int planeStart, int minDistance, int maxDistance, int& encodedAction) noexcept {
	int sourceX = 0;
	int sourceY = 0;
	int cellIndex = 0;
	if (!TryGetSourceCell(action, sourceX, sourceY, cellIndex)) {
		return Result::Failed;
	}

	int dx = 0;
	int dy = 0;
	if (!TryGetTargetOffset(action, sourceX, sourceY, dx, dy)) {
		return Result::Failed;
	}

	int offsetIndex = 0;
	if (!TryGetOffsetIndex(dx, dy, minDistance, maxDistance, offsetIndex)) {
		return Result::Failed;
	}

	encodedAction = (planeStart + offsetIndex) * kCellCount + cellIndex;
	return Result::Succeeded;
}

Result DecodeTargetOffsetAction(int planeStart, int plane, int cellIndex, int minDistance, int maxDistance, Action::Type type, Action& action) noexcept {
	const int offsetIndex = plane - planeStart;

	int dx = 0;
	int dy = 0;
	if (!TryGetOffsetAt(offsetIndex, minDistance, maxDistance, dx, dy)) {
		return Result::Failed;
	}

	const int sourceX = cellIndex % kBoardWidth;
	const int sourceY = cellIndex / kBoardWidth;
	const int targetX = sourceX + dx;
	const int targetY = sourceY + dy;
	if (!IsInBounds(targetX, targetY)) {
		return Result::Failed;
	}

	action = Action(type, sourceX, sourceY, targetX, targetY);
	return Result::Succeeded;
}

Result EncodeMoveAttackAction(const Action& action, int& encodedAction) noexcept {
	int sourceX = 0;
	int sourceY = 0;
	int cellIndex = 0;
	if (!TryGetSourceCell(action, sourceX, sourceY, cellIndex)) {
		return Result::Failed;
	}

	int dx = 0;
	int dy = 0;
	if (!TryGetTargetOffset(action, sourceX, sourceY, dx, dy)) {
		return Result::Failed;
	}

	int offsetIndex = 0;
	if (!TryGetOffsetIndex(dx, dy, 0, kMoveOffsetRadius, offsetIndex)) {
		return Result::Failed;
	}

	if (!action.m_optDirection.has_value()) {
		return Result::Failed;
	}

	int directionIndex = 0;
	if (!TryGetDirectionIndex(*action.m_optDirection, directionIndex)) {
		return Result::Failed;
	}

	encodedAction = (kMoveAttackPlaneStart + offsetIndex * kDirectionCount + directionIndex) * kCellCount + cellIndex;
	return Result::Succeeded;
}

Result DecodeMoveAttackAction(int plane, int cellIndex, Action& action) noexcept {
	const int relativePlane = plane - kMoveAttackPlaneStart;
	const int offsetIndex = relativePlane / kDirectionCount;
	const int directionIndex = relativePlane % kDirectionCount;

	int dx = 0;
	int dy = 0;
	if (!TryGetOffsetAt(offsetIndex, 0, kMoveOffsetRadius, dx, dy)) {
		return Result::Failed;
	}

	Action::Direction direction = Action::Direction::Invalid;
	if (!TryGetDirectionAt(directionIndex, direction)) {
		return Result::Failed;
	}

	const int sourceX = cellIndex % kBoardWidth;
	const int sourceY = cellIndex / kBoardWidth;
	const int targetX = sourceX + dx;
	const int targetY = sourceY + dy;
	if (!IsInBounds(targetX, targetY)) {
		return Result::Failed;
	}

	action = Action(Action::Type::MoveAttack, sourceX, sourceY, direction, targetX, targetY);
	return Result::Succeeded;
}

Result EncodeRepairAction(const Action& action, int& encodedAction) noexcept {
	int sourceX = 0;
	int sourceY = 0;
	int cellIndex = 0;
	if (!TryGetSourceCell(action, sourceX, sourceY, cellIndex) || !action.m_optDirection.has_value()) {
		return Result::Failed;
	}

	int directionIndex = 0;
	if (!TryGetDirectionIndex(*action.m_optDirection, directionIndex)) {
		return Result::Failed;
	}

	encodedAction = (kRepairPlaneStart + directionIndex) * kCellCount + cellIndex;
	return Result::Succeeded;
}

Result DecodeRepairAction(int plane, int cellIndex, Action& action) noexcept {
	Action::Direction direction = Action::Direction::Invalid;
	if (!TryGetDirectionAt(plane - kRepairPlaneStart, direction)) {
		return Result::Failed;
	}

	action = Action(Action::Type::Repair, cellIndex % kBoardWidth, cellIndex / kBoardWidth, direction);
	return Result::Succeeded;
}

Result EncodeUnloadAction(const Action& action, int& encodedAction) noexcept {
	int sourceX = 0;
	int sourceY = 0;
	int cellIndex = 0;
	if (!TryGetSourceCell(action, sourceX, sourceY, cellIndex) ||
		!action.m_optDirection.has_value() ||
		!action.m_optUnloadIndex.has_value()) {
		return Result::Failed;
	}

	int directionIndex = 0;
	if (!TryGetDirectionIndex(*action.m_optDirection, directionIndex)) {
		return Result::Failed;
	}

	const int unloadIndex = *action.m_optUnloadIndex;
	if (unloadIndex < 0 || unloadIndex >= kUnloadIndexCount) {
		return Result::Failed;
	}

	encodedAction = (kUnloadPlaneStart + directionIndex * kUnloadIndexCount + unloadIndex) * kCellCount + cellIndex;
	return Result::Succeeded;
}

Result DecodeUnloadAction(int plane, int cellIndex, Action& action) noexcept {
	const int relativePlane = plane - kUnloadPlaneStart;
	const int directionIndex = relativePlane / kUnloadIndexCount;
	const int unloadIndex = relativePlane % kUnloadIndexCount;

	Action::Direction direction = Action::Direction::Invalid;
	if (!TryGetDirectionAt(directionIndex, direction)) {
		return Result::Failed;
	}

	action = Action(Action::Type::Unload, cellIndex % kBoardWidth, cellIndex / kBoardWidth, direction, unloadIndex);
	return Result::Succeeded;
}

Result EncodeBuyAction(const Action& action, int& encodedAction) noexcept {
	int sourceX = 0;
	int sourceY = 0;
	int cellIndex = 0;
	if (!TryGetSourceCell(action, sourceX, sourceY, cellIndex) || !action.m_optUnitType.has_value()) {
		return Result::Failed;
	}

	const int unitTypeIndex = static_cast<int>(*action.m_optUnitType);
	if (unitTypeIndex < 0 || unitTypeIndex >= kUnitTypeCount) {
		return Result::Failed;
	}

	encodedAction = (kBuyPlaneStart + unitTypeIndex) * kCellCount + cellIndex;
	return Result::Succeeded;
}

Result DecodeBuyAction(int plane, int cellIndex, Action& action) noexcept {
	const int unitTypeIndex = plane - kBuyPlaneStart;
	if (unitTypeIndex < 0 || unitTypeIndex >= kUnitTypeCount) {
		return Result::Failed;
	}

	action = Action(Action::Type::Buy, cellIndex % kBoardWidth, cellIndex / kBoardWidth, static_cast<UnitProperties::Type>(unitTypeIndex));
	return Result::Succeeded;
}
}

const char* ActionSpace::Version() noexcept {
	return "standard-gl-v1";
}

int ActionSpace::BoardWidth() noexcept {
	return kBoardWidth;
}

int ActionSpace::BoardHeight() noexcept {
	return kBoardHeight;
}

int ActionSpace::ActionCount() noexcept {
	return kActionCount;
}

Result ActionSpace::EncodeAction(const Action& action, int& encodedAction) noexcept {
	if (HasNoExtraGlobalFields(action)) {
		switch (action.m_type) {
		case Action::Type::EndTurn:
			encodedAction = kGlobalActionStart;
			return Result::Succeeded;
		case Action::Type::COPower:
			encodedAction = kGlobalActionStart + 1;
			return Result::Succeeded;
		case Action::Type::SCOPower:
			encodedAction = kGlobalActionStart + 2;
			return Result::Succeeded;
		default:
			break;
		}
	}

	switch (action.m_type) {
	case Action::Type::MoveWait:
		return EncodeTargetOffsetAction(action, kMoveWaitPlaneStart, 0, kMoveOffsetRadius, encodedAction);
	case Action::Type::MoveAttack:
		return EncodeMoveAttackAction(action, encodedAction);
	case Action::Type::Attack:
		return EncodeTargetOffsetAction(action, kAttackPlaneStart, kAttackOffsetMinDistance, kAttackOffsetRadius, encodedAction);
	case Action::Type::MoveCapture:
		return EncodeTargetOffsetAction(action, kMoveCapturePlaneStart, 0, kCaptureOffsetRadius, encodedAction);
	case Action::Type::MoveCombine:
		return EncodeTargetOffsetAction(action, kMoveCombinePlaneStart, 0, kMoveOffsetRadius, encodedAction);
	case Action::Type::MoveLoad:
		return EncodeTargetOffsetAction(action, kMoveLoadPlaneStart, 0, kMoveOffsetRadius, encodedAction);
	case Action::Type::MoveHide:
		return EncodeTargetOffsetAction(action, kMoveHidePlaneStart, 0, kHideOffsetRadius, encodedAction);
	case Action::Type::MoveUnhide:
		return EncodeTargetOffsetAction(action, kMoveUnhidePlaneStart, 0, kHideOffsetRadius, encodedAction);
	case Action::Type::Repair:
		return EncodeRepairAction(action, encodedAction);
	case Action::Type::Unload:
		return EncodeUnloadAction(action, encodedAction);
	case Action::Type::Buy:
		return EncodeBuyAction(action, encodedAction);
	default:
		break;
	}

	return Result::Failed;
}

Result ActionSpace::DecodeAction(int encodedAction, Action& action) noexcept {
	if (encodedAction < 0 || encodedAction >= kActionCount) {
		return Result::Failed;
	}

	if (encodedAction >= kGlobalActionStart) {
		switch (encodedAction - kGlobalActionStart) {
		case 0:
			action = Action(Action::Type::EndTurn);
			return Result::Succeeded;
		case 1:
			action = Action(Action::Type::COPower);
			return Result::Succeeded;
		case 2:
			action = Action(Action::Type::SCOPower);
			return Result::Succeeded;
		default:
			return Result::Failed;
		}
	}

	const int plane = encodedAction / kCellCount;
	const int cellIndex = encodedAction % kCellCount;

	if (plane >= kMoveWaitPlaneStart && plane < kMoveAttackPlaneStart) {
		return DecodeTargetOffsetAction(kMoveWaitPlaneStart, plane, cellIndex, 0, kMoveOffsetRadius, Action::Type::MoveWait, action);
	}

	if (plane >= kMoveAttackPlaneStart && plane < kAttackPlaneStart) {
		return DecodeMoveAttackAction(plane, cellIndex, action);
	}

	if (plane >= kAttackPlaneStart && plane < kMoveCapturePlaneStart) {
		return DecodeTargetOffsetAction(kAttackPlaneStart, plane, cellIndex, kAttackOffsetMinDistance, kAttackOffsetRadius, Action::Type::Attack, action);
	}

	if (plane >= kMoveCapturePlaneStart && plane < kMoveCombinePlaneStart) {
		return DecodeTargetOffsetAction(kMoveCapturePlaneStart, plane, cellIndex, 0, kCaptureOffsetRadius, Action::Type::MoveCapture, action);
	}

	if (plane >= kMoveCombinePlaneStart && plane < kMoveLoadPlaneStart) {
		return DecodeTargetOffsetAction(kMoveCombinePlaneStart, plane, cellIndex, 0, kMoveOffsetRadius, Action::Type::MoveCombine, action);
	}

	if (plane >= kMoveLoadPlaneStart && plane < kMoveHidePlaneStart) {
		return DecodeTargetOffsetAction(kMoveLoadPlaneStart, plane, cellIndex, 0, kMoveOffsetRadius, Action::Type::MoveLoad, action);
	}

	if (plane >= kMoveHidePlaneStart && plane < kMoveUnhidePlaneStart) {
		return DecodeTargetOffsetAction(kMoveHidePlaneStart, plane, cellIndex, 0, kHideOffsetRadius, Action::Type::MoveHide, action);
	}

	if (plane >= kMoveUnhidePlaneStart && plane < kRepairPlaneStart) {
		return DecodeTargetOffsetAction(kMoveUnhidePlaneStart, plane, cellIndex, 0, kHideOffsetRadius, Action::Type::MoveUnhide, action);
	}

	if (plane >= kRepairPlaneStart && plane < kUnloadPlaneStart) {
		return DecodeRepairAction(plane, cellIndex, action);
	}

	if (plane >= kUnloadPlaneStart && plane < kBuyPlaneStart) {
		return DecodeUnloadAction(plane, cellIndex, action);
	}

	if (plane >= kBuyPlaneStart && plane < kCellPlaneCount) {
		return DecodeBuyAction(plane, cellIndex, action);
	}

	return Result::Failed;
}

Result ActionSpace::GenerateLegalActionMask(const GameState& gameState, std::vector<std::uint8_t>& mask) noexcept {
	const Map* map = gameState.TryGetMap();
	if (map == nullptr || map->GetCols() > kBoardWidth || map->GetRows() > kBoardHeight) {
		return Result::Failed;
	}

	mask.assign(kActionCount, 0U);

	std::vector<Action> legalActions;
	IfFailedReturn(gameState.GetValidActions(legalActions));
	for (const Action& action : legalActions) {
		int encodedAction = -1;
		IfFailedReturn(EncodeAction(action, encodedAction));
		mask[encodedAction] = 1U;
	}

	return Result::Succeeded;
}
