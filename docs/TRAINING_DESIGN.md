# Self-Play Training Design

This document describes the recommended first training path for the Advance Wars self-play model. The goal is to train one policy/value model for normal Global League Standard games while keeping the engine as the source of truth for rules, state transitions, and legal actions.

## Scope

V1 targets Standard GL only. The in-game policy uses the `standard-gl-v1` action space from `docs/ACTION_SPACE.md`: a fixed `27x23` board, source-cell action planes, and three appended global actions for end turn and powers.

The model should choose only in-game actions. Pregame CO selection should be handled outside this action space and trained/evaluated from matchup statistics later.

## Environment Loop

Self-play should run through an environment wrapper around `GameState`:

1. Create a Standard GL game from a map, CO pair, settings, and deterministic seed.
2. Encode the current state tensor from the current player's perspective.
3. Ask the engine for legal actions and encode them as sparse action indices.
4. Run MCTS using the current policy/value model.
5. Pick an action from MCTS visit counts, then step the engine with that action.
6. Record the training sample.
7. Repeat until terminal, day limit, or an explicit training stop condition.

The engine should reject illegal submitted actions. Training code should never reimplement rules or infer legality from tensors.

## Model Shape

Use a shared convolutional or residual trunk over the fixed board. The policy head should produce action planes over `27x23` plus the three global logits. It should not use a large fully connected classifier over all `1,554,366` actions.

The value head should output the expected game result from the current player's perspective.

The v1 scaffold is `standard-gl-policy-value-v1`. It consumes `standard-gl-v1-state` tensors shaped `[N, 219, 23, 27]`, runs a small residual trunk, emits policy logits shaped `[N, ActionSpace::ActionCount()]`, and emits value predictions shaped `[N, 1]` in `[-1, 1]`. The default architecture is intentionally modest: 64 hidden channels, 4 residual blocks, and 8 group-norm groups. The command-line initializer can make smaller smoke-test models with `--hidden-channels`, `--res-blocks`, and `--norm-groups`; larger production-scale experiments are tracked separately by #177.

Policy logits keep the action-space layout intact: `2503` source-cell planes over `23x27`, followed by the three global actions. MCTS and training should mask or normalize only the sparse legal action indices produced by the engine/action-space layer; the model does not infer legality from the tensor.

Checkpoints are directory bundles containing both `metadata.json` and `model.pt`. `metadata.json` is the compatibility manifest: model version, state tensor version/shape, action-space version/count, policy/value output sizes, architecture knobs, seed, validated device, and parameter count. `model.pt` contains the learned weights and is required; it cannot be reconstructed from `metadata.json`.

Start small enough to fit comfortably on an 8 GB GPU:

- mixed precision when CUDA is available
- small batches such as 1, 2, 4, or 8
- gradient accumulation if a larger effective batch helps stability
- modest channel and block counts before scaling up

Longer training with smaller batches is acceptable. The first priority is stable self-play and replay quality, not maximum GPU throughput.

## Replay Format

Replay data should be versioned and compact. Each sample should include:

- action-space version
- game/setup metadata, including map id, COs, seed, and rules settings
- state tensor or enough serialized state data to rebuild it
- current player
- selected action index
- sparse legal action indices
- sparse MCTS visit counts for legal or visited actions
- final outcome from the sampled player's perspective

Do not store dense legal masks in replay. A dense mask for `standard-gl-v1` is about 1.55 MB per state, which grows too quickly. Materialize dense masks only for the active training batch if the loss implementation needs them.

The v1 C++ replay writer emits sparse JSONL shards using `standard-gl-self-play-replay-v1`; see `docs/SELF_PLAY_REPLAYS.md` for the command and schema. The canonical replay stores full initial/final engine states, raw action history, sparse legal action indices, sparse positive MCTS visit counts, per-sample tensor checksums, and current-player-relative outcomes. Dense tensors and masks remain derived data.

## Training Loop

V1 training should run inside the C++ executable with LibTorch. Do not expose a Python environment API yet; the replay shard format is the language-neutral boundary for future tools if Python becomes useful for experimentation, reporting, or distributed training.

Training should sample replay positions, rebuild tensors, expand sparse legal indices for masking, and optimize two losses:

- policy loss against the MCTS visit distribution
- value loss against the final outcome

Checkpoints should be evaluated against the current best checkpoint on fixed maps, fixed seeds, and a held-out map set. Promote a checkpoint only when it improves head-to-head results or clearly improves selected metrics.

## Metrics

Track at least:

- win rate by checkpoint, map, side, and CO pair
- game length in days and atomic actions
- average legal action count
- search time per move
- policy loss and value loss
- invalid action attempts
- terminal reason

CO matchup and pregame selection metrics should feed the later CO picker, not the in-game action model.

## Recommended First Milestone

The first useful milestone is a single-worker self-play runner that writes validated sparse replay shards from deterministic games on a small Standard GL map set. Once replay validation is reliable, add the neural trainer and only then scale to parallel self-play workers.
