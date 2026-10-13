#!/bin/sh
PROJECT_DIR=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
export PATH="$PROJECT_DIR/.tools/venv/bin:$PROJECT_DIR/.tools/node/bin:$PROJECT_DIR/.tools/bin:$PATH"
export XDG_DATA_HOME="$PROJECT_DIR/.tools/share"
export npm_config_cache="$PROJECT_DIR/.tools/npm-cache"
export PIP_CACHE_DIR="$PROJECT_DIR/.tools/pip-cache"
if [ -d "$PROJECT_DIR/.tools/libpng/lib" ]; then
  export DYLD_LIBRARY_PATH="$PROJECT_DIR/.tools/libpng/lib${DYLD_LIBRARY_PATH:+:$DYLD_LIBRARY_PATH}"
fi
