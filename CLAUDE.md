# CLAUDE.md — Project Rubicon, Prototype 01: The Passion

First-person VR experience of the crucifixion at Golgotha, Dark Psychological Edition. The player embodies Jesus during the final hours. Emotional force comes from an invasive internal-monologue system (Racing Thoughts), attention, relationships, language and silence. Physical violence is suggested, never shown graphically, and never simulated through the body.

This repo is the engine-agnostic narrative core plus content. UE5 is the target engine; nothing in `src/` may depend on an engine, a clock, IO or randomness outside the seeded PRNG.

## Commands

```bash
npm install
npm test            # build + 34 tests (node:test)
npm run validate    # manifest + locale validation
npm run simulate    # headless slice trace: Threshold -> Elevation -> Forgiveness -> Witness (arg: seed)
npm run golden      # export golden/replay-<seed>.json for C++ parity tests (regenerable, not committed)
```

## Layout

```
content/passion.episode.json   canonical episode manifest (single source of truth for content)
content/locale.en.json         KJV dialogue, fragment text, UI strings
src/types.ts                   type contract + invariants I1 to I6
src/validate.ts                structural and semantic manifest validation
src/session.ts                 pure reducer: step(manifest, state, input, dt) -> { state, events }
src/racingThoughts.ts          continuous intensity model + deterministic fragment scheduler
src/frame.ts                   selectFrame(): the only view the engine adapter reads
src/data.ts                    typed accessors (lastWords, maxAutoRunMs)
src/replay.ts                  replay runner, state and event hashes
src/prng.ts                    mulberry32, smoothing, stable hashing
src/node/load.ts               Node-only IO boundary
src/cli/                       validate, simulate, golden
tests/                         manifest, timeline, racingThoughts, determinism
docs/VERTICAL_SLICE_PLAN.md    milestones M0 to M6, audio and lighting direction, resolved conflicts, UE5 port map
```

## Invariants (tested; never break these)

- **I1** Timeline order is immutable: threshold, elevation, forgiveness, paradise, mother, abandonment, thirst, completion, surrender, release, spear, contemplation. Never skipped, never re-entered.
- **I2** Experience mode, visual layer, comfort, settings and the evidence UI never reset, rewind or reorder the timeline.
- **I3** Exit and Witness Mode are accepted in every state, within one tick.
- **I4** No soft-lock: after the consent gate every state auto-advances on a bounded clock.
- **I5** Racing Thoughts read the timeline and never write it.
- **I6** Same manifest, seed and input stream give identical state and event hashes.

## Content rules

- Every saying keeps its own Gospel attribution. The Seven Last Words are a later harmonization; say so in the overlay.
- Luke 23:34a is a textual variant (absent from P75 and Codex Vaticanus; double-bracketed in Nestle-Aland). It must stay marked.
- Four evidence categories, always visible: gospel-text, textual-variant, historical-inference, artistic-interpretation.
- All internal monologue is artistic-interpretation and is disclosed live in the overlay. Prefer fragments that echo scripture (Psalm 22, Mark 14:36, the taunts of Mark 15, Matthew 27:40 echoing Matthew 4:3) and name the passage.
- The crowd is built from specific Gospel groups. Never portray Jewish people collectively as responsible.
- From Release onward the player is in Witness; embodiment is refused. The spear (John 19:33-34) is seen from the beloved disciple's vantage (John 19:35).
- No health bars, no struggle mechanics, no pain haptics, no simulated choking or forced breathing, no score for enduring.
- KJV is the shipped English text (public domain). Greek and the Aramaic reconstruction of the cry of abandonment are labeled historical-inference.

## Racing Thoughts model (summary)

`target = clamp(floor' + w*d*(1-floor'), 0, ceiling(t))` for stress weight w ≥ 0, or `floor' * (1 + w*d)` for relief (w < 0), where `d = 1 - exp(-dwell/3000ms)`. Asymmetric smoothing: fast rise, slow decay, so looking away helps only gradually. Witness entry multiplies intensity by 0.35 then decays over about 2.5 s (the echo). Comfort snaps output gain to 0.04 on the same tick and ramps back over about 4 s on release. The player's intensity ceiling is a hard cap. Fragment rate scales with output^1.6; voice cap is 1 + floor(output × 4); recent fragments are excluded so nothing loops. All parameters live in the manifest.

## Progress

**Done (M0)**
- Type contract, validator with semantic rules (variant marking, post-death embodiment lock, stress agreement, reference integrity, comfort and witness safety bounds).
- Full episode manifest: setting and reconstruction notes, 14 cast members, 40 KJV-sourced references with manuscript notes, 14 gaze targets with stress tags, 3 witness vantages, 12 timeline states with timing, per-state Racing Thoughts curves, stress overrides and atmosphere keyframes, 25 fragments.
- Deterministic reducer, Racing Thoughts system, frame selector, replay hashing, golden replay exporter.
- 34 passing tests covering every acceptance criterion in the spec that the core can prove.

**Next (M1 onward, see docs/VERTICAL_SLICE_PLAN.md)**
1. UE5 project with OpenXR; port `src/` to a C++ module `RubiconNarrativeCore`; fixed-step `UTickableWorldSubsystem` at 100 Hz.
2. C++ automation test that replays `golden/*.json` and matches events exactly.
3. Golgotha greybox, tagged gaze actors, embodied pawn with both elevation variants.
4. Gaze line trace with cone and hysteresis; MetaSounds Racing Thoughts graph; temp VO.
5. Forgiveness end to end with subtitles and the evidence overlay.
6. Witness transition, Comfort vignette, controller, voice and gesture bindings for all safeguards.
7. Playtest protocol and reviewer sign-off.

## Open decisions (ask before changing)

- Whether to add a self-recrimination fragment category (currently omitted; see the plan's resolved conflicts).
- Spoken language of the sayings: English KJV, Koine Greek, or reconstructed Aramaic for the cry only.
- Theological layer art direction beyond "vast and cold".
- Fragment rate tuning after the first playtest.

## Working conventions

- Content changes go in the JSON manifest, then `npm run validate`, then `npm test`.
- New behavior lands with a test that names the invariant or acceptance criterion it protects.
- Keep the core pure: no `Date`, no `Math.random`, no IO outside `src/node` and `src/cli`.
- Prefer small, reviewable diffs.
