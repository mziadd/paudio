# PocketAudio

Hear your Mac or PC speakers on your phone over Wi‑Fi. Tap Listen in the browser.

**Repo:** https://github.com/mziadd/paudio

## You need

- macOS 15+ or Windows 11
- Phone on the same Wi‑Fi as the PC
- Mac: **Screen Recording** allowed for Terminal (System Settings → Privacy & Security)

## Run it

**Terminal 1 — audio server** (from `build/` after cmake):

```bash
./pocket-audio-server
```

Wait for `WebSocket on port 9000`.

**Terminal 2 — web UI** (pick one):

### Python 3

```bash
cd web
python3 -m http.server 8080 --bind 0.0.0.0
```

### Node.js

```bash
cd web
npx --yes serve -l tcp://0.0.0.0:8080
```

(`npx http-server -p 8080 -a 0.0.0.0` works too.)

**On your phone:** open `http://YOUR_MAC_IP:8080/`  
(find IP: System Settings → Network, or `ipconfig getifaddr en0` on Mac)

On the same Mac: `http://localhost:8080/`

Play audio on the PC, tap **Listen**.

The page is plain **http**. Audio goes over **ws://YOUR_MAC_IP:9000** (no certificate).

## Windows

Build and run `Release\pocket-audio-server.exe`, then serve `web/` the same way (Python or Node) from the repo.

## When something breaks

- **Can't connect** — `pocket-audio-server` running? Same Wi‑Fi? Mac firewall allowing port 9000?
- **No sound** — audio playing on the PC? Volume up. Tap Listen again.
- **Page cache** — hard refresh or close the tab.

## Repo layout

- `apps/server/` — server
- `src/capture/` — system audio
- `src/network/` — WebSocket :9000
- `web/` — browser UI
