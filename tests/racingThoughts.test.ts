import { test } from "node:test";
import assert from "node:assert/strict";
import { selectFrame } from "../src/frame.js";
import { TICK_MS } from "../src/session.js";
import { driver, L, M, runUntilState } from "./helpers.js";

const out = (d: ReturnType<typeof driver>) => selectFrame(M, d.s, L).audio.racingThoughts.output;
const fragmentsBetween = (d: ReturnType<typeof driver>, t0: number, t1: number) =>
  d.events.filter((e) => e.type === "rt.fragment" && e.t > t0 && e.t <= t1).length;

function inForgiveness(seed = 3) {
  const d = driver(M, seed).cmd({ type: "acknowledgeAdvisory" });
  return runUntilState(d, "forgiveness", "horizon");
}

test("gazing at the crowd raises intensity and fragment frequency versus the horizon", () => {
  const crowd = inForgiveness();
  const t0c = crowd.s.t;
  crowd.run(20_000, "crowd");
  const calm = inForgiveness();
  const t0h = calm.s.t;
  calm.run(20_000, "horizon");
  assert.ok(out(crowd) > out(calm) + 0.2, `crowd ${out(crowd)} vs horizon ${out(calm)}`);
  assert.ok(fragmentsBetween(crowd, t0c, crowd.s.t) > fragmentsBetween(calm, t0h, calm.s.t));
  assert.ok(selectFrame(M, crowd.s, L).audio.externalDuckDb < -3, "high intensity ducks the outside world");
});

test("prolonged gaze costs more than a glance (dwell escalation)", () => {
  const glance = inForgiveness().run(1000, "crowd");
  const stare = inForgiveness().run(15_000, "crowd");
  assert.ok(out(stare) > out(glance) + 0.15);
});

test("looking away reduces intensity only gradually", () => {
  const d = inForgiveness().run(20_000, "crowd");
  const peak = out(d);
  d.run(1000, "horizon");
  assert.ok(out(d) > peak * 0.85, `after 1s: ${out(d)} vs peak ${peak}`);
  d.run(9000, "horizon");
  assert.ok(out(d) < peak - 0.15, "it does fall over time");
  assert.ok(out(d) > M.timeline.find((s) => s.id === "forgiveness")!.rt.floor * 0.9, "but never below the state's floor");
});

test("comfort mode reduces output to near silence within one tick, and releases gradually", () => {
  const d = inForgiveness().run(20_000, "crowd");
  assert.ok(out(d) > 0.6);
  d.cmd({ type: "setComfort", on: true });
  assert.ok(out(d) <= 0.05, `comfort output ${out(d)}`);
  d.run(5000, "crowd");
  assert.ok(out(d) <= 0.05);
  d.cmd({ type: "setComfort", on: false });
  assert.ok(out(d) < 0.15, "no spike on release");
  d.run(10_000, "crowd");
  assert.ok(out(d) > 0.5, "returns over time");
});

test("witness mode drops intensity immediately, leaves a brief echo, then silence", () => {
  const d = inForgiveness().run(20_000, "crowd");
  const before = out(d);
  d.cmd({ type: "setMode", mode: "witness", vantage: "centurion" });
  const echo = out(d);
  assert.ok(echo <= before * M.racingThoughts.witnessEchoScale + 0.01, `immediate: ${echo}`);
  assert.ok(echo > 0.05, "residual echo lingers");
  d.run(12_000, null);
  assert.ok(out(d) < 0.01, `silence after echo: ${out(d)}`);
});

