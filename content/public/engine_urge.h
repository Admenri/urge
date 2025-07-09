// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_URGE_H_
#define CONTENT_PUBLIC_ENGINE_URGE_H_

#include <stdint.h>

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:URGE,is_module)--*/
class URGE_OBJECT(URGE) {
 public:
  virtual ~URGE() = default;

  /*--urge(name:platform)--*/
  virtual base::String GetPlatform(ExceptionState& exception_state) = 0;

  /*--urge(name:open_url)--*/
  virtual void OpenURL(const base::String& path,
                       ExceptionState& exception_state) = 0;

  /*--urge(name:gets)--*/
  virtual base::String Gets(ExceptionState& exception_state) = 0;

  /*--urge(name:puts)--*/
  virtual void Puts(const base::String& message,
                    ExceptionState& exception_state) = 0;

  /*--urge(name:alert)--*/
  virtual void Alert(const base::String& message,
                     ExceptionState& exception_state) = 0;

  /*--urge(name:confirm)--*/
  virtual bool Confirm(const base::String& message,
                       ExceptionState& exception_state) = 0;

  /*--urge(name:add_load_path)--*/
  virtual bool AddLoadPath(const base::String& new_path,
                           const base::String& mount_point,
                           bool append_to_path,
                           ExceptionState& exception_state) = 0;

  /*--urge(name:remove_load_path)--*/
  virtual bool RemoveLoadPath(const base::String& old_path,
                              ExceptionState& exception_state) = 0;

  /*--urge(name:file_exist?)--*/
  virtual bool IsFileExisted(const base::String& filepath,
                             ExceptionState& exception_state) = 0;

  /*--urge(name:enum_directory)--*/
  virtual base::Vector<base::String> EnumDirectory(
      const base::String& dirpath,
      ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_URGE_H_
