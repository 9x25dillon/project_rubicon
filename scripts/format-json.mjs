// Deterministic JSON formatter for content/*.json (house style):
// any object or array is written on one line when it fits in WIDTH columns, otherwise one member per line.
//   node scripts/format-json.mjs            rewrite content/*.json in place
//   node scripts/format-json.mjs --check    exit 1 if any file is not canonically formatted
import { readFileSync, writeFileSync } from "node:fs";

const WIDTH = 160;

export function formatJson(value) {
  const pad = (n) => " ".repeat(n);
  const inline = (v) => {
    if (v === null || typeof v !== "object") return JSON.stringify(v);
    if (Array.isArray(v)) return v.length ? `[${v.map(inline).join(", ")}]` : "[]";
    const keys = Object.keys(v).filter((k) => v[k] !== undefined);
    return keys.length ? `{ ${keys.map((k) => `${JSON.stringify(k)}: ${inline(v[k])}`).join(", ")} }` : "{}";
  };
  const fmt = (v, ind, used) => {
    if (v === null || typeof v !== "object") return JSON.stringify(v);
    const one = inline(v);
    if (ind + used + one.length <= WIDTH) return one;
    if (Array.isArray(v)) return `[\n${v.map((x) => pad(ind + 2) + fmt(x, ind + 2, 0)).join(",\n")}\n${pad(ind)}]`;
    const keys = Object.keys(v).filter((k) => v[k] !== undefined);
    return `{\n${keys
      .map((k) => {
        const prefix = `${JSON.stringify(k)}: `;
        return pad(ind + 2) + prefix + fmt(v[k], ind + 2, prefix.length);
      })
      .join(",\n")}\n${pad(ind)}}`;
  };
  return fmt(value, 0, 0) + "\n";
}

export const CONTENT_FILES = ["content/passion.episode.json", "content/locale.en.json"];

if (import.meta.url === `file://${process.argv[1]}`) {
  const check = process.argv.includes("--check");
  let bad = 0;
  for (const f of CONTENT_FILES) {
    const text = readFileSync(f, "utf8");
    const out = formatJson(JSON.parse(text));
    if (text !== out) {
      if (check) { console.error(`not canonically formatted: ${f} (run npm run format:content)`); bad++; }
      else { writeFileSync(f, out); console.log(`formatted ${f}`); }
    }
  }
  process.exit(bad ? 1 : 0);
}
