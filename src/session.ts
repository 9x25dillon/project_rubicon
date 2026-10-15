/**
 * The session reducer. Pure, deterministic, fixed-timestep.
 *
 *   step(manifest, state, input, dtMs = TICK_MS) -> { state, events }
 *
 * Order within a tick:
 *   1. commands, in order, at time t (may change mode, comfort, layer, trigger a saying, acknowledge the gate)
 *   2. if running (not paused, not exited): gaze dwell, breath, exertion/consciousness, timeline clock, dialogue cues,
 *      Racing Thoughts, t += dt
 *
 * The engine owns real time and calls step() at a fixed rate (accumulator pattern), so render frame rate
 * never affects simulation results.
 */
import { AWAKE, isUnconscious, stepConsciousness, wake } from "./consciousness.js";
import { EMPTY_CUES, stepCues, tellingSounding } from "./cues.js";
import { clamp01, seedToState, smoothToward } from "./prng.js";
import { stepRacingThoughts } from "./racingThoughts.js";
import {
  PHASES_BY_KIND,
  TIMELINE,
  type Command,
  type EpisodeManifest,
  type Phase,
  type SessionEvent,
  type SessionState,
  type Settings,
  type StepResult,
  type TickInput,
  type TimelineStateDef,
  type TriggerVia,
  type WitnessVantage,
} from "./types.js";

export const TICK_MS = 10;
const BREATH_TAU_MS = 800;

export const DEFAULT_SETTINGS: Settings = {
  intensityCeiling: 1,
  reducedEffects: false,
  seated: true,
  hapticsEnabled: false,
  swoon: true,
  subtitles: true,
};

export function stateDef(m: EpisodeManifest, s: SessionState): TimelineStateDef {
  const d = m.timeline[s.timeline.index];
  if (!d) throw new Error(`timeline index ${s.timeline.index} out of range`);
  return d;
}

export function createSession(
  m: EpisodeManifest,
  seed: number,
  settings: Partial<Omit<Settings, "subtitles">> = {},
): SessionState {
  return {
    t: 0,
    seed: seed >>> 0,
    rng: seedToState(seed),
    timeline: {
      index: 0,
      phase: PHASES_BY_KIND[m.timeline[0]!.kind][0]!,
      phaseElapsedMs: 0,
      stateElapsedMs: 0,
      triggeredVia: null,
      attendedTargetMs: 0,
    },
    mode: "embodiment",
    vantage: "beloved_disciple",
    layer: "historical",
    comfort: false,
    paused: false,
    exited: false,
    evidenceOpen: null,
    settings: { ...DEFAULT_SETTINGS, ...sanitizeSettings(settings) },
    gaze: { target: null, dwellMs: 0 },
    breath: 0,
    rt: { intensity: 0, gain: 1, floorBoost: 0, spawnAccumulator: 0, voices: [], recent: [] },
    cues: EMPTY_CUES,
    consciousness: AWAKE,
  };
}

function sanitizeSettings(p: Partial<Omit<Settings, "subtitles">>): Partial<Settings> {
  const out: Partial<Settings> = { ...p };
  if (p.intensityCeiling !== undefined) out.intensityCeiling = clamp01(p.intensityCeiling);
  return out;
}

/* ------------------------------------------------------------------ */
/* Transitions                                                         */
/* ------------------------------------------------------------------ */

function setPhase(s: SessionState, phase: Phase, events: SessionEvent[], def: TimelineStateDef): SessionState {
  events.push({ t: s.t, type: "timeline.phase", state: def.id, phase });
  return { ...s, timeline: { ...s.timeline, phase, phaseElapsedMs: 0 } };
}

function applyWitnessEntry(m: EpisodeManifest, s: SessionState): SessionState {
  return { ...s, rt: { ...s.rt, intensity: s.rt.intensity * m.racingThoughts.witnessEchoScale } };
}

