// Fully procedural WebAudio soundscape: engine drone, supercruise roar, FSD
// charge, hyperspace, UI blips and alarms. No audio files required.

export class Audio {
  ctx: AudioContext | null = null
  private master!: GainNode
  private engGain!: GainNode
  private engFilter!: BiquadFilterNode
  private oscA!: OscillatorNode
  private oscB!: OscillatorNode
  private noiseGain!: GainNode
  private noiseFilter!: BiquadFilterNode
  private chargeOsc: OscillatorNode | null = null
  private chargeGain: GainNode | null = null
  private ambGain!: GainNode
  muted = false
  volume = 0.7

  init() {
    if (this.ctx) return
    try {
      this.ctx = new AudioContext()
    } catch {
      return
    }
    const ctx = this.ctx
    this.master = ctx.createGain()
    this.master.gain.value = this.volume
    this.master.connect(ctx.destination)

    this.engFilter = ctx.createBiquadFilter()
    this.engFilter.type = 'lowpass'
    this.engFilter.frequency.value = 300
    this.engGain = ctx.createGain()
    this.engGain.gain.value = 0
    this.oscA = ctx.createOscillator()
    this.oscA.type = 'sawtooth'
    this.oscA.frequency.value = 46
    this.oscB = ctx.createOscillator()
    this.oscB.type = 'sawtooth'
    this.oscB.frequency.value = 46.7
    this.oscA.connect(this.engFilter)
    this.oscB.connect(this.engFilter)
    this.engFilter.connect(this.engGain)
    this.engGain.connect(this.master)
    this.oscA.start()
    this.oscB.start()

    const buf = ctx.createBuffer(1, ctx.sampleRate * 2, ctx.sampleRate)
    const d = buf.getChannelData(0)
    let last = 0
    for (let i = 0; i < d.length; i++) {
      last = last * 0.97 + (Math.random() * 2 - 1) * 0.03
      d[i] = last * 6
    }
    const noise = ctx.createBufferSource()
    noise.buffer = buf
    noise.loop = true
    this.noiseFilter = ctx.createBiquadFilter()
    this.noiseFilter.type = 'bandpass'
    this.noiseFilter.frequency.value = 200
    this.noiseFilter.Q.value = 0.7
    this.noiseGain = ctx.createGain()
    this.noiseGain.gain.value = 0
    noise.connect(this.noiseFilter)
    this.noiseFilter.connect(this.noiseGain)
    this.noiseGain.connect(this.master)
    noise.start()

    // ambient cockpit hum
    const amb = ctx.createOscillator()
    amb.type = 'sine'
    amb.frequency.value = 58
    this.ambGain = ctx.createGain()
    this.ambGain.gain.value = 0.025
    amb.connect(this.ambGain)
    this.ambGain.connect(this.master)
    amb.start()
  }

  setMuted(m: boolean) {
    this.muted = m
    if (this.master) this.master.gain.value = m ? 0 : this.volume
  }

  /** mode: 0 normal, 1 supercruise, 2 hyperspace */
  update(throttle: number, boost: boolean, mode: number, speedNorm: number) {
    if (!this.ctx) return
    const t = this.ctx.currentTime
    const thr = Math.abs(throttle)
    const eg = mode === 2 ? 0.05 : 0.045 + thr * 0.07 + (boost ? 0.06 : 0)
    this.engGain.gain.setTargetAtTime(eg, t, 0.15)
    this.engFilter.frequency.setTargetAtTime(160 + thr * 520 + (boost ? 700 : 0) + mode * 200, t, 0.2)
    const f = 42 + thr * 30 + (boost ? 18 : 0)
    this.oscA.frequency.setTargetAtTime(f, t, 0.3)
    this.oscB.frequency.setTargetAtTime(f * 1.013, t, 0.3)
    const ng = mode === 2 ? 0.55 : mode === 1 ? 0.12 + speedNorm * 0.2 : boost ? 0.12 : 0.02 + thr * 0.03
    this.noiseGain.gain.setTargetAtTime(ng, t, 0.3)
    this.noiseFilter.frequency.setTargetAtTime(mode === 2 ? 120 : mode === 1 ? 180 + speedNorm * 400 : 400, t, 0.3)
  }

  private tone(freq: number, dur: number, type: OscillatorType = 'sine', vol = 0.15, slideTo?: number) {
    if (!this.ctx) return
    const ctx = this.ctx
    const o = ctx.createOscillator()
    const g = ctx.createGain()
    o.type = type
    o.frequency.value = freq
    if (slideTo) o.frequency.exponentialRampToValueAtTime(slideTo, ctx.currentTime + dur)
    g.gain.setValueAtTime(0, ctx.currentTime)
    g.gain.linearRampToValueAtTime(vol, ctx.currentTime + 0.01)
    g.gain.exponentialRampToValueAtTime(0.0001, ctx.currentTime + dur)
    o.connect(g)
    g.connect(this.master)
    o.start()
    o.stop(ctx.currentTime + dur + 0.05)
  }

  blip() {
    this.tone(1320, 0.08, 'sine', 0.08)
  }
  confirm() {
    this.tone(880, 0.09, 'triangle', 0.1)
    setTimeout(() => this.tone(1320, 0.12, 'triangle', 0.1), 90)
  }
  deny() {
    this.tone(220, 0.25, 'square', 0.06)
  }
  alarm() {
    this.tone(660, 0.18, 'square', 0.05, 440)
  }
  clunk() {
    this.tone(90, 0.4, 'triangle', 0.35, 40)
  }
  impact() {
    this.tone(70, 0.5, 'sawtooth', 0.3, 30)
  }
  scan() {
    this.tone(500, 0.6, 'sine', 0.08, 2200)
  }

  startCharge(dur: number) {
    if (!this.ctx) return
    this.stopCharge()
    const ctx = this.ctx
    const o = ctx.createOscillator()
    const g = ctx.createGain()
    o.type = 'sawtooth'
    o.frequency.value = 60
    o.frequency.exponentialRampToValueAtTime(420, ctx.currentTime + dur)
    g.gain.value = 0
    g.gain.linearRampToValueAtTime(0.07, ctx.currentTime + dur)
    const f = ctx.createBiquadFilter()
    f.type = 'lowpass'
    f.frequency.value = 900
    o.connect(f)
    f.connect(g)
    g.connect(this.master)
    o.start()
    this.chargeOsc = o
    this.chargeGain = g
  }
  stopCharge() {
    if (this.chargeOsc && this.ctx) {
      this.chargeGain?.gain.setTargetAtTime(0, this.ctx.currentTime, 0.05)
      this.chargeOsc.stop(this.ctx.currentTime + 0.2)
    }
    this.chargeOsc = null
    this.chargeGain = null
  }
  whoosh() {
    if (!this.ctx) return
    this.tone(80, 1.6, 'sawtooth', 0.2, 30)
    this.tone(2400, 1.2, 'sine', 0.04, 200)
  }
}

export const audio = new Audio()
