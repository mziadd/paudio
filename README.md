# PocketAudio

Hear your Mac or PC speakers on your phone over Wi‑Fi. Tap Listen in the browser — that's it. No mic, no upload from the phone.

**Repo:** https://github.com/mziadd/paudio

## You need

- macOS 15+ or Windows 11
- Phone on the same Wi‑Fi
- On Mac: **Screen Recording** allowed for Terminal (or whatever runs the server)

## Run it (Mac)

Two terminals.

**Build once:**

```bash
brew install cmake
mkdir -p build && cd build
cmake ..
cmake --build .
```

**Terminal 1 — server:**

```bash
cd build
./pocket-audio-server
```

Leave it running. You'll see `WebSocket on port 9000`. When someone's listening you'll see ~47 chunks/s. Zero chunks/s just means nobody's connected.

**Terminal 2 — web + HTTPS (from repo root, not `build/`):**

```bash
./scripts/local-dev.sh
```

It prints a URL like `https://192.168.x.x:8443/`. Open that on your phone.

On the phone: accept the cert warning, play something on the Mac, tap **Listen**.

Same machine only? Use `https://localhost:8443/`.

## Windows

```bat
mkdir build && cd build
cmake ..
cmake --build . --config Release
Release\pocket-audio-server.exe
```

Other terminal, repo root: `python scripts\local-dev.py` — then open the https URL on your phone.

## When something breaks

- **Can't connect** — server first, then `local-dev.sh`. Same Wi‑Fi.
- **No sound** — audio playing on the PC? Volume up on both sides. Tap Listen again.
- **Page acting weird** — close the tab and reopen (cache).
- **Mac won't capture** — Screen Recording permission, restart the server.
- **Background on phone** — works best if you add the page to your home screen.

## What's in the repo

- `apps/server/` — main program
- `src/capture/` — grabs system audio (Mac / Windows)
- `src/network/` — WebSocket server on port 9000
- `web/` — browser UI (`player.js`, `processor.js`)
- `scripts/local-dev.sh` — HTTPS on 8443, proxies `/ws` to the server
