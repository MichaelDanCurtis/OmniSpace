// Builds and updates every visual for the current star system using a
// floating origin: absolute positions are kept in float64 on the CPU and each
// frame objects are placed relative to the ship (camera at the origin).

import * as THREE from 'three'
import { starSeed } from '../galaxy'
import { SystemLayout } from '../layout'
import { Rng, hashString } from '../rng'
import type { StarSystem } from '../types'
import { AsteroidField, type Field } from './asteroids'
import { PlanetVisual } from './planet'
import { makeFreighter, Traffic } from './ships'
import { buildStation, type StationModel } from './station'
import { glowTexture, StarVisual } from './star'

export interface StationRuntime {
  id: string
  model: StationModel
  traffic: Traffic
  abs: THREE.Vector3
}

export interface PoiRuntime {
  id: string
  obj: THREE.Object3D
  abs: THREE.Vector3
}

export class SystemScene {
  readonly root = new THREE.Group()
  readonly layout: SystemLayout
  readonly sys: StarSystem
  readonly star: StarVisual
  readonly planets = new Map<string, PlanetVisual>()
  readonly bodyAbs = new Map<string, THREE.Vector3>()
  readonly stations = new Map<string, StationRuntime>()
  readonly pois = new Map<string, PoiRuntime>()
  readonly asteroids = new AsteroidField()
  readonly ambient: THREE.AmbientLight
  readonly skyTilt: THREE.Quaternion
  private sunRel = new THREE.Vector3()
  private tmp = new THREE.Vector3()

  constructor(sys: StarSystem) {
    this.sys = sys
    this.layout = new SystemLayout(sys)
    const seed = starSeed(sys.id)
    const rng = new Rng(seed)
    this.skyTilt = new THREE.Quaternion().setFromEuler(new THREE.Euler(rng.range(-0.5, 0.5), rng.range(0, Math.PI * 2), rng.range(-0.5, 0.5)))
    if (sys.id === 'kelvar') this.skyTilt.setFromEuler(new THREE.Euler(0.35, 2.2, 0.15))

    this.star = new StarVisual(sys.star)
    this.root.add(this.star.group)
    this.root.add(this.star.light)
    this.ambient = new THREE.AmbientLight('#9ab0d0', 0.22)
    this.root.add(this.ambient)

    const settled = sys.profile.population > 1e6 ? Math.min(1, Math.log10(sys.profile.population) / 10) : 0
    const sunColor = this.star.sunColor()
    for (const b of sys.bodies) {
      const pv = new PlanetVisual(b, hashString(b.id + sys.id), settled)
      pv.setSunColor(sunColor)
      this.planets.set(b.id, pv)
      const p = this.layout.positions.get(b.id)!
      this.bodyAbs.set(b.id, new THREE.Vector3(...p))
      this.root.add(pv.group)
    }
    for (const st of sys.stations) {
      const model = buildStation(st)
      const p = this.layout.positions.get(st.id)!
      const abs = new THREE.Vector3(...p)
      // tilt station to the host's equatorial-ish plane
      const host = sys.bodies.find((b) => b.id === st.parent)
      model.group.rotation.z = (host?.tilt ?? 0) * 0.5
      const traffic = new Traffic(model, hashString(st.id), st.kind === 'outpost' ? 3 : st.kind === 'ring' ? 7 : 10)
      model.group.add(traffic.group)
      this.root.add(model.group)
      this.stations.set(st.id, { id: st.id, model, traffic, abs })
    }
    for (const poi of sys.pois) {
      const p = this.layout.positions.get(poi.id)!
      const obj = this.makePoi(poi.kind, hashString(poi.id))
      this.root.add(obj)
      this.pois.set(poi.id, { id: poi.id, obj, abs: new THREE.Vector3(...p) })
    }
    this.root.add(this.asteroids.group)
    this.asteroids.setFields(this.buildFields())
  }

