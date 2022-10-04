#!/bin/bash

set -e
SCRIPT_DIR=$(cd -- "$(dirname $(dirname -- "${BASH_SOURCE[0]}"))" &> /dev/null && pwd)
BASE_PATH="$(dirname -- "$SCRIPT_DIR")"
DEPOT_TOOLS_PATH="$BASE_PATH/depot_tools"

# Reset any applied patches to .gclient
(cd "$BASE_PATH"; git checkout -- .gclient)

# Reset any applied patches to .gclient
(cd "$DEPOT_TOOLS_PATH"; git checkout -- gclient.py)

# Don't trust the system Python
(cd "$DEPOT_TOOLS_PATH";
([ ! -f python3 ] && ln -s vpython3 python3) || true
([ ! -f python3.bat ] && ln -s vpython3.bat python3.bat) || true
([ ! -f python ] && ln -s vpython python) || true
([ ! -f python.bat ] && ln -s vpython.bat python.bat) || true)

echo 'undoing changes'
# Undo any changes
(cd "$BASE_PATH" && (rm .gclient_* || true) && (rm -rf src || true) && git checkout -- src/electron)
