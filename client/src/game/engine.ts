// OmniSpace core: rendering pipeline, flight model (normal flight, supercruise,
// hyperspace), docking, scanning, trading and the bridge to the React HUD.

import * as THREE from 'three'
import { EffectComposer } from 'three/examples/jsm/postprocessing/EffectComposer.js'
import { OutputPass } from 'three/examples/jsm/postprocessing/OutputPass.js'
import { RenderPass } from 'three/examples/jsm/postprocessing/RenderPass.js'
import { UnrealBloomPass } from 'three/examples/jsm/postprocessing/UnrealBloomPass.js'
import { checkForge, loadSystem, prefetchSystem, cachedSystem, lastForgeError } from './api'
import { audio } from './audio'
import { market, missionBoard, SCAN_VALUE, type Mission } from './economy'
import { CLASS_LABEL, distLy, getStar, HOME_ID, HOME_STAR, plotRoute, SCOOPABLE, starsNear } from './galaxy'
import { fmtDist, fmtSpeed, useHud, type FlightMode, type Msg, type Objective } from './hud'
import { Input } from './input'
import { cargoTotal, jumpFuel, jumpRange, newPlayer, SHIP, writeSave, type PlayerSave } from './player'
import { clamp } from './rng'
import { Cockpit } from './render/cockpit'
import { Skybox } from './render/skybox'
import { stationSDF } from './render/station'
import { SystemScene } from './render/systemScene'
import { Tunnel } from './render/tunnel'
import { SpaceDust } from './render/dust'
import { COMMODITIES } from './lore'
import type { NavObject, StarDescriptor, StarSystem, Station } from './types'

const C = 299_792_458
const SC_MIN = 30_000
const SC_MAX = 2001 * C
const SHIP_RADIUS = 22
const DOCK_RANGE = 7500
const MASS_LOCK = 6000

interface Fsd {
  kind: 'sc' | 'jump'
  t: number
  dur: number
  dest?: string
  waitAlign?: number
}

interface HyperState {
  dest: StarDescriptor
  t: number
  ready: StarSystem | null
  log: string[]
  exitT: number
  ai: boolean
}

export class Game {
  readonly canvas: HTMLCanvasElement
  readonly renderer: THREE.WebGLRenderer
  readonly camera: THREE.PerspectiveCamera
  readonly scene = new THREE.Scene()
  readonly sky: Skybox
  readonly cockpit = new Cockpit()
  readonly tunnel = new Tunnel()
  readonly dust = new SpaceDust()
  private lastRenderPos = new THREE.Vector3()
  private composer: EffectComposer
  private skyPass: RenderPass
  private mainPass: RenderPass
  private bloom: UnrealBloomPass
  input: Input
  sys: SystemScene | null = null
  player: PlayerSave

  // ship state
  pos = new THREE.Vector3()
  vel = new THREE.Vector3()
  quat = new THREE.Quaternion()
  angVel = new THREE.Vector3()
  throttle = 0
  scSpeed = 0
  boostT = 0
  boostCharge = 1
  shields = 1
  heat = 0.2
  lastHit = -100
  lastImpact = -100
  mode: FlightMode = 'normal'
  fa = true
  autopilot = false
  gear = false
  target: string | null = null
  hyperTarget: string | null = null
  fsd: Fsd | null = null
  hyper: HyperState | null = null
  docking: { stationId: string; bay: number; granted: boolean; expires: number } | null = null
  dockAnim: { from: THREE.Vector3; fromQ: THREE.Quaternion; t: number; stationId: string; bay: number } | null = null
  launchT = 0
  apStage = 0
  time = 0
  private last = performance.now()
  private hudT = 0
  private saveT = 0
  private raf = 0
  private running = false
  private messages: Msg[] = []
  private msgId = 0
  private radarCtx: CanvasRenderingContext2D | null = null
  private fxCtx: CanvasRenderingContext2D | null = null
  private flags = { galaxyOpened: false }
  private paused = false
  private lastSize = ''
  private navCache: NavObject[] = []
  private investigated = new Set<string>()
  /** Optional deterministic stepping for automated tests. */
  fixedDt: number | null = null

  constructor(canvas: HTMLCanvasElement, player: PlayerSave) {
    this.canvas = canvas
    this.player = player
    this.renderer = new THREE.WebGLRenderer({ canvas, antialias: true, logarithmicDepthBuffer: true, powerPreference: 'high-performance' })
    this.renderer.setPixelRatio(Math.min(window.devicePixelRatio, 1.5))
    this.renderer.toneMapping = THREE.ACESFilmicToneMapping
    this.renderer.toneMappingExposure = 1.05
    this.renderer.outputColorSpace = THREE.SRGBColorSpace
    this.renderer.autoClear = false
    this.camera = new THREE.PerspectiveCamera(62, 16 / 9, 0.05, 1e15)
    this.scene.add(this.camera)
    this.camera.add(this.cockpit.group)
    this.camera.add(this.tunnel.group)
    this.scene.add(this.dust.lines)
    this.sky = new Skybox(62)
    this.composer = new EffectComposer(this.renderer)
    this.skyPass = new RenderPass(this.sky.scene, this.sky.camera)
    this.mainPass = new RenderPass(this.scene, this.camera)
    this.mainPass.clear = false
    this.mainPass.clearDepth = true
    this.bloom = new UnrealBloomPass(new THREE.Vector2(512, 512), 0.85, 0.55, 0.8)
    this.composer.addPass(this.skyPass)
    this.composer.addPass(this.mainPass)
    this.composer.addPass(this.bloom)
    this.composer.addPass(new OutputPass())
    this.input = new Input(canvas, (code, e) => this.onKey(code, e))
    ;(window as unknown as { omni: Game }).omni = this
    ;(window as unknown as { __nearest: () => StarDescriptor }).__nearest = () => {
      const cur = getStar(this.player.systemId) ?? HOME_STAR
      return starsNear(cur.pos, 20).filter((s) => s.id !== cur.id).sort((a, b) => distLy(a, cur) - distLy(b, cur))[0]
    }
  }

  // ------------------------------------------------------------------ setup
  async start() {
    const forge = await checkForge()
    useHud.setState({ forge, commander: this.player.commander })
    const star = getStar(this.player.systemId) ?? HOME_STAR
    const sys = await loadSystem(star, this.player.commander)
    this.enterSystem(sys, 'load')
    this.running = true
    this.last = performance.now()
    this.loop()
    useHud.setState({ ready: true })
  }

  /** Replace the commander (new game) and respawn at Tanara Station. */
  resetPlayer(p: PlayerSave) {
    this.player = p
    this.shields = 1
    this.autopilot = false
    this.fsd = null
    this.hyperTarget = null
    void loadSystem(HOME_STAR, p.commander).then((sys) => this.enterSystem(sys, 'load'))
    useHud.setState({ commander: p.commander, messages: [] })
    this.messages = []
  }

  stop() {
    this.running = false
    cancelAnimationFrame(this.raf)
    this.input.dispose()
    this.save()
  }

  attachRadar(c: HTMLCanvasElement | null) {
    this.radarCtx = c?.getContext('2d') ?? null
  }
  attachFx(c: HTMLCanvasElement | null) {
    this.fxCtx = c?.getContext('2d') ?? null
  }

  private enterSystem(sys: StarSystem, how: 'load' | 'jump') {
    if (this.sys) {
      this.scene.remove(this.sys.root)
      this.sys.dispose()
    }
    this.sys = new SystemScene(sys)
    this.scene.add(this.sys.root)
    this.sky.setSystem(sys.star, this.sys.skyTilt)
    this.navCache = this.sys.layout.nav
    this.player.systemId = sys.id
    this.target = null
    this.docking = null
    this.investigated.clear()
    if (how === 'load') {
      const p = this.player
      if (p.dockedAt && this.sys.stations.has(p.dockedAt)) {
        this.dockAt(p.dockedAt, p.dockedBay, true)
      } else if (p.pos[0] === 0 && p.pos[1] === 0 && p.pos[2] === 0) {
        this.spawnNearStation(sys.stations[0]?.id)
      } else {
        this.pos.set(...p.pos)
        this.quat.set(...p.quat)
        this.mode = 'normal'
        this.vel.set(0, 0, 0)
        // resume in supercruise when parked in open space
        const nearStation = [...this.sys.stations.values()].some((st) => st.abs.distanceTo(this.pos) < 200_000)
        const nearBody = this.sys.sys.bodies.some((b) => this.sys!.bodyAbs.get(b.id)!.distanceTo(this.pos) - b.radiusM < Math.max(b.radiusM * 3, 1e7))
        if (!nearStation && !nearBody) {
          this.mode = 'supercruise'
          this.scSpeed = SC_MIN
        }
      }
    } else {
      // hyperspace arrival: near the star, in supercruise, star off to one side
      const R = this.sys.star.radius
      const d = Math.max(R * 9, 4e8)
      const dir = new THREE.Vector3(0.6, 0.25, 0.75).applyAxisAngle(new THREE.Vector3(0, 1, 0), (sys.id.length * 1.7) % 6.28).normalize()
      this.pos.copy(dir).multiplyScalar(d)
      const look = new THREE.Vector3().sub(this.pos).normalize()
      const side = new THREE.Vector3().crossVectors(look, new THREE.Vector3(0, 1, 0)).normalize()
      look.applyAxisAngle(side, 0.0).applyAxisAngle(new THREE.Vector3(0, 1, 0), 0.42)
      this.lookAlong(look)
      this.mode = 'supercruise'
      this.scSpeed = SC_MIN
      this.throttle = 0
      this.vel.set(0, 0, 0)
      this.onArrival(sys)
    }
    this.pushHudStatic()
  }

