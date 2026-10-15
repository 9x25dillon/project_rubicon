/** Node-only IO boundary. The core never touches the filesystem. */
import { readFileSync } from "node:fs";
import { resolve } from "node:path";
import { assertManifest } from "../validate.js";
import type { EpisodeManifest, Locale } from "../types.js";

export function loadEpisode(
  manifestPath = "content/passion.episode.json",
  localePath = "content/locale.en.json",
): { manifest: EpisodeManifest; locale: Locale } {
  const locale = JSON.parse(readFileSync(resolve(localePath), "utf8")) as Locale;
  const raw: unknown = JSON.parse(readFileSync(resolve(manifestPath), "utf8"));
  return { manifest: assertManifest(raw, locale), locale };
}
