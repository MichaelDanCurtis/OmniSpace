import { useEffect, useRef } from 'react'
import { audio } from '../game/audio'
import type { Game } from '../game/engine'
import { fmtDist, useHud, type HudState, type Rect } from '../game/hud'
import { Logo } from './Logo'
import { ForgePanel } from './ForgePanel'

const KIND_ICON: Record<string, string> = { star: '✶', planet: '●', moon: '◦', station: '◆', poi: '✧', belt: '⁘' }

export function Hud({ game }: { game: Game }) {
  const s = useHud()
  return (
    <div className="hud">
      <div className="brand">
        <div className="brand-logo">
          <Logo />
          <div className="brand-name">
            OMNI<small>SPACE</small>
          </div>
        </div>
        <div className="brand-tags">
          EXPLORE
          <br />
          TRADE
          <br />
          BUILD
          <br />
          BELONG
        </div>
      </div>
      <div className="location">
        <h2>{s.systemName} SYSTEM</h2>
        {s.systemSub.map((l, i) => (
          <div key={i}>{l}</div>
        ))}
      </div>
      {s.mode !== 'hyperspace' && <Objective s={s} />}
      <div className="side-decor">
        DISCOVERY
        <br />
        DRIVES
        <br />
        HUMANITY
        <br />
        FORWARD
      </div>
      <Markers s={s} />
      <Reticle stick={s.stick} />
      {s.mode !== 'hyperspace' && <Messages s={s} />}
      <ModeBar s={s} />
      <Dashboard s={s} game={game} />
    </div>
  )
}

function Objective({ s }: { s: HudState }) {
  if (!s.objective.title) return null
  return (
    <div className="objective">
      <div className="label">CURRENT OBJECTIVE</div>
      <div className="title">
        <span className="dia" />
        {s.objective.title}
      </div>
      <ul>
        {s.objective.steps.map((st, i) => (
          <li key={i} className={st.done ? 'done' : ''}>
            <span className="dot" />
            {st.text}
          </li>
        ))}
      </ul>
    </div>
  )
}

function Reticle({ stick }: { stick: HudState['stick'] }) {
  return (
    <svg className="reticle" width="420" height="300" viewBox="-210 -150 420 300">
      {stick.locked && <circle cx={stick.x * 60} cy={stick.y * 60} r="3.5" fill="#ffb347" opacity="0.9" />}
      {stick.locked && <circle r="60" fill="none" stroke="rgba(255,179,71,0.18)" strokeDasharray="2 6" />}
      <circle r="20" fill="none" stroke="rgba(230,240,250,0.85)" strokeWidth="1.5" />
      <line x1="-36" y1="0" x2="-26" y2="0" stroke="rgba(230,240,250,0.8)" strokeWidth="1.5" />
      <line x1="26" y1="0" x2="36" y2="0" stroke="rgba(230,240,250,0.8)" strokeWidth="1.5" />
      <line x1="0" y1="-36" x2="0" y2="-26" stroke="rgba(230,240,250,0.8)" strokeWidth="1.5" />
      <line x1="0" y1="26" x2="0" y2="36" stroke="rgba(230,240,250,0.8)" strokeWidth="1.5" />
      <path d="M -120 -130 A 190 190 0 0 0 -120 130" fill="none" stroke="rgba(230,240,250,0.55)" strokeWidth="1.5" />
      <path d="M 120 -130 A 190 190 0 0 1 120 130" fill="none" stroke="rgba(230,240,250,0.55)" strokeWidth="1.5" />
      <line x1="-90" y1="0" x2="-70" y2="0" stroke="rgba(230,240,250,0.6)" />
      <line x1="70" y1="0" x2="90" y2="0" stroke="rgba(230,240,250,0.6)" />
      <line x1="0" y1="-80" x2="0" y2="-55" stroke="rgba(230,240,250,0.35)" strokeDasharray="3 4" />
    </svg>
  )
}

function edgePos(angle: number) {
  const w = window.innerWidth
  const h = window.innerHeight
  const cx = w / 2
  const cy = h * 0.46
  const rx = w * 0.42
  const ry = h * 0.36
  return { x: cx + Math.cos(angle) * rx, y: cy - Math.sin(angle) * ry, rot: -angle * (180 / Math.PI) + 90 }
}

