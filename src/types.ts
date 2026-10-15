/**
 * Project Rubicon, Prototype 01: The Passion
 * Core type contract. Engine-agnostic; mirrors 1:1 into UE5 USTRUCT/UENUM (see docs/VERTICAL_SLICE_PLAN.md, "UE5 port").
 *
 * Layering:
 *   content (JSON manifest)  ->  validate  ->  pure reducer step(manifest, state, input)
 *   -> { state, events }  ->  selectFrame(manifest, state)  ->  engine adapter (render, audio, UI)
 *
 * Invariants enforced by the reducer (and tested):
 *   I1  Timeline order is immutable: states are entered strictly in TIMELINE order, never skipped, never re-entered.
 *   I2  ExperienceMode, VisualLayer, Comfort, settings and evidence UI never reset, rewind or reorder the timeline.
 *   I3  Exit and Witness Mode are accepted in every state.
 *   I4  The timeline cannot soft-lock: after the consent gate, every state auto-advances on a bounded clock.
 *   I5  Racing Thoughts never gate narrative progress; they read timeline state, never write it.
 *   I6  Determinism: identical (manifest, seed, input stream) => identical state hash and event log.
 */

/* ------------------------------------------------------------------ */
/* Evidence and stress                                                 */
/* ------------------------------------------------------------------ */

export type Evidence =
  | "gospel-text"
  | "textual-variant"
  | "historical-inference"
  | "artistic-interpretation";

export const EVIDENCE_CATEGORIES: readonly Evidence[] = [
  "gospel-text",
  "textual-variant",
  "historical-inference",
  "artistic-interpretation",
] as const;

/** Extends the starting contract with "relief": a target that lowers intensity under sustained gaze. */
export type StressLevel = "relief" | "low" | "medium" | "high" | "extreme";

export const STRESS_LEVELS: readonly StressLevel[] = ["relief", "low", "medium", "high", "extreme"] as const;

/* ------------------------------------------------------------------ */
/* Orthogonal axes                                                     */
/* ------------------------------------------------------------------ */

export type ExperienceMode = "embodiment" | "witness";
export type VisualLayer = "historical" | "theological";
export type WitnessVantage = "centurion" | "beloved_disciple" | "mary";
export const WITNESS_VANTAGES: readonly WitnessVantage[] = ["centurion", "beloved_disciple", "mary"] as const;

/* ------------------------------------------------------------------ */
/* Timeline                                                            */
/* ------------------------------------------------------------------ */

export const SAYING_IDS = [
  "forgiveness",
  "paradise",
  "mother",
  "abandonment",
  "thirst",
  "completion",
  "surrender",
] as const;
export type SayingId = (typeof SAYING_IDS)[number];

/** Canonical, immutable order. Aftermath is expanded into release -> spear -> contemplation. */
export const TIMELINE = [
  "threshold",
  "elevation",
  ...SAYING_IDS,
  "release",
  "spear",
  "contemplation",
] as const;
export type TimelineStateId = (typeof TIMELINE)[number];

/** Kept for compatibility with the starting contract. */
export const passionTimeline = ["elevation", ...SAYING_IDS, "aftermath"] as const;

export type StateKind = "gate" | "elevation" | "saying" | "timed" | "terminal";

export type Phase =
  | "advisory" // gate: waits for informed consent
  | "raising" // elevation
  | "settled" // elevation
  | "approach" // saying: gaze and attention, saying may be triggered after approachMinMs
  | "utterance" // saying: the line is spoken
  | "resonance" // saying: silence after the line
  | "running" // timed
  | "open"; // terminal

export const PHASES_BY_KIND: Readonly<Record<StateKind, readonly Phase[]>> = {
  gate: ["advisory"],
  elevation: ["raising", "settled"],
  saying: ["approach", "utterance", "resonance"],
  timed: ["running"],
  terminal: ["open"],
};

export type TriggerVia = "select" | "speech" | "auto";

/* ------------------------------------------------------------------ */
/* Manifest schema                                                     */
/* ------------------------------------------------------------------ */

export interface ScriptureText {
  translation: "KJV";
  text: string;
}

export interface SourceRef {
  id: string; // e.g. "luke-23-34"
  citation: string; // e.g. "Luke 23:34"
  evidence: Evidence;
  kjv?: ScriptureText;
  greek?: string;
  note?: string;
  manuscriptNotes?: string[];
}

