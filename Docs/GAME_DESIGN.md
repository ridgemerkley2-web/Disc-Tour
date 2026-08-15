# Game Design Direction

## Product vision
A premium disc-golf game with **simulation-grade throwing depth** and **broadcast-quality presentation**. The player should be able to enjoy it at three layers:

1. **Accessible:** choose a disc, aim, set power, release.
2. **Tour:** manage line, stability, wind, timing, and lie.
3. **Simulation:** control nose, hyzer/anhyzer, launch angle, spin/release quality, stance, throw style, disc/plastic/wear, and environmental reads.

All modes should use the same underlying disc simulation. Easier modes add assists rather than replacing physics with fake trajectories.

## Core loop
1. Read the hole and wind.
2. Choose mold/plastic.
3. Choose backhand/forehand and shot shape.
4. Set aim/power/release parameters.
5. Execute timing/throw mechanic.
6. Watch the disc through broadcast/chase presentation.
7. Evaluate lie and remaining distance.
8. Repeat until holed out.
9. Score round/event and progress career/equipment mastery.

### Current round implementation

The v0.4 shell now exercises this loop across Pine Ridge Opening, Needle Gate, and Gallery Lake. Each hole records strokes and penalties once, the scorecard compares every played result to authored par, and round completion requires all three ordered holes. The player explicitly advances after a completed hole so telemetry, replay, and score review remain available before the next tee. Direct hole starts are development/practice states and never fabricate earlier scores.

The next presentation pass should preserve this authority while replacing the Canvas scorecard and transition messaging with production UI, animation, audio, and tournament dressing.

## Throw skill
A finished throw mechanic should combine:
- setup decisions,
- execution timing,
- optional analog tempo/path,
- player attributes/skill assists,
- lie/stance difficulty,
- fatigue or pressure only if it improves gameplay rather than feeling arbitrary.

Misses should be explainable. The HUD/replay system should be able to tell the player whether a miss came from timing, nose, angle, aim, wind read, or disc choice.

The current vertical slice now resolves timing into an explicit Perfect/Great/Good/Poor release and reports early/late direction, quality percentage, release speed/spin, and signed aim/hyzer/nose/launch effects. Future analog-path, stance, pressure, and player-attribute systems should contribute to the same resolved-release contract rather than modifying the flight solver invisibly.

## Disc system
Long-term bag model:
- mold,
- plastic,
- weight,
- wear/seasoning,
- cosmetic stamp/color,
- player familiarity/mastery.

Real branded discs can be considered later only with licensing. Production development should use fictional molds so physics and career systems are independent of licensing deals.

## Game modes
### Practice
Driving range, putting practice, wind lab, shot-shaping challenges, trajectory telemetry.

### Quick Round
Select course/layout/weather and play immediately.

### Career
Start with a limited bag and local events. Progress through increasingly difficult tournaments, sponsorship-style fictional brand relationships, course unlocks, and skill progression.

### Tournament
Multi-round events, cut lines, tee times, broadcast presentation, leaderboards, pressure moments, weather progression.

### Multiplayer — later
Turn-based asynchronous-friendly rounds are a natural fit. Real-time card play can come later after authoritative shot state is robust.

## Putting
Putting should become its own shot context rather than a weak drive:
- Circle 1 / Circle 2 context,
- spin and push putting styles later,
- wind sensitivity,
- height/pace control,
- chain interaction,
- comeback-putt consequences.

## Difficulty assists
Potential assist sliders:
- aim cone,
- timing-window width,
- nose-angle stabilization,
- hyzer-angle stabilization,
- wind suggestion,
- disc recommendation,
- putting read,
- automatic launch angle.

Competitive modes can standardize assist sets.

## Presentation
Target the emotional language of televised golf/disc-golf coverage without copying another game's proprietary package:
- establishing flyovers,
- tee-card lower thirds,
- clean scorebug,
- shot tracer,
- flight telemetry replays,
- landing/basket cameras,
- gallery reactions,
- event signage,
- announcer hooks,
- leaderboard tension.

## Progression philosophy
Avoid turning physics into RPG dice rolls. Progression should primarily affect:
- assist strength,
- execution forgiveness,
- cosmetic identity,
- access to equipment/course/event content,
- optional player archetype animation/release characteristics.

A perfectly executed throw with known parameters should remain physically consistent.
