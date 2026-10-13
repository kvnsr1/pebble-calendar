#!/bin/sh
set -eu
. "$(dirname "$0")/env.sh"
cd "$PROJECT_DIR"
command -v node >/dev/null || { echo 'Install Node.js first.' >&2; exit 1; }
command -v uv >/dev/null || { echo 'Install uv first (on macOS: brew install uv python@3.13 node).' >&2; exit 1; }
export UV_CACHE_DIR="$PROJECT_DIR/.tools/uv-cache"
uv venv --allow-existing .tools/venv
uv pip install --python .tools/venv/bin/python 'pebble-tool==5.0.40'
python3 scripts/prepare-toolchain.py
pebble sdk install 4.33.1
