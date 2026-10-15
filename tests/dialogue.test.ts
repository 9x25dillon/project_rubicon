import { test } from "node:test";
import assert from "node:assert/strict";
import { cueFrame, selectFrame } from "../src/frame.js";
import { dwellTierId } from "../src/racingThoughts.js";
import { timelineSignature } from "../src/replay.js";
import { TICK_MS } from "../src/session.js";
import type { SessionEvent } from "../src/types.js";
import { driver, L, M, runUntilState } from "./helpers.js";

const cueStarts = (events: SessionEvent[]) => events.filter((e) => e.type === "cue.start") as Extract<SessionEvent, { type: "cue.start" }>[];
const fragmentEvents = (events: SessionEvent[]) => events.filter((e) => e.type === "rt.fragment") as Extract<SessionEvent, { type: "rt.fragment" }>[];
const fragment = (id: string) => M.fragments.find((f) => f.id === id)!;

function inState(state: string, seed = 3, gaze: string | null = "horizon") {
  const d = driver(M, seed).cmd({ type: "acknowledgeAdvisory" });
  return runUntilState(d, state, gaze);
}

test("dwell tiers escalate with continuous gaze: every tiered line plays only in its own tier (glance -> extreme)", () => {
  const d = inState("forgiveness");
  const t0 = d.s.t;
  const from = d.events.length;
  d.run(40_000, "crowd");
  const tiered = fragmentEvents(d.events.slice(from)).filter((e) => fragment(e.fragmentId).gaze);
  assert.ok(tiered.length > 4, `expected several tiered crowd lines, got ${tiered.length}`);
  for (const e of tiered) {
    const f = fragment(e.fragmentId);
    assert.equal(f.gaze!.target, "crowd");
    // dwell at spawn: the gaze has rested on the crowd since t0 (spawn is stamped at the end of its tick)
    assert.equal(f.gaze!.tier, dwellTierId(M, e.t - t0), `${f.id} at dwell ${e.t - t0}`);
  }
  const tiers = new Set(tiered.map((e) => fragment(e.fragmentId).gaze!.tier));
  assert.ok(tiers.has("extreme"), `a long stare reaches the extreme tier (${[...tiers].join(",")})`);
});

test("tiered lines answer the embodied gaze only: never in Witness, never for another target", () => {
  const w = inState("forgiveness").cmd({ type: "setMode", mode: "witness" });
  const from = w.events.length;
  w.run(30_000, "crowd");
  assert.equal(fragmentEvents(w.events.slice(from)).filter((e) => fragment(e.fragmentId).gaze).length, 0);
  const h = inState("forgiveness");
  const fromH = h.events.length;
  h.run(30_000, "horizon");
  assert.ok(fragmentEvents(h.events.slice(fromH)).every((e) => !fragment(e.fragmentId).gaze || fragment(e.fragmentId).gaze!.target === "horizon"));
});

test("Paradise: railing, rebuke and request are heard, in Luke's order, before the saying can answer (embodied and witness)", () => {
  for (const mode of ["embodiment", "witness"] as const) {
    const d = inState("mother", 4, "road");
    // reach paradise via a fresh run so the mode applies throughout; spam the trigger so the saying comes at the earliest moment
    const p = driver(M, 4).cmd({ type: "acknowledgeAdvisory" });
    if (mode === "witness") p.cmd({ type: "setMode", mode: "witness" });
    runUntilState(p, "paradise", "road");
    for (let i = 0; i < 2000 && !p.events.some((e) => e.type === "saying.trigger" && e.saying === "paradise"); i++) {
      p.run(TICK_MS, "road", [{ type: "triggerSaying", via: "select" }]);
    }
    const order = p.events
      .filter((e) => (e.type === "cue.start" && e.cueId.startsWith("paradise_")) || (e.type === "saying.trigger" && e.saying === "paradise"))
      .map((e) => (e.type === "cue.start" ? e.cueId : "SAYING"));
    assert.deepEqual(order, ["paradise_rail", "paradise_rebuke", "paradise_request", "SAYING"], mode);
    void d;
  }
});

