# AWBW Unit Data Audit

Last reviewed: 2026-05-25

This audit covers normal Standard unit data from `AdvanceWarsServer/src/UnitInfo.cpp`.
Local source code and JSON fixtures remain the implementation source of truth; the gameplay
target was checked against the local report at `C:\Users\Roshan\Downloads\advance-wars-by-web-report.md`,
the [AWBW Units wiki page](https://awbw.fandom.com/wiki/Units), the
[AWBW Damage Formula wiki page](https://awbw.fandom.com/wiki/Damage_Formula), and the current
[official AWBW damage chart](https://awbw.amarriner.com/damage.php).

## Method

- Compared all 25 `vrgUnits` rows against the AWBW Units table for movement type, cost, move,
  ammo, fuel, fuel/day, vision, and range.
- Compared the engine's effective full-ammo attacker/defender damage result against the current
  official AWBW combined damage chart. The engine stores primary and secondary tables separately,
  so this check includes the current weapon-selection logic instead of only comparing raw table cells.
- Opened focused follow-up issues for mismatches instead of bundling unrelated corrections into
  this audit PR. No JSON fixtures are added here because no data corrections are made here; each
  follow-up issue requires focused regression fixtures with the fix.

## Summary

| Area | Result | Follow-up |
| --- | --- | --- |
| Unit stat rows | All 25 units match the AWBW Units table. | none |
| Effective base-damage chart | 621 of 625 attacker/defender cells match the official AWBW damage chart. | #147, #148 |
| Unit behavior gaps outside stat/damage data | Existing behavior gaps remain tracked separately. | #33, #34, #35 |
| Piperunner production | Verified and implemented by the completed production work. | #79 closed |

## Unit Rows

| Unit | Stats source/status | Damage source/status | Follow-up |
| --- | --- | --- | --- |
| Anti-Air | AWBW Units table: verified | Official AWBW damage chart: verified | none |
| APC | AWBW Units table: verified | Official AWBW damage chart: verified no-weapon row | none |
| Artillery | AWBW Units table: verified | Mismatch found: Artillery -> Recon and Artillery -> Rocket are 40 locally, 80 in AWBW | #147 |
| B-Copter | AWBW Units table: verified | Official AWBW damage chart: verified through secondary machine-gun targeting | none |
| Battleship | AWBW Units table: verified | Official AWBW damage chart: verified | none |
| Black Boat | AWBW Units table: verified | Official AWBW damage chart: verified no-weapon row | none |
| Black Bomb | AWBW Units table: verified | Official AWBW damage chart: verified no-weapon row | #33 for explosion behavior |
| Bomber | AWBW Units table: verified | Official AWBW damage chart: verified | none |
| Carrier | AWBW Units table: verified | Official AWBW damage chart: verified | none |
| Cruiser | AWBW Units table: verified | Mismatch found: Cruiser anti-air gun secondary damage is present locally but not selected against B-Copter or T-Copter | #148 |
| Fighter | AWBW Units table: verified | Official AWBW damage chart: verified | none |
| Infantry | AWBW Units table: verified | Official AWBW damage chart: verified | none |
| Lander | AWBW Units table: verified | Official AWBW damage chart: verified no-weapon row | none |
| Md. Tank | AWBW Units table: verified | Official AWBW damage chart: verified through secondary machine-gun targeting | none |
| Mech | AWBW Units table: verified | Official AWBW damage chart: verified through secondary machine-gun targeting | none |
| Mega Tank | AWBW Units table: verified | Official AWBW damage chart: verified through secondary machine-gun targeting | #107 for owner-side fixture breadth |
| Missile | AWBW Units table: verified | Official AWBW damage chart: verified | none |
| Neotank | AWBW Units table: verified | Official AWBW damage chart: verified through secondary machine-gun targeting | none |
| Piperunner | AWBW Units table: verified | Official AWBW damage chart: verified | #35 for pipe movement/attack coverage |
| Recon | AWBW Units table: verified | Official AWBW damage chart: verified | #105 for owner-side fixture breadth |
| Rocket | AWBW Units table: verified | Official AWBW damage chart: verified | none |
| Stealth | AWBW Units table: verified | Official AWBW damage chart: verified | #34 for hide/unhide behavior |
| Sub | AWBW Units table: verified | Official AWBW damage chart: verified | #34 for dive/surface behavior |
| T-Copter | AWBW Units table: verified | Official AWBW damage chart: verified no-weapon row | none |
| Tank | AWBW Units table: verified | Official AWBW damage chart: verified through secondary machine-gun targeting | none |

## Mismatch Details

| Finding | Current implementation | AWBW reference | Tracking issue |
| --- | --- | --- | --- |
| Artillery damage to Recon and Rocket | `vrgPrimaryWeaponDamage` stores 40 for both defenders. | Official AWBW chart lists 80 for both defenders. | #147 |
| Cruiser damage to B-Copter and T-Copter | `vrgSecondaryWeaponDamage` stores 115, but the combat selection branch does not use Cruiser's non-machine-gun secondary weapon for copter targets. | Official AWBW chart lists 115 for both defenders. | #148 |

## Existing Non-Data Unit Gaps

| Issue | Status | Scope |
| --- | --- | --- |
| #33 | Open | Black Bomb explosion action and fixtures. |
| #34 | Open | Stealth/Sub hide, unhide, dive, surface, and related visibility/combat behavior. |
| #35 | Open | Piperunner pipe-only movement and remaining attack coverage. |
| #79 | Closed | Piperunner production from Bases and Hachi Merchant Union cities. |
