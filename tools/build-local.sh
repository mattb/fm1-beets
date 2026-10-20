#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
export JIELI_TOOLCHAIN="${JIELI_TOOLCHAIN:-$HOME/.jieli/toolchain}"
export AC79_SDK="${AC79_SDK:-$HOME/.jieli/fw-AC79_AIoT_SDK}"
export PATH="/opt/homebrew/bin:$PATH"
cd "$ROOT"
if [ "${1:-}" = "--all" ]; then
    shift
    "$ROOT/.venv/bin/python" "$ROOT/fm1-base/tools/build.py" "$ROOT/app" --host "$@"
    "$ROOT/.venv/bin/python" "$ROOT/fm1-base/tools/build.py" "$ROOT/app" --web "$@"
    exec "$ROOT/.venv/bin/python" "$ROOT/fm1-base/tools/build.py" "$ROOT/app" "$@"
fi
exec "$ROOT/.venv/bin/python" "$ROOT/fm1-base/tools/build.py" "$ROOT/app" "$@"
