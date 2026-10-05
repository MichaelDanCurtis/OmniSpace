import { useState } from 'react'
import type { Game } from '../game/engine'
import { useHud } from '../game/hud'
import { COMMODITIES } from '../game/lore'
import { cargoTotal, SHIP } from '../game/player'

type Tab = 'concourse' | 'market' | 'contracts' | 'services' | 'cartographics'

export function StationMenu({ game }: { game: Game }) {
  const [tab, setTab] = useState<Tab>('concourse')
  const [, force] = useState(0)
  useHud((s) => s.credits)
  useHud((s) => s.cargo)
  const sys = useHud((s) => s.system)
  const st = game.stationData()
  if (!st || !sys) return null
  const refresh = () => force((x) => x + 1)
  const p = game.player
  const has = (svc: string) => st.services.includes(svc)
  const tabs: [Tab, string, boolean][] = [
    ['concourse', 'Concourse', true],
    ['market', 'Commodities', has('market')],
    ['contracts', 'Contracts', true],
    ['services', 'Refuel & Repair', true],
    ['cartographics', 'Cartographics', has('cartographics')],
  ]
  return (
    <div className="station">
      <header>
        <div>
          <div className="tag">{st.tagline}</div>
          <h1>{st.name}</h1>
        </div>
        <div className="meta">
          {sys.name} · {st.faction}
          <br />
          {st.economy} economy · {st.kind} station
          <br />
          <span style={{ color: 'var(--amber)' }}>{p.credits.toLocaleString()} CR</span> · Cargo {cargoTotal(p)}/{SHIP.cargoCap} t
        </div>
      </header>
      <nav>
        {tabs
          .filter(([, , on]) => on)
          .map(([k, label]) => (
            <button key={k} className={tab === k ? 'on' : ''} onClick={() => setTab(k)}>
              {label}
            </button>
          ))}
      </nav>
      <div className="body">
        {tab === 'concourse' && (
          <div>
            <p className="lore">{st.description}</p>
            <p className="lore" style={{ color: 'var(--text-dim)' }}>
              {sys.profile.lore}
            </p>
            <div style={{ marginTop: 14 }}>
              {sys.source === 'ai' && <span className="pill ai">Charted by the Stellar Forge · CMDR {sys.discoveredBy}</span>}
              <span className="pill">{sys.profile.government}</span>
              <span className="pill">Security: {sys.profile.security}</span>
              <span className="pill">Population {sys.profile.population.toLocaleString()}</span>
              {st.services.map((s) => (
                <span key={s} className="pill">
                  {s}
                </span>
              ))}
            </div>
            <h3 style={{ marginTop: 24, letterSpacing: '0.2em', fontWeight: 500, fontSize: 13, color: 'var(--text-dim)' }}>YOUR CARGO</h3>
            {cargoTotal(p) === 0 ? (
              <p style={{ color: 'var(--text-dim)' }}>Cargo hold empty.</p>
            ) : (
              <div>
                {Object.entries(p.cargo).map(([id, n]) => (
                  <span key={id} className="pill">
                    {COMMODITIES.find((c) => c.id === id)?.name} × {n}
                  </span>
                ))}
                {p.missions
                  .filter((m) => m.tons)
                  .map((m) => (
                    <span key={m.id} className="pill" style={{ borderColor: 'var(--amber-dim)' }}>
                      Contract cargo × {m.tons}
                    </span>
                  ))}
              </div>
            )}
          </div>
        )}
        {tab === 'market' && <Market game={game} refresh={refresh} />}
        {tab === 'contracts' && <Contracts game={game} refresh={refresh} />}
        {tab === 'services' && (
          <div className="cards">
            <div className="card">
              <h3>Refuel</h3>
              <p>
                Fuel {p.fuel.toFixed(1)} / {SHIP.fuelMax} t
              </p>
              <button className="btn primary" disabled={game.refuelCost() <= 0 || p.credits < game.refuelCost()} onClick={() => (game.refuel(), refresh())}>
                Refuel · {game.refuelCost().toLocaleString()} cr
              </button>
            </div>
            <div className="card">
              <h3>Repair</h3>
              <p>Hull integrity {Math.round(p.hull * 100)}%</p>
              <button className="btn primary" disabled={game.repairCost() <= 0 || p.credits < game.repairCost()} onClick={() => (game.repair(), refresh())}>
                Repair · {game.repairCost().toLocaleString()} cr
              </button>
            </div>
            <div className="card">
              <h3>{SHIP.name}</h3>
              <p>
                Jump range {SHIP.jumpRange} ly (laden less) · Cargo {SHIP.cargoCap} t · Top speed {SHIP.maxSpeed} m/s, boost {SHIP.boostSpeed} m/s.
              </p>
            </div>
          </div>
        )}
        {tab === 'cartographics' && (
          <div>
            <p className="lore">Universal Cartographics buys survey data on systems, worlds and anomalies. First discoveries charted by the Stellar Forge pay a 50% bonus.</p>
            <table>
              <thead>
                <tr>
                  <th>System</th>
                  <th>Data</th>
                  <th style={{ textAlign: 'right' }}>Value</th>
                </tr>
              </thead>
              <tbody>
                {p.data.map((d, i) => (
                  <tr key={i}>
                    <td>{d.systemName}</td>
                    <td>{d.name}</td>
                    <td className="num">{d.value.toLocaleString()} cr</td>
                  </tr>
                ))}
              </tbody>
            </table>
            <div style={{ marginTop: 14 }}>
              <button className="btn primary" disabled={!p.data.length} onClick={() => (game.sellData(), refresh())}>
                Sell all data · {game.dataValue().toLocaleString()} cr
              </button>
            </div>
          </div>
        )}
      </div>
      <footer>
        <span>
          Press <span className="kbd">L</span> to launch · <span className="kbd">M</span> galaxy map · <span className="kbd">N</span> system map
        </span>
        <button className="btn primary" onClick={() => game.launch()}>
          Launch
        </button>
      </footer>
    </div>
  )
}

