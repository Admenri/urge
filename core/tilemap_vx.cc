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

#include "core/tilemap_vx.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

#include "core/config.h"
#include "core/graphics.h"
#include "core/pipeline.h"

namespace urge {

namespace {

const RectF kAutotileSrcRegular[] = {
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f},
};

const RectF kAutotileSrcTable[] = {
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {1.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {0.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.0f, 0.0f}, {0.5f, 0.5f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {0.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 2.0f, 0.5f, 0.5f}, {1.5f, 2.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 2.5f, 0.5f, 0.5f}, {1.5f, 2.5f, 0.5f, 0.5f},
    {0.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f},
};

const RectF kAutotileSrcWall[] = {
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.5f, 0.5f}, {0.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {0.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 0.5f, 0.5f, 0.5f}, {1.5f, 0.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {0.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.0f, 0.5f, 0.5f}, {0.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {0.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 1.0f, 0.5f, 0.5f}, {1.5f, 1.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {1.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {1.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
    {0.0f, 0.0f, 0.5f, 0.5f}, {1.5f, 0.0f, 0.5f, 0.5f},
    {0.0f, 1.5f, 0.5f, 0.5f}, {1.5f, 1.5f, 0.5f, 0.5f},
};

const RectF kAutotileSrcWaterfall[] = {
    {1.0f, 0.0f, 0.5f, 1.0f}, {0.5f, 0.0f, 0.5f, 1.0f},
    {0.0f, 0.0f, 0.5f, 1.0f}, {0.5f, 0.0f, 0.5f, 1.0f},
    {1.0f, 0.0f, 0.5f, 1.0f}, {1.5f, 0.0f, 0.5f, 1.0f},
    {0.0f, 0.0f, 0.5f, 1.0f}, {1.5f, 0.0f, 0.5f, 1.0f},
};

}  // namespace

TilemapVXAbove::TilemapVXAbove(TilemapVX* parent, RefPtr<Viewport> viewport)
    : Node(viewport, ZValue(200)), parent_(parent) {}

void TilemapVXAbove::DisposeObject() {}

bool TilemapVXAbove::Prepare(DrawParam param) {
  return parent_ && parent_->HasAboveLayer();
}

bool TilemapVXAbove::DoDraw(DrawParam param) {
  parent_->DrawAboveLayer(param);
  return false;
}

TilemapVX::TilemapVX(RefPtr<Viewport> viewport, int32_t tilesize)
    : Node(viewport, ZValue()),
      rgss3_style_(Config::Get().vxa()),
      above_(MakeRefCounted<TilemapVXAbove>(this, viewport)),
      tilesize_(tilesize) {
  Node::SetupTrait(this);
  CreateShadowSet();
}

TilemapVX::~TilemapVX() {
  Disposable::Dispose();
}

void TilemapVX::Update() {
  if (++frame_index_ >= 30 * 3 * 4)
    frame_index_ = 0;

  const uint8_t kAniIndicesRegular[3 * 4] = {0, 1, 2, 1, 0, 1,
                                             2, 1, 0, 1, 2, 1};
  const uint8_t kAniIndicesWaterfall[3 * 4] = {0, 1, 2, 0, 1, 2,
                                               0, 1, 2, 0, 1, 2};

  regular_anim_ = kAniIndicesRegular[frame_index_ / 30];
  waterfall_anim_ = kAniIndicesWaterfall[frame_index_ / 30];

  flash_timer_ = ++flash_timer_ % 32;
  flash_opacity_ = std::abs(16 - flash_timer_) * 8 + 32;
}

void TilemapVX::SetBitmap(int32_t index, RefPtr<Bitmap> bitmap) {
  bitmaps_[index] = bitmap;
}

RefPtr<Bitmap> TilemapVX::GetBitmap(int32_t index) {
  return bitmaps_[index];
}

ATTR_DEF(TilemapVX, RefPtr<Viewport>, Viewport) {
  if (value.has_value() && above_)
    above_->Attr_Parent(*value);

  auto parent_value = Node::Attr_Parent(value);
  if (parent_value.has_value()) {
    auto parent = *parent_value;
    Viewport* viewport = parent ? parent->TryCast<Viewport>() : nullptr;
    return RefPtr<Viewport>(viewport);
  } else {
    return std::nullopt;
  }
}

ATTR_DEF(TilemapVX, bool, Visible) {
  if (value.has_value() && above_)
    above_->Attr_Visible(*value);
  return Node::Attr_Visible(value);
}

ATTR_DEF(TilemapVX, int32_t, Z) {
  if (value.has_value() && above_)
    above_->Attr_Z(*value + 200);
  return Node::Attr_Z(value);
}

ATTR_DEF(TilemapVX, RefPtr<Table>, MapData) {
  if (value.has_value()) {
    map_data_ = *value;
    return std::nullopt;
  } else {
    return map_data_;
  }
}

ATTR_DEF(TilemapVX, RefPtr<Table>, FlashData) {
  if (value.has_value()) {
    flash_data_ = *value;
    return std::nullopt;
  } else {
    return flash_data_;
  }
}

ATTR_DEF(TilemapVX, RefPtr<Table>, Flags) {
  if (value.has_value()) {
    flags_ = *value;
    return std::nullopt;
  } else {
    return flags_;
  }
}

ATTR_DEF(TilemapVX, RefPtr<Table>, Passages) {
  return Attr_Flags(value);
}

ATTR_DEF(TilemapVX, int32_t, OX) {
  if (value.has_value()) {
    ox_ = *value;
    return std::nullopt;
  } else {
    return ox_;
  }
}

ATTR_DEF(TilemapVX, int32_t, OY) {
  if (value.has_value()) {
    oy_ = *value;
    return std::nullopt;
  } else {
    return oy_;
  }
}

void TilemapVX::DisposeObject() {
  Node::DisposeObject();

  above_.reset();
  shadow_texture_.reset();
  map_layer_.primitive.Reset();
  above_layer_.primitive.Reset();
  map_layer_.draws.clear();
  above_layer_.draws.clear();
}

bool TilemapVX::Prepare(DrawParam param) {
  UpdateViewport();

  object_slot_ = {};
  BuildLayer(false, &map_layer_);
  BuildLayer(true, &above_layer_);

  if (map_layer_.valid || above_layer_.valid) {
    UniformManager& uniforms = UniformManager::Get();
    const ObjectData object_data = {glm::mat4(1.0f)};
    object_slot_ = uniforms.object_uniforms().Acquire(object_data);
  }

  return true;
}

bool TilemapVX::DoDraw(DrawParam param) {
  DrawLayer(param, &map_layer_);
  return false;
}

void TilemapVX::CreateShadowSet() {
  shadow_texture_ = MakeRefCounted<Bitmap>(16 * tilesize_, tilesize_);

  const int32_t half = tilesize_ / 2;
  RefPtr<Color> tint = MakeRefCounted<Color>(0.0f, 0.0f, 0.0f, 128.0f);

  for (int32_t i = 0; i < 16; ++i) {
    const int32_t offset = i * tilesize_;

    if (i & 0x1)
      shadow_texture_->FillRect(offset, 0, half, half, tint);
    if (i & 0x2)
      shadow_texture_->FillRect(offset + half, 0, half, half, tint);
    if (i & 0x4)
      shadow_texture_->FillRect(offset, half, half, half, tint);
    if (i & 0x8)
      shadow_texture_->FillRect(offset + half, half, half, half, tint);
  }
}

void TilemapVX::UpdateViewport() {
  auto viewport = Attr_Viewport();
  int32_t viewport_ox = 0, viewport_oy = 0;
  int32_t viewport_width = 0, viewport_height = 0;
  if (viewport.has_value() && viewport.value()) {
    auto rect = viewport.value()->Attr_Rect();
    viewport_ox = viewport.value()->Attr_OX().value();
    viewport_oy = viewport.value()->Attr_OY().value();
    viewport_width = rect.value()->data.width;
    viewport_height = rect.value()->data.height;
  } else {
    viewport_width = Graphics::Get().Width();
    viewport_height = Graphics::Get().Height();
  }

  const int32_t tilemap_real_ox = ox_ + viewport_ox;
  const int32_t tilemap_real_oy = oy_ + viewport_oy;

  render_viewport_.x = tilemap_real_ox / tilesize_;
  render_viewport_.y = tilemap_real_oy / tilesize_ - 1;
  render_viewport_.width =
      (viewport_width / tilesize_) + !!(viewport_width % tilesize_) + 1;
  render_viewport_.height =
      (viewport_height / tilesize_) + !!(viewport_height % tilesize_) + 2;

  const int32_t display_offset_x = tilemap_real_ox % tilesize_;
  const int32_t display_offset_y = tilemap_real_oy % tilesize_;
  render_offset_ = glm::vec2(static_cast<float>(-display_offset_x),
                             static_cast<float>(-display_offset_y));
  render_offset_.y -= static_cast<float>(tilesize_);
}

void TilemapVX::CollectMapData(bool above, std::vector<TileQuad>* quads) {
  quads->clear();

  auto push_quad = [&](const RefPtr<Bitmap>& texture, const RectF& source,
                       const RectF& destination) {
    TileQuad quad;
    quad.texture = texture;
    quad.source = source;
    quad.destination = destination;
    quads->push_back(quad);
  };

  auto tilesize_src = [&](const RectF& raw) {
    return RectF(raw.x * tilesize_, raw.y * tilesize_, raw.width * tilesize_,
                 raw.height * tilesize_);
  };

  auto value_wrap = [&](int32_t value, int32_t range) {
    int32_t res = value % range;
    return res < 0 ? res + range : res;
  };

  auto get_wrap_data = [&](RefPtr<Table> t, int32_t x, int32_t y,
                           int32_t z) -> int16_t {
    if (!t)
      return 0;

    auto tile_x = xrepeat_ ? value_wrap(x, t->Xsize()) : x;
    auto tile_y = yrepeat_ ? value_wrap(y, t->Ysize()) : y;

    if (!xrepeat_ && (x < 0 || x >= t->Xsize()))
      return 0;
    if (!yrepeat_ && (y < 0 || y >= t->Ysize()))
      return 0;

    return t->Get(tile_x, tile_y, z);
  };

  auto get_map_flag = [&](RefPtr<Table> t, int32_t tile_id) -> int16_t {
    if (!t)
      return 0;

    if (tile_id < 0 || tile_id >= t->Xsize())
      return 0;

    return t->Get(tile_id, 0, 0);
  };

  auto autotile_set_pos = [&](RectF& pos, int32_t i) {
    switch (i) {
      case 0:
        break;
      case 1:
        pos.x += tilesize_ / 2.0f;
        break;
      case 2:
        pos.y += tilesize_ / 2.0f;
        break;
      case 3:
        pos.x += tilesize_ / 2.0f;
        pos.y += tilesize_ / 2.0f;
        break;
      case 4:
        pos.y += tilesize_ * 0.75f;
        break;
      case 5:
        pos.x += tilesize_ / 2.0f;
        pos.y += tilesize_ * 0.75f;
        break;
      default:
        break;
    }
  };

  auto read_autotile_common = [&](int32_t pattern_id,
                                  const RefPtr<Bitmap>& texture,
                                  const glm::vec2& offset, int32_t x, int32_t y,
                                  const RectF* rect_src) {
    for (int32_t i = 0; i < 4; ++i) {
      RectF tex_rect = tilesize_src(rect_src[pattern_id * 4 + i]);
      tex_rect.x += offset.x * tilesize_ + 0.5f;
      tex_rect.y += offset.y * tilesize_ + 0.5f;
      tex_rect.width -= 1.0f;
      tex_rect.height -= 1.0f;

      RectF pos_rect(x * tilesize_, y * tilesize_, tilesize_ / 2.0f,
                     tilesize_ / 2.0f);
      autotile_set_pos(pos_rect, i);

      push_quad(texture, tex_rect, pos_rect);
    }
  };

  auto read_autotile_table = [&](int32_t pattern_id,
                                 const RefPtr<Bitmap>& texture,
                                 const glm::vec2& offset, int32_t x, int32_t y,
                                 bool occlusion) {
    for (int32_t i = 0; i < 6; ++i) {
      const RectF tile_src = kAutotileSrcTable[pattern_id * 6 + i];
      RectF tex_rect = tilesize_src(tile_src);
      tex_rect.x += offset.x * tilesize_ + 0.5f;
      tex_rect.y += offset.y * tilesize_ + 0.5f;
      tex_rect.width = std::max(0.0f, tex_rect.width - 1.0f);
      tex_rect.height = std::max(0.0f, tex_rect.height - 1.0f);

      RectF pos_rect(x * tilesize_, y * tilesize_, tile_src.width * tilesize_,
                     tile_src.height * tilesize_);
      autotile_set_pos(pos_rect, i);

      if (occlusion && i >= 4) {
        const float table_leg = tilesize_ * 0.25f;
        tex_rect.height -= table_leg;
        pos_rect.height -= table_leg;
      }

      if (tex_rect.width <= 0.0f || tex_rect.height <= 0.0f)
        continue;

      push_quad(texture, tex_rect, pos_rect);
    }
  };

  auto read_autotile_waterfall = [&](int32_t pattern_id,
                                     const RefPtr<Bitmap>& texture,
                                     const glm::vec2& offset, int32_t x,
                                     int32_t y) {
    if (pattern_id > 0x3)
      return;

    for (size_t i = 0; i < 2; ++i) {
      RectF tex_rect = tilesize_src(kAutotileSrcWaterfall[pattern_id * 2 + i]);
      tex_rect.x += offset.x * tilesize_ + 0.5f;
      tex_rect.y += offset.y * tilesize_ + 0.5f;
      tex_rect.width -= 1.0f;
      tex_rect.height -= 1.0f;

      RectF pos_rect(x * tilesize_ + i * (tilesize_ / 2.0f), y * tilesize_,
                     tilesize_ / 2.0f, tilesize_);

      push_quad(texture, tex_rect, pos_rect);
    }
  };

  auto process_tile_A1 = [&](int16_t tile_id, int32_t x, int32_t y) {
    auto& bitmap = bitmaps_[TILE_A1];
    if (!Disposable::Check(bitmap))
      return;

    tile_id -= 0x0800;
    const int32_t autotile_id = tile_id / 0x30;
    const int32_t pattern_id = tile_id % 0x30;

    const glm::vec2 waterfall(-1, -1);
    const glm::vec2 src_offset[] = {{0, 0}, {0, 3},    {6, 0}, {6, 3},
                                    {8, 0}, waterfall, {8, 3}, waterfall,
                                    {0, 6}, waterfall, {0, 9}, waterfall,
                                    {8, 6}, waterfall, {8, 9}, waterfall};
    const glm::vec2 waterfall_offset[] = {
        {14, 0}, {14, 3}, {6, 6}, {6, 9}, {14, 6}, {14, 9},
    };

    glm::vec2 src_pos = src_offset[autotile_id];
    bool waterfall_component = (src_pos.x == -1);
    bool regular_component =
        !waterfall_component && autotile_id != 2 && autotile_id != 3;

    if (waterfall_component) {
      src_pos.y += waterfall_anim_;
      read_autotile_waterfall(pattern_id, bitmap,
                              waterfall_offset[(autotile_id - 5) / 2], x, y);
    } else {
      if (regular_component)
        src_pos.x += 2 * regular_anim_;
      read_autotile_common(pattern_id, bitmap, src_pos, x, y,
                           kAutotileSrcRegular);
    }
  };

  auto process_tile_A2 = [&](int16_t tile_id, int32_t x, int32_t y,
                             bool is_table, bool occlusion) {
    auto& bitmap = bitmaps_[TILE_A2];
    if (!Disposable::Check(bitmap))
      return;

    tile_id -= 0x0B00;
    const int32_t autotile_id = tile_id / 0x30;
    const int32_t pattern_id = tile_id % 0x30;

    glm::vec2 offset((autotile_id % 8) * 2, (autotile_id / 8) * 3);
    if (is_table) {
      read_autotile_table(pattern_id, bitmap, offset, x, y, occlusion);
    } else {
      read_autotile_common(pattern_id, bitmap, offset, x, y,
                           kAutotileSrcRegular);
    }
  };

  auto process_tile_A3 = [&](int16_t tile_id, int32_t x, int32_t y) {
    auto& bitmap = bitmaps_[TILE_A3];
    if (!Disposable::Check(bitmap))
      return;

    tile_id -= 0x1100;
    const int32_t autotile_id = tile_id / 0x30;
    const int32_t pattern_id = tile_id % 0x30;
    if (pattern_id >= 0x10)
      return;

    const glm::vec2 offset((autotile_id % 8) * 2, (autotile_id / 8) * 2);
    read_autotile_common(pattern_id, bitmap, offset, x, y, kAutotileSrcWall);
  };

  auto process_tile_A4 = [&](int16_t tile_id, int32_t x, int32_t y) {
    auto& bitmap = bitmaps_[TILE_A4];
    if (!Disposable::Check(bitmap))
      return;

    tile_id -= 0x1700;
    const int32_t autotile_id = tile_id / 0x30;
    const int32_t pattern_id = tile_id % 0x30;

    const int32_t vertical_offset[] = {0, 3, 5, 8, 10, 13};
    const int32_t offset_index = autotile_id / 8;
    const glm::vec2 offset((autotile_id % 8) * 2,
                           vertical_offset[offset_index]);

    if (!(offset_index % 2)) {
      read_autotile_common(pattern_id, bitmap, offset, x, y,
                           kAutotileSrcRegular);
    } else {
      if (pattern_id >= 0x10)
        return;

      read_autotile_common(pattern_id, bitmap, offset, x, y, kAutotileSrcWall);
    }
  };

  auto process_tile_A5 = [&](int16_t tile_id, int32_t x, int32_t y) {
    auto& bitmap = bitmaps_[TILE_A5];
    if (!Disposable::Check(bitmap))
      return;

    tile_id -= 0x0600;
    const int32_t ox = tile_id % 0x8;
    const int32_t oy = tile_id / 0x8;

    const RectF tex(ox * tilesize_ + 0.5f, oy * tilesize_ + 0.5f,
                    tilesize_ - 1.0f, tilesize_ - 1.0f);
    const RectF pos(x * tilesize_, y * tilesize_, tilesize_, tilesize_);

    push_quad(bitmap, tex, pos);
  };

  auto process_tile_bcde = [&](int16_t tile_id, int32_t x, int32_t y) {
    int32_t tile_type = tile_id / 0x100;
    tile_id = tile_id % 0x100;

    auto& bitmap = bitmaps_[TILE_B + tile_type];
    if (!Disposable::Check(bitmap))
      return;

    int32_t ox = tile_id % 0x8;
    int32_t oy = (tile_id / 0x8) % 0x10;
    int32_t ob = tile_id / (0x8 * 0x10);

    ox += (ob % 2) * 0x8;
    oy += (ob / 2) * 0x10;

    const RectF tex(ox * tilesize_ + 0.5f, oy * tilesize_ + 0.5f,
                    tilesize_ - 1.0f, tilesize_ - 1.0f);
    const RectF pos(x * tilesize_, y * tilesize_, tilesize_, tilesize_);

    push_quad(bitmap, tex, pos);
  };

  auto process_shadow_tile = [&](int8_t shadow_id, int32_t x, int32_t y) {
    const RectF tex(shadow_id * tilesize_ + 0.5f, tilesize_ + 0.5f,
                    tilesize_ - 1.0f, tilesize_ - 1.0f);
    const RectF pos(x * tilesize_, y * tilesize_, tilesize_, tilesize_);

    push_quad(shadow_texture_, tex, pos);
  };

  auto process_common_tile = [&](int16_t tile_id, int32_t x, int32_t y,
                                 int32_t z, int16_t under_tile_id) {
    int16_t flag = get_map_flag(flags_, tile_id);
    bool over_player = (flag & 0x10) && (z >= 2);
    bool is_table = rgss3_style_
                        ? (flag & 0x80)
                        : (tile_id - 0x0B00) % (8 * 0x30) >= (7 * 0x30);

    if (over_player != above)
      return;

    if (tile_id >= 0x0800 && tile_id < 0x0B00)
      return process_tile_A1(tile_id, x, y);
    if (tile_id >= 0x0B00 && tile_id < 0x1100)
      return process_tile_A2(tile_id, x, y, is_table,
                             under_tile_id >= 0x1100 && under_tile_id < 0x2000);
    if (tile_id >= 0x1100 && tile_id < 0x1700)
      return process_tile_A3(tile_id, x, y);
    if (tile_id >= 0x1700 && tile_id < 0x2000)
      return process_tile_A4(tile_id, x, y);
    if (tile_id >= 0x0600 && tile_id < 0x0680)
      return process_tile_A5(tile_id, x, y);
    if (tile_id < 0x0400)
      return process_tile_bcde(tile_id, x, y);
  };

  auto process_shadow_layer = [&](int32_t ox, int32_t oy, int32_t w,
                                  int32_t h) {
    if (rgss3_style_) {
      for (int32_t y = 0; y < h; ++y) {
        for (int32_t x = 0; x < w; ++x) {
          int16_t shadow_id = get_wrap_data(map_data_, x + ox, y + oy, 3);
          process_shadow_tile(shadow_id, x, y);
        }
      }
    } else {
      if (!map_data_)
        return;

      for (int32_t y = 0; y < h; ++y) {
        for (int32_t x = 0; x < w; ++x) {
          if ((x + ox) % map_data_->Xsize() == 0 ||
              (y + oy) % map_data_->Ysize() == 0)
            continue;

          const int16_t wall_top =
              get_wrap_data(map_data_, x + ox - 1, y + oy - 1, 0);
          const int16_t wall_bottom =
              get_wrap_data(map_data_, x + ox - 1, y + oy, 0);
          const int16_t current_tile =
              get_wrap_data(map_data_, x + ox, y + oy, 0);

          const bool shadow_floor =
              (current_tile >= 0x0B00 && current_tile < 0x1100) ||
              (current_tile >= 0x0600 && current_tile < 0x0680);

          if ((wall_top >= 0x1100 && wall_top < 0x2000) &&
              (wall_bottom >= 0x1100 && wall_bottom < 0x2000) && shadow_floor) {
            process_shadow_tile(0x05, x, y);
          }
        }
      }
    }
  };

  auto process_common_layer = [&](int32_t ox, int32_t oy, int32_t w, int32_t h,
                                  int32_t z) {
    for (int32_t y = h - 1; y >= 0; --y) {
      for (int32_t x = 0; x < w; ++x) {
        const int16_t tile_id = get_wrap_data(map_data_, x + ox, y + oy, z);
        if (!tile_id)
          continue;

        const int16_t under_tile_id =
            get_wrap_data(map_data_, x + ox, y + oy + 1, 0);

        process_common_tile(tile_id, x, y, z, under_tile_id);
      }
    }
  };

  const int32_t ox = render_viewport_.x, oy = render_viewport_.y;
  const int32_t w = render_viewport_.width, h = render_viewport_.height;

  process_common_layer(ox, oy, w, h, 0);
  process_common_layer(ox, oy, w, h, 1);

  if (!above)
    process_shadow_layer(ox, oy, w, h);

  process_common_layer(ox, oy, w, h, 2);
}

void TilemapVX::BuildLayer(bool above, TileLayer* layer) {
  std::vector<TileQuad> quads;
  CollectMapData(above, &quads);

  layer->draws.clear();
  layer->valid = false;

  layer->primitive.Clear();

  const glm::vec4 color(1.0f);

  size_t index = 0;
  while (index < quads.size()) {
    const RefPtr<Bitmap>& texture = quads[index].texture;

    if (!Disposable::Check(texture)) {
      ++index;
      continue;
    }

    const glm::vec2 texture_size(static_cast<float>(texture->size().x),
                                 static_cast<float>(texture->size().y));

    layer->primitive.BeginQuad().Color4f(color);
    while (index < quads.size() && quads[index].texture == texture) {
      RectF dest = quads[index].destination;
      dest.x += render_offset_.x;
      dest.y += render_offset_.y;

      layer->primitive.Rect(dest, MakeNorm(quads[index].source, texture_size));
      ++index;
    }

    const PrimitiveEmitter::Slot run = layer->primitive.End();
    if (run.count) {
      TileDraw draw;
      draw.texture = texture;
      draw.slot = run;
      layer->draws.push_back(std::move(draw));
    }
  }

  if (layer->draws.empty())
    return;

  layer->primitive.Upload();
  layer->valid = layer->primitive.buffer() != nullptr;
  if (!layer->valid)
    layer->draws.clear();
}

void TilemapVX::DrawLayer(DrawParam param, TileLayer* layer) {
  if (!layer->valid || layer->draws.empty() ||
      object_slot_.chunk == UniformBlockPool::kInvalidChunk)
    return;

  UniformManager& uniforms = UniformManager::Get();
  const UniformBlockPool::Chunk& object_chunk =
      uniforms.object_uniforms().chunk(object_slot_.chunk);

  param->pass.SetPipeline(ShaderSet::Get().state.tilemap.texture_dynamic_pma);
  param->pass.SetBindGroup(0, param->scene, 0, nullptr);
  param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
  param->pass.SetVertexBuffer(0, layer->primitive.buffer(), 0, WGPU_WHOLE_SIZE);

  for (const TileDraw& draw : layer->draws) {
    param->pass.SetBindGroup(2, draw.texture->texture_group(), 0, nullptr);
    param->pass.Draw(draw.slot.count, 1, draw.slot.first, 0);
  }
}

void TilemapVX::DrawAboveLayer(DrawParam param) {
  DrawLayer(param, &above_layer_);
}

}  // namespace urge
