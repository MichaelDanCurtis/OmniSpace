import { useState } from 'react'
import type { Game } from '../game/engine'
import { useHud } from '../game/hud'
import { clearSave } from '../game/player'

const KEYS: [string, string][] = [
  ['Mouse', 'Pitch / yaw (click view to capture)'],
  ['↑ ↓ ← →', 'Pitch / yaw (keyboard)'],
  ['A / D', 'Roll'],
  ['W / S', 'Throttle up / down'],
  ['X / C', 'Full stop / 75% optimal'],
  ['Q / E · R / F', 'Strafe · vertical thrust'],
  ['Tab / Space', 'Boost'],
  ['J', 'Supercruise engage / drop'],
  ['H', 'Hyperspace jump (next route hop)'],
  ['T / Shift+T', 'Cycle nav targets'],
  ['L', 'Request docking / launch'],
  ['P', 'Autopilot to target (docks too)'],
  ['Z', 'Flight assist on / off'],
  ['G', 'Landing gear'],
  ['M', 'Galaxy map'],
  ['N', 'System map'],
  ['1–5', 'Dashboard tabs'],
  ['Esc', 'Release mouse / pause'],
]

export function Overlays({ game }: { game: Game }) {
  const overlay = useHud((s) => s.overlay)
  const [armed, setArmed] = useState(false)
  const mode = useHud((s) => s.mode)
  if (mode === 'destroyed') {
    return (
      <div className="overlay">
        <div className="panel destroyed">
          <h1>SHIP DESTROYED</h1>
          <p style={{ color: 'var(--text-dim)', letterSpacing: '0.1em' }}>Your insurer will deliver a replacement Kestrel to your last port of call. Cargo and contracts are lost.</p>
          <button className="btn primary" onClick={() => game.respawn()}>
            Rebuy ship (2,500 cr)
          </button>
        </div>
      </div>
    )
  }
  if (overlay !== 'pause' && overlay !== 'help') return null
  return (
    <div className="overlay" onClick={() => game.setOverlay(null)}>
      <div className="panel" onClick={(e) => e.stopPropagation()} style={{ minWidth: 560 }}>
        <h2 style={{ margin: 0, letterSpacing: '0.3em', fontWeight: 500 }}>{overlay === 'pause' ? 'PAUSED' : 'FLIGHT CONTROLS'}</h2>
        <div className="help-grid">
          {KEYS.map(([k, v]) => (
            <div key={k}>
              <span className="kbd">{k}</span>
              <span>{v}</span>
            </div>
          ))}
        </div>
        <p style={{ color: 'var(--text-dim)', fontSize: 13, lineHeight: 1.5, maxWidth: 620 }}>
          Supercruise slows near planets and your target — keep throttle around 75% on approach and you will drop out right at stations. Jumps require the hyperspace marker (violet circle) to be centred. Scoop fuel by skimming K/G/B/F/O/A/M stars in supercruise.
        </p>
        <div style={{ display: 'flex', gap: 10, marginTop: 10 }}>
          <button className="btn primary" onClick={() => game.setOverlay(null)}>
            Resume
          </button>
          {overlay === 'pause' && (
            <button
              className="btn"
              onClick={() => {
                if (!armed) return setArmed(true)
                clearSave()
                location.reload()
              }}
            >
              {armed ? 'Click again to confirm reset' : 'Reset commander'}
            </button>
          )}
        </div>
      </div>
    </div>
  )
}