  private spawnNearStation(stationId?: string) {
    if (!this.sys) return
    const st = stationId ? this.sys.stations.get(stationId) : undefined
    if (!st) {
      this.pos.set(this.sys.star.radius * 20, this.sys.star.radius * 2, 0)
      this.lookAt(new THREE.Vector3())
      this.mode = 'supercruise'
      this.scSpeed = SC_MIN
      return
    }
    // concept-art framing: station ahead-right, host planet + rings on the left
    const host = this.sys.sys.stations.find((s) => s.id === st.id)!.parent
    const hostPos = this.sys.bodyAbs.get(host) ?? new THREE.Vector3()
    const toHost = hostPos.clone().sub(st.abs).normalize()
    const up = new THREE.Vector3(0, 1, 0)
    const side = new THREE.Vector3().crossVectors(toHost, up).normalize()
    const offset = toHost.clone().multiplyScalar(-5200).addScaledVector(side, -2400).addScaledVector(up, 700)
    this.pos.copy(st.abs).add(offset)
    const dS = st.abs.clone().sub(this.pos).normalize()
    const dH = hostPos.clone().sub(this.pos).normalize()
    this.lookAlong(dS.lerp(dH, 0.5).normalize())
    this.quat.multiply(new THREE.Quaternion().setFromEuler(new THREE.Euler(-0.13, -0.1, 0)))
    this.mode = 'normal'
    this.vel.set(0, 0, 0)
    this.throttle = 0
    this.target = st.id
  }

  private lookAt(p: THREE.Vector3) {
    this.lookAlong(p.clone().sub(this.pos).normalize())
  }
  private lookAlong(dir: THREE.Vector3) {
    // Matrix4.lookAt(eye, target) makes -Z point from eye toward target.
    const m = new THREE.Matrix4().lookAt(new THREE.Vector3(), dir, new THREE.Vector3(0, 1, 0))
    this.quat.setFromRotationMatrix(m)
  }

  // ------------------------------------------------------------------ messages
  msg(text: string, tone: Msg['tone'] = 'info') {
    this.messages.push({ id: ++this.msgId, text, tone, t: this.time })
    if (this.messages.length > 60) this.messages.shift()
    useHud.setState({ messages: [...this.messages] })
  }

  // ------------------------------------------------------------------ input
  private onKey(code: string, e: KeyboardEvent) {
    const hud = useHud.getState()
    if (hud.overlay === 'title') return
    if (code === 'Escape') {
      if (hud.overlay) {
        this.setOverlay(null)
      } else {
        this.setOverlay('pause')
      }
      return
    }
    if (code === 'KeyM') {
      this.setOverlay(hud.overlay === 'galaxy' ? null : 'galaxy')
      return
    }
    if (code === 'KeyN') {
      this.setOverlay(hud.overlay === 'system' ? null : 'system')
      return
    }
    if (code === 'F1' || code === 'Slash') {
      e.preventDefault()
      this.setOverlay(hud.overlay === 'help' ? null : 'help')
      return
    }
    if (hud.overlay) return
    if (this.mode === 'docked') {
      if (code === 'KeyL' || code === 'Enter') this.launch()
      return
    }
    switch (code) {
      case 'KeyJ':
        this.toggleSupercruise()
        break
      case 'KeyH':
        this.engageJump()
        break
      case 'KeyT':
        this.cycleTarget(e.shiftKey ? -1 : 1)
        break
      case 'KeyL':
        this.requestDocking()
        break
      case 'KeyG':
        this.gear = !this.gear
        this.msg(this.gear ? 'Landing gear deployed' : 'Landing gear retracted')
        audio.blip()
        break
      case 'KeyZ':
        this.fa = !this.fa
        this.msg(this.fa ? 'Flight assist ON — stabilized' : 'Flight assist OFF', this.fa ? 'info' : 'warn')
        audio.blip()
        break
      case 'KeyP':
        this.toggleAutopilot()
        break
      case 'KeyX':
        this.throttle = 0
        break
      case 'KeyC':
        this.throttle = 0.75
        break
      case 'Digit1':
      case 'Digit2':
      case 'Digit3':
      case 'Digit4':
      case 'Digit5':
        useHud.setState({ tab: (['NAV', 'COM', 'SHIP', 'MAP', 'SYS'] as const)[+code.slice(-1) - 1] })
        audio.blip()
        break
    }
  }

  setOverlay(o: ReturnType<typeof useHud.getState>['overlay']) {
    useHud.setState({ overlay: o })
    if (o) this.input.releaseMouse()
    if (o === 'galaxy') this.flags.galaxyOpened = true
    this.paused = o === 'pause' || o === 'help'
    audio.blip()
  }

  toggleAutopilot() {
    if (!this.autopilot && !this.target && !this.nextJump()) {
      this.msg('Autopilot needs a target (press T or pick one in NAV) or a plotted route', 'warn')
      audio.deny()
      return
    }
    this.autopilot = !this.autopilot
    this.apStage = 0
    this.msg(this.autopilot ? 'Autopilot engaged' : 'Autopilot disengaged', 'info')
    audio.blip()
  }

  setTarget(id: string | null) {
    this.target = id
    this.apStage = 0
    if (id) {
      const n = this.navCache.find((x) => x.id === id)
      if (n) this.msg(`Target locked: ${n.name}`)
    }
    audio.blip()
  }

  cycleTarget(dir: number) {
    const list = this.sortedNav()
    if (!list.length) return
    const i = list.findIndex((n) => n.id === this.target)
    const next = list[(i + dir + list.length) % list.length]
    this.setTarget(next.id)
  }

  private sortedNav() {
    return [...this.navCache].sort((a, b) => this.distTo(a) - this.distTo(b))
  }

  navPos(n: NavObject): THREE.Vector3 {
    if (n.kind === 'station' && this.sys) return this.sys.stations.get(n.id)?.abs ?? new THREE.Vector3(...n.pos)
    return new THREE.Vector3(...n.pos)
  }

  distTo(n: NavObject) {
    return this.navPos(n).distanceTo(this.pos)
  }

  // ------------------------------------------------------------------ hyperspace / route
  setHyperTarget(starId: string | null) {
    this.hyperTarget = starId
    this.player.route = []
    if (starId) {
      const cur = getStar(this.player.systemId) ?? HOME_STAR
      const dest = getStar(starId)
      if (!dest) return
      const range = jumpRange(this.player)
      if (distLy(cur, dest) <= range) {
        this.player.route = [starId]
      } else {
        const r = plotRoute(cur, dest, range)
        if (r && r.length > 1) this.player.route = r.slice(1).map((s) => s.id)
        else this.msg('No route found within jump range', 'warn')
      }
      if (this.player.route.length) {
        const next = getStar(this.player.route[0])
        if (next) prefetchSystem(next, this.player.commander)
        this.msg(`Route plotted: ${dest.name} · ${this.player.route.length} jump${this.player.route.length > 1 ? 's' : ''}`, 'good')
      }
    }
    this.pushHudStatic()
  }

  private nextJump(): StarDescriptor | null {
    const id = this.player.route.find((r) => r !== this.player.systemId)
    return id ? getStar(id) ?? null : null
  }

  /** Direction (system frame) toward another star in the galaxy. */
  starDirection(s: StarDescriptor): THREE.Vector3 {
    const cur = getStar(this.player.systemId) ?? HOME_STAR
    const d = new THREE.Vector3(s.pos[0] - cur.pos[0], s.pos[1] - cur.pos[1], s.pos[2] - cur.pos[2]).normalize()
    return this.sys ? d.applyQuaternion(this.sys.skyTilt) : d
  }

  engageJump() {
    if (this.mode !== 'normal' && this.mode !== 'supercruise') return
    const dest = this.nextJump()
    if (!dest) {
      this.msg('No hyperspace destination. Open the galaxy map (M) and plot a route.', 'warn')
      audio.deny()
      return
    }
    if (this.fsd) {
      this.cancelFsd('FSD charge cancelled')
      return
    }
    const cur = getStar(this.player.systemId) ?? HOME_STAR
    const d = distLy(cur, dest)
    const range = jumpRange(this.player)
    if (d > range) {
      this.msg(`${dest.name} is out of range (${d.toFixed(1)} ly > ${range.toFixed(1)} ly)`, 'bad')
      audio.deny()
      return
    }
    const fuel = jumpFuel(d, range)
    if (this.player.fuel < fuel) {
      this.msg(`Insufficient fuel for jump (${fuel.toFixed(1)} t needed). Scoop a star or refuel.`, 'bad')
      audio.deny()
      return
    }
    if (this.mode === 'normal' && this.massLocked()) {
      this.msg('Mass locked — move away from the station', 'warn')
      audio.deny()
      return
    }
    this.gear = false
    this.fsd = { kind: 'jump', t: 0, dur: 5, dest: dest.id }
    prefetchSystem(dest, this.player.commander)
    audio.startCharge(5)
    this.msg(`Frame shift drive charging · destination ${dest.name}`, 'info')
  }

  private cancelFsd(text: string) {
    this.fsd = null
    audio.stopCharge()
    this.msg(text, 'warn')
  }

  toggleSupercruise() {
    if (this.fsd?.kind === 'sc') {
      this.cancelFsd('Supercruise cancelled')
      return
    }
    if (this.mode === 'supercruise') {
      this.dropFromSupercruise(false)
      return
    }
    if (this.mode !== 'normal') return
    if (this.massLocked()) {
      this.msg('Mass locked — move further from the station', 'warn')
      audio.deny()
      return
    }
    this.gear = false
    this.fsd = { kind: 'sc', t: 0, dur: 3 }
    audio.startCharge(3)
    this.msg('Frame shift drive charging · supercruise', 'info')
  }

