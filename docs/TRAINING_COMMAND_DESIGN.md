# Training Command Design

Issue: https://github.com/ryparikh/AdvanceWarsServer/issues/8

This document records the design for the first replay-driven training command in
AdvanceWarsServer. The goal is a practical v1 path from validated self-play
replay shards to a saved policy/value checkpoint, while keeping the engine,
state tensor, action space, and replay schema as the source of truth.

## Goal

Add a command-line entry point that can:

- load `standard-gl-self-play-replay-v1` replay shards,
- reconstruct model-ready batches from sparse replay samples,
- optimize `standard-gl-policy-value-v1` with LibTorch,
- write checkpoint bundles compatible with the existing model scaffold,
- write training-run provenance beside the model checkpoint, and
- print enough metrics to know that a smoke training run consumed data and made
  progress.

This is intentionally the first trainer, not the final distributed AlphaZero
system. It should be good enough to run a supervised-style training smoke test
from existing MCTS replay data and produce a checkpoint that later evaluation
work can compare.

## Repo-Decided Constraints

These branches of the design tree are already answered by code or existing
docs:

- Training v1 runs inside the C++ executable with LibTorch. A Python
  environment API is deferred.
- The model ABI is `standard-gl-policy-value-v1`.
- The model input is `standard-gl-v1-state`, shaped `[N, 219, 23, 27]`.
- The policy output is a dense `ActionSpace::ActionCount()` logit vector.
- The value output is a scalar in `[-1, 1]`, from the current player's
  perspective.
- Device names follow the policy/value model scaffold: `cpu`, `auto`, and
  `cuda`.
- Replay input is sparse JSONL in `standard-gl-self-play-replay-v1`.
- Replay shards store sparse legal action indices and sparse positive MCTS
  visit counts; dense state tensors and dense legal masks are derived at load
  time.
- Checkpoints are directory bundles with strict `metadata.json` compatibility
  and required `model.pt` weights.
- The existing replay module validates shards but does not expose a reusable
  replay-dataset reader for training.

## Proposed V1 Shape

The first training command should be a new top-level executable option:

```powershell
..\x64\Release\AdvanceWarsServer.exe -train `
  --replay <file-or-directory> `
  --checkpoint-in <checkpoint-dir> `
  --checkpoint-out <checkpoint-dir> `
  --epochs 1 `
  --batch-size 32 `
  [--max-samples <n>] `
  --learning-rate 0.001 `
  [--weight-decay 0.0001] `
  [--policy-loss-weight 1.0] `
  [--value-loss-weight 1.0] `
  [--max-grad-norm 0] `
  [--seed 0] `
  [--log-every-batches <n>] `
  --device auto `
  [--force] `
  [--checkpoint-every-epochs <n> --checkpoint-dir <dir>]
```

Decision: require an input checkpoint for issue #8. A separate
`-model-init` command already creates a compatible checkpoint bundle, and
requiring explicit initialization keeps architecture, seed, and compatibility
metadata owned by the checkpoint system from issue #7. `-train` preserves the
architecture and compatibility metadata from `--checkpoint-in`; it does not
accept architecture overrides.

## Data Flow

1. Resolve `--replay` as either one JSONL file or a directory of `.jsonl`
   shards. Directory entries are sorted by path before validation and seeded
   shuffle.
2. Validate each replay shard before consuming samples. In directory mode, any
   invalid shard fails the training command.
3. Load compact replay sample records into memory. Do not eagerly materialize
   dense state tensors for every sample. If `--max-samples <n>` is provided,
   keep the first `n` samples after deterministic replay ordering and parsing,
   before epoch shuffle.
4. For each training batch, reconstruct each needed state from `initialState`
   plus the action prefix before the sample. V1 does not cache reconstructed
   states across batches.
5. Regenerate legal action indices for the reconstructed state and fail if they
   differ from the replay sample.
6. Lazily encode `StateTensor` values for the current batch and compare the
   computed checksum to the replay sample.
