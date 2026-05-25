# JSON Fixture Guide

Last reviewed: 2026-05-25

The JSON suite under `AdvanceWarsServer/test/json` is the main executable documentation for engine behavior. The test runner recursively discovers `.json` files and runs each fixture independently.

Run all fixtures from the `AdvanceWarsServer` project directory:

```powershell
..\x64\Debug\AdvanceWarsServer.exe -test test/json
```

Run a subset:

```powershell
..\x64\Debug\AdvanceWarsServer.exe -test test/json/transport
```

## Fixture Types

The runner supports several top-level fixture shapes.

### State Transition Fixture

Use this for normal behavior: start from an initial state, apply actions, and compare the serialized result with the expected final state.

```json
{
  "initial-game-state": {
  },
  "actions": [
  ],
  "final-game-state": {
  }
}
```

The runner:

1. Parses `initial-game-state`.
2. Applies each action in `actions` with `GameState::DoAction`.
3. Serializes the result.
4. Compares it byte-for-byte with `final-game-state` after both states are normalized through engine serialization.

### Failed Actions Fixture

Use `failedActions` to assert that actions are rejected from the initial state.

```json
{
  "initial-game-state": {
  },
  "failedActions": [
  ]
}
```

Each action must return `Result::Failed`.

Important: current invalid-action behavior is not fully atomic in all paths. If a fixture checks multiple `failedActions`, keep them independent from the same initial state and watch #67 for the broader validation cleanup.

### Valid Actions Fixture

Use `validActions` to assert the exact legal actions for a board coordinate.

```json
{
  "initial-game-state": {
  },
  "validActions": [
    {
      "source": [4, 7],
      "expected": [
      ]
    }
  ]
}
```

The runner calls `GetValidActions(x, y)` and compares the serialized action list to `expected`.

### Deterministic Replay Fixture

Use `deterministic-replay` when an action sequence should serialize to the same trace every time.

```json
{
  "initial-game-state": {
  },
  "actions": [
  ],
  "deterministic-replay": true
}
```

The runner applies the same action sequence twice and compares the full serialized trace.

### Deterministic Luck Fixtures

Player JSON supports `"luck-policy"` for combat fixtures:

- `0`: normal combat luck, using `"combat-rng-seed"` when present.
- `1`: force the lowest total luck outcome.
- `2`: force the highest total luck outcome.
- `3`: force the middle value of each luck range.

For COs with bad luck, "lowest" means minimum good luck and maximum bad luck. "Highest" means maximum good luck and minimum bad luck.

### CO Contract Fixture

Use `co-contract` to check CO parser round-trip, power-meter stars, thresholds, availability flags, and checked-in chart values.

```json
{
  "co-contract": {
    "name": "Andy",
    "power-meter": {
      "cop-stars": 3,
      "scop-stars": 6,
      "cop-threshold": 27000,
      "scop-threshold": 81000,
      "can-use-cop": false,
      "can-use-scop": false
    },
    "stats": [
      {
        "unit": "infantry",
        "normal": [100, 100],
        "cop": [110, 110],
        "scop": [110, 110]
      }
    ]
  }
}
```

Use `invalid-co` to assert that an unknown CO string is rejected.

Power-meter expectations are partial in CO contract fixtures: include only the fields relevant to the behavior under test. State-transition fixtures compare the full serialized game state, including `"charge"`, `"star-value"`, `"cop-threshold"`, `"scop-threshold"`, `"can-use-cop"`, and `"can-use-scop"` for each player.

## Action JSON

Action JSON is parsed by `from_json(json&, Action&)` in `GameState.cpp`. Existing fixtures are the best source for exact spelling. Common action types include:

- `attack`
- `end-turn`
- `move-wait`
- `move-attack`
- `unload`
- `move-load`
- `move-capture`
- `move-combine`
- `repair`
- `buy`
- `co-power`
- `super-co-power`

When adding an action fixture, prefer copying the smallest nearby fixture that already uses the same action shape.

## Authoring Guidelines

- Keep each fixture focused on one behavior or edge case.
- Put new unit-owned behavior under `test/json/units/<unit-type>/` when practical.
- Put cross-cutting rule behavior under the nearest feature folder, such as `captures`, `production`, `transport`, `weather`, `com-tower`, `lab`, or `co`.
- Include at least one boundary/negative case for new mechanics.
- For AWBW-specific behavior, link the corresponding GitHub issue in nearby documentation or the PR body.
- Run the focused folder first, then the full suite.

## Useful Existing Fixture Areas

| Folder | Useful for |
| --- | --- |
| `captures/` | Capture progress, capture interruption, property transfer, airports, ports, labs, and Com Towers. |
| `production/` | Buy actions, production terrain, unit cap, airports, and ports. |
| `transport/` | Load/unload, cargo destruction, APC resupply, carrier/cruiser loaded resupply, Black Boat repair. |
| `co/` | CO contract, economy, power actions, production effects, Grit/Max/Sami behavior. |
| `weather/` | Rain/snow movement, temporary weather, Drake/Olaf powers. |
| `units/` | Unit-owned movement, fuel, combat, ammo, and boundaries. |

Capture-limit fixtures follow AWBW counting: Cities, Bases, Airports, Ports, and HQs count toward the configured limit; Labs and Com Towers do not.

## Debugging Failures

On failure, the runner prints:

- the failing file path
- expected serialized JSON
- actual serialized JSON

The output is often dense. For board-level regressions, the planned fixture visualizer is tracked by #113.
