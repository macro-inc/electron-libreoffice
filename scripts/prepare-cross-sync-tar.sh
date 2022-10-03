#!/bin/bash

set -e

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
BASE_PATH="$(dirname -- "$SCRIPT_DIR")"
DEPOT_TOOLS_PATH="$BASE_PATH/depot_tools"
BUILD_TOOLS_PATH="$BASE_PATH/build-tools"
export PATH="$DEPOT_TOOLS_PATH:$SCRIPT_DIR:$PATH"

cd "$BASE_PATH"

scripts/e init
if [ ! -d src/third_party/electron_node ]; then
  ELECTRON_NODE_VERSION="$(python3 -c "import ast;o = open('src/electron/DEPS', 'r').read(); j = ast.literal_eval(ast.get_source_segment(o, ast.parse(o).body[1].value)); print(j['node_version'])")"
  git clone --depth=1 --single-branch --branch "$ELECTRON_NODE_VERSION" https://github.com/nodejs/node.git src/third_party/electron_node
fi

# Cache between syncs
export GIT_CACHE_PATH="$BASE_PATH/.git-cache"

# Assume ths base platform is Linux, and start there
scripts/cross-sync/linux.sh

# Bundle the base tarball, primarily consisting of the source
tar --zstd -cf base.tzstd --exclude-from="$SCRIPT_DIR/tar_excludes_base.txt" src

# Build the dawn/angle tarball (it needs the .git directories)
tar --zstd -cf dawn-angle.tzstd src/third_party/dawn src/third_party/angle

# Build the confirmed non-OS-specific thirdparty tarball
tar --zstd -cf thirdparty.tzstd --exclude-from="$SCRIPT_DIR/tar_excludes_os.txt" --files-from="$SCRIPT_DIR/tar_thirdparty_includes.txt"

# Build the Windows-specific tarball
scripts/cross-sync/win.sh

# Build the Mac-specific tarball
scripts/cross-sync/mac.sh

# Build the Mac Apple Silicon-specific tarball
scripts/cross-sync/mac-arm.sh
