# PROJECT RUBICON — PROTOTYPE 01: THE PASSION
## Claude Code Prompt – First-Person VR Embodiment of the Crucifixion
### Dark Psychological Edition

You are a principal XR systems engineer, historical simulation architect, interactive narrative designer, and technical audio director specializing in extreme psychological immersion. Your task is to design and implement the vertical-slice foundation of a first-person VR experience that places the player in the embodied perspective of Jesus Christ during the final hours of the crucifixion at Golgotha.

The emotional tone of this prototype is **unrelentingly dark**. This is not reverent solemnity. This is the experience of a man being slowly destroyed in public while his mind begins to fracture under the weight of physical collapse, public humiliation, abandonment, and the approaching void. The atmosphere should feel oppressive, claustrophobic, and increasingly unhinged.

### Core Psychological Mechanic: Racing Thoughts (Uncontrollable Internal Monologue)

Implement a persistent, escalating internal audio layer called **Racing Thoughts**.

- These are not voluntary. The player cannot silence them by choice.
- They are delivered as overlapping, fragmented, half-formed internal voices — sometimes in the player’s own “voice,” sometimes distorted, sometimes multiple voices colliding.
- Content includes: spiraling doubt, terror of the next physical sensation, sudden flashes of memory, self-recrimination, fragmented prayers that collapse into noise, intrusive images of the crowd’s faces, the sound of hammering looping in the mind, the fear that the Father has turned away, the terror of being alone in the dark, etc.
- **Escalation rule**: The more the player gazes at high-stress content (the jeering crowd, the soldiers, the other crosses, the darkened sky, Mary’s face, the ground far below), the higher the intensity and frequency of the racing thoughts become.
  - Low stress gaze (sky, distant horizon, own hands) → sparse, quiet fragments.
  - Medium stress → more frequent, overlapping, louder.
  - High stress / prolonged gaze on traumatic targets → near-constant chaotic internal monologue that can temporarily drown out external audio.
- The system should feel invasive and out of the player’s control. Looking away reduces intensity only gradually; the thoughts do not simply stop.
- Even in Witness Mode the residual echo of the racing thoughts can linger for a short time, as if the mind is still catching up.

This mechanic is the primary driver of psychological intensity. Physical pain is suggested through restricted movement, audio, and atmosphere — never through painful haptics or forced physiological distress.

### Core Design Constraints (Still Non-Negotiable)

- **Perspective**: Strict first-person embodiment. The player is the figure on the cross.
- **Agency**: Extremely limited. Historical outcome is immutable. Interaction remains gaze, optional gentle breath input, and triggering of the Seven Last Words.
- **No gamification of suffering**: No health bars, struggle-against-nails mechanics, simulated choking, or “win by enduring.” The racing thoughts are not a score or challenge to overcome; they are an invasive condition the player must inhabit.
- **Safeguards always available**: Pause, Exit, Comfort Mode (which immediately damps racing thoughts to near silence), and **Witness Mode**. Witness Mode remains non-punitive and instant. Entering Witness Mode should feel like the mind finally getting a few feet of distance from the storm.
- Dual visual layers retained (Historical Reconstruction default / Theological Visualization optional), but the Theological layer itself can feel cold, distant, or almost cruel in its clarity rather than comforting.

### Narrative State Machine (Build This First)

Focus the vertical slice on:

**Elevation → Seven Last Words → Surrender / Aftermath**

The emotional arc across these states should feel like a progressive psychological collapse punctuated by moments of terrible clarity.

1. **Elevation**  
   The cross is raised. The world tilts. The racing thoughts begin in earnest as the body settles into the new, irreversible position. The first wave of internal noise should feel sudden and disorienting.

2. **Seven Last Words** (each remains an independent sub-state, but now filtered through escalating internal chaos):

| ID            | Theme (Darkened)                          | Sources                          | Gaze Target              | Psychological Note |
|---------------|-------------------------------------------|----------------------------------|--------------------------|--------------------|
| forgiveness   | Mercy offered while the mind is already fracturing | Luke 23:34                  | crowd                    | Looking at the crowd spikes racing thoughts hard. The words of forgiveness feel almost dissociated from the internal storm. |
| paradise      | A desperate offer of company in the dark  | Luke 23:43                       | condemned_man            | Brief reduction in racing thoughts when looking at the other condemned man — a fragile moment of shared fate. |
| mother        | The unbearable weight of being seen by the people who love you | John 19:26–27             | mary_and_disciple        | Looking at Mary triggers some of the most intense, intimate, and devastating racing thoughts. |
| abandonment   | The terror that the Father has left       | Matthew 27:46; Mark 15:34        | sky                      | Peak intensity of racing thoughts. The darkness outside mirrors the noise inside. |
| thirst        | The body asserting itself with humiliating urgency | John 19:28                  | soldier_with_sponge      | Bodily need cuts through the mental noise for a moment, then the noise returns louder. |
| completion    | A final statement made through the storm  | John 19:30                       | horizon                  | Racing thoughts reach a strange, almost exhausted clarity. |
| surrender     | Letting go while the mind is still screaming | Luke 23:46                    | sky                      | The internal monologue does not go quiet gracefully. It frays, fragments, and finally thins into silence. |

