import * as THREE from 'three'
import { Rng } from '../rng'

const cache = new Map<string, THREE.Texture>()

/** Grid of lit windows used on station hulls. */
export function windowTexture(seed: number, warm = true): THREE.CanvasTexture {
  const key = `win${seed}${warm}`
  if (cache.has(key)) return cache.get(key) as THREE.CanvasTexture
  const w = 512
  const h = 128
  const cv = document.createElement('canvas')
  cv.width = w
  cv.height = h
  const g = cv.getContext('2d')!
  g.fillStyle = '#000'
  g.fillRect(0, 0, w, h)
  const rng = new Rng(seed)
  const cols = 128
  const rows = 16
  const cw = w / cols
  const rh = h / rows
  for (let y = 0; y < rows; y++) {
    if (y % 4 === 3) continue
    for (let x = 0; x < cols; x++) {
      if (rng.next() < 0.42) continue
      const t = rng.next()
      const c = warm ? (t < 0.7 ? '255,190,110' : t < 0.9 ? '255,235,200' : '140,200,255') : t < 0.6 ? '150,210,255' : '235,245,255'
      g.fillStyle = `rgba(${c},${0.45 + rng.next() * 0.55})`
      g.fillRect(x * cw + 1, y * rh + 2, cw - 2, rh - 4)
    }
  }
  const tex = new THREE.CanvasTexture(cv)
  tex.colorSpace = THREE.SRGBColorSpace
  tex.wrapS = tex.wrapT = THREE.RepeatWrapping
  tex.anisotropy = 4
  cache.set(key, tex)
  return tex
}

/** Subtle hull panelling for metal surfaces. */
export function panelTexture(): THREE.CanvasTexture {
  const key = 'panel'
  if (cache.has(key)) return cache.get(key) as THREE.CanvasTexture
  const s = 512
  const cv = document.createElement('canvas')
  cv.width = cv.height = s
  const g = cv.getContext('2d')!
  const rng = new Rng(99)
  g.fillStyle = '#8a9099'
  g.fillRect(0, 0, s, s)
  for (let i = 0; i < 220; i++) {
    const x = Math.floor(rng.next() * 16) * 32
    const y = Math.floor(rng.next() * 16) * 32
    const w = 32 * rng.int(1, 4)
    const h = 32 * rng.int(1, 3)
    const v = 120 + rng.int(-25, 25)
    g.fillStyle = `rgb(${v},${v + 4},${v + 10})`
    g.fillRect(x, y, w, h)
    g.strokeStyle = 'rgba(30,34,40,0.6)'
    g.lineWidth = 2
    g.strokeRect(x + 1, y + 1, w - 2, h - 2)
  }
  const tex = new THREE.CanvasTexture(cv)
  tex.colorSpace = THREE.SRGBColorSpace
  tex.wrapS = tex.wrapT = THREE.RepeatWrapping
  cache.set(key, tex)
  return tex
}

/** Holographic station sign. */
export function signTexture(name: string, tagline: string): THREE.CanvasTexture {
  const w = 1024
  const h = 320
  const cv = document.createElement('canvas')
  cv.width = w
  cv.height = h
  const g = cv.getContext('2d')!
  g.clearRect(0, 0, w, h)
  g.fillStyle = 'rgba(20,40,70,0.35)'
  g.fillRect(0, 0, w, h)
  g.strokeStyle = 'rgba(140,200,255,0.8)'
  g.lineWidth = 3
  g.strokeRect(6, 6, w - 12, h - 12)
  // logo
  g.fillStyle = 'rgba(170,215,255,0.95)'
  g.beginPath()
  g.moveTo(110, 60)
  g.lineTo(190, 250)
  g.lineTo(150, 250)
  g.lineTo(110, 150)
  g.lineTo(70, 250)
  g.lineTo(30, 250)
  g.closePath()
  g.fill()
  g.font = '600 92px Rajdhani, "Exo 2", sans-serif'
  g.fillStyle = 'rgba(210,235,255,1)'
  g.fillText(name.toUpperCase().slice(0, 18), 230, 140)
  g.font = '500 44px Rajdhani, "Exo 2", sans-serif'
  g.fillStyle = 'rgba(160,205,245,0.95)'
  const words = tagline.toUpperCase().split(' ')
  let line = ''
  let y = 205
  for (const word of words) {
    if ((line + ' ' + word).length > 30) {
      g.fillText(line.trim(), 232, y)
      line = ''
      y += 50
    }
    line += ' ' + word
  }
  g.fillText(line.trim(), 232, y)
  const tex = new THREE.CanvasTexture(cv)
  tex.colorSpace = THREE.SRGBColorSpace
  return tex
}
