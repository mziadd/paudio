# PocketAudio

Hear your Mac or PC speakers on your phone over Wi‑Fi. Tap Listen in the browser.

**Repo:** https://github.com/mziadd/paudio

## Download (ready-to-use)

Pre-built releases (server + web UI, no compiler needed):

**[github.com/mziadd/paudio/releases](https://github.com/mziadd/paudio/releases)**

| Download | Platform |
|----------|----------|
| `PocketAudio-macos-arm64.zip` | macOS 15+ (Apple Silicon) |
| `PocketAudio-windows-x64.zip` | Windows 10/11 (64-bit) |

1. Download and unzip the zip for your OS.
2. Open the folder — inside you’ll find `pocket-audio-server` (or `.exe`), a `web/` folder, and `START.txt`.
3. Follow **Run it** below (use the unzipped folder instead of building from source).

**Intel Mac:** no pre-built zip yet — use [Build (macOS)](#build-macos) below.

### Publish a new release (maintainers)

Push a version tag; GitHub Actions builds both platforms and uploads the zips:

```bash
git tag v1.0.0
git push origin v1.0.0
```

Or run **Actions → Release → Run workflow** on GitHub.

## Public deploy (web online + server at home)

**Live web app:** https://mziadd.github.io/paudio/

**Full guide:** [DEPLOY.md](DEPLOY.md)

Quick version:

1. Run `pocket-audio-server` on your PC (always).
2. Expose it with [Cloudflare Tunnel](https://developers.cloudflare.com/cloudflare-one/connections/connect-apps/) → `wss://audio.yourdomain.com`.
3. GitHub repo → **Settings** → **Secrets** → `POCKET_AUDIO_WS_URL` = `wss://audio.yourdomain.com`.
4. Push to `main` (or re-run **Deploy Web** action).
5. Open the Pages URL on your phone (cellular) → **Listen**.

Test without redeploy: `https://mziadd.github.io/paudio/?ws=wss://audio.yourdomain.com`

## You need

- macOS 15+ or Windows 11
- Phone on the same Wi‑Fi as the PC
- Mac: **Screen Recording** allowed for Terminal (System Settings → Privacy & Security)

## Run it

Works the same whether you use a [release zip](#download-ready-to-use) or built from source.

**Terminal 1 — server** (folder with `pocket-audio-server` or `pocket-audio-server.exe`):

```bash
./pocket-audio-server
```

On Windows:

```powershell
.\pocket-audio-server.exe
```

**Terminal 2 — web** (the `web` folder next to the server — in the zip or repo):

```bash
cd web
python3 -m http.server 8080 --bind 0.0.0.0
```

```bash
cd web
npx --yes serve -l tcp://0.0.0.0:8080
```

**Phone:** `http://YOUR_PC_IP:8080/` — play audio on the computer, tap **Listen**.

Audio WebSocket: `ws://YOUR_PC_IP:9000` (no HTTPS, no certs). Use your Mac or Windows LAN IP.

## Build (macOS)

```bash
mkdir -p build && cd build
cmake ..
cmake --build .
```

The server binary is `build/pocket-audio-server`.

## Build (Windows)

You need:

- **Windows 11** (or Windows 10 with current updates)
- **Visual Studio 2022** (or **Build Tools for Visual Studio**) with the **Desktop development with C++** workload
- **CMake** 3.16+ ([cmake.org](https://cmake.org/download/) or `winget install Kitware.CMake`)
- **Git** (for CMake to fetch ixwebsocket)

Open **Developer PowerShell for VS 2022** (or any shell where `cl` and `cmake` work), then from the repo root:

```powershell
mkdir build
cd build
cmake ..
cmake --build . --config Release
```

The executable is:

```
build\Release\pocket-audio-server.exe
```

(On a single-config generator it may be `build\pocket-audio-server.exe` instead.)

### Run on Windows

**Terminal 1 — server** (from `build\Release` or wherever the `.exe` is):

```powershell
.\pocket-audio-server.exe
```

You should see `listening on 0.0.0.0:9000`. Leave this window open.

**Terminal 2 — web UI** (from the repo `web` folder):

```powershell
cd web
python -m http.server 8080 --bind 0.0.0.0
```

Or with Node:

```powershell
cd web
npx --yes serve -l tcp://0.0.0.0:8080
```

**Phone or another PC:** open `http://YOUR_PC_IP:8080/` (replace with the Windows machine’s LAN IP, e.g. from `ipconfig`), play audio on the PC, tap **Listen**.

- WebSocket: `ws://YOUR_PC_IP:9000`
- If the phone cannot connect, allow **port 9000** (and **8080** for the web page) in **Windows Defender Firewall** for `pocket-audio-server.exe` or for private networks.
- For best quality, set Windows sound output to **48000 Hz** (Settings → System → Sound → your output device → Properties → Advanced).

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
