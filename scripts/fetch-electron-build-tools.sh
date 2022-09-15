#!/bin/bash
set -e

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
BASE_PATH="$(dirname -- "$SCRIPT_DIR")"

cd "$BASE_PATH"
echo "Fetching electron/build-tools"
git clone --depth=1 https://github.com/electron/build-tools.git build-tools
cd build-tools
node "$BASE_PATH/src/electron/script/yarn" install

echo "Fetching Goma"
mkdir third_party
node -e "require('./src/utils/goma.js').downloadAndPrepare({ gomaOneForAll: true })"
