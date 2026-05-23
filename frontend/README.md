# Advance Wars Frontend Contract

This package is the frontend-side contract layer for the C++ engine. It is intentionally UI-free for now.

## Commands

```powershell
yarn install
yarn typecheck
yarn test
```

## Shape

- `src/contract/` contains Zod schemas for raw server and fixture JSON.
- `src/domain/` contains UI-facing TypeScript types.
- `src/adapter/` validates raw payloads and normalizes them into domain objects.
- `samples/wire/current/` contains payloads compatible with the current server/fixture shape.
- `samples/wire/future/` documents expected future response envelopes for #66/#67.

The adapter keeps the server as the source of truth. It does not implement gameplay rules.

## Current Contract Notes

- Raw wire fields mirror server JSON, including names like `unit-cap`, `cap-limit`, and `display-health`.
- Domain fields are frontend-friendly, such as `settings.unitCap`, `settings.captureLimit`, and `displayHealth`.
- Current `GameState` parsing requires exactly two players.
- Unit and property ownership is normalized from army strings to player indices.
- Property ownership may be neutral; unit ownership must match a player.
- Maps must be rectangular and non-empty.
- The server emits both raw `health` and server-owned `display-health`. Domain units always require `displayHealth`; raw `health` is optional for future player-perspective payloads.
- Missing wire `weather` normalizes to domain `weather: "clear"`.

## Legacy Rendering Reference

Older canvas rendering and sprite metadata live outside this repo at `D:\Projects\AdvanceWarAI`. That code should inform #110/#115, but this package should stay focused on validated data contracts and adapter behavior.
