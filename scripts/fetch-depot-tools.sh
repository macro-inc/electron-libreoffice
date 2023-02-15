#!/bin/bash
set -e

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE:-$0}")" &> /dev/null && pwd)
BASE_PATH="$(dirname -- "$SCRIPT_DIR")"
DEPOT_TOOLS_PATH="$BASE_PATH/depot_tools"
DEPOT_TOOLS_PIN="2b1aa8dcabdd430ce92896343b822a128de6e368"

echo "Fetching depot-tools"
cd "$BASE_PATH"
git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git depot_tools
(cd depot_tools && git checkout "$DEPOT_TOOLS_PIN")

echo "Installing depot-tools"
cd "$DEPOT_TOOLS_PATH"
if [ "$(uname)" == "Darwin" ]; then
  # remove ninjalog_uploader_wrapper.py from autoninja since we don't use it and it causes problems
  sed -i '' '/ninjalog_uploader_wrapper.py/d' ./autoninja
else
  sed -i '/ninjalog_uploader_wrapper.py/d' ./autoninja
fi
# By default, gclient's git usage isn't optimal, fix that
git apply "$SCRIPT_DIR/gclient-fix.patch"
git apply "$SCRIPT_DIR/override-ninja-win-path.patch"

# Don't trust the system Python
ln -s vpython3 python3
ln -s vpython3.bat python3.bat
ln -s vpython python
ln -s vpython.bat python.bat
