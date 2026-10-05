import { useEffect, useState } from 'react'
import { checkForge, chatgptModels, chatgptSetModel, chatgptSignIn, chatgptSignOut } from '../game/api'
import { useHud } from '../game/hud'

/** Stellar Forge status + "Sign in with ChatGPT" controls. */
export function ForgePanel({ compact = false }: { compact?: boolean }) {
  const forge = useHud((s) => s.forge)
  const [busy, setBusy] = useState(false)
  const [err, setErr] = useState<string | null>(null)
  const [models, setModels] = useState<{ slug: string; name: string }[]>([])
  const [selected, setSelected] = useState<string>('')

  const refresh = async () => useHud.setState({ forge: await checkForge() })

  // while waiting for the browser sign-in to finish, poll the server
  useEffect(() => {
    if (forge.ai || !forge.online) return
    const t = setInterval(refresh, 2500)
    return () => clearInterval(t)
  }, [forge.ai, forge.online])

  useEffect(() => {
    if (forge.auth !== 'chatgpt' || !forge.signedIn) return
    chatgptModels()
      .then((r) => {
        setModels(r.models)
        setSelected(r.selected ?? '')
      })
      .catch(() => setModels([]))
  }, [forge.auth, forge.signedIn])

  const [link, setLink] = useState<string | null>(null)
  const signIn = async () => {
    setBusy(true)
    setErr(null)
    // open the tab synchronously (inside the click) so Safari's popup blocker allows it
    const tab = window.open('about:blank', '_blank')
    try {
      const url = await chatgptSignIn()
      if (tab) tab.location.href = url
      else setLink(url)
    } catch (e) {
      tab?.close()
      setErr(String((e as Error).message ?? e))
    } finally {
      setBusy(false)
    }
  }

  const cls = forge.ai ? 'ai' : forge.online ? '' : 'off'
  let text: string
  if (forge.ai && forge.auth === 'chatgpt') text = `STELLAR FORGE ONLINE · ChatGPT plan${forge.account ? ` · ${forge.account}` : ''}`
  else if (forge.ai && forge.auth === 'scripted') text = 'STELLAR FORGE ONLINE · scripted demo agent (no API calls)'
  else if (forge.ai) text = `STELLAR FORGE ONLINE · OpenAI API · ${forge.model ?? 'default model'}`
  else if (forge.online) text = forge.reason ?? 'Stellar Forge server online · AI disabled'
  else text = forge.reason ?? 'Stellar Forge offline · onboard procedural generator active'

  return (
    <div style={{ display: 'flex', flexDirection: 'column', gap: 10 }}>
      <div className={`forge-status ${cls}`}>
        <i />
        <span>{text}</span>
      </div>
      {forge.online && !forge.ai && forge.canSignIn && (
        <div style={{ display: 'flex', gap: 10, alignItems: 'center', flexWrap: 'wrap' }}>
          <button className="btn primary chatgpt" disabled={busy} onClick={signIn}>
            Sign in with ChatGPT
          </button>
          {!compact && <span style={{ fontSize: 11, color: 'var(--text-dim)', letterSpacing: '0.08em', textTransform: 'none' }}>Uses your Plus/Pro plan · or set OPENAI_API_KEY in .env</span>}
        </div>
      )}
      {forge.ai && forge.auth === 'chatgpt' && (
        <div style={{ display: 'flex', gap: 10, alignItems: 'center', flexWrap: 'wrap', fontSize: 12 }}>
          {models.length > 0 && (
            <select
              className="search"
              style={{ width: 'auto', margin: 0, padding: '4px 8px', fontSize: 12 }}
              value={selected}
              onChange={async (e) => {
                setSelected(e.target.value)
                try {
                  await chatgptSetModel(e.target.value)
                  await refresh()
                } catch (x) {
                  setErr(String((x as Error).message ?? x))
                }
              }}
            >
              <option value="">Auto (fastest)</option>
              {models.map((m) => (
                <option key={m.slug} value={m.slug}>
                  {m.name}
                </option>
              ))}
            </select>
          )}
          {forge.sharing === false && <span style={{ color: 'var(--amber)' }}>Plan usage not granted — enable it in ChatGPT and sign in again</span>}
          <button
            className="btn small"
            onClick={async () => {
              await chatgptSignOut()
              await refresh()
            }}
          >
            Sign out
          </button>
        </div>
      )}
      {link && (
        <a href={link} target="_blank" rel="noreferrer" style={{ color: 'var(--cyan)', fontSize: 12, textTransform: 'none' }}>
          Your browser blocked the sign-in tab — click here to sign in with ChatGPT
        </a>
      )}
      {err && <div style={{ color: 'var(--red)', fontSize: 12, textTransform: 'none' }}>{err}</div>}
    </div>
  )
}
