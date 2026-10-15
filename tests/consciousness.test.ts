import { test } from "node:test";
import assert from "node:assert/strict";
import { exertionDrive } from "../src/consciousness.js";
import { selectFrame } from "../src/frame.js";
import { timelineSignature } from "../src/replay.js";
import { createSession, step, TICK_MS } from "../src/session.js";
import type { Command, SessionEvent, SessionState, TickInput } from "../src/types.js";
import { L, M, runUntilState, driver } from "./helpers.js";

const changes = (ev: SessionEvent[]) => ev.filter((e) => e.type === "consciousness.change") as Extract<SessionEvent, { type: "consciousness.change" }>[];

/** Drive the session tick by tick with explicit head motion / voice. */
function run(s: SessionState, ms: number, input: Omit<TickInput, "commands">, events: SessionEvent[], commands?: Command[]): SessionState {
  for (let t = 0; t < ms; t += TICK_MS) {
    const r = step(M, s, { ...input, ...(t === 0 && commands ? { commands } : {}) });
    s = r.state;
    events.push(...r.events);
  }
  return s;
}

function inForgiveness(seed = 3, settings = {}) {
  const d = driver(M, seed, settings).cmd({ type: "acknowledgeAdvisory" });
  return runUntilState(d, "forgiveness", "horizon");
}

const VIGOROUS = { gaze: "crowd", headDegPerSec: 200, headCmPerSec: 40 } as const;

test("looking is free: gaze direction alone never costs anything, at any dwell, on any target", () => {
  const d = inForgiveness();
  const ev: SessionEvent[] = [];
  let s = d.s;
  for (const g of M.gazeTargets) s = run(s, 3_000, { gaze: g.id }, ev);
  assert.equal(s.consciousness.exertion, 0);
  assert.equal(changes(ev).length, 0);
  assert.equal(exertionDrive(M, { gaze: "crowd", headDegPerSec: 25, headCmPerSec: 8, voice: 0.1 }), 0, "ordinary looking around is inside the deadzones");
});

test("moving much loses consciousness; moderate movement and stillness do not", () => {
  const ev: SessionEvent[] = [];
  const s = run(inForgiveness().s, 12_000, VIGOROUS, ev);
  const lost = changes(ev).find((e) => !e.conscious);
  assert.ok(lost && lost.cause === "exertion", "vigorous movement takes consciousness within 12 s");
  assert.ok(s.consciousness.swoon);

  const calmEv: SessionEvent[] = [];
  const calm = run(inForgiveness().s, 60_000, { gaze: "crowd", headDegPerSec: 45, headCmPerSec: 12 }, calmEv);
  assert.equal(changes(calmEv).length, 0, "gentle movement for a minute never swoons");
  assert.ok(calm.consciousness.exertion < 1);
});

test("talking much loses consciousness too; the setting turns it off; Witness never builds exertion", () => {
  const ev: SessionEvent[] = [];
  run(inForgiveness().s, 15_000, { gaze: "horizon", voice: 1 }, ev);
  assert.ok(changes(ev).some((e) => !e.conscious && e.cause === "exertion"));

  const offEv: SessionEvent[] = [];
  const off = run(inForgiveness(3, { swoon: false }).s, 20_000, VIGOROUS, offEv);
  assert.equal(changes(offEv).length, 0);
  assert.equal(off.consciousness.swoon, null);

  const wEv: SessionEvent[] = [];
  const w = run(inForgiveness().s, 20_000, VIGOROUS, wEv, [{ type: "setMode", mode: "witness" }]);
  assert.equal(w.consciousness.exertion, 0);
});

test("losing consciousness lets the storm go at once, like Witness, and decays to silence", () => {
  const ev: SessionEvent[] = [];
  let s = run(inForgiveness().s, 8_000, { gaze: "crowd" }, ev); // storm up, no movement
  let before = 0;
  for (let i = 0; i < 2000 && !s.consciousness.swoon; i++) {
    before = selectFrame(M, s, L).audio.racingThoughts.output;
    s = run(s, TICK_MS, VIGOROUS, ev);
  }
  assert.ok(s.consciousness.swoon);
  const after = selectFrame(M, s, L).audio.racingThoughts.output;
  assert.ok(after <= before * M.racingThoughts.witnessEchoScale + 0.02, `${before} -> ${after}`);
  s = run(s, 8_000, { gaze: "crowd" }, ev);
  assert.ok(selectFrame(M, s, L).audio.racingThoughts.output < 0.02);
});

test("unconscious: the moment is told Gospel by Gospel, from where his followers stood", () => {
  const ev: SessionEvent[] = [];
  let s = run(inForgiveness().s, 12_000, VIGOROUS, ev);
  assert.ok(s.consciousness.swoon);
  s = run(s, 30_000, { gaze: "crowd" }, ev); // rest; the tellings play
  const tellings = ev.filter((e) => e.type === "cue.start" && e.kind === "telling").map((e) => (e as { cueId: string }).cueId);
  assert.ok(tellings.length >= 2, `tellings heard: ${tellings.join(",")}`);
  assert.ok(tellings.every((id) => id.startsWith("t_forgiveness_")), "only the current moment is told");
  const order = M.cues.filter((c) => c.kind === "telling" && c.trigger.type === "telling" && c.trigger.state === "forgiveness").map((c) => c.id);
  assert.deepEqual(tellings, order.slice(0, tellings.length), "in manifest order");
  // gaze while unconscious never triggers the embodied scene voices or tiered lines
  assert.ok(!ev.some((e) => e.type === "cue.start" && e.cueId === "priests_trusted"));
});

