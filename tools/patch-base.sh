#!/bin/sh
# Explicitly apply our platform extension; never overwrite conflicting edits.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ "$#" -gt 1 ]; then echo 'usage: tools/patch-base.sh [BASE_DIRECTORY]' >&2; exit 2; fi
BASE=$(CDPATH= cd -- "${1:-${FM1_BASE:-$ROOT/fm1-base}}" && pwd)
# Preflight every missing patch before modifying anything.
for PATCH in "$ROOT"/patches/fm1-base-*.patch; do
    if git -C "$BASE" apply --reverse --check "$PATCH" 2>/dev/null; then continue; fi
    if ! git -C "$BASE" apply --check "$PATCH"; then
        echo "fm1-base patch conflicts: $PATCH; nothing was overwritten." >&2
        exit 1
    fi
done
for PATCH in "$ROOT"/patches/fm1-base-*.patch; do
    if git -C "$BASE" apply --reverse --check "$PATCH" 2>/dev/null; then continue; fi
    git -C "$BASE" apply "$PATCH"
done
