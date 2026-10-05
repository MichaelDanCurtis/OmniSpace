// Background sky: real neighbouring stars from the galaxy catalogue (so the
// constellations shift as you travel), a dense faint field, and a procedural
// galactic band oriented to the true direction of the galactic core.

import * as THREE from 'three'
import { starsNear } from '../galaxy'
import { Rng, hashInts } from '../rng'
import type { StarDescriptor } from '../types'
import { NOISE } from './glsl'

const BAND_VERT = /* glsl */ `
varying vec3 vDir;
void main(){ vDir = normalize(position); gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); }`

const BAND_FRAG = /* glsl */ `
uniform vec3 uCore; uniform vec3 uNebA; uniform vec3 uNebB; uniform float uSeed;
varying vec3 vDir;
${NOISE}
void main(){
  vec3 d = normalize(vDir);
  float lat = d.y;
  float coreDot = dot(normalize(vec3(d.x, 0.0, d.z)), uCore);
  float width = 0.11 + 0.05 * (coreDot * 0.5 + 0.5);
  float band = exp(-lat * lat / (width * width));
  float n = fbm(d * 3.0 + uSeed, 6) * 0.5 + 0.5;
  float fine = fbm(d * 14.0 + uSeed, 4) * 0.5 + 0.5;
  float dust = smoothstep(0.45, 0.8, fbm(d * vec3(5.0, 18.0, 5.0) + uSeed * 2.0, 5) * 0.5 + 0.5) * exp(-lat * lat / (0.03 * 0.03 + width * width * 0.12));
  float core = pow(max(coreDot, 0.0), 6.0) * exp(-lat * lat / 0.05);
  vec3 armCol = vec3(0.55, 0.65, 0.95);
  vec3 coreCol = vec3(1.0, 0.82, 0.62);
  vec3 col = mix(armCol, coreCol, clamp(core * 2.0 + max(coreDot, 0.0) * 0.4, 0.0, 1.0));
  float b = band * (0.25 + 0.75 * n) * (0.65 + 0.35 * fine) * (0.5 + 0.9 * max(coreDot, 0.0) + 0.3) + core * 0.9;
  b *= 1.0 - dust * 0.85;
  vec3 c = col * b * 0.11;
  float neb = smoothstep(0.55, 0.95, fbm(d * 1.6 + uSeed * 3.1, 5) * 0.5 + 0.5);
  float neb2 = smoothstep(0.6, 1.0, fbm(d * 2.3 - uSeed, 5) * 0.5 + 0.5);
  c += uNebA * neb * 0.05 + uNebB * neb2 * 0.035;
  c += vec3(0.004, 0.006, 0.012);
  gl_FragColor = vec4(c, 1.0);
}`

const POINT_VERT = /* glsl */ `
attribute float aSize; attribute vec3 aColor;
varying vec3 vColor; uniform float uPixelRatio;
void main(){
  vColor = aColor;
  vec4 mv = modelViewMatrix * vec4(position, 1.0);
  gl_PointSize = aSize * uPixelRatio;
  gl_Position = projectionMatrix * mv;
}`
const POINT_FRAG = /* glsl */ `
varying vec3 vColor;
void main(){
  vec2 c = gl_PointCoord - 0.5;
  float r = length(c);
  float a = smoothstep(0.5, 0.0, r);
  a = a * a;
  gl_FragColor = vec4(vColor * a, 1.0);
}`

export class Skybox {
  readonly scene = new THREE.Scene()
  readonly camera: THREE.PerspectiveCamera
  private root = new THREE.Group()
  private catalog?: THREE.Points
  private bandMat: THREE.ShaderMaterial

  constructor(fov: number) {
    this.camera = new THREE.PerspectiveCamera(fov, 1, 0.1, 5000)
    this.scene.add(this.root)
    this.bandMat = new THREE.ShaderMaterial({
      vertexShader: BAND_VERT,
      fragmentShader: BAND_FRAG,
      side: THREE.BackSide,
      depthWrite: false,
      uniforms: {
        uCore: { value: new THREE.Vector3(1, 0, 0) },
        uNebA: { value: new THREE.Color('#5a3a9a') },
        uNebB: { value: new THREE.Color('#2a6a9a') },
        uSeed: { value: 0 },
      },
    })
    const band = new THREE.Mesh(new THREE.SphereGeometry(2000, 64, 48), this.bandMat)
    this.root.add(band)
    this.root.add(this.faintField())
  }

