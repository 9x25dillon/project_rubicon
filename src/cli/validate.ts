import { readFileSync } from "node:fs";
import { validateManifest } from "../validate.js";
import type { Locale } from "../types.js";

const [manifestPath = "content/passion.episode.json", localePath = "content/locale.en.json"] = process.argv.slice(2);
const locale = JSON.parse(readFileSync(localePath, "utf8")) as Locale;
const r = validateManifest(JSON.parse(readFileSync(manifestPath, "utf8")), locale);
for (const w of r.warnings) console.warn(`warn  ${w}`);
for (const e of r.errors) console.error(`error ${e}`);
console.log(r.ok ? `OK  ${manifestPath}` : `FAIL  ${r.errors.length} error(s)`);
process.exit(r.ok ? 0 : 1);
