// Deterministic galaxy ("star map"). Like Elite's Stellar Forge, the positions,
// names and stellar classes of every star are fixed by math so the map is the
// same for every player. Only the *contents* of a system are created by the
// AI Stellar Forge when somebody first jumps in.

import { Rng, clamp, hashInts, hashString } from './rng'
import type { StarClass, StarDescriptor } from './types'

export const SECTOR_SIZE = 40 // light years
export const HOME_ID = 'kelvar'
export const HOME_POS: [number, number, number] = [-24180, 12, 6420]
const GALAXY_RADIUS = 46000

interface ClassInfo {
  t: [number, number]
  r: [number, number]
  m: [number, number]
  w: number
}

const CLASS_INFO: Record<StarClass, ClassInfo> = {
  O: { t: [30000, 42000], r: [6.5, 11], m: [18, 50], w: 0.4 },
  B: { t: [11000, 28000], r: [2.4, 6.5], m: [3, 16], w: 2 },
  A: { t: [7500, 10000], r: [1.5, 2.3], m: [1.6, 2.5], w: 4.5 },
  F: { t: [6000, 7400], r: [1.1, 1.5], m: [1.05, 1.5], w: 8 },
  G: { t: [5200, 6000], r: [0.85, 1.15], m: [0.85, 1.12], w: 13 },
  K: { t: [3900, 5200], r: [0.6, 0.88], m: [0.6, 0.85], w: 20 },
  M: { t: [2400, 3800], r: [0.15, 0.6], m: [0.1, 0.6], w: 42 },
  L: { t: [1300, 2300], r: [0.09, 0.12], m: [0.05, 0.08], w: 4 },
  WD: { t: [8000, 30000], r: [0.009, 0.015], m: [0.5, 1.1], w: 4.5 },
  NS: { t: [500000, 900000], r: [1.4e-5, 1.8e-5], m: [1.3, 2.1], w: 1.2 },
}
const CLASS_LIST = Object.keys(CLASS_INFO) as StarClass[]
const CLASS_TOTAL = CLASS_LIST.reduce((s, c) => s + CLASS_INFO[c].w, 0)

export const SCOOPABLE: Record<StarClass, boolean> = {
  O: true, B: true, A: true, F: true, G: true, K: true, M: true, L: false, WD: false, NS: false,
}

export const CLASS_LABEL: Record<StarClass, string> = {
  O: 'O-type blue supergiant',
  B: 'B-type blue-white star',
  A: 'A-type white star',
  F: 'F-type yellow-white star',
  G: 'G-type yellow star',
  K: 'K-type orange dwarf',
  M: 'M-type red dwarf',
  L: 'L-type brown dwarf',
  WD: 'White dwarf',
  NS: 'Neutron star',
}

/** Blackbody-ish colour for a temperature. */
export function starColor(t: number): string {
  // Tanner Helland approximation
  const k = clamp(t, 1000, 40000) / 100
  let r: number, g: number, b: number
  if (k <= 66) {
    r = 255
    g = 99.47 * Math.log(k) - 161.12
    b = k <= 19 ? 0 : 138.52 * Math.log(k - 10) - 305.04
  } else {
    r = 329.7 * Math.pow(k - 60, -0.1332)
    g = 288.12 * Math.pow(k - 60, -0.0755)
    b = 255
  }
  const h = (v: number) => Math.round(clamp(v, 0, 255)).toString(16).padStart(2, '0')
  return `#${h(r)}${h(g)}${h(b)}`
}

