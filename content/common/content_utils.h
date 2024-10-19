// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_UTILS_H_
#define CONTENT_COMMON_CONTENT_UTILS_H_

#include "fiber/fiber.h"

#include <iostream>
#include <sstream>

#define CONTENT_EXPORT

namespace content {

// Set engine kernel adapt version.
// Compat with RGSS-based content diff.
enum class APIVersion : int32_t {
  Null = 0,
  RGSS1 = 1,
  RGSS2 = 2,
  RGSS3 = 3,
};

// Content task scheduler data structure.
// Switch fibers for compating with Emscripten platform's event loop.
struct CoroutineContext {
  // Main fiber controller
  fiber_t* primary_fiber;

  // Binding fiber controller
  fiber_t* main_loop_fiber;

  // Frame render skip count
  bool frame_skip_require;

  CoroutineContext()
      : primary_fiber(nullptr),
        main_loop_fiber(nullptr),
        frame_skip_require(false) {}

  ~CoroutineContext() {
    if (main_loop_fiber)
      fiber_delete(main_loop_fiber);
    if (primary_fiber)
      fiber_delete(primary_fiber);
  }
};

// Content layer universal output interface.
// For content debug commands using.
class Debug {
 public:
  Debug() = default;
  ~Debug() {
#ifdef __ANDROID__
    __android_log_write(ANDROID_LOG_DEBUG, "[LOG]", ss_.str().c_str());
#else
    std::cerr << "[LOG] " << ss_.str() << std::endl;
#endif
  }

  template <typename T>
  Debug& operator<<(const T& t) {
    ss_ << t;
    return *this;
  }

 private:
  std::stringstream ss_;
};

}  // namespace content

#endif  //! CONTENT_COMMON_CONTENT_UTILS_H_
