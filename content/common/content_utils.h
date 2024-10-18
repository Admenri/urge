// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_CONTENT_UTILS_H_
#define CONTENT_COMMON_CONTENT_UTILS_H_

#include "fiber/fiber.h"

#include <iostream>
#include <sstream>

namespace content {

// Set engine RGSS kernel adapt version
enum class APIVersion : int {
  Null = 0,
  RGSS1 = 1,
  RGSS2 = 2,
  RGSS3 = 3,
};

struct CoroutineContext {
  fiber_t* primary_fiber;
  fiber_t* main_loop_fiber;
};

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
