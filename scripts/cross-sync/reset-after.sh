#!/bin/bash

set -e
SCRIPT_DIR=$(cd -- "$(dirname $(dirname -- "${BASH_SOURCE[0]}"))" &> /dev/null && pwd)
BASE_PATH="$(dirname -- "$SCRIPT_DIR")"
DEPOT_TOOLS_PATH="$BASE_PATH/depot_tools"

# Reset any applied patches to .gclient
(cd "$BASE_PATH"; git checkout -- .gclient)

# Reset any applied patches to .gclient
(cd "$DEPOT_TOOLS_PATH"; git checkout -- gclient.py)

# Undo any
(cd "$BASE_PATH" && (rm .gclient_* || true) && git clean -ffdx src && (rm -rf src/.git || true) && git checkout -- src)