// ---------------------------------------------------------------- density
function rawDensity(x: number, y: number, z: number): number {
  const r = Math.hypot(x, z)
  if (r > GALAXY_RADIUS * 1.15) return 0
  const theta = Math.atan2(z, x)
  const pitch = Math.tan((13 * Math.PI) / 180)
  let arm = 0
  if (r > 600) {
    const base = Math.log(r / 600) / pitch
    for (let k = 0; k < 4; k++) {
      let d = theta - (base + (k * Math.PI) / 2)
      d = Math.atan2(Math.sin(d), Math.cos(d))
      arm = Math.max(arm, Math.exp(-(d * d) / (2 * 0.22 * 0.22)))
    }
  }
  const disk = Math.exp(-r / 13000) * (0.35 + 1.4 * arm)
  const h = 260 + r * 0.004
  const vert = Math.exp(-Math.abs(y) / h)
  const bulge = 7 * Math.exp(-((r / 3200) ** 2) - (y / 1800) ** 2)
  const edge = clamp((GALAXY_RADIUS * 1.15 - r) / 6000, 0, 1)
  return (disk * vert + bulge) * edge
}
const HOME_RAW = rawDensity(HOME_POS[0], HOME_POS[1], HOME_POS[2])
const LOCAL_DENSITY = 6.5e-4 // stars per cubic light year near home
export function density(x: number, y: number, z: number) {
  return (rawDensity(x, y, z) / HOME_RAW) * LOCAL_DENSITY
}

// ---------------------------------------------------------------- names
const PRE = ['Ka', 'Ve', 'Ta', 'Or', 'Sel', 'Mi', 'Dra', 'Ny', 'Ae', 'Ul', 'Zer', 'Hal', 'Ix', 'Qua', 'Bel', 'Cor', 'Eos', 'Fen', 'Gal', 'Ith', 'Jor', 'Kes', 'Lio', 'Mor', 'Nax', 'Ost', 'Pyr', 'Rho', 'Sar', 'Thal', 'Vas', 'Wen', 'Yl', 'Zan', 'Ash', 'Bri', 'Cal', 'Dun', 'Esk', 'Har', 'Ka', 'Lum', 'Nem', 'Per', 'Riv', 'Sol', 'Tor', 'Var']
const MID = ['', '', '', 'ra', 'li', 'na', 'ro', 'the', 'ma', 've', 'si', 'do', 'ri', 'la', 'an', 'el', 'is', 'or', 'ta', 'ny']
const END = ['var', 'ris', 'tor', 'nis', 'dor', 'ax', 'eon', 'ia', 'us', 'an', 'os', 'eth', 'ion', 'ar', 'el', 'yx', 'ane', 'oth', 'ix', 'ura', 'ka', 'en', 'is', 'ae', 'ul', 'era', 'ost', 'ine']
const CATALOG = ['HD', 'KX', 'OSC', 'NAV', 'GSC', 'TX']

export function starName(rng: Rng, remote: boolean): string {
  if (remote && rng.chance(0.28)) {
    return `${rng.pick(CATALOG)} ${rng.int(100, 9999)}-${String.fromCharCode(65 + rng.int(0, 25))}`
  }
  let n = rng.pick(PRE) + rng.pick(MID) + rng.pick(END)
  if (rng.chance(0.12)) n += ' ' + rng.pick(['Prime', 'Major', 'Minor', 'Reach', 'Gate', 'Deep'])
  return n
}

const REGION_A = ['Tanaran', 'Ashkar', 'Veyl', 'Orison', 'Cassim', 'Morrow', 'Halcyon', 'Ixion', 'Seraph', 'Norn', 'Pellucid', 'Drakon', 'Umber', 'Lyric', 'Quiet', 'Ember']
const REGION_B = ['Reach', 'Expanse', 'Drift', 'Marches', 'Shoals', 'Verge', 'Deep', 'Cradle', 'Frontier', 'Spur', 'Gulf', 'Hollows']
export function regionName(x: number, y: number, z: number): string {
  const h = hashInts(Math.floor(x / 400), Math.floor(y / 1000), Math.floor(z / 400), 77)
  const r = new Rng(h)
  return `${r.pick(REGION_A)} ${r.pick(REGION_B)}`
}

