// HUD state shared between the engine and React. The engine writes a snapshot
// ~15x per second; React renders it.

import { create } from 'zustand'
import type { Mission } from './economy'
import type { DataEntry } from './player'
import type { NavObject, StarSystem } from './types'

export type FlightMode = 'normal' | 'supercruise' | 'hyperspace' | 'docking' | 'docked' | 'launching' | 'destroyed'
export type Overlay = null | 'galaxy' | 'system' | 'pause' | 'help' | 'title'

export interface Objective {
  title: string
  steps: { text: string; done: boolean }[]
}

export interface Msg {
  id: number
  text: string
  tone: 'info' | 'good' | 'warn' | 'bad' | 'ai'
  t: number
}

export interface Rect {
  x: number
  y: number
  w: number
  h: number
}

export interface HudState {
  ready: boolean
  mode: FlightMode
  overlay: Overlay
  speed: number
  speedLabel: string
  throttle: number
  boost: boolean
  boostCharge: number
  pips: { sys: number; eng: number; wep: number }
  fuel: number
  fuelMax: number
  hull: number
  shields: number
  heat: number
  mass: number
  cargo: number
  cargoCap: number
  credits: number
  commander: string
  systemName: string
  systemSub: string[]
  target: null | { id: string; name: string; kind: string; dist: string; sub: string; sx: number; sy: number; onScreen: boolean; angle: number }
  hyperTarget: null | { id: string; name: string; dist: number; fuel: number; inRange: boolean; sx: number; sy: number; onScreen: boolean; angle: number; aligned: boolean }
  route: { id: string; name: string; dist: number }[]
  fsd: null | { kind: 'sc' | 'jump'; progress: number; label: string }
  flags: { autopilot: boolean; fa: boolean; navLock: boolean; gear: boolean; massLock: boolean; scoop: boolean }
  docking: null | { station: string; bay: string; granted: boolean; dist: number }
  messages: Msg[]
  objective: Objective
  heading: number
  pitch: number
  tab: 'NAV' | 'COM' | 'SHIP' | 'MAP' | 'SYS'
  nav: { id: string; name: string; kind: string; dist: number; sub: string }[]
  system: StarSystem | null
  stationId: string | null
  hyper: null | { phase: 'charging' | 'tunnel' | 'exit'; dest: string; destClass: string; log: string[]; elapsed: number; ai: boolean }
  rects: Record<string, Rect>
  data: DataEntry[]
  missions: Mission[]
  forge: { online: boolean; ai: boolean; model?: string; reason?: string }
  jumpRange: number
  alert: string | null
  markers: { id: string; name: string; kind: string; sx: number; sy: number; dist: string }[]
  day: number
  stick: { x: number; y: number; locked: boolean }
}

export const useHud = create<HudState>(() => ({
  ready: false,
  mode: 'normal',
  overlay: null,
  speed: 0,
  speedLabel: '0',
  throttle: 0,
  boost: false,
  boostCharge: 1,
  pips: { sys: 2, eng: 2, wep: 2 },
  fuel: 16,
  fuelMax: 16,
  hull: 1,
  shields: 1,
  heat: 0.2,
  mass: 0.6,
  cargo: 0,
  cargoCap: 32,
  credits: 0,
  commander: '',
  systemName: '',
  systemSub: [],
  target: null,
  hyperTarget: null,
  route: [],
  fsd: null,
  flags: { autopilot: false, fa: true, navLock: false, gear: false, massLock: false, scoop: false },
  docking: null,
  messages: [],
  objective: { title: '', steps: [] },
  heading: 0,
  pitch: 0,
  tab: 'NAV',
  nav: [],
  system: null,
  stationId: null,
  hyper: null,
  rects: {},
  data: [],
  missions: [],
  forge: { online: false, ai: false },
  jumpRange: 18,
  alert: null,
  markers: [],
  day: 0,
  stick: { x: 0, y: 0, locked: false },
}))

export function fmtDist(m: number): string {
  const LS = 299792458
  if (m < 1000) return `${Math.round(m)} m`
  if (m < 1e6) return `${(m / 1000).toFixed(m < 1e4 ? 1 : 0)} km`
  if (m < LS * 0.5) return `${(m / 1e6).toFixed(1)} Mm`
  if (m < 1.496e11 * 0.5) return `${(m / LS).toFixed(m < LS * 10 ? 1 : 0)} ls`
  return `${(m / 1.496e11).toFixed(2)} AU`
}

export function fmtSpeed(v: number): { value: string; unit: string } {
  const C = 299792458
  if (v < 1000) return { value: `${Math.round(v)}`, unit: 'm/s' }
  if (v < C * 0.5) return { value: `${(v / 1000).toFixed(v < 1e4 ? 1 : 0)}`, unit: 'km/s' }
  return { value: `${(v / C).toFixed(v < C * 10 ? 1 : 0)}`, unit: 'c' }
}

export type { NavObject }
