// Deterministic hashing + PRNG utilities. Every procedural thing in the galaxy
// is derived from these so the star map is identical for every player.

export function hashString(str: string, seed = 0): number {
  let h1 = 0xdeadbeef ^ seed
  let h2 = 0x41c6ce57 ^ seed
  for (let i = 0; i < str.length; i++) {
    const ch = str.charCodeAt(i)
    h1 = Math.imul(h1 ^ ch, 2654435761)
    h2 = Math.imul(h2 ^ ch, 1597334677)
  }
  h1 = Math.imul(h1 ^ (h1 >>> 16), 2246822507) ^ Math.imul(h2 ^ (h2 >>> 13), 3266489909)
  h2 = Math.imul(h2 ^ (h2 >>> 16), 2246822507) ^ Math.imul(h1 ^ (h1 >>> 13), 3266489909)
  return (h2 >>> 0) * 4294967296 + (h1 >>> 0)
}

export function hashInts(...n: number[]): number {
  let h = 0x811c9dc5
  for (const v of n) {
    h = Math.imul(h ^ (v | 0), 16777619)
    h ^= h >>> 13
    h = Math.imul(h, 0x5bd1e995)
    h ^= h >>> 15
  }
  return h >>> 0
}

export class Rng {
  private s: number
  constructor(seed: number) {
    this.s = (seed >>> 0) || 0x9e3779b9
  }
  next(): number {
    // mulberry32
    let t = (this.s += 0x6d2b79f5)
    t = Math.imul(t ^ (t >>> 15), t | 1)
    t ^= t + Math.imul(t ^ (t >>> 7), t | 61)
    return ((t ^ (t >>> 14)) >>> 0) / 4294967296
  }
  range(a: number, b: number): number {
    return a + (b - a) * this.next()
  }
  int(a: number, b: number): number {
    return Math.floor(this.range(a, b + 1))
  }
  pick<T>(arr: readonly T[]): T {
    return arr[Math.floor(this.next() * arr.length) % arr.length]
  }
  chance(p: number): boolean {
    return this.next() < p
  }
  gauss(): number {
    const u = Math.max(1e-9, this.next())
    const v = this.next()
    return Math.sqrt(-2 * Math.log(u)) * Math.cos(2 * Math.PI * v)
  }
  poisson(lambda: number): number {
    if (lambda > 30) return Math.max(0, Math.round(lambda + Math.sqrt(lambda) * this.gauss()))
    const L = Math.exp(-lambda)
    let k = 0
    let p = 1
    do {
      k++
      p *= this.next()
    } while (p > L)
    return k - 1
  }
}

export function clamp(v: number, a: number, b: number) {
  return v < a ? a : v > b ? b : v
}
export function lerp(a: number, b: number, t: number) {
  return a + (b - a) * t
}
export function smoothstep(a: number, b: number, v: number) {
  const t = clamp((v - a) / (b - a), 0, 1)
  return t * t * (3 - 2 * t)
}
