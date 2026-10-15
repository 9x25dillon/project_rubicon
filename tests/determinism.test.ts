import { test } from "node:test";
import assert from "node:assert/strict";
import { maxAutoRunMs } from "../src/data.js";
import { runReplay, timelineSignature } from "../src/replay.js";
import { createSession, step, TICK_MS } from "../src/session.js";
import { fuzzInputs, M } from "./helpers.js";

const TICKS = Math.ceil((maxAutoRunMs(M) + 2000) / TICK_MS);

test("same seed and input stream produce identical state and event hashes (I6)", () => {
  const inputs = fuzzInputs(77, TICKS, { includeTimelineCommands: true });
  const a = runReplay(M, 1234, inputs);
  const b = runReplay(M, 1234, inputs);
  assert.equal(a.stateHash, b.stateHash);
  assert.equal(a.eventHash, b.eventHash);
});

test("a different seed changes the fragment stream but never the timeline", () => {
  const inputs = fuzzInputs(77, TICKS, { includeTimelineCommands: false });
  const a = runReplay(M, 1, inputs);
  const b = runReplay(M, 2, inputs);
  assert.notEqual(a.eventHash, b.eventHash);
  assert.deepEqual(timelineSignature(a.events), timelineSignature(b.events));
  const enter = (r: typeof a) => r.events.filter((e) => e.type === "timeline.enter").map((e) => e.t);
  assert.deepEqual(enter(a), enter(b));
});

const deepFreeze = <T>(o: T): T => {
  if (o && typeof o === "object" && !Object.isFrozen(o)) {
    Object.freeze(o);
    for (const v of Object.values(o as object)) deepFreeze(v);
  }
  return o;
};

test("the reducer never mutates its input state or the manifest (frozen inputs, strict mode)", () => {
  const m = deepFreeze(JSON.parse(JSON.stringify(M)) as typeof M);
  let s = deepFreeze(createSession(m, 9));
  for (const input of fuzzInputs(5, 20_000, { includeTimelineCommands: true })) {
    s = deepFreeze(step(m, s, input).state);
  }
  assert.ok(s.t > 0);
});
