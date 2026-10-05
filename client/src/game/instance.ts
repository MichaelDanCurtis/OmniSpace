import type { Game } from './engine'

let current: Game | null = null
export function setGame(g: Game | null) {
  current = g
}
export function game(): Game | null {
  return current
}
