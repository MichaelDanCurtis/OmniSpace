import { useEffect, useRef, useState } from 'react'
import { audio } from './game/audio'
import { Game } from './game/engine'
import { useHud } from './game/hud'
import { setGame } from './game/instance'
import { loadSave, newPlayer, writeSave } from './game/player'
import { Hud } from './ui/Hud'
import { GalaxyMap } from './ui/GalaxyMap'
import { SystemMap } from './ui/SystemMap'
import { StationMenu } from './ui/StationMenu'
import { Hyperspace } from './ui/Hyperspace'
import { Overlays } from './ui/Overlays'
import { Logo } from './ui/Logo'
import { ForgePanel } from './ui/ForgePanel'

export default function App() {
  const canvas = useRef<HTMLCanvasElement>(null)
  const fx = useRef<HTMLCanvasElement>(null)
  const [game, setG] = useState<Game | null>(null)
  const overlay = useHud((s) => s.overlay)
  const mode = useHud((s) => s.mode)
  const ready = useHud((s) => s.ready)

  useEffect(() => {
    if (!canvas.current) return
    const save = loadSave()
    const g = new Game(canvas.current, save ?? newPlayer('Commander'))
    setGame(g)
    setG(g)
    useHud.setState({ overlay: 'title' })
    g.attachFx(fx.current)
    void g.start()
    return () => {
      g.stop()
      setGame(null)
    }
  }, [])

  return (
    <>
      <canvas ref={canvas} className="game-canvas" />
      <canvas ref={fx} className="fx-canvas" />
      {game && ready && overlay !== 'title' && mode !== 'docked' && <Hud game={game} />}
      {game && mode === 'hyperspace' && <Hyperspace />}
      {game && mode === 'docked' && overlay !== 'galaxy' && overlay !== 'system' && overlay !== 'title' && <StationMenu game={game} />}
      {game && overlay === 'galaxy' && <GalaxyMap game={game} />}
      {game && overlay === 'system' && <SystemMap game={game} />}
      {game && <Overlays game={game} />}
      {overlay === 'title' && <TitleScreen game={game} ready={ready} />}
    </>
  )
}

function TitleScreen({ game, ready }: { game: Game | null; ready: boolean }) {
  const existing = loadSave()
  const [name, setName] = useState(existing?.commander && existing.commander !== 'Commander' ? existing.commander : '')
  const start = (fresh: boolean) => {
    if (!game) return
    audio.init()
    const cmdr = name.trim() || 'Commander'
    if (fresh || !existing) {
      const p = newPlayer(cmdr)
      writeSave(p)
      game.resetPlayer(p)
    } else {
      game.player.commander = cmdr
      useHud.setState({ commander: cmdr })
    }
    useHud.setState({ overlay: null })
    game.msg(`Welcome back, CMDR ${cmdr}. Press F1 for flight controls.`, 'info')
  }
  return (
    <div className="title-screen">
      <Logo size={64} />
      <h1>OMNISPACE</h1>
      <div className="sub">EXPLORE · TRADE · BUILD · BELONG</div>
      <div className="field">
        <label>COMMANDER NAME</label>
        <input value={name} maxLength={24} placeholder="Enter your name" onChange={(e) => setName(e.target.value)} onKeyDown={(e) => e.key === 'Enter' && start(!existing)} />
      </div>
      <div className="actions">
        {existing && (
          <button className="btn primary" disabled={!ready} onClick={() => start(false)}>
            Continue
          </button>
        )}
        <button className={`btn ${existing ? '' : 'primary'}`} disabled={!ready} onClick={() => start(true)}>
          New Game
        </button>
      </div>
      <ForgePanel />
      <div className="title-hint">Click the view to fly with the mouse · F1 controls · M galaxy map · N system map</div>
    </div>
  )
}
