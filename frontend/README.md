# Advance Wars Board Viewer

React + TypeScript + Vite board/debug viewer for serialized `GameState` JSON from the C++ engine.

## Run

Prerequisites:

- Node.js 24 LTS.
- Yarn 1.x.

Install dependencies from this folder:

```powershell
Set-Location .\frontend
yarn install
```

Start the frontend:

```powershell
yarn dev
```

Open <http://127.0.0.1:5173/>. The Vite dev server is bound to `127.0.0.1` by default for local-only access.

The app opens on a board viewer, not a landing page. It can load the bundled Lefty board sample, a small legal-action sample, pasted JSON, or a state created by the current C++ server.

## Server Source

Build the C++ Debug executable from the repository root:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'AdvanceWarsServer.sln' /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

Run the current HTTP server from the engine project directory in a second terminal:

```powershell
Set-Location .\AdvanceWarsServer
..\x64\Debug\AdvanceWarsServer.exe -server
```

The current server uses the legacy routes documented in `docs/API.md`, including `POST /games` and `GET /actions/:gameid/:x/:y`, and listens on port 80. The frontend defaults to `http://localhost:80`, but the base URL is editable and saved in browser local storage.

## Verification

```powershell
yarn test
yarn typecheck
yarn build
```
