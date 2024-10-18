// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BINDING_BINDING_ENGINE_H_
#define CONTENT_BINDING_BINDING_ENGINE_H_

#include "base/memory/ref_counted.h"

namespace content {

class EngineWorker;

class BindingEngine {
 public:
  BindingEngine();
  virtual ~BindingEngine();

  BindingEngine(const BindingEngine&) = delete;
  BindingEngine& operator=(const BindingEngine&) = delete;

  virtual void InitializeBinding(scoped_refptr<EngineWorker> binding_host);
  virtual void RunBindingMain();
  virtual void FinalizeBinding();

  virtual void QuitRequired();
  virtual void ResetRequired();
};

}  // namespace content

#endif  //! CONTENT_ENGINE_BINDING_ENGINE_H_
