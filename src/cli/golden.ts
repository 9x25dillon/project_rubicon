/**
 * Export golden replays for cross-language parity tests (UE5 C++ port must reproduce these).
 *   npm run golden   ->  golden/replay-<seed>.json
 */
import { mkdirSync, writeFileSync } from "node:fs";
import { maxAutoRunMs } from "../data.js";
import { loadEpisode } from "../node/load.js";
import { rngNext } from "../prng.js";
import { runReplay } from "../replay.js";
import { TICK_MS } from "../session.js";
import type { Command, TickInput } from "../types.js";

const { manifest: m } = loadEpisode();
const ticks = Math.ceil((maxAutoRunMs(m) + 2000) / TICK_MS);

function inputs(seed: number): TickInput[] {
  let r = seed >>> 0;
  const rand = () => {
    const d = rngNext(r);
    r = d.state;
    return d.value;
  };
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

mkdirSync("golden", { recursive: true });
for (const seed of [1, 7, 42]) {
  const inp = inputs(seed);
  const r = runReplay(m, seed, inp);
  writeFileSync(
    `golden/replay-${seed}.json`,
    JSON.stringify({
      manifestId: m.id,
      schemaVersion: m.schemaVersion,
      seed,
      tickMs: TICK_MS,
      inputs: inp,
      expected: { stateHash: r.stateHash, eventHash: r.eventHash, events: r.events },
    }),
  );
  console.log(`golden/replay-${seed}.json  events=${r.events.length}  stateHash=${r.stateHash}  eventHash=${r.eventHash}`);
}
