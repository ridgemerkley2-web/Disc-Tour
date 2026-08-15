# Forest Broadcast Presentation Bible

## Authority and intent

`AGENTS.md` remains the project-wide development bible. This document is the art-direction and interaction contract for presentation work, used with `Docs/PRESENTATION.md`, `Docs/GAME_DESIGN.md`, and `Tasks/MILESTONE_04_PRODUCTION_PRESENTATION.md`.

The target is an original premium disc-golf package that combines two useful presentation grammars:

- professional disc golf's persistent event, card, hole, score, and live-stat awareness;
- a modern golf game's sparse shot setup, spatial course reading, immediate execution feedback, tracer, and cinematic replay language.

This is inspiration, not imitation. Do not copy logos, exact palettes, scorebug geometry, typefaces, icon sets, motion stingers, audio, commentary, player likenesses, tournaments, course data, or branded terminology from DGPT, Disc Golf Network, PGA TOUR, or PGA TOUR 2K.

## Research basis

Research reviewed on 2026-08-12:

- DGPT's official 2025 championship viewing material combines live coverage, a studio show, live scoring, standings, and player statistics: <https://www.dgpt.com/news/2025-dgptc-how-to-watch-preview/>.
- DGPT describes Disc Golf Network as supplying a baseline live stream and graphics package to a broadcast partner: <https://www.dgpt.com/announcements/yle-partnership-2025-worlds/>.
- An official DGPT final-round highlights example shows the tour's card-first broadcast framing and post-produced event storytelling: <https://www.youtube.com/watch?v=pCJr6So87c4>.
- PGA TOUR 2K25's official swing guide makes the shot HUD part of the input loop and returns feedback for path, contact, transition, and rhythm after the swing: <https://pgatour.2k.com/2k25/up-your-game/how-to-swing/>.
- Its official shot-shaping guide pairs wind, lie/slope reading, a scout camera, and explicit shot-shape controls: <https://pgatour.2k.com/2k25/up-your-game/shot-shaping/>.

The transferable lesson is hierarchy: broadcast context persists, interactive shot information appears only when actionable, and diagnostic detail arrives after the shot or in replay.

## Core identity

Working name: **Forest Broadcast**.

Three adjectives: **grounded, precise, alive**.

- Grounded: forest, soil, paper, rope, rain, bark, and brushed metal rather than neon esports styling.
- Precise: compact information groups, stable alignment, numeric clarity, and honest telemetry.
- Alive: restrained motion, wind-responsive ambience, crowd anticipation, and cameras that reveal the route.

The world should remain the hero. UI frames the course; it does not wallpaper the screen.

## Presentation modes

### Watch mode

Used for flyovers, live flight, replay, scorecard, hole intro, and round completion.

- Show event/course identity, player or card identity when available, hole/par/distance, score, shot number, and competitive consequence.
- Allow richer lower thirds, replay labels, route annotations, and comparative stats.
- Use authored camera anchors and immutable trajectory samples only.

### Play mode

Used while the player aims, selects equipment, shapes a shot, and times release.

- Keep a compact scorebug persistent.
- Show only actionable setup: disc, hand, power, hyzer, nose, wind, lie, and target distance.
- Keep the center and intended flight corridor clear.
- Put tuning, regression, and performance telemetry in a clearly subordinate developer layer; production builds must be able to hide it.

The Canvas proving ground now resolves Play, Flight, Review, Replay, and Complete as side-effect-free presentation states. The developer layer is hidden by default and can be toggled with the `DGT_ToggleDeveloperHud` console command or enabled at launch with `-DeveloperHUD`.

### Review mode

Used immediately after release and during replay.

- Present grade, timing direction, release speed, spin, aim, hyzer, nose, launch, and lie modifiers.
- Explain the miss without implying that presentation changed the result.
- Favor a two-to-three second transient card, then collapse to the persistent scorebug unless replay is active.

## Color system

Canvas values are linear RGB and are implemented in `DiscGolfHUD.cpp`.

