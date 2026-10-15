import { loadEpisode } from "../src/node/load.js";
import { rngNext } from "../src/prng.js";
import { createSession, step, TICK_MS } from "../src/session.js";
import type { Command, EpisodeManifest, SessionEvent, SessionState, Settings, TickInput } from "../src/types.js";

export const { manifest: M, locale: L } = loadEpisode();

export interface Driver {
  s: SessionState;
  events: SessionEvent[];
  run(ms: number, gaze?: string | null, commands?: Command[]): Driver;
  cmd(...commands: Command[]): Driver;
}

export function driver(
  m: EpisodeManifest = M,
  seed = 1,
  settings: Partial<Omit<Settings, "subtitles">> = {},
): Driver {
  const d: Driver = {
    s: createSession(m, seed, settings),
    events: [],
    run(ms, gaze = null, commands) {
      const ticks = Math.max(1, Math.round(ms / TICK_MS));
      for (let i = 0; i < ticks; i++) {
        const input: TickInput = { gaze, ...(i === 0 && commands ? { commands } : {}) };
        const r = step(m, d.s, input);
        d.s = r.state;
        d.events.push(...r.events);
      }
      return d;
    },
    cmd(...commands) {
      return d.run(TICK_MS, d.s.gaze.target, commands);
    },
  };
  return d;
}

/** Run until the given timeline state is entered (bounded). */
export function runUntilState(d: Driver, state: string, gaze: string | null = null, limitMs = 1_200_000): Driver {
  let t = 0;
  while (M.timeline[d.s.timeline.index]!.id !== state) {
    d.run(TICK_MS, gaze);
    t += TICK_MS;
    if (t > limitMs) throw new Error(`did not reach ${state} within ${limitMs}ms`);
  }
  return d;
}

/** Seeded random input stream mixing every command type with random gaze. */
export function fuzzInputs(seed: number, ticks: number, opts: { includeTimelineCommands: boolean }): TickInput[] {
  let r = seed >>> 0;
  const rand = () => {
    const x = rngNext(r);
    r = x.state;
    return x.value;
  };
  const targets = [null, ...M.gazeTargets.map((g) => g.id)];
  const nonTimeline: (() => Command)[] = [
    () => ({ type: "setMode", mode: rand() < 0.5 ? "witness" : "embodiment", vantage: (["centurion", "beloved_disciple", "mary"] as const)[Math.floor(rand() * 3)]! }),
    () => ({ type: "setLayer", layer: rand() < 0.5 ? "historical" : "theological" }),
    () => ({ type: "setComfort", on: rand() < 0.5 }),
    () => ({ type: "openEvidence", sourceId: M.sources[Math.floor(rand() * M.sources.length)]!.id }),
    () => ({ type: "closeEvidence" }),
    () => ({ type: "setSettings", settings: { intensityCeiling: rand(), reducedEffects: rand() < 0.5 } }),
  ];
  const timelineCmds: (() => Command)[] = [
    () => ({ type: "triggerSaying", via: rand() < 0.5 ? "select" : "speech" }),
  ];
  const pool = opts.includeTimelineCommands ? [...nonTimeline, ...timelineCmds] : nonTimeline;
  const out: TickInput[] = [{ gaze: null, commands: [{ type: "acknowledgeAdvisory" }] }];
  let gaze: string | null = null;
  for (let i = 1; i < ticks; i++) {
    if (rand() < 0.01) gaze = targets[Math.floor(rand() * targets.length)]!;
    const commands = rand() < 0.004 ? [pool[Math.floor(rand() * pool.length)]!()] : undefined;
    out.push({ gaze, ...(commands ? { commands } : {}) });
  }
  return out;
}