test("recorded responses follow their sayings: Elias after the cry, the sponge after thirst, the centurion after death", () => {
  const d = driver(M, 6).cmd({ type: "acknowledgeAdvisory" });
  runUntilState(d, "contemplation", null);
  const seq = d.events.filter((e) => e.type === "cue.start" || e.type === "saying.trigger" || e.type === "timeline.enter");
  const idx = (pred: (e: SessionEvent) => boolean) => seq.findIndex(pred);
  const cry = idx((e) => e.type === "saying.trigger" && e.saying === "abandonment");
  const elias = idx((e) => e.type === "cue.start" && e.cueId === "elias_calleth");
  const letBe = idx((e) => e.type === "cue.start" && e.cueId === "elias_let_be");
  const thirst = idx((e) => e.type === "saying.trigger" && e.saying === "thirst");
  const sponge = idx((e) => e.type === "cue.start" && e.cueId === "thirst_sponge");
  const release = idx((e) => e.type === "timeline.enter" && e.state === "release");
  const centurion = idx((e) => e.type === "cue.start" && e.cueId === "centurion_truly");
  const record = idx((e) => e.type === "cue.start" && e.cueId === "spear_record");
  assert.ok(cry >= 0 && cry < elias && elias < letBe && letBe < thirst, "Elias lines between the cry and thirst");
  assert.ok(thirst < sponge, "the sponge answers I thirst");
  assert.ok(release < centurion && centurion < record, "the centurion after death; the witness record during the spear");
});

test("a held gaze makes the scene speak, once, in the words the Gospels give that group", () => {
  const d = inState("forgiveness");
  const from = d.events.length;
  d.run(2400, "crowd");
  assert.equal(cueStarts(d.events.slice(from)).length, 0, "not before 2.5 s of dwell");
  d.run(300, "crowd");
  const first = cueStarts(d.events.slice(from));
  assert.deepEqual(first.map((c) => c.cueId), ["passersby_temple"]);
  assert.equal(first[0]!.speaker, "passersby");
  const f = selectFrame(M, d.s, L).cue!;
  assert.equal(f.subtitle, L["cue.passersby_temple"]);
  assert.equal(f.origin, "road");
  assert.equal(f.speakerLabel, "Passersby");
  d.run(30_000, "crowd");
  const ids = cueStarts(d.events.slice(from)).map((c) => c.cueId);
  assert.ok(ids.includes("priests_saved_others") && ids.includes("priests_trusted"));
  assert.equal(new Set(ids).size, ids.length, "each cue at most once");
});

test("the saying has the floor: a cue still sounding when the saying begins is cut", () => {
  const d = inState("forgiveness");
  d.run(8_000, "horizon"); // past approachMinMs
  d.run(2_600, "crowd"); // passersby cue starts
  assert.ok(cueFrame(M, d.s, L), "cue sounding");
  d.cmd({ type: "triggerSaying", via: "select" });
  assert.equal(d.s.timeline.phase, "utterance");
  assert.equal(cueFrame(M, d.s, L), null);
});

test("history is narrated only from outside (Witness), with the source's rendering disclosed", () => {
  const e = inState("forgiveness");
  const fromE = e.events.length;
  e.run(20_000, "soldiers");
  assert.ok(!cueStarts(e.events.slice(fromE)).some((c) => c.kind === "narration"));
  const w = inState("forgiveness").cmd({ type: "setMode", mode: "witness" });
  const fromW = w.events.length;
  w.run(3_000, "soldiers");
  assert.deepEqual(cueStarts(w.events.slice(fromW)).map((c) => c.cueId), ["n_soldiers_varus"]);
  const f = selectFrame(M, w.s, L);
  assert.equal(f.cue!.rendering, "paraphrase");
  assert.equal(f.cue!.speakerLabel, "Narrator");
  const item = f.evidenceOverlay.find((i) => i.citation.startsWith("Josephus, Jewish Antiquities 17.295"));
  assert.ok(item && item.sourceType === "ancient-text" && item.category === "historical-inference");
});