function enterState(m: EpisodeManifest, s: SessionState, index: number, events: SessionEvent[]): SessionState {
  const def = m.timeline[index];
  if (!def) return s; // past terminal: unreachable because terminal never advances
  const phase = PHASES_BY_KIND[def.kind][0]!;
  let next: SessionState = {
    ...s,
    timeline: { index, phase, phaseElapsedMs: 0, stateElapsedMs: 0, triggeredVia: null, attendedTargetMs: 0 },
    rt: { ...s.rt, floorBoost: 0 },
  };
  events.push({ t: s.t, type: "timeline.enter", state: def.id });
  events.push({ t: s.t, type: "timeline.phase", state: def.id, phase });
  if (!def.allowEmbodiment) next = wake(next, s.t, "death", events);

  if (def.forcedMode && next.mode !== def.forcedMode) {
    const vantage: WitnessVantage = def.forcedVantage ?? next.vantage;
    next = { ...next, mode: def.forcedMode, vantage };
    if (def.forcedMode === "witness") next = applyWitnessEntry(m, next);
    events.push({ t: s.t, type: "mode.change", mode: next.mode, vantage, forced: true });
  } else if (def.forcedVantage && next.mode === "witness" && next.vantage !== def.forcedVantage) {
    next = { ...next, vantage: def.forcedVantage };
    events.push({ t: s.t, type: "mode.change", mode: next.mode, vantage: next.vantage, forced: true });
  }

  if (next.mode === "embodiment" && !isUnconscious(next) && def.rt.entryImpulse > 0) {
    next = { ...next, rt: { ...next.rt, intensity: Math.max(next.rt.intensity, def.rt.entryImpulse) } };
  }
  return next;
}

function advanceToNextState(m: EpisodeManifest, s: SessionState, events: SessionEvent[]): SessionState {
  const nextIndex = s.timeline.index + 1;
  if (nextIndex >= TIMELINE.length) return s;
  return enterState(m, s, nextIndex, events);
}

function triggerSaying(
  m: EpisodeManifest,
  s: SessionState,
  via: TriggerVia,
  events: SessionEvent[],
): SessionState {
  const def = stateDef(m, s);
  const saying = def.saying!;
  events.push({ t: s.t, type: "saying.trigger", saying: saying.id, via });
  // A saying that comes due wakes him to speak it; speaking costs the body.
  let next: SessionState = wake({ ...s, timeline: { ...s.timeline, triggeredVia: via } }, s.t, "saying", events);
  if (next.mode === "embodiment") {
    const cost = m.exertion.sayingCost * (def.fatigue ?? 1);
    next = { ...next, consciousness: { ...next.consciousness, exertion: clamp01(next.consciousness.exertion + cost) } };
  }
  return setPhase(next, "utterance", events, def);
}

/* ------------------------------------------------------------------ */
/* Commands                                                            */
/* ------------------------------------------------------------------ */

function reject(s: SessionState, c: Command, reason: string, events: SessionEvent[]): SessionState {
  events.push({ t: s.t, type: "command.rejected", command: c.type, reason });
  return s;
}

function applyCommand(m: EpisodeManifest, s: SessionState, c: Command, events: SessionEvent[]): SessionState {
  if (s.exited) return reject(s, c, "session exited", events);
  const def = stateDef(m, s);

  switch (c.type) {
    case "exit":
      events.push({ t: s.t, type: "exit" });
      return { ...s, exited: true, rt: { ...s.rt, voices: [] }, cues: { ...s.cues, active: null } };

    case "pause":
      if (s.paused) return s;
      events.push({ t: s.t, type: "pause.change", paused: true });
      return { ...s, paused: true };

    case "resume":
      if (!s.paused) return s;
      events.push({ t: s.t, type: "pause.change", paused: false });
      return { ...s, paused: false };

    case "acknowledgeAdvisory":
      if (def.kind !== "gate") return reject(s, c, "no advisory pending", events);
      if (s.paused) return reject(s, c, "paused", events);
      return advanceToNextState(m, s, events);

    case "triggerSaying": {
      if (s.paused) return reject(s, c, "paused", events);
      if (isUnconscious(s)) return reject(s, c, "unconscious", events);
      if (def.kind !== "saying" || s.timeline.phase !== "approach") return reject(s, c, "no saying available", events);
      const min = def.timing.approachMinMs ?? 0;
      if (s.timeline.phaseElapsedMs < min) return reject(s, c, `too early (${s.timeline.phaseElapsedMs}ms < ${min}ms)`, events);
      return triggerSaying(m, s, c.via, events);
    }

    case "setMode": {
      const vantage = c.vantage ?? s.vantage;
      if (c.mode === "embodiment" && !def.allowEmbodiment) {
        return reject(s, c, `embodiment unavailable in ${def.id}`, events);
      }
      if (c.mode === s.mode && vantage === s.vantage) return s;
      let next: SessionState = { ...s, mode: c.mode, vantage };
      if (c.mode === "witness" && s.mode !== "witness") next = applyWitnessEntry(m, next);
      events.push({ t: s.t, type: "mode.change", mode: next.mode, vantage, forced: false });
      return next;
    }

    case "setLayer":
      if (c.layer === s.layer) return s;
      events.push({ t: s.t, type: "layer.change", layer: c.layer });
      return { ...s, layer: c.layer };

    case "setComfort":
      if (c.on === s.comfort) return s;
      events.push({ t: s.t, type: "comfort.change", on: c.on });
      // Enabling snaps gain immediately (I3-adjacent guarantee: reliable, same-tick reduction).
      return { ...s, comfort: c.on, rt: { ...s.rt, gain: c.on ? m.racingThoughts.comfortGain : s.rt.gain } };

    case "openEvidence":
      if (!m.sources.some((x) => x.id === c.sourceId)) return reject(s, c, `unknown source ${c.sourceId}`, events);
      events.push({ t: s.t, type: "evidence.change", sourceId: c.sourceId });
      return { ...s, evidenceOpen: c.sourceId };

    case "closeEvidence":
      if (s.evidenceOpen === null) return s;
      events.push({ t: s.t, type: "evidence.change", sourceId: null });
      return { ...s, evidenceOpen: null };

    case "setSettings": {
      const settings = { ...s.settings, ...sanitizeSettings(c.settings) };
      events.push({ t: s.t, type: "settings.change", settings });
      const next = { ...s, settings };
      return settings.swoon ? next : wake(next, s.t, "setting", events);
    }
  }
}

