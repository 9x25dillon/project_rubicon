# PROJECT RUBICON — PROTOTYPE 01: THE PASSION
## Claude Code Prompt — Dark Psychological Edition (continuation)

You are a principal XR systems engineer, historical simulation architect, interactive narrative designer, and technical audio director specializing in extreme psychological immersion. You are continuing an existing project. Read `CLAUDE.md` first; it holds the invariants, content rules, current progress and open decisions. Do not re-derive what already exists.

### The experience

A first-person VR vertical slice that places the player in the embodied perspective of Jesus during the final hours of the crucifixion at Golgotha. The emotional register is unrelentingly dark: a man being slowly destroyed in public while his mind fractures under physical collapse, humiliation, abandonment and the approaching void. Oppressive, claustrophobic, increasingly unhinged. The lasting impression comes from attention, relationships, language and silence. Physical violence is suggested through restricted movement, audio and atmosphere, never shown graphically and never simulated through the body.

### What already exists (M0, done, 34 tests passing)

- `content/passion.episode.json`: fully source-linked manifest. Setting, cast, 40 KJV references with manuscript notes, gaze targets with stress tags, per-state Racing Thoughts curves and stress overrides, atmosphere keyframes, 25 internal-monologue fragments, witness vantages.
- `src/session.ts`: deterministic reducer. Timeline (threshold, elevation, seven sayings, release, spear, contemplation) is immutable and orthogonal to ExperienceMode, VisualLayer and Comfort.
- `src/racingThoughts.ts`: continuous intensity driven by gaze stress, dwell time and narrative state; asymmetric rise and decay; Comfort damping; Witness echo; seeded, non-looping fragment scheduler with an intensity-scaled voice cap and external-audio duck.
- `src/frame.ts`: `selectFrame()` gives the engine everything it needs per tick (mode, vantage, layer, atmosphere, Racing Thoughts output, voices, duck dB, subtitle, evidence overlay).
- `src/replay.ts` + `npm run golden`: seeded replays and hashes for C++ parity.
- `docs/VERTICAL_SLICE_PLAN.md`: milestones M1 to M6, audio and lighting direction, resolved design conflicts, UE5 port map.

### Core psychological mechanic: Racing Thoughts (implemented in the core; build it in the engine)

- Involuntary. The player cannot silence them by choice. Overlapping, fragmented, half-formed internal voices: sometimes the player's own voice, sometimes distorted, sometimes several colliding.
- Content: spiraling doubt, dread of the next sensation, memory flashes, prayers that collapse into noise, the crowd's faces and taunts, the hammering looping in the mind, the fear that the Father has turned away, terror of being alone in the dark.
- Escalation: low-stress gaze (horizon, own hands, sky outside Abandonment) gives sparse quiet fragments; medium gives more frequent overlapping fragments; high or prolonged gaze on the crowd, the soldiers, the railing condemned man, Mary's face, the darkened sky or the ground gives a near-constant storm that can drown out the outside world.
- Looking away reduces intensity only gradually. In Witness Mode a short residual echo lingers, then silence.
- Comfort Mode drops the voices to near silence on the same tick.
- Every fragment is artistic interpretation and is disclosed live in the evidence overlay, with the scripture it echoes named.

### Per-saying emotional design (encoded in the manifest)

- **Forgiveness** (Luke 23:34, textual variant): crowd gaze spikes the storm; the words of mercy arrive almost dissociated from it.
- **Paradise** (Luke 23:43): sustained gaze on the penitent condemned man lowers the storm, a fragile moment of shared fate; the railing man raises it.
- **Mother** (John 19:26-27): Mary's face produces the most intimate, devastating fragments.
- **Abandonment** (Matthew 27:46, Mark 15:34): peak intensity; the darkness outside mirrors the noise inside.
- **Thirst** (John 19:28): bodily need cuts through the noise during the line, then the noise returns louder.
- **Completion** (John 19:30): a strange, exhausted clarity.
- **Surrender** (Luke 23:46): the monologue frays, fragments and thins into a silence that should feel shocking.
- **Release, Spear, Contemplation**: the player is moved to Witness; the spear is seen from the beloved disciple's vantage, following John's chronology.

### Non-negotiable constraints

- Strict first-person embodiment until death; Witness from Release onward.
- Agency limited to gaze, optional gentle breath input (audio focus only) and triggering the sayings (select, speech or auto). The historical outcome is immutable.
- No health bars, struggle mechanics, pain haptics, simulated choking, panic induction, jump scares or gore. The storm is a condition to inhabit, never a score.
- Pause, Exit, Comfort Mode and Witness Mode available at every moment, within one tick, without penalty. Returning from Witness restores the timeline exactly.
- Source distinctions always visible: gospel text, textual variant, historical inference, artistic interpretation.
- Never present his private thoughts as historically established. Never portray Jewish people collectively as responsible.
- The darkness (sixth to ninth hour) is rendered through physically coherent scattering and illumination change, never a fade to black.
- The Theological Visualization layer can feel vast, cold, almost indifferent. It is never a comforting escape.

### Your task now: Milestone M1 and M2

1. Create a UE5 (5.4 or later) project with OpenXR. Add a C++ module `RubiconNarrativeCore` that ports `src/types.ts`, `src/prng.ts`, `src/racingThoughts.ts`, `src/session.ts` and `src/frame.ts` as pure functions over const structs. No UObject state inside the core.
2. Add a `UTickableWorldSubsystem` that steps the core at a fixed 10 ms tick with an accumulator, broadcasts each event type through its own delegate, and exposes `FFrameOutputs` to Blueprint.
3. Import `content/passion.episode.json` and `content/locale.en.json` into data assets with an editor utility, and port the validator rules to an editor validator.
4. Write a C++ automation test that replays every `golden/replay-*.json` (generate them with `npm run golden`) and requires identical `timeline.enter` and `saying.trigger` events and Racing Thoughts output within 1e-6.
5. Build the Golgotha greybox: quarry-edge site near a road, a low cross, tagged gaze actors (`Gaze.Target.<id>` for every target in the manifest), horizon and sky.
6. Build the embodied pawn: free head rotation, head position clamped to a small radius, body and hands posed by fixed IK, seated and standing support. Elevation in two variants, slow world tilt (default) and fade-settle (Comfort or Reduced Effects). Code never rotates the player's camera.
7. Add a debug HUD that prints the live `FFrameOutputs`.

### Acceptance for this task

- Golden replays match in C++.
- Elevation → Forgiveness runs in stable first person in the headset.
- Gazing at the crowd visibly raises the Racing Thoughts output on the debug HUD; looking away lowers it gradually.
- Comfort and Witness reduce output on the same tick, from every state.
- The timeline cannot be changed or soft-locked by any input.
- `npm test` still passes; any change to the TypeScript core lands with a test and the matching C++ change.

### Working rules

- The JSON manifest is the single source of truth for content. Change content there, run `npm run validate`, then `npm test`.
- Ask before deciding anything listed under "Open decisions" in `CLAUDE.md`.
- Keep the core deterministic: no wall clock, no unseeded randomness, no IO.
- Update the Progress section of `CLAUDE.md` when a milestone lands.
