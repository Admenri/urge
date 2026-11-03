#!/bin/bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../../.." && pwd)"
build="$repo/build/macos-native"
tools="$repo/build/macos-tools"
if [[ ! -x "$tools/bin/python" ]]; then python3 -m venv "$tools"; fi
"$tools/bin/python" -m pip install -r "$repo/requirements.txt"
export PATH="$tools/bin:$PATH"

# Keep the native experiment separate from the Wasm/WKWebView build.
cmake -S "$repo" -B "$build" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_OSX_ARCHITECTURES="$(uname -m)" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=26.0 \
  -DCMAKE_FIND_FRAMEWORK=LAST \
  -DPython3_EXECUTABLE="$tools/bin/python" \
  -DCURL_ZLIB=OFF
cmake --build "$build" --target Game --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-8}"

mkdir -p "$build/demo/Data"
cp "$build/app/Game" "$build/demo/Game"
cp "$repo/test/text_input/Game.ini" "$build/demo/Game.ini"
ruby "$repo/test/text_input/pack_scripts.rb" "$repo/test/text_input/main.rb" "$build/demo/Data/Scripts.rxdata"
printf '\nRun %s\n' "$build/demo/Game"