function Markers({ s }: { s: HudState }) {
  const t = s.target
  const ht = s.hyperTarget
  return (
    <>
      {s.markers.map((m) => (
        <div key={m.id} className="navmark" style={{ left: m.sx, top: m.sy }}>
          <i />
          {m.name} · {m.dist}
        </div>
      ))}
      {t && t.onScreen && (
        <div className="marker" style={{ left: t.sx, top: t.sy }}>
          <div className="dia" />
          <div className="tlabel">
            <b>{t.dist}</b>
            <span>{t.name}</span>
          </div>
        </div>
      )}
      {t && !t.onScreen && (() => {
        const p = edgePos(t.angle)
        return <div className="offscreen" style={{ left: p.x - 9, top: p.y - 8, transform: `rotate(${p.rot}deg)` }} />
      })()}
      {ht && ht.onScreen && (s.mode === 'normal' || s.mode === 'supercruise') && (
        <div className={`marker hyper ${ht.aligned ? 'aligned' : ''}`} style={{ left: ht.sx, top: ht.sy }}>
          <div className="dia" />
          <div className="tlabel">
            <b>{ht.name}</b>
            <span>
              {ht.dist.toFixed(2)} LY · JUMP {ht.aligned ? 'ALIGNED' : 'TARGET'}
            </span>
          </div>
        </div>
      )}
      {ht && !ht.onScreen && (s.mode === 'normal' || s.mode === 'supercruise') && (() => {
        const p = edgePos(ht.angle)
        return <div className="offscreen hyper" style={{ left: p.x - 9, top: p.y - 8, transform: `rotate(${p.rot}deg)` }} />
      })()}
    </>
  )
}

function Messages({ s }: { s: HudState }) {
  const now = useRef(0)
  const latest = s.messages.slice(-4)
  now.current = s.messages.length ? Math.max(...latest.map((m) => m.t)) : 0
  const gameTime = (window as unknown as { omni?: { time: number } }).omni?.time ?? 0
  return (
    <div className="messages">
      {latest
        .filter((m) => gameTime - m.t < 8)
        .map((m) => (
          <div key={m.id} className={`msg ${m.tone}`}>
            {m.text}
          </div>
        ))}
    </div>
  )
}

function ModeBar({ s }: { s: HudState }) {
  const modeLabel: Record<string, string> = {
    normal: '',
    supercruise: 'SUPERCRUISE',
    docking: 'DOCKING COMPUTER ENGAGED',
    launching: 'LAUNCHING',
    destroyed: '',
    hyperspace: '',
    docked: '',
  }
  return (
    <div className="modebar">
      {modeLabel[s.mode] && <div className={`mode ${s.mode === 'supercruise' ? 'sc' : ''}`}>{modeLabel[s.mode]}</div>}
      {s.flags.massLock && <div className="mode" style={{ color: 'var(--amber)', fontSize: 10 }}>MASS LOCKED</div>}
      {s.flags.scoop && <div className="mode" style={{ color: 'var(--amber)' }}>FUEL SCOOPING</div>}
      {s.fsd && (
        <>
          <div className="mode">
            {s.fsd.progress < 1 ? `FSD CHARGING · ${s.fsd.label}` : s.fsd.kind === 'jump' ? 'FSD CHARGED' : ''}
          </div>
          <div className="fsdbar">
            <div style={{ width: `${s.fsd.progress * 100}%` }} />
          </div>
        </>
      )}
      {s.alert && <div className="alert">{s.alert}</div>}
      {s.docking && s.mode === 'normal' && (
        <div className="dockinfo">
          {s.docking.station} · BAY {s.docking.bay} · {fmtDist(s.docking.dist)}
        </div>
      )}
    </div>
  )
}

function place(r?: Rect): React.CSSProperties {
  if (!r) return { display: 'none' }
  return { left: r.x, top: r.y, width: r.w, height: r.h }
}

