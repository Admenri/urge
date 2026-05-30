// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

namespace content {

class ExternalBinding {
 public:
  virtual ~ExternalBinding() = default;

  enum class Result {
    CONTINUE,
    SUCCESS,
    FAILURE,
  };

  // Call on binding initialization
  virtual Result BindingInit() { return Result::CONTINUE; }

  // Call per frame
  virtual Result RunningIterate() { return Result::CONTINUE; }

  // Call after running main looping
  virtual void BindingQuit() {}
};

}  // namespace content
