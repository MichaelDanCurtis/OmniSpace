// The cockpit frame: canopy pillars, dashboard and screen bezels, modelled in
// camera space. HTML instruments are laid over the bezels using the projected
// rectangles exposed by `screenRects()`.

import * as THREE from 'three'
import { panelTexture } from './textures'

export interface ScreenAnchor {
  id: 'left' | 'center' | 'right' | 'switches'
  corners: THREE.Vector3[] // camera-space
}

function quad(a: THREE.Vector3, b: THREE.Vector3, c: THREE.Vector3, d: THREE.Vector3): THREE.BufferGeometry {
  const g = new THREE.BufferGeometry()
  const pos = new Float32Array([a.x, a.y, a.z, b.x, b.y, b.z, c.x, c.y, c.z, a.x, a.y, a.z, c.x, c.y, c.z, d.x, d.y, d.z])
  g.setAttribute('position', new THREE.BufferAttribute(pos, 3))
  g.setAttribute('uv', new THREE.BufferAttribute(new Float32Array([0, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1]), 2))
  g.computeVertexNormals()
  return g
}

/** A beam (box) between two points. */
function beam(a: THREE.Vector3, b: THREE.Vector3, w: number, d: number, mat: THREE.Material, twist = 0): THREE.Mesh {
  const len = a.distanceTo(b)
  const m = new THREE.Mesh(new THREE.BoxGeometry(w, len, d), mat)
  m.position.copy(a).add(b).multiplyScalar(0.5)
  m.quaternion.setFromUnitVectors(new THREE.Vector3(0, 1, 0), b.clone().sub(a).normalize())
  m.rotateY(twist)
  return m
}

export class Cockpit {
  readonly group = new THREE.Group()
  readonly anchors: ScreenAnchor[] = []
  private strips: THREE.MeshBasicMaterial
  private interior: THREE.PointLight