test("only the wood of the cross brings relief: below the floor in every embodied state; the penitent is gentle but not relief", () => {
  for (const state of ["elevation", "forgiveness", "paradise", "mother", "abandonment", "thirst", "completion", "surrender"]) {
    const d = driver(M, 5).cmd({ type: "acknowledgeAdvisory" });
    runUntilState(d, state, "horizon").run(15_000, "cross");
    const def = M.timeline.find((s) => s.id === state)!;
    assert.ok(out(d) < def.rt.floor, `${state}: cross ${out(d)} < floor ${def.rt.floor}`);
  }
  const mk = (gaze: string) => {
    const d = driver(M, 5).cmd({ type: "acknowledgeAdvisory" });
    runUntilState(d, "paradise", "horizon");
    return d.run(20_000, gaze);
  };
  const penitent = mk("condemned_man");
  const railing = mk("other_condemned");
  const floor = M.timeline.find((s) => s.id === "paradise")!.rt.floor;
  assert.ok(out(penitent) >= floor * 0.95, `penitent is not relief: ${out(penitent)} vs floor ${floor}`);
  assert.ok(out(railing) > out(penitent) + 0.2, "but the railing man is far worse");
  // content rule: the cross is the only relief target, and nothing is overridden to relief
  assert.deepEqual(M.gazeTargets.filter((g) => g.stress === "relief").map((g) => g.id), ["cross"]);
  assert.ok(M.timeline.every((s) => Object.values(s.stressOverrides).every((l) => l !== "relief")));
});

test("thirst: the bodily need cuts through during the line, then the noise returns louder", () => {
  const d = driver(M, 9).cmd({ type: "acknowledgeAdvisory" });
  runUntilState(d, "thirst", "sky").run(8000, "soldier_with_sponge");
  const preLine = out(d);
  d.cmd({ type: "triggerSaying", via: "select" });
  d.run(1500, "soldier_with_sponge");
  const duringLine = out(d);
  assert.ok(duringLine < preLine * 0.3, `duck ${duringLine} vs ${preLine}`);
  d.run(1500 + 8000, "soldier_with_sponge");
  assert.equal(d.s.timeline.phase, "resonance");
  assert.ok(out(d) > preLine, `rebound ${out(d)} > ${preLine}`);
});

test("abandonment is the peak; surrender thins toward silence across the state", () => {
  const d = driver(M, 4).cmd({ type: "acknowledgeAdvisory" });
  runUntilState(d, "abandonment", "sky").run(20_000, "sky");
  const peak = out(d);
  assert.ok(peak > 0.9, `abandonment peak ${peak}`);
  runUntilState(d, "surrender", "sky");
  d.run(5000, "sky");
  const early = out(d);
  runUntilState(d, "release", "sky");
  assert.ok(d.s.rt.intensity < early * 0.5, "thinned out by the end of surrender");
});

test("the player's intensity ceiling is a hard cap on output", () => {
  const d = driver(M, 3, { intensityCeiling: 0.4 }).cmd({ type: "acknowledgeAdvisory" });
  runUntilState(d, "forgiveness", "crowd").run(30_000, "crowd");
  assert.ok(out(d) <= 0.4 + 1e-9);
});

test("voice count respects the intensity-scaled cap and fragments do not immediately repeat", () => {
  const d = inForgiveness().run(30_000, "crowd");
  const frags = d.events.filter((e) => e.type === "rt.fragment").map((e) => (e as { fragmentId: string }).fragmentId);
  for (let i = 1; i < frags.length; i++) assert.notEqual(frags[i], frags[i - 1]);
  // sample the cap continuously
  for (let i = 0; i < 2000; i++) {
    d.run(TICK_MS, "crowd");
    const f = selectFrame(M, d.s, L).audio.racingThoughts;
    assert.ok(f.activeVoices.length <= M.racingThoughts.maxVoices);
  }
});

test("fragments only come from the current state's pool", () => {
  const d = driver(M, 12).cmd({ type: "acknowledgeAdvisory" });
  runUntilState(d, "release", "crowd");
  let current = "threshold";
  for (const e of d.events) {
    if (e.type === "timeline.enter") current = e.state;
    if (e.type === "rt.fragment") {
      const f = M.fragments.find((x) => x.id === e.fragmentId)!;
      assert.ok(f.states === "*" || (f.states as string[]).includes(current), `${f.id} in ${current}`);
    }
  }
});
