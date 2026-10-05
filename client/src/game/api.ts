// Talks to the Python Stellar Forge. Everything degrades gracefully to the
// onboard procedural generator if the backend or the OpenAI key is missing.

import { distLy, HOME_ID, starsNear } from './galaxy'
import { KELVAR } from './kelvar'
import { generateProcedural } from './procgen'
import type { StarDescriptor, StarSystem } from './types'

export interface ForgeStatus {
  online: boolean
  ai: boolean
  model?: string
  reason?: string
  auth?: 'api_key' | 'chatgpt' | 'scripted'
  account?: string | null
  signedIn?: boolean
  canSignIn?: boolean
  sharing?: boolean
}

export interface ChartedSummary {
  id: string
  name: string
  source: string
  model?: string
  discoveredBy?: string
  createdAt?: string
  faction?: string
  economy?: string
  stations?: number
  bodies?: number
}

const API = (import.meta.env.VITE_API_BASE as string | undefined) ?? '/api'
const memCache = new Map<string, StarSystem>()
let status: ForgeStatus = { online: false, ai: false, reason: 'not checked' }

async function fetchJson<T>(url: string, init?: RequestInit, timeoutMs = 8000): Promise<T> {
  const ctl = new AbortController()
  const t = setTimeout(() => ctl.abort(), timeoutMs)
  try {
    const r = await fetch(url, { ...init, signal: ctl.signal, headers: { 'Content-Type': 'application/json', ...(init?.headers ?? {}) } })
    if (!r.ok) throw new Error(`${r.status}`)
    return (await r.json()) as T
  } finally {
    clearTimeout(t)
  }
}

export async function checkForge(): Promise<ForgeStatus> {
  try {
    type H = { enabled: boolean; model?: string; reason?: string; auth?: ForgeStatus['auth']; account?: string | null; signed_in?: boolean; can_sign_in?: boolean; sharing?: boolean }
    const h = await fetchJson<{ ok: boolean; ai: H }>(`${API}/health`, undefined, 2500)
    status = { online: true, ai: h.ai.enabled, model: h.ai.model, reason: h.ai.reason, auth: h.ai.auth, account: h.ai.account, signedIn: h.ai.signed_in, canSignIn: h.ai.can_sign_in, sharing: h.ai.sharing }
  } catch {
    status = { online: false, ai: false, reason: 'Stellar Forge server offline — onboard procedural generator active' }
  }
  return status
}

export async function chatgptSignIn(): Promise<string> {
  const r = await fetchJson<{ url: string }>(`${API}/auth/chatgpt/start`, undefined, 10000)
  return r.url
}

export async function chatgptSignOut() {
  await fetchJson(`${API}/auth/chatgpt/logout`, { method: 'POST' })
}

export async function chatgptModels(): Promise<{ models: { slug: string; name: string }[]; selected?: string | null }> {
  return fetchJson(`${API}/auth/chatgpt/models`, undefined, 10000)
}

export async function chatgptSetModel(slug: string) {
  await fetchJson(`${API}/auth/chatgpt/model`, { method: 'POST', body: JSON.stringify({ slug }) })
}

export function forgeStatus() {
  return status
}

export function cachedSystem(id: string) {
  if (id === HOME_ID) return KELVAR
  return memCache.get(id)
}

function neighborsOf(star: StarDescriptor) {
  return starsNear(star.pos, 25)
    .filter((s) => s.id !== star.id)
    .map((s) => ({ id: s.id, name: s.name, distanceLy: distLy(s, star), cls: s.cls }))
    .sort((a, b) => a.distanceLy - b.distanceLy)
    .slice(0, 10)
}

async function lockIn(sys: StarSystem) {
  if (!status.online) return
  try {
    await fetchJson(`${API}/systems/${encodeURIComponent(sys.id)}`, { method: 'PUT', body: JSON.stringify(sys) })
  } catch {
    /* ignore */
  }
}

const pending = new Map<string, Promise<StarSystem>>()
export let lastForgeError: string | null = null

/**
 * Load (or create) a star system. `onLog` receives Stellar Forge telemetry
 * lines while the AI builds the system.
 */
export function loadSystem(star: StarDescriptor, commander: string, onLog?: (lines: string[]) => void): Promise<StarSystem> {
  if (star.id === HOME_ID) return Promise.resolve(KELVAR)
  const hit = memCache.get(star.id)
  if (hit) return Promise.resolve(hit)
  const inflight = pending.get(star.id)
  if (inflight && !onLog) return inflight
  const p = (async () => {
    if (!status.online) await checkForge()
    if (!status.online) {
      const sys = generateProcedural(star)
      memCache.set(star.id, sys)
      return sys
    }
    try {
      type Resp = { status: string; system?: StarSystem | null; log?: string[]; error?: string; reason?: string }
      let r = await fetchJson<Resp>(`${API}/systems/${encodeURIComponent(star.id)}/generate`, {
        method: 'POST',
        body: JSON.stringify({ star, neighbors: neighborsOf(star), commander }),
      })
      const started = Date.now()
      while (r.status === 'generating' && Date.now() - started < 300_000) {
        onLog?.(r.log ?? [])
        await new Promise((res) => setTimeout(res, 650))
        r = await fetchJson<Resp>(`${API}/systems/${encodeURIComponent(star.id)}/job`)
      }
      onLog?.(r.status === 'failed' && r.error ? [...(r.log ?? []), `Forge error: ${r.error}`] : r.log ?? [])
      if (r.status === 'failed' && r.error) lastForgeError = r.error
      if (r.status === 'ready' && r.system) {
        memCache.set(star.id, r.system)
        return r.system
      }
      const sys = generateProcedural(star)
      memCache.set(star.id, sys)
      void lockIn(sys)
      return sys
    } catch {
      const sys = generateProcedural(star)
      memCache.set(star.id, sys)
      return sys
    }
  })()
  pending.set(star.id, p)
  p.finally(() => pending.delete(star.id))
  return p
}

/** Fire-and-forget: start AI generation early (e.g. when a route is plotted). */
export function prefetchSystem(star: StarDescriptor, commander: string) {
  if (star.id === HOME_ID || memCache.has(star.id) || pending.has(star.id)) return
  if (!status.online || !status.ai) return
  void fetchJson(`${API}/systems/${encodeURIComponent(star.id)}/generate`, {
    method: 'POST',
    body: JSON.stringify({ star, neighbors: neighborsOf(star), commander, prefetch: true }),
  }).catch(() => undefined)
}

export async function chartedSystems(): Promise<ChartedSummary[]> {
  if (!status.online) return []
  try {
    return await fetchJson<ChartedSummary[]>(`${API}/systems`)
  } catch {
    return []
  }
}

/** Synchronously best-effort system for map previews: cache or procedural. */
export function previewSystem(star: StarDescriptor): StarSystem {
  return cachedSystem(star.id) ?? generateProcedural(star)
}

export type { StarSystem }
