#!/bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
BASE_PATH="$(dirname -- "$SCRIPT_DIR")"
BUILD_TOOLS_PATH="$BASE_PATH/build-tools"
export GOMA_FALLBACK_ON_AUTH_FAILURE=true

if [ -n "$CI" ]; then
  # Automatically restart the compiler proxy when it dies in CI
  export GOMA_START_COMPILER_PROXY=true
fi

case "$(uname)" in
  MINGW*|MSYS*)
    GOMACC="gomacc.exe"
    ;;
  *)
    GOMACC="gomacc"
    ;;
esac

cd "$BUILD_TOOLS_PATH"
case "$1" in
  start)
    third_party/goma/goma_ctl.py ensure_start
    ;;
  restart)
    third_party/goma/goma_ctl.py restart
    ;;
  stop)
    third_party/goma/goma_ctl.py stop
    ;;
  check_restart)
    echo "Checking Goma..."
    "third_party/goma/$GOMACC" port 2 || third_party/goma/goma_ctl.py restart
    ;;
esac
