#!/usr/bin/env python3
# Local HTTPS: web UI + WSS on one port (one cert warning on phone).
# Proxies WebSocket to pocket-audio-server on ws://127.0.0.1:9000 (plain).
#
# Usage:
#   Terminal 1: ./build/pocket-audio-server
#   Terminal 2: python3 scripts/local-dev.py
#   Phone: https://YOUR_MAC_IP:8443  (accept cert warning, tap Listen)

import asyncio
import os
import socket
import ssl
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
WEB = ROOT / "web"
CERT_DIR = ROOT / "web" / ".certs"
CERT = CERT_DIR / "cert.pem"
KEY = CERT_DIR / "key.pem"
PORT = int(os.environ.get("POCKET_AUDIO_HTTPS_PORT", "8443"))
BACKEND_WS = os.environ.get("POCKET_AUDIO_BACKEND", "ws://127.0.0.1:9000")


def local_ip():
    try:
        s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        s.connect(("8.8.8.8", 80))
        ip = s.getsockname()[0]
        s.close()
        return ip
    except OSError:
        return "127.0.0.1"


def ensure_cert():
    CERT_DIR.mkdir(parents=True, exist_ok=True)
    if CERT.exists() and KEY.exists():
        return
    ip = local_ip()
    print(f"Creating self-signed cert (SAN: localhost, 127.0.0.1, {ip}) …")
    subprocess.run(
        [
            "openssl", "req", "-x509", "-newkey", "rsa:2048",
            "-keyout", str(KEY), "-out", str(CERT),
            "-days", "365", "-nodes",
            "-subj", "/CN=PocketAudio-Local",
            "-addext", f"subjectAltName=DNS:localhost,IP:127.0.0.1,IP:{ip}",
        ],
        check=True,
    )


def main():
    try:
        import aiohttp
        from aiohttp import web, WSMsgType
    except ImportError:
        print("Install aiohttp:  pip3 install aiohttp")
        sys.exit(1)

    ensure_cert()

    async def serve_index(_request):
        return web.FileResponse(WEB / "index.html")

    async def ws_proxy(request):
        """Browser WSS <-> pocket-audio-server WS."""
        session = aiohttp.ClientSession()
        try:
            backend = await session.ws_connect(BACKEND_WS)
        except Exception as e:
            await session.close()
            return web.Response(
                text=f"Cannot reach {BACKEND_WS} — start pocket-audio-server first.\n{e}",
                status=502,
            )

        client = web.WebSocketResponse()
        await client.prepare(request)

        async def to_backend():
            async for msg in client:
                if msg.type == WSMsgType.BINARY:
                    await backend.send_bytes(msg.data)
                elif msg.type == WSMsgType.TEXT:
                    await backend.send_str(msg.data)
                elif msg.type in (WSMsgType.CLOSE, WSMsgType.ERROR):
                    break
            await backend.close()

        async def to_client():
            async for msg in backend:
                if msg.type == WSMsgType.BINARY:
                    await client.send_bytes(msg.data)
                elif msg.type == WSMsgType.TEXT:
                    await client.send_str(msg.data)
                elif msg.type in (WSMsgType.CLOSE, WSMsgType.ERROR):
                    break
            await client.close()

        await asyncio.gather(to_backend(), to_client())
        await session.close()
        return client

    app = web.Application()
    app.router.add_get("/ws", ws_proxy)
    app.router.add_get("/", serve_index)
    app.router.add_get("/index.html", serve_index)
    app.router.add_static("/", WEB, show_index=True)

    ssl_ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ssl_ctx.load_cert_chain(str(CERT), str(KEY))

    ip = local_ip()
    print()
    print("  pocket-audio-server must be running on port 9000")
    print(f"  Open on this Mac:  https://localhost:{PORT}/")
    print(f"  Open on phone:     https://{ip}:{PORT}/")
    print("  Accept the certificate warning once, then tap Listen.")
    print()

    web.run_app(app, host="0.0.0.0", port=PORT, ssl_context=ssl_ctx, print=None)


if __name__ == "__main__":
    main()
