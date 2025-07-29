// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef BINDING_MRI_MRI_MAIN_H_
#define BINDING_MRI_MRI_MAIN_H_

#include <map>

#include "content/profile/content_profile.h"
#include "content/worker/engine_binding.h"

namespace binding {

class BindingEngineMri : public content::EngineBindingBase {
 public:
  using BacktraceData = std::map<std::string, std::string>;

  BindingEngineMri();
  ~BindingEngineMri() override;

  BindingEngineMri(const BindingEngineMri&) = delete;
  BindingEngineMri& operator=(const BindingEngineMri&) = delete;

  void PreEarlyInitialization(content::ContentProfile* profile,
                              filesystem::IOService* io_service) override;
  void OnMainMessageLoopRun(content::ExecutionContext* execution,
                            ScopedModuleContext* module_context) override;
  void PostMainLoopRunning() override;
  void ExitSignalRequired() override;
  void ResetSignalRequired() override;

 private:
  void LoadPackedScripts(content::ContentProfile* profile,
                         content::ExceptionState& exception_state);

  content::ContentProfile* profile_;
};

}  // namespace binding

#endif  // !BINDING_MRI_MRI_MAIN_H_
