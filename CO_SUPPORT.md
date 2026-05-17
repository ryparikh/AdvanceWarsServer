# Commanding Officer Support

The gameplay reference for CO mechanics is the [Advance Wars By Web Wiki CO page](https://awbw.fandom.com/wiki/CO). The simulator currently supports a simplified CO model: JSON identity, power-meter costs, power-status transitions, and the checked-in normal/COP/SCOP damage charts are treated as source-of-truth behavior.

## Supported

- JSON parsing and serialization for every `CommandingOfficier::Type` value.
- Loud failure for unknown CO strings during JSON parsing.
- Power-meter star costs for every CO, including Von Bolt's SCOP-only meter.
- COP and SCOP action charge gates, power-status transitions, star-cost increase, and meter spending.
- All checked-in normal/COP/SCOP attack and defense chart values.
- Andy COP/SCOP healing.
- Jess SCOP resupply.
- Active weather JSON parsing/serialization, weather movement/fuel costs, and Drake/Olaf weather-changing powers.
- Drake, Hawke, Kindle, Olaf, Rachel, Sturm, and Von Bolt COP/SCOP HP effects, including Drake fuel drain and Von Bolt next-turn stun.
- Implemented economy and unit-cost effects: Colin, Hachi, and Kanbei build-cost modifiers; Colin Gold Rush and Power of Money; Sasha income, Market Crash, and War Bonds; and Kindle property-based attack bonuses including High Society's owned-property scaling.
- Implemented movement modifiers: Adder, Andy SCOP, Drake sea units, Jake SCOP, Jess vehicles, Koal, Max direct units, Sami transports/footsoldiers, and Sensei transports.
- Implemented capture modifiers: Sami footsoldiers capture at 150% displayed HP rounded down during day-to-day and Double Time, and capture instantly during Victory March.
- Implemented action-state effects: Eagle Lightning Strike refreshes map-present non-footsoldier units for an extra action.
- Implemented terrain/range/luck helpers: Jake plains attack, Koal road attack, Grit day-to-day/COP/SCOP indirect range, Jake COP/SCOP indirect range for vehicles, Max indirect range penalty, Nell/Rachel/Flak/Jugger/Sonja luck bounds, and Sonja SCOP counter-break combat ordering.

## Weather Notes

- Weather JSON uses `"weather": "rain"` or `"weather": "snow"`. CO-created weather also writes `"weather-turns-remaining"` and expires on a later `BeginTurn`.
- Rain and snow movement costs follow the AWBW weather tables, including Drake rain immunity, Olaf snow immunity, and Olaf treating rain like snow.
- Rain vision penalties are intentionally deferred because this simulator does not yet have fog-of-war or vision state; that broader subsystem is tracked by #25.

## HP Effect Notes

- Out-of-combat HP damage and HP drain are applied in true-health units, where 1 displayed HP equals 10 stored health. These effects floor targets at 1 stored health, so they cannot destroy units directly; healing caps at 100.
- Drake's Tsunami and Typhoon halve enemy fuel with integer truncation, then apply their HP damage. Typhoon also keeps the existing rain side effect.
- Rachel, Sturm, and Von Bolt missile-style powers use a 2-range diamond area. Target scoring follows the AWBW wiki criteria using the simulator's two-player friendly/enemy model, unit HP, unit cost, and property capture state. Loaded units are excluded because they are not represented as map occupants.
- Rachel's three Covering Fire target centers are selected before damage is applied, then each missile applies damage. Von Bolt's stun is represented internally until the affected player's next `BeginTurn`, where stunned units remain `"moved": true` for that turn.

## Range Notes

- Indirect range modifiers adjust maximum range only; minimum range is unchanged. Grit applies to all indirect units, Jake applies to vehicle indirects during COP/SCOP, and Max applies his -1 maximum-range penalty to indirect units without changing adjacent direct attacks.

## Economy Notes

- Colin, Hachi, and Kanbei cost modifiers currently apply to unit construction. Hachi's Merchant Union city deployment remains part of the broader production-effects follow-up.
- Sasha's Market Crash applies to the single opposing player in this simulator's two-player model.

## Action-State Notes

- Eagle's Lightning Drive COP only activates the COP stat chart; it does not clear moved flags.
- Eagle's Lightning Strike SCOP clears `"moved"` for the current player's map-present non-footsoldier units, including vehicles, air units, sea units, transports, Black Boats, and newly built non-footsoldier units.
- Infantry and Mechs are footsoldiers, so their capture progress and spent action state are preserved through Lightning Strike.
- Loaded units are not refreshed while they are cargo because they are not map occupants. If a transport unloads after Lightning Strike, the unloaded unit still becomes `"moved": true` under the simulator's existing unload rule.
- Lightning Strike is not a `BeginTurn`: it does not grant income, property repairs, property resupply, APC resupply, fuel-day processing, or capture-point changes. Black Boat repair is a normal action, so a Black Boat that repaired before Lightning Strike can repair again after being refreshed.

## Capture Notes

- Capture progress uses the simulator's displayed-HP calculation, where partial true-health values round up to the current displayed HP before applying capture modifiers.
- Sami's Double Time keeps the same 150% rounded-down capture bonus as her day-to-day ability. Victory March completes captures immediately for Infantry and Mechs, including damaged footsoldiers.
- Interrupted captures still reset to 20 capture points when the capturing unit leaves the property, and blocked captures remain unavailable while an enemy unit occupies the property.

## Tracked Follow-Up Issues

These AWBW mechanics are not implemented yet. They are tracked as GitHub issues so the markdown is only a summary, not the source of truth.

- [#23](https://github.com/ryparikh/AdvanceWarsServer/issues/23): CO production effects.
- [#25](https://github.com/ryparikh/AdvanceWarsServer/issues/25): fog, vision, hiding, and terrain-defense CO effects.