| Token | Linear RGB | Role |
|---|---:|---|
| Charcoal Pine | `0.020, 0.045, 0.040` | primary panels |
| Raised Pine | `0.030, 0.072, 0.062` | score cells and focused surfaces |
| Signal Teal | `0.180, 0.780, 0.640` | live/current/action accent |
| Mist Teal | `0.460, 0.750, 0.660` | secondary labels and controller guidance |
| Tournament Amber | `0.960, 0.670, 0.200` | score, perfect release, replay, completion |
| Success Mint | `0.380, 0.880, 0.560` | legal lie, ready state, pass |
| Penalty Coral | `0.960, 0.340, 0.290` | OB, hazard, failure, destructive warning |
| Paper White | `0.960, 0.950, 0.890` | primary text |
| Fog Gray | `0.690, 0.770, 0.710` | secondary text |
| Slate Gray | `0.430, 0.540, 0.490` | tertiary and developer labels |

Rules:

- Amber is scarce. Reserve it for competitive meaning, replay, and exceptional execution.
- Coral never decorates; it communicates a penalty, failure, or dangerous action.
- Signal Teal means current or interactive, not merely important.
- Do not use DGPT's red/blue pairing or reproduce PGA TOUR 2K's branded aqua/black treatment.
- Every critical state needs text or shape in addition to color.

## Typography and layout

- Production fonts should be original or properly licensed. Preferred structure is a condensed sans for numeric/event labels and a highly legible humanist sans for body and controls.
- Use uppercase for short broadcast labels, not paragraphs.
- Use tabular numerals for scores, distances, speed, spin, and timing.
- Maintain the current 1280x720 reference canvas and safe-frame behavior until Common UI migration.
- Score is the strongest number; hole identity is the strongest phrase; development data is the weakest layer.
- Favor left alignment and consistent data columns. Avoid centered telemetry blocks.

## Component contracts

### Persistent scorebug

Required: mode label, hole number/count, hole name, par, distance, and round score. Play uses the full `TOUR PLAY` surface; Flight and Replay collapse to a shorter, narrower scorebug and remove route/setup navigation text. `TO PAR` remains the score cell label.

### Shot setup

Required: disc name and flight numbers, plastic, hand, power, hyzer, nose, wind, lie, and putting pace/aim when relevant. Do not show post-release diagnostics here.

### Shot feedback

Required: release grade and timing, speed, spin, aim/hyzer/nose/launch offsets, and any lie multiplier. `SHOT FEEDBACK` appears for three seconds after authoritative release, then yields to the unobstructed live-flight view. Replay owns separate chrome rather than reviving the transient live card.

### Scorecard

Required: course, holes completed, strokes, penalties, score to par, hole rows, current-hole marker, and the valid next action. Table hierarchy may feel like a live broadcast, but geometry and styling must remain original.

### Tracer and replay

- Live tracer: Signal Teal with accessible contrast and a subtle taper.
- Ground play: Tournament Amber or an earth-tone derivative.
- Replay: Tournament Amber plus a clear replay label and time/progress.
- Never resimulate, steer, collide, score, or export from presentation actors.

### Cameras and transitions

- Gameplay cuts must reveal the decision: launch line, mid-flight shape, landing consequence.
- Use clean 160-240 ms UI movements, except deliberate scorecard/modal transitions up to 320 ms.
- Avoid branded broadcast wipes. Prefer route-line reveals, restrained panel slides, and short exposure/iris changes.
- Replay can use longer lenses and more dramatic framing, but must preserve disc visibility.

## Audio and atmosphere

- Build original forest beds, distant gallery texture, rope/signage movement, disc air, ground, basket, and result cues.
- Quiet before release; allow the course bed and crowd anticipation to breathe.
- Reaction intensity follows outcome and event context, never UI animation alone.
- Existing semantic presentation events remain the only gameplay-facing trigger contract.

## Accessibility and scalability

- Provide UI scale, high-contrast, color-vision-safe, reduced-motion, tracer-width, and telemetry-detail settings during Common UI migration.
- Maintain readable type and safe zones at 1280x720, 1920x1080, ultrawide, and 4:3 regression captures.
- Visual-quality presets can change materials, foliage, crowds, effects, and shadows; they cannot change competitive collision or simulation.

## Acceptance checklist

- A new player can identify hole, par, distance, score, lie, selected disc, wind, and the next valid action in under three seconds.
- The intended route remains visible during setup.
- A miss is explainable from the shot feedback or replay data.
- Scorecard, replay, flyover, timing, active-flight, completion, and controls states do not overlap incorrectly.
- Presentation emits no physics, rules, score, save, or trajectory-export changes.
- The UI does not reproduce protected third-party trade dress.
- Source checks, reference flight, editor build, automation, and visual captures pass after implementation changes.
