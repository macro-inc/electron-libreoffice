#!/bin/bash

SCRIPT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" &> /dev/null && pwd)
BASE_PATH="$(dirname -- "$SCRIPT_DIR")"

cd "$BASE_PATH"

# Remove large test cases that are from upstream projects (not Electron)
rm -rf src/third_party/catapult/tracing/test_data
rm -rf src/third_party/angle/third_party/VK-GL-CTS
rm -rf src/third_party/blink/web_tests
rm -rf src/third_party/blink/perf_tests
rm -rf src/third_party/WebKit/LayoutTests
# rm -rf src/third_party/swiftshader/tests -- necessary
rm -rf src/chrome/test/data/xr/webvr_info

# Remove iOS and Android portions as well
rm -rf src/android_webview
rm -rf src/ios/chrome
rm -rf src/third_party/android_rust_toolchain

# Remove all .git that aren't strictly necessary
cd "$BASE_PATH/src"
( find . -type d -name ".git" -not -path "./third_party/angle/*" -not -path "./third_party/dawn/*" ) | xargs rm -rf