export interface CastMember {
  id: string;
  name: string;
  sources: string[];
  evidence: Evidence;
  note?: string;
}

export interface GazeTargetDef {
  id: string;
  label: string;
  stress: StressLevel; // base stress; per-state overrides live on TimelineStateDef
  actors: string[]; // cast ids represented by this target
  sources: string[];
  evidence: Evidence;
  note?: string;
}

export interface RacingThoughtsStateParams {
  floor: number; // [0,1] resting intensity in this state
  ceiling: number; // [0,1] cap at state start
  ceilingEnd?: number; // [0,1] cap at nominal state end (linear), for thinning to silence
  tauUpMs: number; // rise time constant
  tauDownMs: number; // decay time constant (slow by design: looking away helps only gradually)
  utteranceDuck: number; // [0,1] multiplier on output while the saying is spoken (1 = no duck)
  postUtteranceFloorBoost: number; // added to floor after the utterance (the noise returns louder)
  entryImpulse: number; // [0,1] intensity is raised to at least this on state entry (embodiment only)
}

export interface Timing {
  raisingMs?: number; // elevation
  settledMs?: number; // elevation
  approachMinMs?: number; // saying: earliest manual trigger
  approachAutoMs?: number; // saying: automatic trigger (no soft-lock)
  utteranceMs?: number; // saying
  resonanceMs?: number; // saying: silence
  runningMs?: number; // timed
}

export interface Pronunciation {
  greek?: string;
  greekTranslit?: string;
  aramaicReconstruction?: string;
  aramaicTranslit?: string;
  evidence: Evidence;
  note?: string;
}

/** Matches and extends the starting contract `Saying`. */
export interface Saying {
  id: SayingId;
  theme: string;
  sources: string[]; // human citations, e.g. "Luke 23:34"
  sourceIds: string[]; // keys into manifest.sources
  evidence: Evidence;
  dialogueKey: string;
  gazeTarget: string;
  stressLevel: StressLevel;
  historicalCue: string;
  theologicalCue: string;
  sourceNote?: string;
  psychologicalNote: string;
  pronunciation: Pronunciation;
  attestation: string; // which Gospel(s), and what the harmonization does
}

export interface AtmosphereKey {
  sunIlluminanceLux: number;
  skyCctK: number;
  aerosolOpticalDepth: number;
  evidence: Evidence;
  note?: string;
}

export interface TimelineStateDef {
  id: TimelineStateId;
  kind: StateKind;
  label: string;
  sourceIds: string[];
  evidence: Evidence;
  timing: Timing;
  rt: RacingThoughtsStateParams;
  stressOverrides: Record<string, StressLevel>;
  allowEmbodiment: boolean;
  forcedMode?: ExperienceMode;
  forcedVantage?: WitnessVantage;
  atmosphere?: AtmosphereKey;
  saying?: Saying;
  notes?: string[];
}

export type FragmentKind = "verbal" | "sonic";
export type FragmentCategory =
  | "scriptural-echo"
  | "crowd-echo"
  | "accusation"
  | "memory-flash"
  | "prayer-collapse"
  | "dread"
  | "sensory-intrusion";

export interface Fragment {
  id: string;
  kind: FragmentKind;
  category: FragmentCategory;
  textKey?: string; // verbal
  cueId?: string; // sonic
  evidence: Evidence; // the internalization is always artistic-interpretation
  anchors: string[]; // source ids the fragment echoes, if any
  minIntensity: number; // [0,1] output intensity required before this fragment is eligible
  durationMs: number;
  weight: number;
  states: TimelineStateId[] | "*";
  targetAffinity: string[]; // gaze targets that raise selection weight
}

export interface RacingThoughtsModel {
  stressWeights: Record<StressLevel, number>; // relief < 0
  noTargetStress: StressLevel;
  dwellTauMs: number;
  witnessEchoScale: number;
  witnessEchoTauMs: number;
  comfortGain: number;
  comfortReleaseTauMs: number;
  affinityMultiplier: number;
  fragmentRate: { minPerSec: number; maxPerSec: number; gamma: number };
  maxVoices: number;
  recentWindow: number;
  externalDuck: { startAt: number; maxDb: number };
  evidence: Evidence;
  disclosure: string;
}

