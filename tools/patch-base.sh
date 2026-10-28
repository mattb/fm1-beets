#!/bin/sh
# Reproducible platform extension on the pinned upstream submodule.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
PATCH="$ROOT/patches/fm1-base-telemetry.patch"
if git -C "$ROOT/fm1-base" apply --reverse --check "$PATCH" 2>/dev/null; then
    exit 0
fi
if ! git -C "$ROOT/fm1-base" apply --check "$PATCH"; then
    echo 'fm1-base telemetry patch conflicts with local changes; nothing was overwritten.' >&2
    exit 1
fi
git -C "$ROOT/fm1-base" apply "$PATCH"
