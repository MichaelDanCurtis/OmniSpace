// NPC ship models (container freighters like the concept art, fighters,
// shuttles) and a simple traffic controller that flies them in and out of a
// station's docking bays.

import * as THREE from 'three'
import { Rng } from '../rng'
import type { Bay, StationModel } from './station'
import { glowTexture } from './star'
import { disposeTree } from './dispose'
import { panelTexture } from './textures'

let shared: { hull: THREE.Material; dark: THREE.Material; cont: THREE.Material[]; cab: THREE.Material; glow: THREE.Material } | null = null
function mats() {
  if (shared) return shared
  const panel = panelTexture()
  shared = {
    hull: new THREE.MeshStandardMaterial({ color: '#8b929c', map: panel, metalness: 0.5, roughness: 0.5 }),
    dark: new THREE.MeshStandardMaterial({ color: '#24282e', metalness: 0.5, roughness: 0.6 }),
    cont: ['#d8d4cc', '#3c4c6a', '#9a5a3a', '#6a7a5a', '#e2e4e6'].map((c) => new THREE.MeshStandardMaterial({ color: c, map: panel, metalness: 0.3, roughness: 0.7 })),
    cab: new THREE.MeshStandardMaterial({ color: '#2a2f36', emissive: new THREE.Color('#ffd7a0'), emissiveIntensity: 0.25, metalness: 0.6, roughness: 0.4 }),
    glow: new THREE.MeshBasicMaterial({ color: new THREE.Color('#ff3020').multiplyScalar(2) }),
  }
  for (const m of [shared.hull, shared.dark, shared.cab, shared.glow, ...shared.cont]) m.userData.shared = true
  return shared
}

function engineGlow(color: string, size: number) {
  const s = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTexture(), color: new THREE.Color(color).multiplyScalar(3), blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }))
  s.scale.setScalar(size)
  return s
}

/** Ship models face -Z (forward) like the camera. */
export function makeFreighter(rng: Rng): THREE.Group {
  const m = mats()
  const g = new THREE.Group()
  const n = rng.int(3, 5)
  const len = 34
  for (let i = 0; i < n; i++) {
    const c = new THREE.Mesh(new THREE.BoxGeometry(22, 22, len), m.cont[rng.int(0, m.cont.length - 1)])
    c.position.z = i * (len + 3)
    g.add(c)
    const c2 = new THREE.Mesh(new THREE.BoxGeometry(22, 22, len), m.cont[rng.int(0, m.cont.length - 1)])
    c2.position.set(0, 23, i * (len + 3))
    if (rng.chance(0.7)) g.add(c2)
  }
  const spine = new THREE.Mesh(new THREE.BoxGeometry(8, 8, n * (len + 3) + 20), m.dark)
  spine.position.set(0, -14, ((n - 1) * (len + 3)) / 2)
  g.add(spine)
  const cab = new THREE.Mesh(new THREE.BoxGeometry(26, 20, 26), m.cab)
  cab.position.set(0, 4, -len / 2 - 16)
  g.add(cab)
  const nose = new THREE.Mesh(new THREE.ConeGeometry(13, 22, 4), m.hull)
  nose.rotation.x = -Math.PI / 2
  nose.rotation.y = Math.PI / 4
  nose.position.set(0, 4, -len / 2 - 40)
  g.add(nose)
  const tail = (n - 1) * (len + 3) + len / 2
  const eng = new THREE.Mesh(new THREE.BoxGeometry(34, 30, 16), m.hull)
  eng.position.set(0, 8, tail + 8)
  g.add(eng)
  for (const x of [-9, 9]) {
    const s = engineGlow('#5aa8ff', 40)
    s.position.set(x, 8, tail + 20)
    g.add(s)
  }
  g.userData.length = tail + 60
  return g
}

export function makeFighter(rng: Rng): THREE.Group {
  const m = mats()
  const g = new THREE.Group()
  const shape = new THREE.Shape()
  shape.moveTo(0, -22)
  shape.lineTo(12, 10)
  shape.lineTo(5, 14)
  shape.lineTo(-5, 14)
  shape.lineTo(-12, 10)
  shape.closePath()
  const body = new THREE.Mesh(new THREE.ExtrudeGeometry(shape, { depth: 4, bevelEnabled: true, bevelThickness: 1.5, bevelSize: 1.5, bevelSegments: 1 }), m.hull)
  body.rotation.x = -Math.PI / 2
  g.add(body)
  const cock = new THREE.Mesh(new THREE.SphereGeometry(3.5, 12, 8), m.cab)
  cock.scale.set(1, 0.6, 2)
  cock.position.set(0, 4, -6)
  g.add(cock)
  for (const x of [-4, 4]) {
    const s = engineGlow(rng.chance(0.5) ? '#ff9a3a' : '#5aa8ff', 14)
    s.position.set(x, 2, 15)
    g.add(s)
  }
  g.userData.length = 40
  return g
}

