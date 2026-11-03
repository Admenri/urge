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
#include <string_view>

namespace urge::ini_encoding {

// Validates Unicode scalar values and rejects embedded NULs in INI text.
bool IsUTF8(std::string_view text);

// Converts without replacing or discarding unrepresentable characters.
bool Convert(std::string_view text,
             const std::string& from,
             const std::string& to,
             std::string& output);

// BOMs take precedence, then an explicit encoding, then valid UTF-8. The
// legacy fallback is Windows' active code page, or GB18030 elsewhere. A legacy
// code page cannot be identified reliably from a short, BOM-less INI file.
// Returns false after filtering to ASCII if decoding fails; error is a warning.
// The caller must not save the filtered configuration over its original file.
bool Decode(const std::string& bytes,
            std::string& text,
            std::string& encoding,
            std::string& bom,
            std::string& error);

}  // namespace urge::ini_encoding
