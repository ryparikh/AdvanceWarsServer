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
- Implemented movement modifiers: Adder, Andy SCOP, Drake sea units, Jake SCOP, Jess vehicles, Koal, Max direct units, Sami transports/footsoldiers, and Sensei transports.
- Implemented terrain/range/luck helpers: Jake plains attack, Koal road attack, Jake COP/SCOP indirect range for vehicles, Nell/Rachel/Flak/Jugger/Sonja luck bounds, and Sonja SCOP counter-break combat ordering.

## Tracked Follow-Up Issues

These AWBW mechanics are not implemented yet. They are tracked as GitHub issues so the markdown is only a summary, not the source of truth.

- [#20](https://github.com/ryparikh/AdvanceWarsServer/issues/20): weather mechanics and weather-changing CO powers.
- [#21](https://github.com/ryparikh/AdvanceWarsServer/issues/21): mass damage, healing, and HP-drain CO power effects.
- [#22](https://github.com/ryparikh/AdvanceWarsServer/issues/22): CO economy and unit-cost effects.
- [#23](https://github.com/ryparikh/AdvanceWarsServer/issues/23): CO production effects.
- [#24](https://github.com/ryparikh/AdvanceWarsServer/issues/24): Eagle extra-action CO power behavior.
- [#25](https://github.com/ryparikh/AdvanceWarsServer/issues/25): fog, vision, hiding, and terrain-defense CO effects.
- [#26](https://github.com/ryparikh/AdvanceWarsServer/issues/26): remaining indirect-range CO effects.
- [#27](https://github.com/ryparikh/AdvanceWarsServer/issues/27): capture-specific CO power behavior.
