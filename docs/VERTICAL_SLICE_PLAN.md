# Vertical Slice Plan: Elevation → Forgiveness → Witness Mode

Project Rubicon, Prototype 01: The Passion (Dark Psychological Edition)

## Goal

A player in a headset consents at the Threshold, is raised on the cross, endures Forgiveness with the Racing Thoughts storm escalating under gaze, hears the saying forced through it, and steps into Witness Mode. Every safeguard works at every moment, every word is attributed, and the run is replayable bit-for-bit.

## Architecture

```
content/passion.episode.json ─┐
content/locale.en.json ───────┼─> validate ─> EpisodeManifest (immutable)
                              │
 engine input (gaze, breath, commands) ──> step(manifest, state, input) ──> state', events
                                                  │ fixed 10 ms tick, pure, seeded
                                                  ▼
                                   selectFrame(manifest, state) ──> FrameOutputs
                                                  │
              ┌───────────────────┬───────────────┼────────────────┬──────────────────┐
              ▼                   ▼               ▼                ▼                  ▼
        VR pawn / camera    atmosphere      Racing Thoughts     dialogue +        evidence
        (mode, vantage,     (lux, CCT,      mixer (output,      subtitles         overlay UI
         elevation style)    AOD)            voices, duck dB)
```

Three orthogonal axes ride on one immutable timeline: ExperienceMode (embodiment, witness), VisualLayer (historical, theological), Comfort. Racing Thoughts is a fourth continuous system that reads the timeline and never writes it.

## Milestones

**M0. Narrative core (done).** Types, manifest, validator, reducer, Racing Thoughts model, frame selector, replay hashing, 34 passing tests, headless simulator.

**M1. Engine skeleton.** UE5.4+ project, OpenXR, `RubiconNarrativeCore` C++ module ported from `src/`. A `UTickableWorldSubsystem` runs `Step()` with a fixed-step accumulator at 100 Hz. Manifest imported to a `UDataAsset` by an editor script, validated with the same rules. Debug HUD prints `FrameOutputs`.
Exit: golden replays (seed + input log exported from TS) produce identical `timeline.enter` sequences and Racing Thoughts output within 1e-6.
Status: core parity met and exceeded outside the engine. Every event, the per-tick trace (error 0) and the final state hash are identical (`npm run parity`). UE modules are written; the in-editor automation run is pending a UE 5.4 build.

**M2. Golgotha greybox and the embodied pawn.** Quarry-edge site, low cross, crowd volumes, horizon, sky. Every gaze target is an actor tagged `Gaze.Target.<id>`. Pawn: free head rotation, positional head motion clamped to a small radius, body and hands posed by fixed IK. Elevation in two variants: slow world tilt (default) and fade-settle (Comfort or Reduced Effects); the player's camera is never rotated by code.
Exit: seated and standing both stable; no forced camera motion in Comfort.
Status: code complete (`unreal/Rubicon/Source/Rubicon`); headset checklist in `unreal/README.md` pending.

**M3. Gaze and Racing Thoughts audio.** HMD-forward line trace with a 3 to 5 degree cone and 150 ms hysteresis feeds `gaze`. MetaSounds graph driven by `output`, `maxVoices`, `externalDuckDb`; fragments trigger from `rt.fragment` events. Temp VO for all fragments in two or three takes each. Microphone voice activity (an envelope follower on the capture bus) feeds `SetVoiceActivity`; temp VO for the 30 tellings and fades on `OnConsciousnessChange`.
Exit: in playtest, crowd gaze is audibly escalating within 5 s, and looking away takes about 10 s to settle.

**M4. Forgiveness.** Saying trigger by controller select, speech keyword, or the 45 s auto clock. Utterance through the dialogue bus with the partial duck, subtitles, then 12 s of resonance. Evidence overlay as a wrist panel and a world-locked plate showing Luke 23:34 with its manuscript notes.
Exit: a player can tell gospel text, variant, inference and interpretation apart without help.

**M5. Witness Mode and safeguards.** One button, one voice word and one gesture for each of Witness, Comfort, Pause, Exit. Witness: 300 ms fade, spawn at the chosen vantage about 1 m from the cross, the internal echo decays over about 2.5 s. Return to embodiment restores the timeline exactly.
Exit: all controls respond within one tick from every state (already proven in the core; verify end to end).

**M6. Playtest.** 8 to 12 participants. Record intensity traces, witness and comfort usage, exits, the Simulator Sickness Questionnaire, and a short debrief. Theology and history reviewers sign off on the overlay content.

## Dialogue and history

