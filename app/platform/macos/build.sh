#!/bin/bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../../.." && pwd)"
work="$repo/build/macos-webview"
wasm="$repo/build/emscripten-wasm32"
bundle="$work/URGE.app"
mkdir -p "$work/demo/Data"

if [[ "${1:-}" != "--skip-wasm" ]]; then
  command -v emcc >/dev/null || { echo 'Source emsdk_env.sh before building.' >&2; exit 1; }
  export EMSDK="${EMSDK:-$(python3 -c 'import pathlib,shutil; print(pathlib.Path(shutil.which("emcc")).resolve().parents[2])')}"
  if [[ ! -x "$work/tools/bin/python" ]]; then python3 -m venv "$work/tools"; fi
  "$work/tools/bin/python" -m pip install -r "$repo/requirements.txt"
  export PATH="$work/tools/bin:$PATH"
  ruby "$repo/test/text_input/pack_scripts.rb" "$repo/test/text_input/main.rb" "$work/demo/Data/Scripts.rxdata"
  cp "$repo/test/text_input/Game.ini" "$work/demo/Game.ini"
  cmake --preset emscripten-wasm32 -S "$repo" -B "$wasm" \
    -DPython3_EXECUTABLE="$work/tools/bin/python" -DURGE_WEB_ASSETS="$work/demo" \
    -DZLIB_BUILD_SHARED=OFF -DZLIB_BUILD_TESTING=OFF \
    '-DCMAKE_EXE_LINKER_FLAGS=-sFS_DEBUG=1 -sEXPORTED_RUNTIME_METHODS=FS -sEXIT_RUNTIME=1'
  cmake --build "$wasm" --target Game --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-8}"
fi

mkdir -p "$bundle/Contents/MacOS" "$bundle/Contents/Resources/Web" "$bundle/Contents/Resources/Demo/Data"
swiftc "$repo/app/platform/macos/App.swift" "$repo/app/platform/macos/LocalServer.swift" -O \
  -target "$(uname -m)-apple-macosx26.0" \
  -framework AppKit -framework WebKit -framework Network -o "$bundle/Contents/MacOS/URGE"
cp "$repo/app/platform/macos/Info.plist" "$bundle/Contents/Info.plist"
for name in Game.html Game.js Game.wasm urge-helper.js; do
  cp "$wasm/app/$name" "$bundle/Contents/Resources/Web/$name"
done
cp "$repo/app/platform/macos/host.js" "$bundle/Contents/Resources/Web/host.js"
cp "$work/demo/Game.ini" "$bundle/Contents/Resources/Demo/Game.ini"
cp "$work/demo/Data/Scripts.rxdata" "$bundle/Contents/Resources/Demo/Data/Scripts.rxdata"
codesign --force --sign - "$bundle"
printf '\nBuilt %s\n' "$bundle"
