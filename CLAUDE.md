# CLAUDE.md — Project Rubicon, Prototype 01: The Passion

First-person VR experience of the crucifixion at Golgotha, Dark Psychological Edition. The player embodies Jesus during the final hours. Emotional force comes from an invasive internal-monologue system (Racing Thoughts), attention, relationships, language and silence. Physical violence is suggested, never shown graphically, and never simulated through the body.

This repo holds the engine-agnostic narrative core (TypeScript reference), the content, and the UE5 project (`unreal/Rubicon`). The C++ core in `unreal/Rubicon/Source/RubiconNarrativeCore/*/NarrativeCore/` is a bit-exact port of `src/`; nothing in either may depend on an engine, a clock, IO or randomness outside the seeded PRNG.

## Commands

```bash
npm install
npm test            # build + 55 tests (node:test)
npm run validate    # manifest + locale validation
npm run simulate    # headless slice trace: Threshold -> Elevation -> Forgiveness -> Witness (arg: seed)
npm run golden      # golden/replay-*.json (+ RT trace), validator-cases.json, math-vectors.json (regenerable, not committed)
npm run parity      # golden + CMake build of the C++ core (native/) + 12 C++ tests + bit-exact parity check
npm run format:content   # canonical formatting of content/*.json (also: node scripts/format-json.mjs --check)
npm run docs:sources     # regenerate docs/SOURCES.md (every source, rendering, debate, and what cites it)
```

## Layout

```
content/passion.episode.json   canonical episode manifest (single source of truth for content)
content/locale.en.json         KJV dialogue, fragment text, UI strings
src/types.ts                   type contract + invariants I1 to I6
src/validate.ts                structural and semantic manifest validation
src/session.ts                 pure reducer: step(manifest, state, input, dt) -> { state, events }
src/racingThoughts.ts          continuous intensity model + deterministic fragment scheduler (incl. dwell tiers)
src/cues.ts                    dialogue cues: the world speaking (spoken KJV, sounds, narrated history)
src/frame.ts                   selectFrame(): the only view the engine adapter reads
src/data.ts                    typed accessors (lastWords, maxAutoRunMs)
src/replay.ts                  replay runner, state and event hashes
src/golden.ts                  golden replay builder (inputs, events, hashes, per-tick RT trace)
src/validatorCases.ts          validator parity cases (manifest mutations + expected messages)
src/prng.ts                    mulberry32, smoothing, stable hashing
src/node/load.ts               Node-only IO boundary
src/cli/                       validate, simulate, golden
tests/                         manifest, timeline, racingThoughts, determinism, golden
native/                        standalone CMake build of the C++ core: rubicon_parity, rubicon_core_tests
unreal/Rubicon/                UE5 project (see unreal/README.md): RubiconNarrativeCore, Rubicon, RubiconEditor
docs/VERTICAL_SLICE_PLAN.md    milestones M0 to M6, audio and lighting direction, resolved conflicts, UE5 port map
docs/SOURCES.md                generated source catalogue for reviewers
```

## Invariants (tested; never break these)

- **I1** Timeline order is immutable: threshold, elevation, forgiveness, paradise, mother, abandonment, thirst, completion, surrender, release, spear, contemplation. Never skipped, never re-entered.
- **I2** Experience mode, visual layer, comfort, settings and the evidence UI never reset, rewind or reorder the timeline.
- **I3** Exit and Witness Mode are accepted in every state, within one tick.
- **I4** No soft-lock: after the consent gate every state auto-advances on a bounded clock.
- **I5** Racing Thoughts read the timeline and never write it.
- **I6** Same manifest, seed and input stream give identical state and event hashes, in TypeScript and in C++ (bit-exact; `npm run parity`).
- **I7** Dialogue cues read timeline, gaze and mode; they never write the timeline or Racing Thoughts. No cue holds a phase open.

## Content rules

