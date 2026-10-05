import type { Mission } from './economy'

export interface DataEntry {
  systemId: string
  systemName: string
  bodyId: string
  name: string
  value: number
  kind: 'discovery' | 'surface' | 'poi'
}

export interface PlayerSave {
  version: 1
  commander: string
  credits: number
  cargo: Record<string, number>
  missionCargo: Record<string, number>
  missions: Mission[]
  data: DataEntry[]
  systemId: string
  pos: [number, number, number]
  quat: [number, number, number, number]
  dockedAt: string | null
  dockedBay: number
  fuel: number
  hull: number
  visited: string[]
  scanned: string[]
  route: string[]
  tutorial: number
  stats: { jumps: number; lyTravelled: number; systemsDiscovered: number; earned: number }
  lastStation: { systemId: string; stationId: string } | null
}

export const SHIP = {
  name: 'OS-7 Kestrel',
  fuelMax: 16,
  cargoCap: 32,
  jumpRange: 18,
  maxSpeed: 340,
  boostSpeed: 560,
  hullMass: 62,
}

const KEY = 'omnispace.save.v1'

export function newPlayer(commander: string): PlayerSave {
  return {
    version: 1,
    commander,
    credits: 5000,
    cargo: {},
    missionCargo: {},
    missions: [],
    data: [],
    systemId: 'kelvar',
    pos: [0, 0, 0],
    quat: [0, 0, 0, 1],
    dockedAt: null,
    dockedBay: 0,
    fuel: SHIP.fuelMax,
    hull: 1,
    visited: ['kelvar'],
    scanned: [],
    route: [],
    tutorial: 0,
    stats: { jumps: 0, lyTravelled: 0, systemsDiscovered: 0, earned: 0 },
    lastStation: null,
  }
}

export function loadSave(): PlayerSave | null {
  try {
    const raw = localStorage.getItem(KEY)
    if (!raw) return null
    const s = JSON.parse(raw) as PlayerSave
    return s.version === 1 ? s : null
  } catch {
    return null
  }
}

export function writeSave(s: PlayerSave) {
  try {
    localStorage.setItem(KEY, JSON.stringify(s))
  } catch {
    /* storage may be unavailable */
  }
}

export function clearSave() {
  try {
    localStorage.removeItem(KEY)
  } catch {
    /* ignore */
  }
}

export function cargoTotal(s: PlayerSave) {
  let t = 0
  for (const v of Object.values(s.cargo)) t += v
  for (const v of Object.values(s.missionCargo)) t += v
  return t
}

export function jumpRange(s: PlayerSave) {
  const mass = SHIP.hullMass + cargoTotal(s) + s.fuel
  return SHIP.jumpRange * ((SHIP.hullMass + SHIP.fuelMax) / mass) * 0.94 + 0.9
}

export function jumpFuel(distance: number, range: number) {
  return 0.35 + 3.4 * Math.pow(Math.min(1, distance / range), 2)
}