  massLocked(): boolean {
    if (!this.sys) return false
    for (const st of this.sys.stations.values()) if (st.abs.distanceTo(this.pos) < MASS_LOCK) return true
    return false
  }

  private dropFromSupercruise(auto: boolean) {
    if (!this.sys) return
    const t = this.target ? this.navCache.find((n) => n.id === this.target) : undefined
    let placed = false
    if (t) {
      const tp = this.navPos(t)
      const d = tp.distanceTo(this.pos)
      const zone = t.kind === 'station' ? 120_000 : t.kind === 'poi' ? 120_000 : t.kind === 'belt' ? 4e6 : 0
      if (zone && d < zone) {
        const dir = this.pos.clone().sub(tp).normalize()
        const standoff = t.kind === 'station' ? 7000 : t.kind === 'poi' ? 2600 : 2000
        this.pos.copy(tp).addScaledVector(dir, standoff)
        this.lookAt(tp)
        placed = true
        this.msg(`Arrived at ${t.name}`, 'good')
      }
    }
    if (!placed && !auto) this.msg('Supercruise disengaged')
    this.mode = 'normal'
    this.scSpeed = 0
    this.vel.set(0, 0, -1).applyQuaternion(this.quat).multiplyScalar(placed ? 120 : 200)
    this.throttle = placed ? 0.35 : 0.5
    this.apStage = 0
    audio.whoosh()
  }

  // ------------------------------------------------------------------ docking
  requestDocking() {
    if (!this.sys || this.mode !== 'normal') return
    let best: { id: string; d: number } | null = null
    for (const st of this.sys.stations.values()) {
      const d = st.abs.distanceTo(this.pos)
      if (d < DOCK_RANGE && (!best || d < best.d || st.id === this.target)) best = { id: st.id, d }
    }
    if (!best) {
      this.msg(`No station within ${DOCK_RANGE / 1000} km`, 'warn')
      audio.deny()
      return
    }
    const st = this.sys.stations.get(best.id)!
    const bay = Math.floor(Math.random() * st.model.bays.length)
    this.docking = { stationId: best.id, bay, granted: true, expires: this.time + 300 }
    this.apStage = 0
    this.target = best.id
    const name = this.sys.sys.stations.find((s) => s.id === best!.id)?.name
    this.msg(`${name}: Docking request granted. Proceed to bay ${st.model.bays[bay].label}.`, 'good')
    audio.confirm()
  }

  private dockAt(stationId: string, bay: number, instant: boolean) {
    if (!this.sys) return
    this.mode = 'docked'
    this.player.dockedAt = stationId
    this.player.dockedBay = bay
    this.player.lastStation = { systemId: this.player.systemId, stationId }
    this.docking = null
    this.autopilot = false
    this.vel.set(0, 0, 0)
    this.angVel.set(0, 0, 0)
    this.throttle = 0
    this.scSpeed = 0
    this.placeAtBay(stationId, bay)
    useHud.setState({ stationId })
    if (!instant) {
      audio.clunk()
      const name = this.sys.sys.stations.find((s) => s.id === stationId)?.name
      this.msg(`Docked at ${name}. Welcome, Commander.`, 'good')
      this.onDocked(stationId)
    }
    this.save()
  }

  private placeAtBay(stationId: string, bay: number) {
    const b = this.sys?.bayWorld(stationId, bay)
    if (!b) return
    this.pos.copy(b.pos).addScaledVector(b.normal, 25)
    this.lookAlong(b.normal.clone())
  }

  launch() {
    if (this.mode !== 'docked') return
    this.mode = 'launching'
    this.launchT = 0
    useHud.setState({ stationId: null })
    this.msg('Launching. Fly safe, Commander.', 'info')
    audio.clunk()
    this.save()
  }

  private onDocked(stationId: string) {
    const p = this.player
    // complete missions
    const done = p.missions.filter((m) => m.destSystem === p.systemId && (!m.destStation || m.destStation === stationId))
    for (const m of done) {
      p.credits += m.reward
      p.stats.earned += m.reward
      if (m.commodity) delete p.missionCargo[m.id]
      this.msg(`Contract complete: ${m.title} · +${m.reward.toLocaleString()} cr`, 'good')
    }
    p.missions = p.missions.filter((m) => !done.includes(m))
    if (p.tutorial === 0 && stationId === 'tanara-station') {
      p.tutorial = 1
      p.credits += 1000
      this.msg('Tanara concourse: Welcome aboard. A 1,000 cr starter grant has been credited.', 'good')
    }
  }

  // ------------------------------------------------------------------ station services
  stationData(): Station | null {
    const id = this.player.dockedAt
    return id ? this.sys?.sys.stations.find((s) => s.id === id) ?? null : null
  }
  day() {
    return Math.floor(Date.now() / (1000 * 60 * 30))
  }
  marketRows() {
    const st = this.stationData()
    return st ? market(st, this.day()) : []
  }
  buy(id: string, qty: number) {
    const row = this.marketRows().find((r) => r.id === id)
    if (!row) return
    const free = SHIP.cargoCap - cargoTotal(this.player)
    const n = Math.min(qty, free, Math.floor(this.player.credits / row.buy), row.supply)
    if (n <= 0) {
      audio.deny()
      return
    }
    this.player.credits -= n * row.buy
    this.player.cargo[id] = (this.player.cargo[id] ?? 0) + n
    audio.confirm()
    this.pushHudStatic()
  }
  sell(id: string, qty: number) {
    const row = this.marketRows().find((r) => r.id === id)
    const have = this.player.cargo[id] ?? 0
    const n = Math.min(qty, have)
    if (!row || n <= 0) return
    this.player.credits += n * row.sell
    this.player.stats.earned += n * row.sell
    this.player.cargo[id] = have - n
    if (!this.player.cargo[id]) delete this.player.cargo[id]
    audio.confirm()
    this.pushHudStatic()
  }
  refuelCost() {
    return Math.ceil((SHIP.fuelMax - this.player.fuel) * 55)
  }
  refuel() {
    const cost = this.refuelCost()
    if (cost <= 0 || this.player.credits < cost) return audio.deny()
    this.player.credits -= cost
    this.player.fuel = SHIP.fuelMax
    audio.confirm()
    this.pushHudStatic()
  }
  repairCost() {
    return Math.ceil((1 - this.player.hull) * 6000)
  }
  repair() {
    const cost = this.repairCost()
    if (cost <= 0 || this.player.credits < cost) return audio.deny()
    this.player.credits -= cost
    this.player.hull = 1
    audio.confirm()
    this.pushHudStatic()
  }
  dataValue() {
    return this.player.data.reduce((s, d) => s + d.value, 0)
  }
  sellData() {
    const v = this.dataValue()
    if (v <= 0) return
    this.player.credits += v
    this.player.stats.earned += v
    this.player.data = []
    if (this.player.tutorial === 2) {
      this.player.tutorial = 3
      this.msg('Universal Cartographics: data accepted. You are now a registered explorer.', 'good')
    }
    this.msg(`Exploration data sold · +${v.toLocaleString()} cr`, 'good')
    audio.confirm()
    this.pushHudStatic()
  }
  missionsOffered(): Mission[] {
    const st = this.stationData()
    if (!st || !this.sys) return []
    const taken = new Set(this.player.missions.map((m) => m.id))
    return missionBoard(st, this.sys.sys.star, this.sys.sys.stations, this.day()).filter((m) => !taken.has(m.id))
  }
  acceptMission(m: Mission) {
    if (this.player.missions.length >= 6) return audio.deny()
    if (m.tons > SHIP.cargoCap - cargoTotal(this.player)) {
      this.msg('Not enough cargo space for this contract', 'warn')
      return audio.deny()
    }
    this.player.missions.push(m)
    if (m.tons) this.player.missionCargo[m.id] = m.tons
    this.msg(`Contract accepted: ${m.title}`, 'good')
    audio.confirm()
    this.pushHudStatic()
  }

  // ------------------------------------------------------------------ discovery / scanning
  private onArrival(sys: StarSystem) {
    const p = this.player
    const first = !p.visited.includes(sys.id)
    if (first) {
      p.visited.push(sys.id)
      p.stats.systemsDiscovered++
      const bodies = sys.bodies.length
      const value = 500 + bodies * 180 + (sys.source === 'ai' ? 400 : 0)
      p.data.push({ systemId: sys.id, systemName: sys.name, bodyId: '*', name: `${sys.name} discovery scan`, value, kind: 'discovery' })
      this.msg(`Discovery scan: ${bodies} bod${bodies === 1 ? 'y' : 'ies'}, ${sys.stations.length} station${sys.stations.length === 1 ? '' : 's'} registered (+${value} cr data)`, 'good')
      if (sys.source === 'ai' && sys.discoveredBy === p.commander) this.msg(`First discovery! The Stellar Forge charted ${sys.name} for CMDR ${p.commander}.`, 'ai')
    }
    this.msg(`${sys.name} · ${sys.profile.faction} · ${sys.profile.economy} · Security: ${sys.profile.security}`, 'info')
    if (sys.profile.description) this.msg(sys.profile.description, 'ai')
  }

