// Keyboard + mouse input. The mouse drives a virtual joystick (pointer lock).

export class Input {
  keys = new Set<string>()
  stick = { x: 0, y: 0 }
  mouseLocked = false
  invertY = false
  sensitivity = 1
  wheel = 0
  private canvas: HTMLElement
  private onAction: (code: string, e: KeyboardEvent) => void
  private handlers: [EventTarget, string, EventListener][] = []

  constructor(canvas: HTMLElement, onAction: (code: string, e: KeyboardEvent) => void) {
    this.canvas = canvas
    this.onAction = onAction
    const on = (t: EventTarget, ev: string, fn: EventListener, opts?: AddEventListenerOptions) => {
      t.addEventListener(ev, fn, opts)
      this.handlers.push([t, ev, fn])
    }
    on(window, 'keydown', ((e: KeyboardEvent) => {
      const el = e.target as HTMLElement
      if (el && (el.tagName === 'INPUT' || el.tagName === 'TEXTAREA')) return
      if (e.code === 'Tab' || e.code === 'Space') e.preventDefault()
      if (!this.keys.has(e.code)) this.onAction(e.code, e)
      this.keys.add(e.code)
    }) as EventListener)
    on(window, 'keyup', ((e: KeyboardEvent) => void this.keys.delete(e.code)) as unknown as EventListener)
    on(window, 'blur', () => this.keys.clear())
    on(canvas, 'click', () => {
      if (!this.mouseLocked) canvas.requestPointerLock?.()
    })
    on(document, 'pointerlockchange', () => {
      this.mouseLocked = document.pointerLockElement === canvas
      if (!this.mouseLocked) this.stick = { x: 0, y: 0 }
    })
    on(document, 'mousemove', ((e: MouseEvent) => {
      if (!this.mouseLocked) return
      const k = 0.0032 * this.sensitivity
      this.stick.x = Math.max(-1, Math.min(1, this.stick.x + e.movementX * k))
      this.stick.y = Math.max(-1, Math.min(1, this.stick.y + e.movementY * k * (this.invertY ? -1 : 1)))
    }) as EventListener)
    on(canvas, 'wheel', ((e: WheelEvent) => {
      this.wheel += e.deltaY < 0 ? 1 : -1
    }) as EventListener, { passive: true })
  }

  releaseMouse() {
    if (document.pointerLockElement) document.exitPointerLock?.()
  }

  down(code: string) {
    return this.keys.has(code)
  }

  axis(neg: string[], pos: string[]) {
    let v = 0
    if (neg.some((k) => this.keys.has(k))) v -= 1
    if (pos.some((k) => this.keys.has(k))) v += 1
    return v
  }

  /** Mouse stick slowly self-centres so it behaves like relative aim. */
  decay(dt: number) {
    const k = Math.exp(-dt * 1.6)
    this.stick.x *= k
    this.stick.y *= k
    if (Math.abs(this.stick.x) < 0.01) this.stick.x = 0
    if (Math.abs(this.stick.y) < 0.01) this.stick.y = 0
  }

  dispose() {
    for (const [t, ev, fn] of this.handlers) t.removeEventListener(ev, fn)
    this.handlers = []
  }
}