  private pointsMaterial() {
    return new THREE.ShaderMaterial({
      vertexShader: POINT_VERT,
      fragmentShader: POINT_FRAG,
      transparent: true,
      depthWrite: false,
      blending: THREE.AdditiveBlending,
      uniforms: { uPixelRatio: { value: Math.min(window.devicePixelRatio, 2) } },
    })
  }

  private faintField(): THREE.Points {
    const n = 26000
    const rng = new Rng(4242)
    const pos = new Float32Array(n * 3)
    const col = new Float32Array(n * 3)
    const size = new Float32Array(n)
    for (let i = 0; i < n; i++) {
      // concentrate toward the galactic plane
      const plane = rng.chance(0.6)
      const y = plane ? rng.gauss() * 0.12 : rng.range(-1, 1)
      const th = rng.range(0, Math.PI * 2)
      const r = Math.sqrt(Math.max(0, 1 - y * y))
      pos.set([Math.cos(th) * r * 1800, y * 1800, Math.sin(th) * r * 1800], i * 3)
      const t = rng.next()
      const c = t < 0.2 ? [0.75, 0.82, 1] : t < 0.8 ? [1, 0.97, 0.92] : [1, 0.8, 0.6]
      const b = Math.pow(rng.next(), 3) * 0.55 + 0.08
      col.set([c[0] * b, c[1] * b, c[2] * b], i * 3)
      size[i] = 1.2 + rng.next() * 1.6
    }
    const g = new THREE.BufferGeometry()
    g.setAttribute('position', new THREE.BufferAttribute(pos, 3))
    g.setAttribute('aColor', new THREE.BufferAttribute(col, 3))
    g.setAttribute('aSize', new THREE.BufferAttribute(size, 1))
    return new THREE.Points(g, this.pointsMaterial())
  }

  /** Rebuild for a new system. `tilt` maps galactic frame -> system frame. */
  setSystem(star: StarDescriptor, tilt: THREE.Quaternion) {
    this.root.quaternion.copy(tilt)
    const core = new THREE.Vector3(-star.pos[0], 0, -star.pos[2]).normalize()
    this.bandMat.uniforms.uCore.value.copy(core)
    const rng = new Rng(hashInts(Math.floor(star.pos[0] / 300), Math.floor(star.pos[2] / 300), 5))
    const nebs = ['#6a3aa8', '#2a7ab0', '#a83a6a', '#3aa88a', '#a8763a', '#4a4ac8']
    this.bandMat.uniforms.uNebA.value.set(rng.pick(nebs))
    this.bandMat.uniforms.uNebB.value.set(rng.pick(nebs))
    this.bandMat.uniforms.uSeed.value = rng.range(0, 50)
    if (this.catalog) {
      this.root.remove(this.catalog)
      this.catalog.geometry.dispose()
    }
    const near = starsNear(star.pos, 140).filter((s) => s.id !== star.id)
    const n = near.length
    const pos = new Float32Array(n * 3)
    const col = new Float32Array(n * 3)
    const size = new Float32Array(n)
    const c = new THREE.Color()
    near.forEach((s, i) => {
      const dx = s.pos[0] - star.pos[0]
      const dy = s.pos[1] - star.pos[1]
      const dz = s.pos[2] - star.pos[2]
      const d = Math.hypot(dx, dy, dz)
      pos.set([(dx / d) * 1500, (dy / d) * 1500, (dz / d) * 1500], i * 3)
      const flux = Math.max(s.luminosity, 0.002) / (d * d)
      const mag = Math.log10(flux * 1e4 + 1e-9)
      const b = THREE.MathUtils.clamp(0.35 + mag * 0.35, 0.12, 1.6)
      c.set(s.color)
      col.set([c.r * b, c.g * b, c.b * b], i * 3)
      size[i] = THREE.MathUtils.clamp(2 + mag * 2.2, 1.6, 9)
    })
    const g = new THREE.BufferGeometry()
    g.setAttribute('position', new THREE.BufferAttribute(pos, 3))
    g.setAttribute('aColor', new THREE.BufferAttribute(col, 3))
    g.setAttribute('aSize', new THREE.BufferAttribute(size, 1))
    this.catalog = new THREE.Points(g, this.pointsMaterial())
    this.root.add(this.catalog)
  }

  sync(main: THREE.PerspectiveCamera) {
    this.camera.quaternion.copy(main.quaternion)
    this.camera.fov = main.fov
    this.camera.aspect = main.aspect
    this.camera.updateProjectionMatrix()
  }
}