export function makeShuttle(): THREE.Group {
  const m = mats()
  const g = new THREE.Group()
  const b = new THREE.Mesh(new THREE.CapsuleGeometry(7, 26, 4, 10), m.hull)
  b.rotation.x = Math.PI / 2
  g.add(b)
  const w = new THREE.Mesh(new THREE.BoxGeometry(34, 2, 10), m.dark)
  w.position.z = 6
  g.add(w)
  const s = engineGlow('#ffd28a', 18)
  s.position.z = 22
  g.add(s)
  g.userData.length = 40
  return g
}

interface Npc {
  obj: THREE.Group
  path: THREE.Vector3[] // in station frame (non-spinning)
  seg: number
  speed: number
  bay: number
  inbound: boolean
  done: boolean
}

/** Flies NPCs in and out of a station. Positions are station-relative. */
export class Traffic {
  readonly group = new THREE.Group()
  private npcs: Npc[] = []
  private rng: Rng
  private model: StationModel

  constructor(model: StationModel, seed: number, count: number) {
    this.model = model
    this.rng = new Rng(seed)
    for (let i = 0; i < count; i++) this.spawn(this.rng.chance(0.5), true)
  }

  private bayWorld(b: Bay): { pos: THREE.Vector3; n: THREE.Vector3 } {
    const q = this.model.spinner.quaternion
    return { pos: b.pos.clone().applyQuaternion(q), n: b.normal.clone().applyQuaternion(q) }
  }

  private spawn(inbound: boolean, scatter: boolean) {
    const r = this.rng
    const kind = r.next()
    const obj = kind < 0.45 ? makeFreighter(r) : kind < 0.8 ? makeFighter(r) : makeShuttle()
    const bayIdx = r.int(0, this.model.bays.length - 1)
    const { pos, n } = this.bayWorld(this.model.bays[bayIdx])
    const far = new THREE.Vector3(r.range(-1, 1), r.range(-0.4, 0.4), r.range(-1, 1)).normalize().multiplyScalar(r.range(9000, 16000))
    const approach = pos.clone().addScaledVector(n, 1800)
    const mouth = pos.clone().addScaledVector(n, 120)
    const path = inbound ? [far, approach, mouth, pos.clone()] : [pos.clone(), mouth, approach, far]
    const npc: Npc = { obj, path, seg: 0, speed: kind < 0.45 ? r.range(90, 160) : r.range(160, 280), bay: bayIdx, inbound, done: false }
    obj.position.copy(path[0])
    if (scatter) {
      // start somewhere along the route
      npc.seg = r.int(0, 1)
      obj.position.lerpVectors(path[npc.seg], path[npc.seg + 1], r.next())
    }
    this.group.add(obj)
    this.npcs.push(npc)
  }

  update(dt: number) {
    const tmp = new THREE.Vector3()
    for (const n of this.npcs) {
      const target = n.path[n.seg + 1]
      if (!target) {
        n.done = true
        continue
      }
      tmp.subVectors(target, n.obj.position)
      const d = tmp.length()
      const slow = n.inbound && n.seg >= 1 ? Math.max(0.25, Math.min(1, d / 1500)) : 1
      const step = n.speed * slow * dt
      if (d <= step) {
        n.obj.position.copy(target)
        n.seg++
      } else {
        tmp.multiplyScalar(1 / d)
        n.obj.position.addScaledVector(tmp, step)
        const look = new THREE.Matrix4().lookAt(new THREE.Vector3(), tmp, new THREE.Vector3(0, 1, 0))
        const q = new THREE.Quaternion().setFromRotationMatrix(look)
        n.obj.quaternion.slerp(q, Math.min(1, dt * 1.5))
      }
    }
    const done = this.npcs.filter((n) => n.done)
    if (done.length) {
      for (const n of done) {
        this.group.remove(n.obj)
        disposeTree(n.obj)
        this.spawn(!n.inbound, false)
      }
      this.npcs = this.npcs.filter((n) => !n.done)
    }
  }

  /** Contacts for radar, station-relative. */
  contacts(): THREE.Vector3[] {
    return this.npcs.map((n) => n.obj.position)
  }

  dispose() {
    disposeTree(this.group)
    this.group.clear()
    this.npcs = []
  }
}