  private scanCheck() {
    if (!this.sys) return
    const p = this.player
    for (const b of this.sys.sys.bodies) {
      const key = `${this.sys.sys.id}:${b.id}`
      if (p.scanned.includes(key)) continue
      const abs = this.sys.bodyAbs.get(b.id)!
      const d = abs.distanceTo(this.pos) - b.radiusM
      if (d < b.radiusM * 5 + 2e6) {
        p.scanned.push(key)
        let value = SCAN_VALUE[b.type] ?? 800
        if (b.kind === 'moon') value = Math.round(value * 0.6)
        if (this.sys.sys.discoveredBy === p.commander) value = Math.round(value * 1.5)
        p.data.push({ systemId: this.sys.sys.id, systemName: this.sys.sys.name, bodyId: b.id, name: b.name, value, kind: 'surface' })
        this.msg(`Surface scan complete: ${b.name} — ${b.type.replace('_', ' ')} (+${value.toLocaleString()} cr data)`, 'good')
        audio.scan()
        if (p.tutorial === 2) this.msg('Objective: sell your exploration data at any station with cartographics.', 'info')
      }
    }
    if (this.mode === 'normal') {
      for (const poi of this.sys.sys.pois) {
        const key = `${this.sys.sys.id}:${poi.id}`
        if (p.scanned.includes(key)) continue
        const r = this.sys.pois.get(poi.id)
        if (r && r.abs.distanceTo(this.pos) < 4000) {
          p.scanned.push(key)
          p.data.push({ systemId: this.sys.sys.id, systemName: this.sys.sys.name, bodyId: poi.id, name: poi.name, value: poi.reward, kind: 'poi' })
          this.msg(`Investigated ${poi.name}: ${poi.description} (+${poi.reward.toLocaleString()} cr data)`, 'ai')
          audio.scan()
        }
      }
    }
  }

  // ------------------------------------------------------------------ main loop
  private loop = () => {
    if (!this.running) return
    this.raf = requestAnimationFrame(this.loop)
    const now = performance.now()
    let dt = (now - this.last) / 1000
    this.last = now
    if (this.fixedDt) dt = this.fixedDt
    dt = Math.min(dt, 0.1)
    this.frame(dt)
  }

  /** Run the simulation without rendering (automated tests / fast-forward). */
  step(dt: number, n: number) {
    for (let i = 0; i < n; i++) {
      this.time += dt
      this.simulate(dt)
      if (this.sys) this.sys.update(this.pos, dt, this.time, this.mode === 'normal')
    }
    this.pushHud()
  }

  /** Advance simulation + render one frame (exposed for automated tests). */
  frame(dt: number) {
    const overlay = useHud.getState().overlay
    if (!this.paused) {
      this.time += dt
      this.simulate(dt)
    }
    if (overlay !== 'galaxy' && overlay !== 'system') this.render(dt)
    this.hudT += dt
    if (this.hudT > 0.066) {
      this.hudT = 0
      this.pushHud()
    }
    this.saveT += dt
    if (this.saveT > 10) {
      this.saveT = 0
      this.save()
    }
  }

  private controls(dt: number) {
    const i = this.input
    i.decay(dt)
    const pitch = clamp(-i.stick.y + i.axis(['ArrowDown'], ['ArrowUp']), -1, 1)
    const yaw = clamp(-i.stick.x + i.axis(['ArrowRight'], ['ArrowLeft']), -1, 1)
    const roll = i.axis(['KeyD'], ['KeyA'])
    const strafe = i.axis(['KeyQ'], ['KeyE'])
    const vert = i.axis(['KeyF'], ['KeyR'])
    const thr = i.axis(['KeyS'], ['KeyW'])
    if (thr) this.throttle = clamp(this.throttle + thr * dt * 0.75, this.mode === 'supercruise' ? 0 : -0.5, 1)
    if (i.wheel) {
      this.throttle = clamp(this.throttle + i.wheel * 0.1, this.mode === 'supercruise' ? 0 : -0.5, 1)
      i.wheel = 0
    }
    const boost = i.down('Tab') || i.down('Space')
    return { pitch, yaw, roll, strafe, vert, boost, manual: Math.abs(pitch) + Math.abs(yaw) + Math.abs(roll) > 0.05 }
  }

  private simulate(dt: number) {
    if (!this.sys) return
    const ctl = this.controls(dt)
    if (useHud.getState().overlay) {
      ctl.pitch = ctl.yaw = ctl.roll = ctl.strafe = ctl.vert = 0
      ctl.boost = false
      ctl.manual = false
    }
    if (ctl.manual && this.autopilot && this.mode !== 'docking' && Math.abs(this.input.stick.x) + Math.abs(this.input.stick.y) + Math.abs(ctl.roll) > 0.4) {
      this.autopilot = false
      this.msg('Autopilot disengaged — manual override', 'warn')
    }
    let { pitch, yaw, roll, strafe, vert } = ctl
    let boost = ctl.boost
    if (this.autopilot && (this.mode === 'normal' || this.mode === 'supercruise')) {
      const ap = this.autopilotControls()
      if (ap) {
        pitch = ap.pitch
        yaw = ap.yaw
        roll = 0
        strafe = 0
        vert = 0
        boost = false
        this.throttle = ap.throttle
        if (ap.engageSC && !this.fsd && this.mode === 'normal' && !this.massLocked()) this.toggleSupercruise()
        if (ap.dropSC && this.mode === 'supercruise') this.dropFromSupercruise(true)
      }
    }

    // FSD charging
    if (this.fsd) {
      this.fsd.t += dt
      if (this.fsd.t >= this.fsd.dur) {
        if (this.fsd.kind === 'sc') {
          this.fsd = null
          this.mode = 'supercruise'
          this.scSpeed = Math.max(SC_MIN, this.vel.length())
          this.throttle = Math.max(this.throttle, 0.0)
          audio.whoosh()
          this.msg('Supercruise engaged', 'info')
        } else {
          const dest = getStar(this.fsd.dest!)
          if (!dest) {
            this.fsd = null
          } else {
            const dir = this.starDirection(dest)
            const fwd = new THREE.Vector3(0, 0, -1).applyQuaternion(this.quat)
            const aligned = fwd.angleTo(dir) < 0.2
            if (aligned) {
              this.fsd = null
              this.beginHyperspace(dest)
            } else {
              this.fsd.waitAlign = (this.fsd.waitAlign ?? 0) + dt
              if (this.fsd.waitAlign > 20) this.cancelFsd('Jump aborted — not aligned with destination')
            }
          }
        }
      }
    }

    switch (this.mode) {
      case 'normal':
        this.flyNormal(dt, pitch, yaw, roll, strafe, vert, boost)
        break
      case 'supercruise':
        this.flySupercruise(dt, pitch, yaw, roll)
        break
      case 'hyperspace':
        this.flyHyperspace(dt)
        break
      case 'docking':
        this.flyDocking(dt)
        break
      case 'docked':
        if (this.player.dockedAt) this.placeAtBay(this.player.dockedAt, this.player.dockedBay)
        break
      case 'launching':
        this.flyLaunch(dt)
        break
      case 'destroyed':
        break
    }

    // shields regen
    if (this.time - this.lastHit > 4) this.shields = Math.min(1, this.shields + dt * 0.04)
    if (this.mode !== 'hyperspace' && this.mode !== 'docked') this.scanCheck()
    if (this.docking && this.time > this.docking.expires) {
      this.msg('Docking clearance expired', 'warn')
      this.docking = null
    }
    const speedNorm = this.mode === 'supercruise' ? clamp(Math.log10(this.scSpeed / SC_MIN) / 6, 0, 1) : 0
    audio.update(this.mode === 'docked' ? 0 : this.throttle, this.boostT > 0, this.mode === 'supercruise' ? 1 : this.mode === 'hyperspace' ? 2 : 0, speedNorm)
  }

  private rotate(dt: number, pitch: number, yaw: number, roll: number, rates: [number, number, number]) {
    const blue = this.mode === 'normal' ? (this.throttle > 0.45 && this.throttle < 0.8 ? 1 : 0.8) : 1
    const target = new THREE.Vector3(pitch * rates[0] * blue, yaw * rates[1] * blue, roll * rates[2])
    if (this.fa || this.mode !== 'normal') {
      this.angVel.lerp(target, 1 - Math.exp(-dt * 5))
    } else {
      const acc = 2.2
      this.angVel.x += clamp(target.x - this.angVel.x, -acc * dt, acc * dt) * (Math.abs(pitch) > 0.02 ? 1 : 0)
      this.angVel.y += clamp(target.y - this.angVel.y, -acc * dt, acc * dt) * (Math.abs(yaw) > 0.02 ? 1 : 0)
      this.angVel.z += clamp(target.z - this.angVel.z, -acc * dt, acc * dt) * (Math.abs(roll) > 0.02 ? 1 : 0)
    }
    const dq = new THREE.Quaternion().setFromEuler(new THREE.Euler(this.angVel.x * dt, this.angVel.y * dt, this.angVel.z * dt, 'XYZ'))
    this.quat.multiply(dq).normalize()
  }