7. Read sparse legal action indices and sparse visit counts from the sample.
8. Convert the sparse visits to a target distribution over only legal actions.
9. Gather model logits at legal action indices or apply a batch-local legal
   mask before the policy loss.
10. Train value predictions against sample `outcome`. The final partial batch is
   included when sample count is not divisible by `--batch-size`.
11. Save the final checkpoint bundle only after all epochs complete
   successfully, plus optional periodic checkpoint bundles when configured.

All checkpoint output paths are validated before replay loading or optimization
starts. `--checkpoint-out` must not already exist unless `--force` is passed.
With `--force`, v1 may only overwrite known bundle files: `metadata.json`,
`model.pt`, and `training.json`. Periodic checkpoint directories must also be
checked up front so a long run does not fail only when the first periodic save
is due.

`--checkpoint-in` and `--checkpoint-out` must resolve to different paths, even
when `--force` is passed. V1 does not support in-place checkpoint mutation.

Periodic checkpoints are written under `--checkpoint-dir` as deterministic
epoch subdirectories such as `epoch-000001`, `epoch-000002`, and so on. The
trainer only creates a periodic subdirectory when that epoch checkpoint is due,
but it checks planned periodic paths for conflicts during startup. Periodic
`training.json` files should record `checkpointRole: "periodic"` and the
completed epoch.

Final and periodic checkpoints should ideally be written atomically: write the
complete bundle to a temporary sibling directory, validate the expected files
exist, and then move it into the final checkpoint path. This hardening is
tracked by #182 and is an optional stretch for #8.

If an atomic checkpoint write fails, the trainer should try to delete only the
temporary directory it created. If cleanup fails, leave the temp directory in
place and print its path in the failure summary.

## Public C++ Surface

The CLI should be a thin wrapper over reusable trainer APIs. Issue #8 should
introduce focused C++ types such as `TrainingOptions`, `TrainingSummary`,
`TrainingError`, and `RunTraining(...)`, plus helpers for replay dataset loading
and lazy batch assembly. `main.cpp` should only dispatch `-train` and
`-test-train`.

The initial file layout should be:

- `AdvanceWarsServer/inc/Training.h`
- `AdvanceWarsServer/src/Training.cpp`
- `AdvanceWarsServer/inc/TrainingCommand.h`
- `AdvanceWarsServer/src/TrainingCommand.cpp`
- `AdvanceWarsServer/inc/TrainingTest.h`
- `AdvanceWarsServer/src/TrainingTest.cpp`

Replay-dataset and batch-assembly helper types can remain private to
`Training.cpp` until they become large enough to deserve their own module.

## Option Validation

The command should fail early with clear errors for invalid numeric ranges:

- `--epochs` must be greater than `0`.
- `--batch-size` must be greater than `0`.
- `--learning-rate` must be finite and greater than `0`.
- `--weight-decay` must be finite and greater than or equal to `0`.
- `--policy-loss-weight` and `--value-loss-weight` must be greater than or
  equal to `0`, finite, with at least one positive.
- `--max-grad-norm` must be finite and greater than or equal to `0`.
- `--seed` must be greater than or equal to `0`.
- `--max-samples`, when present, must be greater than `0`.
- `--checkpoint-every-epochs` must be greater than or equal to `0`; `0`
  disables periodic checkpointing.
- `--log-every-batches` must be greater than or equal to `0`; `0` disables
  batch progress logging.

## Final Design Closure

The remaining smaller design branches use these adopted defaults:

1. Empty replay inputs fail. A replay file or directory that resolves to zero
   usable samples is an error, including a directory with no `.jsonl` shards or
   a `--max-samples` setting that would leave no samples.
2. Epoch shuffling is deterministic. The same replay input, `--seed`, and
   options produce the same sample order. Each epoch uses a seed derived from
   the trainer seed and epoch number so epochs do not repeat the exact same
   order.
3. #8 does not add a validation split, holdout metrics, early stopping, or
   best-checkpoint selection. Those belong with evaluation and promotion work
   in #9.
4. #8 is single-process and single-worker. It does not add background prefetch,
   parallel replay loading, or asynchronous batch assembly. Larger data-loading
   performance work belongs to #180.
