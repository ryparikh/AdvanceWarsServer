# AdvanceWarsServer

AdvanceWarsServer is a C++ Advance Wars By Web style simulation engine. The current project goal is to make normal Global League Standard games playable through a reliable engine/API boundary, then use that same boundary for AI self-play, replay verification, and eventually a human-facing React client.

The engine is intentionally treated as the source of truth for rules and legal actions. Frontends, bots, and training loops should render state, request legal actions, and submit actions instead of reimplementing gameplay rules.

## Current Target

The first complete rules target is normal AWBW Global League Standard:

- 2 players.
- Clear weather.
- Fog of War off.
- CO powers on.
- Tags off.
- 0 starting funds.
- 1000 funds per fund-producing property.
- Black Bomb production banned.

Other modes such as Fog, High Funds, Tags, Random Weather, Teleport Tiles, and multiplayer/team games are intentionally deferred until the Standard engine path is reliable.

See [STANDARD_ENGINE_COMPLETENESS.md](STANDARD_ENGINE_COMPLETENESS.md) for the current implementation matrix and GitHub issue index.

## Repository Map

| Path | Purpose |
| --- | --- |
| `AdvanceWarsServer/` | C++ engine, HTTP server wrapper, tests, resources, and Visual Studio project. |
| `AdvanceWarsServer/inc/` | Public engine headers for game state, actions, units, map tiles, COs, and MCTS. |
| `AdvanceWarsServer/src/` | Engine implementation, JSON fixture runner, map parser, HTTP routes, and platform code. |
| `AdvanceWarsServer/test/json/` | Recursive JSON regression suite for engine behavior. |
| `AdvanceWarsServer/res/AWBW/` | AWBW map/game source data used for development and conversion experiments. |
| `BUILD_AND_TEST.md` | Build, executable, and test commands. |
| `STANDARD_ENGINE_COMPLETENESS.md` | Standard-mode rules/API completeness matrix and issue index. |
| `CO_SUPPORT.md` | Implemented CO behavior and focused CO follow-up issues. |
| `TRAINING_LOOP_WORK_ITEMS.md` | Self-play, MCTS, tensor, model, and training-loop roadmap. |
| `docs/API.md` | Current and target REST/API contract notes. |
| `docs/JSON_FIXTURES.md` | JSON regression fixture format and authoring guide. |

## Quick Start

Build the Debug x64 configuration from the repository root:

```powershell
& 'C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe' 'AdvanceWarsServer.sln' /m /p:Configuration=Debug /p:Platform=x64 /v:minimal
```

Run the full JSON suite from the project directory:

```powershell
Set-Location .\AdvanceWarsServer
..\x64\Debug\AdvanceWarsServer.exe -test test/json
```

Run a focused subset:

```powershell
..\x64\Debug\AdvanceWarsServer.exe -test test/json/transport
```

More command details are in [BUILD_AND_TEST.md](BUILD_AND_TEST.md).

## Development Workflow

1. Check the relevant docs and issues before changing rules behavior.
2. Use the AWBW wiki as the gameplay reference, but prefer local source and JSON tests as the source of truth for current behavior.
3. When local behavior differs from AWBW, document the gap in a focused issue before changing code.
4. Add or update JSON fixtures for rule changes.
5. Run the narrowest relevant JSON subset, then the full JSON suite.

The project notes in [AGENTS.md](AGENTS.md) are short but important: use the [Advance Wars By Web Wiki](https://awbw.fandom.com/wiki/Advance_Wars_By_Web_Wiki) for rules research, and call out any source-vs-wiki gap before changing implementation.

## Testing Model

The JSON suite is the main executable documentation for engine behavior. A fixture can:

- Apply actions from an `initial-game-state` and compare with `final-game-state`.
- Assert that actions fail.
- Assert legal actions for a selected tile.
- Check CO parser/chart contracts.
- Check deterministic replay traces.

See [docs/JSON_FIXTURES.md](docs/JSON_FIXTURES.md) and [AdvanceWarsServer/test/json/units/README.md](AdvanceWarsServer/test/json/units/README.md).

## API And Frontend Direction

The desired direction is:

- C++ engine owns rules, legal actions, and state transitions.
- REST API exposes game creation, state fetch, legal actions, explicit action stepping, errors, and terminal metadata.
- React frontend renders board state and asks the server what actions are legal.
- Bot replay and fixture visualizers reuse the same board renderer.

The current HTTP wrapper is still early and hardcoded in places. See [docs/API.md](docs/API.md) and the related issues #66, #67, #68, #110, #111, #112, #113, #114, and #115.

## Rule References

Primary local/reference docs:

- Local report: `C:\Users\Roshan\Downloads\advance-wars-by-web-report.md`
- AWBW wiki hub: https://awbw.fandom.com/wiki/Advance_Wars_By_Web_Wiki
- AWBW Metagame: https://awbw.fandom.com/wiki/Metagame
- AWBW Guide: https://awbw.fandom.com/wiki/AWBW_Guide
- AWBW Units: https://awbw.fandom.com/wiki/Units
- AWBW Properties: https://awbw.fandom.com/wiki/Properties
- AWBW Damage Formula: https://awbw.fandom.com/wiki/Damage_Formula
- AWBW COs: https://awbw.fandom.com/wiki/CO

## Known Caveats

- The Standard completeness matrix is current as of its review date, not a substitute for issue triage.
- The current REST wrapper does not yet satisfy the target API contract.
- Several engine behaviors are intentionally documented as gaps rather than papered over in the docs.
- Some local paths in experimental simulation/model code are still machine-specific and should be cleaned up before relying on those workflows broadly.
