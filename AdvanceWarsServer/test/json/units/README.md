# Unit Coverage Manifest

`units/<unit-type>/` is the stable fixture home for each `UnitProperties::Type` value from `AdvanceWarsServer/inc/Unit.h`. Use the same string spelling as `UnitProperties::getTypename()` when adding new unit-owned fixtures.

Existing JSON fixtures stay in their current feature folders for now to avoid path churn. Move them into the unit home when a follow-up is already changing that behavior, or cross-link them here when keeping a feature folder is clearer.

The JSON suite discovers tests recursively under `test/json`; non-`.json` files in this manifest tree are documentation or placeholders.

Coverage key:

- `yes`: at least one JSON fixture exercises the unit as the behavior owner.
- `partial`: fixtures mention the unit or cover one narrow behavior, but the unit does not yet have full owner coverage.
- `none`: no meaningful fixture coverage for that status yet.

| UnitProperties::Type | JSON type | Fixture home | Happy path | Boundary | Existing coverage notes |
| --- | --- | --- | --- | --- | --- |
| AntiAir | `anti-air` | `units/anti-air/` | partial | partial | Present as infantry combat target/opponent and production option. Needs anti-air owned attack tests. |
| Apc | `apc` | `units/apc/` | yes | yes | `transport/apc/` covers load, unload, resupply, movement, and capacity boundaries. |
| Artillery | `artillery` | `units/artillery/` | yes | yes | `units/artillery/` covers indirect min/max range, ammo consumption, no ammo, invalid air target, and move-fire rejection. |
| BCopter | `bcopter` | `units/bcopter/` | partial | partial | Present as infantry combat target/opponent and cruiser cargo. Needs B-copter owned movement/combat tests. |
| Battleship | `battleship` | `units/battleship/` | yes | yes | `units/battleship/` covers indirect min/max range, ammo consumption, no ammo, invalid air target, and move-fire rejection. |
| BlackBoat | `blackboat` | `units/blackboat/` | yes | yes | `transport/blackboat/` covers load, unload, repair, resupply, funds, and sea unload boundaries. |
| BlackBomb | `blackbomb` | `units/blackbomb/` | partial | none | Present in production fixtures. Explosion behavior is still uncovered. |
| Bomber | `bomber` | `units/bomber/` | partial | partial | Present as production option and infantry combat target/opponent. Needs bomber owned air attack tests. |
| Carrier | `carrier` | `units/carrier/` | yes | yes | `transport/carrier/` covers loading/resupply; `units/carrier/` covers indirect anti-air range, ammo, invalid ground target, and move-fire rejection. |
| Crusier | `crusier` | `units/crusier/` | yes | yes | Enum and JSON spelling are `crusier`; `transport/crusier/` covers copter loading and loaded resupply. |
| Fighter | `fighter` | `units/fighter/` | partial | partial | Present in production and as carrier cargo/invalid transport cargo. Needs fighter owned air combat tests. |
| Infantry | `infantry` | `units/infantry/` | yes | yes | Broad coverage exists in `combat/infantry/`, `captures/`, `joining/`, and transport cargo tests. |
| Lander | `lander` | `units/lander/` | yes | yes | `transport/lander/` covers multi-load, unload, and no-unload-at-sea boundary. |
| MediumTank | `medium-tank` | `units/medium-tank/` | partial | partial | Present as production option and infantry combat target/opponent. Needs medium tank owned combat tests. |
| Mech | `mech` | `units/mech/` | yes | yes | Covered through capture, combat, joining, and transport cargo scenarios. Needs dedicated mech-owned folder tests later. |
| MegaTank | `megatank` | `units/megatank/` | partial | partial | Present as production option and infantry combat target/opponent. Needs mega tank owned combat tests. |
| Missile | `missile` | `units/missile/` | yes | yes | `units/missile/` covers indirect anti-air min/max range, ammo consumption, no ammo, invalid ground target, and move-fire rejection. |
| Neotank | `neotank` | `units/neotank/` | partial | partial | Present as production option and infantry combat target/opponent. Needs neotank owned combat tests. |
| Piperunner | `piperunner` | `units/piperunner/` | partial | partial | `units/piperunner/` covers indirect min/max range, ammo consumption, no ammo, empty target, and move-fire rejection. Pipe movement still needs dedicated fixtures. |
| Recon | `recon` | `units/recon/` | partial | partial | Present as production option and infantry combat target/opponent. Needs recon owned movement/vision tests. |
| Rocket | `rocket` | `units/rocket/` | yes | yes | `units/rocket/` covers indirect min/max range, ammo consumption, no ammo, invalid air target, and move-fire rejection. |
| Stealth | `stealth` | `units/stealth/` | partial | partial | Present in production and carrier hidden-cargo fixtures. Hide/unhide and fuel drain behavior are uncovered. |
| Sub | `sub` | `units/sub/` | partial | none | Present in production fixtures. Dive/hide/fuel/combat behavior is uncovered. |
| TCopter | `tcopter` | `units/tcopter/` | yes | yes | `transport/tcopter/` covers load, unload, movement unload, and capacity boundaries. |
| Tank | `tank` | `units/tank/` | yes | yes | `combat/tank/` and broader combat/production fixtures cover basic tank behavior and no-ammo combat. |
