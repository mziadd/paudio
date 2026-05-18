# PocketAudio

Hear your Mac or PC speakers on your phone over Wi‑Fi. Tap Listen in the browser.

**Repo:** https://github.com/mziadd/paudio

## You need

- macOS 15+ or Windows 11
- Phone on the same Wi‑Fi as the PC
- Mac: **Screen Recording** allowed for Terminal (System Settings → Privacy & Security)

## Run it

**Terminal 1 — server** (from `build/` after cmake):

```bash
./pocket-audio-server
```

**Terminal 2 — web** (Python or Node):

```bash
cd web
python3 -m http.server 8080 --bind 0.0.0.0
```

```bash
cd web
npx --yes serve -l tcp://0.0.0.0:8080
```

**Phone:** `http://YOUR_MAC_IP:8080/` — play audio on the PC, tap **Listen**.

Audio WebSocket: `ws://YOUR_MAC_IP:9000` (no HTTPS, no certs).

Stream format: **48 kHz stereo float32** PCM (~48 chunks/s on LAN).

## Build

```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

## Layout

```
server/          C++ app (capture + WebSocket)
  main.cpp
  core/          format, chunks, ports
  capture/       Mac / Windows audio in
  network/       WebSocket out
web/             browser UI
```

## Problems

- **Can't connect** — server running? Same Wi‑Fi? Firewall open on port 9000?
- **No sound** — volume on PC and phone; tap Listen again.