5. `training.json` is the machine-readable training artifact. Console output is
   human-readable.
6. `training.json` should include a `trainingMetadataVersion` field starting at
   `1`, plus enough resolved options and input/output paths to reproduce the
   smoke run.
7. During optimization the model must be in training mode. Checkpoint
   validation or any inference-only checks should use eval/no-grad mode.
8. Checkpoint save/load compatibility stays centralized in the policy/value
   checkpoint layer. The trainer may add `training.json`, but it should not
   weaken existing `metadata.json` validation.
9. The trainer should return structured `TrainingError` values with stable
   codes and context fields rather than relying only on exception text.
10. #8 does not implement neural-guided self-play or PUCT priors. It consumes
   replay data generated by the existing MCTS/self-play path.

## Checkpoint Outputs

Training-produced checkpoint bundles should include `training.json` beside
`metadata.json` and `model.pt`. `metadata.json` remains the strict model
compatibility manifest owned by the policy/value checkpoint layer.
`training.json` records run provenance and aggregate metrics, including replay
paths, resolved sample count, epochs completed, batch size, optimizer settings,
loss weights, trainer seed, device, elapsed time, checkpoint role, and latest
aggregate losses. It should also include a compact per-epoch history array with
epoch number, samples, batches, average policy loss, average value loss, average
total loss, elapsed seconds, and periodic checkpoint path when one was written.

V1 checkpoint continuation is model-only: any saved checkpoint can be used as a
future `--checkpoint-in`, but AdamW optimizer state is not persisted or
restored. Full optimizer-state resume is deferred to #181 and should be treated
as a prerequisite before spending serious compute on a competitive model.

`--device auto` uses CUDA only when the binary was built with CUDA support and
CUDA is available; otherwise it uses CPU. `--device cuda` fails when CUDA is not
available. The resolved device is printed and written to `training.json`.

## Losses

Policy loss should train against the normalized MCTS visit distribution after
illegal actions are excluded. Run `log_softmax` over all legal actions for the
sample, including legal actions with zero visits. Assign target probability
only to positive-visit actions. This penalizes probability mass assigned to
legal-but-unvisited actions while keeping illegal actions out of the loss.

The implementation can avoid materializing a full dense target vector by
gathering legal logits, applying `log_softmax` over that legal subset, and
computing the negative weighted log probability for visited actions.
`selectedActionIndex` is useful for validation and debugging, but it does not
add a separate supervised action-pick loss in v1.

Value loss should use mean squared error between the scalar value head and the
sample outcome. Samples from games that end by the self-play action limit stay
in the batch with value target `0`, matching the replay writer's v1 outcome
contract.

V1 total loss should be:

```text
totalLoss = policyLoss * policyLossWeight + valueLoss * valueLossWeight
```

Both weights default to `1.0` and are configurable with
`--policy-loss-weight` and `--value-loss-weight`.

## Optimizer

V1 should use AdamW only. The command exposes `--learning-rate` and
`--weight-decay`; optimizer family selection is deferred until there is a
measured reason to add it. The learning rate is constant for issue #8.
Learning-rate schedules and scheduler checkpoint state are deferred to #181.
Training uses float32 only; mixed precision and scaler checkpointing are also
deferred to #181.

V1 may optionally clip gradients by global norm when `--max-grad-norm` is
greater than `0`. The default is disabled.

## Metrics

V1 `-train` should print:

- sample count,
- epoch,
- batch count,
- average policy loss,
- average value loss,
- average total loss,
- elapsed time,
- device,
- input checkpoint, and
- output checkpoint.

Successful runs print one startup summary and one progress line per epoch by
default. Batch-level progress is opt-in through `--log-every-batches <n>`.

Win rate, head-to-head results, map/side/CO breakdowns, resignations, game
length, search throughput during evaluation, promotion decisions, and
best-checkpoint selection belong to the checkpoint evaluation harness in #9.

