# Self-Play Replay Shards

Self-play replay shards are compact JSONL files produced by `-self-play` and checked by `-validate-replay`.

The v1 format is `standard-gl-self-play-replay-v1`. It targets normal Global League Standard games that fit the `standard-gl-v1` action space and `standard-gl-v1-state` tensor shape.

## Commands

Generate one deterministic smoke replay:

```powershell
Set-Location .\AdvanceWarsServer
..\x64\Debug\AdvanceWarsServer.exe -self-play `
  --out ..\artifacts\replays\smoke.jsonl `
  --map mcts `
  --player0-co andy `
  --player1-co adder `
  --seed 123 `
  --games 1 `
  --max-actions 1000 `
  --mcts-simulations 128
```

Validate an existing shard:

```powershell
..\x64\Debug\AdvanceWarsServer.exe -validate-replay ..\artifacts\replays\smoke.jsonl
```

`-self-play` writes the replay and then validates the whole file. If validation fails, the file is left in place for debugging and the process returns nonzero.

By default, `-self-play` fails when `--out` already exists. Pass `--append` to append to an existing compatible shard. Appends validate the whole existing file first. An empty existing file is treated as a new shard and receives a fresh header.

## Options

Required:

| Option | Meaning |
| --- | --- |
| `--out <path>` | Output JSONL shard. Missing parent directories are created. |
| `--map <id>` | Supported Standard map id, currently `lefty` or `mcts`. |
| `--player0-co <id>` | Player 0 CO id in API style, such as `andy` or `von-bolt`. |
| `--player1-co <id>` | Player 1 CO id in API style. |

Common optional settings:

| Option | Default | Meaning |
| --- | ---: | --- |
| `--games <n>` | `1` | Number of games to write. |
| `--seed <n>` | `0` | Base deterministic seed. Per-game combat and MCTS seeds are derived from it. |
| `--max-actions <n>` | `1000` | Runner safety cap over all atomic actions. |
| `--unit-cap <n>` | `50` | Standard unit cap. |
| `--capture-limit <n>` | `21` | Standard capture limit. |
| `--day-limit <n>` | unset | Engine day-limit rule. |
| `--heuristic-auto-resign` | off | Opt into the existing engine army-value early resign heuristic. |
| `--append` | off | Append to a compatible shard. |
| `--quiet` | off | Suppress progress output, but not errors. |

MCTS options:

| Option | Default | Meaning |
| --- | ---: | --- |
| `--mcts-simulations <n>` | `128` | Root simulations per action. Must be at least `1`. |
| `--mcts-max-nodes <n>` | `10000` | Search node cap. |
| `--mcts-max-rollout-actions <n>` | `512` | Rollout action cap. |
| `--mcts-exploration <x>` | `sqrt(2)` | UCT exploration constant. |
| `--temperature <x>` | `1.0` | Visit-count action selection temperature. Use `0` for greedy highest-visit selection. |

## JSONL Records

Each line is one compact JSON object with an explicit `recordType`.

The first line is a header:

```json
{
  "recordType": "header",
  "replayFormatVersion": "standard-gl-self-play-replay-v1",
  "createdAt": "2026-06-01T00:00:00Z",
  "versions": {
    "stateTensor": "standard-gl-v1-state",
    "actionSpace": "standard-gl-v1"
  },
  "config": {
    "mapId": "mcts",
    "player0Co": "andy",
    "player1Co": "adder",
    "baseSeed": 123,
    "maxActions": 1000,
    "settings": {},
    "mctsOptions": {}
  }
}
```

Every following line is a game record. Game records are self-contained enough to inspect or validate independently:

```json
{
  "recordType": "game",
  "replayFormatVersion": "standard-gl-self-play-replay-v1",
  "versions": {
    "stateTensor": "standard-gl-v1-state",
    "actionSpace": "standard-gl-v1"
  },
  "gameIndex": 0,
  "config": {
    "mapId": "mcts",
    "baseSeed": 123,
    "combatSeed": 123,
    "mctsSeed": 1000126,
    "maxActions": 1000,
    "mctsOptions": {}
  },
  "players": [
    { "slot": 0, "co": "andy", "armyType": "orange-star" },
    { "slot": 1, "co": "adder", "armyType": "blue-moon" }
  ],
  "settings": {},
  "initialState": {},
  "actions": [],
  "samples": [],
  "finalState": {},
  "terminalReason": "action-limit",
  "winner": null,
  "metrics": {}
}
```

`initialState` and `finalState` are full authoritative engine states. They intentionally include exact hidden information because replay shards are training/debug artifacts, not player-facing API responses.

## Actions And Samples

There is exactly one sample for every applied atomic action:

```text
actions.length == samples.length
```

`ply` is zero-based. Sample `ply: 0` is the pre-action state for `actions[0]`.

Action history entries contain raw action JSON plus the stable encoded action index:

```json
{
  "ply": 0,
  "player": 0,
  "actionIndex": 1554363,
  "action": { "type": "end-turn" }
}
```

Samples contain compact training targets:

```json
{
  "ply": 0,
  "currentPlayer": 0,
  "stateTensorChecksum": "0123456789abcdef",
  "legalActionCount": 3,
  "legalActionIndices": [1203, 98112, 1554363],
  "visitCounts": [
    { "actionIndex": 1203, "visits": 18 },
    { "actionIndex": 1554363, "visits": 110 }
  ],
  "selectedActionIndex": 1554363,
  "outcome": -1,
  "mcts": {
    "mctsSeed": 3992670690,
    "simulationsRun": 128,
    "nodesCreated": 128,
    "searchTimeMs": 14.7
  }
}
```

Replay v1 does not store dense state tensors or dense legal action masks. Validation rebuilds the state from `initialState` plus `actions[0..ply)`, regenerates the tensor checksum and sparse legal action indices, and compares them exactly.

`visitCounts` stores positive visits only, sorted by `actionIndex`. Legal actions with zero visits are implied by `legalActionIndices`.

`outcome` is from the sample's `currentPlayer` perspective:

- `1`: current player eventually won
- `-1`: current player eventually lost
- `0`: draw or runner safety stop

## Terminal Handling

Engine terminal states store the engine winner and terminal reason. If `--heuristic-auto-resign` is enabled and the engine emits `heuristic-resign`, samples receive normal win/loss outcomes.

If the runner reaches `--max-actions` before the engine is terminal, the game record uses:

```json
{
  "terminalReason": "action-limit",
  "winner": null
}
```

All sample outcomes for an action-limit game are `0`.

## Validation Rules

`-validate-replay` is strict for v1:

- unknown fields are rejected
- replay, state tensor, and action-space versions must match this binary
- action history must replay from `initialState` to `finalState`
- every sample's tensor checksum and sparse legal action indices must match the replayed pre-action state
- selected actions must be legal and have positive MCTS visits
- `sum(visitCounts.visits)` must equal `mcts.simulationsRun`
- `actions.length` must equal `samples.length`
- timing metrics must be present and nonnegative

Validation does not rerun MCTS. It treats visit counts as recorded search output and checks structural consistency.

## Deferred Optimizations

The v1 shard is plain JSONL. Follow-up work tracks derived materialized caches (#172), map/matchup orchestration and side balancing (#173), and compressed shard support (#174).
