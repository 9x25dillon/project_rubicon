import { test } from "node:test";
import assert from "node:assert/strict";
import { buildGolden, GOLDEN_CASES, goldenInputs, goldenTicks } from "../src/golden.js";
import { rngNext } from "../src/prng.js";
import { runReplay, timelineSignature } from "../src/replay.js";
import type { Command, TickInput } from "../src/types.js";
import { M } from "./helpers.js";

test("golden export is deterministic: same case gives identical hashes, events and trace (I6, C++ parity)", () => {
  const a = buildGolden(M, 7, "basic");
  const b = buildGolden(M, 7, "basic");
  assert.equal(a.expected.stateHash, b.expected.stateHash);
  assert.equal(a.expected.eventHash, b.expected.eventHash);
  assert.deepEqual(a.expected.trace, b.expected.trace);
});

test("basic golden inputs are unchanged from M0 (earlier golden files stay reproducible)", () => {
  // Reference copy of the M0 generator in src/cli/golden.ts.
  let r = 42;
  const rand = () => { const d = rngNext(r); r = d.state; return d.value; };
  const targets = [null, ...M.gazeTargets.map((g) => g.id)];
  const ref: TickInput[] = [{ gaze: null, commands: [{ type: "acknowledgeAdvisory" }] }];
  let gaze: string | null = null;
  for (let i = 1; i < goldenTicks(M); i++) {
    if (rand() < 0.01) gaze = targets[Math.floor(rand() * targets.length)]!;
    let commands: Command[] | undefined;
    const p = rand();
    if (p < 0.001) commands = [{ type: "setMode", mode: rand() < 0.5 ? "witness" : "embodiment" }];
    else if (p < 0.002) commands = [{ type: "setComfort", on: rand() < 0.5 }];
    else if (p < 0.003) commands = [{ type: "triggerSaying", via: "select" }];
    ref.push({ gaze, ...(commands ? { commands } : {}) });
  }
  assert.deepEqual(goldenInputs(M, 42, "basic"), ref);
});

test("the Racing Thoughts trace has one sample per tick, bounded to [0,1], and output never exceeds intensity", () => {
  for (const { seed, profile } of GOLDEN_CASES) {
    const g = buildGolden(M, seed, profile);
    const { intensity, output, gain } = g.expected.trace;
    assert.equal(intensity.length, g.inputs.length);
    assert.equal(output.length, g.inputs.length);
    assert.equal(gain.length, g.inputs.length);
    for (let i = 0; i < output.length; i++) {
      for (const v of [intensity[i]!, output[i]!, gain[i]!]) assert.ok(v >= 0 && v <= 1, `${profile}/${seed} tick ${i}: ${v}`);
      assert.ok(output[i]! <= intensity[i]! + 1e-12, `${profile}/${seed} tick ${i}: output above intensity`);
    }
  }
});

test("the coverage golden exercises every command type, a rejection of each class, and ends exited", () => {
  const g = buildGolden(M, 1234, "coverage");
  const used = new Set(g.inputs.flatMap((i) => (i.commands ?? []).map((c) => c.type)));
  for (const t of ["acknowledgeAdvisory", "triggerSaying", "setMode", "setLayer", "setComfort", "pause", "resume", "exit", "openEvidence", "closeEvidence", "setSettings"]) {
    assert.ok(used.has(t as Command["type"]), `coverage golden never uses ${t}`);
  }
  const reasons = new Set(g.expected.events.flatMap((e) => (e.type === "command.rejected" ? [e.reason.split(" ")[0]] : [])));
  for (const r of ["no", "too", "paused", "unknown", "session"]) assert.ok(reasons.has(r), `no rejection starting "${r}"`);
  assert.ok(g.expected.events.some((e) => e.type === "exit"));
  assert.ok(g.inputs.some((i) => i.breath !== undefined));
});

test("golden replays never break the canonical timeline order (I1)", () => {
  for (const { seed, profile } of GOLDEN_CASES) {
    const g = buildGolden(M, seed, profile);
    const sig = timelineSignature(runReplay(M, seed, g.inputs, g.settings).events);
    const canon = M.timeline.map((s) => s.id).slice(1, 1 + sig.length);
    assert.deepEqual(sig, canon, `${profile}/${seed}`);
  }
});

test("validator parity cases cover errors and warnings and the canonical manifest stays clean", async () => {
  const { validatorCases } = await import("../src/validatorCases.js");
  const { L } = await import("./helpers.js");
  const cases = validatorCases(M, L);
  assert.deepEqual(cases[0]!.errors, []);
  assert.ok(cases.slice(2).every((c) => c.errors.length > 0 || c.warnings.length > 0));
  assert.ok(cases.reduce((a, c) => a + c.errors.length, 0) > 40);
});
