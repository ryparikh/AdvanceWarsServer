# Action Space Encoding

The v1 policy/action encoding is `standard-gl-v1`. It is scoped to in-game actions for Standard GL self-play. Pregame CO choice is not an `Action`; it is tracked separately by issue #164.

## Board Shape

`standard-gl-v1` uses a fixed `27x23` board shape, matching the largest current Standard GL map checked during issue #4 design. Smaller maps are encoded in the top-left of that shape. Off-map source cells and impossible target offsets remain masked illegal.

Mask generation fails if the current map is wider than 27 tiles or taller than 23 tiles. That failure is intentional so a new map pool cannot silently train against a clipped policy space.

## Flat Index Layout

Source-cell actions are arranged as action planes over the fixed board:

```text
cell = y * 27 + x
index = plane * (27 * 23) + cell
```

There are `621` source cells. Global actions are appended after all source-cell planes.

## Plane Order

Offsets use Manhattan distance and are ordered by `dy`, then `dx`. Directions are ordered `north`, `east`, `south`, `west`. Unload indices are ordered `0`, then `1`. Buy unit planes follow `UnitProperties::Type` enum order.

| Plane start | Plane count | Action family | Payload |
| ---: | ---: | --- | --- |
| 0 | 265 | `move-wait` | target offset, radius 11 |
| 265 | 1060 | `move-attack` | target offset radius 11, then direction |
| 1325 | 260 | `attack` | target offset distance 2 through 11 |
| 1585 | 61 | `move-capture` | target offset, radius 5 |
| 1646 | 265 | `move-combine` | target offset, radius 11 |
| 1911 | 265 | `move-load` | target offset, radius 11 |
| 2176 | 145 | `move-hide` | target offset, radius 8 |
| 2321 | 145 | `move-unhide` | target offset, radius 8 |
| 2466 | 4 | `repair` | direction |
| 2470 | 8 | `unload` | direction, then unload index |
| 2478 | 25 | `buy` | unit type |

The source-cell plane count is `2503`.

## Global Actions

Global action indices are appended after the source-cell planes:

| Index | Action |
| ---: | --- |
| 1554363 | `end-turn` |
| 1554364 | `co-power` |
| 1554365 | `super-co-power` |

The total action count is `1,554,366`.

## Decode And Masks

`ActionSpace::DecodeAction` turns a structurally valid index into the candidate `Action` even if that action is not legal in a specific state. It fails for out-of-range indices and indices whose offset would leave the fixed `27x23` board.

`ActionSpace::GenerateLegalActionMask` is the state-aware gate. It calls `GameState::GetValidActions`, encodes each legal action, and returns a `std::vector<std::uint8_t>` with `0/1` values.
