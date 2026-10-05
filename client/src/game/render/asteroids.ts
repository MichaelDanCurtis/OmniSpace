// Local asteroid fields. Rocks are generated deterministically per spatial cell
// around the ship, so belts and rings look dense up close without storing
// billions of rocks.

import * as THREE from 'three'
import { Rng, hashInts } from '../rng'

export interface Field {
  /** returns 0..1 density at an absolute position */
  density: (p: THREE.Vector3) => number
  /** cheap reject: is the ship anywhere near? */
  near: (p: THREE.Vector3) => boolean
  icy: boolean
  scale: number
}

function rockGeometry(seed: number, detail: number): THREE.BufferGeometry {
  const g = new THREE.IcosahedronGeometry(1, detail)
  const pos = g.attributes.position as THREE.BufferAttribute
  const rng = new Rng(seed)
  const bumps = Array.from({ length: 7 }, () => ({ d: new THREE.Vector3(rng.gauss(), rng.gauss(), rng.gauss()).normalize(), a: rng.range(-0.35, 0.3), w: rng.range(2, 6) }))
  const sx = rng.range(0.7, 1.3)
  const sy = rng.range(0.55, 1)
  const v = new THREE.Vector3()
  for (let i = 0; i < pos.count; i++) {
    v.fromBufferAttribute(pos, i)
    let r = 1
    for (const b of bumps) r += b.a * Math.pow(Math.max(0, v.dot(b.d)), b.w)
    r += (rng.next() - 0.5) * 0.06
    v.multiplyScalar(r)
    pos.setXYZ(i, v.x * sx, v.y * sy, v.z)
  }
  g.computeVertexNormals()
  return g
}

const CELL = 1600
const RANGE = 4 // cells in each direction
const PER_KIND = 1400

export class AsteroidField {
  readonly group = new THREE.Group()
  private meshes: THREE.InstancedMesh[] = []
  private origin = new THREE.Vector3()
  private lastCell = ''
  fields: Field[] = []
  /** absolute positions + radius of current rocks, for collisions */
  rocks: { p: THREE.Vector3; r: number }[] = []

  constructor() {
    const rockMat = new THREE.MeshStandardMaterial({ color: '#6e6660', roughness: 0.95, metalness: 0.05, flatShading: true })
    const iceMat = new THREE.MeshStandardMaterial({ color: '#c8d6e2', roughness: 0.6, metalness: 0.1, flatShading: true })
    for (let k = 0; k < 4; k++) {
      const m = new THREE.InstancedMesh(rockGeometry(100 + k, k < 2 ? 2 : 1), k % 2 === 0 ? rockMat : iceMat, PER_KIND)
      m.count = 0
      m.frustumCulled = false
      this.meshes.push(m)
      this.group.add(m)
    }
  }

  setFields(fields: Field[]) {
    this.fields = fields
    this.lastCell = ''
    this.rocks = []
    for (const m of this.meshes) m.count = 0
  }

  update(shipPos: THREE.Vector3) {
    const active = this.fields.filter((f) => f.near(shipPos))
    const cx = Math.floor(shipPos.x / CELL)
    const cy = Math.floor(shipPos.y / CELL)
    const cz = Math.floor(shipPos.z / CELL)
    const key = `${cx},${cy},${cz},${active.length}`
    if (key !== this.lastCell) {
      this.lastCell = key
      this.rebuild(cx, cy, cz, active)
    }
    this.group.position.set(this.origin.x - shipPos.x, this.origin.y - shipPos.y, this.origin.z - shipPos.z)
  }

  private rebuild(cx: number, cy: number, cz: number, active: Field[]) {
    this.origin.set(cx * CELL, cy * CELL, cz * CELL)
    const counts = [0, 0, 0, 0]
    this.rocks = []
    if (!active.length) {
      for (const m of this.meshes) m.count = 0
      return
    }
    const mtx = new THREE.Matrix4()
    const q = new THREE.Quaternion()
    const e = new THREE.Euler()
    const s = new THREE.Vector3()
    const p = new THREE.Vector3()
    const local = new THREE.Vector3()
    for (let x = -RANGE; x <= RANGE; x++)
      for (let y = -RANGE; y <= RANGE; y++)
        for (let z = -RANGE; z <= RANGE; z++) {
          const rng = new Rng(hashInts(cx + x, cy + y, cz + z, 7))
          for (let i = 0; i < 7; i++) {
            p.set((cx + x + rng.next()) * CELL, (cy + y + rng.next()) * CELL, (cz + z + rng.next()) * CELL)
            let dens = 0
            let icy = false
            let scale = 1
            for (const f of active) {
              const d = f.density(p)
              if (d > dens) {
                dens = d
                icy = f.icy
                scale = f.scale
              }
            }
            const roll = rng.next()
            if (roll > dens) continue
            const kind = (icy ? 1 : 0) + (rng.chance(0.5) ? 2 : 0)
            const kk = kind === 1 || kind === 3 ? kind : kind
            if (counts[kk] >= PER_KIND) continue
            const big = rng.chance(0.06)
            const r = (big ? rng.range(150, 420) : rng.range(8, 90)) * scale
            e.set(rng.range(0, 6.28), rng.range(0, 6.28), rng.range(0, 6.28))
            q.setFromEuler(e)
            s.setScalar(r)
            local.subVectors(p, this.origin)
            mtx.compose(local, q, s)
            this.meshes[kk].setMatrixAt(counts[kk]++, mtx)
            this.rocks.push({ p: p.clone(), r: r * 0.85 })
          }
        }
    this.meshes.forEach((m, i) => {
      m.count = counts[i]
      m.instanceMatrix.needsUpdate = true
    })
  }
}
