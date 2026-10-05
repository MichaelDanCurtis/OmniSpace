import { HOME_STAR, distLy, getStar, starsNear } from './galaxy'
import { COMMODITIES, ROMAN } from './lore'
import { Rng, hashString } from './rng'
import type { StarDescriptor, Station } from './types'

export interface MarketRow {
  id: string
  name: string
  buy: number // price the station sells at
  sell: number // price the station pays
  supply: number
  tag: 'produced' | 'consumed' | ''
}

export function market(st: Station, day: number): MarketRow[] {
  const rows: MarketRow[] = []
  for (const c of COMMODITIES) {
    const rng = new Rng(hashString(`${st.id}:${c.id}:${day}`))
    const prod = c.produced.includes(st.economy)
    const cons = c.consumed.includes(st.economy)
    let f = 1
    if (prod) f = 0.68
    if (cons) f = 1.38
    f *= 1 + (rng.next() - 0.5) * 0.18
    const price = Math.round(c.base * f)
    rows.push({
      id: c.id,
      name: c.name,
      buy: Math.round(price * 1.03),
      sell: Math.round(price * 0.97),
      supply: prod ? rng.int(400, 2400) : cons ? rng.int(0, 60) : rng.int(40, 600),
      tag: prod ? 'produced' : cons ? 'consumed' : '',
    })
  }
  return rows
}

export interface Mission {
  id: string
  kind: 'delivery' | 'courier'
  title: string
  description: string
  fromStation: string
  fromSystem: string
  destSystem: string
  destSystemName: string
  destStation: string | null // null = any station in system
  destStationName: string | null
  commodity?: string
  tons: number
  reward: number
  distanceLy: number
}

const CLIENTS = ['Halden Logistics', 'Meridian Survey Corps', 'Brightwater Medical', 'Ostrova Freight', 'a private client', 'the Free Worlds Relief Fund', 'Kelvar Directorate Logistics', 'Ashkar Pilgrim Office']

export function missionBoard(st: Station, sysStar: StarDescriptor, sysStations: Station[], day: number): Mission[] {
  const rng = new Rng(hashString(`${st.id}:missions:${day}`))
  const out: Mission[] = []
  const neighbors = starsNear(sysStar.pos, 26)
    .filter((s) => s.id !== sysStar.id && s.settlement > 0.45)
    .sort((a, b) => distLy(a, sysStar) - distLy(b, sysStar))
  if (sysStar.id !== HOME_STAR.id && distLy(sysStar, HOME_STAR) < 26) neighbors.unshift(HOME_STAR)
  const n = rng.int(3, 5)
  for (let i = 0; i < n; i++) {
    const local = sysStations.filter((s) => s.id !== st.id)
    const useLocal = local.length > 0 && rng.chance(0.3)
    let destSystem = sysStar
    let destStation: Station | null = null
    if (useLocal) destStation = rng.pick(local)
    else if (neighbors.length) destSystem = rng.pick(neighbors.slice(0, 8))
    else if (local.length) destStation = rng.pick(local)
    else continue
    const dist = distLy(destSystem, sysStar)
    const courier = rng.chance(0.35)
    const com = rng.pick(COMMODITIES.filter((c) => c.base < 3000))
    const tons = courier ? 0 : rng.int(2, 12) * 2
    const reward = Math.round((900 + dist * 320 + tons * 75 + (destStation ? 300 : 0)) * rng.range(0.85, 1.3) / 10) * 10
    const client = rng.pick(CLIENTS)
    const where = destStation ? `${destStation.name}${destSystem.id === sysStar.id ? '' : ` in ${destSystem.name}`}` : `any station in the ${destSystem.name} system`
    out.push({
      id: `${st.id}-${day}-${i}`,
      kind: courier ? 'courier' : 'delivery',
      title: courier ? `Courier: sealed data to ${destSystem.name}` : `Deliver ${tons}t ${com.name}`,
      description: courier
        ? `${client[0].toUpperCase() + client.slice(1)} needs encrypted data carried to ${where}. No cargo space required.`
        : `${client[0].toUpperCase() + client.slice(1)} requests ${tons} tonnes of ${com.name} delivered to ${where}. Cargo is provided.`,
      fromStation: st.id,
      fromSystem: sysStar.id,
      destSystem: destSystem.id,
      destSystemName: destSystem.name,
      destStation: destStation?.id ?? null,
      destStationName: destStation?.name ?? null,
      commodity: courier ? undefined : com.id,
      tons,
      reward,
      distanceLy: Math.round(dist * 10) / 10,
    })
  }
  return out
}

export const SCAN_VALUE: Record<string, number> = {
  rocky: 900, barren: 500, desert: 1100, lava: 1300, ice: 1000, ocean: 6500, earthlike: 14000, jungle: 9000, toxic: 1600, gas_giant: 3200, ice_giant: 2800,
}

export function starName(id: string) {
  return getStar(id)?.name ?? id
}

export { ROMAN }
