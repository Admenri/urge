// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CONTEXT_ENGINE_OBJECT_H_
#define CONTENT_CONTEXT_ENGINE_OBJECT_H_

#include "content/content_config.h"

namespace content {

class EngineObject {
 public:
  EngineObject(ExecutionContext* execution_context)
      : execution_context_(execution_context) {}
  virtual ~EngineObject() = default;

  EngineObject(const EngineObject&) = delete;
  EngineObject& operator=(const EngineObject&) = delete;

 protected:
  ExecutionContext* context() const { return execution_context_; }

 private:
  ExecutionContext* execution_context_;
};

}  // namespace content

#endif  //! CONTENT_CONTEXT_ENGINE_OBJECT_H_