export interface EpisodeManifest {
  schemaVersion: string;
  id: string;
  title: string;
  edition: string;
  setting: {
    place: string;
    era: string;
    sourceIds: string[];
    reconstructionNotes: { text: string; evidence: Evidence; sourceIds: string[] }[];
  };
  harmonizationNote: string;
  cast: CastMember[];
  sources: SourceRef[];
  gazeTargets: GazeTargetDef[];
  witnessVantages: { id: WitnessVantage; castId: string; sourceIds: string[]; evidence: Evidence }[];
  racingThoughts: RacingThoughtsModel;
  fragments: Fragment[];
  timeline: TimelineStateDef[];
  advisory: { titleKey: string; bodyKey: string };
}

export type Locale = Record<string, string>;

/* ------------------------------------------------------------------ */
/* Runtime state                                                       */
/* ------------------------------------------------------------------ */

export interface Settings {
  intensityCeiling: number; // [0,1] player-chosen hard cap on Racing Thoughts output
  reducedEffects: boolean;
  seated: boolean;
  hapticsEnabled: boolean; // default false; this experience never uses pain haptics in any case
  subtitles: true; // sayings are always captioned
}

export interface Voice {
  fragmentId: string;
  startedAt: number;
  endsAt: number;
}

export interface TimelineRuntime {
  index: number; // into TIMELINE
  phase: Phase;
  phaseElapsedMs: number;
  stateElapsedMs: number;
  triggeredVia: TriggerVia | null;
  attendedTargetMs: number; // cumulative gaze on the saying's target in this state (reveal, never trigger)
}

export interface RacingThoughtsRuntime {
  intensity: number; // internal [0,1]
  gain: number; // comfort gain [0,1]
  floorBoost: number;
  spawnAccumulator: number;
  voices: Voice[];
  recent: string[];
}

export interface SessionState {
  t: number; // simulation ms since start
  seed: number;
  rng: number; // u32 PRNG state
  timeline: TimelineRuntime;
  mode: ExperienceMode;
  vantage: WitnessVantage;
  layer: VisualLayer;
  comfort: boolean;
  paused: boolean;
  exited: boolean;
  evidenceOpen: string | null;
  settings: Settings;
  gaze: { target: string | null; dwellMs: number };
  breath: number; // smoothed [0,1]; audio focus only
  rt: RacingThoughtsRuntime;
}

/* ------------------------------------------------------------------ */
/* Inputs and events                                                   */
/* ------------------------------------------------------------------ */

export type Command =
  | { type: "acknowledgeAdvisory" }
  | { type: "triggerSaying"; via: "select" | "speech" }
  | { type: "setMode"; mode: ExperienceMode; vantage?: WitnessVantage }
  | { type: "setLayer"; layer: VisualLayer }
  | { type: "setComfort"; on: boolean }
  | { type: "pause" }
  | { type: "resume" }
  | { type: "exit" }
  | { type: "openEvidence"; sourceId: string }
  | { type: "closeEvidence" }
  | { type: "setSettings"; settings: Partial<Omit<Settings, "subtitles">> };

export interface TickInput {
  /** Gaze target resolved by the engine (raycast against tagged actors), or null. */
  gaze: string | null;
  /** Optional breath signal [0,1]. Affects audio focus only. */
  breath?: number;
  commands?: Command[];
}

export type SessionEvent =
  | { t: number; type: "timeline.enter"; state: TimelineStateId }
  | { t: number; type: "timeline.phase"; state: TimelineStateId; phase: Phase }
  | { t: number; type: "saying.trigger"; saying: SayingId; via: TriggerVia }
  | { t: number; type: "mode.change"; mode: ExperienceMode; vantage: WitnessVantage; forced: boolean }
  | { t: number; type: "layer.change"; layer: VisualLayer }
  | { t: number; type: "comfort.change"; on: boolean }
  | { t: number; type: "pause.change"; paused: boolean }
  | { t: number; type: "exit" }
  | { t: number; type: "evidence.change"; sourceId: string | null }
  | { t: number; type: "settings.change"; settings: Settings }
  | { t: number; type: "rt.fragment"; fragmentId: string; kind: FragmentKind; voice: number }
  | { t: number; type: "command.rejected"; command: Command["type"]; reason: string };

export interface StepResult {
  state: SessionState;
  events: SessionEvent[];
}
