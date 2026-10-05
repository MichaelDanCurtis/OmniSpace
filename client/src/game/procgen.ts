// Procedural fallback generator. Used when the AI Stellar Forge is offline (no
// backend / no API key) so the game is always playable. Produces the same
// StarSystem shape the AI produces.

import { CLASS_LABEL, equilibriumTempK, frostLineAU, habitableZoneAU, HOME_ID, starSeed } from './galaxy'
import { ECONOMIES, FACTIONS, ROMAN } from './lore'
import { Rng, clamp } from './rng'
import { AU, KM, SOLAR_RADIUS, type Body, type Belt, type PlanetType, type POI, type StarDescriptor, type StarSystem, type Station } from './types'
import { KELVAR } from './kelvar'

const PALETTES: Record<PlanetType, string[][]> = {
  rocky: [['#5b524a', '#8a7d6e', '#b8ab98'], ['#4a4642', '#76706a', '#a8a29a']],
  barren: [['#3e3b39', '#6b6662', '#9c9590'], ['#55483e', '#7e6b5a', '#a99580']],
  desert: [['#7a4a25', '#c18a4d', '#e8c48a'], ['#6e3b2a', '#b5643e', '#e09a6a']],
  lava: [['#1a0d0a', '#3a1a12', '#ff5a1f'], ['#120c0c', '#2e1714', '#ffae3a']],
  ice: [['#8fa6b8', '#cfe0ea', '#ffffff'], ['#7a95ad', '#b8d0e4', '#eef6ff']],
  ocean: [['#0b2a4a', '#1d5c8a', '#6fb6d8'], ['#082640', '#185070', '#4fa0b8']],
  earthlike: [['#123d6b', '#3b7a3a', '#c9b98a'], ['#0e3560', '#4d7d3e', '#d6c79a']],
  jungle: [['#0f3a2a', '#2f6e2a', '#8ab04a'], ['#173a1a', '#3e7a2e', '#a0b85a']],
  toxic: [['#4a5a1a', '#9aa83a', '#e2d27a'], ['#3a4a2a', '#7a8a3a', '#cfd89a']],
  gas_giant: [['#7a5a3a', '#c8a070', '#efe0c0'], ['#6a3a2a', '#b8704a', '#f0c8a0'], ['#5a4a6a', '#a090b0', '#e8dff0']],
  ice_giant: [['#4a7a9a', '#8ab8d4', '#e0f0f8'], ['#3a5a8a', '#6f9ad0', '#cfe2f6'], ['#3a8a8a', '#7ac0c0', '#d8f4f0']],
}
const ATMOS: Partial<Record<PlanetType, string>> = {
  earthlike: '#6fb0ff',
  ocean: '#7ac4ff',
  jungle: '#8fd0a0',
  toxic: '#d8d070',
  gas_giant: '#e8c8a0',
  ice_giant: '#a8d8ff',
  desert: '#e8b080',
}

const RADIUS_KM: Record<PlanetType, [number, number]> = {
  rocky: [1800, 6500],
  barren: [900, 4500],
  desert: [2500, 7000],
  lava: [1500, 6000],
  ice: [900, 5000],
  ocean: [4500, 9000],
  earthlike: [5200, 7600],
  jungle: [5000, 7400],
  toxic: [4000, 8000],
  gas_giant: [38000, 78000],
  ice_giant: [17000, 30000],
}

const DESCRIPTIONS: Record<PlanetType, string[]> = {
  rocky: ['A cratered silicate world scarred by ancient impacts.', 'A rocky planet with long fault canyons and thin dusty air.'],
  barren: ['An airless, battered rock baking and freezing in turn.', 'A dead world of grey regolith and impact basins.'],
  desert: ['Dune seas and salt flats stretch between eroded mesas.', 'A rust-red desert world swept by planet-wide dust storms.'],
  lava: ['Tidally tortured, its crust splits into rivers of molten rock.', 'A volcanic hellscape glowing through cracks in a basalt crust.'],
  ice: ['A frozen ball of water ice with buried ocean signatures.', 'Glacial plains crisscrossed by blue pressure ridges.'],
  ocean: ['A world-spanning ocean beneath a heavy, humid sky.', 'Endless dark water with scattered volcanic archipelagos.'],
  earthlike: ['A rare temperate garden world with liquid oceans and living continents.', 'Blue seas, green continents and white weather — a jewel of a world.'],
  jungle: ['Hot, wet and smothered in tangled biomass from pole to pole.', 'A humid world of continent-sized rainforest.'],
  toxic: ['A choking sulfuric atmosphere hides a surface of acid lakes.', 'Yellow-green clouds of reactive gases churn over a hot crust.'],
  gas_giant: ['A banded gas giant with storms larger than terrestrial worlds.', 'A massive hydrogen giant with turbulent amber cloud belts.'],
  ice_giant: ['A pale ice giant wrapped in high methane haze.', 'A cold, blue giant of water, ammonia and methane ices.'],
}

