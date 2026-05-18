import { kSampleRateHz, kChunkBytes } from './format.js';

const $ = (id) => document.getElementById(id);
const playBtn = $('play');
const statusEl = $('status');
const dot = $('status-dot');
const meterBar = $('meter-bar');
const volumeSldr = $('volume');
const mediaOutEl = $('media-out');

const WS_TIMEOUT = 8000;
const AUDIO_TIMEOUT = 12000;
const SILENT_WAV =
  'data:audio/wav;base64,UklGRiQAAABXQVZFZm10IBAAAAABAAEARKwAAIhYAQACABAAZGF0YQAAAAA=';

let socket = null;
let audioCtx = null;
let gainNode = null;
let workletNode = null;
let scriptNode = null;
let mediaDest = null;
let keepAliveEl = null;
let bgTimer = null;

let isListening = false;
let isStarting = false;
let audioReady = false;
let workletLoaded = false;
let pending = [];

const isMobile = () =>
  /Android|iPhone|iPad|iPod/i.test(navigator.userAgent) ||
  (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1);

const host = () => {
  const h = location.hostname;
  return !h || h === '::1' ? '127.0.0.1' : h;
};

const wsUrl = () => `ws://${host()}:9000`;

function setStatus(kind, text) {
  if (statusEl) statusEl.textContent = text;
  if (dot) dot.dataset.kind = kind;
}

function refreshUI() {
  if (playBtn) {
    playBtn.textContent = isListening || isStarting ? 'Stop' : 'Listen';
    playBtn.classList.toggle('active', isListening);
    playBtn.disabled = false;
  }
  setStatus(isListening ? 'live' : isStarting ? 'busy' : 'idle',
    isListening ? 'Listening' : isStarting ? 'Connecting...' : 'Ready');
}

function tryPlay(el) {
  if (!el) return;
  const p = el.play();
  if (p?.catch) p.catch(() => {});
}

function withTimeout(promise, ms, label) {
  return Promise.race([
    promise,
    new Promise((_, r) => setTimeout(() => r(new Error(`${label} timeout`)), ms)),
  ]);
}

function resampleStereo(input, fromRate, toRate) {
  if (fromRate === toRate || input.length < 2) return input;
  const inF = input.length / 2;
  const outF = Math.max(1, Math.round(inF * toRate / fromRate));
  const out = new Float32Array(outF * 2);
  for (let i = 0; i < outF; i++) {
    const sp = i * (fromRate / toRate);
    const i0 = Math.floor(sp);
    const f = sp - i0;
    const i1 = Math.min(i0 + 1, inF - 1);
    for (let ch = 0; ch < 2; ch++) {
      out[i * 2 + ch] = input[i0 * 2 + ch] + (input[i1 * 2 + ch] - input[i0 * 2 + ch]) * f;
    }
  }
  return out;
}

function pushSamples(samples) {
  if (!isListening && !isStarting) return;
  if (!audioReady) {
    pending.push(samples);
    if (pending.length > 24) pending.shift();
    return;
  }
  let s = samples;
  if (audioCtx?.sampleRate !== kSampleRateHz) {
    s = resampleStereo(samples, kSampleRateHz, audioCtx.sampleRate);
  }
  if (workletNode) {
    workletNode.port.postMessage({ type: 'samples', samples: s.slice() });
  } else if (scriptNode?._enqueue) {
    scriptNode._enqueue(s);
  }
}

function updateMeter(samples) {
  if (!meterBar || !isListening) return;
  let sum = 0;
  for (let i = 0; i < samples.length; i++) sum += samples[i] * samples[i];
  meterBar.style.width = Math.min(100, Math.sqrt(sum / samples.length) * 400) + '%';
}

async function loadWorklet() {
  if (workletLoaded) return;
  await audioCtx.audioWorklet.addModule(new URL('processor.js', import.meta.url).href);
  workletLoaded = true;
}

function buildScriptFallback() {
  scriptNode = audioCtx.createScriptProcessor(2048, 0, 2);
  const q = [];
  let pos = 0;
  scriptNode.onaudioprocess = (e) => {
    const L = e.outputBuffer.getChannelData(0);
    const R = e.outputBuffer.getChannelData(1);
    for (let i = 0; i < L.length; i++) {
      if (!q.length) { L[i] = R[i] = 0; continue; }
      const c = q[0];
      L[i] = c[pos];
      R[i] = c[pos + 1];
      pos += 2;
      if (pos >= c.length) { q.shift(); pos = 0; }
    }
  };
  scriptNode.connect(gainNode);
  scriptNode._enqueue = (s) => q.push(s);
}

async function buildAudioGraph() {
  if (audioReady) return;

  gainNode = audioCtx.createGain();
  gainNode.gain.value = volumeSldr ? Number(volumeSldr.value) : 1;

  try {
    await loadWorklet();
    workletNode = new AudioWorkletNode(audioCtx, 'pcm-player', { outputChannelCount: [2] });
    workletNode.connect(gainNode);
  } catch {
    buildScriptFallback();
  }

  if (mediaOutEl) {
    mediaDest = audioCtx.createMediaStreamDestination();
    gainNode.connect(mediaDest);
    mediaOutEl.srcObject = mediaDest.stream;
    tryPlay(mediaOutEl);
  }
  if (!isMobile()) gainNode.connect(audioCtx.destination);

  keepAliveEl = new Audio(SILENT_WAV);
  keepAliveEl.loop = true;
  keepAliveEl.volume = 0.001;
  tryPlay(keepAliveEl);

  if ('mediaSession' in navigator) {
    navigator.mediaSession.metadata = new MediaMetadata({ title: 'PocketAudio', artist: 'PC audio' });
    navigator.mediaSession.playbackState = 'playing';
  }

  if (audioCtx.state === 'suspended') await audioCtx.resume();
  audioReady = true;
  const q = pending;
  pending = [];
  for (const s of q) pushSamples(s);
}

