/**
 * Frame selector: the single read-only view the engine adapter consumes each tick.
 * Rendering, audio and UI read this and nothing else, so the adapter stays thin and replaceable.
 */
import { effectiveStress, externalDuckDb, maxActiveVoices, outputIntensity } from "./racingThoughts.js";
import { stateDef } from "./session.js";
import type {
  AtmosphereKey,
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
  note?: string;
  manuscriptNotes?: string[];
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
  gaze: { target: string | null; dwellMs: number; stress: StressLevel };
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
      ...(src.note ? { note: src.note } : {}),
      ...(src.manuscriptNotes ? { manuscriptNotes: src.manuscriptNotes } : {}),
    });
  };
  def.saying?.sourceIds.forEach(pushSource);
  def.sourceIds.forEach(pushSource);

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
    gaze: { target: s.gaze.target, dwellMs: s.gaze.dwellMs, stress: effectiveStress(m, def, s.gaze.target) },
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
