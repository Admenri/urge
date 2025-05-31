// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_IO_SERVICE_H_
#define COMPONENTS_FILESYSTEM_IO_SERVICE_H_

#include "SDL3/SDL_iostream.h"

#include "base/bind/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

namespace filesystem {

struct IOState {
  int32_t error_count;
  base::String error_message;

  IOState() : error_count(0) {}
};

class IOService {
 public:
  IOService(const base::String& argv0);
  ~IOService();

  IOService(const IOService&) = delete;
  IOService& operator=(const IOService&) = delete;

  bool SetWritePath(const base::String& path);

  int32_t AddLoadPath(const base::String& new_path,
                      const base::String& mount_point,
                      bool append = true);
  int32_t RemoveLoadPath(const base::String& old_path);
  bool Exists(const base::String& filename);
  base::Vector<base::String> EnumDir(const base::String& dir);

  base::String GetLastError();

  using OpenCallback =
      base::RepeatingCallback<bool(SDL_IOStream*, const base::String&)>;
  void OpenRead(const base::String& file_path,
                OpenCallback callback,
                IOState* io_state);
  SDL_IOStream* OpenReadRaw(const base::String& filename, IOState* io_state);
  SDL_IOStream* OpenWrite(const base::String& filename, IOState* io_state);
};

}  // namespace filesystem

#endif  //! COMPONENTS_FILESYSTEM_IO_SERVICE_H_
