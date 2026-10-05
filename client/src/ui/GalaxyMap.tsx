import { useEffect, useMemo, useRef, useState } from 'react'
import * as THREE from 'three'
import { OrbitControls } from 'three/examples/jsm/controls/OrbitControls.js'
import { chartedSystems, cachedSystem, type ChartedSummary } from '../game/api'
import type { Game } from '../game/engine'
import { CLASS_LABEL, density, distLy, getStar, HOME_POS, HOME_STAR, SCOOPABLE, starsNear } from '../game/galaxy'
import { useHud } from '../game/hud'
import { jumpFuel, jumpRange } from '../game/player'
import { Rng } from '../game/rng'
import type { StarDescriptor } from '../game/types'

const SIZE: Record<string, number> = { O: 9, B: 7.5, A: 6, F: 5.2, G: 5, K: 4.4, M: 3.6, L: 3, WD: 3.4, NS: 3.4 }
const LOCAL_RADIUS = 80

const PT_VERT = /* glsl */ `
attribute float aSize; attribute vec3 aColor; attribute float aFlag;
varying vec3 vColor; varying float vFlag;
uniform float uScale;
void main(){
  vColor = aColor; vFlag = aFlag;
  vec4 mv = modelViewMatrix * vec4(position,1.0);
  gl_PointSize = aSize * uScale * clamp(60.0 / -mv.z, 0.55, 2.2);
  gl_Position = projectionMatrix * mv;
}`
const PT_FRAG = /* glsl */ `
varying vec3 vColor; varying float vFlag;
void main(){
  vec2 c = gl_PointCoord - 0.5; float r = length(c);
  float core = smoothstep(0.5, 0.0, r);
  float ring = vFlag > 0.5 ? smoothstep(0.06, 0.0, abs(r - 0.42)) : 0.0;
  vec3 ringCol = vFlag > 1.5 ? vec3(0.42, 1.0, 0.7) : vec3(0.78, 0.56, 1.0);
  vec3 col = vColor * core * core * 1.6 + ringCol * ring;
  float a = max(core * core, ring);
  if (a < 0.02) discard;
  gl_FragColor = vec4(col, a);
}`

let galaxyCloud: Float32Array | null = null
function sampleGalaxy(): Float32Array {
  if (galaxyCloud) return galaxyCloud
  const n = 45000
  const out = new Float32Array(n * 3)
  const rng = new Rng(777)
  const max = density(0, 0, 0)
  let i = 0
  let guard = 0
  while (i < n && guard++ < 3e6) {
    const r = Math.sqrt(rng.next()) * 52000
    const th = rng.next() * Math.PI * 2
    const x = Math.cos(th) * r
    const z = Math.sin(th) * r
    const y = rng.gauss() * (300 + r * 0.01) * 2
    const d = density(x, y, z)
    if (rng.next() * max * 0.35 > d) continue
    out.set([x - HOME_POS[0], y - HOME_POS[1], z - HOME_POS[2]], i * 3)
    i++
  }
  galaxyCloud = out.subarray(0, i * 3)
  return galaxyCloud
}

