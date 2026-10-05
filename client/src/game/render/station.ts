// Procedural station models: 'spire' (concept-art trade spire), 'ring'
// (orbital ring habitat) and 'outpost'. Each exposes docking bays and simple
// signed-distance colliders for flight physics.

import * as THREE from 'three'
import { mergeGeometries } from 'three/examples/jsm/utils/BufferGeometryUtils.js'
import { Rng, hashString } from '../rng'
import type { Station } from '../types'
import { glowTexture } from './star'
import { panelTexture, signTexture, windowTexture } from './textures'

export interface Bay {
  pos: THREE.Vector3 // local
  normal: THREE.Vector3 // local, pointing out of the bay
  label: string
}

export type Collider =
  | { kind: 'cyl'; r: number; y0: number; y1: number }
  | { kind: 'torus'; R: number; r: number }
  | { kind: 'box'; c: THREE.Vector3; h: THREE.Vector3 }

export type MatKey = 'hull' | 'panel' | 'dark' | 'win' | 'win2' | 'orange' | 'cyan' | 'white'

export class Builder {
  parts: Record<string, THREE.BufferGeometry[]> = {}
  add(key: MatKey, geo: THREE.BufferGeometry, pos?: THREE.Vector3 | [number, number, number], rot?: THREE.Euler, scale?: THREE.Vector3) {
    const m = new THREE.Matrix4()
    const p = Array.isArray(pos) ? new THREE.Vector3(...pos) : pos ?? new THREE.Vector3()
    m.compose(p, new THREE.Quaternion().setFromEuler(rot ?? new THREE.Euler()), scale ?? new THREE.Vector3(1, 1, 1))
    const g = geo.index ? geo.toNonIndexed() : geo.clone()
    g.applyMatrix4(m)
    if (!g.attributes.uv) {
      g.setAttribute('uv', new THREE.BufferAttribute(new Float32Array(g.attributes.position.count * 2), 2))
    }
    ;(this.parts[key] ??= []).push(g)
  }
  build(mats: Record<MatKey, THREE.Material>): THREE.Group {
    const group = new THREE.Group()
    for (const [k, list] of Object.entries(this.parts)) {
      const merged = mergeGeometries(list, false)
      if (!merged) continue
      const mesh = new THREE.Mesh(merged, mats[k as MatKey])
      group.add(mesh)
    }
    return group
  }
}

export function materials(seed: number): Record<MatKey, THREE.Material> {
  const panel = panelTexture()
  const win = windowTexture(seed, true)
  const win2 = windowTexture(seed + 7, false)
  return {
    hull: new THREE.MeshStandardMaterial({ color: '#59616c', map: panel, metalness: 0.55, roughness: 0.5 }),
    panel: new THREE.MeshStandardMaterial({ color: '#9aa2ad', map: panel, metalness: 0.45, roughness: 0.55 }),
    dark: new THREE.MeshStandardMaterial({ color: '#16191e', metalness: 0.4, roughness: 0.7 }),
    win: new THREE.MeshStandardMaterial({ color: '#30353d', map: panel, emissive: '#ffffff', emissiveMap: win, emissiveIntensity: 2.2, metalness: 0.5, roughness: 0.5 }),
    win2: new THREE.MeshStandardMaterial({ color: '#30353d', emissive: '#ffffff', emissiveMap: win2, emissiveIntensity: 1.8, metalness: 0.5, roughness: 0.5 }),
    orange: new THREE.MeshBasicMaterial({ color: new THREE.Color('#ffab4a').multiplyScalar(3.2) }),
    cyan: new THREE.MeshBasicMaterial({ color: new THREE.Color('#5cc8ff').multiplyScalar(2.6) }),
    white: new THREE.MeshBasicMaterial({ color: new THREE.Color('#ffffff').multiplyScalar(3) }),
  }
}

function cylUV(r: number, h: number, seg = 96, open = false, repeat = 6) {
  const g = new THREE.CylinderGeometry(r, r, h, seg, 1, open)
  const uv = g.attributes.uv as THREE.BufferAttribute
  for (let i = 0; i < uv.count; i++) uv.setX(i, uv.getX(i) * repeat)
  return g
}

export interface StationModel {
  group: THREE.Group
  spinner: THREE.Group
  bays: Bay[]
  colliders: Collider[]
  spinRate: number
  lights: THREE.Sprite[]
  boundRadius: number
}

function blinkers(group: THREE.Object3D, positions: THREE.Vector3[], color: string, size: number): THREE.Sprite[] {
  const out: THREE.Sprite[] = []
  for (const p of positions) {
    const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTexture(), color: new THREE.Color(color).multiplyScalar(2), blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }))
    s.position.copy(p)
    s.scale.setScalar(size)
    group.add(s)
    out.push(s)
  }
  return out
}