- Every saying keeps its own Gospel attribution. The Seven Last Words are a later harmonization; say so in the overlay.
- Luke 23:34a is a textual variant (absent from P75 and Codex Vaticanus; double-bracketed in Nestle-Aland). It must stay marked.
- Four evidence categories, always visible: gospel-text, textual-variant, historical-inference, artistic-interpretation.
- All internal monologue is artistic-interpretation and is disclosed live in the overlay. Prefer fragments that echo scripture (Psalm 22, Mark 14:36, the taunts of Mark 15, Matthew 27:40 echoing Matthew 4:3) and name the passage.
- The crowd is built from specific Gospel groups. Never portray Jewish people collectively as responsible.
- From Release onward the player is in Witness; embodiment is refused. The spear (John 19:33-34) is seen from the beloved disciple's vantage (John 19:35).
- No health bars, no struggle mechanics, no pain haptics, no simulated choking or forced breathing, no score for enduring.
- KJV is the shipped English text (public domain). Greek and the Aramaic reconstruction of the cry of abandonment are labeled historical-inference.
- Every source has a `sourceType` (gospel, new-testament, hebrew-bible, ancient-text, archaeology, scholarship). Ancient texts, archaeology and scholarship are always historical-inference. Non-KJV text carries its rendering: public-domain translation (name the translator), own translation from the original, paraphrase, or description. A paraphrase is never quoted; record authenticity or identification questions in `debate`.
- Spoken cues are verbatim KJV from their cited passages (ellipses mark omissions), attributed to the group that said them. Narrations quote only what a cited non-paraphrase source contains. Both are checked mechanically by `tests/dialogue.test.ts`.
- History is narrated from outside only (Witness and Contemplation); the embodied player hears the scene, never a lecture.
- Dwell-tiered lines echo the specific taunts and words the Gospels attribute to specific groups, and the crowd tiers always include its non-mocking members (Luke 23:27, 48). The internal monologue never says "the Jews" (tested).
- Sources whose framing assigns collective responsibility (Mara bar Serapion's "wise king" line, Melito's *On Pascha*) are not voiced or displayed.

## Racing Thoughts model (summary)

`target = clamp(floor' + w*d*(1-floor'), 0, ceiling(t))` for stress weight w ≥ 0, or `floor' * (1 + w*d)` for relief (w < 0), where `d = 1 - exp(-dwell/3000ms)`. Asymmetric smoothing: fast rise, slow decay, so looking away helps only gradually. Witness entry multiplies intensity by 0.35 then decays over about 2.5 s (the echo). Comfort snaps output gain to 0.04 on the same tick and ramps back over about 4 s on release. The player's intensity ceiling is a hard cap. Fragment rate scales with output^1.6; voice cap is 1 + floor(output × 4); recent fragments are excluded so nothing loops. All parameters live in the manifest.

## Progress

**Done (M0)**
- Type contract, validator with semantic rules (variant marking, post-death embodiment lock, stress agreement, reference integrity, comfort and witness safety bounds).
- Full episode manifest: setting and reconstruction notes, 14 cast members, 40 KJV-sourced references with manuscript notes, 14 gaze targets with stress tags, 3 witness vantages, 12 timeline states with timing, per-state Racing Thoughts curves, stress overrides and atmosphere keyframes, 25 fragments.
- Deterministic reducer, Racing Thoughts system, frame selector, replay hashing, golden replay exporter.
- 34 passing tests covering every acceptance criterion in the spec that the core can prove.

**Done (M1, verified outside the engine)**
- `RubiconNarrativeCore`: std-only C++17 port of types, prng, racingThoughts, session, frame, validator and replay hashing, as pure functions over const structs. `Exp`/`Pow` are bit-exact ports of V8's fdlibm routines, so C++ equals Node bit for bit.
- `npm run parity` (g++ 13, clang 18, with and without `<charconv>`): 4/4 golden replays with identical events, RT trace error 0, identical state and event hashes; validator 15/15 cases message-identical; 40000/40000 math vectors bit-exact; 10 C++ acceptance tests.
- UE layer: Blueprint types, episode and locale data assets (canonical JSON stored and compiled at runtime through the tested path), `URubiconNarrativeSubsystem` (fixed 10 ms step with accumulator, one delegate per event type, `FRubiconFrameOutputs` to Blueprint, replay export), editor importer and validator, automation tests (`Rubicon.NarrativeCore.*`).
- TS: golden export gains a per-tick RT trace, a coverage replay that exercises every command, validator cases and math vectors. The validator now bounds `fragmentRate`, `externalDuck`, `affinityMultiplier` and requires integer voice counts.

