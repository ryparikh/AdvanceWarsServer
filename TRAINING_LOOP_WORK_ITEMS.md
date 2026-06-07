# Advance Wars Self-Play Training Loop Work Items

This file tracks the major work needed to turn the current Advance Wars simulator into an AlphaZero-style self-play training system.

The rules/API completeness target for the initial playable environment is normal Global League Standard play. See `STANDARD_ENGINE_COMPLETENESS.md` for the current implementation matrix and issue index.

## Current Direction

- Use action-level MCTS first. A full player turn is represented as a sequence of same-player atomic actions ending with `EndTurn`.
- Keep the game engine as the single source of truth for legal actions and state transitions.
- Start with deterministic or seedable simulations before adding stochastic luck back into training.
- Build self-play data generation before attempting full neural network training.

## Open Product Decisions

- [x] Decide whether v1 targets full AWBW fidelity or a simplified deterministic subset. V1 targets normal Global League Standard play, with other modes explicitly deferred in `STANDARD_ENGINE_COMPLETENESS.md`.
- [x] Decide how combat luck should work during self-play. V1 uses seeded deterministic combat RNG derived from the self-play setup seed; averaged or disabled luck can remain a later experiment if training quality needs it.
- [x] Decide whether v1 includes fog, hidden units, CO powers, transports, naval units, and all unit types. V1 targets normal Global League Standard: fog disabled, CO powers enabled, transports/naval units in scope, and Standard unit bans/settings enforced through setup.
- [x] Decide the first map set for self-play. The bootstrap runner/API map catalog currently supports `lefty` and `mcts`; broader map pools and side balancing are tracked by #173.
- [x] Decide whether training should run inside the C++ executable only, or whether C++ should expose an environment API for a Python training process. V1 training stays inside the C++ executable with LibTorch; a Python environment API is deferred unless replay-driven C++ training becomes the bottleneck.

## Phase 0: Stabilize The Simulator

