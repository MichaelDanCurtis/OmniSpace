import * as THREE from 'three'
import type { Body, PlanetType } from '../types'
import { LOGDEPTH_FRAG, LOGDEPTH_FRAG_PARS, LOGDEPTH_VERT, LOGDEPTH_VERT_PARS, NOISE } from './glsl'

const TYPE_INDEX: Record<PlanetType, number> = {
  rocky: 0, barren: 1, desert: 2, lava: 3, ice: 4, ocean: 5, earthlike: 6, jungle: 7, toxic: 8, gas_giant: 9, ice_giant: 10,
}

let sphereGeo: THREE.SphereGeometry | null = null
function unitSphere() {
  if (!sphereGeo) sphereGeo = new THREE.SphereGeometry(1, 160, 110)
  return sphereGeo
}

const SURFACE_VERT = /* glsl */ `
${LOGDEPTH_VERT_PARS}
varying vec3 vObj;
varying vec3 vWorld;
varying vec3 vNormalW;
void main(){
  vObj = position;
  vec4 wp = modelMatrix * vec4(position,1.0);
  vWorld = wp.xyz;
  vNormalW = normalize(mat3(modelMatrix) * normal);
  gl_Position = projectionMatrix * viewMatrix * wp;
  ${LOGDEPTH_VERT}
}`

const SURFACE_FRAG = /* glsl */ `
${LOGDEPTH_FRAG_PARS}
uniform vec3 uC0; uniform vec3 uC1; uniform vec3 uC2;
uniform int uType; uniform vec3 uSun; uniform float uSeed; uniform float uBands;
uniform float uTime; uniform float uCity; uniform vec3 uAtmo; uniform float uHasAtmo;
uniform vec3 uSunColor;
varying vec3 vObj; varying vec3 vWorld; varying vec3 vNormalW;
${NOISE}
void main(){
  ${LOGDEPTH_FRAG}
  vec3 p = normalize(vObj);
  vec3 sp = p + vec3(uSeed);
  vec3 N = normalize(vNormalW);
  vec3 L = normalize(uSun - vWorld);
  vec3 V = normalize(-vWorld);
  float ndl = dot(N, L);
  vec3 col; vec3 emis = vec3(0.0); float spec = 0.0; float land = 0.0;
  if (uType == 9 || uType == 10 || uType == 8) {
    float giant = uType == 10 ? 0.55 : 1.0;
    float warp = fbm(sp * vec3(1.3, 4.0, 1.3), 4);
    float lat = p.y + warp * 0.035 * (0.6 + uBands);
    float bands = sin(lat * (8.0 + uBands * 10.0) * 3.14159 + uSeed * 3.0) * 0.5 + 0.5;
    float fine = fbm(vec3(lat * 60.0, sp.x * 0.45, sp.z * 0.45), 4) * 0.5 + 0.5;
    float m = mix(0.5, bands, uBands * giant) * 0.65 + fine * 0.35;
    col = mix(uC0, uC1, smoothstep(0.15, 0.6, m));
    col = mix(col, uC2, smoothstep(0.55, 0.95, m + fine * 0.15));
    // great storm
    vec3 sc = normalize(vec3(0.8, -0.32, 0.5));
    float sd = distance(p, sc);
    float storm = smoothstep(0.16, 0.0, sd + fbm(sp * 6.0, 3) * 0.04);
    col = mix(col, uC0 * 0.8 + uC2 * 0.25, storm * 0.7 * uBands * giant);
    if (uType == 8) col = mix(col, uC2, fbm(sp * 3.0 + uTime * 0.01, 4) * 0.4 + 0.2);
  } else if (uType == 5 || uType == 6 || uType == 7) {
    float h = fbm(sp * 1.7, 7);
    float detail = fbm(sp * 14.0, 4);
    float sea = uType == 5 ? 0.22 : (uType == 7 ? -0.12 : 0.04);
    if (h < sea) {
      col = mix(uC0 * 0.45, uC0, smoothstep(sea - 0.45, sea, h));
      spec = 1.0;
    } else {
      land = 1.0;
      float e = smoothstep(sea, sea + 0.45, h + detail * 0.12);
      col = mix(uC1, uC2, e);
      col = mix(col, uC1 * 0.7, smoothstep(0.2, 0.6, detail) * 0.4);
    }
    float ice = smoothstep(0.80, 0.86, abs(p.y) + fbm(sp * 5.0, 3) * 0.06);
    col = mix(col, vec3(0.92, 0.95, 1.0), ice);
    land *= 1.0 - ice;
  } else if (uType == 3) {
    float h = fbm(sp * 2.4, 6) * 0.5 + 0.5;
    col = mix(uC0, uC1, h);
    float cr = pow(ridged(sp * 3.2, 6), 9.0);
    emis = uC2 * cr * 3.0 + uC2 * pow(ridged(sp * 11.0, 4), 14.0) * 1.5;
  } else {
    float h = fbm(sp * 2.6, 7) * 0.5 + 0.5;
    float detail = fbm(sp * 22.0, 4);
    col = mix(uC0, uC1, smoothstep(0.3, 0.68, h));
    col = mix(col, uC2, smoothstep(0.6, 0.9, h + detail * 0.08));
    float cr = ridged(sp * 6.0, 4);
    if (uType == 1 || uType == 0) col *= 0.75 + 0.35 * smoothstep(0.7, 0.95, cr);
    if (uType == 2) {
      float dunes = sin((p.x + p.z) * 160.0 + fbm(sp * 8.0, 3) * 12.0) * 0.5 + 0.5;
      col = mix(col, uC2, dunes * 0.12);
    }
    if (uType == 4) {
      float cracks = pow(1.0 - abs(snoise(sp * 7.0)), 18.0);
      col = mix(col, uC0 * 0.7, cracks * 0.8);
    }
  }
  float diff = smoothstep(-0.12, 1.0, ndl);
  vec3 H = normalize(L + V);
  float s = pow(max(dot(N, H), 0.0), 60.0) * spec * step(0.0, ndl);
  vec3 lit = col * (diff * 1.15 + 0.012) * uSunColor + s * 0.6 * uSunColor;
  // scattering tint near the terminator
  lit += uAtmo * uHasAtmo * 0.18 * smoothstep(0.45, 0.0, abs(ndl)) * smoothstep(-0.3, 0.1, ndl);
  // night-side city lights
  if (uCity > 0.0 && land > 0.5) {
    float c = smoothstep(0.62, 0.9, fbm(sp * 38.0, 3) * 0.5 + 0.5) * smoothstep(0.1, -0.15, ndl);
    lit += vec3(1.0, 0.72, 0.38) * c * uCity * 1.4;
  }
  lit += emis;
  gl_FragColor = vec4(lit, 1.0);
}`

