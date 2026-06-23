// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <format>
#include <string>

#include "base/buildflags/compiler_specific.h"
#include "base/memory/stack_allocated.h"

namespace content {

enum ExceptionCode {
  NO_EXCEPTION = 0,
  CONTENT_ERROR,
  GPU_ERROR,
  IO_ERROR,
  CODE_NUMS,
};

class ExceptionState {
  STACK_ALLOCATED();

 public:
  ExceptionState() : code_(ExceptionCode::NO_EXCEPTION) {}

  ExceptionState(const ExceptionState&) = delete;
  ExceptionState& operator=(const ExceptionState&) = delete;

  // Throws a ContentException due to the given exception code.
  void Throw(ExceptionCode exception_code,
             const std::string_view format,
             auto&&... args) {
    code_ = exception_code;
    message_ = std::vformat(format, std::make_format_args(args...));
  }

  // Returns true if there is a pending exception.
  bool HadException() const { return code_ != ExceptionCode::NO_EXCEPTION; }

  // Fetch info for binding throw exception
  ExceptionCode FetchException(std::string& message) const {
    message = message_;
    return code_;
  }

 private:
  ExceptionCode code_;
  std::string message_;
};

}  // namespace content
