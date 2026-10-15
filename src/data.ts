/**
 * Data layer: typed accessors over the canonical JSON manifest.
 * The manifest is the single source of truth; nothing here duplicates content.
 */
import { SAYING_IDS, type EpisodeManifest, type Saying, type SourceRef, type TimelineStateDef } from "./types.js";

/** The starting-contract `lastWords`, derived from the manifest in canonical order. */
export function lastWords(m: EpisodeManifest): Saying[] {
  return SAYING_IDS.map((id) => {
    const s = m.timeline.find((x) => x.id === id)?.saying;
    if (!s) throw new Error(`manifest missing saying ${id}`);
    return s;
  });
}

export function source(m: EpisodeManifest, id: string): SourceRef {
  const s = m.sources.find((x) => x.id === id);
  if (!s) throw new Error(`unknown source ${id}`);
  return s;
}

export function stateById(m: EpisodeManifest, id: TimelineStateDef["id"]): TimelineStateDef {
  const s = m.timeline.find((x) => x.id === id);
  if (!s) throw new Error(`unknown state ${id}`);
  return s;
}

/** Upper bound on time from Elevation to Contemplation with zero player input (I4). */
export function maxAutoRunMs(m: EpisodeManifest): number {
  return m.timeline.reduce((acc, d) => {
    const t = d.timing;
    switch (d.kind) {
      case "elevation":
        return acc + (t.raisingMs ?? 0) + (t.settledMs ?? 0);
      case "saying":
        return acc + (t.approachAutoMs ?? 0) + (t.utteranceMs ?? 0) + (t.resonanceMs ?? 0);
      case "timed":
        return acc + (t.runningMs ?? 0);
      default:
        return acc;
    }
  }, 0);
}
