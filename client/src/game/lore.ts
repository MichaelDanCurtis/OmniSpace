// Shared canon. The same bible is given to the AI Stellar Forge (server side
// copy lives in server/omnispace_server/bible.py) so that procedurally and
// AI-built systems feel like one universe.

export const FACTIONS = [
  'Nexus Astra Combine',
  'Kelvar Directorate',
  'Free Worlds Accord',
  'Ashkar Covenant',
  'Veyl Syndicate',
  'Meridian Survey Corps',
  'Independent',
  'Unclaimed',
] as const

export const ECONOMIES = [
  'Industrial',
  'Extraction',
  'Refinery',
  'Agriculture',
  'High Tech',
  'Tourism',
  'Military',
  'Research',
  'Colony',
] as const

export const GOVERNMENTS = ['Corporate', 'Democracy', 'Theocracy', 'Cooperative', 'Anarchy', 'Directorate', 'Patronage', 'None']

export const STATION_SERVICES = ['refuel', 'repair', 'market', 'cartographics', 'shipyard', 'outfitting', 'missions'] as const

export interface Commodity {
  id: string
  name: string
  base: number
  produced: string[]
  consumed: string[]
}

export const COMMODITIES: Commodity[] = [
  { id: 'water', name: 'Purified Water', base: 120, produced: ['Extraction', 'Refinery'], consumed: ['Colony', 'Agriculture', 'Tourism'] },
  { id: 'food', name: 'Hydroponic Food', base: 260, produced: ['Agriculture'], consumed: ['Industrial', 'Extraction', 'Military', 'Colony'] },
  { id: 'ore', name: 'Raw Ore', base: 180, produced: ['Extraction'], consumed: ['Refinery', 'Industrial'] },
  { id: 'metals', name: 'Refined Metals', base: 520, produced: ['Refinery'], consumed: ['Industrial', 'High Tech', 'Military'] },
  { id: 'machinery', name: 'Heavy Machinery', base: 900, produced: ['Industrial'], consumed: ['Extraction', 'Colony', 'Agriculture'] },
  { id: 'electronics', name: 'Quantum Electronics', base: 1650, produced: ['High Tech'], consumed: ['Industrial', 'Research', 'Tourism'] },
  { id: 'medicine', name: 'Medical Supplies', base: 1100, produced: ['High Tech', 'Research'], consumed: ['Colony', 'Military', 'Extraction'] },
  { id: 'fuelcells', name: 'Fusion Fuel Cells', base: 700, produced: ['Refinery', 'Industrial'], consumed: ['Military', 'Colony', 'Research'] },
  { id: 'luxury', name: 'Luxury Goods', base: 2400, produced: ['Tourism', 'Agriculture'], consumed: ['High Tech', 'Tourism', 'Research'] },
  { id: 'weapons', name: 'Personal Weapons', base: 1900, produced: ['Military', 'Industrial'], consumed: ['Colony', 'Extraction'] },
  { id: 'artifacts', name: 'Xeno Artifacts', base: 5200, produced: ['Research'], consumed: ['High Tech', 'Tourism'] },
  { id: 'data', name: 'Encrypted Data Cores', base: 3100, produced: ['Research', 'High Tech'], consumed: ['Military', 'Industrial'] },
]

export const ROMAN = ['I', 'II', 'III', 'IV', 'V', 'VI', 'VII', 'VIII', 'IX', 'X', 'XI', 'XII', 'XIII', 'XIV', 'XV']
