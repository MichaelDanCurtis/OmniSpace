import { useMemo, useState } from 'react'
import type { Game } from '../game/engine'
import { CLASS_LABEL } from '../game/galaxy'
import { useHud } from '../game/hud'
import { AU, type Body, type Station } from '../game/types'

type Sel = { kind: 'star' } | { kind: 'body'; b: Body } | { kind: 'station'; s: Station }

export function SystemMap({ game }: { game: Game }) {
  const sys = useHud((s) => s.system)
  const target = useHud((s) => s.target)
  const [sel, setSel] = useState<Sel>({ kind: 'star' })
  const layout = useMemo(() => {
    if (!sys) return null
    const planets = sys.bodies.filter((b) => b.kind === 'planet').sort((a, b) => a.orbitM - b.orbitM)
    const W = 1600
    const x0 = 210
    const step = planets.length ? Math.min(170, (W - x0 - 80) / planets.length) : 170
    const Y = 600
    const radius = (b: Body) => Math.max(6, Math.min(46, Math.log10(b.radiusM / 1e5) * 15))
    const items = planets.map((p, i) => {
      const x = x0 + i * step + step / 2
      const moons = sys.bodies.filter((m) => m.parent === p.id)
      const stations = sys.stations.filter((s) => s.parent === p.id || moons.some((m) => m.id === s.parent))
      return { p, x, r: radius(p), moons: moons.map((m, k) => ({ m, y: Y + 64 + k * 36, r: Math.max(4, radius(m) * 0.6) })), stations }
    })
    return { items, step, W, Y }
  }, [sys])
  if (!sys || !layout) return null
  const Y = layout.Y
  const scanned = new Set(game.player.scanned)
  const starR = 90
  const grad = (id: string, pal: string[]) => (
    <radialGradient id={`g-${id}`} cx="35%" cy="35%" r="70%">
      <stop offset="0" stopColor={pal[2]} />
      <stop offset="0.5" stopColor={pal[1]} />
      <stop offset="1" stopColor={pal[0]} />
    </radialGradient>
  )
  const isSel = (id: string) => (sel.kind === 'body' && sel.b.id === id) || (sel.kind === 'station' && sel.s.id === id)
  return (
    <div className="map sysmap">
      <svg viewBox="0 0 1600 900" preserveAspectRatio="xMidYMid meet">
        <defs>
          <radialGradient id="g-star" cx="50%" cy="50%" r="50%">
            <stop offset="0" stopColor="#fff" />
            <stop offset="0.4" stopColor={sys.star.color} />
            <stop offset="1" stopColor={sys.star.color} stopOpacity="0" />
          </radialGradient>
          {sys.bodies.map((b) => grad(b.id, b.palette))}
        </defs>
        <line x1="60" y1={Y} x2="1560" y2={Y} stroke="rgba(143,208,255,0.25)" />
        <g className="body-hit" onClick={() => setSel({ kind: 'star' })}>
          <circle cx="90" cy={Y} r={starR * 1.6} fill="url(#g-star)" />
          <text x="90" y={Y + 170} textAnchor="middle" style={{ fontSize: 14 }}>
            {sys.name.toUpperCase()}
          </text>
          <text x="90" y={Y + 190} textAnchor="middle" style={{ fill: '#8fa6bd' }}>
            {sys.star.cls}
          </text>
        </g>
        {sys.belts.map((b) => {
          const idx = layout.items.findIndex((it) => it.p.orbitM > b.radiusM)
          const x = idx < 0 ? layout.items.length ? layout.items[layout.items.length - 1].x + layout.step / 2 : 260 : layout.items[idx].x - layout.step / 2
          return (
            <g key={b.id}>
              <line x1={x} y1={Y - 140} x2={x} y2={Y + 140} stroke="#a89a88" strokeDasharray="2 6" strokeWidth="3" />
              <text x={x} y={Y - 150} textAnchor="middle" style={{ fill: '#a89a88' }}>
                {b.name.toUpperCase()}
              </text>
            </g>
          )
        })}
        {layout.items.map(({ p, x, r, moons, stations }) => (
          <g key={p.id}>
            {moons.length > 0 && <line x1={x} y1={Y + r} x2={x} y2={moons[moons.length - 1].y} stroke="rgba(143,208,255,0.2)" />}
            <g className="body-hit" onClick={() => setSel({ kind: 'body', b: p })}>
              {p.rings && <ellipse cx={x} cy={Y} rx={r * 2} ry={r * 0.45} fill="none" stroke={p.rings.color} strokeOpacity={0.7} strokeWidth={r * 0.25} />}
              <circle cx={x} cy={Y} r={r} fill={`url(#g-${p.id})`} stroke={isSel(p.id) ? '#ffb347' : target?.id === p.id ? '#ffb347aa' : 'none'} strokeWidth="2" />
              {p.atmosphere && <circle cx={x} cy={Y} r={r + 2} fill="none" stroke={p.atmosphere} strokeOpacity="0.5" />}
              <text x={x} y={Y - r - (p.rings ? 14 : 10)} textAnchor="middle">
                {p.name.toUpperCase()}
              </text>
              <text x={x} y={Y - r - (p.rings ? 28 : 24)} textAnchor="middle" style={{ fill: scanned.has(`${sys.id}:${p.id}`) ? '#6cffb0' : '#8fa6bd', fontSize: 9 }}>
                {(p.orbitM / AU).toFixed(2)} AU {scanned.has(`${sys.id}:${p.id}`) ? '· SCANNED' : ''}
              </text>
            </g>
            {moons.map(({ m, y, r: mr }) => (
              <g key={m.id} className="body-hit" onClick={() => setSel({ kind: 'body', b: m })}>
                <circle cx={x} cy={y} r={mr} fill={`url(#g-${m.id})`} stroke={isSel(m.id) ? '#ffb347' : 'none'} strokeWidth="2" />
                <text x={x + mr + 8} y={y + 4}>
                  {m.name}
                </text>
              </g>
            ))}
            {stations.map((s, k) => (
              <g key={s.id} className="body-hit" onClick={() => setSel({ kind: 'station', s })}>
                <rect x={x - 7} y={Y - 90 - k * 26 - r} width="14" height="14" transform={`rotate(45 ${x} ${Y - 83 - k * 26 - r})`} fill={isSel(s.id) ? '#ffb347' : '#8fd0ff'} />
                <text x={x + 14} y={Y - 78 - k * 26 - r} style={{ fill: '#8fd0ff' }}>
                  {s.name}
                </text>
              </g>
            ))}
          </g>
        ))}
      </svg>
      <div className="close">
        <button className="btn small" onClick={() => game.setOverlay(null)}>
          Close map (N)
        </button>
      </div>
      <div className="side left" style={{ bottom: 'auto', maxHeight: '44vh' }}>
        <h2>{sys.name}</h2>
        <div style={{ color: 'var(--text-dim)', fontSize: 13 }}>{sys.starFlavor ?? CLASS_LABEL[sys.star.cls]}</div>
        {sys.source === 'ai' && (
          <div style={{ marginTop: 8 }}>
            <span className="pill ai">Stellar Forge · {sys.model ?? 'AI'}</span>
            <span className="pill">CMDR {sys.discoveredBy}</span>
          </div>
        )}
        <p>{sys.profile.description}</p>
        <div className="kv">
          <span>Faction</span>
          <span>{sys.profile.faction}</span>
          <span>Government</span>
          <span>{sys.profile.government}</span>
          <span>Economy</span>
          <span>{sys.profile.economy}</span>
          <span>Security</span>
          <span>{sys.profile.security}</span>
          <span>Population</span>
          <span>{sys.profile.population.toLocaleString()}</span>
          <span>Danger</span>
          <span>{'▲'.repeat(sys.profile.danger) || '—'}</span>
        </div>
        <p style={{ color: 'var(--text-dim)' }}>{sys.profile.lore}</p>
      </div>
      <div className="side right" style={{ bottom: 'auto', maxHeight: '60vh' }}>
        {sel.kind === 'star' && (
          <>
            <h2>{sys.name}</h2>
            <div className="kv">
              <span>Class</span>
              <span>{CLASS_LABEL[sys.star.cls]}</span>
              <span>Temperature</span>
              <span>{Math.round(sys.star.temperature).toLocaleString()} K</span>
              <span>Radius</span>
              <span>{sys.star.radiusSolar.toFixed(3)} R☉</span>
              <span>Mass</span>
              <span>{sys.star.massSolar.toFixed(2)} M☉</span>
              <span>Age</span>
              <span>{sys.star.ageGyr} Gyr</span>
            </div>
            <div style={{ marginTop: 14 }}>
              <button className="btn" onClick={() => game.setTarget('star')}>
                Set as target
              </button>
            </div>
          </>
        )}
        {sel.kind === 'body' && (
          <>
            <h2>{sel.b.name}</h2>
            <div style={{ color: 'var(--text-dim)', fontSize: 13, textTransform: 'capitalize' }}>
              {sel.b.kind} · {sel.b.type.replace('_', ' ')}
            </div>
            <p>{sel.b.description}</p>
            <div className="kv">
              <span>Radius</span>
              <span>{Math.round(sel.b.radiusM / 1000).toLocaleString()} km</span>
              <span>Gravity</span>
              <span>{sel.b.gravityG ?? '?'} g</span>
              <span>Temperature</span>
              <span>{sel.b.temperatureK ?? '?'} K</span>
              <span>Orbit</span>
              <span>{sel.b.kind === 'planet' ? `${(sel.b.orbitM / AU).toFixed(2)} AU` : `${Math.round(sel.b.orbitM / 1000).toLocaleString()} km`}</span>
              <span>Rings</span>
              <span>{sel.b.rings ? 'Yes' : 'No'}</span>
              <span>Atmosphere</span>
              <span>{sel.b.atmosphere ? 'Yes' : 'None'}</span>
              <span>Survey</span>
              <span>{scanned.has(`${sys.id}:${sel.b.id}`) ? 'Surface scanned' : 'Fly within ~5 radii to scan'}</span>
            </div>
            <div style={{ marginTop: 14 }}>
              <button className="btn" onClick={() => game.setTarget(sel.b.id)}>
                Set as target
              </button>
            </div>
          </>
        )}
        {sel.kind === 'station' && (
          <>
            <h2>{sel.s.name}</h2>
            <div style={{ color: 'var(--amber)', fontSize: 12, letterSpacing: '0.18em', textTransform: 'uppercase' }}>{sel.s.tagline}</div>
            <p>{sel.s.description}</p>
            <div className="kv">
              <span>Faction</span>
              <span>{sel.s.faction}</span>
              <span>Economy</span>
              <span>{sel.s.economy}</span>
              <span>Type</span>
              <span>{sel.s.kind}</span>
              <span>Services</span>
              <span>{sel.s.services.join(', ')}</span>
            </div>
            <div style={{ marginTop: 14 }}>
              <button className="btn" onClick={() => game.setTarget(sel.s.id)}>
                Set as target
              </button>
            </div>
          </>
        )}
      </div>
      {(sys.pois.length > 0) && (
        <div className="legend">
          {sys.pois.map((p) => (
            <span key={p.id} style={{ cursor: 'pointer', pointerEvents: 'auto' }} onClick={() => game.setTarget(p.id)}>
              <i style={{ background: '#c890ff' }} />
              {p.name}
            </span>
          ))}
        </div>
      )}
    </div>
  )
}
