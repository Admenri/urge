// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <string>

namespace urge {

/* The encrypted asset container.
 *
 * A shipped game carries its resources in a single `<name>.acs` file next to
 * the executable -- `Game.exe` reads `Game.acs`, the way it already reads
 * `Game.ini`.  The container is a PhysicsFS archive, so once it is registered
 * and mounted every read through `IOService` goes through it and nothing else
 * in the engine has to know it is there: `Bitmap.new("Graphics/...")`,
 * `load_data`, the audio mixer and the font loader all keep working as they
 * do against a plain directory.
 *
 * The implementation is not in this tree.  It lives in the `admenri/` drop-in,
 * which is git-ignored, and this header is the whole of what the open-source
 * side sees of it: a clone without `admenri/` compiles `core/archive.cc`,
 * whose three functions answer "not available" and mount nothing.  The
 * encryption is therefore a property of a build, not of the source.
 */
namespace archive {

/* True when this build carries the container archiver, i.e. `admenri/` was
   present when CMake configured the project. */
bool Available();

/* Registers the `.acs` archiver with PhysicsFS, which must be up already.
   `IOService` calls this from its constructor, so a caller holding an
   `IOService` never has to; calling it again is a no-op. */
void Register();

/* Mounts `path` at the archive root.  Prepending is the default, so a
   container shadows the loose directory a development build reads instead.
   Returns false when this build has no archiver, or when the mount failed. */
bool Mount(const std::string& path, bool prepend = true);

}  // namespace archive

}  // namespace urge