export function GalaxyMap({ game }: { game: Game }) {
  const wrap = useRef<HTMLDivElement>(null)
  const labelsRef = useRef<HTMLDivElement>(null)
  const cur = getStar(game.player.systemId) ?? HOME_STAR
  const [selected, setSelected] = useState<StarDescriptor | null>(() => (game.player.route.length ? getStar(game.player.route[game.player.route.length - 1]) ?? null : null))
  const [focus, setFocus] = useState<StarDescriptor>(cur)
  const [charted, setCharted] = useState<Map<string, ChartedSummary>>(new Map())
  const [query, setQuery] = useState('')
  const [overview, setOverview] = useState(false)
  const route = useHud((s) => s.route)
  const range = jumpRange(game.player)
  const api = useRef<{ select: (s: StarDescriptor | null) => void; frame: (s: StarDescriptor) => void; setOverview: (o: boolean) => void } | null>(null)
  const stars = useMemo(() => starsNear(focus.pos, LOCAL_RADIUS), [focus])

  useEffect(() => {
    void chartedSystems().then((list) => setCharted(new Map(list.map((c) => [c.id, c]))))
  }, [])

  useEffect(() => {
    const el = wrap.current!
    const renderer = new THREE.WebGLRenderer({ antialias: true })
    renderer.setPixelRatio(Math.min(window.devicePixelRatio, 1.5))
    el.prepend(renderer.domElement)
    const scene = new THREE.Scene()
    scene.background = new THREE.Color('#01040a')
    const camera = new THREE.PerspectiveCamera(55, 1, 0.1, 400000)
    const origin = new THREE.Vector3(...cur.pos)
    const rel = (s: StarDescriptor) => new THREE.Vector3(s.pos[0] - origin.x, s.pos[1] - origin.y, s.pos[2] - origin.z)
    const focusRel = rel(focus)
    camera.position.copy(focusRel).add(new THREE.Vector3(0, 38, 62))
    const controls = new OrbitControls(camera, renderer.domElement)
    controls.target.copy(selected ? rel(selected) : focusRel)
    controls.enableDamping = true
    controls.maxDistance = 400
    controls.minDistance = 4

    // local stars
    const visited = new Set(game.player.visited)
    const n = stars.length
    const pos = new Float32Array(n * 3)
    const col = new Float32Array(n * 3)
    const size = new Float32Array(n)
    const flag = new Float32Array(n)
    const c = new THREE.Color()
    stars.forEach((s, i) => {
      const r = rel(s)
      pos.set([r.x, r.y, r.z], i * 3)
      c.set(s.color)
      col.set([c.r, c.g, c.b], i * 3)
      size[i] = SIZE[s.cls] ?? 4
      flag[i] = visited.has(s.id) ? 2 : charted.has(s.id) ? 1 : 0
    })
    const geo = new THREE.BufferGeometry()
    geo.setAttribute('position', new THREE.BufferAttribute(pos, 3))
    geo.setAttribute('aColor', new THREE.BufferAttribute(col, 3))
    geo.setAttribute('aSize', new THREE.BufferAttribute(size, 1))
    geo.setAttribute('aFlag', new THREE.BufferAttribute(flag, 1))
    const mat = new THREE.ShaderMaterial({ vertexShader: PT_VERT, fragmentShader: PT_FRAG, transparent: true, depthWrite: false, uniforms: { uScale: { value: renderer.getPixelRatio() * 1.3 } } })
    const points = new THREE.Points(geo, mat)
    scene.add(points)

    // grid + jump range
    const local = new THREE.Group()
    scene.add(local)
    local.add(points)
    const grid = new THREE.PolarGridHelper(LOCAL_RADIUS, 12, 8, 96, 0x1d3550, 0x12233a)
    grid.position.copy(focusRel).setY(rel(cur).y)
    local.add(grid)
    const rangeSphere = new THREE.Mesh(new THREE.SphereGeometry(range, 32, 20), new THREE.MeshBasicMaterial({ color: 0x3a8ad0, wireframe: true, transparent: true, opacity: 0.035 }))
    local.add(rangeSphere)
    const rangeRing = new THREE.Mesh(new THREE.RingGeometry(range - 0.15, range + 0.15, 96), new THREE.MeshBasicMaterial({ color: 0x5ab0ff, transparent: true, opacity: 0.5, side: THREE.DoubleSide }))
    rangeRing.rotation.x = -Math.PI / 2
    local.add(rangeRing)
    // stalks from grid plane to each nearby star for depth perception
    const stalkPos: number[] = []
    for (const s of stars) {
      const r = rel(s)
      if (r.distanceTo(focusRel) > 30) continue
      stalkPos.push(r.x, r.y, r.z, r.x, grid.position.y, r.z)
    }
    const stalks = new THREE.LineSegments(new THREE.BufferGeometry().setAttribute('position', new THREE.Float32BufferAttribute(stalkPos, 3)), new THREE.LineBasicMaterial({ color: 0x24486a, transparent: true, opacity: 0.5 }))
    local.add(stalks)

    // selection & current markers
    const ringTex = (() => {
      const cv = document.createElement('canvas')
      cv.width = cv.height = 64
      const g = cv.getContext('2d')!
      g.strokeStyle = '#fff'
      g.lineWidth = 4
      g.beginPath()
      g.arc(32, 32, 26, 0, Math.PI * 2)
      g.stroke()
      return new THREE.CanvasTexture(cv)
    })()
    const selSprite = new THREE.Sprite(new THREE.SpriteMaterial({ map: ringTex, color: 0xffb347, depthTest: false, transparent: true }))
    selSprite.scale.setScalar(2.4)
    selSprite.visible = false
    local.add(selSprite)
    const curSprite = new THREE.Sprite(new THREE.SpriteMaterial({ map: ringTex, color: 0x6cffb0, depthTest: false, transparent: true }))
    curSprite.scale.setScalar(2.2)
    curSprite.position.copy(rel(cur))
    local.add(curSprite)

    // route line
    const routeLine = new THREE.Line(new THREE.BufferGeometry(), new THREE.LineBasicMaterial({ color: 0xc890ff, transparent: true, opacity: 0.9 }))
    local.add(routeLine)
    const drawRoute = () => {
      const ids = [cur.id, ...game.player.route]
      const pts = ids.map((id) => getStar(id)).filter(Boolean).map((s) => rel(s!))
      routeLine.geometry.dispose()
      routeLine.geometry = new THREE.BufferGeometry().setFromPoints(pts)
    }
    drawRoute()
    const unsub = useHud.subscribe((s, prev) => {
      if (s.route !== prev.route) drawRoute()
    })

    // galaxy overview cloud
    const cloud = sampleGalaxy()
    const cg = new THREE.BufferGeometry()
    cg.setAttribute('position', new THREE.BufferAttribute(cloud, 3))
    const cloudPts = new THREE.Points(cg, new THREE.PointsMaterial({ size: 1.6, sizeAttenuation: false, color: 0x9ab8e8, transparent: true, opacity: 0.55, depthWrite: false, blending: THREE.AdditiveBlending }))
    cloudPts.visible = false
    scene.add(cloudPts)
    const youAreHere = new THREE.Sprite(new THREE.SpriteMaterial({ map: ringTex, color: 0x6cffb0, depthTest: false, sizeAttenuation: false, transparent: true }))
    youAreHere.scale.setScalar(0.04)
    youAreHere.visible = false
    scene.add(youAreHere)

    let sel: StarDescriptor | null = selected
    const select = (s: StarDescriptor | null) => {
      sel = s
      selSprite.visible = !!s
      if (s) selSprite.position.copy(rel(s))
      setSelected(s)
    }
    select(sel)
    const frame = (s: StarDescriptor) => {
      controls.target.copy(rel(s))
    }
    const setOv = (o: boolean) => {
      local.visible = !o
      cloudPts.visible = o
      youAreHere.visible = o
      if (o) {
        controls.maxDistance = 200000
        controls.target.set(-origin.x, 0, -origin.z)
        camera.position.set(-origin.x, 70000, -origin.z + 40000)
      } else {
        controls.maxDistance = 400
        controls.target.copy(sel ? rel(sel) : focusRel)
        camera.position.copy(controls.target).add(new THREE.Vector3(0, 38, 62))
      }
    }
    api.current = { select, frame, setOverview: setOv }

    // picking
    const ray = new THREE.Raycaster()
    const mouse = new THREE.Vector2()
    let downAt = { x: 0, y: 0 }
    const onDown = (e: PointerEvent) => (downAt = { x: e.clientX, y: e.clientY })
    const onUp = (e: PointerEvent) => {
      if (Math.hypot(e.clientX - downAt.x, e.clientY - downAt.y) > 5 || !local.visible) return
      const rect = renderer.domElement.getBoundingClientRect()
      mouse.set(((e.clientX - rect.left) / rect.width) * 2 - 1, -((e.clientY - rect.top) / rect.height) * 2 + 1)
      ray.setFromCamera(mouse, camera)
      ray.params.Points = { threshold: Math.max(0.5, camera.position.distanceTo(controls.target) * 0.012) }
      const hits = ray.intersectObject(points)
      if (hits.length) {
        hits.sort((a, b) => (a.distanceToRay ?? 0) - (b.distanceToRay ?? 0))
        select(stars[hits[0].index!])
      }
    }
    renderer.domElement.addEventListener('pointerdown', onDown)
    renderer.domElement.addEventListener('pointerup', onUp)

    // labels pool
    const pool: HTMLDivElement[] = []
    const labelRoot = labelsRef.current!
    const getLabel = (i: number) => {
      if (!pool[i]) {
        const d = document.createElement('div')
        d.className = 'label'
        labelRoot.appendChild(d)
        pool[i] = d
      }
      return pool[i]
    }
    const v = new THREE.Vector3()
    let raf = 0
    let last = ''
    const resize = () => {
      const w = el.clientWidth
      const h = el.clientHeight
      const k = `${w}x${h}`
      if (k === last) return
      last = k
      renderer.setSize(w, h)
      camera.aspect = w / h
      camera.updateProjectionMatrix()
    }
    const loop = () => {
      raf = requestAnimationFrame(loop)
      resize()
      controls.update()
      renderer.render(scene, camera)
      if (cloudPts.visible) {
        youAreHere.position.set(0, 0, 0)
      }
      // labels
      let li = 0
      const w = el.clientWidth
      const h = el.clientHeight
      if (local.visible) {
        const t = controls.target
        const cand = stars
          .map((s) => ({ s, d: rel(s).distanceTo(t) }))
          .filter((x) => x.d < 16 || x.s.id === cur.id || x.s.id === sel?.id || game.player.route.includes(x.s.id))
          .sort((a, b) => a.d - b.d)
          .slice(0, 40)
        for (const { s } of cand) {
          v.copy(rel(s)).project(camera)
          if (v.z > 1 || Math.abs(v.x) > 1.1 || Math.abs(v.y) > 1.1) continue
          const d = getLabel(li++)
          d.style.display = 'block'
          d.style.left = `${(v.x * 0.5 + 0.5) * w}px`
          d.style.top = `${(-v.y * 0.5 + 0.5) * h}px`
          d.className = `label ${s.id === cur.id ? 'cur' : s.id === sel?.id ? 'sel' : game.player.route.includes(s.id) ? 'route' : ''}`
          d.textContent = s.name
        }
      }
      for (let i = li; i < pool.length; i++) pool[i].style.display = 'none'
    }
    loop()
    return () => {
      cancelAnimationFrame(raf)
      unsub()
      renderer.domElement.removeEventListener('pointerdown', onDown)
      renderer.domElement.removeEventListener('pointerup', onUp)
      controls.dispose()
      renderer.dispose()
      renderer.domElement.remove()
      pool.forEach((p) => p.remove())
      api.current = null
    }
    // eslint-disable-next-line react-hooks/exhaustive-deps
  }, [stars, charted])

  const results = useMemo(() => {
    const q = query.trim().toLowerCase()
    if (q.length < 2) return []
    return stars.filter((s) => s.name.toLowerCase().includes(q)).slice(0, 12)
  }, [query, stars])

  const sel = selected
  const d = sel ? distLy(cur, sel) : 0
  const ch = sel ? charted.get(sel.id) : undefined
  const preview = sel ? cachedSystem(sel.id) : undefined
  const visited = sel ? game.player.visited.includes(sel.id) : false

  return (
    <div className="map" ref={wrap}>
      <div ref={labelsRef} />
      <div className="close">
        <button className="btn small" onClick={() => game.setOverlay(null)}>
          Close map (M)
        </button>
        <button
          className="btn small"
          onClick={() => {
            setOverview(!overview)
            api.current?.setOverview(!overview)
          }}
        >
          {overview ? 'Local stars' : 'Galaxy view'}
        </button>
      </div>
      <div className="side left">
        <h2>Galaxy map</h2>
        <div style={{ color: 'var(--text-dim)', fontSize: 13 }}>
          {cur.name} · {cur.region}
        </div>
        <h4>Find star</h4>
        <input className="search" placeholder="Search nearby systems…" value={query} onChange={(e) => setQuery(e.target.value)} />
        <div className="results">
          {results.map((s) => (
            <div
              key={s.id}
              onClick={() => {
                api.current?.select(s)
                api.current?.frame(s)
                setQuery('')
              }}
            >
              {s.name} <span style={{ color: 'var(--text-dim)' }}>· {s.cls} · {distLy(cur, s).toFixed(1)} ly</span>
            </div>
          ))}
        </div>
        <h4>Frame shift drive</h4>
        <div className="kv">
          <span>Jump range</span>
          <span>{range.toFixed(2)} ly</span>
          <span>Fuel</span>
          <span>
            {game.player.fuel.toFixed(1)} / 16 t
          </span>
        </div>
        <h4>Route</h4>
        {route.length === 0 && <p style={{ color: 'var(--text-dim)' }}>No route plotted. Select a star and press Plot route.</p>}
        {route.map((r, i) => (
          <div key={r.id} style={{ fontSize: 13, padding: '2px 0', color: i === 0 ? 'var(--violet)' : undefined }}>
            {i + 1}. {r.name} <span style={{ color: 'var(--text-dim)' }}>· {r.dist.toFixed(1)} ly</span>
          </div>
        ))}
        {route.length > 0 && (
          <button className="btn small" style={{ marginTop: 8 }} onClick={() => game.setHyperTarget(null)}>
            Clear route
          </button>
        )}
        <h4>Legend</h4>
        <div style={{ fontSize: 12, color: 'var(--text-dim)', lineHeight: 1.8 }}>
          <div>
            <span style={{ color: '#6cffb0' }}>◯</span> visited by you
          </div>
          <div>
            <span style={{ color: '#c890ff' }}>◯</span> charted by the Stellar Forge
          </div>
          <div>
            <span style={{ color: '#5ab0ff' }}>—</span> jump range
          </div>
          <div>Drag to orbit · scroll to zoom · click a star</div>
        </div>
      </div>
      {sel && !overview && (
        <div className="side right">
          <h2>{sel.name}</h2>
          <div style={{ color: 'var(--text-dim)', fontSize: 13 }}>{CLASS_LABEL[sel.cls]}</div>
          <div className="kv" style={{ marginTop: 12 }}>
            <span>Distance</span>
            <span>
              {d.toFixed(2)} ly {d <= range ? '· in range' : ''}
            </span>
            <span>Temperature</span>
            <span>{Math.round(sel.temperature).toLocaleString()} K</span>
            <span>Luminosity</span>
            <span>{sel.luminosity < 0.01 ? sel.luminosity.toExponential(1) : sel.luminosity.toFixed(2)} L☉</span>
            <span>Fuel scoop</span>
            <span>{SCOOPABLE[sel.cls] ? 'Scoopable' : 'No'}</span>
            <span>Region</span>
            <span>{sel.region}</span>
            <span>Settlement</span>
            <span>{sel.settlement > 0.6 ? 'Core worlds' : sel.settlement > 0.3 ? 'Inner frontier' : sel.settlement > 0.05 ? 'Frontier' : 'Deep space'}</span>
            {d <= range && (
              <>
                <span>Jump fuel</span>
                <span>{jumpFuel(d, range).toFixed(2)} t</span>
              </>
            )}
          </div>
          <h4>Survey status</h4>
          {visited || preview || ch ? (
            <p>
              {ch?.source === 'ai' && <span className="pill ai">AI charted{ch.discoveredBy ? ` · CMDR ${ch.discoveredBy}` : ''}</span>}
              {preview ? `${preview.profile.description} ` : ''}
              {preview ? `${preview.bodies.length} bodies · ${preview.stations.length} stations · ${preview.profile.faction}` : ch ? `${ch.bodies ?? '?'} bodies · ${ch.stations ?? 0} stations · ${ch.faction ?? ''}` : 'Visited.'}
            </p>
          ) : (
            <p>Uncharted. The Stellar Forge will create this system the moment a pilot jumps in.</p>
          )}
          <div style={{ display: 'flex', gap: 8, marginTop: 16, flexWrap: 'wrap' }}>
            <button className="btn primary" disabled={sel.id === cur.id} onClick={() => game.setHyperTarget(sel.id)}>
              Plot route
            </button>
            <button className="btn" onClick={() => api.current?.frame(sel)}>
              Centre view
            </button>
            {distLy(focus, sel) > LOCAL_RADIUS * 0.6 && (
              <button className="btn" onClick={() => setFocus(sel)}>
                Load stars here
              </button>
            )}
          </div>
          {route.length > 0 && <p style={{ marginTop: 14, color: 'var(--text-dim)' }}>Close the map, centre the violet jump marker and press H to jump.</p>}
        </div>
      )}
    </div>
  )
}
