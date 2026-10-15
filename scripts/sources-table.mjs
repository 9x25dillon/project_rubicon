// Generates docs/SOURCES.md from the manifest: every source, its evidence category, type, rendering and debate,
// and which cues and lines cite it. For theology and history reviewers (M6 sign-off).
//   node scripts/sources-table.mjs
import { readFileSync, writeFileSync } from "node:fs";

const m = JSON.parse(readFileSync("content/passion.episode.json", "utf8"));
const L = JSON.parse(readFileSync("content/locale.en.json", "utf8"));
const esc = (s) => String(s ?? "").replace(/\|/g, "\\|").replace(/\n/g, " ");
const citedBy = (id) => [
  ...m.cues.filter((c) => c.sourceIds.includes(id)).map((c) => `cue ${c.id}`),
  ...m.fragments.filter((f) => f.anchors.includes(id)).map((f) => `line ${f.id}`),
];

let out = `# Sources

Generated from \`content/passion.episode.json\` by \`node scripts/sources-table.mjs\`. Do not edit by hand.

Every claim the experience voices or displays traces to one of these. Evidence category (one of the four, always
visible) says how a claim relates to the scene; source type says what kind of witness the source is. Ancient texts,
archaeology and scholarship are always historical-inference: they describe the world around the scene, not the scene.
A paraphrase is never displayed or voiced as a quotation (checked by tests/dialogue.test.ts).

`;
const groups = [
  ["ancient-text", "Ancient texts"],
  ["archaeology", "Archaeology"],
  ["scholarship", "Scholarship"],
  ["gospel", "Gospels"],
  ["new-testament", "Other New Testament"],
  ["hebrew-bible", "Hebrew Bible"],
];
for (const [type, title] of groups) {
  const rows = m.sources.filter((s) => s.sourceType === type);
  if (!rows.length) continue;
  out += `\n## ${title} (${rows.length})\n\n| Source | Evidence | Text | Rendering | Debate / note | Used by |\n|---|---|---|---|---|---|\n`;
  for (const s of rows) {
    const text = s.kjv ? `KJV: ${s.kjv.text}` : s.excerpt ? s.excerpt.text : "";
    const rendering = s.kjv ? "KJV (public domain)" : s.excerpt ? `${s.excerpt.rendering}${s.excerpt.translator ? `, ${s.excerpt.translator}` : ""}` : "note only";
    const note = [s.debate, s.note, ...(s.manuscriptNotes ?? [])].filter(Boolean).join(" ");
    const used = citedBy(s.id);
    out += `| ${esc(s.citation)} | ${s.evidence} | ${esc(text)} | ${esc(rendering)} | ${esc(note)} | ${esc(used.length > 6 ? used.slice(0, 6).join(", ") + ` (+${used.length - 6})` : used.join(", "))} |\n`;
  }
}
out += `\n## Dialogue cues (${m.cues.length})\n\n| Cue | Kind | Speaker | Trigger | Text | Sources |\n|---|---|---|---|---|---|\n`;
for (const c of m.cues) {
  const tr = c.trigger.type === "gaze" ? `gaze ${c.trigger.target} >= ${c.trigger.minDwellMs / 1000} s (${c.trigger.mode})` : `${c.trigger.state}/${c.trigger.phase} at ${c.trigger.atMs / 1000} s`;
  out += `| ${c.id} | ${c.kind} | ${c.speaker} | ${esc(tr)} | ${esc(L[c.textKey])} | ${esc(c.sourceIds.join(", "))} |\n`;
}
out += `\n## Deliberately not used\n\nSources whose framing assigns responsibility for the crucifixion to the Jewish people collectively are not voiced or displayed (content rules in CLAUDE.md), for example the "wise king" passage in the letter of Mara bar Serapion and the deicide charge of Melito of Sardis, *On Pascha*. Rabbinic material is cited only for the documented custom of a drink of mercy for the condemned.\n`;
writeFileSync("docs/SOURCES.md", out);
console.log(`docs/SOURCES.md  sources=${m.sources.length} cues=${m.cues.length}`);
