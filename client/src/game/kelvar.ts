// Hand-authored home system, built to match the concept art: the ringed ice
// giant Tanara VI with Tanara Station holding orbit above its rings.

import { HOME_STAR } from './galaxy'
import { AU, KM, type StarSystem } from './types'

export const KELVAR: StarSystem = {
  id: 'kelvar',
  name: 'Kelvar',
  source: 'authored',
  discoveredBy: 'Nexus Astra Survey Wing',
  star: HOME_STAR,
  starFlavor: 'G-type yellow star',
  profile: {
    description: 'Capital system of the Tanaran Reach and headquarters of the Nexus Astra Combine.',
    lore: 'Settled three centuries ago by the first Combine arkships, Kelvar grew rich on the volatile ices of Tanara VI. Every trade lane in the Reach eventually leads back to Tanara Station.',
    faction: 'Nexus Astra Combine',
    government: 'Corporate',
    economy: 'High Tech',
    security: 'high',
    population: 8_400_000_000,
    danger: 0,
  },
  bodies: [
    {
      id: 'ember', name: 'Ember', kind: 'planet', type: 'lava', parent: 'star', orbitM: 0.34 * AU, phase: 1.1, inclination: 0.02,
      radiusM: 2900 * KM, palette: ['#140b08', '#3a1a10', '#ff6a20'], atmosphere: null, tilt: 0.05, temperatureK: 480, gravityG: 0.42,
      description: 'A tidally locked furnace world. Its night side is mined by automated crawlers for rare metals.',
    },
    {
      id: 'halcyon', name: 'Halcyon', kind: 'planet', type: 'earthlike', parent: 'star', orbitM: 1.02 * AU, phase: 2.4, inclination: -0.01,
      radiusM: 6420 * KM, palette: ['#0f3a66', '#3f7d3c', '#d6c79a'], atmosphere: '#6fb0ff', clouds: 0.55, tilt: 0.4, temperatureK: 288, gravityG: 1.02,
      description: 'The garden capital of the Reach: temperate continents, deep oceans and eight billion citizens.',
    },
    {
      id: 'tanara', name: 'Tanara VI', kind: 'planet', type: 'ice_giant', parent: 'star', orbitM: 3.3 * AU, phase: 0.6, inclination: 0.01,
      radiusM: 25200 * KM, palette: ['#5d87a8', '#a6c8de', '#eef6fb'], atmosphere: '#bfe2ff', bands: 0.75, tilt: 0.32, temperatureK: 92, gravityG: 1.31,
      rings: { innerM: 33000 * KM, outerM: 64000 * KM, color: '#d6dbe0', opacity: 0.85 },
      description: 'A pale ice giant whose methane-rich atmosphere fuels half the Reach. Its broad rings are mined for water ice.',
    },
    {
      id: 'orin', name: 'Orin', kind: 'moon', type: 'barren', parent: 'tanara', orbitM: 150000 * KM, phase: 3.32, inclination: 1.25,
      radiusM: 2600 * KM, palette: ['#55524f', '#8c8884', '#c8c4be'], atmosphere: null, temperatureK: 88, gravityG: 0.16,
      description: 'A cratered grey moon hosting the Combine deep-space listening array.',
    },
    {
      id: 'sil', name: 'Sil', kind: 'moon', type: 'ice', parent: 'tanara', orbitM: 240000 * KM, phase: 2.2, inclination: -0.1,
      radiusM: 900 * KM, palette: ['#9fb6c6', '#d6e6ef', '#ffffff'], atmosphere: null, temperatureK: 80, gravityG: 0.08,
      description: 'A small ice moon riddled with geysers.',
    },
    {
      id: 'vashti', name: 'Vashti', kind: 'planet', type: 'gas_giant', parent: 'star', orbitM: 8.6 * AU, phase: 4.2, inclination: 0.03,
      radiusM: 66000 * KM, palette: ['#7a5236', '#c79a68', '#f1e2c4'], atmosphere: '#f0d0a0', bands: 1, tilt: 0.08, temperatureK: 60, gravityG: 2.4,
      description: 'An amber gas giant with a storm system three times the width of Halcyon.',
    },
    {
      id: 'thule', name: 'Thule', kind: 'planet', type: 'ice', parent: 'star', orbitM: 19 * AU, phase: 3.3, inclination: 0.05,
      radiusM: 3100 * KM, palette: ['#8aa2b6', '#cadced', '#ffffff'], atmosphere: null, temperatureK: 41, gravityG: 0.3,
      description: 'A frozen outer world and the edge of Combine-patrolled space.',
    },
  ],
  stations: [
    {
      id: 'tanara-station', name: 'Tanara Station', kind: 'spire', parent: 'tanara', orbitM: 92000 * KM, phase: 4.9, inclination: 0.22,
      faction: 'Nexus Astra Combine', economy: 'High Tech',
      services: ['refuel', 'repair', 'market', 'cartographics', 'missions'],
      tagline: 'Trade makes a brighter tomorrow',
      description: 'Flagship trade spire of the Nexus Astra Combine. Forty thousand residents, three concourses and the busiest docking ring in the Reach.',
    },
    {
      id: 'halcyon-gateway', name: 'Halcyon Gateway', kind: 'ring', parent: 'halcyon', orbitM: 14000 * KM, phase: 1.9, inclination: 0.15,
      faction: 'Kelvar Directorate', economy: 'Tourism',
      services: ['refuel', 'repair', 'market', 'cartographics', 'missions'],
      tagline: 'Further, together',
      description: 'The great orbital ring above Halcyon — seat of the Kelvar Directorate and gateway to the capital.',
    },
    {
      id: 'ember-works', name: 'Ember Foundry', kind: 'outpost', parent: 'ember', orbitM: 6200 * KM, phase: 0.4, inclination: 0.1,
      faction: 'Nexus Astra Combine', economy: 'Refinery',
      services: ['refuel', 'repair', 'market'],
      tagline: 'Forged in fire',
      description: 'An automated refinery outpost skimming metals from the night side of Ember.',
    },
  ],
  belts: [
    { id: 'drift', name: 'Kelvar Drift', parent: 'star', radiusM: 5.4 * AU, widthM: 0.5 * AU, composition: 'metallic' },
  ],
  pois: [
    {
      id: 'calloway', name: 'Derelict Freighter Calloway', kind: 'derelict', parent: 'halcyon', offsetM: 40000 * KM, phase: 4.0,
      description: 'A Combine bulk hauler that went dark forty years ago. Its black box was never recovered.', reward: 4000,
    },
    {
      id: 'orin-signal', name: 'Orin Listening Array', kind: 'beacon', parent: 'orin', offsetM: 6000 * KM, phase: 1.2,
      description: 'Combine deep-space listening array. Something has been answering it.', reward: 2500,
    },
  ],
}