  private flyNormal(dt: number, pitch: number, yaw: number, roll: number, strafe: number, vert: number, boost: boolean) {
    this.rotate(dt, pitch, yaw, roll, [0.85, 0.5, 1.7])
    if (boost && this.boostCharge >= 1 && this.boostT <= 0) {
      this.boostT = 3
      this.boostCharge = 0
      audio.whoosh()
    }
    if (this.boostT > 0) this.boostT -= dt
    else this.boostCharge = Math.min(1, this.boostCharge + dt / 6)
    const maxV = this.boostT > 0 ? SHIP.boostSpeed : SHIP.maxSpeed
    const inv = this.quat.clone().invert()
    const local = this.vel.clone().applyQuaternion(inv)
    const fwdTarget = this.boostT > 0 ? -SHIP.boostSpeed : -this.throttle * SHIP.maxSpeed * (this.gear ? 0.5 : 1)
    if (this.fa) {
      const desired = new THREE.Vector3(strafe * 100, vert * 100, fwdTarget)
      const ax = 90 * dt
      local.x += clamp(desired.x - local.x, -ax, ax)
      local.y += clamp(desired.y - local.y, -ax, ax)
      local.z += clamp(desired.z - local.z, -(this.boostT > 0 ? 260 : 150) * dt, 150 * dt)
    } else {
      local.x += strafe * 90 * dt
      local.y += vert * 90 * dt
      local.z += (this.boostT > 0 ? -260 : -this.throttle * 150) * dt
      const sp = local.length()
      if (sp > maxV) local.multiplyScalar(maxV / sp)
    }
    this.vel.copy(local.applyQuaternion(this.quat))
    this.pos.addScaledVector(this.vel, dt)
    this.collide(dt)
    // docking trigger
    if (this.docking?.granted && this.sys) {
      const b = this.sys.bayWorld(this.docking.stationId, this.docking.bay)
      if (b) {
        const mouth = b.pos.clone().addScaledVector(b.normal, 60)
        const d = mouth.distanceTo(this.pos)
        if (d < 160 && this.vel.length() < 140) {
          this.mode = 'docking'
          this.dockAnim = { from: this.pos.clone().sub(this.sys.stations.get(this.docking.stationId)!.abs), fromQ: this.quat.clone(), t: 0, stationId: this.docking.stationId, bay: this.docking.bay }
          this.msg('Docking computer engaged', 'info')
          audio.confirm()
        }
      }
    }
  }

  private collide(dt: number) {
    if (!this.sys) return
    // stations
    for (const st of this.sys.stations.values()) {
      const rel = this.pos.clone().sub(st.abs)
      if (rel.length() > st.model.boundRadius + 200) continue
      const local = this.sys.stationLocal(st.id, this.pos)!
      const d = stationSDF(st.model.colliders, local)
      if (d < SHIP_RADIUS) {
        const e = 2
        const grad = new THREE.Vector3(
          stationSDF(st.model.colliders, local.clone().add(new THREE.Vector3(e, 0, 0))) - stationSDF(st.model.colliders, local.clone().add(new THREE.Vector3(-e, 0, 0))),
          stationSDF(st.model.colliders, local.clone().add(new THREE.Vector3(0, e, 0))) - stationSDF(st.model.colliders, local.clone().add(new THREE.Vector3(0, -e, 0))),
          stationSDF(st.model.colliders, local.clone().add(new THREE.Vector3(0, 0, e))) - stationSDF(st.model.colliders, local.clone().add(new THREE.Vector3(0, 0, -e))),
        ).normalize()
        const n = grad.applyQuaternion(this.sys.stationWorldQuat(st.id))
        this.pos.addScaledVector(n, SHIP_RADIUS - d + 1)
        this.impact(n)
      }
    }
    // bodies
    for (const b of this.sys.sys.bodies) {
      const c = this.sys.bodyAbs.get(b.id)!
      const d = c.distanceTo(this.pos)
      if (d < b.radiusM + 1500) {
        const n = this.pos.clone().sub(c).normalize()
        this.pos.copy(c).addScaledVector(n, b.radiusM + 1500)
        this.impact(n, 3)
      }
    }
    // asteroids
    if (this.sys.asteroids.group.visible) {
      for (const r of this.sys.asteroids.rocks) {
        const d = r.p.distanceTo(this.pos)
        if (d < r.r + SHIP_RADIUS) {
          const n = this.pos.clone().sub(r.p).normalize()
          this.pos.copy(r.p).addScaledVector(n, r.r + SHIP_RADIUS + 1)
          this.impact(n)
        }
      }
    }
    void dt
  }

  private impact(n: THREE.Vector3, mult = 1) {
    const vn = this.vel.dot(n)
    if (vn >= 0) return
    const speed = -vn
    this.vel.addScaledVector(n, -vn * 1.4)
    this.vel.multiplyScalar(0.6)
    if (speed < 15 || this.time - this.lastImpact < 0.6) return
    this.lastImpact = this.time
    this.damage(speed * 0.0022 * mult)
    audio.impact()
  }

  damage(amount: number) {
    this.lastHit = this.time
    const absorbed = Math.min(this.shields, amount)
    this.shields -= absorbed
    const rest = amount - absorbed
    if (rest > 0) this.player.hull = Math.max(0, this.player.hull - rest)
    if (amount > 0.02) this.msg(rest > 0 ? `Hull damage! Integrity ${(this.player.hull * 100).toFixed(0)}%` : 'Shields absorbing impact', rest > 0 ? 'bad' : 'warn')
    if (this.player.hull <= 0 && this.mode !== 'destroyed') {
      this.mode = 'destroyed'
      this.fsd = null
      this.autopilot = false
      this.msg('Ship destroyed', 'bad')
    }
  }

  respawn() {
    const p = this.player
    p.cargo = {}
    p.missionCargo = {}
    p.missions = []
    p.credits = Math.max(0, p.credits - 2500)
    p.hull = 1
    p.fuel = SHIP.fuelMax
    this.shields = 1
    const target = p.lastStation ?? { systemId: HOME_ID, stationId: 'tanara-station' }
    p.dockedAt = target.stationId
    p.dockedBay = 0
    p.systemId = target.systemId
    const star = getStar(target.systemId) ?? HOME_STAR
    void loadSystem(star, p.commander).then((sys) => {
      this.enterSystem(sys, 'load')
      if (!this.sys?.stations.has(target.stationId)) this.spawnNearStation(sys.stations[0]?.id)
      this.msg('Insurance claim processed (−2,500 cr). New ship delivered.', 'warn')
    })
  }

  private flySupercruise(dt: number, pitch: number, yaw: number, roll: number) {
    if (!this.sys) return
    this.rotate(dt, pitch, yaw, roll, [0.6, 0.38, 1.2])
    // gravity-well speed cap
    let cap = SC_MAX
    let nearest = Infinity
    let nearestBody: { R: number; d: number; name: string } | null = null
    const sr = this.sys.star.radius
    const sd = this.pos.length() - sr
    cap = Math.min(cap, SC_MIN + Math.max(0, sd) * 0.35)
    for (const b of this.sys.sys.bodies) {
      const d = this.sys.bodyAbs.get(b.id)!.distanceTo(this.pos) - b.radiusM
      const well = SC_MIN + Math.max(0, d) * (0.3 + 0.5 * Math.min(1, 6e6 / b.radiusM))
      cap = Math.min(cap, well)
      if (d < nearest) {
        nearest = d
        nearestBody = { R: b.radiusM, d, name: b.name }
      }
    }
    // approach cap toward the current target ("seven second rule")
    const t = this.target ? this.navCache.find((n) => n.id === this.target) : undefined
    let dt2 = Infinity
    if (t) {
      const tp = this.navPos(t)
      dt2 = tp.distanceTo(this.pos)
      const surf = t.kind === 'planet' || t.kind === 'moon' ? dt2 - t.radius : dt2
      const fwd = new THREE.Vector3(0, 0, -1).applyQuaternion(this.quat)
      const facing = fwd.dot(tp.clone().sub(this.pos).normalize())
      if (facing > 0.6) cap = Math.min(cap, SC_MIN + Math.max(0, surf) / 5.5)
    }
    const targetSpeed = Math.max(SC_MIN, this.throttle * cap)
    if (targetSpeed > this.scSpeed) this.scSpeed = Math.min(targetSpeed, this.scSpeed * Math.exp(dt * 1.05) + 4000 * dt)
    else this.scSpeed = targetSpeed + (this.scSpeed - targetSpeed) * Math.exp(-dt / 0.9)
    const fwd = new THREE.Vector3(0, 0, -1).applyQuaternion(this.quat)
    this.pos.addScaledVector(fwd, this.scSpeed * dt)
    this.vel.copy(fwd).multiplyScalar(this.scSpeed)

    // auto-drop at station/poi/belt targets
    if (t && (t.kind === 'station' || t.kind === 'poi') && dt2 < 60_000) this.dropFromSupercruise(true)
    else if (t && t.kind === 'belt' && dt2 < 2e6) this.dropFromSupercruise(true)

    // star: fuel scoop and heat
    const scoopable = SCOOPABLE[this.sys.sys.star.cls]
    const ratio = this.pos.length() / sr
    let scooping = false
    if (ratio < 4 && scoopable && this.player.fuel < SHIP.fuelMax) {
      const rate = 0.9 * Math.pow(1.2 / Math.max(ratio, 1.2), 2)
      this.player.fuel = Math.min(SHIP.fuelMax, this.player.fuel + rate * dt)
      scooping = true
    }
    this.heat = clamp(0.2 + Math.max(0, 3 - ratio) * 0.35, 0, 1.2)
    if (this.heat > 0.95) this.damage(dt * 0.05 * (this.heat - 0.9) * 10)
    useHud.setState({ flags: { ...useHud.getState().flags, scoop: scooping } })
    // emergency drop when too close to a body
    if (nearestBody && nearestBody.d < Math.max(nearestBody.R * 0.04, 30_000)) {
      this.msg(`Emergency drop! Too close to ${nearestBody.name}`, 'bad')
      this.damage(0.12)
      this.dropFromSupercruise(true)
      const c = this.sys.sys.bodies.find((b) => b.name === nearestBody!.name)
      if (c) {
        const ctr = this.sys.bodyAbs.get(c.id)!
        const n = this.pos.clone().sub(ctr).normalize()
        this.pos.copy(ctr).addScaledVector(n, c.radiusM + Math.max(c.radiusM * 0.05, 40_000))
      }
    }
    if (ratio < 1.25) {
      this.msg('Emergency drop! Stellar corona breach', 'bad')
      this.damage(0.2)
      this.pos.setLength(sr * 2)
      this.dropFromSupercruise(true)
    }
  }

