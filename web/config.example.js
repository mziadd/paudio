// Copy to config.js and set your public WebSocket URL (from Cloudflare Tunnel, etc.).
// Leave WS_URL empty to use LAN mode: ws://same-host-as-page:9000

/** @type {string} e.g. 'wss://audio.yourdomain.com' */
export const WS_URL = '';

/** LAN fallback port when WS_URL is empty */
export const LAN_WS_PORT = 9000;