test("the view follows the telling's vantage: John beside the cross, the others afar off", () => {
  const ev: SessionEvent[] = [];
  let s = run(inForgiveness().s, 12_000, VIGOROUS, ev);
  for (let i = 0; i < 3000 && !(s.cues.active && s.cues.active.id.startsWith("t_")); i++) s = run(s, TICK_MS, { gaze: "crowd" }, ev);
  const f = selectFrame(M, s, L);
  const c = M.cues.find((x) => x.id === s.cues.active!.id)!;
  assert.equal(f.view, "telling");
  assert.equal(f.viewVantage, c.vantage);
  assert.equal(f.cue!.speakerLabel, L[`ui.telling.${c.telling}`]);
  for (const t of M.cues.filter((x) => x.kind === "telling")) assert.equal(t.vantage, t.telling === "john" ? "beloved_disciple" : "women_afar", t.id);
});

test("unconscious, he cannot choose to speak; a saying that comes due wakes him to speak it", () => {
  const ev: SessionEvent[] = [];
  let s = inForgiveness().s;
  // rest until shortly before the saying comes due on its own clock, then exhaust him
  const auto = M.timeline.find((x) => x.id === "forgiveness")!.timing.approachAutoMs!;
  s = run(s, auto - 12_000 - s.timeline.phaseElapsedMs, { gaze: "crowd" }, ev);
  for (let i = 0; i < 1100 && !s.consciousness.swoon; i++) s = run(s, TICK_MS, VIGOROUS, ev);
  assert.ok(s.consciousness.swoon, "unconscious before the saying is due");
  s = run(s, TICK_MS, { gaze: "crowd" }, ev, [{ type: "triggerSaying", via: "speech" }]);
  assert.ok(ev.some((e) => e.type === "command.rejected" && e.reason === "unconscious"));
  for (let i = 0; i < 2000 && !ev.some((e) => e.type === "saying.trigger"); i++) s = run(s, TICK_MS, { gaze: "crowd" }, ev);
  const sayingAt = ev.findIndex((e) => e.type === "saying.trigger");
  assert.ok(sayingAt >= 0 && (ev[sayingAt] as { via: string }).via === "auto");
  const wake = ev[sayingAt + 1] as Extract<SessionEvent, { type: "consciousness.change" }>;
  assert.equal(wake.type, "consciousness.change");
  assert.equal(wake.conscious, true);
  assert.equal(wake.cause, "saying");
  assert.equal(s.consciousness.swoon, null);
  assert.ok(s.consciousness.exertion > M.exertion.wakeAt, "and speaking costs him");
});

test("rest brings consciousness back, never before minSwoonMs, never mid-telling", () => {
  const ev: SessionEvent[] = [];
  let s = run(inForgiveness().s, 12_000, VIGOROUS, ev);
  const since = s.consciousness.swoon!.since;
  s = run(s, 60_000, { gaze: "cross" }, ev);
  const woke = changes(ev).find((e) => e.conscious);
  assert.ok(woke, "wakes after rest");
  assert.ok(woke!.t - since >= M.exertion.minSwoonMs);
  const tellingEnds = ev
    .filter((e) => e.type === "cue.start" && e.kind === "telling")
    .map((e) => ({ start: e.t, end: e.t + M.cues.find((c) => c.id === (e as { cueId: string }).cueId)!.durationMs }));
  if (woke!.cause === "recovered") assert.ok(tellingEnds.every((x) => woke!.t <= x.start || woke!.t >= x.end), "not mid-telling");
});

test("consciousness never changes the timeline (I1, I2): same entries at the same times", () => {
  const plain = driver(M, 9).cmd({ type: "acknowledgeAdvisory" });
  runUntilState(plain, "contemplation", null);
  let s = createSession(M, 9);
  const ev: SessionEvent[] = [];
  s = run(s, TICK_MS, { gaze: null }, ev, [{ type: "acknowledgeAdvisory" }]);
  for (let i = 0; i < 100_000 && M.timeline[s.timeline.index]!.id !== "contemplation"; i++) {
    const vigorous = Math.floor(i / 1500) % 3 === 0; // bursts of movement throughout
    s = run(s, TICK_MS, vigorous ? VIGOROUS : { gaze: "crowd" }, ev);
  }
  assert.ok(changes(ev).length >= 4, "swooned and woke several times");
  const enters = (e: SessionEvent[]) => e.filter((x) => x.type === "timeline.enter").map((x) => `${(x as { state: string }).state}@${x.t}`);
  assert.deepEqual(timelineSignature(ev), timelineSignature(plain.events));
  // auto-advance times can only match if no saying was triggered manually; sayings auto-fire on the same clock
  assert.deepEqual(enters(ev), enters(plain.events));
  assert.equal(s.consciousness.swoon, null, "death clears unconsciousness (Witness from Release)");
});

test("Comfort, Witness, Pause and Exit still work within one tick while unconscious", () => {
  const ev: SessionEvent[] = [];
  let s = run(inForgiveness().s, 12_000, VIGOROUS, ev);
  assert.ok(s.consciousness.swoon);
  s = run(s, TICK_MS, { gaze: "crowd" }, ev, [{ type: "setMode", mode: "witness" }, { type: "setComfort", on: true }, { type: "pause" }]);
  assert.equal(s.mode, "witness");
  assert.equal(s.comfort, true);
  assert.equal(s.paused, true);
  assert.equal(selectFrame(M, s, L).view, "witness");
  s = run(s, TICK_MS, { gaze: "crowd" }, ev, [{ type: "exit" }]);
  assert.equal(s.exited, true);
});
