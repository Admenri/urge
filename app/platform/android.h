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

#include "core/common/exception.h"

namespace platform {
namespace android {

/*! The directory the game reads from and writes to, unpacked from the APK.

    A game ships inside the APK's `assets/`, which is a zip: PhysicsFS can mount
    a directory, not an APK, and RGSS asks for its files by name in a way that
    needs a real one (the case-insensitive, extension-optional lookup in
    `IOService::OpenRead` walks the directory).  So the first launch of a new
    build unpacks `assets/` into the app's own external files directory --
    `Android/data/<package>/files` -- and the engine uses that as both the load
    path and the write path, exactly as it uses the game's folder on a desktop.

    The unpacking itself is Java (`URGEActivity.ensureBundle`): it compares the
    MD5 of the installed APK against the one recorded in `URGE_BUNDLE_MD5` and
    only unpacks when they differ, so a normal launch is one file read.  This
    function is the three lines of JNI that call it and hand back the path.

    Returns an empty string with `exception_state` set when the directory could
    not be prepared -- there is nothing the engine can do without it.
*/
std::string PrepareStorageRoot(urge::ExceptionState& exception_state);

}  // namespace android
}  // namespace platform