test("cues never change the timeline (I7): the same entries at the same times whatever the gaze", () => {
  const a = driver(M, 9).cmd({ type: "acknowledgeAdvisory" });
  runUntilState(a, "contemplation", null);
  const b = driver(M, 9).cmd({ type: "acknowledgeAdvisory" });
  runUntilState(b, "contemplation", "crowd");
  const enters = (d: typeof a) => d.events.filter((e) => e.type === "timeline.enter").map((e) => `${(e as { state: string }).state}@${e.t}`);
  assert.deepEqual(enters(a), enters(b));
  assert.deepEqual(timelineSignature(a.events), timelineSignature(b.events));
  assert.ok(cueStarts(b.events).length > cueStarts(a.events).length);
});

test("exit silences a sounding cue", () => {
  const d = inState("forgiveness").run(2_700, "crowd");
  assert.ok(d.s.cues.active);
  d.cmd({ type: "exit" });
  assert.equal(d.s.cues.active, null);
});

/* Content integrity: quotations are checked against the cited texts, mechanically. */

const norm = (x: string) => x.toLowerCase().replace(/[‘’]/g, "'").replace(/\s+/g, " ").trim();
const citedTexts = (ids: string[], allowParaphrase: boolean) =>
  ids.flatMap((id) => {
    const s = M.sources.find((x) => x.id === id)!;
    const out: string[] = [];
    if (s.kjv) out.push(s.kjv.text);
    if (s.excerpt && (allowParaphrase || s.excerpt.rendering !== "paraphrase")) out.push(s.excerpt.text);
    return out;
  });

test("every spoken or scripture-read cue is verbatim KJV from its cited passages (ellipses mark omissions)", () => {
  for (const c of M.cues.filter((x) => x.kind !== "sound" && (x.evidence === "gospel-text" || x.evidence === "textual-variant"))) {
    const corpus = norm(citedTexts(c.sourceIds, false).join(" "));
    for (const seg of L[c.textKey]!.split(" ... ")) {
      const s = norm(seg).replace(/[.?!,;:]+$/, "");
      assert.ok(corpus.includes(s), `${c.id}: "${seg}" not found in ${c.sourceIds.join(", ")}`);
    }
  }
});

test("every quotation inside a narration appears in a cited source, and paraphrased sources are never quoted", () => {
  const quote = /(?:^|[\s(:])'(.+?)'(?=[\s.,;:)]|$)/g;
  for (const c of M.cues.filter((x) => (x.kind === "narration" || x.kind === "telling") && x.evidence !== "gospel-text" && x.evidence !== "textual-variant")) {
    const corpus = norm(citedTexts(c.sourceIds, false).join(" "));
    const text = L[c.textKey]!;
    let n = 0;
    for (const m of text.matchAll(quote)) {
      n++;
      const q = norm(m[1]!).replace(/[.?!,;:]+$/, "");
      assert.ok(corpus.includes(q), `${c.id}: quoted "${m[1]}" is not in a cited (non-paraphrase) source`);
    }
    void n;
  }
});

test("internal monologue never names a people collectively; crowd lines name the Gospel groups", () => {
  for (const f of M.fragments) {
    if (!f.textKey) continue;
    assert.ok(!/\bjews\b/i.test(L[f.textKey]!), `${f.id}: internal monologue must not speak of "the Jews"`);
  }
  const crowdLines = M.fragments.filter((f) => f.gaze?.target === "crowd").map((f) => L[f.textKey!]!).join(" ");
  assert.match(crowdLines, /they that pass by/);
  assert.match(crowdLines, /chief priests, with the scribes/);
  assert.match(crowdLines, /not all of them/);
});

test("every gaze target has a full set of dwell tiers except the low-stress anchors (own hands, horizon)", () => {
  const tiers = M.racingThoughts.dwellTiers.map((t) => t.id);
  for (const g of M.gazeTargets) {
    if (g.id === "own_hands" || g.id === "horizon") continue;
    const have = new Set(M.fragments.filter((f) => f.gaze?.target === g.id).map((f) => f.gaze!.tier));
    assert.deepEqual([...tiers].filter((t) => !have.has(t)), [], g.id);
  }
});
