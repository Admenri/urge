#!/bin/bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../../.." && pwd)"
target="${URGE_MACOS_DEPLOYMENT_TARGET:-13.3}"
build="${URGE_NATIVE_BUILD_DIR:-$repo/build/macos-native}"
tools="$repo/build/macos-tools"
if [[ ! -x "$tools/bin/python" ]]; then python3 -m venv "$tools"; fi
"$tools/bin/python" -m pip install -r "$repo/requirements.txt"
export PATH="$tools/bin:$PATH"

arch="$(uname -m)"
"$tools/bin/python" "$repo/app/platform/macos/prepare-deps.py" --arch "$arch" --target "$target"
prefix="$repo/build/macos-deps/$arch-$target/prefix"
configure=(cmake)
# Discard dependency probes from the earlier system-library build once. In
# particular, curl caches pkg-config results even if pkg-config is later off.
if [[ ! -f "$build/.static-deps-version" ]] || [[ "$(cat "$build/.static-deps-version")" != "1:$prefix" ]]; then
  configure+=(--fresh)
fi

# Keep the native runner separate from the Wasm/WKWebView build.
"${configure[@]}" -S "$repo" -B "$build" -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_OSX_ARCHITECTURES="$arch" \
  -DCMAKE_OSX_DEPLOYMENT_TARGET="$target" \
  -DCMAKE_FIND_FRAMEWORK=LAST \
  -DCMAKE_C_FLAGS="-Werror=unguarded-availability -Werror=unguarded-availability-new" \
  -DCMAKE_CXX_FLAGS="-Werror=unguarded-availability -Werror=unguarded-availability-new" \
  -DCMAKE_OBJC_FLAGS="-Werror=unguarded-availability -Werror=unguarded-availability-new" \
  -DCMAKE_OBJCXX_FLAGS="-Werror=unguarded-availability -Werror=unguarded-availability-new" \
  -DPython3_EXECUTABLE="$tools/bin/python" \
  -DURGE_MACOS_DEPS_PREFIX="$prefix"
cmake --build "$build" --target Game --parallel "${CMAKE_BUILD_PARALLEL_LEVEL:-8}"
"$tools/bin/python" "$repo/app/platform/macos/audit-native.py" "$build/app/Game" \
  --target "$target" --archives-root "$prefix" --archives-root "$build"
printf '1:%s\n' "$prefix" > "$build/.static-deps-version"

mkdir -p "$build/demo/Data"
cp "$build/app/Game" "$build/demo/Game"
cp "$repo/test/text_input/Game.ini" "$build/demo/Game.ini"
ruby "$repo/test/text_input/pack_scripts.rb" "$repo/test/text_input/main.rb" "$build/demo/Data/Scripts.rxdata"
cp -R "$prefix/share/urge/licenses" "$build/"
printf '\nRun %s\n' "$build/demo/Game"
