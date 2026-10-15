/**
 * Deterministic primitives. No Math.random, no Date, no platform clocks anywhere in the core.
 * mulberry32 is chosen because it is a single u32 of state and ports exactly to C++ (uint32_t) and UE5.
 */

export interface RngDraw {
  value: number; // [0,1)
  state: number; // next u32 state
}

export function rngNext(state: number): RngDraw {
  const s = (state + 0x6d2b79f5) >>> 0;
  let z = s;
  z = Math.imul(z ^ (z >>> 15), z | 1);
  z ^= z + Math.imul(z ^ (z >>> 7), z | 61);
  const out = ((z ^ (z >>> 14)) >>> 0) / 4294967296;
  return { value: out, state: s };
}

export function seedToState(seed: number): number {
  return seed >>> 0;
}

export const clamp01 = (x: number): number => (x < 0 ? 0 : x > 1 ? 1 : x);
export const clamp = (x: number, lo: number, hi: number): number => (x < lo ? lo : x > hi ? hi : x);
export const lerp = (a: number, b: number, t: number): number => a + (b - a) * t;

/** Exact exponential smoothing step for a first-order system with time constant tauMs. */
export function smoothToward(current: number, target: number, dtMs: number, tauMs: number): number {
  if (tauMs <= 0) return target;
  return current + (target - current) * (1 - Math.exp(-dtMs / tauMs));
}

export function smoothstep(edge0: number, edge1: number, x: number): number {
  const t = clamp01((x - edge0) / (edge1 - edge0));
  return t * t * (3 - 2 * t);
}

/** Stable stringify (sorted keys) for hashing replays. */
export function stableStringify(v: unknown): string {
  if (v === null || typeof v !== "object") return JSON.stringify(v);
  if (Array.isArray(v)) return `[${v.map(stableStringify).join(",")}]`;
  const o = v as Record<string, unknown>;
  return `{${Object.keys(o)
    .sort()
    .filter((k) => o[k] !== undefined)
    .map((k) => `${JSON.stringify(k)}:${stableStringify(o[k])}`)
    .join(",")}}`;
}

/** FNV-1a 32-bit over UTF-16 code units. */
export function fnv1a(s: string): string {
  let h = 0x811c9dc5;
  for (let i = 0; i < s.length; i++) {
    h ^= s.charCodeAt(i);
    h = Math.imul(h, 0x01000193) >>> 0;
  }
  return h.toString(16).padStart(8, "0");
}
