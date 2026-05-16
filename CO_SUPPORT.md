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

## Intentionally Unsupported

These AWBW CO effects are not implemented yet. The simulator keeps their chart stat changes where present, but does not model the listed special effects.

- Mass damage, healing, or draining beyond Andy/Jess: Drake, Hawke, Kindle, Olaf, Rachel, Sturm, and Von Bolt.
- Economy and unit-cost effects: Colin, Hachi, Kanbei, Kindle, and Sasha.
- Production effects: Hachi city deployment and Sensei unit spawning.
- Turn and action-state effects: Eagle's extra actions.
- Fog, vision, hiding, and terrain-defense special effects: Javier, Lash, Sonja vision, and related fog-only behavior.
- Indirect-range special effects outside the currently implemented Jake helper, including Grit.
- Capture-specific power behavior beyond chart damage modifiers, including Sami's instant capture behavior.