**Done (M2, code complete, not yet built or run in the engine)**
- Golgotha greybox (quarry edge, knoll, road, city wall, low cross, hill-ring horizon, sky atmosphere), one `Gaze.Target.<id>` actor per manifest target (own_hands on the embodied body), each placement annotated with its basis.
- Embodied body posed by analytic two-bone IK; pawn with free head rotation, position clamp (12 cm, 8 cm seated), seated and standing calibration, position-only recenter; Witness moves under a fade.
- Elevation: slow world tilt (default) and fade-settle (Comfort or Reduced Effects). The camera is never rotated by code.
- Gaze resolver (trace, 4 degree cone, 150 ms hysteresis, sky, horizon and ground fallbacks); atmosphere driver (lux, CCT, AOD to sun, Mie and fog); debug HUD (world-space panel plus desktop canvas); `Rubicon.Game.*` tests.

**Done (dialogue and history)**
- Dwell tiers: glance (0 s), sustained (3 s), long (9 s), extreme (18 s); 62 tiered lines over 15 targets, exclusive per tier, embodied only.
- Dialogue cues: 34 in all. Luke's exchange before Paradise, Elias after the cry, the sponge after thirst, the centurion and the crowd going home after death, the beloved disciple's record at the spear; gaze-held taunts in the groups' own words; 15 Witness narrations and 4 Contemplation readings drawn from ancient texts, archaeology and scholarship (see docs/SOURCES.md). Rules: fit, floor, preempt, once.
- Paradise `approachMinMs` raised from 8 s to 15 s so the request is heard before the answer (validated via `beforeSaying`).
- New points of interest: road, city wall, garden (manifest, greybox, coverage test).
- C++: bit-exact port (5 goldens incl. a dwell profile, 21 validator cases); UE: `OnDialogueCue`, `FRubiconFrameOutputs.Cue` and `GazeTier`, cue info on the episode asset, HUD lines.

**Not yet verified (needs a UE 5.4 machine and a headset)**: compiling the UE modules (written against the 5.4 API but never compiled here), running the automation tests in-editor, and the headset checklist in `unreal/README.md`.

**Next (M3 onward, see docs/VERTICAL_SLICE_PLAN.md)**
1. Build in UE 5.4, fix any API drift, run `Automation RunTests Rubicon`, walk the headset checklist.
2. MetaSounds Racing Thoughts graph driven by `output`, `maxVoices`, `externalDuckDb` and `OnRacingThoughtsFragment`; temp VO for fragments and cues (spatialized at `Cue.Origin`, narrator head-locked, ducked by `externalDuckDb`).
3. Forgiveness end to end with subtitles and the evidence overlay.
4. Witness vantage polish, Comfort vignette, voice and gesture bindings for all safeguards.
5. Playtest protocol and reviewer sign-off.

## Open decisions (ask before changing)

- Whether to add a self-recrimination fragment category (currently omitted; see the plan's resolved conflicts).
- Spoken language of the sayings: English KJV, Koine Greek, or reconstructed Aramaic for the cry only.
- Theological layer art direction beyond "vast and cold".
- Fragment rate tuning after the first playtest.
- Witness orientation: by default Witness moves the player by position only and they turn their own head toward the cross. `URubiconSettings::bAlignYawOnWitness` can snap yaw while the view is fully black. Which should be the default?
- Cue tuning after the first playtest: dwell thresholds (2.5 s for a cue, 3/9/18 s for tiers), whether narrations should also be offered as text-only in the evidence archive, and narrator voice casting.
- Evidence label for Hebrew Bible and other New Testament sources: they use the `gospel-text` category (canonical text) and show their `sourceType` beside it. Rename the category to "scripture text"? That changes an invariant, so ask first.

## Working conventions

- Content changes go in the JSON manifest, then `npm run validate`, then `npm test`.
- New behavior lands with a test that names the invariant or acceptance criterion it protects.
- Keep the core pure: no `Date`, no `Math.random`, no IO outside `src/node` and `src/cli`.
- Any change to `src/` core behaviour lands with a test and the matching change in `NarrativeCore/`; `npm run parity` must pass. Use only `Math.exp` and `Math.pow` (ported); any new transcendental function needs a bit-exact C++ port first.
- Prefer small, reviewable diffs.
