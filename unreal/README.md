# Rubicon UE5 project (M1 + M2)

UE 5.4+, OpenXR, C++. Three modules:

| Module | Type | Contents |
|---|---|---|
| `RubiconNarrativeCore` | Runtime | `NarrativeCore/`: the deterministic core, standard C++17 only (no UObject, clock, IO or unseeded randomness), compiled unchanged by `native/` for parity. UE layer: Blueprint types, `URubiconEpisodeAsset` / `URubiconLocaleAsset`, `URubiconNarrativeSubsystem` (fixed 10 ms step), automation tests. |
| `Rubicon` | Runtime | Golgotha greybox, tagged gaze actors, embodied body (fixed IK), embodied pawn, gaze resolver, atmosphere driver, elevation director, debug HUD, game mode. |
| `RubiconEditor` | Editor | `Rubicon.ImportEpisode`, `Rubicon.CreateGolgothaMap`, editor validator. |

```
engine frame ──► FFixedStepClock ──► N × Step(manifest, state, input, 10 ms) ──► events ──► one delegate per event type
   ▲                                              │
   │ gaze, breath, commands (latched)             └──► SelectFrame ──► FRubiconFrameOutputs ──► HUD, atmosphere, elevation, pawn
URubiconGazeResolverComponent, pawn input
```

## First run

1. Generate project files from `Rubicon.uproject` and build `RubiconEditor` (Development Editor).
2. Open the editor and run, in the console:
   - `Rubicon.ImportEpisode`: validates `../../content/*.json` with the ported validator and writes `/Game/Rubicon/Data/DA_PassionEpisode` + `DA_Locale_en`. It writes nothing if validation fails.
   - `Rubicon.CreateGolgothaMap` (optional): saves `/Game/Rubicon/Maps/Golgotha` with the greybox and all gaze actors placed, so they can be moved by hand. Then point `GameDefaultMap` / `EditorStartupMap` at it.
3. Play in VR Preview. Any empty map also works: the game mode spawns the greybox, and in non-shipping builds a missing episode asset falls back to reading `content/*.json` directly.

## Automation tests

```bash
npm run golden            # in the repo root: golden/replay-*.json, validator-cases.json, math-vectors.json
UnrealEditor-Cmd unreal/Rubicon/Rubicon.uproject -ExecCmds="Automation RunTests Rubicon; Quit" -unattended -nullrhi -RubiconRepoRoot=$PWD
```

| Test | Checks |
|---|---|
| `Rubicon.NarrativeCore.GoldenReplays` | every golden: identical `timeline.enter` + `saying.trigger` (acceptance), RT trace within 1e-6 (acceptance), every event identical, state and event hashes identical; math vectors bit-exact; floating-point self-test |
| `Rubicon.NarrativeCore.ValidatorParity` | 23 mutation cases: same errors and warnings, in order, as `src/validate.ts` |
| `Rubicon.NarrativeCore.Safeguards` | crowd gaze raises output, looking away lowers it gradually, Comfort and Witness cut output on the same tick from every state, frame chunking never changes results |
| `Rubicon.Game.*` | two-bone IK, head clamp, head speed (exertion input), the cross as a gaze actor, gaze hysteresis and fallbacks, elevation transforms, every manifest gaze target has a tagged actor |

The same core checks run without Unreal: `npm run parity` (CMake + any C++17 compiler).

## Controls

| Action | Quest/Touch | Keyboard | Gamepad |
|---|---|---|---|
| Consent at the Threshold / speak the saying | right trigger | Space, Enter | A |
| Witness (toggle) | A | W | Y |
| Comfort (toggle) | B | C | X |
| Pause (toggle) | X | P | Start |
| Exit | left menu | Escape | Back |
| Visual layer | Y | L | |
| Recenter (position only) | left stick click | R | |
| Debug HUD | right stick click | F1 | |

`rubicon.DebugHUD 0|1` toggles the HUD from the console.

## Dialogue cues and dwell tiers

