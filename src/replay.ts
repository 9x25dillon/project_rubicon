/**
 * Deterministic replay: (manifest, seed, input stream) -> (final state, event log, hash).
 * Used by tests, by QA bug reports (attach seed + input log), and as a cross-language parity check
 * against the UE5 C++ port (same inputs must yield the same timeline event sequence).
 */
import { fnv1a, stableStringify } from "./prng.js";
import { createSession, step, TICK_MS } from "./session.js";
import type { EpisodeManifest, SessionEvent, SessionState, Settings, TickInput } from "./types.js";

export interface ReplayResult {
  state: SessionState;
  events: SessionEvent[];
  stateHash: string;
  eventHash: string;
}

export function runReplay(
  m: EpisodeManifest,
  seed: number,
  inputs: Iterable<TickInput>,
  settings: Partial<Omit<Settings, "subtitles">> = {},
): ReplayResult {
  let s = createSession(m, seed, settings);
  const events: SessionEvent[] = [];
  for (const input of inputs) {
    const r = step(m, s, input, TICK_MS);
    s = r.state;
    events.push(...r.events);
  }
  return {
    state: s,
    events,
    stateHash: fnv1a(stableStringify(s)),
    eventHash: fnv1a(stableStringify(events)),
  };
}

/** Timeline-only projection of an event log: the part that must be invariant under mode/layer/comfort changes. */
export function timelineSignature(events: SessionEvent[]): string[] {
  return events.filter((e) => e.type === "timeline.enter").map((e) => (e as { state: string }).state);
}
