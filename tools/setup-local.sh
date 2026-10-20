#!/bin/sh
set -eu
ROOT=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
cd "$ROOT"
git submodule update --init --recursive
uv venv --python /opt/homebrew/opt/python@3.14/bin/python3.14 .venv
uv pip install --python .venv/bin/python 'pillow==13.0.0'
printf '%s\n' 'Requires emcc, ffmpeg, Docker, ~/.jieli/toolchain and ~/.jieli/fw-AC79_AIoT_SDK; see README.md.'