/* ------------------------------------------------------------------ */
/* Timeline clock                                                      */
/* ------------------------------------------------------------------ */

function advanceTimeline(m: EpisodeManifest, s: SessionState, dtMs: number, events: SessionEvent[]): SessionState {
  const def = stateDef(m, s);
  const tl = s.timeline;
  const gazeOnSayingTarget = def.saying && s.gaze.target === def.saying.gazeTarget && s.mode === "embodiment";
  let next: SessionState = {
    ...s,
    timeline: {
      ...tl,
      phaseElapsedMs: tl.phaseElapsedMs + dtMs,
      stateElapsedMs: tl.stateElapsedMs + dtMs,
      attendedTargetMs: tl.attendedTargetMs + (gazeOnSayingTarget ? dtMs : 0),
    },
  };
  const e = next.timeline.phaseElapsedMs;
  const t = def.timing;

  switch (def.kind) {
    case "gate":
    case "terminal":
      return next;
    case "elevation":
      if (tl.phase === "raising" && e >= t.raisingMs!) return setPhase(next, "settled", events, def);
      if (tl.phase === "settled" && e >= t.settledMs!) return advanceToNextState(m, next, events);
      return next;
    case "saying":
      if (tl.phase === "approach" && e >= t.approachAutoMs!) return triggerSaying(m, next, "auto", events);
      if (tl.phase === "utterance" && e >= t.utteranceMs!) {
        next = { ...next, rt: { ...next.rt, floorBoost: next.rt.floorBoost + def.rt.postUtteranceFloorBoost } };
        return setPhase(next, "resonance", events, def);
      }
      if (tl.phase === "resonance" && e >= t.resonanceMs!) return advanceToNextState(m, next, events);
      return next;
    case "timed":
      if (e >= t.runningMs!) return advanceToNextState(m, next, events);
      return next;
  }
}

/* ------------------------------------------------------------------ */
/* Step                                                                */
/* ------------------------------------------------------------------ */

export function step(m: EpisodeManifest, s0: SessionState, input: TickInput, dtMs: number = TICK_MS): StepResult {
  if (!(Number.isInteger(dtMs) && dtMs > 0)) throw new Error("dtMs must be a positive integer");
  const events: SessionEvent[] = [];
  let s = s0;

  for (const c of input.commands ?? []) s = applyCommand(m, s, c, events);
  if (s.paused || s.exited) return { state: s, events };

  /* gaze and breath */
  const sameTarget = input.gaze === s.gaze.target;
  s = {
    ...s,
    gaze: { target: input.gaze, dwellMs: (sameTarget ? s.gaze.dwellMs : 0) + dtMs },
    breath: clamp01(smoothToward(s.breath, input.breath ?? s.breath, dtMs, BREATH_TAU_MS)),
  };

  /* exertion and consciousness (reads the cue runtime to let a telling finish before waking) */
  const cs = stepConsciousness(m, stateDef(m, s), s, input, dtMs, tellingSounding(m, s, s.t + dtMs));
  s = cs.state;
  events.push(...cs.events);

  /* timeline */
  s = advanceTimeline(m, s, dtMs, events);

  /* dialogue cues and racing thoughts, against the (possibly new) current state; neither writes the timeline */
  const def = stateDef(m, s);
  const cues = stepCues(m, def, s, dtMs);
  const rt = stepRacingThoughts(m, def, s, dtMs);
  s = { ...s, cues: cues.cues, rt: rt.rt, rng: rt.rng, t: s.t + dtMs };
  events.push(...cues.events, ...rt.events);

  return { state: s, events };
}
