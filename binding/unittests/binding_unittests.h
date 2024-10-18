// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BINDING_UNITTESTS_BINDING_UNITTESTS_H_
#define BINDING_UNITTESTS_BINDING_UNITTESTS_H_

#include "content/binding/binding_engine.h"
#include "content/worker/engine_worker.h"

#include <map>

namespace binding {

class BindingUnittests : public content::BindingEngine {
 public:
  BindingUnittests() = default;

  BindingUnittests(const BindingUnittests&) = delete;
  BindingUnittests& operator=(const BindingUnittests&) = delete;

  void InitializeBinding(
      scoped_refptr<content::EngineWorker> binding_host) override;
  void RunBindingMain() override;
  void FinalizeBinding() override;

  void QuitRequired() override;
  void ResetRequired() override;

 private:
  scoped_refptr<content::EngineWorker> binding_;
};

}  // namespace binding

#endif  //! BINDING_UNITTESTS_BINDING_UNITTESTS_H_
