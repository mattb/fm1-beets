#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/.jieli/fw-AC79_AIoT_SDK}"
export PATH="/opt/homebrew/bin:$PATH"
BASE=$("$ROOT/tools/prepare-base.sh")
cd "$ROOT"
printf 'Using fm1-base: %s\n' "$BASE" >&2
if [ "${1:-}" = "--all" ]; then
    shift
    "$ROOT/.venv/bin/python" "$BASE/tools/build.py" "$ROOT/app" --host "$@"
    "$ROOT/.venv/bin/python" "$BASE/tools/build.py" "$ROOT/app" --web "$@"
    exec "$ROOT/.venv/bin/python" "$BASE/tools/build.py" "$ROOT/app" "$@"
fi
exec "$ROOT/.venv/bin/python" "$BASE/tools/build.py" "$ROOT/app" "$@"
