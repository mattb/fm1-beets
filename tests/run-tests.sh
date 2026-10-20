#!/bin/bash
# Rebuild shared app targets, then exercise sourcepack, engine, controls and WASM.
set -euo pipefail
cd "$(dirname "$0")/.."
case "${1:-}" in
  '') tools/build-local.sh --host; tools/build-local.sh --web ;;
  --no-build) ;;
  *) echo 'usage: tests/run-tests.sh [--no-build]' >&2; exit 2 ;;
esac
mkdir -p app/build/tests/host
.venv/bin/python tests/sample_pack_test.py | tee app/build/tests/sample-pack.log
cc -std=c99 -Wall -Wextra -Werror -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Iapp/dsp -Iapp/build/gen tests/engine_test.c app/dsp/engine.c \
  -o app/build/tests/engine-test
app/build/tests/engine-test | tee app/build/tests/engine.log
app/build/host/beets_host app/tests/basic.txt app/build/tests/host | tee app/build/tests/host.log
node tests/wasm-smoke.mjs | tee app/build/tests/wasm.log
printf 'All Beets tests passed. Evidence: app/build/tests/\n'
