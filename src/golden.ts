/**
 * Golden replays for cross-language parity (the UE5 C++ port must reproduce these bit for bit).
 *
 * Pure: builds the input stream and the expected results in memory. IO lives in src/cli/golden.ts.
 *
 * Two input profiles:
 *   "basic"     gaze changes plus setMode / setComfort / triggerSaying (the original M0 goldens)
 *   "coverage"  every command type, breath input, rejected commands, non-default settings and a final exit
 *   "dwell"     long gaze holds (mean 20 s per target) so every dwell tier and the gaze-triggered dialogue occur
 *
 * Expected results carry the full event log, the state and event hashes, and a per-tick Racing Thoughts
 * trace (internal intensity, mixer output, comfort gain) sampled after every step.
 */
import { maxAutoRunMs } from "./data.js";
import { rngNext } from "./prng.js";
import { outputIntensity } from "./racingThoughts.js";
import { runReplay } from "./replay.js";
import { createSession, stateDef, step, TICK_MS } from "./session.js";
import type { Command, EpisodeManifest, SessionEvent, Settings, TickInput, WitnessVantage } from "./types.js";

export type GoldenProfile = "basic" | "coverage" | "dwell";

export interface RacingThoughtsTrace {
  intensity: number[];
  output: number[];
  gain: number[];
}

export interface GoldenReplay {
  manifestId: string;
  schemaVersion: string;
  profile: GoldenProfile;
  seed: number;
  tickMs: number;
  settings: Partial<Omit<Settings, "subtitles">>;
  inputs: TickInput[];
  expected: { stateHash: string; eventHash: string; events: SessionEvent[]; trace: RacingThoughtsTrace };
}

export const GOLDEN_CASES: readonly { seed: number; profile: GoldenProfile }[] = [
  { seed: 1, profile: "basic" },
  { seed: 7, profile: "basic" },
  { seed: 42, profile: "basic" },
  { seed: 1234, profile: "coverage" },
  { seed: 99, profile: "dwell" },
];

export function goldenTicks(m: EpisodeManifest): number {
  return Math.ceil((maxAutoRunMs(m) + 2000) / TICK_MS);
}

function seededRand(seed: number): () => number {
  let r = seed >>> 0;
  return () => {
    const d = rngNext(r);
    r = d.state;
    return d.value;
  };
}

/** The original M0 golden stream. Kept byte-identical so earlier golden files stay reproducible. */
function basicInputs(m: EpisodeManifest, seed: number, ticks: number): TickInput[] {
  const rand = seededRand(seed);
  const targets = [null, ...m.gazeTargets.map((g) => g.id)];
  const out: TickInput[] = [{ gaze: null, commands: [{ type: "acknowledgeAdvisory" }] }];
  let gaze: string | null = null;
  for (let i = 1; i < ticks; i++) {
    if (rand() < 0.01) gaze = targets[Math.floor(rand() * targets.length)]!;
    let commands: Command[] | undefined;
    const p = rand();
    if (p < 0.001) commands = [{ type: "setMode", mode: rand() < 0.5 ? "witness" : "embodiment" }];
    else if (p < 0.002) commands = [{ type: "setComfort", on: rand() < 0.5 }];
    else if (p < 0.003) commands = [{ type: "triggerSaying", via: "select" }];
    out.push({ gaze, ...(commands ? { commands } : {}) });
  }
  return out;
}

const VANTAGES: readonly WitnessVantage[] = ["centurion", "beloved_disciple", "mary"];

