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
- Implemented terrain/range/luck helpers: Jake plains attack, Koal road attack, Jake COP/SCOP indirect range for vehicles, Nell/Rachel/Flak/Jugger/Sonja luck bounds, and Sonja SCOP counter-break combat ordering.

## Weather Notes

- Weather JSON uses `"weather": "rain"` or `"weather": "snow"`. CO-created weather also writes `"weather-turns-remaining"` and expires on a later `BeginTurn`.
- Rain and snow movement costs follow the AWBW weather tables, including Drake rain immunity, Olaf snow immunity, and Olaf treating rain like snow.
- Rain vision penalties are intentionally deferred because this simulator does not yet have fog-of-war or vision state; that broader subsystem is tracked by #25.

## HP Effect Notes

- Out-of-combat HP damage and HP drain are applied in true-health units, where 1 displayed HP equals 10 stored health. These effects floor targets at 1 stored health, so they cannot destroy units directly; healing caps at 100.
- Drake's Tsunami and Typhoon halve enemy fuel with integer truncation, then apply their HP damage. Typhoon also keeps the existing rain side effect.
- Rachel, Sturm, and Von Bolt missile-style powers use a 2-range diamond area. Target scoring follows the AWBW wiki criteria using the simulator's two-player friendly/enemy model, unit HP, unit cost, and property capture state. Loaded units are excluded because they are not represented as map occupants.
- Rachel's three Covering Fire target centers are selected before damage is applied, then each missile applies damage. Von Bolt's stun is represented internally until the affected player's next `BeginTurn`, where stunned units remain `"moved": true` for that turn.

## Economy Notes

- Colin, Hachi, and Kanbei cost modifiers currently apply to unit construction. Hachi's Merchant Union city deployment remains part of the broader production-effects follow-up.
- Sasha's Market Crash applies to the single opposing player in this simulator's two-player model.

## Tracked Follow-Up Issues

These AWBW mechanics are not implemented yet. They are tracked as GitHub issues so the markdown is only a summary, not the source of truth.

- [#23](https://github.com/ryparikh/AdvanceWarsServer/issues/23): CO production effects.
- [#24](https://github.com/ryparikh/AdvanceWarsServer/issues/24): Eagle extra-action CO power behavior.
- [#25](https://github.com/ryparikh/AdvanceWarsServer/issues/25): fog, vision, hiding, and terrain-defense CO effects.
- [#26](https://github.com/ryparikh/AdvanceWarsServer/issues/26): remaining indirect-range CO effects.
- [#27](https://github.com/ryparikh/AdvanceWarsServer/issues/27): capture-specific CO power behavior.
