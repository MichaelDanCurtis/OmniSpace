// Resolves orbital parameters into absolute positions (metres, star at origin).

import type { Body, NavObject, POI, StarSystem, Station } from './types'
import { SOLAR_RADIUS } from './types'

export type V3 = [number, number, number]

export function orbitOffset(r: number, phase: number, inc: number): V3 {
  return [r * Math.cos(phase), r * Math.sin(phase) * Math.sin(inc), r * Math.sin(phase) * Math.cos(inc)]
}

export class SystemLayout {
  readonly sys: StarSystem
  readonly positions = new Map<string, V3>()
  readonly nav: NavObject[] = []
  readonly starRadius: number

  constructor(sys: StarSystem) {
    this.sys = sys
    this.starRadius = sys.star.radiusSolar * SOLAR_RADIUS
    this.positions.set('star', [0, 0, 0])
    const bodies = new Map(sys.bodies.map((b) => [b.id, b]))
    const resolve = (b: Body, depth = 0): V3 => {
      const cached = this.positions.get(b.id)
      if (cached) return cached
      const parent = b.parent === 'star' || depth > 4 ? ([0, 0, 0] as V3) : resolve(bodies.get(b.parent) ?? ({ ...b, parent: 'star' } as Body), depth + 1)
      const o = orbitOffset(b.orbitM, b.phase, b.inclination)
      const p: V3 = [parent[0] + o[0], parent[1] + o[1], parent[2] + o[2]]
      this.positions.set(b.id, p)
      return p
    }
    this.nav.push({ id: 'star', name: sys.name, kind: 'star', pos: [0, 0, 0], radius: this.starRadius, subtitle: sys.starFlavor ?? sys.star.cls, ref: sys.star })
    for (const b of sys.bodies) {
      const p = resolve(b)
      this.nav.push({ id: b.id, name: b.name, kind: b.kind, pos: p, radius: b.radiusM, subtitle: `${b.kind === 'moon' ? 'Moon · ' : ''}${b.type.replace('_', ' ')}`, ref: b })
    }
    for (const s of sys.stations) {
      const host = this.positions.get(s.parent) ?? [0, 0, 0]
      const o = orbitOffset(s.orbitM, s.phase, s.inclination)
      const p: V3 = [host[0] + o[0], host[1] + o[1], host[2] + o[2]]
      this.positions.set(s.id, p)
      this.nav.push({ id: s.id, name: s.name, kind: 'station', pos: p, radius: 2500, subtitle: `${s.faction} · ${s.economy}`, ref: s })
    }
    for (const poi of sys.pois) {
      const host = this.positions.get(poi.parent) ?? [0, 0, 0]
      const o = orbitOffset(poi.offsetM, poi.phase, 0.2)
      const p: V3 = [host[0] + o[0], host[1] + o[1], host[2] + o[2]]
      this.positions.set(poi.id, p)
      this.nav.push({ id: poi.id, name: poi.name, kind: 'poi', pos: p, radius: 400, subtitle: poi.kind, ref: poi })
    }
    for (const belt of sys.belts) {
      const host = this.positions.get(belt.parent) ?? [0, 0, 0]
      const o = orbitOffset(belt.radiusM, 0.9, 0)
      const p: V3 = [host[0] + o[0], host[1] + o[1], host[2] + o[2]]
      this.positions.set(belt.id, p)
      this.nav.push({ id: belt.id, name: belt.name, kind: 'belt', pos: p, radius: 1000, subtitle: `${belt.composition} asteroid belt`, ref: belt })
    }
  }

  body(id: string): Body | undefined {
    return this.sys.bodies.find((b) => b.id === id)
  }
  station(id: string): Station | undefined {
    return this.sys.stations.find((s) => s.id === id)
  }
  poi(id: string): POI | undefined {
    return this.sys.pois.find((s) => s.id === id)
  }
  navById(id: string): NavObject | undefined {
    return this.nav.find((n) => n.id === id)
  }
}
