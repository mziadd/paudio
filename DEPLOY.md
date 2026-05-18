# Deploy PocketAudio (public internet)

Split setup:

| Part | Where | What |
|------|--------|------|
| **Web UI** | GitHub Pages (automatic) | https://mziadd.github.io/paudio/ |
| **Audio server** | Your Mac/PC at home | `pocket-audio-server` on port 9000 |
| **Tunnel** | Cloudflare (free) | `wss://your-subdomain` → your PC |

The server **must** run on the machine that plays the audio (your PC). Only the web page is hosted online.

---

## 1. Run the server at home

```bash
./pocket-audio-server
```

Leave it running. You should see `listening on 0.0.0.0:9000`.

**Mac:** allow **Screen Recording** for Terminal.

---

## 2. Expose port 9000 with Cloudflare Tunnel

### Install

```bash
brew install cloudflared
```

### One-time setup

```bash
cloudflared tunnel login
cloudflared tunnel create pocket-audio
```

Pick a hostname (you need a domain on Cloudflare):

```bash
cloudflared tunnel route dns pocket-audio audio.yourdomain.com
```

Copy and edit the example config:

```bash
mkdir -p ~/.cloudflared
cp deploy/cloudflared.example.yml ~/.cloudflared/config.yml
```

Edit `~/.cloudflared/config.yml`:

- Replace `YOUR_TUNNEL_ID` with the ID from `cloudflared tunnel create`
- Replace `credentials-file` path with the `.json` file path shown
- Replace `audio.yourdomain.com` with your real hostname

### Start the tunnel (every time you stream)

```bash
cloudflared tunnel run pocket-audio
```

Test: your tunnel URL should reach the server (browser tools may not show audio without the web app).

---

## 3. Point the web app at your tunnel

### Option A — GitHub secret (best for GitHub Pages)

1. GitHub repo → **Settings** → **Secrets and variables** → **Actions**
2. New secret: `POCKET_AUDIO_WS_URL` = `wss://audio.yourdomain.com` (no trailing slash)
3. Re-run **Actions** → **Deploy Web**, or push to `main`

### Option B — Local `web/config.js` (manual hosting)

```bash
cp web/config.example.js web/config.js
```

Edit `web/config.js`:

```javascript
export const WS_URL = 'wss://audio.yourdomain.com';
export const LAN_WS_PORT = 9000;
```

### Option C — URL parameter (quick test, no redeploy)

Open:

```
https://mziadd.github.io/paudio/?ws=wss://audio.yourdomain.com
```

---

## 4. Web hosting (GitHub Pages)

Already configured: push to `main` deploys `web/` automatically.

1. Repo → **Settings** → **Pages** → Source: **GitHub Actions**
2. After deploy: **https://mziadd.github.io/paudio/**

Enable Pages once if the first workflow fails with a permissions error.

---

## 5. Test from outside your Wi‑Fi

1. Server running on PC  
2. Tunnel running  
3. `POCKET_AUDIO_WS_URL` set (or `?ws=` in URL)  
4. Phone on **cellular** (not home Wi‑Fi)  
5. Open the Pages URL → **Listen**

---

## Security warning

Anyone who knows your URL can listen to your PC speakers. There is **no password** yet.

- Do not share the tunnel URL publicly unless you accept that risk  
- Consider Cloudflare Access later for login  

---

## Bandwidth

About **3 Mbps upload** per listener (48 kHz stereo PCM). Home upload speed is the limit.

---

## Troubleshooting

| Problem | Fix |
|---------|-----|
| Can't connect on HTTPS site | Use `wss://` not `ws://` in `WS_URL` |
| Works on Wi‑Fi only | Tunnel not running or wrong `WS_URL` |
| GitHub Pages 404 | Enable Pages → GitHub Actions in repo settings |
| No audio, meter still | Mac Screen Recording permission |
| Windows wrong pitch | Set speakers to **48000 Hz** |
