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

# Bundle the base tarball, primarily consisting of the source
tar --zstd -cf base.tzstd --anchored --exclude-from="$SCRIPT_DIR/tar_excludes_base.txt" src

# Dereference and repack angle and dawn git to not use the cache
(cd src/third_party/angle; git repack -a -d; rm -f .git/objects/info/alternates)
(cd src/third_party/dawn; git repack -a -d; rm -f .git/objects/info/alternates)

# Build the dawn/angle tarball (it needs the .git directories without )
tar --zstd -cf dawn-angle.tzstd --anchored --exclude="src/third_party/angle/third_party/VK_GL-CTS/src" src/third_party/dawn src/third_party/angle

# Build the confirmed non-OS-specific thirdparty tarball
tar --zstd -cf thirdparty.tzstd --anchored --exclude-from="$SCRIPT_DIR/tar_thirdparty_excludes.txt" --files-from="$SCRIPT_DIR/tar_thirdparty_includes.txt"

# Build the Windows-specific tarball
scripts/cross-sync/win.sh