The world speaks only in recorded words. Spoken cues are verbatim KJV, voiced at their source in the scene and attributed to the specific group that said them. Some answer a held gaze; others follow the sayings in Gospel order (the condemned men's exchange before Paradise, Elias after the cry, the sponge after thirst, the centurion after death). Dwell tiers make looking costly in a second dimension: a glance gets a line, and a long stare gets harder ones (glance, sustained, long, extreme). Lesser-known history (Quintilian on crowded roads, Cicero on the cross, Josephus on Varus and on burial before sunset, Philo on Pilate, Thallus and the darkness, the Pilate Stone, the Yehohanan heel bone, the Alexamenos graffito) is narrated only from outside, in Witness and Contemplation. Each account names its source, rendering and any authenticity debate. See docs/SOURCES.md.

## Exertion, loss of consciousness and the tellings

Looking is always free: the player may look anywhere at any time, and gaze direction never builds exertion. What costs is moving much (sustained head rotation or translation above a dead zone) and talking much (voice activity above a dead zone). Exertion rises with a per-state fatigue factor (1.0 at Elevation to 1.7 at Surrender) and falls with stillness. At full exertion he loses consciousness: the storm lets go like the Witness echo, the view fades over 1.2 s, and the player hears the current moment as one Gospel tells it, verbatim KJV, from where that Gospel's witnesses stood (John's beloved disciple beside the cross; the women beholding afar off for Matthew, Mark and Luke). When the tellings for the moment are done and he has rested at least 15 s, he wakes on the cross. A saying that comes due wakes him to speak it. The timeline never waits for him (I8). Exertion is never displayed and never scored; `settings.swoon` turns the mechanic off. The device is disclosed as artistic interpretation: the Gospels do not say he lost consciousness.

The wood is the one exception to "everything costs". Gazing at the cross itself (the crossbeam beside the arms, the upright above the head) is the only relief target in the episode; the penitent thief is gentle (low) but not relief. The cross carries the Dream of the Rood as a disclosed tradition source (artistic interpretation), heard only from outside.

## Racing Thoughts audio direction

Voices are head-locked and deliberately non-externalized: near-zero interaural difference, close-mic dry signal, so they sit inside the skull while the world stays spatialized outside it. One actor in several registers (whisper, flat, cracked, layered) plus distorted duplicates for crowd echoes. No looping beds: everything is a discrete fragment, never repeating within the recent window. External ambience ducks by up to 18 dB above output 0.6 so the storm can drown the world. During an utterance the storm is ducked only partially, so the line is heard through it. After Surrender the ceiling falls to zero and the ambience drops with it: the silence is a designed event, and it should land like one.

## Lighting direction

The darkness from the sixth to the ninth hour is rendered as rising aerosol optical depth (Mie extinction and forward scattering) with falling illuminance and color temperature, keyed per state in the manifest. Never a fade to black, never an eclipse disc: the Gospels report the darkness without a mechanism, and an eclipse is impossible at the Passover full moon. The theological layer swaps the reconstruction for procedural light structures and, per the Dark Edition, reads vast and cold.

## Resolved design conflicts

1. **Post-death residual voices.** The Dark Edition lets voices linger into the aftermath; the core spec forbids invented post-death sensation. Resolution: Release forces Witness, so any echo is the player's own mind catching up, heard from outside the body.
2. **Invented interior life.** The core spec forbids presenting his private thoughts as established. Resolution: every fragment is tagged artistic interpretation, the overlay discloses it live, and fragments echo scripture where possible (Psalm 22, Mark 14:36, the taunts of Mark 15).
3. **Self-recrimination.** Not seeded. For a figure most traditions hold sinless it reads as heterodox. The "accusation" category carries that weight instead: the crowd's "If thou be the Son of God, come down" (Matthew 27:40) echoes the tempter's formula from Matthew 4:3. The type system allows adding it if you choose to.
4. **The ground far below.** Roman crosses were likely low (the drink was offered on a reed). The fragment is dread of closeness and exposure.
5. **Sky stress.** Low by default, extreme during Abandonment, medium during Surrender, through per-state overrides.
6. **Threshold gate.** A consent screen with an intensity ceiling is the only state that waits on the player. Standard for intrusive-audio VR; it protects the people this experience is most likely to hit hardest.

## UE5 port

- `src/types.ts` → `RubiconTypes.h`: each union becomes a `UENUM(BlueprintType)`, each interface a `USTRUCT(BlueprintType)`.
- `src/session.ts`, `src/racingThoughts.ts` → pure C++ functions over const structs; no `UObject` state in the core.
- `src/prng.ts` → `uint32` mulberry32, identical bit-for-bit.
- `src/frame.ts` → `FFrameOutputs` exposed to Blueprint for audio, UI and lighting.
- Events → a multicast delegate per event type.
- Replays: TS writes `golden/*.json` (seed, inputs, expected events, per-tick RT trace); `native/` and the C++ automation test replay them and compare events, trace and hashes.
- Numerics: `Math.exp` and `Math.pow` are V8's fdlibm ports, reproduced bit for bit in `RubiconMath.cpp` (V8's `pow` differs from stock fdlibm in one expression, reproduced). Host libm results differ from V8 on about 8% of inputs, so the core never calls them. FMA contraction is disabled per file, and a startup self-test detects builds that override that.