function Dashboard({ s, game }: { s: HudState; game: Game }) {
  const radar = useRef<HTMLCanvasElement>(null)
  useEffect(() => {
    game.attachRadar(s.tab === 'NAV' ? radar.current : null)
    return () => game.attachRadar(null)
  }, [game, s.tab])
  const [val, unit] = s.speedLabel.split('|')
  const maxSpeed = s.mode === 'supercruise' ? 1 : 560
  const speedFrac = s.mode === 'supercruise' ? Math.min(1, Math.log10(Math.max(1, s.speed / 30000)) / 7.3) : Math.min(1, s.speed / maxSpeed)
  const left = s.rects.left
  const dialSize = left ? Math.min(left.h * 0.92, left.w * 0.42) : 100
  const right = s.rects.right
  const ringSize = right ? Math.min(right.h * 0.92, right.w * 0.36) : 90
  return (
    <>
      <div className="screen" style={place(s.rects.switches)}>
        <div className="switches">
          <Switch on={s.flags.autopilot} label="AUTOPILOT" />
          <Switch on={s.flags.fa} label="STABILIZED" />
          <Switch on={s.flags.navLock} label="NAV LOCK" />
          <Switch on={s.flags.gear} label="LANDING GEAR" />
        </div>
      </div>
      <div className="screen" style={place(left)}>
        <div className="inner" style={{ justifyContent: 'space-between', padding: '0 4%' }}>
          <div className="bars">
            <Bar label="THR" v={s.throttle} />
            <Bar label="ENG" v={s.boostCharge} />
            <Bar label="SYS" v={s.shields} />
            <Bar label="WEP" v={1} />
          </div>
          <div className="dial" style={{ width: dialSize, height: dialSize }}>
            <svg width={dialSize} height={dialSize} viewBox="-50 -50 100 100">
              <circle r="44" fill="none" stroke="rgba(143,208,255,0.15)" strokeWidth="5" />
              <circle
                r="44"
                fill="none"
                stroke={s.boost ? '#ffd28a' : '#8fd0ff'}
                strokeWidth="5"
                strokeDasharray={`${speedFrac * 276} 276`}
                transform="rotate(-90)"
                style={{ filter: 'drop-shadow(0 0 3px #8fd0ff)' }}
              />
              <circle r="37" fill="none" stroke="rgba(143,208,255,0.35)" strokeWidth="0.6" />
            </svg>
            <div className="val">
              <b>{val}</b>
              <span>{unit}</span>
            </div>
          </div>
        </div>
      </div>
      <div className="screen center-screen interactive" style={{ ...place(s.rects.center), display: s.rects.center ? 'flex' : 'none' }}>
        <Compass heading={s.heading} />
        <div className="tabbody">
          {s.tab === 'NAV' && <canvas ref={radar} className="radar" />}
          {s.tab === 'MAP' && <NavList s={s} game={game} />}
          {s.tab === 'COM' && <ComLog s={s} />}
          {s.tab === 'SHIP' && <ShipInfo s={s} />}
          {s.tab === 'SYS' && <SysInfo s={s} game={game} />}
        </div>
        <div className="tabs">
          {(['NAV', 'COM', 'SHIP', 'MAP', 'SYS'] as const).map((t) => (
            <button
              key={t}
              className={s.tab === t ? 'on' : ''}
              onClick={() => {
                useHud.setState({ tab: t })
                audio.blip()
              }}
            >
              {t}
            </button>
          ))}
        </div>
      </div>
      <div className="screen" style={place(right)}>
        <div className="shield-panel">
          <div className="ring" style={{ width: ringSize, height: ringSize }}>
            <svg width={ringSize} height={ringSize} viewBox="-50 -50 100 100">
              <circle r="44" fill="none" stroke="rgba(143,208,255,0.15)" strokeWidth="4" />
              <circle r="44" fill="none" stroke="#8fd0ff" strokeWidth="4" strokeDasharray={`${s.shields * 276} 276`} transform="rotate(-90)" style={{ filter: 'drop-shadow(0 0 4px #8fd0ff)' }} />
              {Array.from({ length: 36 }).map((_, i) => (
                <line key={i} x1="0" y1="-38" x2="0" y2="-35" stroke="rgba(143,208,255,0.4)" transform={`rotate(${i * 10})`} />
              ))}
              <path d="M0 -30 L5 -12 L15 -2 L15 2 L5 0 L3 7 L-3 7 L-5 0 L-15 2 L-15 -2 L-5 -12 Z" fill="none" stroke="#cfe8ff" strokeWidth="1.2" />
            </svg>
            <div className="val">
              <b>{Math.round(s.shields * 100)}%</b>
              <span>SHIELDS</span>
            </div>
          </div>
          <div className="stats">
            <ShipSilhouette />
            <Stat label="HULL" v={s.hull} />
            <Stat label="FUEL" v={s.fuel / s.fuelMax} />
            <Stat label="MASS" v={s.mass} />
          </div>
        </div>
      </div>
    </>
  )
}

function ShipSilhouette() {
  return (
    <svg viewBox="0 0 140 24" style={{ width: '70%', height: 18, alignSelf: 'center' }}>
      <path d="M2 12 L40 6 L70 4 L110 8 L138 12 L110 16 L70 20 L40 18 Z" fill="none" stroke="#cfe8ff" strokeWidth="1" />
      <circle cx="96" cy="12" r="7" fill="none" stroke="#cfe8ff" strokeWidth="1" />
      <line x1="40" y1="12" x2="88" y2="12" stroke="#8fd0ff" strokeWidth="0.8" />
    </svg>
  )
}

