#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
exec "$ROOT/.venv/bin/python" -m http.server "${1:-8081}" --bind 127.0.0.1 --directory "$ROOT/app/build/emu"