  private makePoi(kind: string, seed: number): THREE.Object3D {
    const rng = new Rng(seed)
    const g = new THREE.Group()
    if (kind === 'derelict' || kind === 'wreck') {
      const n = kind === 'wreck' ? 6 : 1
      for (let i = 0; i < n; i++) {
        const f = makeFreighter(rng)
        f.traverse((o) => {
          const m = o as THREE.Mesh
          if ((m as unknown as THREE.Sprite).isSprite) o.visible = false
          if (m.isMesh) m.material = (m.material as THREE.MeshStandardMaterial).clone()
          if (m.isMesh) (m.material as THREE.MeshStandardMaterial).color?.multiplyScalar(0.45)
        })
        f.scale.setScalar(kind === 'derelict' ? 3 : rng.range(0.6, 1.4))
        f.position.set(rng.range(-400, 400) * (n > 1 ? 1 : 0), rng.range(-200, 200) * (n > 1 ? 1 : 0), rng.range(-400, 400) * (n > 1 ? 1 : 0))
        f.rotation.set(rng.range(0, 6), rng.range(0, 6), rng.range(0, 6))
        g.add(f)
      }
    }
    const color = kind === 'anomaly' ? '#b070ff' : kind === 'beacon' ? '#40ff9a' : kind === 'signal' ? '#ffcf40' : '#ff5a3a'
    const glow = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTexture(), color: new THREE.Color(color).multiplyScalar(2.5), blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }))
    glow.scale.setScalar(kind === 'anomaly' ? 1500 : 220)
    g.add(glow)
    g.userData.glow = glow
    g.userData.kind = kind
    if (kind === 'beacon') {
      const mast = new THREE.Mesh(new THREE.CylinderGeometry(6, 10, 300, 8), new THREE.MeshStandardMaterial({ color: '#7a828c', metalness: 0.6, roughness: 0.4 }))
      g.add(mast)
    }
    if (kind === 'anomaly') {
      const s = new THREE.Mesh(new THREE.IcosahedronGeometry(600, 3), new THREE.MeshBasicMaterial({ color: new THREE.Color('#7a40ff').multiplyScalar(1.2), wireframe: true, transparent: true, opacity: 0.35, blending: THREE.AdditiveBlending, depthWrite: false }))
      g.add(s)
      g.userData.spin = s
    }
    return g
  }

  private buildFields(): Field[] {
    const fields: Field[] = []
    for (const belt of this.sys.belts) {
      const c = this.layout.positions.get(belt.parent) ?? [0, 0, 0]
      const center = new THREE.Vector3(...c)
      const R = belt.radiusM
      const half = belt.widthM / 2
      const thick = Math.max(2e6, belt.widthM * 0.02)
      fields.push({
        icy: belt.composition.includes('ic'),
        scale: 1,
        near: (p) => {
          const dx = p.x - center.x
          const dz = p.z - center.z
          const r = Math.hypot(dx, dz)
          return Math.abs(r - R) < half && Math.abs(p.y - center.y) < thick
        },
        density: (p) => {
          const r = Math.hypot(p.x - center.x, p.z - center.z)
          const t = 1 - Math.abs(r - R) / half
          return Math.max(0, t) * 0.55 * Math.max(0, 1 - Math.abs(p.y - center.y) / thick)
        },
      })
    }
    for (const b of this.sys.bodies) {
      if (!b.rings) continue
      const center = this.bodyAbs.get(b.id)!
      const tilt = b.tilt ?? 0
      const normal = new THREE.Vector3(0, 1, 0).applyAxisAngle(new THREE.Vector3(0, 0, 1), tilt)
      const rin = b.rings.innerM
      const rout = b.rings.outerM
      const thick = 2500
      const d = new THREE.Vector3()
      fields.push({
        icy: true,
        scale: 0.8,
        near: (p) => {
          d.subVectors(p, center)
          const h = d.dot(normal)
          const r = Math.sqrt(Math.max(0, d.lengthSq() - h * h))
          return Math.abs(h) < thick * 3 && r > rin * 0.98 && r < rout * 1.02
        },
        density: (p) => {
          d.subVectors(p, center)
          const h = d.dot(normal)
          const r = Math.sqrt(Math.max(0, d.lengthSq() - h * h))
          if (r < rin || r > rout) return 0
          return 0.9 * Math.max(0, 1 - Math.abs(h) / thick) * b.rings!.opacity
        },
      })
    }
    // ice haulage debris around stations above ringed worlds (concept-art look)
    for (const st of this.stations.values()) {
      const host = this.sys.bodies.find((b) => b.id === this.sys.stations.find((s) => s.id === st.id)?.parent)
      if (!host?.rings) continue
      const c = st.abs
      const d = new THREE.Vector3()
      fields.push({
        icy: true,
        scale: 0.6,
        near: (p) => p.distanceTo(c) < 40000,
        density: (p) => {
          const r = d.subVectors(p, c).length()
          if (r < 4500) return 0
          return 0.12 * Math.max(0, 1 - r / 30000)
        },
      })
    }
    return fields
  }

  /** Per-frame update. */
  update(shipPos: THREE.Vector3, dt: number, time: number, showAsteroids: boolean) {
    const rel = (abs: THREE.Vector3, out: THREE.Object3D) => out.position.set(abs.x - shipPos.x, abs.y - shipPos.y, abs.z - shipPos.z)
    this.sunRel.set(-shipPos.x, -shipPos.y, -shipPos.z)
    this.star.group.position.copy(this.sunRel)
    this.star.light.position.copy(this.sunRel)
    this.star.update(this.sunRel.length(), time)
    for (const [id, pv] of this.planets) {
      rel(this.bodyAbs.get(id)!, pv.group)
      pv.update(this.sunRel, time)
    }
    for (const st of this.stations.values()) {
      rel(st.abs, st.model.group)
      const dist = st.model.group.position.length()
      st.model.group.visible = dist < 3e7
      st.model.spinner.rotation.y = time * st.model.spinRate
      if (dist < 80000) st.traffic.update(dt)
      const blink = Math.sin(time * 3) > 0.6
      for (const l of st.model.lights) l.visible = blink
    }
    for (const p of this.pois.values()) {
      rel(p.abs, p.obj)
      p.obj.visible = p.obj.position.length() < 2e7
      const glow = p.obj.userData.glow as THREE.Sprite
      glow.material.opacity = 0.6 + 0.4 * Math.sin(time * 2.4)
      if (p.obj.userData.spin) (p.obj.userData.spin as THREE.Object3D).rotation.y = time * 0.3
    }
    this.asteroids.group.visible = showAsteroids
    if (showAsteroids) this.asteroids.update(shipPos)
    void this.tmp
  }

  /** Absolute position of a station bay (mouth) right now. */
  bayWorld(stationId: string, bayIndex: number): { pos: THREE.Vector3; normal: THREE.Vector3 } | null {
    const st = this.stations.get(stationId)
    if (!st) return null
    const bay = st.model.bays[bayIndex % st.model.bays.length]
    st.model.group.updateMatrixWorld(true)
    const q = new THREE.Quaternion()
    st.model.spinner.getWorldQuaternion(q)
    // The group's world matrix includes the floating-origin offset; compute
    // orientation only and add to the absolute station position.
    const local = bay.pos.clone().applyQuaternion(q)
    const n = bay.normal.clone().applyQuaternion(q)
    return { pos: st.abs.clone().add(local), normal: n }
  }

  stationLocal(stationId: string, abs: THREE.Vector3): THREE.Vector3 | null {
    const st = this.stations.get(stationId)
    if (!st) return null
    const q = new THREE.Quaternion()
    st.model.spinner.getWorldQuaternion(q)
    return abs.clone().sub(st.abs).applyQuaternion(q.invert())
  }

  stationWorldQuat(stationId: string): THREE.Quaternion {
    const q = new THREE.Quaternion()
    this.stations.get(stationId)?.model.spinner.getWorldQuaternion(q)
    return q
  }

  sunColor() {
    return this.star.sunColor()
  }

  dispose() {
    for (const pv of this.planets.values()) pv.dispose()
    for (const st of this.stations.values()) st.traffic.dispose()
    this.root.traverse((o) => {
      const m = o as THREE.Mesh
      if (m.geometry) m.geometry.dispose()
    })
    this.root.clear()
  }
}
