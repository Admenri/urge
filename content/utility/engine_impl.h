// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_UTILITY_ENGINE_IMPL_H_
#define CONTENT_UTILITY_ENGINE_IMPL_H_

#include "components/filesystem/io_service.h"
#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/profile/content_profile.h"
#include "content/public/engine_urge.h"
#include "ui/widget/widget.h"

namespace content {

class EngineImpl : public URGE,
                   public EngineObject,
                   public DisposableCollection {
 public:
  EngineImpl(ExecutionContext* execution_context);
  ~EngineImpl() override;

  EngineImpl(const EngineImpl&) = delete;
  EngineImpl& operator=(const EngineImpl&) = delete;

 public:
  // URGE methods:
  std::string GetBuildDate(ExceptionState& exception_state) override;
  std::string GetRevision(ExceptionState& exception_state) override;
  std::string GetPlatform(ExceptionState& exception_state) override;
  int32_t GetAPIVersion(ExceptionState& exception_state) override;

  void Reset(ExceptionState& exception_state) override;

  void OpenURL(const std::string& path,
               ExceptionState& exception_state) override;

  std::string Gets(ExceptionState& exception_state) override;
  void Puts(const std::string& message,
            ExceptionState& exception_state) override;

  void Alert(const std::string& message,
             ExceptionState& exception_state) override;
  bool Confirm(const std::string& message,
               ExceptionState& exception_state) override;

  bool AddLoadPath(const std::string& new_path,
                   const std::string& mount_point,
                   bool append_to_path,
                   ExceptionState& exception_state) override;
  bool RemoveLoadPath(const std::string& old_path,
                      ExceptionState& exception_state) override;
  bool IsFileExisted(const std::string& filepath,
                     ExceptionState& exception_state) override;
  std::vector<std::string> EnumDirectory(
      const std::string& dirpath,
      ExceptionState& exception_state) override;

  // DisposableCollection methods:
  void AddDisposable(Disposable* disp) override;

 private:
  base::LinkedList<Disposable> disposable_elements_;
};

}  // namespace content

#endif  //! CONTENT_UTILITY_ENGINE_IMPL_H_
