# Standard Engine Completeness Matrix

Last reviewed: 2026-05-18

This document tracks the simulator against the first playable target: normal Advance Wars By Web Global League Standard games that can be driven through a REST API or AI training loop. Local source code and JSON fixtures are the source of truth for current implementation behavior. The gameplay target is checked against the local report at `C:\Users\Roshan\Downloads\advance-wars-by-web-report.md` and the [Advance Wars By Web Wiki](https://awbw.fandom.com/wiki/Advance_Wars_By_Web_Wiki), especially [Metagame](https://awbw.fandom.com/wiki/Metagame), [AWBW Guide](https://awbw.fandom.com/wiki/AWBW_Guide), [Units](https://awbw.fandom.com/wiki/Units), [Properties](https://awbw.fandom.com/wiki/Properties), [Damage Formula](https://awbw.fandom.com/wiki/Damage_Formula), [CO](https://awbw.fandom.com/wiki/CO), and [Changes in AWBW](https://awbw.fandom.com/wiki/Changes_in_AWBW).

When implementation and AWBW behavior differ, the gap should be tracked by a focused GitHub issue before code changes are made.

## Target Ruleset

Normal Global League Standard means:

- 2 players.
- Clear weather.
- Fog of War disabled.
- CO powers enabled.
- Tags disabled.
- 0 starting funds.
- 1000 funds per fund-producing property.
- Black Bomb production banned.
- Standard timer metadata: 7 day initial time, 1 day increment, 7 day max turn time.

Explicitly deferred modes and options:

| Deferred area | Issue |
| --- | --- |
| Fog of War Global League mode | #90 |
| High Funds Global League mode | #91 |
| Random weather | #92 |
| Teleport tiles | #93 |
| Tag powers and tag-team modes | #96 |
| CO Powers Off setting | #97 |
| Multiplayer, teams, and free-for-all | #98 |

## Status Key

| Status | Meaning |
| --- | --- |
| Complete | Implemented and covered by local source/tests well enough for Standard play. |
| Partial | Usable behavior exists, but a known AWBW or API gap remains. |
| Audit | Behavior exists, but needs a source-backed data/formula audit before being treated as complete. |
| Deferred | Not needed for normal Standard, tracked so it does not leak into the target silently. |

## Setup And Settings

| Area | Current implementation | Standard target | Status | Issue |
| --- | --- | --- | --- | --- |
| Player count | `GameState` is fixed to two players. | Standard is two-player. | Complete for Standard | #98 for non-Standard player counts |
| Country identity | Player army identity supports Orange Star and Blue Moon only. | Any AWBW country identity used by two-player Standard maps should round-trip independently from CO choice. | Partial | #70 |
| Game settings model | State has scattered fields, and REST create is hardcoded. | Serialize/create Standard settings: clear, no fog, powers on, tags off, 0 start funds, 1000 income, bans, limits, timers. | Partial | #65 |
| Starting funds and income | Normal income exists; Sasha income modifier exists; configurable mode presets are incomplete. | 0 starting funds and 1000/property for Standard. | Partial | #65 |
| High Funds | No preset. | 2000/property only in High Funds modes. | Deferred | #91 |
| Unit cap | `unit-cap` serializes and production fixtures cover under-cap and at-cap behavior. | Enforce configured unit limit. | Complete for current model | none |
| Capture limit | Capture-limit terminal path exists, but Labs/Comm Towers are counted incorrectly. | Limit excludes Labs and Comm Towers. | Partial | #73 |
| Day limit | No day-limit terminal rule. | Configurable day limit with property-count winner/tie handling. | Partial | #74 |
| Unit bans | No setup-level ban list. | Standard bans Black Bomb production. | Partial | #75 |
| Lab units | No setup-level lab-unit requirement. | Selected lab units require owned Lab access. | Partial | #76 |
| Timers | No timer model or timeout terminal reason. | Standard timer metadata and optional enforcement story. | Partial | #82 |

## REST API And Training Loop

| Area | Current implementation | Target | Status | Issue |
| --- | --- | --- | --- | --- |
| Game lifecycle API | `POST /games` ignores request body and creates a hardcoded Lefty game. | Create/get/reset/step/terminal contract for map, players, settings, and seed. | Partial | #66 |
| Submitted action validation | Legal actions are generated, but direct submitted actions can bypass some validation or mutate before failing. | Submitted actions are validated and rejected atomically. | Partial | #67 |
| Step semantics | REST action application can auto-end-turn after a submitted action. | One submitted action causes exactly one committed action. | Partial | #68 |
| Terminal reasons | HQ capture, lab win, rout/fuel-out behavior exist; heuristic auto-resign also exists. | Terminal reason should be explicit: HQ, lab, rout, capture limit, day limit, timeout, resign. | Partial | #69, #74, #77, #82 |
| State tensor | `Tensor.cpp` exists as planned home. | Deterministic current-player-relative tensor. | Partial | #3 |
| Action encoding and masks | `Action` variants and legal action generation exist. | Stable encoded action space plus mask. | Partial | #4 |
| MCTS sequence handling | Existing MCTS needs action-level self-play cleanup. | Same-player action sequences backed up correctly until `EndTurn`. | Partial | #5 |
| Replay writer | Not complete. | Versioned self-play samples and reconstructable action history. | Partial | #6 |
| Policy/value scaffold | Not complete. | Model input/output aligned with tensor/action encodings. | Partial | #7 |
| Training entry point | Not complete. | Train from replay data and write checkpoints. | Partial | #8 |
| Evaluation harness | Not complete. | Deterministic checkpoint evaluation against baselines. | Partial | #9 |

## Actions And Terminal Conditions

| Area | Current implementation | Standard target | Status | Issue |
| --- | --- | --- | --- | --- |
| Movement and wait | Covered broadly by JSON fixtures. | AWBW movement/fuel/path legality. | Partial because submitted invalid actions need atomic rejection | #67 |
| Combat actions | Direct, indirect, counterattack, ammo, HP, terrain, and many CO/power cases are covered. | AWBW combat formula and data. | Audit | #80, #81 |
| Capture | Capture progress, interruption, HQ capture, Labs, Com Towers, Airports, Ports, and Sami capture have fixtures. | AWBW capture points and capture-limit counting. | Partial | #73 |
| Buy | Production fixtures cover common buy legality and unit cap. | Add setup bans, lab units, ghosted blockers, Piperunner production verification. | Partial | #75, #76, #78, #79 |
| Load and unload | APC, T-Copter, Lander, Black Boat, Cruiser, Carrier, loaded-unit destruction, and many boundaries are covered. | AWBW free unload behavior and legal terrain/occupancy. | Partial because submitted invalid actions need atomic rejection | #67 |
| Combine/join | Joining/refund/capture-preservation fixtures exist. | AWBW join/refund behavior. | Complete for current Standard coverage | none |
| Black Boat repair action | Dedicated repair/resupply fixtures exist. | AWBW Black Boat repair and resupply. | Complete for current Standard coverage | none |
| CO power actions | Power gates, spending, many direct effects, and mass effects are covered. | Full CO behavior and meter math. | Partial | #81, #83, #84, #85, #86, #87, #88, #89, #99, #100, #101, #102, #103 |
| Black Bomb explode | Black Bomb movement/fuel/no-weapon fixtures exist; explosion action is absent. | Black Bomb explosion if predeployed/custom games need it; production banned in Standard. | Deferred from production, incomplete as unit behavior | #33, #75 |
| Missile silo launch | Terrain exists, but launch behavior is incomplete. | Launch action, empty silo state, damage area, legal action generation. | Partial | #42 |
| Resign/delete | Heuristic resign exists; explicit player actions do not. | Explicit resign and delete-unit actions if AWBW permits delete. | Partial | #77 |
| Heuristic auto-resign | Engine can defeat a player by army-value heuristic. | Standard terminal logic should not auto-resign except through explicit/tracked rules. | Partial | #69 |

## Units

The unit fixture manifest lives at `AdvanceWarsServer/test/json/units/README.md`.

| Area | Current implementation | Status | Issue |
| --- | --- | --- | --- |
| Full unit roster exists | All `UnitProperties::Type` values exist and many have dedicated fixtures. | Complete structurally | none |
| Unit stat and damage data | Combat chart and stats exist with broad fixtures. | Audit | #80 |
| Anti-Air owner-side coverage | Mostly covered as target/opponent. | Partial | #104 |
| Recon owner-side coverage | Mostly covered as target/opponent/production. | Partial | #105 |
| Medium Tank owner-side coverage | Mostly covered as target/opponent/production. | Partial | #106 |
| Mega Tank owner-side coverage | Mostly covered as target/opponent/production. | Partial | #107 |
| Neotank owner-side coverage | Mostly covered as target/opponent/production. | Partial | #108 |
| Piperunner pipe movement and attacks | Range/ammo fixtures exist; pipe-only movement still needs coverage. | Partial | #35 |
| Piperunner production | Current standard ground production list excludes Piperunner. | Partial | #79 |
| Black Bomb explosion | Movement/fuel/fuel-out/no-weapon fixtures exist; explosion absent. | Partial or deferred by Standard ban | #33, #75 |
| Stealth and Sub hidden behavior | Visible combat and hidden fuel drain pieces exist. | Partial; full hide/unhide and visibility deferred from Standard fog-off mode | #34, #90 |

## Terrain, Map Objects, Properties, And Economy

| Area | Current implementation | Standard target | Status | Issue |
| --- | --- | --- | --- | --- |
| Core terrain movement | JSON fixtures cover many terrain, weather, air, land, and sea movement cases. | AWBW movement tables. | Partial where Piperunner/Sturm/Teleport remain | #35, #85, #93 |
| Properties and income | Cities/Bases/Airports/Ports/HQ income behavior exists; Labs/Com Towers no-income fixtures exist. | AWBW fund-producing properties only. | Complete for normal income, configurable settings partial | #65 |
| Property repair/resupply | Begin-turn property service is gated by owner, property type, and unit class; Labs and Com Towers do not repair or resupply. | Land on City/Base/HQ, air on Airport, sea on Port; owner only. | Complete for normal property service | none |
| Rachel property repair | Normal property repair is not Rachel-aware. | Rachel repairs +1 extra displayed HP on compatible properties. | Partial | #72 |
| APC/Cruiser/Carrier resupply | Dedicated transport and loaded-resupply fixtures exist. | AWBW logistics order and compatible cargo. | Complete for current Standard coverage | none |
| Labs | Lab capture/no-income/lab-victory fixtures exist. | Labs can act as no-HQ loss condition and do not produce income. | Complete for two-player Standard model | #76 for lab-unit production setting |
| Com Towers | Capture/no-income/attack-bonus fixtures exist. | +10 attack per owned tower, no income. | Complete for standard tower attack bonus | #87 for Javier tower defense |
| Ghosted production tiles | No ghost tile/blocker model. | Ghosts disable production while preserving property behavior. | Partial | #78 |
| Teleport tiles | No terrain type. | AWBW-only movement tile, deferred unless map pool requires it. | Deferred | #93 |
| Missile silos | Terrain parses but launch action is absent. | Launch/use/empty state and area damage. | Partial | #42 |
| Country-coded map import | Terrain IDs include many countries, but player ownership maps to two armies only. | Support all two-player AWBW country identities. | Partial | #70 |

## Commanding Officers

Detailed implementation notes live in `CO_SUPPORT.md`.

| Area | Current implementation | Status | Issue |
| --- | --- | --- | --- |
| CO parsing and contracts | All COs parse/serialize with contract fixtures. | Complete | none |
| Power costs and meter state | Star costs and charge exist. | Audit | #81 |
| Checked-in attack/defense charts | Normal/COP/SCOP charts exist. | Audit with combat data | #80, #81 |
| Mass HP, weather, economy, production, range, movement, capture, and many power effects | See `CO_SUPPORT.md`. | Partial but broad support exists | focused gaps below |
| Rachel property repair | Missing. | Partial | #72 |
| Jess COP resupply | SCOP resupply exists; COP resupply missing. | Partial | #83 |
| Eagle air fuel upkeep | Missing. | Partial | #84 |
| Sturm all-terrain movement | Missing/incomplete. | Partial | #85 |
| Lash terrain-star effects | Missing/incomplete. | Partial | #86 |
| Javier indirect and Comm Tower defense | Missing/incomplete. | Partial | #87 |
| Kanbei Samurai Spirit counterattack | Missing/incomplete. | Partial | #88 |
| Sonja counterattack and hidden HP API view | Missing/incomplete. | Partial | #89 |
| Luck CO deterministic fixtures | Luck bounds exist; exact deterministic fixtures split per CO. | Partial | #99, #100, #101, #102, #103 |
| Fog/vision-only CO effects | Not needed for Standard fog-off target. | Deferred | #90 |

## Weather And Fog

| Area | Current implementation | Standard target | Status | Issue |
| --- | --- | --- | --- | --- |
| Clear weather | Supported as normal state. | Standard uses clear weather. | Complete | none |
| Rain/snow state and power-created weather | JSON state, movement/fuel costs, expiration, and Drake/Olaf powers have fixtures. | Required for CO powers even in Standard. | Complete for current covered effects | none |
| Random weather | Not implemented. | Deferred from Standard. | Deferred | #92 |
| Rain vision | No vision/fog subsystem. | Fog-only behavior. | Deferred | #90 |
| Fog of War | No full player-perspective visibility model. | Deferred from Standard. | Deferred | #90 |

## Documentation And Maintenance

| Area | Current implementation | Target | Status | Issue |
| --- | --- | --- | --- | --- |
| Completeness matrix | This document. | Keep linked to focused issues and update when gaps close. | In progress | #94 |
| CO notes | `CO_SUPPORT.md` summarizes implemented CO behavior and follow-ups. | Keep follow-up list aligned with focused issues. | In progress | #94 |
| Unit manifest | `AdvanceWarsServer/test/json/units/README.md` lists unit fixture status. | Keep gaps linked to targeted issues. | In progress | #94 |
| Build/test commands | `BUILD_AND_TEST.md` documents local commands. | Keep commands current. | Complete | none |

## Open Issue Index

Training loop:

- #3: Implement state tensor encoding.
- #4: Implement action encoding and legal action masks.
- #5: Refactor MCTS for action-level self-play and same-player turn sequences.
- #6: Add self-play replay writer.
- #7: Add policy/value network scaffold.
- #8: Add training command-line entry point.
- #9: Add checkpoint evaluation harness.

Standard API, settings, and terminal behavior:

- #65: Define and enforce normal Global League Standard game settings.
- #66: Add REST game lifecycle and step API contract for self-play.
- #67: Validate and atomically reject illegal submitted actions.
- #68: Make REST action stepping explicit and remove implicit auto-end-turn.
- #69: Remove heuristic auto-resign from Standard engine terminal logic.
- #70: Support AWBW map imports with all two-player country identities.
- #73: Fix capture-limit counting to exclude Labs and Comm Towers.
- #74: Implement day-limit victory and draw resolution.
- #75: Add game setup support for unit bans and the Standard Black Bomb ban.
- #76: Add lab-unit production requirements from game setup.
- #77: Add explicit resign and delete-unit actions.
- #82: Add timer settings and timeout terminal state metadata.

Actions, units, and data:

- #33: Implement and test black bomb explosion behavior.
- #34: Implement and test stealth and sub unit hiding behavior.
- #35: Add piperunner pipe movement and attack coverage.
- #42: Implement and test missile silo launch behavior.
- #78: Implement ghosted production tile blockers.
- #79: Allow Piperunner production from Bases and Hachi Merchant Union cities.
- #80: Verify unit stats and damage lookup data against AWBW charts.
- #104: Add Anti-Air owned combat coverage.
- #105: Add Recon owned movement and combat coverage.
- #106: Add Medium Tank owned combat coverage.
- #107: Add Mega Tank owned combat coverage.
- #108: Add Neotank owned combat coverage.

Properties, economy, and COs:

- #72: Implement Rachel property repair bonus.
- #81: Verify CO star costs and power-meter charge math against AWBW.
- #83: Implement Jess Turbo Charge COP resupply side effect.
- #84: Implement Eagle air-unit fuel upkeep modifier.
- #85: Implement Sturm all-terrain movement rules.
- #86: Implement Lash terrain-star attack and movement effects.
- #87: Implement Javier indirect-defense and comm-tower defense bonuses.
- #88: Implement Kanbei Samurai Spirit counterattack bonus.
- #89: Implement Sonja counterattack bonus and hidden-HP API redaction.
- #99: Implement and test Nell luck combat modifiers.
- #100: Implement and test Rachel luck combat modifiers.
- #101: Implement and test Flak luck combat modifiers.
- #102: Implement and test Jugger luck combat modifiers.
- #103: Implement and test Sonja luck combat modifiers.

Deferred modes and map features:

- #90: Placeholder: Fog of War Global League mode.
- #91: Placeholder: High Funds Global League mode.
- #92: Placeholder: Random weather game mode/settings.
- #93: Placeholder: Teleport tile mechanics.
- #96: Placeholder: Tag powers and tag-team modes.
- #97: Placeholder: CO Powers Off game setting.
- #98: Placeholder: Multiplayer, team, and free-for-all game modes.

Performance and maintenance:

- #51: Optimize all-unit operations with a unit cache.
- #94: Document Standard engine completeness matrix and issue index.
