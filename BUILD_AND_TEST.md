# Build And Test

This project can be built from the Visual Studio solution with MSBuild.

## Prerequisites

- Visual Studio 2022 with the v143 C++ toolset.
- Windows SDK 10.0.
- The local dependency paths referenced by `AdvanceWarsServer/AdvanceWarsServer.vcxproj`, including nlohmann-json, asio, and via-httplib.
- LibTorch for Windows. Pass `LIBTORCH_ROOT` to MSBuild, pointing at the directory that contains `include/`, `lib/`, and `bin/`.
- Node.js 24 LTS and Yarn 1.x for the React board viewer.

## Build

From the repository root:

```powershell
$libtorchRoot = 'C:\path\to\libtorch'
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'AdvanceWarsServer.sln' /m /p:Configuration=Debug /p:Platform=x64 /p:LIBTORCH_ROOT="$libtorchRoot" /v:minimal
```

Release x64:

```powershell
$libtorchRoot = 'C:\path\to\libtorch'
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'AdvanceWarsServer.sln' /m /p:Configuration=Release /p:Platform=x64 /p:LIBTORCH_ROOT="$libtorchRoot" /v:minimal
```

The default build links the CPU LibTorch libraries and keeps CUDA probing disabled so development does not require a configured CUDA toolchain. To build the CUDA-enabled path, pass `/p:UseLibtorchCuda=true` and make sure `CUDA_PATH`, `CUDNN_LIB_PATH`, and `NVTOOLSEXT_PATH` resolve on the machine.

When running model commands, add the LibTorch `bin` directory to `PATH` first:

```powershell
$libtorchRoot = 'C:\path\to\libtorch'
$env:PATH = "$libtorchRoot\bin;" + $env:PATH
```

## Run Tests

Run tests from the `AdvanceWarsServer` project directory so the relative `test/json` path resolves:

```powershell
Set-Location .\AdvanceWarsServer
..\x64\Debug\AdvanceWarsServer.exe -test test/json
..\x64\Debug\AdvanceWarsServer.exe -test-api-contract
..\x64\Debug\AdvanceWarsServer.exe -test-mcts
..\x64\Debug\AdvanceWarsServer.exe -test-model
..\x64\Debug\AdvanceWarsServer.exe -test-replay
```

Release:

```powershell
Set-Location .\AdvanceWarsServer
..\x64\Release\AdvanceWarsServer.exe -test test/json
```

You can also run a subset:

```powershell
..\x64\Debug\AdvanceWarsServer.exe -test test/json/transport
```

## Executable Options

The executable currently recognizes these development commands:

| Option | Purpose | Notes |
| --- | --- | --- |
| `-test [path]` | Run the recursive JSON fixture suite. | Defaults to `test/json`. Run from `AdvanceWarsServer/` so relative paths resolve. |
| `-test-api-contract` | Run focused REST lifecycle/action contract tests. | Run from `AdvanceWarsServer/` so map templates and fixtures resolve. |
| `-test-mcts` | Run focused MCTS contract tests. | Uses scripted fake states for deterministic search semantics. |
| `-test-model [--device cpu|auto|cuda]` | Run focused policy/value model tests. | Verifies forward shapes, real Standard tensor inference, deterministic init, checkpoint round trip, and strict metadata rejection. |
| `-test-replay` | Run focused self-play replay writer/validator tests. | Writes temporary replay shards and cleans them up on success. |
| `-model-init --out <checkpoint-dir> [--device cpu|auto|cuda] [--hidden-channels <n>] [--res-blocks <n>] [--norm-groups <n>] [--seed <n>] [--force]` | Initialize and validate a policy/value checkpoint bundle. | Writes `metadata.json` plus `model.pt`. `model.pt` is required; it cannot be reconstructed from metadata alone. |
| `-self-play --out <path> --map <id> --player0-co <id> --player1-co <id> [options]` | Generate validated sparse self-play replay JSONL. | Fails if `--out` exists unless `--append` is passed. See `docs/SELF_PLAY_REPLAYS.md`. |
| `-validate-replay <path>` | Validate a self-play replay JSONL shard. | Prints a concise summary on success and exact failure location on error. |
| `-sim-random-move-game [seed]` | Run an experimental random-action simulation. | Uses local output paths that still need cleanup before general use. |
| `-sim-mcts-game` | Run an experimental MCTS simulation. | Uses local output paths that still need cleanup before general use. |
| `-server` | Run the current HTTP server on port 80. | Serves the canonical REST routes documented in `docs/API.md`. |
| `-converter` | Regenerate the hardcoded Lefty map JSON. | Developer utility. |
| `-torchlib [--mnist-path <path>]` | Run a local libtorch smoke check, optionally followed by the old MNIST experiment. | MNIST stays disabled unless `--mnist-path` is passed. |

## Frontend Board Viewer

The React/Vite board viewer lives in `frontend/`.

```powershell
Set-Location .\frontend
yarn install
yarn dev
```

Open <http://127.0.0.1:5173/>. The app loads the checked-in Lefty sample by default, and the `Action sample` button loads a small fixture with legal-action highlights.

Verification:

```powershell
yarn test
yarn typecheck
yarn build
```

The frontend defaults to `http://localhost:80` when creating a server-backed game. Start the C++ server from the engine project directory in a second terminal:

```powershell
Set-Location .\AdvanceWarsServer
..\x64\Debug\AdvanceWarsServer.exe -server
```

## Additional Docs

- `README.md`: project overview and navigation.
- `STANDARD_ENGINE_COMPLETENESS.md`: current Standard rules/API matrix.
- `docs/API.md`: current and target API shape.
- `docs/JSON_FIXTURES.md`: fixture authoring guide.
- `docs/SELF_PLAY_REPLAYS.md`: self-play replay command and schema reference.
