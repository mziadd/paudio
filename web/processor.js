const MAX_MS = 300;
const MAX_FRAMES = Math.floor(sampleRate * MAX_MS / 1000);

class PcmPlayer extends AudioWorkletProcessor {
  constructor() {
    super();
    this.chunks = [];
    this.pos = 0;

    this.port.onmessage = (e) => {
      const d = e.data;
      if (!d || d.type !== 'samples' || !(d.samples instanceof Float32Array)) return;
      this.chunks.push(d.samples);
      this.trim();
    };
  }

  framesQueued() {
    if (!this.chunks.length) return 0;
    let frames = (this.chunks[0].length - this.pos) / 2;
    for (let i = 1; i < this.chunks.length; i++) {
      frames += this.chunks[i].length / 2;
    }
    return frames;
  }

  trim() {
    while (this.framesQueued() > MAX_FRAMES && this.chunks.length > 1) {
      this.chunks.shift();
      this.pos = 0;
    }
  }

  process(_inputs, outputs) {
    const L = outputs[0][0];
    const R = outputs[0][1];
    const n = L.length;

    for (let i = 0; i < n; i++) {
      if (!this.chunks.length) {
        L[i] = 0;
        R[i] = 0;
        continue;
      }
      const c = this.chunks[0];
      L[i] = c[this.pos];
      R[i] = c[this.pos + 1];
      this.pos += 2;
      if (this.pos >= c.length) {
        this.chunks.shift();
        this.pos = 0;
      }
    }
    return true;
  }
}

registerProcessor('pcm-player', PcmPlayer);