  private beginHyperspace(dest: StarDescriptor) {
    const cur = getStar(this.player.systemId) ?? HOME_STAR
    const d = distLy(cur, dest)
    this.player.fuel -= jumpFuel(d, jumpRange(this.player))
    this.player.stats.jumps++
    this.player.stats.lyTravelled += d
    this.player.dockedAt = null
    this.mode = 'hyperspace'
    this.autopilot = false
    this.hyper = { dest, t: 0, ready: null, log: [], exitT: 0, ai: useHud.getState().forge.ai && !cachedSystem(dest.id) }
    this.tunnel.setColors(dest.color)
    this.tunnel.group.visible = true
    this.msg(`Hyperspace jump · ${dest.name} · ${d.toFixed(2)} ly`, 'info')
    audio.whoosh()
    const h = this.hyper
    void loadSystem(dest, this.player.commander, (lines) => {
      h.log = lines
    }).then((sys) => {
      h.ready = sys
    })
  }

  private flyHyperspace(dt: number) {
    const h = this.hyper
    if (!h) return
    h.t += dt
    const revealDone = h.t > 0.8 + h.log.length * 0.55
    if (h.ready && h.t > 6.5 && (revealDone || h.t > 14)) {
      h.exitT += dt
      if (h.exitT > 0.9) {
        const sys = h.ready
        this.hyper = null
        this.tunnel.group.visible = false
        // advance route
        const idx = this.player.route.indexOf(sys.id)
        if (idx >= 0) this.player.route = this.player.route.slice(idx + 1)
        if (!this.player.route.length) this.hyperTarget = null
        if (this.player.tutorial === 1) this.player.tutorial = 2
        this.enterSystem(sys, 'jump')
        if (h.ai && sys.source !== 'ai') this.msg(`Stellar Forge unavailable${lastForgeError ? `: ${lastForgeError}` : ''} — charted by onboard survey instead`, 'warn')
        audio.whoosh()
        this.save()
      }
    }
  }

  private flyDocking(dt: number) {
    const a = this.dockAnim
    if (!a || !this.sys) return
    a.t += dt / 6
    const b = this.sys.bayWorld(a.stationId, a.bay)
    const st = this.sys.stations.get(a.stationId)
    if (!b || !st) return
    const end = b.pos.clone().addScaledVector(b.normal, 25)
    const k = a.t * a.t * (3 - 2 * a.t)
    const start = st.abs.clone().add(a.from)
    this.pos.copy(start.lerp(end, Math.min(1, k)))
    const m = new THREE.Matrix4().lookAt(new THREE.Vector3(), b.normal.clone(), new THREE.Vector3(0, 1, 0))
    const endQ = new THREE.Quaternion().setFromRotationMatrix(m)
    // face outward once docked (so the view looks out to space)
    this.quat.copy(a.fromQ).slerp(endQ, Math.min(1, k))
    if (a.t >= 1) {
      this.dockAnim = null
      this.dockAt(a.stationId, a.bay, false)
    }
  }

  private flyLaunch(dt: number) {
    if (!this.sys || !this.player.dockedAt) {
      this.mode = 'normal'
      return
    }
    this.launchT += dt
    const b = this.sys.bayWorld(this.player.dockedAt, this.player.dockedBay)
    if (b) {
      this.lookAlong(b.normal)
      this.pos.copy(b.pos).addScaledVector(b.normal, 25 + this.launchT * this.launchT * 40)
      this.vel.copy(b.normal).multiplyScalar(this.launchT * 80)
    }
    if (this.launchT > 3.5) {
      this.mode = 'normal'
      this.player.dockedAt = null
      this.throttle = 0.3
      this.msg('Launch complete. You have control.', 'info')
    }
  }

  // ------------------------------------------------------------------ autopilot
  private autopilotControls(): { pitch: number; yaw: number; throttle: number; engageSC?: boolean; dropSC?: boolean } | null {
    if (!this.sys) return null
    if (!this.target || this.fsd?.kind === 'jump') {
      // no local target (or jump charging): align with the next hyperspace jump and engage
      const nj = this.nextJump()
      if (!nj) return null
      const local = this.starDirection(nj).applyQuaternion(this.quat.clone().invert())
      const yawErr = Math.atan2(-local.x, -local.z)
      const pitchErr = Math.atan2(local.y, Math.hypot(local.x, local.z))
      const locked = this.mode === 'normal' && this.massLocked()
      if (Math.hypot(yawErr, pitchErr) < 0.08 && !this.fsd && !locked) this.engageJump()
      return { pitch: clamp(pitchErr * 3, -1, 1), yaw: clamp(yawErr * 3, -1, 1), throttle: locked ? 1 : this.mode === 'supercruise' ? 0 : 0.5 }
    }
    if (!this.target) return null
    const t = this.navCache.find((n) => n.id === this.target)
    if (!t) return null
    const tp = this.navPos(t)
    let aim = tp.clone()
    let throttle = 0.75
    let engageSC = false
    let dropSC = false
    const dist = tp.distanceTo(this.pos)
    if (this.mode === 'supercruise') {
      if ((t.kind === 'planet' || t.kind === 'moon') && dist - t.radius < t.radius * 2.5) dropSC = true
      if (t.kind === 'star' && dist < t.radius * 6) dropSC = true
      if (t.kind === 'planet' || t.kind === 'moon') {
        // aim at the limb so we glide past rather than into it
        const toT = tp.clone().sub(this.pos)
        const side = new THREE.Vector3().crossVectors(toT, new THREE.Vector3(0, 1, 0)).normalize()
        aim = tp.clone().addScaledVector(side, t.radius * 1.6)
      }
    } else {
      if (t.kind === 'station') {
        if (dist < DOCK_RANGE && !this.docking) this.requestDocking()
        if (this.docking?.stationId === t.id) {
          const b = this.sys.bayWorld(t.id, this.docking.bay)!
          const p1 = b.pos.clone().addScaledVector(b.normal, 1800)
          const p2 = b.pos.clone().addScaledVector(b.normal, 300)
          const p3 = b.pos.clone().addScaledVector(b.normal, 55)
          if (this.apStage === 0 && this.pos.distanceTo(p1) < 350) this.apStage = 1
          if (this.apStage === 1 && this.pos.distanceTo(p2) < 120) this.apStage = 2
          aim = this.apStage === 0 ? p1 : this.apStage === 1 ? p2 : p3
          if (this.apStage === 0) {
            // steer around the station if the straight line to the approach point crosses the hull
            const st = this.sys.stations.get(t.id)!
            const seg = p1.clone().sub(this.pos)
            let minSdf = Infinity
            for (let i = 1; i <= 24; i++) {
              const q = this.pos.clone().addScaledVector(seg, i / 24)
              minSdf = Math.min(minSdf, stationSDF(st.model.colliders, this.sys.stationLocal(t.id, q)!))
            }
            if (minSdf < 150) {
              const rel = this.pos.clone().sub(st.abs)
              const perp = rel.clone().addScaledVector(b.normal, -rel.dot(b.normal))
              if (perp.lengthSq() < 1) perp.crossVectors(b.normal, Math.abs(b.normal.y) < 0.9 ? new THREE.Vector3(0, 1, 0) : new THREE.Vector3(1, 0, 0))
              aim = st.abs.clone().addScaledVector(perp.normalize(), st.model.boundRadius + 600)
            }
          }
          const d = aim.distanceTo(this.pos)
          throttle = clamp(d / 1200, 0.12, this.apStage === 0 ? 1 : 0.4)
        } else {
          throttle = clamp(dist / 20000, 0.2, 1)
        }
        if (dist > 40_000 && !this.massLocked()) engageSC = true
      } else {
        if (dist > 30_000) engageSC = true
        throttle = clamp(dist / 8000, 0.0, 1)
        if ((t.kind === 'planet' || t.kind === 'moon') && dist - t.radius < t.radius * 3) {
          throttle = 0
          engageSC = false
        }
      }
    }
    const dir = aim.sub(this.pos).normalize()
    const local = dir.applyQuaternion(this.quat.clone().invert())
    const yawErr = Math.atan2(-local.x, -local.z)
    const pitchErr = Math.atan2(local.y, Math.hypot(local.x, local.z))
    const misalign = Math.hypot(yawErr, pitchErr)
    if (misalign > 0.5 && this.mode === 'normal') throttle = Math.min(throttle, 0.55)
    if (misalign > 0.3) engageSC = false
    return { pitch: clamp(pitchErr * 3, -1, 1), yaw: clamp(yawErr * 3, -1, 1), throttle, engageSC, dropSC }
  }

  // ------------------------------------------------------------------ rendering
  private resize() {
    const w = this.canvas.clientWidth
    const h = this.canvas.clientHeight
    const key = `${w}x${h}`
    if (key === this.lastSize || !w || !h) return
    this.lastSize = key
    this.renderer.setSize(w, h, false)
    this.composer.setSize(w, h)
    this.camera.aspect = w / h
    // keep a sensible horizontal FOV on tall screens
    this.camera.fov = w / h < 1.3 ? 75 : 62
    this.camera.updateProjectionMatrix()
    this.bloom.resolution.set(w / 2, h / 2)
    const rects = this.cockpit.screenRects(this.camera, w, h)
    useHud.setState({ rects })
  }