function sign(st: Station, w: number): THREE.Object3D {
  const tex = signTexture(st.name, st.tagline)
  const mat = new THREE.MeshBasicMaterial({ map: tex, transparent: true, blending: THREE.AdditiveBlending, depthWrite: false, side: THREE.FrontSide, color: new THREE.Color(1.6, 1.6, 1.6) })
  const g = new THREE.Group()
  const front = new THREE.Mesh(new THREE.PlaneGeometry(w, w * 0.3125), mat)
  const back = front.clone()
  back.rotation.y = Math.PI
  g.add(front, back)
  return g
}

function buildSpire(st: Station, rng: Rng): StationModel {
  const b = new Builder()
  const D = 1150 // disc radius
  // main concourse disc
  b.add('hull', new THREE.CylinderGeometry(D, D * 0.96, 40, 96), [0, 70, 0])
  b.add('hull', new THREE.CylinderGeometry(D * 0.96, D, 40, 96), [0, -70, 0])
  b.add('win', cylUV(D + 4, 110, 128, true, 10), [0, 0, 0])
  b.add('panel', new THREE.CylinderGeometry(D * 0.7, D * 0.95, 60, 96), [0, 110, 0])
  // upper disc
  b.add('win', cylUV(720, 90, 96, true, 6), [0, 190, 0])
  b.add('hull', new THREE.CylinderGeometry(700, 720, 30, 96), [0, 250, 0])
  b.add('hull', new THREE.CylinderGeometry(720, 760, 30, 96), [0, 135, 0])
  // lower disc + skirt
  b.add('panel', new THREE.CylinderGeometry(D * 0.95, D * 0.55, 140, 96), [0, -160, 0])
  b.add('win2', cylUV(D * 0.55, 120, 96, true, 5), [0, -290, 0])
  b.add('hull', new THREE.CylinderGeometry(D * 0.55, D * 0.35, 120, 64), [0, -410, 0])
  // central column
  b.add('hull', new THREE.CylinderGeometry(150, 170, 3200, 32), [0, 200, 0])
  b.add('win2', cylUV(175, 1200, 32, true, 2), [0, 950, 0])
  b.add('win', cylUV(180, 700, 32, true, 2), [0, -900, 0])
  for (let i = 0; i < 9; i++) {
    const y = -1500 + i * 360
    b.add('panel', new THREE.CylinderGeometry(230 - (i % 3) * 25, 230, 60, 24), [0, y, 0])
  }
  b.add('hull', new THREE.ConeGeometry(150, 900, 24), [0, 2250, 0])
  b.add('panel', new THREE.CylinderGeometry(12, 12, 900, 8), [0, 3050, 0])
  b.add('hull', new THREE.CylinderGeometry(110, 40, 500, 24), [0, -2050, 0])
  // side towers on the disc
  const towers = 7
  for (let i = 0; i < towers; i++) {
    const a = (i / towers) * Math.PI * 2 + 0.2
    const r = rng.range(560, 880)
    const h = rng.range(160, 520)
    const x = Math.cos(a) * r
    const z = Math.sin(a) * r
    b.add('win2', new THREE.BoxGeometry(70, h, 70), [x, 140 + h / 2, z])
    b.add('hull', new THREE.BoxGeometry(90, 24, 90), [x, 140 + h, z])
    b.add('panel', new THREE.CylinderGeometry(4, 4, h * 0.6, 6), [x, 140 + h * 1.3, z])
  }
  // radial struts under the disc
  for (let i = 0; i < 12; i++) {
    const a = (i / 12) * Math.PI * 2
    b.add('dark', new THREE.BoxGeometry(40, 30, D * 0.9), [Math.cos(a) * D * 0.5, -240, Math.sin(a) * D * 0.5], new THREE.Euler(0, -a + Math.PI / 2, 0))
  }
  // light rings (orange accents like the concept art)
  b.add('orange', new THREE.TorusGeometry(D + 10, 5, 6, 160), [0, -58, 0], new THREE.Euler(Math.PI / 2, 0, 0))
  b.add('orange', new THREE.TorusGeometry(D + 10, 3, 6, 160), [0, 58, 0], new THREE.Euler(Math.PI / 2, 0, 0))
  b.add('orange', new THREE.TorusGeometry(730, 3, 6, 128), [0, 238, 0], new THREE.Euler(Math.PI / 2, 0, 0))
  b.add('cyan', new THREE.TorusGeometry(D * 0.56, 4, 6, 128), [0, -350, 0], new THREE.Euler(Math.PI / 2, 0, 0))
  // docking bays
  const bays: Bay[] = []
  const nb = 4
  for (let i = 0; i < nb; i++) {
    const a = (i / nb) * Math.PI * 2 + Math.PI / 4
    const n = new THREE.Vector3(Math.cos(a), 0, Math.sin(a))
    const rot = new THREE.Euler(0, -a + Math.PI / 2, 0)
    const base = n.clone().multiplyScalar(D + 20)
    b.add('dark', new THREE.BoxGeometry(260, 86, 60), base, rot)
    const t = new THREE.Vector3(-n.z, 0, n.x)
    b.add('cyan', new THREE.BoxGeometry(280, 6, 6), base.clone().add(new THREE.Vector3(0, 46, 0)).addScaledVector(n, 32), rot)
    b.add('cyan', new THREE.BoxGeometry(280, 6, 6), base.clone().add(new THREE.Vector3(0, -46, 0)).addScaledVector(n, 32), rot)
    b.add('orange', new THREE.BoxGeometry(6, 92, 6), base.clone().addScaledVector(t, 140).addScaledVector(n, 32), rot)
    b.add('orange', new THREE.BoxGeometry(6, 92, 6), base.clone().addScaledVector(t, -140).addScaledVector(n, 32), rot)
    bays.push({ pos: n.clone().multiplyScalar(D + 40), normal: n, label: String(i + 1).padStart(2, '0') })
  }
  const mats = materials(hashString(st.id) % 1000)
  const model = b.build(mats)
  const spinner = new THREE.Group()
  spinner.add(model)
  const s = sign(st, 900)
  const signA = Math.PI / 4 + Math.PI / 4 // between bay 1 and 2
  s.position.set(Math.cos(signA) * (D + 140), 330, Math.sin(signA) * (D + 140))
  s.lookAt(s.position.clone().multiplyScalar(2))
  spinner.add(s)
  const lights = blinkers(
    spinner,
    [new THREE.Vector3(0, 3500, 0), new THREE.Vector3(0, -2300, 0), ...bays.map((bb) => bb.pos.clone().addScaledVector(bb.normal, 30).add(new THREE.Vector3(0, 60, 0)))],
    '#ff5040',
    90,
  )
  const group = new THREE.Group()
  group.add(spinner)
  return {
    group,
    spinner,
    bays,
    colliders: [
      { kind: 'cyl', r: D + 15, y0: -95, y1: 95 },
      { kind: 'cyl', r: 770, y0: 95, y1: 270 },
      { kind: 'cyl', r: D * 0.95, y0: -240, y1: -95 },
      { kind: 'cyl', r: D * 0.56, y0: -480, y1: -240 },
      { kind: 'cyl', r: 240, y0: -2300, y1: 2700 },
    ],
    spinRate: (Math.PI * 2) / 900,
    lights,
    boundRadius: 3600,
  }
}

