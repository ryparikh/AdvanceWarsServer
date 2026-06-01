# State Tensor Encoding

The v1 state tensor encoding is `standard-gl-v1-state`. It is scoped to normal Global League Standard self-play and is designed to pair with the `standard-gl-v1` action space in `docs/ACTION_SPACE.md`.

The tensor is a stable, versioned ABI for model input, replay validation, and tests. It represents the current active player's observable position. It does not encode legal actions; callers should use `ActionSpace::GenerateLegalActionMask` separately.

## Goals

- Keep the game engine as the source of truth for rules and state transitions.
- Encode the board from the active player's perspective.
- Keep the tensor deterministic, testable, and independent of LibTorch.
- Avoid training on information that would not be visible in real play.
- Prefer a clear v1 contract over premature channel minimization.

## Shape And Layout

`standard-gl-v1-state` uses the same fixed board canvas as the action space:

```text
channels x height x width
C x 23 x 27
```

Smaller maps are placed at the top-left of the canvas. Off-map cells are zero in every channel. Maps wider than 27 tiles or taller than 23 tiles fail encoding.

The flat storage layout is channel-major:

```text
index = channel * 23 * 27 + y * 27 + x
```

Tensor values are `float`. All scalar values are finite and clipped to `[0, 1]`.

## Public API Shape

The public C++ surface mirrors `ActionSpace` while staying free of LibTorch:

```cpp
class StateTensor final {
public:
    static const char* Version() noexcept;
    static int BoardWidth() noexcept;
    static int BoardHeight() noexcept;
    static int ChannelCount() noexcept;
    static const std::vector<std::string>& ChannelNames();
    static Result ChannelIndex(const std::string& name, int& index) noexcept;
    static Result Encode(const GameState& gameState, std::vector<float>& values) noexcept;
    static Result Checksum(const std::vector<float>& values, std::uint64_t& checksum) noexcept;
};
```

The public header is `AdvanceWarsServer/inc/StateTensor.h`. The implementation uses the existing `AdvanceWarsServer/src/Tensor.cpp`.

## Perspective

All ownership features are relative to the active player:

- `self` means the active player.
- `opponent` means the other player.
- `neutral` means no owner.

The tensor does not encode absolute active-player identity, player index, or army color. Replay metadata may still store those values.

The tensor preserves absolute board coordinates. It does not flip or rotate by active player.

## Observation Rules

The tensor avoids exposing hidden information that a real ladder player would not see.

Health is encoded as displayed HP pips, not exact internal HP:

```text
displayHp = ceil(internalHp / 10.0)
health = displayHp / 10.0
```

Ammo and fuel are visible exact values in the current Standard scope and are normalized by the unit type's maximum values from `GetUnitInfo`.

Enemy hidden Stealth/Sub units are encoded only if revealed to the active player. A hidden enemy Stealth/Sub is revealed when it is adjacent to one of the active player's units or standing on one of the active player's properties. Otherwise, the unit and its cargo are omitted from all unit-related planes and aggregates.

Enemy Sonja unit health is unknown to the active player. For visible enemy Sonja units, encode unit type, position, ammo, fuel, moved, hidden, stunned, and cargo presence as usual, but set opponent health to `0` and set `opponent-health-known` to `0`. For other visible opponent units, `opponent-health-known` is `1`.

Unknown enemy Sonja HP contributes `0` to opponent army value.

## Channel Order

Channel order is part of the ABI. Tests assert channel count and representative channel names/indices.

Channels are ordered by broad group, then by stable enum order inside enum-backed groups:

1. metadata
2. terrain
3. properties
4. board units
5. unit attributes
6. loaded units
7. economy and global material
8. CO identity
9. CO power state
10. turn and weather

## Channels

### Metadata

```text
in-bounds
```

`in-bounds` is `1` for real map cells and `0` for padded cells.

### Terrain

One binary plane per `Terrain::Type`, in enum order:

```text
terrain/plain
terrain/mountain
terrain/forest
terrain/river
terrain/road
terrain/bridge
terrain/sea
terrain/shoal
terrain/reef
terrain/city
terrain/base
terrain/airport
terrain/port
terrain/headquarters
terrain/pipe
terrain/missile-silo
terrain/com-tower
terrain/lab
```

Terrain is categorical. There is no numeric terrain-id plane and no terrain-defense plane in v1.

### Properties

```text
property-self
property-opponent
property-neutral
property-capture-points
```

`property-capture-points` is remaining capture points divided by `20.0`. Non-property cells are `0`.

Terrain planes identify the property type. Ownership planes identify who owns the property.

### Board Unit Types

One binary unit-type plane per side and per `UnitProperties::Type`, in enum order:

```text
self-unit/anti-air
self-unit/apc
self-unit/artillery
self-unit/bcopter
self-unit/battleship
self-unit/blackboat
self-unit/blackbomb
self-unit/bomber
self-unit/carrier
self-unit/crusier
self-unit/fighter
self-unit/infantry
self-unit/lander
self-unit/medium-tank
self-unit/mech
self-unit/megatank
self-unit/missile
self-unit/neotank
self-unit/piperunner
self-unit/recon
self-unit/rocket
self-unit/stealth
self-unit/sub
self-unit/tcopter
self-unit/tank
opponent-unit/anti-air
opponent-unit/apc
opponent-unit/artillery
opponent-unit/bcopter
opponent-unit/battleship
opponent-unit/blackboat
opponent-unit/blackbomb
opponent-unit/bomber
opponent-unit/carrier
opponent-unit/crusier
opponent-unit/fighter
opponent-unit/infantry
opponent-unit/lander
opponent-unit/medium-tank
opponent-unit/mech
opponent-unit/megatank
opponent-unit/missile
opponent-unit/neotank
opponent-unit/piperunner
opponent-unit/recon
opponent-unit/rocket
opponent-unit/stealth
opponent-unit/sub
opponent-unit/tcopter
opponent-unit/tank
```

### Unit Attributes

```text
self-health
self-ammo
self-fuel
self-moved
self-hidden
self-stunned
opponent-health
opponent-ammo
opponent-fuel
opponent-moved
opponent-hidden
opponent-stunned
opponent-health-known
```

Health uses displayed HP pips. Ammo and fuel are normalized against each visible unit type's maximum. Units without ammo encode ammo as `0`.

`moved` and `stunned` are separate. `moved` represents same-turn action availability. `stunned` represents Von Bolt's future-tempo effect before it is converted into `moved` at the stunned player's next turn.

### Loaded Units

Loaded units are encoded as aggregate cargo features on the transport's cell. Cargo slot order is not encoded.

```text
self-loaded-count
opponent-loaded-count
self-loaded/anti-air
self-loaded/apc
self-loaded/artillery
self-loaded/bcopter
self-loaded/battleship
self-loaded/blackboat
self-loaded/blackbomb
self-loaded/bomber
self-loaded/carrier
self-loaded/crusier
self-loaded/fighter
self-loaded/infantry
self-loaded/lander
self-loaded/medium-tank
self-loaded/mech
self-loaded/megatank
self-loaded/missile
self-loaded/neotank
self-loaded/piperunner
self-loaded/recon
self-loaded/rocket
self-loaded/stealth
self-loaded/sub
self-loaded/tcopter
self-loaded/tank
opponent-loaded/anti-air
opponent-loaded/apc
opponent-loaded/artillery
opponent-loaded/bcopter
opponent-loaded/battleship
opponent-loaded/blackboat
opponent-loaded/blackbomb
opponent-loaded/bomber
opponent-loaded/carrier
opponent-loaded/crusier
opponent-loaded/fighter
opponent-loaded/infantry
opponent-loaded/lander
opponent-loaded/medium-tank
opponent-loaded/mech
opponent-loaded/megatank
opponent-loaded/missile
opponent-loaded/neotank
opponent-loaded/piperunner
opponent-loaded/recon
opponent-loaded/rocket
opponent-loaded/stealth
opponent-loaded/sub
opponent-loaded/tcopter
opponent-loaded/tank
self-loaded-health
opponent-loaded-health
```

`loaded-count` is cargo count divided by `2.0`.

Loaded unit type planes encode count by type divided by `2.0`. For example, two loaded Infantry set `self-loaded/infantry` to `1.0`; one loaded Infantry sets it to `0.5`.

Loaded health is total displayed cargo HP divided by `20.0`, since the maximum visible cargo is two full-health units.

Cargo visibility follows transport visibility. If an enemy hidden transport is not observable, its cargo does not appear in loaded-unit planes or unit-related aggregates.

### Economy And Global Material

Broadcast over real in-bounds cells only:

```text
self-funds
opponent-funds
self-income
opponent-income
self-unit-count
opponent-unit-count
self-army-value
opponent-army-value
self-capture-progress
opponent-capture-progress
```

Funds:

```text
min(funds, 50000) / 50000.0
```

Income uses actual engine income, including CO modifiers and non-income properties, then:

```text
min(actualIncome, 50000) / 50000.0
```

Unit count includes visible loaded units and is normalized by unit cap:

```text
min(visibleUnitCount, unitCap) / unitCap
```

Army value uses displayed HP, not exact internal HP:

```text
unitValue = displayHp / 10.0 * unitCost
armyValue = min(sum(unitValue), 200000) / 200000.0
```

Capture progress uses properties counted by capture-limit rules, excluding Labs and Com Towers:

```text
min(ownedCaptureLimitProperties, captureLimit) / captureLimit
```

### CO Identity

One broadcast binary plane per side and per `CommandingOfficier::Type`, in enum order:

