import * as THREE from 'three'
import { LOGDEPTH_FRAG, LOGDEPTH_FRAG_PARS, LOGDEPTH_VERT, LOGDEPTH_VERT_PARS, NOISE } from './glsl'
import { glowTexture } from './star'

const VERT = /* glsl */ `
${LOGDEPTH_VERT_PARS}
varying vec2 vUv;
void main(){ vUv = uv; gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.0); ${LOGDEPTH_VERT} }`
const FRAG = /* glsl */ `
${LOGDEPTH_FRAG_PARS}
uniform float uTime; uniform vec3 uColA; uniform vec3 uColB; uniform float uIntensity; uniform float uExit;
varying vec2 vUv;
${NOISE}
void main(){
  ${LOGDEPTH_FRAG}
  float a = vUv.x * 6.28318;
  float z = vUv.y;
  vec3 q = vec3(cos(a) * 2.0, sin(a) * 2.0, z * 6.0 - uTime * 2.2);
  float swirl = fbm(q + vec3(0.0, 0.0, uTime * 0.2), 5) * 0.5 + 0.5;
  float streak = pow(snoise(vec3(vUv.x * 90.0, z * 3.0 - uTime * 6.0, 0.0)) * 0.5 + 0.5, 7.0);
  float depth = smoothstep(0.0, 0.25, z) * smoothstep(1.0, 0.55, z);
  vec3 col = mix(uColA, uColB, swirl) * swirl * 1.3 + vec3(0.8, 0.9, 1.0) * streak * 2.5;
  col *= depth * uIntensity;
  col += uColB * uExit * 3.0 * smoothstep(0.4, 0.0, z);
  gl_FragColor = vec4(col, 1.0);
}`

export class Tunnel {
  readonly group = new THREE.Group()
  private mat: THREE.ShaderMaterial
  private flare: THREE.Sprite

  constructor() {
    this.mat = new THREE.ShaderMaterial({
      vertexShader: VERT,
      fragmentShader: FRAG,
      side: THREE.BackSide,
      depthWrite: false,
      uniforms: {
        uTime: { value: 0 },
        uColA: { value: new THREE.Color('#1a3a8a') },
        uColB: { value: new THREE.Color('#8ac8ff') },
        uIntensity: { value: 1 },
        uExit: { value: 0 },
      },
    })
    const geo = new THREE.CylinderGeometry(60, 60, 4000, 64, 32, true)
    const m = new THREE.Mesh(geo, this.mat)
    m.rotation.x = Math.PI / 2
    m.position.z = -1900
    this.group.add(m)
    this.flare = new THREE.Sprite(new THREE.SpriteMaterial({ map: glowTexture(), color: new THREE.Color(3, 3, 3), blending: THREE.AdditiveBlending, depthWrite: false, transparent: true }))
    this.flare.position.z = -3600
    this.flare.scale.setScalar(400)
    this.group.add(this.flare)
    this.group.visible = false
  }

  setColors(dest: string) {
    const c = new THREE.Color(dest)
    this.mat.uniforms.uColB.value.copy(c)
    this.mat.uniforms.uColA.value.copy(c.clone().multiplyScalar(0.25).lerp(new THREE.Color('#220a44'), 0.5))
    ;(this.flare.material as THREE.SpriteMaterial).color.copy(c).multiplyScalar(3)
  }

  update(time: number, intensity: number, exit: number) {
    this.mat.uniforms.uTime.value = time
    this.mat.uniforms.uIntensity.value = intensity
    this.mat.uniforms.uExit.value = exit
    this.flare.scale.setScalar(300 + exit * 6000)
    this.group.rotation.z = time * 0.3
  }
}
