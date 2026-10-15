import { test } from "node:test";
import assert from "node:assert/strict";
import { maxAutoRunMs } from "../src/data.js";
import { selectFrame } from "../src/frame.js";
import { runReplay, timelineSignature } from "../src/replay.js";
import { TICK_MS } from "../src/session.js";
import { TIMELINE } from "../src/types.js";
import { driver, fuzzInputs, L, M, runUntilState } from "./helpers.js";

const ENTERED_AFTER_GATE = TIMELINE.slice(1);

test("gate waits for consent; nothing advances without it", () => {
  const d = driver().run(60_000, "crowd");
  assert.equal(M.timeline[d.s.timeline.index]!.id, "threshold");
  assert.equal(d.s.rt.intensity, 0);
});

test("no soft-lock: zero input reaches contemplation within the computed bound (I4)", () => {
  const d = driver().cmd({ type: "acknowledgeAdvisory" });
  d.run(maxAutoRunMs(M) + 1000, null);
  assert.equal(M.timeline[d.s.timeline.index]!.id, "contemplation");
  assert.deepEqual(timelineSignature(d.events), [...ENTERED_AFTER_GATE]);
  const triggers = d.events.filter((e) => e.type === "saying.trigger");
  assert.equal(triggers.length, 7);
  assert.ok(triggers.every((e) => e.type === "saying.trigger" && e.via === "auto"));
});

test("no soft-lock at maximum internal intensity (I5: racing thoughts never gate progress)", () => {
  const d = driver().cmd({ type: "acknowledgeAdvisory" });
  // stare at the most stressful target in every state
  d.run(maxAutoRunMs(M) + 1000, "crowd");
  assert.equal(M.timeline[d.s.timeline.index]!.id, "contemplation");
});

test("sayings can be triggered early by select or speech, never before approachMinMs", () => {
  const d = driver().cmd({ type: "acknowledgeAdvisory" });
  runUntilState(d, "forgiveness", "crowd");
  d.cmd({ type: "triggerSaying", via: "speech" });
  assert.ok(d.events.some((e) => e.type === "command.rejected" && e.reason.startsWith("too early")));
  d.run(8000, "crowd");
  d.cmd({ type: "triggerSaying", via: "speech" });
  const trig = d.events.filter((e) => e.type === "saying.trigger");
  assert.equal(trig.length, 1);
  assert.equal(d.s.timeline.phase, "utterance");
});

test("historical outcome is immutable under random input, including trigger spam (I1)", () => {
  for (const seed of [1, 2, 3, 4, 5, 6, 7, 8]) {
    const inputs = fuzzInputs(seed, Math.ceil((maxAutoRunMs(M) + 2000) / TICK_MS), { includeTimelineCommands: true });
    const r = runReplay(M, seed, inputs);
    assert.deepEqual(timelineSignature(r.events), [...ENTERED_AFTER_GATE], `seed ${seed}`);
    // indices are strictly increasing; never re-entered
    const idx = timelineSignature(r.events).map((id) => TIMELINE.indexOf(id as (typeof TIMELINE)[number]));
    for (let i = 1; i < idx.length; i++) assert.equal(idx[i], idx[i - 1]! + 1);
  }
});

test("mode, layer, comfort, evidence and settings never move the timeline (I2)", () => {
  const ticks = Math.ceil((maxAutoRunMs(M) + 2000) / TICK_MS);
  const baseline = runReplay(M, 11, fuzzInputs(999, ticks, { includeTimelineCommands: false }).map((i) => ({ gaze: i.gaze, ...(i.commands?.[0]?.type === "acknowledgeAdvisory" ? { commands: i.commands } : {}) })));
  const noisy = runReplay(M, 11, fuzzInputs(999, ticks, { includeTimelineCommands: false }));
  const enterTimes = (r: typeof baseline) => r.events.filter((e) => e.type === "timeline.enter").map((e) => [e.t, (e as { state: string }).state]);
  assert.deepEqual(enterTimes(noisy), enterTimes(baseline));
});

test("witness and exit are accepted in every state (I3)", () => {
  for (const target of TIMELINE) {
    const d = driver().cmd({ type: "acknowledgeAdvisory" });
    if (target !== "threshold") runUntilState(d, target);
    else {
      const g = driver();
      g.cmd({ type: "setMode", mode: "witness" });
      assert.equal(g.s.mode, "witness");
      g.cmd({ type: "exit" });
      assert.equal(g.s.exited, true);
      continue;
    }
    d.cmd({ type: "setMode", mode: "witness", vantage: "mary" });
    assert.equal(d.s.mode, "witness", target);
    assert.ok(!d.events.some((e) => e.type === "command.rejected" && e.command === "setMode"), target);
    d.cmd({ type: "exit" });
    assert.equal(d.s.exited, true, target);
    assert.equal(selectFrame(M, d.s, L).audio.racingThoughts.output, 0, target);
  }
});

test("witness is non-punitive: returning to embodiment keeps the timeline where it was", () => {
  const d = driver().cmd({ type: "acknowledgeAdvisory" });
  runUntilState(d, "forgiveness", "crowd").run(5000, "crowd");
  const before = { ...d.s.timeline };
  d.cmd({ type: "setMode", mode: "witness" });
  d.cmd({ type: "setMode", mode: "embodiment" });
  assert.equal(d.s.timeline.index, before.index);
  assert.equal(d.s.timeline.phase, before.phase);
  assert.ok(d.s.timeline.phaseElapsedMs > before.phaseElapsedMs);
});

test("after death the player is moved to witness and embodiment is refused; spear uses John's vantage", () => {
  const d = driver().cmd({ type: "acknowledgeAdvisory" });
  runUntilState(d, "release");
  assert.equal(d.s.mode, "witness");
  assert.ok(d.events.some((e) => e.type === "mode.change" && e.forced));
  d.cmd({ type: "setMode", mode: "embodiment" });
  assert.equal(d.s.mode, "witness");
  assert.ok(d.events.some((e) => e.type === "command.rejected" && e.reason.includes("embodiment unavailable")));
  runUntilState(d, "spear");
  assert.equal(d.s.vantage, "beloved_disciple");
});

test("pause freezes the timeline clock and silences output", () => {
  const d = driver().cmd({ type: "acknowledgeAdvisory" });
  d.run(3000, "crowd");
  const snap = { ...d.s.timeline };
  d.cmd({ type: "pause" });
  d.run(60_000, "crowd");
  assert.deepEqual(d.s.timeline, snap);
  assert.equal(selectFrame(M, d.s, L).audio.racingThoughts.output, 0);
  d.cmd({ type: "resume" });
  d.run(1000, "crowd");
  assert.ok(d.s.timeline.phaseElapsedMs > snap.phaseElapsedMs);
});

test("comfort or reduced effects replace the elevation tilt with a fade", () => {
  const d = driver().cmd({ type: "acknowledgeAdvisory" });
  assert.equal(selectFrame(M, d.s, L).visual.elevationTransition, "slow-tilt");
  d.cmd({ type: "setComfort", on: true });
  assert.equal(selectFrame(M, d.s, L).visual.elevationTransition, "fade-settle");
  const r = driver(M, 1, { reducedEffects: true }).cmd({ type: "acknowledgeAdvisory" });
  assert.equal(selectFrame(M, r.s, L).visual.elevationTransition, "fade-settle");
});
