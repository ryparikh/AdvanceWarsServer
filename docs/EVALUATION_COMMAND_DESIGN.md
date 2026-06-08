# Evaluation Command Design

Issue: https://github.com/ryparikh/AdvanceWarsServer/issues/9

This document records the v1 checkpoint evaluation design for the self-play
training loop. It is intentionally the first reproducible scoreboard, not the
final AlphaZero search/evaluation system.

## Goal

Add a report-only command that can compare checkpoint agents against another
checkpoint agent or a random baseline on deterministic Standard games. The
command should make it possible to answer: did this checkpoint produce better
game outcomes under a fixed evaluation schedule?

The command does not mutate, copy, rename, or promote checkpoint bundles. It
writes a machine-readable recommendation only.

## Command Shape

The top-level command is `-evaluate`:

```powershell
..\x64\Debug\AdvanceWarsServer.exe -evaluate `
  --agent0 checkpoint-policy --checkpoint0 <dir> `
  --agent1 random `
  --map mcts `
  --player0-co andy `
  --player1-co adder `
  --rounds 20 `
  --seed 0 `
  --out ..\artifacts\evaluations\eval.json
```

The focused test gate is `-test-evaluate`.

Required options:

- `--agent0 <checkpoint-policy|random>`
- `--agent1 <checkpoint-policy|random>`
- `--checkpoint0 <dir>` when `--agent0 checkpoint-policy`
- `--checkpoint1 <dir>` when `--agent1 checkpoint-policy`
- `--map <id>`
- `--player0-co <id>`
- `--player1-co <id>`
- `--out <path>`

Common optional settings:

- `--rounds <n>`, default `1`
- `--seed <n>`, default `0`
- `--swap-sides`, default off
- `--max-actions <n>`, default `1000`
- `--unit-cap <n>`, default `50`
- `--capture-limit <n>`, default `21`
- `--day-limit <n>`, unset by default
- `--heuristic-auto-resign`, default off
- `--device <cpu|auto|cuda>`, default `auto`
- `--promotion-score-threshold <x>`, default `0.55`
- `--min-promotion-rounds <n>`, default `20`
- `--force`, default off
- `--quiet`, default off

`--out` must not already exist unless `--force` is passed. There is no append
mode. Parent directories are created when needed.

## Agent Modes

V1 supports exactly two agent modes.

`checkpoint-policy` loads a policy/value checkpoint once at evaluation startup,
encodes the current state with `StateTensor`, runs `RunPolicyValueInference`,
restricts logits to engine-legal actions from `GameState::GetValidActions` and
`ActionSpace::EncodeAction`, and chooses the legal action with the highest
policy logit. Ties are broken by the smallest encoded action index. The value
head output is recorded as diagnostic data but does not choose moves.

This mode is intentionally labeled `checkpoint-policy` because it is not the
eventual neural-guided MCTS player. Issue #166 owns a later `checkpoint-puct`
style agent that uses priors, value evaluation, and PUCT search.

`random` chooses uniformly among engine-legal atomic actions with deterministic
per-game/per-ply RNG. It is a sanity/control baseline, not a hand-coded AWBW
bot. V1 deliberately does not include a heuristic baseline.

## Schedule

V1 evaluates one explicit map and CO pair per invocation. Broader map pools,
matchup lists, and side balancing orchestration stay with #173.

`--rounds <n>` controls deterministic seed rounds. Without `--swap-sides`, each
round runs one game:

- `agent0` and `player0-co` in map template slot 0
- `agent1` and `player1-co` in map template slot 1

With `--swap-sides`, each round runs two games:

- normal orientation as above
- swapped orientation with `agent1 + player1-co` in slot 0 and
  `agent0 + player0-co` in slot 1

Map template army identities remain slot-bound because the Standard setup
contract requires each requested army type to match the template slot.

Combat RNG seeds are derived deterministically from `--seed`, round, and
orientation. Random-agent action seeds are derived deterministically from the
same run seed plus game and ply.

## Terminal Outcomes

Evaluation steps only engine-owned legal actions. After each action, the runner
calls the engine terminal checks already used by self-play.

`--heuristic-auto-resign` is optional and default off. When enabled, the report
counts `terminalReason: "heuristic-resign"` separately from other terminal
reasons.

`--max-actions` is a runner safety stop. If the limit is reached before the
engine reaches a terminal state, the game records:

- `terminalReason: "action-limit"`
- `winner: null`
- `winningAgent: null`

Action-limit games count as `noResult`, not as engine draws.

Optional `--day-limit` is an engine setting. Day-limit games are engine
terminal games and may produce player 0, player 1, or a true draw winner.

## Report

V1 writes one formatted JSON report:

```json
{
  "evaluationMetadataVersion": 1,
  "createdAt": "...",
  "versions": {
    "model": "...",
    "stateTensor": "...",
    "actionSpace": "..."
  },
  "config": {},
  "agents": [],
  "summary": {},
  "promotionRecommendation": {},
  "games": []
}
```

The report stores compact per-game records and does not store full action
histories. Self-play replay shards remain the forensic replay format.

Each game record includes:

- `round`
- `orientation`: `normal` or `swapped`
- `seed`
- `mapId`
- `players`: slot, agent index, agent type, CO, army type
- `winner`: `0`, `1`, `2`, or `null`
- `winningAgent`: `0`, `1`, or `null`
- `terminalReason`
- `actions`
- `turns`
- `elapsedMs`
- `averageActionSelectionMs`
- `invalidActionCount`

Checkpoint agents are identified from existing artifacts:

- `checkpointPath`
- `metadata.json` fields: model version, created timestamp, seed,
  architecture, and parameter count
- selected `training.json` fields when present: checkpoint role, completed
  epoch, replay path, samples trained, and aggregate losses

V1 does not compute a `model.pt` hash.

## Summary Metrics

The aggregate summary separates true draws from runner no-results:

- `wins.agent0`
- `wins.agent1`
- `draws`
- `noResult`
- `terminalReasons`
- `rounds`
- `games`
- `decisiveGames`
- `decisiveWinRate.agent0`
- `decisiveWinRate.agent1`
- `overallScoreRate.agent0`
- `overallScoreRate.agent1`
- `averageActions`
- `averageTurns`
- `averageActionSelectionMs`

Score rate treats a win as `1.0`, a loss as `0.0`, and draw/no-result as
`0.5`.

## Promotion Recommendation

The report uses a transparent v1 rule:

- recommend `promote` when agent 0 has at least `--min-promotion-rounds`, has
  `overallScoreRate >= --promotion-score-threshold`, and has
  `decisiveWinRate >= 0.50` when any decisive games exist
- recommend `reject` when enough data exists but thresholds are not met
- recommend `insufficient-data` when too few rounds complete or no-results make
  the result too weak to trust

The recommendation is informational only. No checkpoint mutation happens in
this issue.

## Tests

`-test-evaluate` should cover:

- checkpoint-policy versus random writes a report
- report includes compact per-game rows, separated wins/draws/no-results, and
  a promotion recommendation
- `--swap-sides` doubles games per round and swaps agent/CO pairs across fixed
  map slots
- existing output files are rejected unless `--force` is passed
- checkpoint agents require matching checkpoint paths
- deterministic seeds produce reproducible results for a tiny smoke schedule

## Follow-Ups

- #166: add neural-guided MCTS/PUCT checkpoint agents.
- #173: add map-pool, matchup, and side-balancing orchestration.
- A later hardening issue may add model weight hashes to evaluation reports.