const CLOUD_FRAG = /* glsl */ `
${LOGDEPTH_FRAG_PARS}
uniform vec3 uSun; uniform float uSeed; uniform float uTime; uniform float uCover; uniform vec3 uSunColor; uniform vec3 uTint;
varying vec3 vObj; varying vec3 vWorld; varying vec3 vNormalW;
${NOISE}
void main(){
  ${LOGDEPTH_FRAG}
  vec3 p = normalize(vObj);
  vec3 N = normalize(vNormalW);
  vec3 L = normalize(uSun - vWorld);
  float ndl = dot(N, L);
  vec3 q = p * 2.2 + vec3(uSeed + 11.0);
  q += 0.25 * vec3(fbm(q + uTime * 0.004, 3), fbm(q + 5.2, 3), fbm(q + 9.1, 3));
  float n = fbm(q, 6) * 0.5 + 0.5;
  float a = smoothstep(1.0 - uCover, 1.0 - uCover + 0.28, n);
  float diff = smoothstep(-0.1, 1.0, ndl);
  gl_FragColor = vec4(uTint * (diff * 1.1 + 0.01) * uSunColor, a * 0.92);
}`

const ATMO_FRAG = /* glsl */ `
${LOGDEPTH_FRAG_PARS}
uniform vec3 uSun; uniform vec3 uAtmo; uniform float uStrength;
varying vec3 vObj; varying vec3 vWorld; varying vec3 vNormalW;
void main(){
  ${LOGDEPTH_FRAG}
  vec3 N = normalize(vNormalW);
  vec3 V = normalize(-vWorld);
  vec3 L = normalize(uSun - vWorld);
  float fres = pow(1.0 - clamp(dot(N, V), 0.0, 1.0), 2.6);
  float sun = smoothstep(-0.35, 0.5, dot(N, L));
  gl_FragColor = vec4(uAtmo * fres * sun * uStrength, 1.0);
}`

