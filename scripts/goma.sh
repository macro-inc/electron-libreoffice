#!/bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
BASE_PATH="$(dirname -- "$SCRIPT_DIR")"
BUILD_TOOLS_PATH="$BASE_PATH/build-tools"
export GOMA_FALLBACK_ON_AUTH_FAILURE=true

cd "$BUILD_TOOLS_PATH"
case "$1" in
  start)
    third_party/goma/goma_ctl.py ensure_start
    ;;
  stop)
    third_party/goma/goma_ctl.py ensure_stop
    ;;
esac
