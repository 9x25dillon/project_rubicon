/**
 * Racing Thoughts: a continuous, deterministic intensity system.
 *
 * It READS timeline state (I5) and never writes it. Model per tick (dt fixed):
 *
 *   w      = stressWeight(effectiveStress(state, gazeTarget))          relief < 0 < low < ... < extreme
 *   d      = 1 - exp(-dwellMs / dwellTau)                               prolonged gaze costs more
 *   floor' = floor + floorBoost
 *   target = clamp( w >= 0 ? floor' + w*d*(1-floor') : floor' * (1 + w*d),  0, ceiling(t) )
 *   target = 0                                                           when in Witness
 *   I     <- I + (target - I)(1 - exp(-dt/tau)),   tau = tauUp if target > I else tauDown
 *
 *   gain   : Comfort snaps to comfortGain immediately; release ramps back with comfortReleaseTau
 *   output = I * gain * settings.intensityCeiling * (utterance ? utteranceDuck : 1) * (paused ? 0 : 1)
 *
 * Asymmetric taus (tauDown >> tauUp) produce the required "looking away helps only gradually".
 * Witness entry multiplies I by witnessEchoScale (immediate drop) and then decays with witnessEchoTau
 * (the residual echo of a mind catching up).
 *
 * Fragments: Poisson-like but deterministic. rate = min + (max-min) * output^gamma.
 * accumulator += rate*dt; each unit spawns one fragment if a voice slot is free.
 * maxActiveVoices = 1 + floor(output * (maxVoices - 1)). Selection is weighted and seeded; recent fragments
 * are excluded so the stream does not loop.
 *
 * Dwell tiers: a fragment with `gaze: { target, tier }` is eligible only in Embodiment, while the gaze rests on that
 * target, and only in that tier (glance -> sustained -> long -> extreme as dwell grows). Tiers are exclusive, so a
 * long stare replaces the glance lines with harder ones instead of adding to them.
 */
import { clamp, clamp01, lerp, rngNext, smoothToward } from "./prng.js";
import type {
  EpisodeManifest,
  Fragment,
  SessionEvent,
  SessionState,
  StressLevel,
  TimelineStateDef,
  Voice,
} from "./types.js";

export function nominalStateDurationMs(def: TimelineStateDef): number {
  const t = def.timing;
  switch (def.kind) {
    case "elevation":
      return (t.raisingMs ?? 0) + (t.settledMs ?? 0);
    case "saying":
      return (t.approachAutoMs ?? 0) + (t.utteranceMs ?? 0) + (t.resonanceMs ?? 0);
    case "timed":
      return t.runningMs ?? 1;
    default:
      return 1;
  }
}

export function effectiveStress(m: EpisodeManifest, def: TimelineStateDef, target: string | null): StressLevel {
  if (target === null) return m.racingThoughts.noTargetStress;
  const override = def.stressOverrides[target];
  if (override) return override;
  const g = m.gazeTargets.find((x) => x.id === target);
  return g ? g.stress : m.racingThoughts.noTargetStress;
}

export function currentCeiling(def: TimelineStateDef, stateElapsedMs: number): number {
  const r = def.rt;
  if (r.ceilingEnd === undefined) return r.ceiling;
  const p = clamp01(stateElapsedMs / nominalStateDurationMs(def));
  return lerp(r.ceiling, r.ceilingEnd, p);
}

export function intensityTarget(m: EpisodeManifest, def: TimelineStateDef, s: SessionState): number {
  if (s.mode === "witness") return 0;
  const model = m.racingThoughts;
  const lvl = effectiveStress(m, def, s.gaze.target);
  const w = model.stressWeights[lvl];
  const d = s.gaze.target === null ? 1 : 1 - Math.exp(-s.gaze.dwellMs / model.dwellTauMs);
  const floor = clamp01(def.rt.floor + s.rt.floorBoost);
  const raw = w >= 0 ? floor + w * d * (1 - floor) : floor * (1 + w * d);
  return clamp(raw, 0, currentCeiling(def, s.timeline.stateElapsedMs));
}

export function outputIntensity(m: EpisodeManifest, def: TimelineStateDef, s: SessionState): number {
  if (s.paused || s.exited) return 0;
  const duck = def.kind === "saying" && s.timeline.phase === "utterance" ? def.rt.utteranceDuck : 1;
  return clamp01(s.rt.intensity * s.rt.gain * s.settings.intensityCeiling * duck);
}

export function maxActiveVoices(m: EpisodeManifest, output: number): number {
  return 1 + Math.floor(output * (m.racingThoughts.maxVoices - 1));
}

export function spawnRatePerSec(m: EpisodeManifest, output: number): number {
  const r = m.racingThoughts.fragmentRate;
  return output <= 0 ? 0 : r.minPerSec + (r.maxPerSec - r.minPerSec) * Math.pow(output, r.gamma);
}

