/**
 * Frame selector: the single read-only view the engine adapter consumes each tick.
 * Rendering, audio and UI read this and nothing else, so the adapter stays thin and replaceable.
 */
import { dwellTierId, effectiveStress, externalDuckDb, maxActiveVoices, outputIntensity } from "./racingThoughts.js";
import { stateDef } from "./session.js";
import type {
  AtmosphereKey,
  CueKind,
  Gospel,
  Rendering,
  ViewMode,
  SourceType,
  EpisodeManifest,
  Evidence,
  ExperienceMode,
  Locale,
  Phase,
  SayingId,
  SessionState,
  StressLevel,
  TimelineStateId,
  VisualLayer,
  WitnessVantage,
} from "./types.js";

export interface EvidenceItem {
  category: Evidence;
  citation: string;
  label: string;
  sourceType?: SourceType;
  note?: string;
  manuscriptNotes?: string[];
  debate?: string;
}

/** The dialogue cue currently sounding. */
export interface CueFrame {
  id: string;
  kind: CueKind;
  speaker: string;
  speakerLabel: string; // cast name, or "Narrator"
  subtitle: string;
  citations: string[];
  evidence: Evidence;
  /** How the words were rendered, when every cited source shares one (e.g. "paraphrase" is never shown in quotes). */
  rendering: Rendering | "kjv" | "mixed";
  telling: Gospel | null;
  origin: string | null; // gaze target to spatialize from; null for the narrator
  elapsedMs: number;
  durationMs: number;
  cueId: string | null;
}

export interface FrameOutputs {
  t: number;
  state: TimelineStateId;
  phase: Phase;
  phaseProgress: number; // [0,1] for timed phases, 0 for open phases
  mode: ExperienceMode;
  vantage: WitnessVantage;
  layer: VisualLayer;
  comfort: boolean;
  paused: boolean;
  exited: boolean;
  embodimentAvailable: boolean;
  /** Where the camera rig should be: the body, the chosen Witness vantage, or (unconscious) a telling's vantage. */
  view: ViewMode;
  viewVantage: WitnessVantage;
  consciousness: { conscious: boolean; exertion: number }; // exertion is for debugging only: never shown as a meter
  controls: { pause: boolean; exit: boolean; witness: boolean; comfort: boolean }; // always true while not exited
  visual: {
    elevationTransition: "slow-tilt" | "fade-settle";
    atmosphere: AtmosphereKey | null;
    theological: boolean;
  };
  audio: {
    racingThoughts: {
      intensity: number; // internal state
      output: number; // what the mixer applies
      maxVoices: number;
      activeVoices: { fragmentId: string; elapsedMs: number }[];
    };
    externalDuckDb: number;
    dialogueActive: boolean;
    breathFocus: number; // [0,1], audio focus only
  };
  gaze: { target: string | null; dwellMs: number; stress: StressLevel; tier: string };
  cue: CueFrame | null;
  saying: null | {
    id: SayingId;
    subtitle: string | null; // shown during utterance and resonance
    citations: string[];
    evidence: Evidence;
    sourceNote: string | null;
    gazeTarget: string;
    attended: boolean;
  };
  evidenceOverlay: EvidenceItem[];
  evidenceOpen: string | null;
}

const PHASE_DURATION_KEY: Partial<Record<Phase, "raisingMs" | "settledMs" | "approachAutoMs" | "utteranceMs" | "resonanceMs" | "runningMs">> = {
  raising: "raisingMs",
  settled: "settledMs",
  approach: "approachAutoMs",
  utterance: "utteranceMs",
  resonance: "resonanceMs",
  running: "runningMs",
};

export function evidenceForState(m: EpisodeManifest, s: SessionState, locale?: Locale): EvidenceItem[] {
  const def = stateDef(m, s);
  const seen = new Set<string>();
  const items: EvidenceItem[] = [];
  const pushSource = (id: string) => {
    if (seen.has(id)) return;
    seen.add(id);
    const src = m.sources.find((x) => x.id === id);
    if (!src) return;
    items.push({
      category: src.evidence,
      citation: src.citation,
      label: locale?.[`ui.evidence.${src.evidence}`] ?? src.evidence,
      sourceType: src.sourceType,
      ...(src.note ? { note: src.note } : {}),
      ...(src.manuscriptNotes ? { manuscriptNotes: src.manuscriptNotes } : {}),
      ...(src.debate ? { debate: src.debate } : {}),
    });
  };
  def.saying?.sourceIds.forEach(pushSource);
  def.sourceIds.forEach(pushSource);
  /* the cue sounding now: its passages and accounts, with their source type and any authenticity debate */
  const cue = activeCue(m, s);
  cue?.sourceIds.forEach(pushSource);

  /* active internal voices: always disclosed as artistic interpretation, with the passage they echo */
  for (const v of s.rt.voices) {
    const f = m.fragments.find((x) => x.id === v.fragmentId);
    if (!f) continue;
    const anchors = f.anchors.map((a) => m.sources.find((x) => x.id === a)?.citation).filter(Boolean) as string[];
    items.push({
      category: "artistic-interpretation",
      citation: anchors.length ? `internal voice, echoing ${anchors.join("; ")}` : "internal voice",
      label: locale?.["ui.evidence.artistic-interpretation"] ?? "artistic-interpretation",
      note: m.racingThoughts.disclosure,
    });
  }
  if (def.atmosphere) {
    items.push({
      category: def.atmosphere.evidence,
      citation: "lighting and atmosphere",
      label: locale?.[`ui.evidence.${def.atmosphere.evidence}`] ?? def.atmosphere.evidence,
      ...(def.atmosphere.note ? { note: def.atmosphere.note } : {}),
    });
  }
  return items;
}

