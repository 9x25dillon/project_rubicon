/**
 * Manifest validation: structural shape + semantic invariants.
 * Dependency-free so the same rules can be ported to a UE5 editor validator (UEditorValidatorBase).
 */
import {
  CUE_KINDS,
  CUE_MODES,
  EVIDENCE_CATEGORIES,
  NON_SCRIPTURAL_SOURCE_TYPES,
  RENDERINGS,
  SOURCE_TYPES,
  PHASES_BY_KIND,
  SAYING_IDS,
  STRESS_LEVELS,
  TIMELINE,
  WITNESS_VANTAGES,
  type EpisodeManifest,
  type Locale,
  type TimelineStateDef,
} from "./types.js";

export interface ValidationResult {
  ok: boolean;
  errors: string[];
  warnings: string[];
}

const isObj = (v: unknown): v is Record<string, unknown> => typeof v === "object" && v !== null && !Array.isArray(v);
const isStr = (v: unknown): v is string => typeof v === "string" && v.length > 0;
const isNum = (v: unknown): v is number => typeof v === "number" && Number.isFinite(v);
const in01 = (v: unknown): boolean => isNum(v) && v >= 0 && v <= 1;

export function validateManifest(raw: unknown, locale?: Locale): ValidationResult {
  const errors: string[] = [];
  const warnings: string[] = [];
  const err = (m: string) => errors.push(m);

  if (!isObj(raw)) return { ok: false, errors: ["manifest: not an object"], warnings };
  const m = raw as unknown as EpisodeManifest;

  for (const k of ["schemaVersion", "id", "title", "edition", "harmonizationNote"] as const) {
    if (!isStr(m[k])) err(`manifest.${k}: required string`);
  }
  for (const k of ["cast", "sources", "gazeTargets", "fragments", "cues", "timeline", "witnessVantages"] as const) {
    if (!Array.isArray(m[k])) err(`manifest.${k}: required array`);
  }
  if (errors.length) return { ok: false, errors, warnings };

  const evidenceOk = (e: unknown) => EVIDENCE_CATEGORIES.includes(e as never);
  const stressOk = (s: unknown) => STRESS_LEVELS.includes(s as never);

  /* sources */
  const sourceIds = new Set<string>();
  for (const s of m.sources) {
    if (!isStr(s.id)) { err("source: missing id"); continue; }
    if (sourceIds.has(s.id)) err(`source ${s.id}: duplicate id`);
    sourceIds.add(s.id);
    if (!isStr(s.citation)) err(`source ${s.id}: missing citation`);
    if (!evidenceOk(s.evidence)) err(`source ${s.id}: invalid evidence "${String(s.evidence)}"`);
    if (s.evidence === "textual-variant" && !(s.manuscriptNotes && s.manuscriptNotes.length)) {
      err(`source ${s.id}: textual-variant requires manuscriptNotes`);
    }
    if (s.kjv && !isStr(s.kjv.text)) err(`source ${s.id}: kjv.text empty`);
    if (!SOURCE_TYPES.includes(s.sourceType)) err(`source ${s.id}: invalid sourceType "${String(s.sourceType)}"`);
    else if (NON_SCRIPTURAL_SOURCE_TYPES.includes(s.sourceType) && s.evidence !== "historical-inference") {
      err(`source ${s.id}: ${s.sourceType} sources must be historical-inference`);
    }
    if (s.excerpt !== undefined && !(isObj(s.excerpt) && isStr(s.excerpt.text) && RENDERINGS.includes(s.excerpt.rendering))) {
      err(`source ${s.id}: excerpt requires text and a valid rendering`);
    }
    if (!s.kjv && !s.excerpt && !isStr(s.note)) err(`source ${s.id}: needs kjv text, an excerpt, or a note`);
  }
  const refOk = (ctx: string, ids: unknown) => {
    if (!Array.isArray(ids)) { err(`${ctx}: sourceIds must be an array`); return; }
    for (const id of ids) if (!sourceIds.has(id as string)) err(`${ctx}: unknown source "${String(id)}"`);
  };

  refOk("setting", m.setting?.sourceIds);
  for (const [i, n] of (m.setting?.reconstructionNotes ?? []).entries()) {
    if (!evidenceOk(n.evidence)) err(`setting.reconstructionNotes[${i}]: invalid evidence`);
    refOk(`setting.reconstructionNotes[${i}]`, n.sourceIds);
  }

  /* cast */
  const castIds = new Set<string>();
  for (const c of m.cast) {
    if (castIds.has(c.id)) err(`cast ${c.id}: duplicate`);
    castIds.add(c.id);
    if (!evidenceOk(c.evidence)) err(`cast ${c.id}: invalid evidence`);
    refOk(`cast ${c.id}`, c.sources);
  }

  /* gaze targets */
  const targetIds = new Set<string>();
  for (const g of m.gazeTargets) {
    if (targetIds.has(g.id)) err(`gazeTarget ${g.id}: duplicate`);
    targetIds.add(g.id);
    if (!stressOk(g.stress)) err(`gazeTarget ${g.id}: invalid stress "${String(g.stress)}"`);
    if (!evidenceOk(g.evidence)) err(`gazeTarget ${g.id}: invalid evidence`);
    refOk(`gazeTarget ${g.id}`, g.sources);
    for (const a of g.actors) if (!castIds.has(a)) err(`gazeTarget ${g.id}: unknown actor "${a}"`);
  }

  /* witness vantages */
  const vantages = new Set(m.witnessVantages.map((v) => v.id));
  for (const v of WITNESS_VANTAGES) if (!vantages.has(v)) err(`witnessVantages: missing "${v}"`);
  for (const v of m.witnessVantages) {
    if (!castIds.has(v.castId)) err(`witnessVantage ${v.id}: unknown cast "${v.castId}"`);
    refOk(`witnessVantage ${v.id}`, v.sourceIds);
  }

  /* racing thoughts model */
  const rt = m.racingThoughts;
  if (!isObj(rt)) err("racingThoughts: required");
  else {
    for (const s of STRESS_LEVELS) if (!isNum(rt.stressWeights?.[s])) err(`racingThoughts.stressWeights.${s}: required`);
    if (!(rt.stressWeights.relief < 0)) err("racingThoughts.stressWeights.relief must be negative");
    const ordered = ["low", "medium", "high", "extreme"] as const;
    for (let i = 1; i < ordered.length; i++) {
      if (!(rt.stressWeights[ordered[i]!] > rt.stressWeights[ordered[i - 1]!])) {
        err(`racingThoughts.stressWeights must increase: ${ordered[i - 1]} < ${ordered[i]}`);
      }
    }
    if (!stressOk(rt.noTargetStress)) err("racingThoughts.noTargetStress invalid");
    for (const k of ["witnessEchoScale", "comfortGain"] as const) if (!in01(rt[k])) err(`racingThoughts.${k}: [0,1]`);
    if (!(rt.comfortGain <= 0.1)) err("racingThoughts.comfortGain must be <= 0.1 (Comfort Mode is near silence)");
    if (!(rt.witnessEchoScale <= 0.5)) err("racingThoughts.witnessEchoScale must be <= 0.5 (Witness must reduce immediately)");
    for (const k of ["dwellTauMs", "witnessEchoTauMs", "comfortReleaseTauMs", "maxVoices", "recentWindow"] as const) {
      if (!(isNum(rt[k]) && rt[k] > 0)) err(`racingThoughts.${k}: positive number`);
    }
    if (!(Number.isInteger(rt.maxVoices) && Number.isInteger(rt.recentWindow))) err("racingThoughts.maxVoices and recentWindow must be integers");
    const fr = rt.fragmentRate as unknown;
    if (!(isObj(fr) && isNum(fr.minPerSec) && fr.minPerSec >= 0 && isNum(fr.maxPerSec) && fr.maxPerSec >= fr.minPerSec && isNum(fr.gamma) && fr.gamma > 0)) {
      err("racingThoughts.fragmentRate: requires 0 <= minPerSec <= maxPerSec and gamma > 0");
    }
    const xd = rt.externalDuck as unknown;
    if (!(isObj(xd) && isNum(xd.startAt) && xd.startAt >= 0 && xd.startAt < 1 && isNum(xd.maxDb) && xd.maxDb <= 0)) {
      err("racingThoughts.externalDuck: requires 0 <= startAt < 1 and maxDb <= 0");
    }
    if (!(isNum(rt.affinityMultiplier) && rt.affinityMultiplier >= 1)) err("racingThoughts.affinityMultiplier: must be >= 1");
    const tiers = rt.dwellTiers as unknown;
    if (!(Array.isArray(tiers) && tiers.length > 0)) err("racingThoughts.dwellTiers: required non-empty array");
    else {
      const tierIds = new Set<unknown>();
      tiers.forEach((t: Record<string, unknown>, i: number) => {
        if (!isStr(t?.id)) err(`racingThoughts.dwellTiers[${i}]: id required`);
        else if (tierIds.has(t.id)) err(`racingThoughts.dwellTiers: duplicate id "${t.id}"`);
        tierIds.add(t?.id);
        if (!isNum(t?.minDwellMs)) err(`racingThoughts.dwellTiers[${i}].minDwellMs: number required`);
        else if (i === 0 && t.minDwellMs !== 0) err("racingThoughts.dwellTiers[0].minDwellMs must be 0");
        else if (i > 0 && !(t.minDwellMs > Number((tiers[i - 1] as Record<string, unknown> | undefined)?.minDwellMs))) err(`racingThoughts.dwellTiers[${i}].minDwellMs must increase`);
      });
    }
    if (rt.evidence !== "artistic-interpretation") err("racingThoughts.evidence must be artistic-interpretation");
    if (!isStr(rt.disclosure)) err("racingThoughts.disclosure: required");
  }

  /* timeline */
  if (m.timeline.length !== TIMELINE.length) err(`timeline: expected ${TIMELINE.length} states, got ${m.timeline.length}`);
  const timelineIds = new Set<string>();
  m.timeline.forEach((s: TimelineStateDef, i) => {
    const expected = TIMELINE[i];
    if (s.id !== expected) err(`timeline[${i}]: expected "${expected}", got "${s.id}" (canonical order is immutable)`);
    timelineIds.add(s.id);
    const ctx = `state ${s.id}`;
    if (!PHASES_BY_KIND[s.kind]) err(`${ctx}: invalid kind "${s.kind}"`);
    if (!evidenceOk(s.evidence)) err(`${ctx}: invalid evidence`);
    refOk(ctx, s.sourceIds);

    const r = s.rt;
    for (const k of ["floor", "ceiling", "utteranceDuck", "postUtteranceFloorBoost", "entryImpulse"] as const) {
      if (!in01(r?.[k])) err(`${ctx}.rt.${k}: [0,1]`);
    }
    if (r?.ceilingEnd !== undefined && !in01(r.ceilingEnd)) err(`${ctx}.rt.ceilingEnd: [0,1]`);
    if (r && r.floor > r.ceiling) err(`${ctx}.rt: floor > ceiling`);
    if (!(isNum(r?.tauUpMs) && r.tauUpMs > 0 && isNum(r?.tauDownMs) && r.tauDownMs > 0)) err(`${ctx}.rt: taus must be positive`);
    if (r && r.tauDownMs < r.tauUpMs) warnings.push(`${ctx}.rt: tauDown < tauUp (looking away will feel fast)`);

    for (const [t, lvl] of Object.entries(s.stressOverrides ?? {})) {
      if (!targetIds.has(t)) err(`${ctx}.stressOverrides: unknown target "${t}"`);
      if (!stressOk(lvl)) err(`${ctx}.stressOverrides.${t}: invalid stress`);
    }

    const tm = s.timing ?? {};
    switch (s.kind) {
      case "elevation":
        if (!(tm.raisingMs! > 0 && tm.settledMs! > 0)) err(`${ctx}: elevation requires raisingMs, settledMs`);
        break;
      case "saying": {
        if (!(tm.approachMinMs! >= 0 && tm.approachAutoMs! > tm.approachMinMs! && tm.utteranceMs! > 0 && tm.resonanceMs! > 0)) {
          err(`${ctx}: saying requires 0 <= approachMinMs < approachAutoMs, utteranceMs > 0, resonanceMs > 0`);
        }
        const sy = s.saying;
        if (!sy) { err(`${ctx}: saying payload missing`); break; }
        if (sy.id !== s.id) err(`${ctx}: saying.id must equal state id`);
        if (!SAYING_IDS.includes(sy.id)) err(`${ctx}: unknown saying id`);
        if (!sy.sources.length) err(`${ctx}: saying needs at least one citation`);
        refOk(`${ctx}.saying`, sy.sourceIds);
        if (sy.sources.length !== sy.sourceIds.length) err(`${ctx}: saying.sources and sourceIds length mismatch`);
        if (!targetIds.has(sy.gazeTarget)) err(`${ctx}: unknown gazeTarget "${sy.gazeTarget}"`);
        if (!stressOk(sy.stressLevel)) err(`${ctx}: invalid saying.stressLevel`);
        const effective = s.stressOverrides?.[sy.gazeTarget] ?? m.gazeTargets.find((g) => g.id === sy.gazeTarget)?.stress;
        if (effective !== sy.stressLevel) err(`${ctx}: saying.stressLevel "${sy.stressLevel}" disagrees with effective target stress "${String(effective)}"`);
        if (!evidenceOk(sy.evidence)) err(`${ctx}: invalid saying.evidence`);
        if (!evidenceOk(sy.pronunciation?.evidence)) err(`${ctx}: invalid pronunciation.evidence`);
        const variantSrc = sy.sourceIds.some((id) => m.sources.find((x) => x.id === id)?.evidence === "textual-variant");
        if (variantSrc && sy.evidence !== "textual-variant") err(`${ctx}: cites a textual-variant source but saying.evidence is "${sy.evidence}"`);
        if (sy.evidence === "textual-variant" && !isStr(sy.sourceNote)) err(`${ctx}: textual-variant saying requires sourceNote`);
        if (locale && !locale[sy.dialogueKey]) err(`${ctx}: locale missing "${sy.dialogueKey}"`);
        break;
      }
      case "timed":
        if (!(tm.runningMs! > 0)) err(`${ctx}: timed requires runningMs`);
        break;
      case "gate":
      case "terminal":
        break;
    }
    if (s.kind !== "saying" && s.saying) err(`${ctx}: only saying states carry a saying`);
    if (s.forcedMode === "witness" && s.allowEmbodiment) err(`${ctx}: forcedMode witness contradicts allowEmbodiment`);
  });

  /* post-death embodiment lock: John 19:33-34 spear must be witnessed, never embodied */
  const spearIdx = TIMELINE.indexOf("spear");
  m.timeline.forEach((s, i) => {
    if (i >= spearIdx && s.allowEmbodiment) err(`state ${s.id}: embodiment must be locked from spear onward`);
  });
  const spear = m.timeline.find((s) => s.id === "spear");
  if (spear && spear.forcedMode !== "witness") err("state spear: forcedMode must be witness");

  /* forgiveness must carry the Luke 23:34 variant */
  const fg = m.timeline.find((s) => s.id === "forgiveness")?.saying;
  if (fg && fg.evidence !== "textual-variant") err("forgiveness: Luke 23:34a must be marked textual-variant");

  /* fragments */
  const fragIds = new Set<string>();
  for (const f of m.fragments) {
    const ctx = `fragment ${f.id}`;
    if (fragIds.has(f.id)) err(`${ctx}: duplicate`);
    fragIds.add(f.id);
    if (f.evidence !== "artistic-interpretation") err(`${ctx}: internal monologue must be artistic-interpretation`);
    if (f.kind === "verbal" && !isStr(f.textKey)) err(`${ctx}: verbal requires textKey`);
    if (f.kind === "sonic" && !isStr(f.cueId)) err(`${ctx}: sonic requires cueId`);
    if (locale && f.textKey && !locale[f.textKey]) err(`${ctx}: locale missing "${f.textKey}"`);
    if (!in01(f.minIntensity)) err(`${ctx}: minIntensity [0,1]`);
    if (!(f.durationMs > 0 && f.weight > 0)) err(`${ctx}: durationMs and weight must be positive`);
    refOk(ctx, f.anchors);
    if (f.states !== "*") for (const s of f.states) if (!TIMELINE.includes(s)) err(`${ctx}: unknown state "${s}"`);
    for (const t of f.targetAffinity) if (!targetIds.has(t)) err(`${ctx}: unknown affinity target "${t}"`);
    if (f.gaze !== undefined) {
      if (!targetIds.has(f.gaze?.target)) err(`${ctx}: gaze.target unknown "${String(f.gaze?.target)}"`);
      const tierList = Array.isArray(m.racingThoughts?.dwellTiers) ? m.racingThoughts.dwellTiers : [];
      if (!tierList.some((t) => t?.id === f.gaze?.tier)) err(`${ctx}: gaze.tier unknown "${String(f.gaze?.tier)}"`);
    }
  }

  /* dialogue cues */
  const cueIds = new Set<string>();
  for (const c of m.cues) {
    const ctx = `cue ${c.id}`;
    if (cueIds.has(c.id)) err(`${ctx}: duplicate`);
    cueIds.add(c.id);
    if (!CUE_KINDS.includes(c.kind)) err(`${ctx}: invalid kind "${String(c.kind)}"`);
    if (c.speaker !== "narrator" && !castIds.has(c.speaker)) err(`${ctx}: unknown speaker "${String(c.speaker)}"`);
    if (c.kind === "narration" && c.speaker !== "narrator") err(`${ctx}: narration is spoken by the narrator`);
    if (c.kind === "spoken" && c.evidence !== "gospel-text" && c.evidence !== "textual-variant") {
      err(`${ctx}: spoken cues quote scripture (evidence gospel-text or textual-variant)`);
    }
    if (!evidenceOk(c.evidence)) err(`${ctx}: invalid evidence`);
    if (!isStr(c.textKey)) err(`${ctx}: textKey required`);
    else if (locale && !locale[c.textKey]) err(`${ctx}: locale missing "${c.textKey}"`);
    if (!(Array.isArray(c.sourceIds) && c.sourceIds.length > 0)) err(`${ctx}: at least one source`);
    refOk(ctx, c.sourceIds);
    if (c.origin !== undefined && !targetIds.has(c.origin)) err(`${ctx}: unknown origin "${String(c.origin)}"`);
    if (!(isNum(c.durationMs) && c.durationMs > 0)) err(`${ctx}: durationMs must be positive`);
    if (c.states !== "*") for (const st of Array.isArray(c.states) ? c.states : []) if (!TIMELINE.includes(st)) err(`${ctx}: unknown state "${String(st)}"`);
    const tr = c.trigger as Record<string, unknown> | undefined;
    if (tr?.type === "gaze") {
      if (!targetIds.has(tr.target as string)) err(`${ctx}: unknown trigger target "${String(tr.target)}"`);
      if (!(isNum(tr.minDwellMs) && tr.minDwellMs >= 0)) err(`${ctx}: trigger.minDwellMs must be >= 0`);
      if (!CUE_MODES.includes(tr.mode as never)) err(`${ctx}: invalid trigger.mode "${String(tr.mode)}"`);
    } else if (tr?.type === "phase") {
      const stateDef = m.timeline.find((x) => x.id === tr.state);
      if (!stateDef) err(`${ctx}: unknown trigger state "${String(tr.state)}"`);
      else if (!(PHASES_BY_KIND[stateDef.kind] ?? []).includes(tr.phase as never)) err(`${ctx}: phase "${String(tr.phase)}" does not occur in state "${String(tr.state)}"`);
      if (!(isNum(tr.atMs) && tr.atMs >= 0)) err(`${ctx}: trigger.atMs must be >= 0`);
    } else {
      err(`${ctx}: invalid trigger type "${String(tr?.type)}"`);
    }
    if (c.beforeSaying === true) {
      const sd = tr?.type === "phase" ? m.timeline.find((x) => x.id === tr.state) : undefined;
      if (!sd || sd.kind !== "saying" || tr?.phase !== "approach") err(`${ctx}: beforeSaying requires a phase trigger in a saying's approach`);
      else {
        const end = (tr.atMs as number) + c.durationMs;
        const min = sd.timing?.approachMinMs ?? 0;
        if (end > min) err(`${ctx}: beforeSaying cue must end by approachMinMs (${end}ms > ${min}ms)`);
      }
    }
  }

  /* every RT-active state needs eligible fragments, or the monologue would be silent where it should not be */
  for (const s of m.timeline) {
    if (s.rt.ceiling > 0 && s.allowEmbodiment) {
      const n = m.fragments.filter((f) => f.states === "*" || f.states.includes(s.id)).length;
      if (n < 3) warnings.push(`state ${s.id}: only ${n} eligible fragments (repetition risk)`);
    }
  }

  if (locale) {
    for (const k of [m.advisory?.titleKey, m.advisory?.bodyKey]) if (!k || !locale[k]) err(`advisory: locale missing "${String(k)}"`);
  }

  void timelineIds;
  return { ok: errors.length === 0, errors, warnings };
}

export class ManifestError extends Error {
  constructor(public readonly errors: string[]) {
    super(`Invalid episode manifest:\n  ${errors.join("\n  ")}`);
  }
}

export function assertManifest(raw: unknown, locale?: Locale): EpisodeManifest {
  const r = validateManifest(raw, locale);
  if (!r.ok) throw new ManifestError(r.errors);
  return raw as EpisodeManifest;
}
