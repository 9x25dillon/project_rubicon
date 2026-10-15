/**
 * Export golden replays for cross-language parity tests (UE5 C++ port must reproduce these).
 *   npm run golden   ->  golden/replay-<seed>.json
 */
import { mkdirSync, writeFileSync } from "node:fs";
import { buildGolden, GOLDEN_CASES } from "../golden.js";
import { loadEpisode } from "../node/load.js";
import { validatorCases } from "../validatorCases.js";
import { rngNext } from "../prng.js";

const { manifest: m, locale } = loadEpisode();

mkdirSync("golden", { recursive: true });
for (const { seed, profile } of GOLDEN_CASES) {
  const g = buildGolden(m, seed, profile);
  writeFileSync(`golden/replay-${seed}.json`, JSON.stringify(g));
  console.log(
    `golden/replay-${seed}.json  profile=${profile}  ticks=${g.inputs.length}  events=${g.expected.events.length}  stateHash=${g.expected.stateHash}  eventHash=${g.expected.eventHash}`,
  );
}

const cases = validatorCases(m, locale);
writeFileSync("golden/validator-cases.json", JSON.stringify(cases));
console.log(`golden/validator-cases.json  cases=${cases.length}  errors=${cases.reduce((a, c) => a + c.errors.length, 0)}`);

/* Math vectors: V8 Math.exp / Math.pow on the input ranges the core uses (and wider). C++ must match bit for bit. */
{
  let r = 99;
  const rand = () => { const d = rngNext(r); r = d.state; return d.value; };
  const exp: number[][] = [];
  const pow: number[][] = [];
  for (let i = 0; i < 20000; i++) {
    const x = i % 2 ? -rand() * 60 : -(Math.floor(rand() * 400000) * 10) / (500 + Math.floor(rand() * 9500));
    exp.push([x, Math.exp(x)]);
    const px = i % 3 === 0 ? rand() * 50 : rand();
    const py = i % 3 === 2 ? rand() * 6 - 3 : 1.6;
    pow.push([px, py, Math.pow(px, py)]);
  }
  writeFileSync("golden/math-vectors.json", JSON.stringify({ exp, pow }));
  console.log(`golden/math-vectors.json  exp=${exp.length}  pow=${pow.length}`);
}
