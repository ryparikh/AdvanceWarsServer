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
- [ ] Decide how combat luck should work during self-play: disabled, averaged, or seeded.
- [ ] Decide whether v1 includes fog, hidden units, CO powers, transports, naval units, and all unit types.
- [ ] Decide the first map set for self-play. Small maps will make early iteration much faster.
- [ ] Decide whether training should run inside the C++ executable only, or whether C++ should expose an environment API for a Python training process.

## Phase 0: Stabilize The Simulator

- [ ] Run the JSON subsystem tests from a current build and make failures visible in CI or local output.
- [ ] Fix action deserialization for `unloadIndex`; it is read but not assigned to `Action::m_optUnloadIndex`. Track under submitted-action validation work in #67 if still present.
- [x] Fix capture-limit property counting. Capture-limit wins count Cities, Bases, Airports, Ports, and HQs, excluding Labs and Com Towers (#73).
- [ ] Audit unit count caching around load/unload, clone, add, and destroy paths before relying on unit-cap logic in training.
- [ ] Replace hardcoded local paths such as `D:/awai`, `D:/MNIST`, and local libtorch directories with config or command-line options.
- [ ] Add a maximum turn/action limit outcome for self-play games so training cannot hang indefinitely. Coordinate with day-limit and terminal metadata work in #74 and #82.
- [x] Make combat RNG deterministic, seedable, or policy-controlled from the self-play runner (#2).

## Phase 1: Trainable Environment API

- [ ] Add a small environment wrapper around `GameState`; the REST/self-play API contract is tracked by #66.
- [ ] Expose `reset`, `legalActions`, `step`, `isTerminal`, `currentPlayer`, and `winner` operations.
- [ ] Add stable state serialization for replay data.
- [x] Add state tensor encoding in `Tensor.cpp` (#3).
- [ ] Add a stable action encoding scheme for every `Action` variant (#4).
- [ ] Add legal action masks aligned with the action encoding (#4).
- [ ] Add tests proving encode/decode preserves actions, including move-attack, unload, buy, powers, and end-turn.

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

- [ ] Add a self-play command-line entry point.
- [ ] Generate games using MCTS-selected actions.
- [ ] Record each training sample as state tensor, legal action mask, MCTS visit policy, current player, and final outcome.
- [ ] Store replay data in a versioned format.
- [ ] Add replay validation that can reconstruct a game from recorded actions.
- [ ] Add basic metrics: game length, winner, resignations, average branching factor, and search time per move.
- [ ] Add parallel self-play workers after the single-worker path is reliable.

## Phase 4: Neural Network

- [x] Define model input planes via `standard-gl-v1-state` in `docs/STATE_TENSOR.md`.
- [ ] Define policy head output shape matching the action encoding.
- [ ] Define value head output as win/loss value from the current player's perspective.
- [ ] Add model save/load checkpoints.
- [ ] Add inference batching for MCTS.
- [ ] Add CPU-only fallback so development does not depend on CUDA being configured.

## Phase 5: Training Loop

- [ ] Add supervised-style training over replay samples.
- [ ] Train policy with cross entropy against MCTS visit distributions.
- [ ] Train value with mean squared error or cross entropy against final outcomes.
- [ ] Add replay buffer sampling and retention policy.
- [ ] Add checkpoint promotion/evaluation against previous models.
- [ ] Add command-line options for epochs, batch size, learning rate, device, checkpoint path, and replay path.

## Phase 6: Evaluation And Iteration

- [ ] Add head-to-head evaluation between checkpoints.
- [ ] Add random-player and heuristic-player baselines.
- [ ] Add deterministic smoke tests for short self-play games.
- [ ] Track win rate, average value loss, policy loss, game length, invalid action count, and search throughput.
- [ ] Add regression maps that cover captures, purchases, transport logic, indirect combat, powers, and end-game conditions.

## Candidate GitHub Issues

- [x] #2 Create deterministic combat RNG for self-play.
- [x] #3 Implement state tensor encoding.
- [ ] #4 Implement action encoding and legal action masks.
- [ ] #5 Refactor MCTS for action-level self-play and same-player turn sequences.
- [ ] #6 Add self-play replay writer.
- [ ] #7 Add policy/value network scaffold.
- [ ] #8 Add training command-line entry point.
- [ ] #9 Add checkpoint evaluation harness.
- [ ] #166 Add neural-guided MCTS with PUCT.
- [ ] #167 Add reusable MCTS search tree re-rooting.
- [ ] #168 Add wall-clock MCTS search limits.
- [ ] #65 Define and enforce normal Global League Standard game settings.
- [ ] #66 Add REST game lifecycle and step API contract for self-play.
- [ ] #67 Validate and atomically reject illegal submitted actions.
- [x] #68 Make REST action stepping explicit and remove implicit auto-end-turn.
- [x] #69 Gate heuristic auto-resign behind an explicit setting that defaults off.
