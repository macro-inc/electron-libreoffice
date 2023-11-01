#!/bin/bash
set -e

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE:-$0}")" &> /dev/null && pwd)
BASE_PATH="$(dirname -- "$SCRIPT_DIR")"
DEPOT_TOOLS_PATH="$BASE_PATH/depot_tools"
DEPOT_TOOLS_PIN="b7d8efd8bee494f4cfacacc19cf50fc4d4be3900"

echo "Fetching depot-tools"
cd "$BASE_PATH"
git clone --depth=1 https://chromium.googlesource.com/chromium/tools/depot_tools.git depot_tools
cd depot_tools
git fetch --depth 1 origin "$DEPOT_TOOLS_PIN"
git checkout "$DEPOT_TOOLS_PIN"
touch .disable_auto_update
if [ "$(uname)" == "Darwin" ]; then
  # remove ninjalog_uploader_wrapper.py from autoninja since we don't use it and it causes problems
  sed -i '' '/ninjalog_uploader_wrapper.py/d' ./autoninja
else
  sed -i '/ninjalog_uploader_wrapper.py/d' ./autoninja
fi
patch gclient.py -R <<EOF
676,677c676
<         packages = dep_value.get('packages', [])
<         for package in (x for x in packages if "infra/3pp/tools/swift-format" not in x.get('package')):
---
>         for package in dep_value.get('packages', []):
EOF
# By default, gclient's git usage isn't optimal, fix that
git apply "$SCRIPT_DIR/gclient-fix.patch"
git apply "$SCRIPT_DIR/override-ninja-win-path.patch"

if [ "$(uname)" == "Darwin" ] || [ "$(uname)" == "Linux" ]; then
  echo "Boostrapping python..."
  source "$DEPOT_TOOLS_PATH/bootstrap_python3"
  bootstrap_python3
  source "$DEPOT_TOOLS_PATH/cipd_bin_setup.sh"
  cipd_bin_setup
fi

# Don't trust the system Python
ln -s vpython3 python3
ln -s vpython3.bat python3.bat
ln -s vpython python
ln -s vpython.bat python.bat

