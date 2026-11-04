#!/bin/bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../../.." && pwd)"
target="${URGE_MACOS_DEPLOYMENT_TARGET:-13.3}"
build="${URGE_NATIVE_BUILD_DIR:-$repo/build/macos-native}"
tools="$repo/build/macos-tools"
if [[ ! -x "$tools/bin/python" ]]; then python3 -m venv "$tools"; fi
"$tools/bin/python" -m pip install -r "$repo/requirements.txt"
export PATH="$tools/bin:$PATH"

# Do not let shell include/library overrides inject package-manager libraries.
unset CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH LIBRARY_PATH CPPFLAGS LDFLAGS
unset CMAKE_PREFIX_PATH CMAKE_INCLUDE_PATH CMAKE_LIBRARY_PATH CMAKE_FRAMEWORK_PATH
arch="$(uname -m)"
"$tools/bin/python" "$repo/third_party/macos/prepare-deps.py" --arch "$arch" --target "$target"
prefix="$repo/build/macos-deps/$arch-$target/prefix"
configure=(cmake)
# Discard dependency probes from the earlier system-library build once. In
# particular, curl caches pkg-config results even if pkg-config is later off.
if [[ ! -f "$build/.static-deps-version" ]] || [[ "$(cat "$build/.static-deps-version")" != "2:$prefix" ]]; then
  configure+=(--fresh)
fi

# Configure the native Metal engine with repository-built static dependencies.
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
"$tools/bin/python" "$repo/third_party/macos/audit-native.py" "$build/app/Game" \
  --target "$target" --archives-root "$prefix" --archives-root "$build"
printf '2:%s\n' "$prefix" > "$build/.static-deps-version"

cp -R "$prefix/share/urge/licenses" "$build/"
cp "$repo/core/audio/decoders/LICENSES.md" "$build/licenses/audio-codecs.txt"

# The bundle and the standalone executable use the same native Metal engine.
# Keep data under Resources, outside the bundle's signed executable directory.
bundle="$build/URGE.app"
mkdir -p "$bundle/Contents/MacOS" "$bundle/Contents/Resources/licenses"
cp "$build/licenses/audio-codecs.txt" "$bundle/Contents/Resources/licenses/"
# Remove demo resources left by previous builds.
rm -rf "$bundle/Contents/Resources/Demo" "$build/demo"
rm -rf "$bundle/Contents/MacOS/Data"
rm -f "$bundle/Contents/MacOS/Game.ini" "$bundle/Contents/MacOS/Game.log"
cp "$build/app/Game" "$bundle/Contents/MacOS/Game"
cp "$repo/app/platform/macos/NativeInfo.plist" "$bundle/Contents/Info.plist"
/usr/libexec/PlistBuddy -c "Set :LSMinimumSystemVersion $target" "$bundle/Contents/Info.plist"
"$tools/bin/python" "$repo/app/platform/macos/build-icon.py" icns "$repo/app/resources/urge_favicon.png" \
  "$bundle/Contents/Resources/URGE.icns"
codesign --force --sign - "$bundle"
printf '\nRun %s or open %s\n' "$build/app/Game" "$bundle"