function Market({ game, refresh }: { game: Game; refresh: () => void }) {
  const rows = game.marketRows()
  const p = game.player
  const free = SHIP.cargoCap - cargoTotal(p)
  return (
    <table>
      <thead>
        <tr>
          <th>Commodity</th>
          <th></th>
          <th style={{ textAlign: 'right' }}>Buy</th>
          <th style={{ textAlign: 'right' }}>Sell</th>
          <th style={{ textAlign: 'right' }}>Supply</th>
          <th style={{ textAlign: 'right' }}>Held</th>
          <th></th>
        </tr>
      </thead>
      <tbody>
        {rows.map((r) => (
          <tr key={r.id}>
            <td>{r.name}</td>
            <td>{r.tag === 'produced' ? <span className="tagp">PRODUCED · CHEAP</span> : r.tag === 'consumed' ? <span className="tagc">IN DEMAND</span> : ''}</td>
            <td className="num">{r.buy.toLocaleString()}</td>
            <td className="num">{r.sell.toLocaleString()}</td>
            <td className="num">{r.supply.toLocaleString()}</td>
            <td className="num">{p.cargo[r.id] ?? 0}</td>
            <td style={{ textAlign: 'right', whiteSpace: 'nowrap' }}>
              <button className="btn small" disabled={free <= 0 || p.credits < r.buy || r.supply <= 0} onClick={() => (game.buy(r.id, 1), refresh())}>
                +1
              </button>{' '}
              <button className="btn small" disabled={free <= 0 || p.credits < r.buy || r.supply <= 0} onClick={() => (game.buy(r.id, 8), refresh())}>
                +8
              </button>{' '}
              <button className="btn small" disabled={!p.cargo[r.id]} onClick={() => (game.sell(r.id, p.cargo[r.id] ?? 0), refresh())}>
                Sell all
              </button>
            </td>
          </tr>
        ))}
      </tbody>
    </table>
  )
}

function Contracts({ game, refresh }: { game: Game; refresh: () => void }) {
  const offers = game.missionsOffered()
  const mine = game.player.missions
  return (
    <div>
      {mine.length > 0 && (
        <>
          <h3 style={{ letterSpacing: '0.2em', fontWeight: 500, fontSize: 13, color: 'var(--text-dim)' }}>ACTIVE CONTRACTS</h3>
          <div className="cards" style={{ marginBottom: 20 }}>
            {mine.map((m) => (
              <div key={m.id} className="card" style={{ borderColor: 'var(--amber-dim)' }}>
                <h3>{m.title}</h3>
                <p>{m.description}</p>
                <div className="row">
                  <span className="reward">{m.reward.toLocaleString()} cr</span>
                  <span style={{ color: 'var(--text-dim)', fontSize: 12 }}>{m.distanceLy ? `${m.distanceLy} ly` : 'in system'}</span>
                </div>
              </div>
            ))}
          </div>
        </>
      )}
      <h3 style={{ letterSpacing: '0.2em', fontWeight: 500, fontSize: 13, color: 'var(--text-dim)' }}>MISSION BOARD</h3>
      {offers.length === 0 && <p style={{ color: 'var(--text-dim)' }}>No contracts available right now. Check back later.</p>}
      <div className="cards">
        {offers.map((m) => (
          <div key={m.id} className="card">
            <h3>{m.title}</h3>
            <p>{m.description}</p>
            <div className="row">
              <span className="reward">{m.reward.toLocaleString()} cr</span>
              <span style={{ color: 'var(--text-dim)', fontSize: 12 }}>{m.distanceLy ? `${m.distanceLy} ly` : 'in system'}</span>
              <button
                className="btn small primary"
                onClick={() => {
                  game.acceptMission(m)
                  if (m.destSystem !== game.player.systemId) game.setHyperTarget(m.destSystem)
                  refresh()
                }}
              >
                Accept
              </button>
            </div>
          </div>
        ))}
      </div>
    </div>
  )
}
