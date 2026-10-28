#!/bin/bash
# Rebuild shared app targets, then exercise sourcepack, engine, controls and WASM.
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/.." && pwd)
BASE=$("$ROOT/tools/prepare-base.sh")
export FM1_BASE="$BASE"
cd "$ROOT"
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
cc -std=c99 -Wall -Wextra -Werror -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Iapp/dsp -Iapp/build/gen tests/contract_test.c app/dsp/engine.c \
  -o app/build/tests/contract-test
app/build/tests/contract-test | tee app/build/tests/contract.log
cc -std=c99 -Wall -Wextra -Werror -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Iapp -I"$BASE/include" tests/telemetry_transport_test.c \
  app/telemetry_stream.c app/telemetry.c app/vendor/jems/jems.c \
  -o app/build/tests/telemetry-transport-test
app/build/tests/telemetry-transport-test | tee app/build/tests/telemetry-transport.log
cc -std=c99 -Wall -Wextra -Werror -O1 -g \
  -fsanitize=address,undefined -fno-omit-frame-pointer \
  -Iapp -Iapp/build/gen tests/telemetry_playback_test.c app/dsp/engine.c \
  -o app/build/tests/telemetry-playback-test
app/build/tests/telemetry-playback-test
.venv/bin/python tests/telemetry_serializer_test.py | tee app/build/tests/telemetry-serializer.log
FM1B_SYSEX_LOG="$PWD/app/build/tests/host-telemetry.syx" app/build/host/beets_host app/tests/basic.txt app/build/tests/host | tee app/build/tests/host.log
.venv/bin/python tests/telemetry_host_capture.py app/build/tests/host-telemetry.syx | tee app/build/tests/telemetry-host.log
node tests/wasm-smoke.mjs | tee app/build/tests/wasm.log
printf 'All Beets tests passed. Evidence: app/build/tests/\n'
