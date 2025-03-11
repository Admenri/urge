// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CONTEXT_EXCEPTION_STATE_H_
#define CONTENT_CONTEXT_EXCEPTION_STATE_H_

#include "base/buildflags/compiler_specific.h"
#include "base/memory/stack_allocated.h"

#include <string>

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
  ExceptionState()
      : had_exception_(false), code_(ExceptionCode::NO_EXCEPTION) {}
  ~ExceptionState() = default;

  ExceptionState(const ExceptionState&) = delete;
  ExceptionState& operator=(const ExceptionState&) = delete;

  // Throws a ContentException due to the given exception code.
  BASE_NOINLINE void ThrowContentError(ExceptionCode exception_code,
                                       const std::string& message);

  // Returns true if there is a pending exception.
  bool HadException() const { return had_exception_; }

  // Fetch info for binding throw exception
  ExceptionCode FetchException(std::string& message) const {
    message = message_;
    return code_;
  }

  ExceptionState& ReturnThis() { return *this; }

 private:
  int32_t had_exception_;
  ExceptionCode code_;
  std::string message_;
};

}  // namespace content

#endif  //! CONTENT_CONTEXT_EXCEPTION_STATE_H_