function buildRing(st: Station, rng: Rng): StationModel {
  const b = new Builder()
  const R = 1800
  const torus = new THREE.TorusGeometry(R, 170, 24, 160)
  const uv = torus.attributes.uv as THREE.BufferAttribute
  for (let i = 0; i < uv.count; i++) uv.setXY(i, uv.getX(i) * 24, uv.getY(i) * 1)
  b.add('win', torus, [0, 0, 0], new THREE.Euler(Math.PI / 2, 0, 0))
  b.add('hull', new THREE.TorusGeometry(R, 120, 12, 160), [0, 150, 0], new THREE.Euler(Math.PI / 2, 0, 0))
  b.add('orange', new THREE.TorusGeometry(R + 175, 6, 6, 200), [0, 0, 0], new THREE.Euler(Math.PI / 2, 0, 0))
  for (let i = 0; i < 6; i++) {
    const a = (i / 6) * Math.PI * 2
    b.add('panel', new THREE.BoxGeometry(70, 70, R), [Math.cos(a) * R * 0.5, 0, Math.sin(a) * R * 0.5], new THREE.Euler(0, -a + Math.PI / 2, 0))
  }
  b.add('hull', new THREE.CylinderGeometry(280, 280, 900, 48), [0, 0, 0])
  b.add('win2', cylUV(290, 400, 48, true, 3), [0, 0, 0])
  b.add('panel', new THREE.CylinderGeometry(330, 280, 80, 48), [0, 470, 0])
  b.add('dark', new THREE.BoxGeometry(220, 30, 90), [0, 512, 0])
  b.add('cyan', new THREE.BoxGeometry(240, 6, 6), [0, 530, 48])
  b.add('cyan', new THREE.BoxGeometry(240, 6, 6), [0, 530, -48])
  b.add('hull', new THREE.ConeGeometry(260, 600, 32), [0, -760, 0], new THREE.Euler(Math.PI, 0, 0))
  const mats = materials(hashString(st.id) % 1000)
  const model = b.build(mats)
  const spinner = new THREE.Group()
  spinner.add(model)
  const s = sign(st, 700)
  s.position.set(0, 900, 0)
  s.lookAt(0, 900, 2000)
  spinner.add(s)
  const bays: Bay[] = [{ pos: new THREE.Vector3(0, 560, 0), normal: new THREE.Vector3(0, 1, 0), label: '01' }]
  const lights = blinkers(spinner, [new THREE.Vector3(R + 180, 0, 0), new THREE.Vector3(-R - 180, 0, 0), new THREE.Vector3(0, 0, R + 180), new THREE.Vector3(0, 0, -R - 180), new THREE.Vector3(0, 600, 0)], '#ff5040', 110)
  const group = new THREE.Group()
  group.add(spinner)
  void rng
  return {
    group,
    spinner,
    bays,
    colliders: [
      { kind: 'torus', R, r: 300 },
      { kind: 'cyl', r: 340, y0: -1100, y1: 510 },
    ],
    spinRate: (Math.PI * 2) / 240,
    lights,
    boundRadius: 2400,
  }
}

