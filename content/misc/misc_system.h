// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_MISC_MISC_SYSTEM_H_
#define CONTENT_MISC_MISC_SYSTEM_H_

#include "components/filesystem/io_service.h"
#include "content/public/engine_urge.h"
#include "ui/widget/widget.h"

namespace content {

class MiscSystem : public URGE {
 public:
  MiscSystem(base::WeakPtr<ui::Widget> window,
             filesystem::IOService* io_service);
  ~MiscSystem() override;

  MiscSystem(const MiscSystem&) = delete;
  MiscSystem& operator=(const MiscSystem&) = delete;

 public:
  base::String GetPlatform(ExceptionState& exception_state) override;
  void OpenURL(const base::String& path,
               ExceptionState& exception_state) override;

  base::String Gets(ExceptionState& exception_state) override;
  void Puts(const base::String& message,
            ExceptionState& exception_state) override;

  void Alert(const base::String& message,
             ExceptionState& exception_state) override;
  bool Confirm(const base::String& message,
               ExceptionState& exception_state) override;

  bool AddLoadPath(const base::String& new_path,
                   const base::String& mount_point,
                   bool append_to_path,
                   ExceptionState& exception_state) override;
  bool RemoveLoadPath(const base::String& old_path,
                      ExceptionState& exception_state) override;
  bool IsFileExisted(const base::String& filepath,
                     ExceptionState& exception_state) override;
  base::Vector<base::String> EnumDirectory(
      const base::String& dirpath,
      ExceptionState& exception_state) override;

 private:
  base::WeakPtr<ui::Widget> window_;
  filesystem::IOService* io_service_;
};

}  // namespace content

#endif  //! CONTENT_MISC_MISC_SYSTEM_H_
