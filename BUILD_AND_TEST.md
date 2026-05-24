# Build And Test

This project can be built from the Visual Studio solution with MSBuild.

## Prerequisites

- Visual Studio 2022 with the v143 C++ toolset.
- Windows SDK 10.0.
- The local dependency paths referenced by `AdvanceWarsServer/AdvanceWarsServer.vcxproj`, including libtorch, nlohmann-json, asio, and via-httplib.
- Node.js 24 LTS and Yarn 1.x for the React board viewer.

## Build

From the repository root:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'AdvanceWarsServer.sln' /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

Release x64:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'AdvanceWarsServer.sln' /m /p:Configuration=Release /p:Platform=x64 /v:minimal
```

## Run Tests

Run tests from the `AdvanceWarsServer` project directory so the relative `test/json` path resolves:

```powershell
Set-Location .\AdvanceWarsServer
..\x64\Debug\AdvanceWarsServer.exe -test test/json
..\x64\Debug\AdvanceWarsServer.exe -test-api-contract
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
| `-sim-random-move-game [seed]` | Run an experimental random-action simulation. | Uses local output paths that still need cleanup before general use. |
| `-sim-mcts-game` | Run an experimental MCTS simulation. | Uses local output paths that still need cleanup before general use. |
| `-server` | Run the current HTTP server on port 80. | Serves the canonical REST routes documented in `docs/API.md`. |
| `-converter` | Regenerate the hardcoded Lefty map JSON. | Developer utility. |
| `-torchlib` | Run a local libtorch/MNIST experiment. | Depends on local `D:/MNIST` and libtorch paths. |

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
