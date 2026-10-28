#!/bin/sh
# Print the selected absolute dependency path. Progress/errors go to stderr.
# Default: pinned submodule, bootstrapped on demand. FM1_BASE: user-owned checkout.
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ -n "${FM1_BASE:-}" ]; then
    BASE=$(CDPATH= cd -- "$FM1_BASE" && pwd) || exit 1
    if [ ! -f "$BASE/tools/build.py" ] || [ ! -f "$BASE/include/plat.h" ] || [ ! -f "$BASE/bsp/chip/usb.c" ]; then
        echo 'FM1_BASE must point to an fm1-base checkout with its bsp submodule initialized.' >&2
        exit 1
    fi
    # Do not edit an external checkout as a side effect of building this app.
    for PATCH in "$ROOT"/patches/fm1-base-*.patch; do
        if ! git -C "$BASE" apply --reverse --check "$PATCH" 2>/dev/null; then
            echo 'External FM1_BASE lacks an expected platform patch (or differs from the supported base).' >&2
            echo "Review it, then explicitly run: tools/patch-base.sh \"$BASE\"" >&2
            exit 1
        fi
    done
else
    BASE="$ROOT/fm1-base"
    if [ ! -f "$BASE/tools/build.py" ]; then
        git -C "$ROOT" submodule update --init --recursive -- fm1-base >&2
    elif [ ! -f "$BASE/bsp/chip/usb.c" ]; then
        git -C "$BASE" submodule update --init --recursive >&2
    fi
    "$ROOT/tools/patch-base.sh" "$BASE" >&2
fi
printf '%s\n' "$BASE"