function resetAudio() {
  audioReady = false;
  try { workletNode?.disconnect(); } catch {}
  try { scriptNode?.disconnect(); } catch {}
  try { gainNode?.disconnect(); } catch {}
  workletNode = scriptNode = gainNode = mediaDest = null;
  if (mediaOutEl) { mediaOutEl.pause(); mediaOutEl.srcObject = null; }
  if (keepAliveEl) { keepAliveEl.pause(); keepAliveEl = null; }
  pending = [];
  if (meterBar) meterBar.style.width = '0%';
}

function closeSocket() {
  if (!socket) return;
  const s = socket;
  socket = null;
  s.onopen = s.onclose = s.onerror = s.onmessage = null;
  try { s.close(); } catch {}
}

function openSocket() {
  return new Promise((resolve, reject) => {
    closeSocket();
    let done = false;
    const timer = setTimeout(() => {
      if (done) return;
      done = true;
      closeSocket();
      reject(new Error('ws timeout'));
    }, WS_TIMEOUT);

    const finish = (err) => {
      if (done) return;
      done = true;
      clearTimeout(timer);
      err ? reject(err) : resolve();
    };

    socket = new WebSocket(wsUrl());
    socket.binaryType = 'arraybuffer';
    socket.onopen = () => {
      socket.onmessage = (e) => {
        if (!isListening && !isStarting) return;
        const buf = e.data;
        if (!(buf instanceof ArrayBuffer)) return;
        const n = kChunkBytes / 4;
        const end = buf.byteLength - (buf.byteLength % kChunkBytes);
        for (let off = 0; off < end; off += kChunkBytes) {
          const samples = new Float32Array(n);
          samples.set(new Float32Array(buf, off, n));
          updateMeter(samples);
          pushSamples(samples);
        }
      };
      finish(null);
    };
    socket.onerror = () => finish(new Error('ws error'));
    socket.onclose = () => {
      const wasOpen = done;
      closeSocket();
      if (!wasOpen) return finish(new Error('ws closed'));
      if (isListening) {
        isListening = isStarting = false;
        stopBgTimer();
        resetAudio();
        refreshUI();
        setStatus('error', 'Disconnected');
      }
    };
  });
}

function resumePlayback() {
  if (!isListening) return;
  if (audioCtx?.state === 'suspended') audioCtx.resume().catch(() => {});
  tryPlay(mediaOutEl);
  tryPlay(keepAliveEl);
  if (isMobile() && (!socket || socket.readyState !== WebSocket.OPEN)) {
    openSocket().catch(() => {});
  }
}

function startBgTimer() {
  stopBgTimer();
  bgTimer = setInterval(resumePlayback, 2000);
}

function stopBgTimer() {
  if (bgTimer) { clearInterval(bgTimer); bgTimer = null; }
}

async function startListening() {
  if (isListening || isStarting) return;
  if (location.protocol === 'file:') {
    setStatus('error', 'Serve web/ over http — e.g. python3 -m http.server 8080');
    return;
  }

  isStarting = true;
  pending = [];
  refreshUI();

  try {
    if (!audioCtx) {
      const opts = { latencyHint: 'playback' };
      if (!isMobile()) opts.sampleRate = kSampleRateHz;
      audioCtx = new AudioContext(opts);
    }
    if (audioCtx.state === 'suspended') await audioCtx.resume();

    await withTimeout(openSocket(), WS_TIMEOUT, 'ws');
    if (!isStarting) return;

    await withTimeout(buildAudioGraph(), AUDIO_TIMEOUT, 'audio');
    if (!isStarting) return;

    isListening = true;
    isStarting = false;
    startBgTimer();
    refreshUI();
  } catch (err) {
    isListening = isStarting = false;
    closeSocket();
    resetAudio();
    const m = String(err?.message || err);
    setStatus('error', m.includes('ws')
      ? "Can't connect — is pocket-audio-server running?"
      : 'Audio failed — tap Listen again');
    refreshUI();
  }
}

function stopListening() {
  if (!isListening && !isStarting) return;
  isListening = isStarting = false;
  stopBgTimer();
  closeSocket();
  resetAudio();
  refreshUI();
}

if (volumeSldr) {
  volumeSldr.addEventListener('input', () => {
    if (gainNode) gainNode.gain.value = Number(volumeSldr.value);
  });
}

if (playBtn) {
  playBtn.addEventListener('click', () => {
    isListening || isStarting ? stopListening() : startListening();
  });
}

const onWake = () => { if (isListening) resumePlayback(); };
document.addEventListener('visibilitychange', onWake);
window.addEventListener('pageshow', onWake);

refreshUI();
