import { useHud } from '../game/hud'

export function Hyperspace() {
  const h = useHud((s) => s.hyper)
  if (!h) return null
  // reveal telemetry progressively so it reads like a live uplink
  const shown = Math.max(0, Math.floor((h.elapsed - 0.8) / 0.55) + 1)
  const lines = h.log.slice(0, shown).slice(-9)
  return (
    <>
      {h.phase === 'exit' && <div className="flash" />}
      <div className="hyper">
        <h2>HYPERSPACE</h2>
        <h1>{h.dest}</h1>
        <div className="cls">{h.destClass}</div>
        <div className="forge-log">
          <div className="hd">
            <span>{h.ai ? 'STELLAR FORGE · AI CARTOGRAPHY' : 'ONBOARD SURVEY COMPUTER'}</span>
            <span>T+{h.elapsed.toFixed(1)}s</span>
          </div>
          {lines.length === 0 && <div className="line">{h.ai ? 'Contacting the Stellar Forge…' : 'Plotting exit vector from stellar catalogue…'}</div>}
          {lines.map((l, i) => (
            <div key={`${i}-${l}`} className="line">
              {l}
            </div>
          ))}
          {h.elapsed > 6.5 && h.phase !== 'exit' && shown >= h.log.length && <div className="line">Holding in witch-space until cartography completes…</div>}
        </div>
      </div>
    </>
  )
}