const STATION_PREFIX = ['Tanara', 'Calloway', 'Meridian', 'Orison', 'Vance', 'Halden', 'Ostrova', 'Kepler', 'Sato', 'Amari', 'Ishikawa', 'Novak', 'Brightwater', 'Okoye', 'Lindqvist', 'Haldane', 'Ravel', 'Ferro', 'Celestine', 'Dawnward']
const STATION_SUFFIX = ['Station', 'Port', 'Hub', 'Terminal', 'Dock', 'Citadel', 'Exchange', 'Gateway', 'Spire', 'Anchorage']
const TAGLINES = ['Trade makes a brighter tomorrow', 'Further, together', 'The frontier starts here', 'Built by many hands', 'Safe harbour, fair prices', 'Discovery drives humanity forward', 'Ever outward']

function planetTypeFor(rng: Rng, tempK: number, au: number, frost: number): PlanetType {
  if (tempK > 1000) return 'lava'
  if (tempK > 650) return rng.pick(['lava', 'barren', 'toxic', 'rocky'] as const)
  if (tempK > 330) return rng.pick(['desert', 'rocky', 'toxic', 'barren', 'gas_giant'] as const)
  if (tempK > 190) {
    const r = rng.next()
    if (r < 0.14) return 'earthlike'
    if (r < 0.3) return 'ocean'
    if (r < 0.4) return 'jungle'
    if (r < 0.6) return 'desert'
    if (r < 0.8) return 'rocky'
    return 'gas_giant'
  }
  if (au < frost) return rng.pick(['rocky', 'ice', 'barren', 'desert'] as const)
  const r = rng.next()
  if (r < 0.38) return 'gas_giant'
  if (r < 0.68) return 'ice_giant'
  return rng.pick(['ice', 'ice', 'barren'] as const)
}

function jitterColor(hex: string, rng: Rng, amt = 0.12): string {
  const n = parseInt(hex.slice(1), 16)
  const f = (v: number) => clamp(Math.round(v * (1 + (rng.next() - 0.5) * amt * 2)), 0, 255).toString(16).padStart(2, '0')
  return `#${f((n >> 16) & 255)}${f((n >> 8) & 255)}${f(n & 255)}`
}

function planetCount(rng: Rng, star: StarDescriptor) {
  switch (star.cls) {
    case 'O':
    case 'B':
      return rng.int(0, 4)
    case 'L':
    case 'WD':
      return rng.int(0, 3)
    case 'NS':
      return rng.int(0, 2)
    case 'M':
      return rng.int(1, 6)
    default:
      return rng.int(2, 9)
  }
}

