# DiscGolfCharacterFramework

Runtime source plugin.

## Runtime objects

- `UDiscGolfCharacterProfile`: baseline created-player data.
- `UDiscGolfAnimationLibrary`: references throw montages.
- `UDiscGolfThrowComponent`: authoritative animation/gameplay bridge for throw state.
- `UDiscGolfAnimInstance`: parent class for the project's animation Blueprint.
- `UDiscGolfAppearanceComponent`: optional standardized morph target bridge.
- `UDiscGolfCharacterSaveGame`: serialization structure for created-player settings.
- `UAnimNotify_DiscRelease`: exact release event.
- `UAnimNotify_ThrowPhase`: animation phase event.
- `UAnimNotify_ThrowFinished`: closes the throw state.

The plugin does not spawn a disc actor itself. Bind `OnDiscRelease` to the existing game-specific disc physics/spawn code.


## v1.4 runtime additions

- Disc/plastic/catalog data
- Player bag component
- Throw telemetry + Throw Lab save
- Telemetry shot replay
- Camera/replay/tracer bridge components
- Course definition + validation
- Audio event bridge
- Settings/UI route subsystems
- Tournament/scoring/career contracts
- AI golfer candidate contracts
- Flight regression data
- Integrated player-profile save

Optional Unreal plugins remain project adapters, not hard runtime dependencies.