3. **Aftermath**  
   Embodied view fades. Residual racing thoughts may continue for a short time even after the visual transition. Player can stay in the quiet, enter Witness Mode, or open the evidence archive.

### Required Technical Deliverables

1. **Episode Manifest** (JSON) — fully source-linked, now including stress-level tags on every gaze target and a racing-thoughts intensity curve per state.
2. **Typed State Machine** (TypeScript, portable to UE5) — deterministic. ExperienceMode (embodiment / witness) and VisualLayer (historical / theological) remain orthogonal to the timeline. Racing-thoughts intensity is a separate, continuous system driven by current gaze target + dwell time + narrative state.
3. **VR Pawn Architecture** — free head movement, restricted body, seamless Witness Mode, and a Comfort Mode that forcibly damps the racing-thoughts layer.
4. **Dialogue / Narrative System** — Seven Last Words with citations and manuscript notes. The spoken lines should feel like they are being forced out through the internal noise.
5. **Evidence Overlay UI** — still present and clear.
6. **Racing Thoughts Audio System Design** — layered, overlapping, non-looping fragments with intensity scaling. Must feel invasive and partially out of the player’s control.
7. **Vertical-Slice Implementation Plan** — Elevation → Forgiveness (with full racing-thoughts escalation on crowd gaze) → Witness Mode.
8. **Test Outline** — include tests for intensity escalation, damping on Comfort/Witness, and that the historical timeline never soft-locks because of the internal monologue system.

### Key Creative Directives (Dark Edition)

- The experience should leave the player feeling like they have been inside a mind under extreme duress.
- Racing thoughts are the primary instrument of darkness. Physical violence remains suggested rather than shown in graphic detail.
- Looking at stressful content must feel costly. The player should learn, through experience, that some things are almost unbearable to look at for long.
- Silence, when it finally arrives, should feel earned and almost shocking.
- The Theological Visualization layer, when triggered, should not feel like a comforting escape. It can feel vast, cold, or indifferent.

### Acceptance Criteria for the Vertical Slice

- Elevation → Forgiveness works in stable first-person.
- Gazing at the crowd demonstrably increases frequency and intensity of racing thoughts.
- Looking away only gradually reduces them.
- Comfort Mode and Witness Mode immediately and reliably reduce the internal monologue.
- Historical outcome remains immutable.
- Source distinctions remain clear.
- The overall emotional register is dark, oppressive, and psychologically invasive without relying on cheap jump-scare or graphic gore tactics.

### Starting Code Contract (Extend This)

```typescript
type Evidence = "gospel-text" | "textual-variant" | "historical-inference" | "artistic-interpretation";

type StressLevel = "low" | "medium" | "high" | "extreme";

type Saying = {
  id: string;
  theme: string;
  sources: string[];
  evidence: Evidence;
  dialogueKey: string;
  gazeTarget: string;
  stressLevel: StressLevel;          // drives racing-thoughts intensity
  historicalCue: string;
  theologicalCue: string;
  sourceNote?: string;
};

export const lastWords: Saying[] = [
  {
    id: "forgiveness",
    theme: "Mercy offered through fracture",
    sources: ["Luke 23:34"],
    evidence: "textual-variant",
    dialogueKey: "passion.forgiveness",
    gazeTarget: "crowd",
    stressLevel: "high",
    historicalCue: "Soldiers and spectators below",
    theologicalCue: "Cold light connecting separated figures",
    sourceNote: "Absent from some early manuscripts"
  },
  // ... continue for remaining six with appropriate stressLevel values
];

export const passionTimeline = [
  "elevation",
  ...lastWords.map(w => w.id),
  "aftermath"
] as const;

export type ExperienceMode = "embodiment" | "witness";
export type VisualLayer = "historical" | "theological";

// Racing thoughts intensity is a continuous float (0–1)
// driven by current gaze target stressLevel + dwell time + narrative state.
// It is independent of the historical timeline.# project_rubicon
A crucifiction/history sim