export function generateProcedural(star: StarDescriptor): StarSystem {
  if (star.id === HOME_ID) return structuredClone(KELVAR)
  const rng = new Rng(starSeed(star.id) ^ 0x51a7)
  const lum = star.luminosity
  const hz = habitableZoneAU(lum)
  const frost = frostLineAU(lum)
  const starRadiusAU = (star.radiusSolar * SOLAR_RADIUS) / AU
  const bodies: Body[] = []
  const belts: Belt[] = []
  const stations: Station[] = []
  const pois: POI[] = []

  const n = planetCount(rng, star)
  let au = Math.max(starRadiusAU * 6, 0.22 * Math.sqrt(Math.max(lum, 0.0004)) * rng.range(0.5, 1.5))
  au = Math.max(au, star.cls === 'NS' ? 0.3 : 0.02)
  let beltPlaced = false
  for (let i = 0; i < n; i++) {
    const temp = equilibriumTempK(lum, au)
    const type = planetTypeFor(rng, temp, au, frost)
    const [rmin, rmax] = RADIUS_KM[type]
    const radiusM = rng.range(rmin, rmax) * KM
    const giant = type === 'gas_giant' || type === 'ice_giant'
    const id = `p${i + 1}`
    const name = `${star.name} ${ROMAN[i]}`
    const palette = rng.pick(PALETTES[type]).map((c) => jitterColor(c, rng))
    const body: Body = {
      id,
      name,
      kind: 'planet',
      type,
      parent: 'star',
      orbitM: au * AU,
      phase: rng.range(0, Math.PI * 2),
      inclination: rng.range(-0.06, 0.06),
      radiusM,
      palette,
      atmosphere: ATMOS[type] ?? (rng.chance(0.3) && type !== 'barren' ? '#b0c0d0' : null),
      clouds: type === 'earthlike' || type === 'ocean' || type === 'jungle' ? rng.range(0.35, 0.7) : type === 'toxic' ? 0.9 : 0,
      bands: giant ? rng.range(0.4, 1) : 0,
      tilt: rng.range(-0.5, 0.5),
      rings: null,
      temperatureK: Math.round(temp),
      gravityG: Math.round((giant ? rng.range(1.1, 3.2) : (radiusM / 6.371e6) * rng.range(0.7, 1.1)) * 100) / 100,
      description: rng.pick(DESCRIPTIONS[type]),
    }
    if (giant && rng.chance(0.45)) {
      const inner = radiusM * rng.range(1.25, 1.6)
      body.rings = {
        innerM: inner,
        outerM: inner + radiusM * rng.range(0.6, 1.6),
        color: rng.pick(['#d8ccb8', '#b8b0a8', '#c8d8e8', '#a89888']),
        opacity: rng.range(0.45, 0.85),
      }
    }
    bodies.push(body)
    // moons
    const moonCount = giant ? rng.int(1, 4) : radiusM > 4e6 && rng.chance(0.35) ? 1 : 0
    let mo = (body.rings ? body.rings.outerM : radiusM) * rng.range(2.2, 3.5)
    for (let m = 0; m < moonCount; m++) {
      const mtype: PlanetType = temp < 200 ? rng.pick(['ice', 'barren', 'rocky'] as const) : rng.pick(['barren', 'rocky', 'lava'] as const)
      const [mr0, mr1] = RADIUS_KM[mtype]
      const mr = Math.min(rng.range(mr0 * 0.5, mr1 * 0.6) * KM, radiusM * 0.35)
      bodies.push({
        id: `${id}m${m + 1}`,
        name: `${name} ${String.fromCharCode(97 + m)}`,
        kind: 'moon',
        type: mtype,
        parent: id,
        orbitM: mo,
        phase: rng.range(0, Math.PI * 2),
        inclination: rng.range(-0.15, 0.15),
        radiusM: mr,
        palette: rng.pick(PALETTES[mtype]).map((c) => jitterColor(c, rng)),
        atmosphere: null,
        temperatureK: Math.round(temp),
        gravityG: Math.round((mr / 6.371e6) * 0.8 * 100) / 100,
        description: rng.pick(DESCRIPTIONS[mtype]),
      })
      mo *= rng.range(1.5, 2.2)
    }
    const nextAu = au * rng.range(1.45, 2.1)
    if (!beltPlaced && i > 0 && rng.chance(0.35)) {
      const bau = (au + nextAu) / 2
      belts.push({ id: `belt${belts.length + 1}`, name: `${star.name} Belt`, parent: 'star', radiusM: bau * AU, widthM: bau * AU * 0.12, composition: rng.pick(['metallic', 'rocky', 'icy']) })
      beltPlaced = true
    }
    au = nextAu
  }

  // settlement & stations
  const settle = star.settlement
  let stationCount = 0
  if (settle > 0.45) stationCount = rng.int(1, 3)
  else if (settle > 0.2) stationCount = rng.chance(0.75) ? 1 : 0
  else if (settle > 0.02) stationCount = rng.chance(0.25) ? 1 : 0
  const hosts = bodies.filter((b) => b.kind === 'planet')
  const faction = settle > 0.6 ? rng.pick(['Nexus Astra Combine', 'Kelvar Directorate', 'Free Worlds Accord'] as const) : settle > 0.2 ? rng.pick(['Free Worlds Accord', 'Ashkar Covenant', 'Independent', 'Meridian Survey Corps', 'Veyl Syndicate'] as const) : settle > 0.02 ? rng.pick(['Independent', 'Meridian Survey Corps', 'Veyl Syndicate'] as const) : 'Unclaimed'
  const economy = rng.pick(ECONOMIES)
  for (let s = 0; s < Math.min(stationCount, hosts.length); s++) {
    const host = hosts[(s * 3 + rng.int(0, hosts.length - 1)) % hosts.length]
    if (stations.some((st) => st.parent === host.id)) continue
    const outer = host.rings ? host.rings.outerM : host.radiusM
    const kind = settle > 0.45 ? rng.pick(['spire', 'ring', 'spire'] as const) : 'outpost'
    stations.push({
      id: `st${s + 1}`,
      name: `${rng.pick(STATION_PREFIX)} ${rng.pick(STATION_SUFFIX)}`,
      kind,
      parent: host.id,
      orbitM: outer + host.radiusM * rng.range(0.8, 1.6),
      phase: rng.range(0, Math.PI * 2),
      inclination: rng.range(0.1, 0.3),
      faction,
      economy: s === 0 ? economy : rng.pick(ECONOMIES),
      services: kind === 'outpost' ? ['refuel', 'repair', 'market', 'cartographics'] : ['refuel', 'repair', 'market', 'cartographics', 'missions'],
      tagline: rng.pick(TAGLINES),
      description: kind === 'outpost' ? `A small ${faction === 'Unclaimed' ? 'independent' : faction} outpost clinging to the edge of known space.` : `A busy ${economy.toLowerCase()} station serving the ${star.name} system.`,
    })
  }

  // points of interest
  const poiCount = rng.int(0, 2)
  for (let i = 0; i < poiCount && hosts.length; i++) {
    const host = rng.pick(hosts)
    const kind = rng.pick(['derelict', 'signal', 'anomaly', 'wreck', 'beacon'] as const)
    pois.push({
      id: `poi${i + 1}`,
      name: kind === 'derelict' ? `Derelict ${rng.pick(['Freighter', 'Survey Ship', 'Hauler', 'Liner'])}` : kind === 'signal' ? 'Unidentified Signal Source' : kind === 'anomaly' ? 'Gravitational Anomaly' : kind === 'wreck' ? 'Wreckage Field' : 'Nav Beacon',
      kind,
      parent: host.id,
      offsetM: host.radiusM * rng.range(2, 5),
      phase: rng.range(0, Math.PI * 2),
      description: 'Sensors register an unexplained contact here.',
      reward: rng.int(2, 12) * 500,
    })
  }

  const pop = stationCount ? Math.round(Math.pow(10, rng.range(4, 9.5) * settle + 3)) : 0
  return {
    id: star.id,
    name: star.name,
    source: 'procedural',
    star,
    starFlavor: CLASS_LABEL[star.cls],
    profile: {
      description: bodies.length
        ? `${CLASS_LABEL[star.cls]} with ${hosts.length} planet${hosts.length === 1 ? '' : 's'}${stations.length ? ` and ${stations.length} station${stations.length === 1 ? '' : 's'}` : ''}.`
        : `A lonely ${CLASS_LABEL[star.cls].toLowerCase()} with no planetary bodies detected.`,
      lore: stations.length ? `${faction} interests hold sway here.` : 'No recorded human presence.',
      faction: stations.length ? faction : 'Unclaimed',
      government: stations.length ? rng.pick(['Corporate', 'Democracy', 'Cooperative', 'Patronage', 'Anarchy']) : 'None',
      economy: stations.length ? economy : 'None',
      security: settle > 0.6 ? 'high' : settle > 0.3 ? 'medium' : settle > 0.05 ? 'low' : 'none',
      population: pop,
      danger: clamp(Math.round((1 - settle) * 3 + rng.range(0, 2)), 0, 5),
    },
    bodies,
    stations,
    belts,
    pois,
  }
}

export { FACTIONS }
