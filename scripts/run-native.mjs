// Runs the native C++ core tests and the golden parity check (single-config and multi-config generators).
import { existsSync } from "node:fs";
import { spawnSync } from "node:child_process";

const pick = (name) =>
  [`native/build/${name}`, `native/build/Release/${name}`, `native/build/${name}.exe`, `native/build/Release/${name}.exe`].find(existsSync);
for (const [name, args] of [["rubicon_core_tests", ["content"]], ["rubicon_parity", ["golden", "content"]]]) {
  const bin = pick(name);
  if (!bin) { console.error(`missing ${name}; build failed?`); process.exit(2); }
  const r = spawnSync(bin, args, { stdio: "inherit" });
  if (r.status !== 0) process.exit(r.status ?? 1);
}
