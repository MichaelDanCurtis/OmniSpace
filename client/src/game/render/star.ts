import * as THREE from 'three'
import type { StarDescriptor } from '../types'
import { SOLAR_RADIUS } from '../types'
import { LOGDEPTH_FRAG, LOGDEPTH_FRAG_PARS, LOGDEPTH_VERT, LOGDEPTH_VERT_PARS, NOISE } from './glsl'

let glowTex: THREE.Texture | null = null
export function glowTexture(): THREE.Texture {
  if (glowTex) return glowTex
  const s = 256
  const cv = document.createElement('canvas')
  cv.width = cv.height = s
  const g = cv.getContext('2d')!
  const grd = g.createRadialGradient(s / 2, s / 2, 0, s / 2, s / 2, s / 2)
  grd.addColorStop(0, 'rgba(255,255,255,1)')
  grd.addColorStop(0.08, 'rgba(255,255,255,0.85)')
  grd.addColorStop(0.2, 'rgba(255,255,255,0.32)')
  grd.addColorStop(0.45, 'rgba(255,255,255,0.08)')
  grd.addColorStop(1, 'rgba(255,255,255,0)')
  g.fillStyle = grd
  g.fillRect(0, 0, s, s)
  glowTex = new THREE.CanvasTexture(cv)
  glowTex.colorSpace = THREE.SRGBColorSpace
  return glowTex
}

const STAR_VERT = /* glsl */ `
${LOGDEPTH_VERT_PARS}
varying vec3 vObj; varying vec3 vNormalW; varying vec3 vWorld;
void main(){
  vObj = position;
  vec4 wp = modelMatrix * vec4(position,1.0);
  vWorld = wp.xyz;
  vNormalW = normalize(mat3(modelMatrix)*normal);
  gl_Position = projectionMatrix * viewMatrix * wp;
  ${LOGDEPTH_VERT}
}`
const STAR_FRAG = /* glsl */ `
${LOGDEPTH_FRAG_PARS}
uniform vec3 uColor; uniform float uTime;
varying vec3 vObj; varying vec3 vNormalW; varying vec3 vWorld;
${NOISE}
void main(){
  ${LOGDEPTH_FRAG}
  vec3 p = normalize(vObj);
  float g = fbm(p * 9.0 + vec3(uTime * 0.03), 5) * 0.5 + 0.5;
  float spots = smoothstep(0.62, 0.8, fbm(p * 2.5 + vec3(uTime * 0.005), 4) * 0.5 + 0.5);
  vec3 V = normalize(-vWorld);
  float mu = clamp(dot(normalize(vNormalW), V), 0.0, 1.0);
  float limb = 0.35 + 0.65 * pow(mu, 0.45);
  vec3 col = uColor * (0.75 + 0.5 * g) * limb * (1.0 - spots * 0.35);
  gl_FragColor = vec4(col * 3.2, 1.0);
}`

export class StarVisual {
  readonly group = new THREE.Group()
  readonly light: THREE.PointLight
  readonly radius: number
  readonly color: THREE.Color
  private mat: THREE.ShaderMaterial
  private glow: THREE.Sprite
  private halo: THREE.Sprite
  readonly star: StarDescriptor

  constructor(star: StarDescriptor) {
    this.star = star
    this.radius = star.radiusSolar * SOLAR_RADIUS
    this.color = new THREE.Color(star.color)
    this.mat = new THREE.ShaderMaterial({
      vertexShader: STAR_VERT,
      fragmentShader: STAR_FRAG,
      uniforms: { uColor: { value: this.color.clone() }, uTime: { value: 0 } },
    })
    const mesh = new THREE.Mesh(new THREE.SphereGeometry(1, 96, 64), this.mat)
    mesh.scale.setScalar(Math.max(this.radius, 8000))
    this.group.add(mesh)
    const gm = new THREE.SpriteMaterial({ map: glowTexture(), color: this.color.clone().multiplyScalar(2.2), blending: THREE.AdditiveBlending, depthWrite: false, transparent: true })
    this.glow = new THREE.Sprite(gm)
    this.group.add(this.glow)
    const hm = new THREE.SpriteMaterial({ map: glowTexture(), color: this.color.clone().multiplyScalar(0.5), blending: THREE.AdditiveBlending, depthWrite: false, transparent: true })
    this.halo = new THREE.Sprite(hm)
    this.group.add(this.halo)
    if (star.cls === 'NS') {
      const jetMat = new THREE.MeshBasicMaterial({ color: new THREE.Color('#9fc4ff').multiplyScalar(2.5), transparent: true, opacity: 0.55, blending: THREE.AdditiveBlending, depthWrite: false })
      const jet = new THREE.Mesh(new THREE.ConeGeometry(4e7, 1.2e9, 24, 1, true), jetMat)
      jet.position.y = 0.6e9
      const jet2 = jet.clone()
      jet2.rotation.z = Math.PI
      jet2.position.y = -0.6e9
      const tilt = new THREE.Group()
      tilt.rotation.z = 0.4
      tilt.add(jet, jet2)
      this.group.add(tilt)
    }
    const lc = this.color.clone().lerp(new THREE.Color(1, 1, 1), 0.55)
    this.light = new THREE.PointLight(lc, 3.2, 0, 0)
  }

  /** Light colour used by custom planet shaders. */
  sunColor(): THREE.Color {
    return this.color.clone().lerp(new THREE.Color(1, 1, 1), 0.6)
  }

  update(distance: number, time: number) {
    this.mat.uniforms.uTime.value = time
    const r = Math.max(this.radius, 8000)
    const s = Math.max(r * 7, distance * 0.05)
    this.glow.scale.setScalar(s)
    this.halo.scale.setScalar(Math.max(r * 30, distance * 0.22))
  }
}
