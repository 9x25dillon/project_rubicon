/**
 * Exertion and consciousness. Pure and deterministic; reads engine-measured head motion and voice activity from
 * the tick input, never writes the timeline (I1, I5).
 *
 * Looking is free: gaze direction never costs anything. Moving the head fast or far, and talking, cost the body;
 * resting recovers it. When exertion reaches 1 the player loses consciousness: the Racing Thoughts storm lets go
 * (as on Witness entry) and the current moment is told, Gospel by Gospel, from the disciples' vantages (telling
 * cues). The timeline never waits. Consciousness returns after rest, or when a saying comes due (he wakes to speak).
 * Never a meter, never a score; Settings.swoon = false turns it off.
 */
import { clamp01 } from "./prng.js";
import type { ConsciousnessCause, ConsciousnessRuntime, EpisodeManifest, SessionEvent, SessionState, TickInput, TimelineStateDef } from "./types.js";

export const AWAKE: ConsciousnessRuntime = { exertion: 0, swoon: null };

export function isUnconscious(s: SessionState): boolean {
  return s.consciousness.swoon !== null;
}

/** [0,1] exertion drive from one tick's measurements. */
export function exertionDrive(m: EpisodeManifest, input: TickInput): number {
  const x = m.exertion;
  const ang = Math.max(0, (input.headDegPerSec ?? 0) - x.angularDeadzoneDegPerSec) / x.angularScaleDegPerSec;
  const lin = Math.max(0, (input.headCmPerSec ?? 0) - x.linearDeadzoneCmPerSec) / x.linearScaleCmPerSec;
  const voice = Math.max(0, (input.voice ?? 0) - x.voiceDeadzone);
  return clamp01(ang + lin + voice);
}

export function canSwoonIn(def: TimelineStateDef): boolean {
  return def.allowEmbodiment && def.kind !== "gate";
}

function change(t: number, conscious: boolean, cause: ConsciousnessCause): SessionEvent {
  return { t, type: "consciousness.change", conscious, cause };
}

/** Wake immediately (a saying comes due, the setting is turned off, death). No-op when already conscious. */
export function wake(s: SessionState, t: number, cause: ConsciousnessCause, events: SessionEvent[]): SessionState {
  if (!isUnconscious(s)) return s;
  events.push(change(t, true, cause));
  return { ...s, consciousness: { ...s.consciousness, swoon: null } };
}

/**
 * Advance exertion by dtMs (after commands and gaze, before the timeline clock). `tellingSounding` holds waking
 * until the telling being read has finished.
 */
export function stepConsciousness(
  m: EpisodeManifest,
  def: TimelineStateDef,
  s: SessionState,
  input: TickInput,
  dtMs: number,
  tellingSounding: boolean,
): { state: SessionState; events: SessionEvent[] } {
  const x = m.exertion;
  const now = s.t + dtMs;
  const events: SessionEvent[] = [];
  const c = s.consciousness;

  if (c.swoon) {
    const exertion = clamp01(c.exertion - (x.swoonRecoveryPerSec * dtMs) / 1000);
    const rested = exertion <= x.wakeAt && now - c.swoon.since >= x.minSwoonMs && !tellingSounding;
    if (rested) {
      events.push(change(now, true, "recovered"));
      return { state: { ...s, consciousness: { exertion, swoon: null } }, events };
    }
    return { state: { ...s, consciousness: { ...c, exertion } }, events };
  }

  const active = canSwoonIn(def) && s.mode === "embodiment";
  const drive = active ? exertionDrive(m, input) : 0;
  const fatigue = def.fatigue ?? 1;
  const exertion = clamp01(c.exertion + ((drive * x.risePerSec * fatigue - (1 - drive) * x.recoveryPerSec) * dtMs) / 1000);
  const speaking = def.kind === "saying" && s.timeline.phase === "utterance";
  if (active && s.settings.swoon && exertion >= 1 && !speaking) {
    events.push(change(now, false, "exertion"));
    // The mind lets go: the storm drops as it does on Witness entry, then decays to silence.
    return {
      state: { ...s, consciousness: { exertion, swoon: { since: now } }, rt: { ...s.rt, intensity: s.rt.intensity * m.racingThoughts.witnessEchoScale } },
      events,
    };
  }
  return { state: { ...s, consciousness: { ...c, exertion } }, events };
}