export function activeCue(m: EpisodeManifest, s: SessionState) {
  const a = s.cues.active;
  if (!a || a.endsAt <= s.t) return undefined;
  return m.cues.find((c) => c.id === a.id);
}

function cueRendering(m: EpisodeManifest, sourceIds: string[]): CueFrame["rendering"] {
  const kinds = new Set(
    sourceIds.map((id) => {
      const src = m.sources.find((x) => x.id === id);
      return src?.excerpt ? src.excerpt.rendering : src?.kjv ? "kjv" : "description";
    }),
  );
  return kinds.size === 1 ? ([...kinds][0] as CueFrame["rendering"]) : "mixed";
}

export function cueFrame(m: EpisodeManifest, s: SessionState, locale?: Locale): CueFrame | null {
  const c = activeCue(m, s);
  const a = s.cues.active;
  if (!c || !a) return null;
  const speakerLabel = c.telling
    ? locale?.[`ui.telling.${c.telling}`] ?? c.telling
    : c.speaker === "narrator"
      ? locale?.["ui.narrator"] ?? "Narrator"
      : m.cast.find((x) => x.id === c.speaker)?.name ?? c.speaker;
  return {
    id: c.id,
    kind: c.kind,
    speaker: c.speaker,
    speakerLabel,
    subtitle: locale?.[c.textKey] ?? c.textKey,
    citations: c.sourceIds.map((id) => m.sources.find((x) => x.id === id)?.citation ?? id),
    evidence: c.evidence,
    rendering: cueRendering(m, c.sourceIds),
    origin: c.origin ?? (c.trigger.type === "gaze" && c.kind !== "narration" ? c.trigger.target : null),
    elapsedMs: s.t - a.startedAt,
    durationMs: c.durationMs,
    cueId: c.cueId ?? null,
    telling: c.telling ?? null,
  };
}

/** Unconscious: the vantage of the telling being read, else of the last telling heard, else beside the cross. */
export function viewOf(m: EpisodeManifest, s: SessionState): { view: ViewMode; viewVantage: WitnessVantage } {
  if (s.mode === "witness") return { view: "witness", viewVantage: s.vantage };
  if (s.consciousness.swoon === null) return { view: "embodiment", viewVantage: s.vantage };
  for (let i = s.cues.played.length - 1; i >= 0; i--) {
    const c = m.cues.find((x) => x.id === s.cues.played[i]);
    if (c?.kind === "telling" && c.vantage) return { view: "telling", viewVantage: c.vantage };
  }
  return { view: "telling", viewVantage: "beloved_disciple" };
}

export function selectFrame(m: EpisodeManifest, s: SessionState, locale?: Locale): FrameOutputs {
  const def = stateDef(m, s);
  const key = PHASE_DURATION_KEY[s.timeline.phase];
  const dur = key ? def.timing[key] : undefined;
  const output = outputIntensity(m, def, s);
  const saying = def.saying;
  const showSubtitle = s.timeline.phase === "utterance" || s.timeline.phase === "resonance";
  const live = !s.exited;

  return {
    t: s.t,
    state: def.id,
    phase: s.timeline.phase,
    phaseProgress: dur ? Math.min(1, s.timeline.phaseElapsedMs / dur) : 0,
    mode: s.mode,
    vantage: s.vantage,
    layer: s.layer,
    comfort: s.comfort,
    paused: s.paused,
    exited: s.exited,
    embodimentAvailable: def.allowEmbodiment,
    ...viewOf(m, s),
    consciousness: { conscious: s.consciousness.swoon === null, exertion: s.consciousness.exertion },
    controls: { pause: live, exit: live, witness: live, comfort: live },
    visual: {
      elevationTransition: s.comfort || s.settings.reducedEffects ? "fade-settle" : "slow-tilt",
      atmosphere: def.atmosphere ?? null,
      theological: s.layer === "theological",
    },
    audio: {
      racingThoughts: {
        intensity: s.rt.intensity,
        output,
        maxVoices: maxActiveVoices(m, output),
        activeVoices: s.rt.voices.map((v) => ({ fragmentId: v.fragmentId, elapsedMs: s.t - v.startedAt })),
      },
      externalDuckDb: externalDuckDb(m, output),
      dialogueActive: s.timeline.phase === "utterance",
      breathFocus: s.breath,
    },
    gaze: { target: s.gaze.target, dwellMs: s.gaze.dwellMs, stress: effectiveStress(m, def, s.gaze.target), tier: dwellTierId(m, s.gaze.dwellMs) },
    cue: cueFrame(m, s, locale),
    saying: saying
      ? {
          id: saying.id,
          subtitle: showSubtitle ? locale?.[saying.dialogueKey] ?? saying.dialogueKey : null,
          citations: saying.sources,
          evidence: saying.evidence,
          sourceNote: saying.sourceNote ?? null,
          gazeTarget: saying.gazeTarget,
          attended: s.timeline.attendedTargetMs > 0,
        }
      : null,
    evidenceOverlay: evidenceForState(m, s, locale),
    evidenceOpen: s.evidenceOpen,
  };
}
