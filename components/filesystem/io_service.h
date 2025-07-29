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
  std::string error_message;

  IOState() : error_count(0) {}
};

class IOService {
 public:
  ~IOService();

  IOService(const IOService&) = delete;
  IOService& operator=(const IOService&) = delete;

  static std::unique_ptr<IOService> Create(const std::string& argv0);

  bool SetWritePath(const std::string& path);

  int32_t AddLoadPath(const std::string& new_path,
                      const std::string& mount_point,
                      bool append = true);
  int32_t RemoveLoadPath(const std::string& old_path);
  bool Exists(const std::string& filename);
  std::vector<std::string> EnumDir(const std::string& dir);

  std::string GetLastError();

  using OpenCallback =
      base::RepeatingCallback<bool(SDL_IOStream*, const std::string&)>;
  void OpenRead(const std::string& file_path,
                OpenCallback callback,
                IOState* io_state);
  SDL_IOStream* OpenReadRaw(const std::string& filename, IOState* io_state);
  SDL_IOStream* OpenWrite(const std::string& filename, IOState* io_state);

 private:
  IOService() = default;
};

}  // namespace filesystem

#endif  //! COMPONENTS_FILESYSTEM_IO_SERVICE_H_
