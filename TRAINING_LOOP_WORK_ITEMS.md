# Advance Wars Self-Play Training Loop Work Items

This file tracks the major work needed to turn the current Advance Wars simulator into an AlphaZero-style self-play training system.

## Current Direction

- Use action-level MCTS first. A full player turn is represented as a sequence of same-player atomic actions ending with `EndTurn`.
- Keep the game engine as the single source of truth for legal actions and state transitions.
- Start with deterministic or seedable simulations before adding stochastic luck back into training.
- Build self-play data generation before attempting full neural network training.

## Open Product Decisions

- [ ] Decide whether v1 targets full AWBW fidelity or a simplified deterministic subset.
- [ ] Decide how combat luck should work during self-play: disabled, averaged, or seeded.
- [ ] Decide whether v1 includes fog, hidden units, CO powers, transports, naval units, and all unit types.
- [ ] Decide the first map set for self-play. Small maps will make early iteration much faster.
- [ ] Decide whether training should run inside the C++ executable only, or whether C++ should expose an environment API for a Python training process.

## Phase 0: Stabilize The Simulator

- [ ] Run the JSON subsystem tests from a current build and make failures visible in CI or local output.
- [ ] Fix action deserialization for `unloadIndex`; it is read but not assigned to `Action::m_optUnloadIndex`.
- [ ] Fix capture-limit property counting. The current lab/com tower exclusion condition appears to use `||` where `&&` was likely intended.
- [ ] Audit unit count caching around load/unload, clone, add, and destroy paths before relying on unit-cap logic in training.
- [ ] Replace hardcoded local paths such as `D:/awai`, `D:/MNIST`, and local libtorch directories with config or command-line options.
- [ ] Add a maximum turn/action limit outcome for self-play games so training cannot hang indefinitely.
- [ ] Make combat RNG deterministic, seedable, or policy-controlled from the self-play runner.

## Phase 1: Trainable Environment API

- [ ] Add a small environment wrapper around `GameState`.
- [ ] Expose `reset`, `legalActions`, `step`, `isTerminal`, `currentPlayer`, and `winner` operations.
- [ ] Add stable state serialization for replay data.
- [ ] Add state tensor encoding in `Tensor.cpp`.
- [ ] Add a stable action encoding scheme for every `Action` variant.
- [ ] Add legal action masks aligned with the action encoding.
- [ ] Add tests proving encode/decode preserves actions, including move-attack, unload, buy, powers, and end-turn.

## Phase 2: MCTS Improvements

- [ ] Separate pure rollout MCTS from neural-network-guided MCTS.
- [ ] Expand one child at a time or track unexpanded actions to avoid expanding every legal action immediately.
- [ ] Replace `rand()` rollout action selection with a seeded RNG object.
- [ ] Add PUCT selection using policy priors once a policy head exists.
- [ ] Store visit counts per legal action for training targets.
- [ ] Handle same-player action sequences explicitly in value backup. The player only changes on `EndTurn`.
- [ ] Add temperature controls for early-game exploration and late-game exploitation.
- [ ] Add MCTS search limits by simulations, wall-clock time, or node count.

## Phase 3: Self-Play Data Generation

- [ ] Add a self-play command-line entry point.
- [ ] Generate games using MCTS-selected actions.
- [ ] Record each training sample as state tensor, legal action mask, MCTS visit policy, current player, and final outcome.
- [ ] Store replay data in a versioned format.
- [ ] Add replay validation that can reconstruct a game from recorded actions.
- [ ] Add basic metrics: game length, winner, resignations, average branching factor, and search time per move.
- [ ] Add parallel self-play workers after the single-worker path is reliable.

## Phase 4: Neural Network

- [ ] Define model input planes for terrain, ownership, units, health, ammo, fuel, moved flags, capture points, funds, powers, turn/player context, and optional fog state.
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

- [ ] Create deterministic combat RNG for self-play.
- [ ] Implement state tensor encoding.
- [ ] Implement action encoding and legal action masks.
- [ ] Refactor MCTS for action-level self-play and same-player turn sequences.
- [ ] Add self-play replay writer.
- [ ] Add policy/value network scaffold.
- [ ] Add training command-line entry point.
- [ ] Add checkpoint evaluation harness.