```text
self-co/adder
self-co/andy
self-co/colin
self-co/drake
self-co/eagle
self-co/flak
self-co/grimm
self-co/grit
self-co/hachi
self-co/hawke
self-co/jake
self-co/javier
self-co/jess
self-co/jugger
self-co/kanbei
self-co/kindle
self-co/koal
self-co/lash
self-co/max
self-co/nell
self-co/olaf
self-co/rachel
self-co/sami
self-co/sasha
self-co/sensei
self-co/sonja
self-co/sturm
self-co/vonbolt
opponent-co/adder
opponent-co/andy
opponent-co/colin
opponent-co/drake
opponent-co/eagle
opponent-co/flak
opponent-co/grimm
opponent-co/grit
opponent-co/hachi
opponent-co/hawke
opponent-co/jake
opponent-co/javier
opponent-co/jess
opponent-co/jugger
opponent-co/kanbei
opponent-co/kindle
opponent-co/koal
opponent-co/lash
opponent-co/max
opponent-co/nell
opponent-co/olaf
opponent-co/rachel
opponent-co/sami
opponent-co/sasha
opponent-co/sensei
opponent-co/sonja
opponent-co/sturm
opponent-co/vonbolt
```

CO choice is not an in-game action, but it is part of the state because it changes combat, economy, movement, powers, and strategy.

### CO Power State

Broadcast over real in-bounds cells only:

```text
self-power-normal
self-power-cop-active
self-power-scop-active
self-power-charge
opponent-power-normal
opponent-power-cop-active
opponent-power-scop-active
opponent-power-charge
```

Power status is one-hot. Charge is normalized by that player's SCOP threshold:

```text
min(charge, scopThreshold) / scopThreshold
```

There are no `can-use-cop` or `can-use-scop` planes in v1. Power action legality comes from the legal action mask.

### Turn And Weather

Broadcast over real in-bounds cells only:

```text
turn-count
weather-clear
weather-rain
weather-snow
weather-turns-remaining
```

Turn count:

```text
min(turnCount, 30) / 30.0
```

Weather is one-hot. Temporary weather turns remaining:

```text
min(turnsRemaining, 3) / 3.0
```

When no temporary weather counter exists, `weather-turns-remaining` is `0`.

## Channel Count And Memory Budget

With the current enum sizes, the expected v1 channel budget is about 219 channels:

```text
1 metadata
18 terrain
4 property
50 board unit type
13 unit attributes
54 loaded-unit aggregate
10 economy and global material
56 CO identity
8 CO power
5 turn and weather
```

For a `219 x 23 x 27` tensor:

```text
135,999 floats
~0.52 MiB as float32
~0.26 MiB as float16
```

The dense policy output for `standard-gl-v1` is larger:

```text
1,554,366 action logits
~5.93 MiB as float32 per sample
~2.96 MiB as float16 per sample
```

This channel count is acceptable for v1. The design favors clear, auditable planes over aggressive compression. Replay storage should not store dense tensor floats by default; store serialized state and tensor version, then regenerate tensors on load.

## Validation And Tests

Tensor encoding should fail when:

- the map exceeds `27x23`
- a terrain, unit type, or CO enum is invalid
- a unit owner cannot be mapped to self or opponent
- a property owner cannot be mapped to self, opponent, or neutral
- a normalized value is NaN, Inf, or outside `[0, 1]`

The JSON fixture runner supports a `stateTensor` assertion block, similar to `actionSpace` and `legalActionMask`:

```json
{
  "stateTensor": {
    "version": "standard-gl-v1-state",
    "width": 27,
    "height": 23,
    "channels": 219,
    "values": [
      { "channel": "self-unit/infantry", "at": [0, 0], "value": 1.0 },
      { "channel": "opponent-funds", "at": [0, 0], "value": 0.2 }
    ]
  }
}
```

Tests compare floats with a small tolerance, such as `1e-6`.

The implementation also includes a deterministic checksum helper over version, shape, channel names, and flat tensor values. The checksum is for replay validation and ABI drift detection, not gameplay logic.

## Replay Guidance

Replay samples should store:

- tensor version
- action-space version
- serialized game state or enough state data to rebuild the tensor
- current player
- selected action index
- sparse legal action indices
- sparse MCTS visit counts
- final outcome from the sampled player's perspective

Do not store dense state tensors or dense legal action masks by default. Regenerate tensors and materialize dense masks only when loading a training batch.

## V1 Non-Goals

The following are intentionally omitted from `standard-gl-v1-state`:

- legal action planes
- map id planes
- absolute player index or army-color planes
- terrain defense scalar planes
- production availability planes
- unit-ban planes
- fixed Standard settings planes
- day-limit progress planes
- hidden-unit memory or last-known-position planes
- game-over, winner, or terminal-reason planes
- luck policy or combat RNG seed planes
- dense tensor storage in replay shards
- LibTorch-specific tensor return types

Future experiments may add some of these in a new tensor version if training data shows a need.