function buildOutpost(st: Station, rng: Rng): StationModel {
  const b = new Builder()
  b.add('panel', new THREE.BoxGeometry(60, 60, 1400), [0, 0, 0])
  b.add('panel', new THREE.BoxGeometry(1000, 50, 50), [0, 0, 0])
  for (let i = 0; i < 5; i++) {
    const z = -600 + i * 300
    b.add('win2', new THREE.CylinderGeometry(90, 90, 220, 20), [rng.range(-80, 80), rng.range(-30, 60), z], new THREE.Euler(0, 0, Math.PI / 2))
  }
  b.add('hull', new THREE.BoxGeometry(260, 30, 260), [420, 0, 0])
  b.add('cyan', new THREE.BoxGeometry(250, 4, 4), [420, 18, 128])
  b.add('cyan', new THREE.BoxGeometry(250, 4, 4), [420, 18, -128])
  b.add('orange', new THREE.BoxGeometry(4, 4, 250), [548, 18, 0])
  b.add('panel', new THREE.BoxGeometry(260, 30, 260), [-420, 0, 0])
  b.add('dark', new THREE.BoxGeometry(800, 4, 300), [0, 0, 760])
  b.add('panel', new THREE.CylinderGeometry(6, 6, 600, 6), [0, 330, -600])
  const mats = materials(hashString(st.id) % 1000)
  const model = b.build(mats)
  const spinner = new THREE.Group()
  spinner.add(model)
  const s = sign(st, 420)
  s.position.set(0, 160, 0)
  s.lookAt(0, 160, 1000)
  spinner.add(s)
  const bays: Bay[] = [{ pos: new THREE.Vector3(420, 40, 0), normal: new THREE.Vector3(0, 1, 0), label: '01' }]
  const lights = blinkers(spinner, [new THREE.Vector3(0, 640, -600), new THREE.Vector3(420, 30, 130), new THREE.Vector3(420, 30, -130)], '#ff5040', 60)
  const group = new THREE.Group()
  group.add(spinner)
  return {
    group,
    spinner,
    bays,
    colliders: [
      { kind: 'box', c: new THREE.Vector3(0, 0, 0), h: new THREE.Vector3(110, 110, 760) },
      { kind: 'box', c: new THREE.Vector3(420, 0, 0), h: new THREE.Vector3(140, 20, 140) },
      { kind: 'box', c: new THREE.Vector3(-420, 0, 0), h: new THREE.Vector3(140, 20, 140) },
    ],
    spinRate: 0,
    lights,
    boundRadius: 1100,
  }
}

export function buildStation(st: Station): StationModel {
  const rng = new Rng(hashString(st.id))
  if (st.kind === 'ring') return buildRing(st, rng)
  if (st.kind === 'outpost') return buildOutpost(st, rng)
  return buildSpire(st, rng)
}

/** Signed distance from a local-space point to the station hull. */
export function stationSDF(cols: Collider[], p: THREE.Vector3): number {
  let d = Infinity
  for (const c of cols) {
    let v: number
    if (c.kind === 'cyl') {
      const dr = Math.hypot(p.x, p.z) - c.r
      const cy = (c.y0 + c.y1) / 2
      const hy = (c.y1 - c.y0) / 2
      const dy = Math.abs(p.y - cy) - hy
      v = Math.min(Math.max(dr, dy), 0) + Math.hypot(Math.max(dr, 0), Math.max(dy, 0))
    } else if (c.kind === 'torus') {
      const q = Math.hypot(p.x, p.z) - c.R
      v = Math.hypot(q, p.y) - c.r
    } else {
      const qx = Math.abs(p.x - c.c.x) - c.h.x
      const qy = Math.abs(p.y - c.c.y) - c.h.y
      const qz = Math.abs(p.z - c.c.z) - c.h.z
      v = Math.hypot(Math.max(qx, 0), Math.max(qy, 0), Math.max(qz, 0)) + Math.min(Math.max(qx, qy, qz), 0)
    }
    if (v < d) d = v
  }
  return d
}
