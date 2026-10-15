import { test } from "node:test";
import assert from "node:assert/strict";
import { lastWords, maxAutoRunMs } from "../src/data.js";
import { validateManifest } from "../src/validate.js";
import { SAYING_IDS, TIMELINE } from "../src/types.js";
import { L, M } from "./helpers.js";

const clone = <T>(x: T): T => JSON.parse(JSON.stringify(x)) as T;

test("manifest validates against the locale with no errors", () => {
  const r = validateManifest(M, L);
  assert.deepEqual(r.errors, []);
  assert.equal(r.ok, true);
});

test("timeline is in canonical order and lastWords follows the traditional sequence", () => {
  assert.deepEqual(M.timeline.map((s) => s.id), [...TIMELINE]);
  assert.deepEqual(lastWords(M).map((s) => s.id), [...SAYING_IDS]);
});

test("every saying cites its Gospel(s) and every cited source resolves with KJV text", () => {
  for (const s of lastWords(M)) {
    assert.ok(s.sources.length > 0, s.id);
    for (const id of s.sourceIds) {
      const src = M.sources.find((x) => x.id === id);
      assert.ok(src, `${s.id} -> ${id}`);
      assert.ok(src.kjv?.text, `${id} has text`);
    }
  }
});

test("expected Gospel attributions (source fidelity)", () => {
  const cites = Object.fromEntries(lastWords(M).map((s) => [s.id, s.sources]));
  assert.deepEqual(cites, {
    forgiveness: ["Luke 23:34"],
    paradise: ["Luke 23:43"],
    mother: ["John 19:26-27"],
    abandonment: ["Matthew 27:46", "Mark 15:34"],
    thirst: ["John 19:28"],
    completion: ["John 19:30"],
    surrender: ["Luke 23:46"],
  });
});

test("Luke 23:34 is a textual variant with visible manuscript notes", () => {
  const fg = lastWords(M).find((s) => s.id === "forgiveness")!;
  assert.equal(fg.evidence, "textual-variant");
  assert.ok(fg.sourceNote);
  const src = M.sources.find((x) => x.id === "luke-23-34")!;
  assert.equal(src.evidence, "textual-variant");
  assert.ok((src.manuscriptNotes ?? []).length >= 2);
});

test("all internal-monologue fragments are disclosed as artistic interpretation", () => {
  assert.equal(M.racingThoughts.evidence, "artistic-interpretation");
  for (const f of M.fragments) assert.equal(f.evidence, "artistic-interpretation", f.id);
});

test("embodiment is locked after death; the spear is witnessed from John's vantage", () => {
  const i = TIMELINE.indexOf("release");
  for (const s of M.timeline.slice(i)) {
    assert.equal(s.allowEmbodiment, false, s.id);
    assert.equal(s.forcedMode, "witness", s.id);
  }
  assert.equal(M.timeline.find((s) => s.id === "spear")!.forcedVantage, "beloved_disciple");
});

test("validator rejects reordering, missing variant marking, bad references and unsafe comfort gain", () => {
  const swapped = clone(M);
  [swapped.timeline[2], swapped.timeline[3]] = [swapped.timeline[3]!, swapped.timeline[2]!];
  assert.ok(validateManifest(swapped, L).errors.some((e) => e.includes("canonical order")));

  const noVariant = clone(M);
  noVariant.timeline.find((s) => s.id === "forgiveness")!.saying!.evidence = "gospel-text";
  assert.ok(!validateManifest(noVariant, L).ok);

  const badRef = clone(M);
  badRef.timeline.find((s) => s.id === "mother")!.saying!.sourceIds = ["john-99-99"];
  assert.ok(validateManifest(badRef, L).errors.some((e) => e.includes("unknown source")));

  const loud = clone(M);
  loud.racingThoughts.comfortGain = 0.5;
  assert.ok(validateManifest(loud, L).errors.some((e) => e.includes("comfortGain")));

  const embodiedSpear = clone(M);
  embodiedSpear.timeline.find((s) => s.id === "spear")!.allowEmbodiment = true;
  assert.ok(!validateManifest(embodiedSpear, L).ok);

  const stressMismatch = clone(M);
  stressMismatch.timeline.find((s) => s.id === "abandonment")!.saying!.stressLevel = "low";
  assert.ok(validateManifest(stressMismatch, L).errors.some((e) => e.includes("disagrees")));
});

test("auto-run bound is finite and under 12 minutes", () => {
  const ms = maxAutoRunMs(M);
  assert.ok(ms > 0 && ms < 12 * 60_000, String(ms));
});

test("validator bounds the Racing Thoughts mixer parameters the core reads (fragment rate, duck, affinity)", () => {
  assert.deepEqual(validateManifest(M, L).errors, []);
  const bad = clone(M);
  bad.racingThoughts.fragmentRate.gamma = 0;
  bad.racingThoughts.externalDuck.maxDb = 3;
  bad.racingThoughts.affinityMultiplier = 0.5;
  bad.racingThoughts.maxVoices = 2.5;
  const errors = validateManifest(bad, L).errors;
  for (const k of ["fragmentRate", "externalDuck", "affinityMultiplier", "must be integers"]) {
    assert.ok(errors.some((e) => e.includes(k)), `missing ${k} error: ${errors.join("; ")}`);
  }
});
