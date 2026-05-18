// PocketAudio — Listen only (stream PC speakers to phone/browser).

import { kSampleRateHz, kChunkBytes } from './format.js';

const $ = (id) => document.getElementById(id);
const playBtn    = $('play');
const statusEl   = $('status');
const dot        = $('status-dot');
const meterBar   = $('meter-bar');
const volumeSldr = $('volume');
const mediaOutEl = $('media-out');

const WS_TIMEOUT_MS    = 8000;
const AUDIO_TIMEOUT_MS = 12000;
const BG_INTERVAL_MS   = 2000;
const PENDING_MAX      = 24;
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

let isListening = false;   // fully connected + audio playing
let isStarting  = false;   // mid-startup (so Stop can cancel)
let audioReady  = false;
let workletModuleReady = false;
let pending = [];

function isIOS() {
  return (
    /iPad|iPhone|iPod/.test(navigator.userAgent) ||
    (navigator.platform === 'MacIntel' && navigator.maxTouchPoints > 1)
  );
}

function isAndroid() {
  return /Android/i.test(navigator.userAgent);
}

// iOS + Android: Web Audio suspends in background — route via <audio> element.
function isMobile() {
  return isIOS() || isAndroid();
}

function pageHost() {
  const h = location.hostname;
  return (!h || h === '::1') ? '127.0.0.1' : h;
}

function wsUrl() {
  const host = pageHost();
  if (location.protocol === 'https:') {
    return `wss://${host}:${location.port || '443'}/ws`;
  }
  return `ws://${host}:9000`;
}

function setStatus(kind, text) {
  if (statusEl) statusEl.textContent = text;
  if (dot) dot.dataset.kind = kind;
}

function refreshUI() {
  if (playBtn) {
    playBtn.textContent = (isListening || isStarting) ? 'Stop' : 'Listen';
    playBtn.classList.toggle('active', isListening);
    playBtn.disabled = false;
  }
  if (isListening) setStatus('live', 'Listening');
  else if (!isStarting) setStatus('idle', 'Ready');
}

function acceptingAudio() {
  return isListening || isStarting;
}

function withTimeout(promise, ms, label) {
  return Promise.race([
    promise,
    new Promise((_, reject) => {
      setTimeout(() => reject(new Error(`${label} timeout`)), ms);
    }),
  ]);
}

// Never await play() — on iOS it can hang forever outside a gesture.
function tryPlay(el) {
  if (!el) return;
  const p = el.play();
  if (p && typeof p.catch === 'function') p.catch(() => {});
}

// Safari 18+ / some mobile browsers: declare playback (not ambient) session.
function setupAudioSession() {
  if ('audioSession' in navigator) {
    try { navigator.audioSession.type = 'playback'; } catch (_) { /* ignore */ }
  }
}

// Lock-screen controls + background eligibility on mobile.
function setupMediaSession() {
  if (!('mediaSession' in navigator)) return;
  navigator.mediaSession.metadata = new MediaMetadata({
    title: 'PocketAudio',
    artist: 'Computer audio',
  });
  navigator.mediaSession.playbackState = 'playing';
  const noop = () => { resumePlayback(); };
  try { navigator.mediaSession.setActionHandler('play', noop); } catch (_) { /* ignore */ }
  try { navigator.mediaSession.setActionHandler('pause', noop); } catch (_) { /* ignore */ }
}

// Nudge audio + socket when tab goes to background or returns.
function resumePlayback() {
  if (!isListening) return;

  setupAudioSession();
  if ('mediaSession' in navigator) {
    navigator.mediaSession.playbackState = 'playing';
  }

  if (audioCtx && audioCtx.state === 'suspended') {
    audioCtx.resume().catch(() => {});
  }
  tryPlay(mediaOutEl);
  tryPlay(keepAliveEl);

  // Mobile browsers often kill WebSocket in background — reconnect when we wake up.
  if (isMobile() && (!socket || socket.readyState !== WebSocket.OPEN)) {
    openSocket().catch((err) => console.warn('background reconnect:', err));
  }
}

function resampleStereo(input, fromRate, toRate) {
  if (fromRate === toRate || input.length < 2) return input;
  const inFrames = input.length / 2;
  const outFrames = Math.max(1, Math.round(inFrames * toRate / fromRate));
  const out = new Float32Array(outFrames * 2);
  for (let i = 0; i < outFrames; i++) {
    const src = i * (fromRate / toRate);
    const i0 = Math.floor(src);
    const frac = src - i0;
    const i1 = Math.min(i0 + 1, inFrames - 1);
    for (let ch = 0; ch < 2; ch++) {
      out[i * 2 + ch] =
        input[i0 * 2 + ch] + (input[i1 * 2 + ch] - input[i0 * 2 + ch]) * frac;
    }
  }
  return out;
}