  private render(dt: number) {
    this.resize()
    this.camera.quaternion.copy(this.quat)
    // a little camera shake under boost / impacts
    const shake = (this.boostT > 0 ? 0.0015 : 0) + Math.max(0, 0.6 - (this.time - this.lastHit)) * 0.01 + (this.mode === 'hyperspace' ? 0.002 : 0)
    if (shake > 0) this.camera.quaternion.multiply(new THREE.Quaternion().setFromEuler(new THREE.Euler((Math.random() - 0.5) * shake, (Math.random() - 0.5) * shake, 0)))
    this.camera.updateMatrixWorld()
    const inHyper = this.mode === 'hyperspace'
    if (this.sys) {
      this.sys.root.visible = !inHyper
      this.sys.update(this.pos, dt, this.time, this.mode === 'normal' || this.mode === 'docking' || this.mode === 'launching' || this.mode === 'docked')
    }
    const move = this.pos.clone().sub(this.lastRenderPos)
    this.lastRenderPos.copy(this.pos)
    if (inHyper || this.mode === 'docked' || move.length() > 5e5 && this.mode !== 'supercruise') this.dust.lines.visible = false
    else this.dust.update(move, this.mode === 'supercruise' ? this.vel : this.vel, this.mode === 'supercruise')
    this.skyPass.enabled = !inHyper
    this.mainPass.clear = inHyper
    if (inHyper && this.hyper) this.tunnel.update(this.time, Math.min(1, this.hyper.t / 1.5), this.hyper.exitT)
    this.cockpit.setAlert(this.player.hull < 0.3 || this.heat > 0.9, this.time)
    this.sky.sync(this.camera)
    this.composer.render()
    this.drawRadar()
    this.drawFx()
  }

  private projectRel(rel: THREE.Vector3) {
    const v = rel.clone().applyQuaternion(this.camera.quaternion.clone().invert())
    const behind = v.z > 0
    v.applyMatrix4(this.camera.projectionMatrix)
    return { x: v.x, y: v.y, behind }
  }

  private drawFx() {
    const ctx = this.fxCtx
    if (!ctx || !this.sys) return
    const w = ctx.canvas.clientWidth
    const h = ctx.canvas.clientHeight
    if (ctx.canvas.width !== w || ctx.canvas.height !== h) {
      ctx.canvas.width = w
      ctx.canvas.height = h
    }
    ctx.clearRect(0, 0, w, h)
    if (this.mode === 'hyperspace') return
    // lens flare from the star
    const rel = new THREE.Vector3(-this.pos.x, -this.pos.y, -this.pos.z)
    const p = this.projectRel(rel)
    if (p.behind || Math.abs(p.x) > 1.3 || Math.abs(p.y) > 1.3) return
    // occlusion by planets
    const dir = rel.clone().normalize()
    const dist = rel.length()
    for (const b of this.sys.sys.bodies) {
      const c = this.sys.bodyAbs.get(b.id)!.clone().sub(this.pos)
      const tca = c.dot(dir)
      if (tca < 0 || tca > dist) continue
      const d2 = c.lengthSq() - tca * tca
      if (d2 < b.radiusM * b.radiusM) return
    }
    const sx = (p.x * 0.5 + 0.5) * w
    const sy = (-p.y * 0.5 + 0.5) * h
    const cx = w / 2
    const cy = h / 2
    const col = new THREE.Color(this.sys.sys.star.color)
    const rgb = `${Math.round(col.r * 255)},${Math.round(col.g * 255)},${Math.round(col.b * 255)}`
    const edge = 1 - Math.min(1, Math.hypot(p.x, p.y) / 1.4)
    ctx.globalCompositeOperation = 'lighter'
    const streak = ctx.createLinearGradient(sx - w * 0.3, sy, sx + w * 0.3, sy)
    streak.addColorStop(0, 'rgba(0,0,0,0)')
    streak.addColorStop(0.5, `rgba(${rgb},${0.35 * edge})`)
    streak.addColorStop(1, 'rgba(0,0,0,0)')
    ctx.fillStyle = streak
    ctx.fillRect(sx - w * 0.3, sy - 1.5, w * 0.6, 3)
    const ghosts = [0.35, 0.6, 1.25, 1.6, 2.1]
    ghosts.forEach((g, i) => {
      const gx = sx + (cx - sx) * g
      const gy = sy + (cy - sy) * g
      const r = [26, 12, 40, 18, 60][i] * (h / 900)
      const grd = ctx.createRadialGradient(gx, gy, 0, gx, gy, r)
      grd.addColorStop(0, `rgba(${rgb},${0.12 * edge})`)
      grd.addColorStop(0.7, `rgba(${i % 2 ? '120,180,255' : rgb},${0.06 * edge})`)
      grd.addColorStop(1, 'rgba(0,0,0,0)')
      ctx.fillStyle = grd
      ctx.beginPath()
      ctx.arc(gx, gy, r, 0, Math.PI * 2)
      ctx.fill()
    })
    ctx.globalCompositeOperation = 'source-over'
  }

  private drawRadar() {
    const ctx = this.radarCtx
    if (!ctx || !this.sys) return
    const cv = ctx.canvas
    const W = cv.clientWidth
    const H = cv.clientHeight
    if (cv.width !== W * 2 || cv.height !== H * 2) {
      cv.width = W * 2
      cv.height = H * 2
    }
    ctx.setTransform(2, 0, 0, 2, 0, 0)
    ctx.clearRect(0, 0, W, H)
    const cx = W / 2
    const cy = H * 0.52
    const rx = W * 0.44
    const ry = H * 0.36
    ctx.strokeStyle = 'rgba(120,190,255,0.35)'
    ctx.lineWidth = 1
    for (const k of [1, 0.66, 0.33]) {
      ctx.beginPath()
      ctx.ellipse(cx, cy, rx * k, ry * k, 0, 0, Math.PI * 2)
      ctx.stroke()
    }
    ctx.beginPath()
    ctx.moveTo(cx - rx, cy)
    ctx.lineTo(cx + rx, cy)
    ctx.moveTo(cx, cy - ry)
    ctx.lineTo(cx, cy + ry)
    ctx.strokeStyle = 'rgba(120,190,255,0.18)'
    ctx.stroke()
    // sweep
    const a = (this.time * 1.6) % (Math.PI * 2)
    const grd = ctx.createConicGradient?.(a, cx, cy)
    if (grd) {
      grd.addColorStop(0, 'rgba(90,170,255,0.18)')
      grd.addColorStop(0.08, 'rgba(90,170,255,0)')
      grd.addColorStop(1, 'rgba(90,170,255,0)')
      ctx.fillStyle = grd
      ctx.beginPath()
      ctx.ellipse(cx, cy, rx, ry, 0, 0, Math.PI * 2)
      ctx.fill()
    }
    const range = this.mode === 'supercruise' ? 2e8 : 8000
    const inv = this.quat.clone().invert()
    const plot = (abs: THREE.Vector3, color: string, size: number, diamond = false) => {
      const rel = abs.clone().sub(this.pos)
      const d = rel.length()
      if (d > range * 1.5) return
      const local = rel.applyQuaternion(inv)
      const k = Math.log1p((d / range) * 9) / Math.log1p(9)
      const n = d > 0 ? Math.min(1, k) / d : 0
      const x = cx + local.x * n * rx
      const y = cy + local.z * n * ry
      const stalk = -local.y * n * ry * 0.8
      ctx.strokeStyle = color
      ctx.beginPath()
      ctx.moveTo(x, y)
      ctx.lineTo(x, y + stalk)
      ctx.stroke()
      ctx.fillStyle = color
      if (diamond) {
        ctx.beginPath()
        ctx.moveTo(x, y + stalk - size)
        ctx.lineTo(x + size, y + stalk)
        ctx.lineTo(x, y + stalk + size)
        ctx.lineTo(x - size, y + stalk)
        ctx.closePath()
        ctx.fill()
      } else ctx.fillRect(x - size / 2, y + stalk - size / 2, size, size)
    }
    for (const st of this.sys.stations.values()) {
      plot(st.abs, st.id === this.target ? '#ffb347' : '#8fd0ff', 5, true)
      if (this.mode !== 'supercruise' && st.abs.distanceTo(this.pos) < 30000) {
        for (const c of st.traffic.contacts()) plot(st.abs.clone().add(c), '#d8e6f2', 3)
      }
    }
    for (const p of this.sys.pois.values()) plot(p.abs, '#c890ff', 4, true)
    if (this.mode === 'supercruise') for (const [id, b] of this.sys.bodyAbs) plot(b, id === this.target ? '#ffb347' : '#7aa8cc', 4)
    // own ship
    ctx.fillStyle = '#e8f4ff'
    ctx.beginPath()
    ctx.moveTo(cx, cy - 6)
    ctx.lineTo(cx + 4, cy + 4)
    ctx.lineTo(cx - 4, cy + 4)
    ctx.closePath()
    ctx.fill()
  }

