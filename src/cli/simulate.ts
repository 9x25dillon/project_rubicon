/**
 * Headless run of the vertical slice: Threshold -> Elevation -> Forgiveness -> Witness.
 * Prints a 1 Hz trace so intensity curves can be inspected without an engine.
 *
 *   npm run simulate              (default seed 7)
 *   node dist/src/cli/simulate.js 42
 */
import { loadEpisode } from "../node/load.js";
import { selectFrame } from "../frame.js";
import { createSession, step, TICK_MS } from "../session.js";
import type { Command, SessionState, TickInput } from "../types.js";

const seed = Number(process.argv[2] ?? 7);
const { manifest: m, locale } = loadEpisode();
let s: SessionState = createSession(m, seed);

type Segment = { label: string; ms: number; gaze: string | null; commands?: Command[] };
const script: Segment[] = [
  { label: "consent", ms: TICK_MS, gaze: null, commands: [{ type: "acknowledgeAdvisory" }] },
  { label: "elevation: look at own hands", ms: 21000, gaze: "own_hands" },
  { label: "forgiveness: stare at crowd", ms: 20000, gaze: "crowd" },
  { label: "forgiveness: look away to horizon", ms: 10000, gaze: "horizon" },
  { label: "forgiveness: select saying", ms: TICK_MS, gaze: "crowd", commands: [{ type: "triggerSaying", via: "select" }] },
  { label: "forgiveness: utterance + resonance", ms: 6000, gaze: "crowd" },
  { label: "witness mode (centurion)", ms: 8000, gaze: null, commands: [{ type: "setMode", mode: "witness", vantage: "centurion" }] },
];

const pad = (x: string, n: number) => x.padEnd(n);
console.log(pad("t(s)", 7) + pad("state/phase", 26) + pad("gaze", 12) + pad("I", 7) + pad("out", 7) + pad("duck dB", 9) + "voices");
let fragments = 0;
for (const seg of script) {
  console.log(`-- ${seg.label}`);
  const ticks = Math.max(1, Math.round(seg.ms / TICK_MS));
  for (let i = 0; i < ticks; i++) {
    const input: TickInput = { gaze: seg.gaze, ...(i === 0 && seg.commands ? { commands: seg.commands } : {}) };
    const r = step(m, s, input);
    s = r.state;
    for (const e of r.events) {
      if (e.type === "rt.fragment") fragments++;
      else if (e.type !== "timeline.phase") console.log(`   event @${(e.t / 1000).toFixed(2)}s ${JSON.stringify(e)}`);
    }
    if (s.t % 1000 === 0) {
      const f = selectFrame(m, s, locale);
      console.log(
        pad((s.t / 1000).toFixed(0), 7) +
          pad(`${f.state}/${f.phase}`, 26) +
          pad(String(f.gaze.target), 12) +
          pad(f.audio.racingThoughts.intensity.toFixed(3), 7) +
          pad(f.audio.racingThoughts.output.toFixed(3), 7) +
          pad(f.audio.externalDuckDb.toFixed(1), 9) +
          f.audio.racingThoughts.activeVoices.map((v) => v.fragmentId).join(", "),
      );
    }
  }
}
const f = selectFrame(m, s, locale);
console.log(`\nfragments spawned: ${fragments}`);
console.log(`subtitle: ${f.saying?.subtitle ?? "(none)"}`);
console.log("evidence overlay:");
for (const e of f.evidenceOverlay.slice(0, 4)) console.log(`  [${e.label}] ${e.citation}${e.manuscriptNotes ? " (+manuscript notes)" : ""}`);