function pushSamples(samples) {
  if (!acceptingAudio()) return;
  if (!audioReady) {
    pending.push(samples);
    while (pending.length > PENDING_MAX) pending.shift();
    return;
  }
  let s = samples;
  if (audioCtx && audioCtx.sampleRate !== kSampleRateHz) {
    s = resampleStereo(samples, kSampleRateHz, audioCtx.sampleRate);
  }
  if (workletNode) {
    workletNode.port.postMessage({ type: 'samples', samples: s.slice() });
  } else if (scriptNode?._enqueue) {
    scriptNode._enqueue(s);
  }
}

function updateMeter(samples) {
  if (!meterBar || !acceptingAudio()) return;
  let sum = 0;
  for (let i = 0; i < samples.length; i++) sum += samples[i] * samples[i];
  const rms = Math.sqrt(sum / samples.length);
  meterBar.style.width = Math.min(100, rms * 400) + '%';
}

async function ensureWorkletModule() {
  if (workletModuleReady) return;
  const url = new URL('processor.js', import.meta.url).href;
  await audioCtx.audioWorklet.addModule(url);
  workletModuleReady = true;
}

function buildScriptProcessorFallback() {
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
  gainNode.gain.value = volumeSldr ? Number(volumeSldr.value) : 1.0;

  try {
    await ensureWorkletModule();
    workletNode = new AudioWorkletNode(audioCtx, 'pcm-player', {
      outputChannelCount: [2],
    });
    workletNode.connect(gainNode);
  } catch (err) {
    console.warn('AudioWorklet failed, using ScriptProcessor:', err);
    buildScriptProcessorFallback();
  }

  // Mobile (iOS + Android): playback must go through <audio> for background/lock screen.
  // Desktop: also use speakers via Web Audio destination.
  if (mediaOutEl) {
    mediaDest = audioCtx.createMediaStreamDestination();
    gainNode.connect(mediaDest);
    mediaOutEl.srcObject = mediaDest.stream;
    mediaOutEl.muted = false;
    mediaOutEl.volume = 1;
    mediaOutEl.setAttribute('playsinline', '');
    tryPlay(mediaOutEl);
  }
  if (!isMobile()) {
    gainNode.connect(audioCtx.destination);
  }

  keepAliveEl = new Audio(SILENT_WAV);
  keepAliveEl.loop = true;
  keepAliveEl.volume = 0.001;
  keepAliveEl.setAttribute('playsinline', '');
  tryPlay(keepAliveEl);

  setupAudioSession();
  setupMediaSession();

  if (audioCtx.state === 'suspended') {
    await audioCtx.resume();
  }

  audioReady = true;
  const queued = pending;
  pending = [];
  for (const s of queued) pushSamples(s);
}

function resetAudioGraph() {
  audioReady = false;
  try { workletNode?.disconnect(); } catch (_) { /* ignore */ }
  try {
    if (scriptNode) {
      scriptNode.disconnect();
      scriptNode.onaudioprocess = null;
    }
  } catch (_) { /* ignore */ }
  try { gainNode?.disconnect(); } catch (_) { /* ignore */ }
  workletNode = scriptNode = gainNode = mediaDest = null;

  if (mediaOutEl) {
    mediaOutEl.pause();
    mediaOutEl.srcObject = null;
  }
  if (keepAliveEl) {
    keepAliveEl.pause();
    keepAliveEl = null;
  }
  if ('mediaSession' in navigator) {
    navigator.mediaSession.playbackState = 'none';
  }
  pending = [];
  if (meterBar) meterBar.style.width = '0%';
}

function destroySocket() {
  if (!socket) return;
  const s = socket;
  socket = null;
  s.onopen = s.onclose = s.onerror = s.onmessage = null;
  try { s.close(); } catch (_) { /* ignore */ }
}

function openSocket() {
  return new Promise((resolve, reject) => {
    destroySocket();

    let settled = false;
    const timer = setTimeout(() => {
      if (settled) return;
      settled = true;
      destroySocket();
      reject(new Error('ws timeout'));
    }, WS_TIMEOUT_MS);

    const finish = (err) => {
      if (settled) return;
      settled = true;
      clearTimeout(timer);
      if (err) reject(err);
      else resolve();
    };

    const url = wsUrl();
    console.log('WebSocket connecting to', url);
    socket = new WebSocket(url);
    socket.binaryType = 'arraybuffer';

    socket.onopen = () => {
      console.log('WebSocket open');
      installDataHandler();
      finish(null);
    };

    socket.onerror = () => finish(new Error('ws error'));

    socket.onclose = (ev) => {
      const wasOpen = settled;
      destroySocket();
      if (!wasOpen) {
        finish(new Error('ws closed'));
        return;
      }
      if (isListening) {
        isListening = false;
        isStarting = false;
        stopBgTimer();
        resetAudioGraph();
        refreshUI();
        setStatus('error', 'Disconnected');
      }
    };
  });
}

