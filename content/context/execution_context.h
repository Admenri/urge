// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CONTEXT_EXECUTION_CONTEXT_H_
#define CONTENT_CONTEXT_EXECUTION_CONTEXT_H_

#include <memory>

namespace content {

class ScopedFontData;

class ExecutionContext {
 public:
  ~ExecutionContext() = default;

  ExecutionContext(const ExecutionContext&) = delete;
  ExecutionContext& operator=(const ExecutionContext&) = delete;

  std::unique_ptr<ExecutionContext> MakeContext();

  ScopedFontData* GetFontContext();

 private:
  ExecutionContext();

  ScopedFontData* font_context_;
};

}  // namespace content

#endif  //! CONTENT_CONTEXT_EXECUTION_CONTEXT_H_