export function settlementAt(pos: [number, number, number], seed: number): number {
  const d = Math.hypot(pos[0] - HOME_POS[0], pos[1] - HOME_POS[1], pos[2] - HOME_POS[2])
  const base = Math.pow(clamp(1 - d / 95, 0, 1), 1.2)
  const jitter = ((seed % 1000) / 1000 - 0.5) * 0.25
  return clamp(base + jitter * (base > 0 ? 1 : 0), 0, 1)
}

// ---------------------------------------------------------------- stars
function makeStar(id: string, pos: [number, number, number], seed: number): StarDescriptor {
  const rng = new Rng(seed)
  let roll = rng.next() * CLASS_TOTAL
  let cls: StarClass = 'M'
  for (const c of CLASS_LIST) {
    roll -= CLASS_INFO[c].w
    if (roll <= 0) {
      cls = c
      break
    }
  }
  const info = CLASS_INFO[cls]
  const f = rng.next()
  const temperature = Math.round(info.t[0] + (info.t[1] - info.t[0]) * f)
  const radiusSolar = info.r[0] + (info.r[1] - info.r[0]) * (0.3 * rng.next() + 0.7 * f)
  const massSolar = info.m[0] + (info.m[1] - info.m[0]) * f
  let luminosity = radiusSolar * radiusSolar * Math.pow(temperature / 5778, 4)
  if (cls === 'NS') luminosity = 0.02
  const settlement = settlementAt(pos, seed)
  return {
    id,
    name: starName(rng, settlement < 0.05),
    cls,
    pos,
    temperature,
    radiusSolar,
    massSolar,
    luminosity,
    color: cls === 'NS' ? '#cfe0ff' : starColor(temperature),
    ageGyr: Math.round(rng.range(0.2, 11) * 10) / 10,
    settlement,
    region: regionName(pos[0], pos[1], pos[2]),
  }
}

export const HOME_STAR: StarDescriptor = {
  id: HOME_ID,
  name: 'Kelvar',
  cls: 'G',
  pos: HOME_POS,
  temperature: 5650,
  radiusSolar: 1.04,
  massSolar: 1.02,
  luminosity: 1.04 * 1.04 * Math.pow(5650 / 5778, 4),
  color: starColor(5650),
  ageGyr: 4.1,
  settlement: 1,
  region: 'Tanaran Reach',
}

const sectorCache = new Map<string, StarDescriptor[]>()
const byId = new Map<string, StarDescriptor>()
byId.set(HOME_ID, HOME_STAR)

export function sectorOf(p: [number, number, number]): [number, number, number] {
  return [Math.floor(p[0] / SECTOR_SIZE), Math.floor(p[1] / SECTOR_SIZE), Math.floor(p[2] / SECTOR_SIZE)]
}

export function getSector(sx: number, sy: number, sz: number): StarDescriptor[] {
  const key = `${sx}.${sy}.${sz}`
  const cached = sectorCache.get(key)
  if (cached) return cached
  const cx = (sx + 0.5) * SECTOR_SIZE
  const cy = (sy + 0.5) * SECTOR_SIZE
  const cz = (sz + 0.5) * SECTOR_SIZE
  const rng = new Rng(hashInts(sx, sy, sz, 1337))
  const count = rng.poisson(density(cx, cy, cz) * SECTOR_SIZE ** 3)
  const stars: StarDescriptor[] = []
  for (let i = 0; i < count; i++) {
    const pos: [number, number, number] = [
      (sx + rng.next()) * SECTOR_SIZE,
      (sy + rng.next()) * SECTOR_SIZE,
      (sz + rng.next()) * SECTOR_SIZE,
    ]
    // reject by fine density so arms/disk edges are crisp inside the sector
    const accept = density(pos[0], pos[1], pos[2]) / Math.max(1e-12, density(cx, cy, cz))
    if (rng.next() > Math.min(1, accept * 0.85 + 0.15)) continue
    // keep a clear bubble around home so Kelvar sits alone
    if (Math.hypot(pos[0] - HOME_POS[0], pos[1] - HOME_POS[1], pos[2] - HOME_POS[2]) < 4.2) continue
    const id = `${sx}.${sy}.${sz}.${i}`
    const s = makeStar(id, pos, hashInts(sx, sy, sz, i, 99))
    stars.push(s)
    byId.set(id, s)
  }
  const hs = sectorOf(HOME_POS)
  if (hs[0] === sx && hs[1] === sy && hs[2] === sz) stars.push(HOME_STAR)
  sectorCache.set(key, stars)
  return stars
}