function Switch({ on, label }: { on: boolean; label: string }) {
  return (
    <div className={`switch ${on ? 'on' : ''}`}>
      <i />
      {label}
    </div>
  )
}

function Bar({ label, v }: { label: string; v: number }) {
  const neg = v < 0
  return (
    <div className="bar">
      <div className="track">
        <div className={`fill ${neg ? 'neg' : ''}`} style={{ height: `${Math.min(1, Math.abs(v)) * 100}%` }} />
      </div>
      <label>{label}</label>
    </div>
  )
}

function Stat({ label, v }: { label: string; v: number }) {
  return (
    <div className="stat">
      <span>{label}</span>
      <div className="track">
        <div style={{ width: `${Math.max(0, Math.min(1, v)) * 100}%` }} />
      </div>
      <span className="num">{Math.round(v * 100)}%</span>
    </div>
  )
}

function Compass({ heading }: { heading: number }) {
  const labels = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW']
  const items = []
  for (let k = -2; k < 18; k++) items.push(labels[((k % 8) + 8) % 8])
  const offset = -((heading / 45) * 44) - 2 * 44
  return (
    <div className="compass">
      <div className="tape" style={{ transform: `translateX(${offset + 22 * 0}px)`, left: '50%', marginLeft: -22 }}>
        {items.map((l, i) => (
          <span key={i}>{l}</span>
        ))}
      </div>
      <div className="tick" />
    </div>
  )
}

function NavList({ s, game }: { s: HudState; game: Game }) {
  return (
    <div className="navlist">
      {s.nav.map((n) => (
        <div key={n.id} className={`row ${s.target?.id === n.id ? 'on' : ''}`} onClick={() => game.setTarget(n.id)}>
          <span>
            <span className="kind">{KIND_ICON[n.kind] ?? '·'}</span>
            {n.name}
          </span>
          <small>{fmtDist(n.dist)}</small>
        </div>
      ))}
    </div>
  )
}

function ComLog({ s }: { s: HudState }) {
  return (
    <div className="mini">
      {s.messages
        .slice(-30)
        .reverse()
        .map((m) => (
          <div key={m.id} className="msgline" style={{ color: m.tone === 'ai' ? '#e2d0ff' : m.tone === 'bad' ? '#ffb0a0' : m.tone === 'warn' ? '#ffe0b0' : undefined }}>
            {m.text}
          </div>
        ))}
    </div>
  )
}

function ShipInfo({ s }: { s: HudState }) {
  return (
    <div className="mini">
      <div>
        <span className="k">Credits</span> {s.credits.toLocaleString()} cr
      </div>
      <div>
        <span className="k">Cargo</span> {s.cargo}/{s.cargoCap} t · <span className="k">Fuel</span> {s.fuel.toFixed(1)} t · <span className="k">Range</span> {s.jumpRange.toFixed(1)} ly
      </div>
      <div>
        <span className="k">Exploration data</span> {s.data.reduce((a, d) => a + d.value, 0).toLocaleString()} cr ({s.data.length})
      </div>
      {s.missions.map((m) => (
        <div key={m.id}>
          <span className="k">Contract</span> {m.title} → {m.destStationName ?? m.destSystemName} ({m.reward.toLocaleString()} cr)
        </div>
      ))}
      {s.route.length > 0 && (
        <div>
          <span className="k">Route</span> {s.route.map((r) => r.name).join(' › ')}
        </div>
      )}
    </div>
  )
}

function SysInfo({ s, game }: { s: HudState; game: Game }) {
  const sys = s.system
  return (
    <div className="mini">
      {sys && (
        <>
          <div>
            <span className="k">{sys.name}</span> · {sys.profile.faction} · {sys.profile.government} · {sys.profile.economy}
          </div>
          <div>
            <span className="k">Charted by</span> {sys.source === 'ai' ? `Stellar Forge AI (${sys.model ?? 'agent'}) for CMDR ${sys.discoveredBy}` : sys.source === 'authored' ? sys.discoveredBy : 'onboard survey'}
          </div>
        </>
      )}
      <div className="interactive" style={{ margin: '4px 0' }}>
        <ForgePanel compact />
      </div>
      <div style={{ marginTop: 4, display: 'flex', gap: 6, flexWrap: 'wrap' }}>
        <button className="btn small" onClick={() => (game.input.invertY = !game.input.invertY)}>
          Invert Y
        </button>
        <button className="btn small" onClick={() => audio.setMuted(!audio.muted)}>
          {audio.muted ? 'Unmute' : 'Mute'}
        </button>
        <button className="btn small" onClick={() => game.setOverlay('help')}>
          Controls
        </button>
      </div>
    </div>
  )
}