  // ------------------------------------------------------------------ HUD bridge
  private objective(): Objective {
    const p = this.player
    const st = this.sys?.stations.get('tanara-station')
    switch (p.tutorial) {
      case 0: {
        const d = st ? st.abs.distanceTo(this.pos) : Infinity
        return {
          title: 'Dock at Tanara Station',
          steps: [
            { text: 'Approach landing bay (W throttle · mouse/arrows steer)', done: d < DOCK_RANGE || !!this.docking || this.mode === 'docked' },
            { text: 'Request clearance (L)', done: !!this.docking || this.mode === 'docking' || this.mode === 'docked' },
            { text: 'Fly into the lit bay slowly — or press P for autopilot', done: this.mode === 'docked' },
          ],
        }
      }
      case 1:
        return {
          title: 'Make your first jump',
          steps: [
            { text: 'Open the galaxy map (M)', done: this.flags.galaxyOpened || this.player.route.length > 0 },
            { text: 'Plot a route to a nearby star', done: this.player.route.length > 0 },
            { text: 'Launch, align and engage FSD (H)', done: false },
          ],
        }
      case 2: {
        const scannedHere = p.scanned.some((s) => s.startsWith(`${p.systemId}:`)) || p.data.some((d) => d.kind === 'surface')
        return {
          title: 'Chart the unknown',
          steps: [
            { text: 'Arrive in a new system', done: true },
            { text: 'Fly close to a planet to scan it', done: scannedHere },
            { text: 'Sell exploration data at a station', done: false },
          ],
        }
      }
      default: {
        const m = p.missions[0]
        if (m)
          return {
            title: m.title,
            steps: [
              { text: `Travel to ${m.destSystemName}${m.distanceLy ? ` (${m.distanceLy} ly)` : ''}`, done: p.systemId === m.destSystem },
              { text: `Dock at ${m.destStationName ?? 'any station'}`, done: false },
              { text: `Reward ${m.reward.toLocaleString()} cr`, done: false },
            ],
          }
        return {
          title: 'Explore · Trade · Build · Belong',
          steps: [
            { text: 'Take contracts from station mission boards', done: false },
            { text: 'Trade goods between economies', done: false },
            { text: 'Jump into uncharted space — the Stellar Forge awaits', done: false },
          ],
        }
      }
    }
  }

  pushHudStatic() {
    const p = this.player
    const route = p.route.map((id) => {
      const s = getStar(id)
      const cur = getStar(p.systemId) ?? HOME_STAR
      return { id, name: s?.name ?? id, dist: s ? distLy(cur, s) : 0 }
    })
    useHud.setState({
      system: this.sys?.sys ?? null,
      credits: p.credits,
      cargo: cargoTotal(p),
      cargoCap: SHIP.cargoCap,
      data: [...p.data],
      missions: [...p.missions],
      route,
      jumpRange: jumpRange(p),
      day: this.day(),
    })
  }

  private pushHud() {
    if (!this.sys) return
    const p = this.player
    const speed = this.mode === 'supercruise' ? this.scSpeed : this.vel.length()
    const sp = fmtSpeed(speed)
    const fwd = new THREE.Vector3(0, 0, -1).applyQuaternion(this.quat)
    const heading = ((Math.atan2(fwd.x, -fwd.z) * 180) / Math.PI + 360) % 360
    const w = this.canvas.clientWidth
    const h = this.canvas.clientHeight
    const toScreen = (abs: THREE.Vector3) => {
      const pr = this.projectRel(abs.clone().sub(this.pos))
      const onScreen = !pr.behind && Math.abs(pr.x) < 0.95 && Math.abs(pr.y) < 0.92
      let angle = Math.atan2(pr.y, pr.x)
      if (pr.behind) angle = Math.atan2(-pr.y, -pr.x)
      return { sx: (pr.x * 0.5 + 0.5) * w, sy: (-pr.y * 0.5 + 0.5) * h, onScreen, angle }
    }
    let target: ReturnType<typeof useHud.getState>['target'] = null
    if (this.target) {
      const n = this.navCache.find((x) => x.id === this.target)
      if (n) {
        const abs = this.navPos(n)
        const d = abs.distanceTo(this.pos) - (n.kind === 'planet' || n.kind === 'moon' || n.kind === 'star' ? n.radius : 0)
        target = { id: n.id, name: n.name, kind: n.kind, dist: fmtDist(Math.max(0, d)), sub: n.subtitle, ...toScreen(abs) }
      }
    }
    let hyperTarget: ReturnType<typeof useHud.getState>['hyperTarget'] = null
    const nj = this.nextJump()
    if (nj) {
      const cur = getStar(p.systemId) ?? HOME_STAR
      const d = distLy(cur, nj)
      const dir = this.starDirection(nj)
      const scr = toScreen(this.pos.clone().addScaledVector(dir, 1e15))
      const range = jumpRange(p)
      hyperTarget = { id: nj.id, name: nj.name, dist: d, fuel: jumpFuel(d, range), inRange: d <= range, ...scr, aligned: fwd.angleTo(dir) < 0.2 }
    }
    // nearest body + location info
    let near: { name: string; type: string; orb: number } | null = null
    let best = Infinity
    const planets = this.sys.sys.bodies.filter((b) => b.kind === 'planet').sort((a, b) => a.orbitM - b.orbitM)
    for (const b of this.sys.sys.bodies) {
      const d = (this.sys.bodyAbs.get(b.id)!.distanceTo(this.pos) - b.radiusM) / b.radiusM
      if (d < best) {
        best = d
        const pl = b.kind === 'planet' ? b : this.sys.sys.bodies.find((x) => x.id === b.parent)
        near = { name: b.name, type: b.type.replace('_', ' '), orb: pl ? planets.indexOf(pl) + 1 : 0 }
      }
    }
    const systemSub = near && best < 60 ? [near.name, `ORB ${near.orb}`, near.type] : [CLASS_LABEL[this.sys.sys.star.cls], this.sys.sys.profile.faction]
    const nav = this.navCache
      .map((n) => ({ id: n.id, name: n.name, kind: n.kind, dist: this.distTo(n), sub: n.subtitle }))
      .sort((a, b) => a.dist - b.dist)
    const markers = this.navCache
      .filter((n) => n.id !== this.target && (n.kind === 'station' || n.kind === 'planet' || n.kind === 'poi' || n.kind === 'star'))
      .map((n) => {
        const abs = this.navPos(n)
        const s = toScreen(abs)
        const dm = abs.distanceTo(this.pos)
        return { id: n.id, name: n.name, kind: n.kind, sx: s.sx, sy: s.sy, dist: fmtDist(dm), dm, on: s.onScreen }
      })
      .filter((m) => m.on)
      .sort((a, b) => a.dm - b.dm)
    const kept: typeof markers = []
    for (const m of markers) {
      if (kept.length >= 10) break
      if (target && Math.hypot(target.sx - m.sx, target.sy - m.sy) < 60) continue
      if (kept.some((k) => Math.abs(k.sx - m.sx) < 150 && Math.abs(k.sy - m.sy) < 16)) continue
      kept.push(m)
    }
    let docking: ReturnType<typeof useHud.getState>['docking'] = null
    if (this.docking) {
      const st = this.sys.sys.stations.find((s) => s.id === this.docking!.stationId)
      const b = this.sys.bayWorld(this.docking.stationId, this.docking.bay)
      docking = { station: st?.name ?? '', bay: this.sys.stations.get(this.docking.stationId)?.model.bays[this.docking.bay].label ?? '', granted: this.docking.granted, dist: b ? b.pos.distanceTo(this.pos) : 0 }
    }
    const mass = (SHIP.hullMass + cargoTotal(p) + p.fuel) / (SHIP.hullMass + SHIP.cargoCap + SHIP.fuelMax)
    let alert: string | null = null
    if (this.fsd?.kind === 'jump' && this.fsd.t >= this.fsd.dur) alert = 'ALIGN WITH DESTINATION'
    else if (this.heat > 0.9) alert = 'HEAT WARNING'
    else if (p.hull < 0.25) alert = 'HULL CRITICAL'
    else if (this.mode === 'normal' && this.massLocked() && this.fsd === null && this.throttle === 0 && false) alert = 'MASS LOCKED'
    const hyper = this.hyper
      ? { phase: (this.hyper.exitT > 0 ? 'exit' : 'tunnel') as 'exit' | 'tunnel', dest: this.hyper.dest.name, destClass: CLASS_LABEL[this.hyper.dest.cls], log: this.hyper.log, elapsed: this.hyper.t, ai: this.hyper.ai }
      : null
    useHud.setState({
      mode: this.mode,
      speed,
      speedLabel: `${sp.value}|${sp.unit}`,
      throttle: this.throttle,
      boost: this.boostT > 0,
      boostCharge: this.boostCharge,
      fuel: p.fuel,
      fuelMax: SHIP.fuelMax,
      hull: p.hull,
      shields: this.shields,
      heat: this.heat,
      mass,
      credits: p.credits,
      systemName: this.sys.sys.name,
      systemSub,
      target,
      hyperTarget,
      fsd: this.fsd ? { kind: this.fsd.kind, progress: Math.min(1, this.fsd.t / this.fsd.dur), label: this.fsd.kind === 'sc' ? 'SUPERCRUISE' : 'HYPERSPACE' } : null,
      flags: { autopilot: this.autopilot, fa: this.fa, navLock: !!this.target, gear: this.gear, massLock: this.mode === 'normal' && this.massLocked(), scoop: useHud.getState().flags.scoop && this.mode === 'supercruise' },
      docking,
      objective: this.objective(),
      heading,
      pitch: (Math.asin(clamp(fwd.y, -1, 1)) * 180) / Math.PI,
      nav,
      hyper,
      markers: kept,
      stick: { x: this.input.stick.x, y: this.input.stick.y, locked: this.input.mouseLocked },
      alert,
      cargo: cargoTotal(p),
      data: p.data,
      missions: p.missions,
    })
  }

  save() {
    const p = this.player
    if (this.mode !== 'hyperspace' && this.sys) {
      p.pos = [this.pos.x, this.pos.y, this.pos.z]
      p.quat = [this.quat.x, this.quat.y, this.quat.z, this.quat.w]
      p.systemId = this.sys.sys.id
      if (this.mode !== 'docked' && this.mode !== 'launching') p.dockedAt = null
    }
    writeSave(p)
  }
}

export function createPlayer(name: string) {
  return newPlayer(name)
}

export { COMMODITIES }