On failure, `-train` should print a concise stderr summary with any progress
known at the failure point: replay shards resolved, samples loaded, epoch and
batch reached, last completed periodic checkpoint, and the structured error
context.

## Tests

Issue #8 should add a focused `-test-train` command. It should stay separate
from `-test-model` and `-test-replay` so trainer failures are easy to isolate.
The focused gate should cover replay dataset loading, lazy batch assembly, loss
computation, tiny deterministic training, checkpoint output, overwrite
failures, and structured error diagnostics.

The core deterministic smoke test should generate a tiny temporary replay shard
with the current replay writer path where practical, initialize a very small
model, run one epoch with a batch size that creates a partial final batch,
assert that the final checkpoint and `training.json` are written, assert sample
and batch counts, and assert that at least one model parameter changed from the
input checkpoint. Tiny checked-in malformed fixtures are acceptable for
error-path tests when generating malformed data would be cumbersome.

## Documentation Updates

When #8 is implemented, update:

- `BUILD_AND_TEST.md` with `-train` and `-test-train` command references.
- `README.md` with a basic replay-to-checkpoint smoke command sequence.
- `TRAINING_LOOP_WORK_ITEMS.md` to mark the #8 training-command work complete
  and point deferred scale/resume/evaluation work to #180, #181, and #9.

The docs should include a minimal end-to-end smoke sequence that initializes a
small model checkpoint, generates a tiny self-play replay shard, trains for one
epoch, and writes a new checkpoint.

## Open Decisions

The following decisions should be resolved before implementation:

No unresolved decisions remain from the initial design interview.

## Decision Log

- `-train` requires `--checkpoint-in <checkpoint-dir>` for v1. It does not
  initialize a fresh model implicitly. Use `-model-init` first to create the
  starting checkpoint bundle.
- `-train` validates replay shards by default before training. A future
  performance option may skip validation only after replay loading becomes a
  measured bottleneck.
- V1 loads all reconstructed samples into memory and performs a seeded shuffle
  each epoch. Streaming, bounded buffers, reservoir sampling, or other
  large-corpus loading modes are deferred to #180.
- V1 exposes optional `--max-samples <n>`, default unlimited, for quick smoke
  runs. It selects the first `n` samples after deterministic replay ordering and
  parsing, before epoch shuffle.
- V1 writes the final `--checkpoint-out` bundle and also supports optional
  periodic checkpointing with `--checkpoint-every-epochs <n>` and
  `--checkpoint-dir <dir>`. Periodic checkpoints are for interruption recovery
  and experiment inspection; checkpoint promotion and best-model selection stay
  with the evaluation harness in #9.
- `--checkpoint-out` preserves the architecture and compatibility metadata from
  `--checkpoint-in`. `-train` does not accept architecture overrides.
- V1 `-train` prints sample count, epoch, batch count, average policy loss,
  average value loss, average total loss, elapsed time, device, input
  checkpoint, and output checkpoint. Evaluation metrics and promotion reporting
  are tracked by #9.
- V1 uses AdamW only, with `--learning-rate` and `--weight-decay` exposed as
  trainer options. The learning rate is constant; learning-rate schedules are
  deferred to #181.
- V1 trains in float32 only. Mixed precision is deferred to #181 so scaler
  state can be handled with other resumable trainer state.
- V1 exposes optional global-norm gradient clipping with `--max-grad-norm`.
  The default `0` disables clipping.
- V1 exposes `--policy-loss-weight` and `--value-loss-weight`, both defaulting
  to `1.0`.
- V1 trains policy only against the normalized MCTS visit distribution.
  `selectedActionIndex` is not a separate policy-loss target.
- Policy `log_softmax` is computed over all legal actions. Positive target mass
  comes only from positive-visit actions; zero-visit legal actions remain in the
  denominator; illegal actions are excluded.
- Samples from games that ended by `action-limit` are included in value
  training with target `0`.
- `--replay` accepts either a single JSONL file or a directory. Directory mode
  consumes `.jsonl` shards sorted by path before sample reconstruction and
  seeded epoch shuffling.
- In directory mode, one invalid shard fails the command. V1 does not silently
  skip bad replay files.
