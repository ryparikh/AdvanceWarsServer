# Commanding Officer Support

The gameplay reference for CO mechanics is the [Advance Wars By Web Wiki CO page](https://awbw.fandom.com/wiki/CO), cross-checked against the official AWBW CO chart when auditing meter data. The simulator currently supports a simplified CO model: JSON identity, power-meter costs, power-status transitions, and the checked-in normal/COP/SCOP damage charts are treated as source-of-truth behavior. The broader Standard rules matrix is maintained in `STANDARD_ENGINE_COMPLETENESS.md`.

## Supported

- JSON parsing and serialization for every `CommandingOfficier::Type` value.
- Loud failure for unknown CO strings during JSON parsing.
- Power-meter star costs for every CO, including Von Bolt's SCOP-only meter.
- COP and SCOP action charge gates, power-status transitions, star-cost increase, and meter spending.
- All checked-in normal/COP/SCOP attack and defense chart values.
- Andy COP/SCOP healing.
- Jess COP/SCOP resupply.
- Active weather JSON parsing/serialization, weather movement/fuel costs, and Drake/Olaf weather-changing powers.
- Drake, Hawke, Kindle, Olaf, Rachel, Sturm, and Von Bolt COP/SCOP HP effects, including Drake fuel drain and Von Bolt next-turn stun.
- Implemented economy and unit-cost effects: Colin, Hachi, and Kanbei build-cost modifiers; Colin Gold Rush and Power of Money; Sasha income, Market Crash, and War Bonds; and Kindle property-based attack bonuses including High Society's owned-property scaling.
- Implemented Rachel day-to-day property repair: compatible owned properties repair up to 3 displayed HP instead of the normal 2, with funds charged for displayed HP actually restored.
- Implemented production effects: Hachi's Merchant Union allows standard ground-unit deployment, including Piperunners, from owned empty cities; Sensei's Copter Command/Airborne Assault spawn 9 HP unwaited Infantry/Mechs on owned empty cities in top-row, left-to-right order until the unit cap is reached.
- Implemented movement modifiers: Adder, Andy SCOP, Drake sea units, Jake SCOP, Jess vehicles, Koal, Max direct units, Sami transports/footsoldiers, Sensei transports, and Sturm legal-terrain movement costs outside snow.
- Implemented fuel-upkeep modifiers: Eagle air units consume 2 less fuel per day.
- Implemented capture modifiers: Sami footsoldiers capture at 150% displayed HP rounded down during day-to-day and Double Time, and capture instantly during Victory March.
- Implemented action-state effects: Eagle Lightning Strike refreshes map-present non-footsoldier units for an extra action.
- Implemented terrain/range/luck helpers: Jake plains attack, Koal road attack, Grit day-to-day/COP/SCOP indirect range, Jake COP/SCOP indirect range for vehicles, Max indirect range penalty, Nell/Rachel/Flak/Jugger/Sonja luck bounds, and Sonja SCOP counter-break combat ordering.

## Luck Notes

- JSON combat fixtures can force deterministic luck with `"luck-policy"`: `0` uses normal RNG or `"combat-rng-seed"`, `1` forces the lowest total luck outcome, `2` forces the highest total luck outcome, and `3` forces the middle value of each luck range.
- For bad-luck COs, the lowest total outcome uses minimum good luck and maximum bad luck; the highest total outcome uses maximum good luck and minimum bad luck.
- Sonja's AWBW combat luck is covered separately from her information and counterattack effects: she keeps +0..+9 good luck and 0..9 bad luck in day-to-day, Enhanced Vision, and Counter Break, while COP/SCOP still use the universal AWBW +10 attack/+10 defense chart bonus.
- Flak uses independent AWBW good-luck and bad-luck rolls. Day-to-day uses 0..24 good luck and 0..9 bad luck, Brute Force uses 0..49 and 0..19, and Barbaric Blow uses 0..89 and 0..39; the resulting total ranges are -9..+24, -19..+49, and -39..+89.
- Jugger uses independent AWBW good-luck and bad-luck rolls. Day-to-day uses 0..29 good luck and 0..14 bad luck, Overclock uses 0..54 and 0..24, and System Crash uses 0..94 and 0..44; the resulting total ranges are -14..+29, -24..+54, and -44..+94.
- Sonja's hidden HP, fog vision, day-to-day counterattack multiplier, and Counter Break first-strike edge cases remain tracked by [#89](https://github.com/ryparikh/AdvanceWarsServer/issues/89) and [#90](https://github.com/ryparikh/AdvanceWarsServer/issues/90), rather than by the combat-luck fixtures.

## Power Meter Notes

- CO star costs are covered by contract fixtures for every CO. The audited values match the AWBW wiki/official chart, including Sturm at 6+4 stars and Von Bolt as SCOP-only at 0+10 stars.
- Power meters start at a star value of 9000. COP threshold is `cop-stars * star-value`; SCOP/full threshold is `(cop-stars + scop-stars) * star-value`. After either power is used, the star value is multiplied by 1.2, truncated to an integer, and capped at 55720.
- Combat charge uses displayed HP lost and the simulator's scaled unit cost, where `Unit::GetUnitCost` is one tenth of the normal build price. Damage dealt grants half of the lost displayed-HP value to the attacker, integer-truncated if needed; damage received grants the full lost displayed-HP value to the defender.
- Zero displayed-HP loss adds no charge. Destruction charge is based on the displayed HP actually removed before the unit reaches zero. Combat does not add charge for a side whose CO power status is active, and direct HP effects from powers do not add meter charge.
- Serialized player JSON exposes `"charge"`, `"star-value"`, `"cop-stars"`, `"scop-stars"`, `"cop-threshold"`, `"scop-threshold"`, `"can-use-cop"`, and `"can-use-scop"` so clients can display meter progress and identify legal power actions without duplicating the threshold formula.

## Weather Notes

- Weather JSON uses `"weather": "rain"` or `"weather": "snow"`. CO-created weather also writes `"weather-turns-remaining"` and expires on a later `BeginTurn`.
- Rain and snow movement costs follow the AWBW weather tables, including Drake rain immunity, Olaf snow immunity, and Olaf treating rain like snow.
- Sturm units pay 1 fuel/move point on terrain their movement type can legally enter in clear and rain weather, including during active COP/SCOP turns. Snow disables this passive, so normal snow movement costs apply. Illegal terrain remains illegal, including non-Piperunner units entering pipes.
- Rain vision penalties are intentionally deferred because this simulator does not yet have fog-of-war or vision state; that broader subsystem is tracked by [#90](https://github.com/ryparikh/AdvanceWarsServer/issues/90).

## HP Effect Notes

- Out-of-combat HP damage and HP drain are applied in true-health units, where 1 displayed HP equals 10 stored health. These effects floor targets at 1 stored health, so they cannot destroy units directly; healing caps at 100.
- Drake's Tsunami and Typhoon halve enemy fuel with integer truncation, then apply their HP damage. Typhoon also keeps the existing rain side effect.
- Rachel, Sturm, and Von Bolt missile-style powers use a 2-range diamond area. Target scoring follows the AWBW wiki criteria using the simulator's two-player friendly/enemy model, unit HP, unit cost, and property capture state. Loaded units are excluded because they are not represented as map occupants.
- Rachel's three Covering Fire target centers are selected before damage is applied, then each missile applies damage. Von Bolt's stun is represented internally until the affected player's next `BeginTurn`, where stunned units remain `"moved": true` for that turn.

## Range Notes

- Indirect range modifiers adjust maximum range only; minimum range is unchanged. Grit applies to all indirect units, Jake applies to vehicle indirects during COP/SCOP, and Max applies his -1 maximum-range penalty to indirect units without changing adjacent direct attacks.

## Economy Notes

- Colin, Hachi, and Kanbei cost modifiers apply to unit construction. Hachi's Merchant Union uses the simulator's standard ground-production list for city deployment, matching the AWBW wiki's Piperunner note that Piperunners can be built at Bases or spawned from Cities during Hachi's SCOP.
- Sasha's Market Crash applies to the single opposing player in this simulator's two-player model.
- Rachel's extra property repair composes with the normal owner and unit-class gates: land units repair only on owned Cities/Bases/HQs, air units on owned Airports, and sea units on owned Ports. If Rachel cannot afford the full 3 displayed HP restore, she restores the affordable displayed HP and pays only for that repair.

## Action-State Notes

- Eagle's Lightning Drive COP only activates the COP stat chart; it does not clear moved flags.
- Eagle's Lightning Strike SCOP clears `"moved"` for the current player's map-present non-footsoldier units, including vehicles, air units, sea units, transports, Black Boats, and newly built non-footsoldier units.
- Infantry and Mechs are footsoldiers, so their capture progress and spent action state are preserved through Lightning Strike.
- Loaded units are not refreshed while they are cargo because they are not map occupants. If a transport unloads after Lightning Strike, the unloaded unit still becomes `"moved": true` under the simulator's existing unload rule.
- Lightning Strike is not a `BeginTurn`: it does not grant income, property repairs, property resupply, APC resupply, fuel-day processing, or capture-point changes. Black Boat repair is a normal action, so a Black Boat that repaired before Lightning Strike can repair again after being refreshed.

## Fuel Upkeep Notes

- Eagle air units reduce daily upkeep by 2 fuel, to a minimum of 0. This applies to B-Copters, Black Bombs, Bombers, Fighters, Stealths, and T-Copters, including hidden Stealth upkeep.
- Non-air units keep their normal daily fuel upkeep under Eagle, including sea units with base upkeep and ground units with no daily upkeep.
- Begin-turn temporary power and weather expiration still runs before daily fuel upkeep; Eagle's reduced air upkeep remains a day-to-day CO effect after those statuses expire.

## Capture Notes

- Capture progress uses the simulator's displayed-HP calculation, where partial true-health values round up to the current displayed HP before applying capture modifiers.
- Sami's Double Time keeps the same 150% rounded-down capture bonus as her day-to-day ability. Victory March completes captures immediately for Infantry and Mechs, including damaged footsoldiers.
- Interrupted captures still reset to 20 capture points when the capturing unit leaves the property, and blocked captures remain unavailable while an enemy unit occupies the property.

## Tracked Follow-Up Issues

These AWBW mechanics are not implemented yet. They are tracked as GitHub issues so the markdown is only a summary, not the source of truth.

- [#86](https://github.com/ryparikh/AdvanceWarsServer/issues/86): Lash terrain-star attack and movement effects.
- [#87](https://github.com/ryparikh/AdvanceWarsServer/issues/87): Javier indirect-defense and Comm Tower defense bonuses.
- [#88](https://github.com/ryparikh/AdvanceWarsServer/issues/88): Kanbei Samurai Spirit counterattack bonus.
- [#89](https://github.com/ryparikh/AdvanceWarsServer/issues/89): Sonja counterattack bonus and hidden-HP API redaction.
- [#90](https://github.com/ryparikh/AdvanceWarsServer/issues/90): fog, vision, hiding visibility, and fog-only CO effects.
- [#99](https://github.com/ryparikh/AdvanceWarsServer/issues/99) and [#100](https://github.com/ryparikh/AdvanceWarsServer/issues/100): deterministic luck CO combat fixtures for Nell and Rachel.
