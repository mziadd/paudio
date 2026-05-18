const MAX_FRAMES = Math.floor((sampleRate * 300) / 1000);

class PcmPlayer extends AudioWorkletProcessor {
  constructor() {
    super();
    this.chunks = [];
    this.pos = 0;
    this.port.onmessage = (e) => {
      const s = e.data?.samples;
      if (s instanceof Float32Array) {
        this.chunks.push(s);
        while (this.framesQueued() > MAX_FRAMES && this.chunks.length > 1) {
          this.chunks.shift();
          this.pos = 0;
        }
      }
    };
  }

  framesQueued() {
    if (!this.chunks.length) return 0;
    let n = (this.chunks[0].length - this.pos) / 2;
    for (let i = 1; i < this.chunks.length; i++) n += this.chunks[i].length / 2;
    return n;
  }

  process(_in, out) {
    const L = out[0][0];
    const R = out[0][1];
    for (let i = 0; i < L.length; i++) {
      if (!this.chunks.length) {
        L[i] = R[i] = 0;
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

registerProcessor("pcm-player", PcmPlayer);