export function getStar(id: string): StarDescriptor | undefined {
  const s = byId.get(id)
  if (s) return s
  const parts = id.split('.')
  if (parts.length === 4) {
    getSector(+parts[0], +parts[1], +parts[2])
    return byId.get(id)
  }
  return undefined
}

export function distLy(a: StarDescriptor, b: StarDescriptor) {
  return Math.hypot(a.pos[0] - b.pos[0], a.pos[1] - b.pos[1], a.pos[2] - b.pos[2])
}

export function starsNear(p: [number, number, number], radius: number): StarDescriptor[] {
  const s0 = sectorOf([p[0] - radius, p[1] - radius, p[2] - radius])
  const s1 = sectorOf([p[0] + radius, p[1] + radius, p[2] + radius])
  const out: StarDescriptor[] = []
  for (let x = s0[0]; x <= s1[0]; x++)
    for (let y = s0[1]; y <= s1[1]; y++)
      for (let z = s0[2]; z <= s1[2]; z++)
        for (const s of getSector(x, y, z)) {
          if (Math.hypot(s.pos[0] - p[0], s.pos[1] - p[1], s.pos[2] - p[2]) <= radius) out.push(s)
        }
  return out
}

/** A* route over stars reachable within jumpRange. */
export function plotRoute(from: StarDescriptor, to: StarDescriptor, jumpRange: number, maxNodes = 4000): StarDescriptor[] | null {
  if (from.id === to.id) return [from]
  const key = (s: StarDescriptor) => s.id
  const open: { s: StarDescriptor; f: number; g: number }[] = [{ s: from, f: distLy(from, to), g: 0 }]
  const came = new Map<string, StarDescriptor>()
  const gScore = new Map<string, number>([[from.id, 0]])
  const closed = new Set<string>()
  let expanded = 0
  while (open.length) {
    open.sort((a, b) => a.f - b.f)
    const cur = open.shift()!
    if (cur.s.id === to.id) {
      const path = [cur.s]
      let k = cur.s.id
      while (came.has(k)) {
        const p = came.get(k)!
        path.unshift(p)
        k = p.id
      }
      return path
    }
    if (closed.has(key(cur.s))) continue
    closed.add(key(cur.s))
    if (++expanded > maxNodes) return null
    for (const n of starsNear(cur.s.pos, jumpRange)) {
      if (closed.has(n.id) || n.id === cur.s.id) continue
      // each jump costs a fixed overhead so routes prefer fewer, longer jumps
      const g = cur.g + distLy(cur.s, n) + jumpRange * 0.35
      if (g < (gScore.get(n.id) ?? Infinity)) {
        gScore.set(n.id, g)
        came.set(n.id, cur.s)
        open.push({ s: n, g, f: g + distLy(n, to) * 1.05 })
      }
    }
  }
  return null
}

export function habitableZoneAU(lum: number): [number, number] {
  return [Math.sqrt(lum / 1.1), Math.sqrt(lum / 0.53)]
}
export function frostLineAU(lum: number) {
  return 2.7 * Math.sqrt(lum)
}
export function equilibriumTempK(lum: number, au: number) {
  return 278 * Math.pow(lum, 0.25) / Math.sqrt(au)
}

export function starSeed(id: string) {
  return hashString(id) >>> 0
}
