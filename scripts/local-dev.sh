#!/bin/bash
# One-command local HTTPS for phone testing (Listen).
set -e
cd "$(dirname "$0")/.."
if ! python3 -c "import aiohttp" 2>/dev/null; then
  echo "Installing aiohttp (once)…"
  pip3 install aiohttp
fi
exec python3 scripts/local-dev.py