function installDataHandler() {
  if (!socket) return;
  socket.onmessage = (e) => {
    if (!acceptingAudio()) return;
    const buf = e.data;
    if (!(buf instanceof ArrayBuffer) || buf.byteLength === 0) return;

    const usable = buf.byteLength - (buf.byteLength % kChunkBytes);
    const floatsPerChunk = kChunkBytes / 4;

    for (let off = 0; off < usable; off += kChunkBytes) {
      const samples = new Float32Array(floatsPerChunk);
      samples.set(new Float32Array(buf, off, floatsPerChunk));
      updateMeter(samples);
      pushSamples(samples);
    }
  };
}

function closeSocket() {
  destroySocket();
}

function startBgTimer() {
  stopBgTimer();
  // setInterval is throttled in background; still helps on Android/desktop.
  bgTimer = setInterval(() => resumePlayback(), BG_INTERVAL_MS);
}

function stopBgTimer() {
  if (bgTimer) {
    clearInterval(bgTimer);
    bgTimer = null;
  }
}

function cancelStartup() {
  isStarting = false;
  isListening = false;
  stopBgTimer();
  closeSocket();
  resetAudioGraph();
  refreshUI();
}

async function startListening() {
  if (isListening || isStarting) return;
  if (location.protocol === 'file:') {
    setStatus('error', 'Open via https:// — not as a local file');
    return;
  }

  isStarting = true;
  pending = [];
  if (playBtn) playBtn.disabled = true;
  setStatus('busy', 'Connecting...');
  refreshUI();

  try {
    if (!audioCtx) {
      // Some Android devices reject forced 48 kHz — use native rate + resample on mobile.
      const ctxOpts = { latencyHint: 'playback' };
      if (!isMobile()) ctxOpts.sampleRate = kSampleRateHz;
      audioCtx = new AudioContext(ctxOpts);
    }
    if (audioCtx.state === 'suspended') {
      await audioCtx.resume();
    }

    await withTimeout(openSocket(), WS_TIMEOUT_MS + 500, 'ws');

    if (!isStarting) return;

    setStatus('busy', 'Starting audio...');
    await withTimeout(buildAudioGraph(), AUDIO_TIMEOUT_MS, 'audio');

    if (!isStarting) return;

    isListening = true;
    isStarting = false;
    startBgTimer();
    refreshUI();
  } catch (err) {
    console.error('startListening failed:', err);
    isStarting = false;
    isListening = false;
    closeSocket();
    resetAudioGraph();

    const msg = String(err?.message || err);
    if (msg.includes('ws')) {
      setStatus('error', "Can't connect — is pocket-audio-server running on port 9000?");
    } else if (msg.includes('audio')) {
      setStatus('error', 'Audio setup timed out — tap Listen again');
    } else {
      setStatus('error', 'Failed — tap Listen to retry');
    }
    refreshUI();
  }
}

function stopListening() {
  if (!isListening && !isStarting) return;
  isStarting = false;
  isListening = false;
  stopBgTimer();
  closeSocket();
  resetAudioGraph();
  refreshUI();
}

if (volumeSldr) {
  volumeSldr.addEventListener('input', () => {
    if (gainNode) gainNode.gain.value = Number(volumeSldr.value);
  });
}

if (playBtn) {
  playBtn.addEventListener('click', () => {
    if (isListening || isStarting) stopListening();
    else startListening();
  });
}

// Tab hidden/visible — iOS + Android background audio.
document.addEventListener('visibilitychange', () => {
  if (isListening) resumePlayback();
});

// pagehide: runs before mobile OS suspends the page.
window.addEventListener('pagehide', () => {
  if (isListening) resumePlayback();
});

window.addEventListener('pageshow', () => {
  if (isListening) resumePlayback();
});

window.addEventListener('focus', () => {
  if (isListening) resumePlayback();
});

window.addEventListener('blur', () => {
  if (isListening) resumePlayback();
});

// Chrome on Android: page frozen in background (Page Lifecycle API).
document.addEventListener('freeze', () => {
  if (isListening) resumePlayback();
});
document.addEventListener('resume', () => {
  if (isListening) resumePlayback();
});

refreshUI();
