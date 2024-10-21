// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef BINDING_MRI_MRI_MAIN_H_
#define BINDING_MRI_MRI_MAIN_H_

#include "content/binding/binding_engine.h"

#include <map>

namespace binding {

class BindingEngineMri : public content::BindingEngine {
 public:
  using BacktraceData = std::map<std::string, std::string>;

  BindingEngineMri();
  ~BindingEngineMri() override;

  BindingEngineMri(const BindingEngineMri&) = delete;
  BindingEngineMri& operator=(const BindingEngineMri&) = delete;

  void InitializeBinding(
      scoped_refptr<content::EngineWorker> binding_host) override;
  void RunBindingMain() override;
  void QuitRequired() override;
  void ResetRequired() override;
  void FinalizeBinding() override;

 private:
  void LoadPackedScripts();

  scoped_refptr<content::EngineWorker> binding_;
  BacktraceData backtrace_;
};

}  // namespace binding

#endif  // !BINDING_MRI_MRI_MAIN_H_
