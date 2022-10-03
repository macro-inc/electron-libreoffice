#!/bin/bash

set -e

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
BASE_PATH="$(dirname -- "$SCRIPT_DIR")"
DEPOT_TOOLS_PATH="$BASE_PATH/depot_tools"
BUILD_TOOLS_PATH="$BASE_PATH/build-tools"
export PATH="$DEPOT_TOOLS_PATH:$SCRIPT_DIR:$PATH"

cd "$BASE_PATH"

scripts/e init

# Cache between syncs
export GIT_CACHE_PATH="$BASE_PATH/.git-cache"

# Assume ths base platform is Linux, and start there
scripts/cross-sync/linux.sh

# Manually apply the patches, since the hooks will not be run until build-time
vpython3 src/electron/script/apply_all_patches.py src/electron/patches/config.json
vpython3 src/electron/script/patches-mtime-cache.py apply --cache-file src/electron/patches/mtime-cache.json

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
