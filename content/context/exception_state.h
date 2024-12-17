// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CONTEXT_EXCEPTION_STATE_H_
#define CONTENT_CONTEXT_EXCEPTION_STATE_H_

#include "base/buildflags/compiler_specific.h"
#include "base/memory/stack_allocated.h"

#include <string>

namespace content {

enum class ExceptionCode {
  kNoException = 0,
  kDisposedObject,
  kContentError,
  kIOError,
  kGPUError,
  kNums,
};

class ExceptionState {
  STACK_ALLOCATED();

 public:
  ExceptionState() : code_(ExceptionCode::kNoException), message_() {}
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
  }

  ExceptionState& ReturnThis() { return *this; }

 private:
  bool had_exception_ = false;
  ExceptionCode code_;
  std::string message_;
};

}  // namespace content

#endif  //! CONTENT_CONTEXT_EXCEPTION_STATE_H_