export function externalDuckDb(m: EpisodeManifest, output: number): number {
  const d = m.racingThoughts.externalDuck;
  if (output <= d.startAt) return 0;
  const t = clamp01((output - d.startAt) / (1 - d.startAt));
  return d.maxDb * t * t * (3 - 2 * t);
}

/** Index of the dwell tier for a continuous gaze of dwellMs (the last tier whose minDwellMs <= dwellMs). */
export function dwellTierIndex(m: EpisodeManifest, dwellMs: number): number {
  const tiers = m.racingThoughts.dwellTiers;
  let idx = 0;
  for (let i = 0; i < tiers.length; i++) if (tiers[i]!.minDwellMs <= dwellMs) idx = i;
  return idx;
}

export function dwellTierId(m: EpisodeManifest, dwellMs: number): string {
  return m.racingThoughts.dwellTiers[dwellTierIndex(m, dwellMs)]!.id;
}

/** Dwell-tiered lines answer the embodied gaze only: the current target, in exactly the current tier. */
function gazeEligible(m: EpisodeManifest, f: Fragment, s: SessionState): boolean {
  if (!f.gaze) return true;
  return s.mode === "embodiment" && s.gaze.target === f.gaze.target && dwellTierId(m, s.gaze.dwellMs) === f.gaze.tier;
}

function eligible(m: EpisodeManifest, def: TimelineStateDef, s: SessionState, voices: Voice[], output: number): Fragment[] {
  const active = new Set(voices.map((v) => v.fragmentId));
  const recent = new Set(s.rt.recent);
  return m.fragments.filter(
    (f) =>
      (f.states === "*" || f.states.includes(def.id)) &&
      f.minIntensity <= output &&
      !active.has(f.id) &&
      !recent.has(f.id) &&
      gazeEligible(m, f, s),
  );
}

/**
 * Advance the Racing Thoughts subsystem by dtMs. Pure: returns the new rt runtime, rng and events.
 */
export function stepRacingThoughts(
  m: EpisodeManifest,
  def: TimelineStateDef,
  s: SessionState,
  dtMs: number,
): { rt: SessionState["rt"]; rng: number; events: SessionEvent[] } {
  const model = m.racingThoughts;
  const events: SessionEvent[] = [];
  let rng = s.rng;

  /* intensity */
  const target = intensityTarget(m, def, s);
  const tau =
    s.mode === "witness" ? model.witnessEchoTauMs : target > s.rt.intensity ? def.rt.tauUpMs : def.rt.tauDownMs;
  const intensity = clamp01(smoothToward(s.rt.intensity, target, dtMs, tau));

  /* comfort gain: snap down on enable (handled in command), ramp up on release */
  const gain = s.comfort ? model.comfortGain : clamp01(smoothToward(s.rt.gain, 1, dtMs, model.comfortReleaseTauMs));

  const next: SessionState = { ...s, rt: { ...s.rt, intensity, gain } };
  const output = outputIntensity(m, def, next);

  /* voices */
  let voices = s.rt.voices.filter((v) => v.endsAt > s.t + dtMs);
  let recent = s.rt.recent;
  let acc = s.rt.spawnAccumulator + (spawnRatePerSec(m, output) * dtMs) / 1000;
  const cap = maxActiveVoices(m, output);

  while (acc >= 1) {
    if (voices.length >= cap) {
      acc = 1; // hold one pending spawn until a voice frees; prevents bursts after saturation
      break;
    }
    const pool = eligible(m, def, { ...next, rt: { ...next.rt, recent } }, voices, output);
    if (pool.length === 0) {
      acc = 0;
      break;
    }
    const weights = pool.map((f) =>
      f.weight * (s.gaze.target !== null && f.targetAffinity.includes(s.gaze.target) ? model.affinityMultiplier : 1),
    );
    const total = weights.reduce((a, b) => a + b, 0);
    const draw = rngNext(rng);
    rng = draw.state;
    let pick = draw.value * total;
    let chosen = pool[pool.length - 1]!;
    for (let i = 0; i < pool.length; i++) {
      pick -= weights[i]!;
      if (pick < 0) {
        chosen = pool[i]!;
        break;
      }
    }
    const startedAt = s.t + dtMs;
    voices = [...voices, { fragmentId: chosen.id, startedAt, endsAt: startedAt + chosen.durationMs }];
    recent = [...recent, chosen.id].slice(-model.recentWindow);
    events.push({ t: startedAt, type: "rt.fragment", fragmentId: chosen.id, kind: chosen.kind, voice: voices.length - 1 });
    acc -= 1;
  }

  return {
    rt: { ...next.rt, voices, recent, spawnAccumulator: acc },
    rng,
    events,
  };
}