function coverageInputs(m: EpisodeManifest, seed: number, ticks: number): TickInput[] {
  const rand = seededRand(seed);
  const pickInt = (n: number) => Math.floor(rand() * n);
  const targets = [null, ...m.gazeTargets.map((g) => g.id)];
  const commandMakers: (() => Command)[] = [
    () => ({ type: "setMode", mode: rand() < 0.5 ? "witness" : "embodiment", vantage: VANTAGES[pickInt(3)]! }),
    () => ({ type: "setMode", mode: rand() < 0.5 ? "witness" : "embodiment" }),
    () => ({ type: "setLayer", layer: rand() < 0.5 ? "historical" : "theological" }),
    () => ({ type: "setComfort", on: rand() < 0.5 }),
    () => ({ type: "openEvidence", sourceId: m.sources[pickInt(m.sources.length)]!.id }),
    () => ({ type: "openEvidence", sourceId: "no-such-source" }),
    () => ({ type: "closeEvidence" }),
    () => ({ type: "setSettings", settings: { intensityCeiling: rand() * 1.2 - 0.1 } }),
    () => ({ type: "setSettings", settings: { reducedEffects: rand() < 0.5, seated: rand() < 0.5, hapticsEnabled: false } }),
    () => ({ type: "triggerSaying", via: rand() < 0.5 ? "select" : "speech" }),
    () => ({ type: "acknowledgeAdvisory" }),
    () => ({ type: "pause" }),
  ];
  const out: TickInput[] = [
    { gaze: null, commands: [{ type: "triggerSaying", via: "select" }] }, // rejected at the gate
    { gaze: null, breath: 0.5, commands: [{ type: "setSettings", settings: { intensityCeiling: 0.9 } }, { type: "acknowledgeAdvisory" }] },
  ];
  let gaze: string | null = null;
  let breath = 0.5;
  let paused = false;
  for (let i = out.length; i < ticks; i++) {
    if (rand() < 0.01) gaze = targets[pickInt(targets.length)]!;
    if (rand() < 0.02) breath = Math.round(rand() * 1000) / 1000;
    let commands: Command[] | undefined;
    if (paused) {
      if (rand() < 0.02) {
        commands = [{ type: "resume" }];
        paused = false;
      } else if (rand() < 0.005) {
        commands = [{ type: "triggerSaying", via: "select" }]; // rejected while paused
      }
    } else if (rand() < 0.004) {
      commands = [commandMakers[pickInt(commandMakers.length)]!()];
      if (rand() < 0.2) commands.push(commandMakers[pickInt(commandMakers.length)]!());
      paused = commands.some((c) => c.type === "pause");
      if (commands.some((c) => c.type === "resume")) paused = false;
    }
    if (i === ticks - 2) commands = [...(commands ?? []), { type: "exit" }];
    if (i === ticks - 1) commands = [{ type: "setLayer", layer: "theological" }]; // rejected after exit
    out.push({ gaze, ...(rand() < 0.9 ? { breath } : {}), ...(commands ? { commands } : {}) });
  }
  return out;
}

function dwellInputs(m: EpisodeManifest, seed: number, ticks: number): TickInput[] {
  const rand = seededRand(seed);
  const targets = m.gazeTargets.map((g) => g.id);
  const out: TickInput[] = [{ gaze: null, commands: [{ type: "acknowledgeAdvisory" }] }];
  let gaze: string | null = targets[0]!;
  for (let i = 1; i < ticks; i++) {
    if (rand() < 0.0005) gaze = targets[Math.floor(rand() * targets.length)]!;
    let commands: Command[] | undefined;
    const p = rand();
    if (p < 0.0002) commands = [{ type: "setMode", mode: rand() < 0.5 ? "witness" : "embodiment" }];
    else if (p < 0.0004) commands = [{ type: "setComfort", on: rand() < 0.3 }];
    else if (p < 0.0007) commands = [{ type: "triggerSaying", via: "speech" }];
    out.push({ gaze, ...(commands ? { commands } : {}) });
  }
  return out;
}

export function goldenInputs(m: EpisodeManifest, seed: number, profile: GoldenProfile): TickInput[] {
  const ticks = goldenTicks(m);
  if (profile === "basic") return basicInputs(m, seed, ticks);
  if (profile === "dwell") return dwellInputs(m, seed, ticks);
  return coverageInputs(m, seed, ticks);
}

export function goldenSettings(profile: GoldenProfile): Partial<Omit<Settings, "subtitles">> {
  return profile === "coverage" ? { intensityCeiling: 0.85, seated: false } : {};
}

/** Per-tick Racing Thoughts trace, sampled after each step (what the engine mixer would read that tick). */
export function racingThoughtsTrace(
  m: EpisodeManifest,
  seed: number,
  inputs: readonly TickInput[],
  settings: Partial<Omit<Settings, "subtitles">>,
): RacingThoughtsTrace {
  const trace: RacingThoughtsTrace = { intensity: [], output: [], gain: [] };
  let s = createSession(m, seed, settings);
  for (const input of inputs) {
    s = step(m, s, input, TICK_MS).state;
    trace.intensity.push(s.rt.intensity);
    trace.output.push(outputIntensity(m, stateDef(m, s), s));
    trace.gain.push(s.rt.gain);
  }
  return trace;
}

export function buildGolden(m: EpisodeManifest, seed: number, profile: GoldenProfile): GoldenReplay {
  const inputs = goldenInputs(m, seed, profile);
  const settings = goldenSettings(profile);
  const r = runReplay(m, seed, inputs, settings);
  return {
    manifestId: m.id,
    schemaVersion: m.schemaVersion,
    profile,
    seed,
    tickMs: TICK_MS,
    settings,
    inputs,
    expected: {
      stateHash: r.stateHash,
      eventHash: r.eventHash,
      events: r.events,
      trace: racingThoughtsTrace(m, seed, inputs, settings),
    },
  };
}