- [x] Run the JSON subsystem tests from a current build and make failures visible in CI or local output. Confirmed locally on 2026-06-06 with `-test test/json`.
- [x] Fix action deserialization for `unloadIndex`; action JSON now assigns `Action::m_optUnloadIndex`, equality includes it, and REST/action-space tests cover unload-index handling (#4, #67).
- [x] Fix capture-limit property counting. Capture-limit wins count Cities, Bases, Airports, Ports, and HQs, excluding Labs and Com Towers (#73).
- [ ] Audit unit count caching around load/unload, clone, add, and destroy paths before relying on unit-cap logic in training.
- [ ] Replace remaining hardcoded local paths such as `D:/awai` with config or command-line options. LibTorch now uses `LIBTORCH_ROOT`, and the old MNIST experiment only runs when `-torchlib --mnist-path <path>` is provided.
- [x] Add a maximum action limit outcome for self-play games so training cannot hang indefinitely. `-self-play` records `terminalReason: "action-limit"` and null winner for runner safety stops (#6).
- [x] Make combat RNG deterministic, seedable, or policy-controlled from the self-play runner (#2).

## Phase 1: Trainable Environment API

- [x] Add a small environment wrapper around `GameState`; `RestGameService`, `StandardGameSetup`, and the self-play runner now create Standard games and step through engine-owned actions (#66).
- [x] Expose create/reset-by-recreate, legal actions, step, terminal status, current player, and winner through the REST/self-play contract. V1 intentionally recreates from setup instead of exposing a mutable reset endpoint (#66).
- [x] Add stable state serialization for replay data via full initial/final engine states and `SerializeGameStateForReplay`.
- [x] Add state tensor encoding in `Tensor.cpp` (#3).
- [x] Add a stable action encoding scheme for every in-game `Action` variant (#4).
- [x] Add legal action masks aligned with the action encoding (#4).
- [x] Add tests proving encode/decode preserves actions, including move-attack, unload, buy, powers, and end-turn.

## Phase 2: MCTS Improvements

- [x] Separate pure rollout MCTS from neural-network-guided MCTS. Neural-guided PUCT is tracked by #166.
- [x] Expand one child at a time or track unexpanded actions to avoid expanding every legal action immediately.
- [x] Replace `rand()` rollout action selection with a seeded RNG object.
- [ ] Add PUCT selection using policy priors once a policy head exists (#166).
- [x] Store visit counts per legal action for training targets.
- [x] Handle same-player action sequences explicitly in value backup.
- [x] Add temperature controls for early-game exploration and late-game exploitation.
- [x] Add MCTS search limits by simulation count, rollout action count, and node count. Wall-clock limits are tracked by #168.

## Phase 3: Self-Play Data Generation

- [x] Add a self-play command-line entry point (`-self-play`).
- [x] Generate games using MCTS-selected atomic actions.
- [x] Record each training sample as a reconstructable sparse row: tensor checksum, sparse legal action indices, positive MCTS visit counts, current player, selected action index, and final outcome.
- [x] Store replay data in the versioned `standard-gl-self-play-replay-v1` JSONL format.
- [x] Add replay validation that reconstructs a game from recorded actions (`-validate-replay`).
- [x] Add basic metrics: action count, turn count, winner, resignations, average branching factor, invalid action count, and search time per action.
- [ ] Add map-pool, matchup, and side-balancing orchestration on top of the single-map runner (#173).
- [ ] Add optional compressed replay shard support after plain JSONL replay loading is stable (#174).
- [ ] Add parallel self-play workers after the single-worker path is reliable.

## Phase 4: Neural Network

- [x] Define model input planes via `standard-gl-v1-state` in `docs/STATE_TENSOR.md`.
- [x] Define policy head output shape matching the action encoding: source-cell planes over `27x23` plus global logits in `docs/TRAINING_DESIGN.md`.
- [x] Define value head output as expected win/loss result from the current player's perspective in `docs/TRAINING_DESIGN.md`.
- [x] Add a LibTorch policy/value model scaffold for `standard-gl-policy-value-v1` (#7).
- [x] Add model save/load checkpoints with strict `metadata.json` compatibility checks and required `model.pt` weights (#7).
- [ ] Add inference batching for MCTS.
- [x] Add CPU-only fallback so development does not depend on CUDA being configured (#7).
- [x] Add focused model smoke tests for forward shape, real Standard tensor inference, deterministic init, and checkpoint rejection (`-test-model`) (#7).

## Phase 5: Training Loop

- [ ] Add supervised-style training over replay samples.
- [ ] Train policy with cross entropy against MCTS visit distributions.
- [ ] Train value with mean squared error or cross entropy against final outcomes.
- [ ] Add a replay shard reader/dataset that reconstructs tensors, sparse legal indices, dense batch-local masks, visit distributions, and outcomes from `standard-gl-self-play-replay-v1`.
- [ ] Add replay buffer sampling and retention policy.
- [ ] Add optional materialized replay cache for faster repeated training loads (#172).
- [ ] Add checkpoint promotion/evaluation against previous models.
- [ ] Add command-line options for epochs, batch size, learning rate, device, checkpoint path, and replay path.

## Phase 6: Evaluation And Iteration

- [ ] Add head-to-head evaluation between checkpoints.
- [ ] Add random-player and heuristic-player baselines.
- [x] Add deterministic smoke tests for short self-play games (`-test-replay`).
- [ ] Track win rate, average value loss, policy loss, game length, invalid action count, and search throughput.
- [ ] Track CO matchup statistics for pregame CO selection separately from the in-game action model (#164).
- [ ] Add regression maps that cover captures, purchases, transport logic, indirect combat, powers, and end-game conditions.

## Candidate GitHub Issues

- [x] #2 Create deterministic combat RNG for self-play.
- [x] #3 Implement state tensor encoding.
- [x] #4 Implement action encoding and legal action masks.
- [x] #5 Refactor MCTS for action-level self-play and same-player turn sequences.
- [x] #6 Add self-play replay writer.
- [x] #7 Add policy/value network scaffold.
- [ ] #8 Add training command-line entry point.
- [ ] #9 Add checkpoint evaluation harness.
- [ ] #164 Track CO matchup statistics for pregame selection.
- [ ] #166 Add neural-guided MCTS with PUCT.
- [ ] #167 Add reusable MCTS search tree re-rooting.
- [ ] #168 Add wall-clock MCTS search limits.
- [ ] #172 Add optional materialized replay cache for faster training loads.
- [ ] #173 Add self-play orchestration for map pools, matchups, and side balancing.
- [ ] #174 Add compressed self-play replay shard support.
- [x] #65 Define and enforce normal Global League Standard game settings.
- [x] #66 Add REST game lifecycle and step API contract for self-play.
- [x] #67 Validate and atomically reject illegal submitted actions.
- [x] #68 Make REST action stepping explicit and remove implicit auto-end-turn.
- [x] #69 Gate heuristic auto-resign behind an explicit setting that defaults off.
