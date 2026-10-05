// Shared data model. The Python Stellar Forge produces exactly this JSON shape
// (see server/omnispace_server/schema.py) and the procedural generator mirrors it.

export const LS = 299_792_458 // metres per light-second
export const AU = 149_597_870_700
export const LY = 9.4607e15
export const SOLAR_RADIUS = 695_700_000
export const KM = 1000

export type StarClass = 'O' | 'B' | 'A' | 'F' | 'G' | 'K' | 'M' | 'L' | 'WD' | 'NS'

export interface StarDescriptor {
  id: string
  name: string
  cls: StarClass
  /** galactic position in light years */
  pos: [number, number, number]
  temperature: number
  radiusSolar: number
  massSolar: number
  luminosity: number
  color: string
  ageGyr: number
  /** 0..1 how settled the region is (1 = core worlds) */
  settlement: number
  region: string
}

export type PlanetType =
  | 'rocky'
  | 'barren'
  | 'desert'
  | 'lava'
  | 'ice'
  | 'ocean'
  | 'earthlike'
  | 'jungle'
  | 'toxic'
  | 'gas_giant'
  | 'ice_giant'

export interface Rings {
  innerM: number
  outerM: number
  color: string
  opacity: number
}

export interface Body {
  id: string
  name: string
  kind: 'planet' | 'moon'
  type: PlanetType
  parent: string // 'star' or body id
  orbitM: number
  phase: number // radians
  inclination: number // radians
  radiusM: number
  palette: string[] // 3 colours: base, mid, highlight
  atmosphere?: string | null
  clouds?: number // 0..1
  bands?: number // 0..1 banding intensity for giants
  tilt?: number // axial tilt radians
  rings?: Rings | null
  temperatureK?: number
  gravityG?: number
  description: string
}

export type StationKind = 'spire' | 'ring' | 'outpost'

export interface Station {
  id: string
  name: string
  kind: StationKind
  parent: string // body id
  orbitM: number
  phase: number
  inclination: number
  faction: string
  economy: string
  services: string[]
  tagline: string
  description: string
}

export interface Belt {
  id: string
  name: string
  parent: string // 'star' or body id
  radiusM: number
  widthM: number
  composition: string
}

export interface POI {
  id: string
  name: string
  kind: 'derelict' | 'signal' | 'anomaly' | 'beacon' | 'wreck'
  parent: string
  offsetM: number
  phase: number
  description: string
  reward: number
}

export interface SystemProfile {
  description: string
  lore: string
  faction: string
  government: string
  economy: string
  security: 'none' | 'low' | 'medium' | 'high'
  population: number
  danger: number
}

export interface StarSystem {
  id: string
  name: string
  source: 'ai' | 'procedural' | 'authored'
  model?: string
  discoveredBy?: string
  createdAt?: string
  star: StarDescriptor
  starFlavor?: string
  profile: SystemProfile
  bodies: Body[]
  stations: Station[]
  belts: Belt[]
  pois: POI[]
}

/** A resolved, positioned thing in the system used for nav / rendering. */
export interface NavObject {
  id: string
  name: string
  kind: 'star' | 'planet' | 'moon' | 'station' | 'poi' | 'belt'
  pos: [number, number, number]
  radius: number
  subtitle: string
  ref: Body | Station | POI | Belt | StarDescriptor
}
