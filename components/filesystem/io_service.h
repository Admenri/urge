// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_IO_SERVICE_H_
#define COMPONENTS_FILESYSTEM_IO_SERVICE_H_

#include "base/bind/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

#include "SDL3/SDL_iostream.h"

namespace filesystem {

struct IOState {
  int32_t error_count;
  std::string error_message;

  IOState() : error_count(0) {}
};

class IOService {
 public:
  IOService(const std::string& argv0);
  ~IOService();

  IOService(const IOService&) = delete;
  IOService& operator=(const IOService&) = delete;

  void AddLoadPath(const std::string& path);
  bool Exists(const std::string& filename);
  std::vector<std::string> EnumDir(const std::string& dir);

  using OpenCallback =
      base::RepeatingCallback<bool(SDL_IOStream*, const std::string&)>;
  void OpenRead(const std::string& file_path,
                OpenCallback callback,
                IOState* io_state);
  SDL_IOStream* OpenReadRaw(const std::string& filename, IOState* io_state);
};

}  // namespace filesystem

#endif  //! COMPONENTS_FILESYSTEM_IO_SERVICE_H_