`OnDialogueCue(TimeMs, FRubiconCueInfo)` fires when the world speaks. Play the VO or sound (`AudioCueId`) at the gaze actor named by `Origin`. The narrator has no origin and is head-locked like the internal voices. Duck everything by `ExternalDuckDb` so the storm can drown the taunts. `FRubiconFrameOutputs.Cue` carries the live subtitle, citations, evidence, `Rendering` and progress. A `paraphrase` rendering must never be shown in quotation marks. `GazeTier` shows the current dwell tier (glance, sustained, long, extreme). The debug HUD prints both. Without an HMD, the mouse looks around (desktop debugging only).

## Exertion, loss of consciousness and tellings

Looking anywhere is always free. The pawn measures head motion in tracking space (angular deg/s and linear cm/s, smoothed over 0.25 s) and calls `SetHeadMotion` every frame; desktop mouse look never counts. `SetVoiceActivity(0..1)` takes microphone activity (no microphone source is wired yet; feed it from an envelope follower on the capture bus). Sustained motion or talking builds exertion; at full exertion `OnConsciousnessChange(TimeMs, false, "exertion")` fires and `FRubiconFrameOutputs.View` becomes `Telling`, with `ViewVantage` set to the Gospel's vantage (John: beside the cross; Matthew, Mark, Luke: `WomenAfar`). The pawn fades out over 1.2 s, moves there, and fades in; the telling plays through `OnDialogueCue` (kind `Telling`, narrator, head-locked). On waking (`recovered`, `saying`, `death`, `setting`) it fades back to the cross. `Exertion` is in the frame for debugging only: never show it to the player. `SetSwoonEnabled(false)` turns the mechanic off.

The cross itself is the `cross` gaze actor (`ERubiconProxyShape::Cross`, collision, attached to `CrossRoot`): the only target that brings relief.

## Headset acceptance checklist (manual)

- [ ] Threshold shows the advisory; the right trigger starts Elevation.
- [ ] Elevation, slow tilt: the world tilts about the cross base over 9 s, then settles; head rotation stays free and the camera is never rotated by code.
- [ ] Elevation with Comfort on before or during the raise: fade, cut to upright, fade in; no world motion.
- [ ] Forgiveness in stable first person, seated and standing (recenter with the left stick).
- [ ] Gazing at the crowd raises `RT output` on the debug panel within about 5 s; looking at the horizon brings it down over about 10 s, not at once.
- [ ] Comfort and Witness drop `RT output` immediately, in every state.
- [ ] Looking around slowly, at anything, never makes the player lose consciousness; rapid head movement for several seconds does.
- [ ] Unconscious: a slow fade, the view from the Gospel's vantage, a telling in the narrator's voice; resting brings the player back to the cross under a fade.
- [ ] Gazing at the crossbeam or the upright resolves `cross` and lowers `RT output` below every other target.
- [ ] No input changes or stalls the timeline (`state` only ever advances in canonical order).

## Engineering notes

- **Floating point.** The core matches V8 bit for bit: `Exp`/`Pow` are ports of V8's fdlibm routines (including V8's one-expression deviation in `pow`), and every `NarrativeCore` file disables FMA contraction by pragma. `clang -ffp-contract=fast` overrides the pragmas; the module's startup self-test logs an error if that ever happens. Even then events and the timeline still match, and the trace stays within 1e-16.
- **Latency.** Commands are applied on the next 10 ms tick, so the delay is at most one tick. Engine readers see the frame published at the end of the previous engine frame.
- **Coordinates.** The cross base is the origin, +X is the direction the crucified faces, +Y is his right, and units are cm. Each `Layout[].Basis` records the passage or tradition behind a placement.
- **Atmosphere.** Lux, colour temperature and aerosol optical depth come from the manifest. AOD scales Mie scattering and absorption, and illuminance is smoothed in log space. On HDR renderers exposure is limited to EV100 8.5 to 15, so the darkness reads as dark, not black. The LDR path (mobile without HDR) uses a logarithmic intensity map.
