/**
 * Dialogue cues: the world speaking. Pure and deterministic; no randomness (the Racing Thoughts PRNG stream is
 * untouched, so adding cues never changes which fragments play).
 *
 * Reads timeline, gaze and mode; never writes the timeline or Racing Thoughts state (I7). One cue sounds at a
 * time; each plays at most once per session. When several become eligible on the same tick, manifest order wins.
 *
 *   gaze trigger    the gaze has rested on `target` for at least minDwellMs, in the given mode (embodied gaze cues
 *                   need the player conscious)
 *   phase trigger   the timeline is in `state` / `phase` and the phase has run at least atMs
 *   telling trigger the player is unconscious (and embodied) in `state`: the Gospels' tellings, in manifest order
 *
 * Playback rules:
 *   fit     a cue starts only if it can finish before the nominal end of the current phase (phases with a fixed
 *           length), so a cue never spills into the next phase or state
 *   floor   during a saying's utterance the saying has the floor: any cue not triggered by that utterance is cut,
 *           and gaze cues do not start
 *   preempt the scene's recorded events outrank points of interest and tellings: an eligible phase cue cuts a
 *           sounding gaze or telling cue (so nothing can delay the exchange Luke records before Paradise)
 *   once    a cue that plays is marked played; a phase cue whose phase ends before it fits is never heard
 *           (phases are never held open for dialogue: I4)
 */
import type { Cue, CueRuntime, EpisodeManifest, Phase, SessionEvent, SessionState, TimelineStateDef } from "./types.js";

export const EMPTY_CUES: CueRuntime = { played: [], active: null };

/** Nominal length of a phase, or undefined for open-ended phases (advisory, open). */
/** The telling being read now, if any (used to hold waking until it finishes). */
export function tellingSounding(m: EpisodeManifest, s: SessionState, now: number): boolean {
  const a = s.cues.active;
  return a !== null && a.endsAt > now && m.cues.find((c) => c.id === a.id)?.kind === "telling";
}

export function phaseDurationMs(def: TimelineStateDef, phase: Phase): number | undefined {
  const t = def.timing;
  switch (phase) {
    case "raising": return t.raisingMs;
    case "settled": return t.settledMs;
    case "approach": return t.approachAutoMs;
    case "utterance": return t.utteranceMs;
    case "resonance": return t.resonanceMs;
    case "running": return t.runningMs;
    default: return undefined;
  }
}

function inUtterance(def: TimelineStateDef, s: SessionState): boolean {
  return def.kind === "saying" && s.timeline.phase === "utterance";
}

function triggeredByThisUtterance(c: Cue, def: TimelineStateDef): boolean {
  return c.trigger.type === "phase" && c.trigger.state === def.id && c.trigger.phase === "utterance";
}

export function cueEligible(c: Cue, def: TimelineStateDef, s: SessionState): boolean {
  if (!(c.states === "*" || c.states.includes(def.id))) return false;
  const tr = c.trigger;
  const unconscious = s.consciousness.swoon !== null;
  if (tr.type === "gaze") {
    if (inUtterance(def, s)) return false;
    if (tr.mode !== "any" && tr.mode !== s.mode) return false;
    if (tr.mode === "embodiment" && unconscious) return false;
    if (!(s.gaze.target === tr.target && s.gaze.dwellMs >= tr.minDwellMs)) return false;
  } else if (tr.type === "telling") {
    if (inUtterance(def, s) || !unconscious || s.mode !== "embodiment" || def.id !== tr.state) return false;
  } else if (!(def.id === tr.state && s.timeline.phase === tr.phase && s.timeline.phaseElapsedMs >= tr.atMs)) {
    return false;
  }
  const dur = phaseDurationMs(def, s.timeline.phase);
  return dur === undefined || s.timeline.phaseElapsedMs + c.durationMs <= dur;
}

/** Advance cue playback by dtMs, against the state after gaze and timeline have been updated for this tick. */
export function stepCues(
  m: EpisodeManifest,
  def: TimelineStateDef,
  s: SessionState,
  dtMs: number,
): { cues: CueRuntime; events: SessionEvent[] } {
  const now = s.t + dtMs;
  let active = s.cues.active && s.cues.active.endsAt > now ? s.cues.active : null;
  if (active && inUtterance(def, s)) {
    const c = m.cues.find((x) => x.id === active!.id);
    if (!c || !triggeredByThisUtterance(c, def)) active = null; // the saying has the floor
  }
  const played = new Set(s.cues.played);
  const start = (c: Cue) => ({
    cues: { played: [...s.cues.played, c.id], active: { id: c.id, startedAt: now, endsAt: now + c.durationMs } },
    events: [{ t: now, type: "cue.start", cueId: c.id, kind: c.kind, speaker: c.speaker } as SessionEvent],
  });
  const activeIsGaze = active !== null && m.cues.find((x) => x.id === active!.id)?.trigger.type !== "phase";

  /* pass 1: phase cues (recorded events), which may preempt a sounding gaze cue */
  if (active === null || activeIsGaze) {
    for (const c of m.cues) {
      if (c.trigger.type === "phase" && !played.has(c.id) && cueEligible(c, def, s)) return start(c);
    }
  }
  if (active) return { cues: active === s.cues.active ? s.cues : { ...s.cues, active }, events: [] };

  /* pass 2: gaze cues (points of interest) and tellings */
  for (const c of m.cues) {
    if (c.trigger.type !== "phase" && !played.has(c.id) && cueEligible(c, def, s)) return start(c);
  }
  return { cues: s.cues.active === null ? s.cues : { ...s.cues, active: null }, events: [] };
}