const RING_VERT = /* glsl */ `
${LOGDEPTH_VERT_PARS}
varying vec3 vWorld;
varying float vR;
void main(){
  vR = length(position.xy);
  vec4 wp = modelMatrix * vec4(position, 1.0);
  vWorld = wp.xyz;
  gl_Position = projectionMatrix * viewMatrix * wp;
  ${LOGDEPTH_VERT}
}`

const RING_FRAG = /* glsl */ `
${LOGDEPTH_FRAG_PARS}
uniform vec3 uColor; uniform float uOpacity; uniform float uInner; uniform float uOuter;
uniform vec3 uSun; uniform vec3 uPlanet; uniform float uPlanetR; uniform vec3 uNormal; uniform float uSeed; uniform vec3 uSunColor;
varying vec3 vWorld; varying float vR;
${NOISE}
void main(){
  ${LOGDEPTH_FRAG}
  float t = (vR - uInner) / (uOuter - uInner);
  if (t < 0.0 || t > 1.0) discard;
  float d = 0.55 + 0.25 * sin(t * 71.0 + uSeed) + 0.2 * sin(t * 23.0 + uSeed * 2.0);
  d += snoise(vec3(t * 160.0, uSeed, 0.0)) * 0.25 + snoise(vec3(t * 700.0, uSeed * 2.0, 1.0)) * 0.12;
  d *= smoothstep(0.0, 0.05, t) * smoothstep(1.0, 0.9, t);
  d *= 1.0 - 0.9 * smoothstep(0.022, 0.0, abs(t - 0.63));
  d *= 1.0 - 0.5 * smoothstep(0.012, 0.0, abs(t - 0.31));
  d = clamp(d, 0.0, 1.0);
  vec3 L = normalize(uSun - vWorld);
  vec3 oc = vWorld - uPlanet;
  float b = dot(oc, L);
  float c = dot(oc, oc) - uPlanetR * uPlanetR;
  float shadow = (b < 0.0 && b * b - c > 0.0) ? 0.08 : 1.0;
  float light = 0.35 + 0.65 * abs(dot(normalize(uNormal), L));
  vec3 V = normalize(-vWorld);
  float fwd = pow(max(dot(-V, L), 0.0), 8.0) * 0.6;
  vec3 col = uColor * (0.75 + 0.35 * snoise(vec3(t * 40.0, 3.0, uSeed)));
  gl_FragColor = vec4(col * (light + fwd) * shadow * uSunColor, d * uOpacity);
}`

export class PlanetVisual {
  readonly group = new THREE.Group()
  readonly spin = new THREE.Group()
  readonly body: Body
  private surfaceMat: THREE.ShaderMaterial
  private cloudMat?: THREE.ShaderMaterial
  private atmoMat?: THREE.ShaderMaterial
  private ringMat?: THREE.ShaderMaterial
  private ringNormal = new THREE.Vector3()
  readonly radius: number