  constructor() {
    const panel = panelTexture()
    const frame = new THREE.MeshStandardMaterial({ color: '#2a2f37', map: panel, metalness: 0.65, roughness: 0.42 })
    const pillarMat = new THREE.MeshStandardMaterial({ color: '#1c2026', map: panel, metalness: 0.4, roughness: 0.75 })
    const dark = new THREE.MeshStandardMaterial({ color: '#0d1014', metalness: 0.5, roughness: 0.6 })
    const bezel = new THREE.MeshStandardMaterial({ color: '#1a1e24', metalness: 0.7, roughness: 0.35 })
    const screen = new THREE.MeshBasicMaterial({ color: '#03070c', transparent: true, opacity: 0.88 })
    this.strips = new THREE.MeshBasicMaterial({ color: new THREE.Color('#ffa040').multiplyScalar(1.25) })
    const cyan = new THREE.MeshBasicMaterial({ color: new THREE.Color('#3aa8ff').multiplyScalar(1.6) })

    // --- dashboard: a slanted console facing the pilot
    const zTop = -1.05
    const yTop = -0.205
    const zBot = -0.86
    const yBot = -0.53
    const W = 1.75
    const dash = quad(new THREE.Vector3(-W, yBot, zBot), new THREE.Vector3(W, yBot, zBot), new THREE.Vector3(W, yTop, zTop), new THREE.Vector3(-W, yTop, zTop))
    this.group.add(new THREE.Mesh(dash, frame))
    // top hood lip
    const hood = new THREE.Mesh(new THREE.BoxGeometry(1.0, 0.045, 0.22), dark)
    hood.position.set(0, yTop + 0.012, zTop + 0.04)
    this.group.add(hood)
    const hoodStrip = new THREE.Mesh(new THREE.BoxGeometry(0.1, 0.004, 0.006), this.strips)
    hoodStrip.position.set(0, yTop + 0.018, zTop + 0.152)
    this.group.add(hoodStrip)
    // lower body under the console
    this.group.add(new THREE.Mesh(quad(new THREE.Vector3(-W, -1.4, -0.7), new THREE.Vector3(W, -1.4, -0.7), new THREE.Vector3(W, yBot, zBot), new THREE.Vector3(-W, yBot, zBot)), dark))

    // screens laid on the console surface
    const along = (u: number, v: number, x: number) => {
      // v: 0 = bottom edge, 1 = top edge of the console face
      const y = yBot + (yTop - yBot) * v
      const z = zBot + (zTop - zBot) * v + 0.004
      return new THREE.Vector3(x * u, y, z)
    }
    const addScreen = (id: ScreenAnchor['id'], x0: number, x1: number, v0: number, v1: number) => {
      const pad = 0.018
      const b = quad(along(1, v0 - 0.03, x0 - pad), along(1, v0 - 0.03, x1 + pad), along(1, v1 + 0.03, x1 + pad), along(1, v1 + 0.03, x0 - pad))
      const bm = new THREE.Mesh(b, bezel)
      bm.position.z += 0.002
      this.group.add(bm)
      const corners = [along(1, v0, x0), along(1, v0, x1), along(1, v1, x1), along(1, v1, x0)]
      corners.forEach((c) => (c.z += 0.008))
      this.group.add(new THREE.Mesh(quad(corners[0], corners[1], corners[2], corners[3]), screen))
      this.anchors.push({ id, corners })
      // edge glow line along the top of each screen
      const top = new THREE.Mesh(new THREE.BoxGeometry(Math.abs(x1 - x0) * 0.3, 0.004, 0.004), id === 'center' ? this.strips : cyan)
      top.position.copy(along(1, v1 + 0.045, (x0 + x1) / 2))
      this.group.add(top)
    }
    addScreen('left', -0.66, -0.2, 0.16, 0.86)
    addScreen('center', -0.17, 0.17, 0.12, 0.9)
    addScreen('right', 0.2, 0.66, 0.16, 0.86)
    addScreen('switches', -0.93, -0.7, 0.14, 0.7)

    // --- canopy pillars (diagonal, like the concept art)
    for (const side of [-1, 1]) {
      const bot = new THREE.Vector3(side * 0.9, -0.2, -1.1)
      const top = new THREE.Vector3(side * 1.25, 0.62, -0.86)
      const p = beam(bot, top, 0.085, 0.07, pillarMat)
      this.group.add(p)
            const sideConsole = quad(
        new THREE.Vector3(side * 1.0, -0.21, -1.1),
        new THREE.Vector3(side * 2.4, -0.21, -0.7),
        new THREE.Vector3(side * 2.4, -0.9, -0.6),
        new THREE.Vector3(side * W, yBot, zBot),
      )
      const sc = new THREE.Mesh(sideConsole, dark.clone())
      ;(sc.material as THREE.MeshStandardMaterial).side = THREE.DoubleSide
      this.group.add(sc)
      // short orange lamp on the pillar
      const lamp = beam(
        bot.clone().lerp(top, 0.12).add(new THREE.Vector3(-side * 0.048, 0, 0.03)),
        bot.clone().lerp(top, 0.2).add(new THREE.Vector3(-side * 0.048, 0, 0.03)),
        0.01,
        0.01,
        this.strips,
      )
      this.group.add(lamp)
      // handles / throttle boxes
      const grip = new THREE.Mesh(new THREE.BoxGeometry(0.18, 0.08, 0.2), frame)
      grip.position.set(side * 1.25, -0.62, -0.7)
      grip.rotation.x = -0.4
      this.group.add(grip)
      const gs = new THREE.Mesh(new THREE.BoxGeometry(0.1, 0.006, 0.006), this.strips)
      gs.position.set(side * 1.25, -0.575, -0.62)
      this.group.add(gs)
    }
    // top canopy frame (only edges visible at very top)
    const topBar = beam(new THREE.Vector3(-1.55, 0.62, -1.02), new THREE.Vector3(1.55, 0.62, -1.02), 0.04, 0.04, frame)
    topBar.position.y += 0.6
    this.group.add(topBar)

    this.interior = new THREE.PointLight('#ffd0a0', 0.6, 3, 2)
    this.interior.position.set(0, 0.3, -0.2)
    this.group.add(this.interior)
    const fill = new THREE.PointLight('#5aa0ff', 0.35, 3, 2)
    fill.position.set(0, -0.3, -0.5)
    this.group.add(fill)
    this.group.traverse((o) => {
      o.renderOrder = 10
    })
  }

  setAlert(on: boolean, t: number) {
    const k = on ? 1.2 + Math.sin(t * 10) * 0.8 : 1.25
    this.strips.color.set(on ? '#ff3020' : '#ffa040').multiplyScalar(k)
  }

  /** Project each screen anchor into CSS pixel rectangles. */
  screenRects(camera: THREE.PerspectiveCamera, w: number, h: number) {
    const out: Record<string, { x: number; y: number; w: number; h: number }> = {}
    const v = new THREE.Vector3()
    for (const a of this.anchors) {
      let x0 = Infinity
      let y0 = Infinity
      let x1 = -Infinity
      let y1 = -Infinity
      for (const c of a.corners) {
        v.copy(c).applyMatrix4(camera.projectionMatrix)
        const sx = (v.x * 0.5 + 0.5) * w
        const sy = (-v.y * 0.5 + 0.5) * h
        x0 = Math.min(x0, sx)
        x1 = Math.max(x1, sx)
        y0 = Math.min(y0, sy)
        y1 = Math.max(y1, sy)
      }
      out[a.id] = { x: x0, y: y0, w: x1 - x0, h: y1 - y0 }
    }
    return out
  }
}