- V1 stores compact sample records in memory and lazily reconstructs/encodes
  `StateTensor` batches during training. Full streaming and bounded-buffer
  loading are deferred to #180.
- V1 reconstructs sample states from `initialState` plus the action prefix each
  time the sample is used. It does not cache reconstructed states across
  batches.
- During batch assembly, v1 regenerates legal action indices for each
  reconstructed state and fails if they differ from replay-stored
  `legalActionIndices`.
- During batch assembly, v1 recomputes each encoded `StateTensor` checksum and
  fails with shard/game/ply context if it differs from the replay sample.
- Any invalid sample fails the whole training command. V1 does not skip or
  quarantine individual samples.
- The final `--checkpoint-out` bundle is written only after all epochs complete
  successfully. Failed runs may leave previously completed periodic checkpoints,
  but they do not write a final checkpoint that appears complete.
- Failed runs print a concise stderr summary with resolved replay/progress
  context and the structured error.
- Successful runs print a startup summary and one progress line per epoch by
  default. `--log-every-batches <n>` enables batch-level progress logs.
- Issue #8 adds a focused `-test-train` command for trainer dataset, loss,
  checkpoint, and error-path coverage.
- The core `-test-train` smoke test proves that a tiny deterministic run writes
  a final checkpoint plus `training.json`, reports correct sample/batch counts,
  includes a partial final batch, and changes at least one model parameter.
- Valid trainer smoke-test replay shards should be generated temporarily through
  the current replay writer path where practical. Checked-in malformed fixtures
  may be used for focused error-path coverage.
- Implementation should update `BUILD_AND_TEST.md`, `README.md`, and
  `TRAINING_LOOP_WORK_ITEMS.md` with the new trainer commands and follow-up
  issue boundaries.
- Docs should include a minimal end-to-end smoke sequence chaining
  `-model-init`, `-self-play`, and `-train`.
- Trainer internals should be reusable C++ APIs with a thin CLI wrapper, not a
  large training loop embedded in `main.cpp`.
- Initial implementation should add `Training`, `TrainingCommand`, and
  `TrainingTest` files. Replay-dataset and batch helpers can stay private to
  `Training.cpp` for v1.
- CLI parsing should validate numeric option ranges up front and fail with
  clear errors before replay loading or training starts.
- `--checkpoint-in` and `--checkpoint-out` must resolve to different paths.
  In-place checkpoint mutation is not supported.
- Periodic checkpoints are named as epoch subdirectories under
  `--checkpoint-dir`, for example `epoch-000001`, and planned periodic paths
  are checked for conflicts during startup.
- Final and periodic checkpoints are written through a temporary sibling
  directory and moved into place only after the bundle is complete. This
  hardening is tracked by #182 and is an optional stretch for #8.
- Failed atomic checkpoint writes use best-effort cleanup of the trainer-created
  temp directory; if cleanup fails, the failure summary prints the temp path.
- `--checkpoint-out` fails early if it already exists unless `--force` is
  passed. Output and periodic checkpoint path conflicts are checked during
  command startup before replay loading or optimization begins.
- Training-produced checkpoint bundles include `training.json` for run
  provenance and aggregate training metrics. `metadata.json` remains the model
  compatibility manifest.
- Final `training.json` includes compact per-epoch history with losses, sample
  and batch counts, elapsed seconds, and any periodic checkpoint path.
- V1 supports model-only continuation from checkpoints. It does not persist or
  restore AdamW optimizer state. Full optimizer-state resume is tracked by #181
  and should be completed before serious competitive training runs.
- V1 exposes `--seed`, defaulting to `0`, for training-data order only. Model
  initialization seed comes from `--checkpoint-in`; replay generation seeds come
  from the replay shards.
- V1 trains on the final partial batch instead of dropping it. Metrics report
  actual batches and samples consumed.
- `--device auto` falls back to CPU when CUDA is unavailable. `--device cuda`
  fails if CUDA is unavailable. The resolved device is printed and recorded in
  `training.json`.
