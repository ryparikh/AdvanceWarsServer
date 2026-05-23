# API Notes

Last reviewed: 2026-05-23

This document describes the current HTTP surface and the target API shape for the Standard engine/frontend work. The C++ engine must remain authoritative for legal actions and state transitions; API clients should not duplicate rule logic.

## Current Implementation

The HTTP route registration lives in `AdvanceWarsServer/src/AdvanceWarsServer.cpp`.

| Method | Route | Current behavior |
| --- | --- | --- |
| `POST` | `/games` | Parses the request body, ignores its contents, creates one hardcoded Lefty game with Andy vs. Adder, and returns the serialized game state. |
| `GET` | `/actions/:gameid/:x/:y` | Returns valid actions for a single board coordinate. |
| `GET` | `/actions/:gameid` | Returns all valid actions for the active player. |
| `POST` | `/actions/:gameid` | Parses an `Action`, applies it to cached state, may auto-end-turn if only `EndTurn` remains, and returns the serialized game state. |

Important caveats:

- The current `main.cpp` usage text advertises `-server`, but the command dispatcher does not currently call `AdvanceWarsServer::run()` for that option.
- Game creation ignores map, settings, players, countries, COs, and seed.
- Unknown game ids and invalid action responses are not a stable contract yet.
- `POST /actions/:gameid` can apply implicit extra state change through auto-end-turn behavior.
- Action execution and legal-action generation do not yet have a fully atomic shared validation contract.

The target cleanup is tracked by:

- #66: REST game lifecycle and step API contract.
- #67: Validate and atomically reject illegal submitted actions.
- #68: Make REST action stepping explicit and remove implicit auto-end-turn.
- #69: Remove heuristic auto-resign from Standard terminal logic.
- #65: Define and enforce normal Global League Standard game settings.

## Target API Shape

The eventual API should support both humans and AI clients with the same loop:

1. Create a game from map, settings, players, COs, countries, and optional deterministic seed.
2. Fetch the authoritative game state.
3. Fetch legal actions for the active player or selected tile.
4. Submit exactly one action.
5. Receive either an atomic failure or the next authoritative state.
6. Observe terminal status and winner/reason when the game ends.

Suggested resources:

| Method | Route | Purpose |
| --- | --- | --- |
| `POST` | `/games` | Create a game from a validated setup payload. |
| `GET` | `/games/:gameid` | Fetch current authoritative game state. |
| `GET` | `/games/:gameid/actions` | Fetch all legal actions for the active player. |
| `GET` | `/games/:gameid/actions?x=4&y=7` | Fetch legal actions for one selected coordinate. |
| `POST` | `/games/:gameid/actions` | Submit exactly one action. |
| `GET` | `/games/:gameid/trace` | Optional replay/debug trace once action history exists. |

The exact route names can change, but the contract should be explicit before the React client and training loop depend on it.

## Response Principles

Game state responses should include:

- `gameId`
- active player and day/turn information
- map tiles, terrain, property ownership, capture points, and units
- player funds, CO, power meter/status, and army/country identity
- weather and settings
- terminal status, winner, and terminal reason when applicable

Action responses should include:

- submitted action
- success/failure
- validation errors for rejected actions
- resulting game state on success
- no hidden implicit action commits

Legal-action responses should include:

- the current active player context
- the legal actions as server-serialized `Action` objects
- optional selection/source metadata for tile-specific requests

## Frontend Contract Layer

The `frontend/` package is the current #114 compatibility layer between server JSON and future React UI code. It validates raw server/fixture payloads with Zod, accepts additive unknown fields while checking known fields, and normalizes data into UI-facing domain objects.

Current package responsibilities:

- Parse current game-state and legal-action payloads from the C++ server and JSON fixtures.
- Expose `createApiClient()` with configurable `baseUrl` and injectable `fetch`.
- Return `ApiResult<T>` values instead of throwing for network, HTTP, parse, and validation failures.
- Serialize frontend domain actions back to the current server `Action` shape before submitting them.
- Keep current wire samples in `frontend/samples/wire/current/` and future response-envelope sketches in `frontend/samples/wire/future/`.

The adapter currently calls the existing routes where they exist (`POST /games`, `GET /actions/:gameid`, `GET /actions/:gameid/:x/:y`, and `POST /actions/:gameid`) and includes `GET /games/:gameid` as the target state-fetch route for #66.

## Frontend And Bot Consumer Rules

Clients should:

- Treat server state as authoritative.
- Render legal actions supplied by the server.
- Submit one action at a time.
- Resync from the server after each accepted action or rejected action.
- Avoid inferring legality from visual state except for presentation hints.

Related frontend trackers:

- #110: React frontend workspace and board renderer.
- #111: Bot replay and self-play game visualizer.
- #112: Human-play web client using server legal actions.
- #113: JSON fixture and scenario visualization workflow.
- #114: Frontend API adapter and shared game-state types.
- #115: Frontend asset pipeline.
