/**
 * Validator parity cases: targeted manifest mutations with the exact errors and warnings the TypeScript
 * validator reports. Exported by `npm run golden` to golden/validator-cases.json; the C++ editor validator
 * (RubiconNarrativeCore ValidateManifest) must report identical messages in identical order.
 */
import { validateManifest } from "./validate.js";
import type { EpisodeManifest, Locale } from "./types.js";

export interface ValidatorCase {
  name: string;
  manifest: unknown;
  locale: Locale | null;
  errors: string[];
  warnings: string[];
}

type Mutable = Record<string, any>; // eslint-disable-line @typescript-eslint/no-explicit-any

const clone = <T>(x: T): T => JSON.parse(JSON.stringify(x)) as T;

export function validatorCases(m: EpisodeManifest, locale: Locale): ValidatorCase[] {
  const state = (x: Mutable, id: string): Mutable => x.timeline.find((s: Mutable) => s.id === id);
  const mutations: { name: string; locale?: Locale | null; mutate: (x: Mutable) => unknown }[] = [
    { name: "canonical manifest", mutate: (x) => x },
    { name: "canonical manifest without locale", locale: null, mutate: (x) => x },
    { name: "not an object", mutate: () => 42 },
    {
      name: "missing title, fragments not an array",
      mutate: (x) => { delete x.title; x.fragments = {}; return x; },
    },
    {
      name: "timeline reordered",
      mutate: (x) => { [x.timeline[2], x.timeline[3]] = [x.timeline[3], x.timeline[2]]; return x; },
    },
    {
      name: "variant unmarked, bad reference",
      mutate: (x) => {
        state(x, "forgiveness").saying.evidence = "gospel-text";
        state(x, "mother").saying.sourceIds = ["john-99-99"];
        return x;
      },
    },
    {
      name: "unsafe comfort and witness, non-monotone stress weights",
      mutate: (x) => {
        x.racingThoughts.comfortGain = 0.5;
        x.racingThoughts.witnessEchoScale = 0.9;
        x.racingThoughts.stressWeights.relief = 0.1;
        x.racingThoughts.stressWeights.high = 0.2;
        x.racingThoughts.noTargetStress = "calm";
        x.racingThoughts.evidence = "gospel-text";
        return x;
      },
    },
    {
      name: "invalid racing thoughts parameters",
      mutate: (x) => {
        x.racingThoughts.fragmentRate.gamma = 0;
        x.racingThoughts.externalDuck.maxDb = 3;
        x.racingThoughts.affinityMultiplier = 0.5;
        x.racingThoughts.maxVoices = 2.5;
        delete x.racingThoughts.dwellTauMs;
        return x;
      },
    },
    {
      name: "post-death embodiment unlocked",
      mutate: (x) => {
        const sp = state(x, "spear");
        sp.allowEmbodiment = true;
        delete sp.forcedMode;
        state(x, "release").allowEmbodiment = true;
        return x;
      },
    },
    {
      name: "fragment errors",
      mutate: (x) => {
        const f = x.fragments[1];
        f.id = x.fragments[0].id;
        f.evidence = "gospel-text";
        f.states = ["elevation", "nowhere"];
        f.targetAffinity = ["the_moon"];
        f.minIntensity = 2;
        f.weight = 0;
        x.fragments[2].anchors = ["no-such-anchor"];
        if (x.fragments[3].kind === "verbal") delete x.fragments[3].textKey;
        else delete x.fragments[3].cueId;
        return x;
      },
    },
    {
      name: "gaze, cast and vantage errors",
      mutate: (x) => {
        x.gazeTargets[0].stress = "unbearable";
        x.gazeTargets[0].actors = ["nobody"];
        x.gazeTargets[1].id = x.gazeTargets[0].id;
        x.cast[1].evidence = 7;
        x.witnessVantages = x.witnessVantages.filter((v: Mutable) => v.id !== "mary");
        x.witnessVantages[0].castId = "ghost";
        return x;
      },
    },
    {
      name: "source errors",
      mutate: (x) => {
        const s = x.sources.find((y: Mutable) => y.id === "luke-23-34");
        delete s.manuscriptNotes;
        x.sources[0].evidence = "rumour";
        x.sources[1].kjv.text = "";
        delete x.sources[2].citation;
        x.sources.push({ citation: "Nowhere 1:1", evidence: "gospel-text" });
        return x;
      },
    },
    {
      name: "state timing, rt and stress agreement errors",
      mutate: (x) => {
        delete state(x, "elevation").timing.raisingMs;
        const fg = state(x, "forgiveness");
        fg.saying.stressLevel = "low";
        fg.stressOverrides.the_moon = "high";
        fg.rt.floor = 0.99;
        fg.rt.tauDownMs = 10;
        fg.timing.approachAutoMs = 1;
        state(x, "release").timing.runningMs = 0;
        state(x, "thirst").kind = "ordeal";
        state(x, "threshold").saying = { id: "forgiveness" };
        return x;
      },
    },
    {
      name: "locale missing keys",
      locale: Object.fromEntries(Object.entries(locale).filter(([k]) => k !== "passion.thirst" && k !== "ui.advisory.body" && !k.startsWith("rt.w"))),
      mutate: (x) => x,
    },
    {
      name: "too few fragments (warnings only)",
      mutate: (x) => { x.fragments = x.fragments.filter((f: Mutable) => f.states === "*").slice(0, 1); return x; },
    },
  ];

  return mutations.map(({ name, locale: loc, mutate }) => {
    const useLocale = loc === undefined ? locale : loc;
    const manifest = mutate(clone(m) as Mutable);
    const r = validateManifest(manifest, useLocale ?? undefined);
    return { name, manifest, locale: useLocale, errors: r.errors, warnings: r.warnings };
  });
}
