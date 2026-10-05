// Space dust: short streaks around the ship that make velocity readable, the
// way Elite's "space dust" does. Particles live in a cube that wraps around
// the camera.

import * as THREE from 'three'
import { Rng } from '../rng'

const N = 700
const BOX = 360

export class SpaceDust {
  readonly lines: THREE.LineSegments
  private pts: Float32Array
  private geo: THREE.BufferGeometry
  private pos: Float32Array
  private mat: THREE.LineBasicMaterial

  constructor() {
    const rng = new Rng(5)
    this.pts = new Float32Array(N * 3)
    for (let i = 0; i < N * 3; i++) this.pts[i] = rng.range(-BOX / 2, BOX / 2)
    this.pos = new Float32Array(N * 6)
    this.geo = new THREE.BufferGeometry()
    this.geo.setAttribute('position', new THREE.BufferAttribute(this.pos, 3))
    this.mat = new THREE.LineBasicMaterial({ color: new THREE.Color(0.75, 0.82, 0.95), transparent: true, opacity: 0.5, depthWrite: false, blending: THREE.AdditiveBlending })
    this.lines = new THREE.LineSegments(this.geo, this.mat)
    this.lines.frustumCulled = false
    this.lines.renderOrder = 5
  }

  /** move: displacement this frame (m), vel: m/s (for streak length), sc: supercruise. */
  update(move: THREE.Vector3, vel: THREE.Vector3, sc: boolean) {
    const speed = vel.length()
    const visible = speed > 2
    this.lines.visible = visible
    if (!visible) return
    const half = BOX / 2
    // in supercruise the dust moves at a visual (not physical) speed
    const vis = sc ? move.clone().setLength(Math.min(160, 30 + Math.log10(speed / 3e4 + 1) * 60)) : move
    const streak = sc ? vis.clone().multiplyScalar(2.2) : vel.clone().multiplyScalar(0.04)
    if (streak.length() > 60) streak.setLength(60)
    const p = this.pts
    const o = this.pos
    for (let i = 0; i < N; i++) {
      const k = i * 3
      p[k] -= vis.x
      p[k + 1] -= vis.y
      p[k + 2] -= vis.z
      for (let a = 0; a < 3; a++) {
        if (p[k + a] > half) p[k + a] -= BOX
        else if (p[k + a] < -half) p[k + a] += BOX
      }
      const j = i * 6
      o[j] = p[k]
      o[j + 1] = p[k + 1]
      o[j + 2] = p[k + 2]
      o[j + 3] = p[k] + streak.x
      o[j + 4] = p[k + 1] + streak.y
      o[j + 5] = p[k + 2] + streak.z
    }
    ;(this.geo.attributes.position as THREE.BufferAttribute).needsUpdate = true
    this.mat.opacity = sc ? 0.35 : Math.min(0.55, 0.1 + speed / 600)
    this.mat.color.setRGB(sc ? 0.6 : 0.75, sc ? 0.75 : 0.82, 1)
  }
}