  constructor(body: Body, seed: number, cityLights: number) {
    this.body = body
    this.radius = body.radiusM
    const c = body.palette.map((h) => new THREE.Color(h))
    while (c.length < 3) c.push(new THREE.Color('#888888'))
    const atmo = new THREE.Color(body.atmosphere ?? '#000000')
    const tilt = new THREE.Group()
    tilt.rotation.z = body.tilt ?? 0
    this.group.add(tilt)
    tilt.add(this.spin)
    this.surfaceMat = new THREE.ShaderMaterial({
      vertexShader: SURFACE_VERT,
      fragmentShader: SURFACE_FRAG,
      uniforms: {
        uC0: { value: c[0] }, uC1: { value: c[1] }, uC2: { value: c[2] },
        uType: { value: TYPE_INDEX[body.type] ?? 0 },
        uSun: { value: new THREE.Vector3() },
        uSeed: { value: (seed % 1000) / 37.0 },
        uBands: { value: body.bands ?? 0.6 },
        uTime: { value: 0 },
        uCity: { value: body.type === 'earthlike' || body.type === 'jungle' || body.type === 'ocean' ? cityLights : 0 },
        uAtmo: { value: atmo },
        uHasAtmo: { value: body.atmosphere ? 1 : 0 },
        uSunColor: { value: new THREE.Color(1, 1, 1) },
      },
    })
    const surf = new THREE.Mesh(unitSphere(), this.surfaceMat)
    surf.scale.setScalar(body.radiusM)
    this.spin.add(surf)

    if ((body.clouds ?? 0) > 0.02) {
      this.cloudMat = new THREE.ShaderMaterial({
        vertexShader: SURFACE_VERT,
        fragmentShader: CLOUD_FRAG,
        transparent: true,
        depthWrite: false,
        uniforms: {
          uSun: { value: new THREE.Vector3() }, uSeed: { value: (seed % 777) / 13 }, uTime: { value: 0 },
          uCover: { value: body.clouds ?? 0.5 }, uSunColor: { value: new THREE.Color(1, 1, 1) },
          uTint: { value: body.type === 'toxic' ? new THREE.Color(c[2]) : new THREE.Color(1, 1, 1) },
        },
      })
      const clouds = new THREE.Mesh(unitSphere(), this.cloudMat)
      clouds.scale.setScalar(body.radiusM * 1.008)
      this.spin.add(clouds)
    }
    if (body.atmosphere) {
      this.atmoMat = new THREE.ShaderMaterial({
        vertexShader: SURFACE_VERT,
        fragmentShader: ATMO_FRAG,
        transparent: true,
        depthWrite: false,
        blending: THREE.AdditiveBlending,
        uniforms: {
          uSun: { value: new THREE.Vector3() }, uAtmo: { value: atmo },
          uStrength: { value: body.type === 'gas_giant' || body.type === 'ice_giant' ? 1.1 : 1.6 },
        },
      })
      const a = new THREE.Mesh(unitSphere(), this.atmoMat)
      a.scale.setScalar(body.radiusM * 1.02)
      tilt.add(a)
    }
    if (body.rings) {
      const r = body.rings
      const geo = new THREE.RingGeometry(r.innerM, r.outerM, 256, 1)
      this.ringMat = new THREE.ShaderMaterial({
        vertexShader: RING_VERT,
        fragmentShader: RING_FRAG,
        transparent: true,
        depthWrite: false,
        side: THREE.DoubleSide,
        uniforms: {
          uColor: { value: new THREE.Color(r.color) }, uOpacity: { value: r.opacity },
          uInner: { value: r.innerM }, uOuter: { value: r.outerM },
          uSun: { value: new THREE.Vector3() }, uPlanet: { value: new THREE.Vector3() }, uPlanetR: { value: body.radiusM },
          uNormal: { value: new THREE.Vector3(0, 1, 0) }, uSeed: { value: (seed % 97) / 7 }, uSunColor: { value: new THREE.Color(1, 1, 1) },
        },
      })
      const ring = new THREE.Mesh(geo, this.ringMat)
      ring.rotation.x = -Math.PI / 2
      ring.renderOrder = 2
      tilt.add(ring)
    }
  }

  setSunColor(c: THREE.Color) {
    this.surfaceMat.uniforms.uSunColor.value.copy(c)
    this.cloudMat?.uniforms.uSunColor.value.copy(c)
    this.ringMat?.uniforms.uSunColor.value.copy(c)
  }

  /** sunRel: star position relative to camera (world space). */
  update(sunRel: THREE.Vector3, time: number) {
    this.spin.rotation.y = time * 0.004
    this.surfaceMat.uniforms.uSun.value.copy(sunRel)
    this.surfaceMat.uniforms.uTime.value = time
    if (this.cloudMat) {
      this.cloudMat.uniforms.uSun.value.copy(sunRel)
      this.cloudMat.uniforms.uTime.value = time
    }
    if (this.atmoMat) this.atmoMat.uniforms.uSun.value.copy(sunRel)
    if (this.ringMat) {
      this.ringMat.uniforms.uSun.value.copy(sunRel)
      this.ringMat.uniforms.uPlanet.value.copy(this.group.position)
      this.ringNormal.set(0, 1, 0).applyAxisAngle(new THREE.Vector3(0, 0, 1), this.body.tilt ?? 0)
      this.ringMat.uniforms.uNormal.value.copy(this.ringNormal)
    }
  }

  dispose() {
    this.group.traverse((o) => {
      const m = o as THREE.Mesh
      if (m.geometry && m.geometry !== sphereGeo) m.geometry.dispose()
      if (m.material) (m.material as THREE.Material).dispose()
    })
  }
}
