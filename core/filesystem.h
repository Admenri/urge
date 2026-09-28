// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_IO_SERVICE_H_
#define COMPONENTS_FILESYSTEM_IO_SERVICE_H_

#include <functional>

#include "SDL3/SDL_iostream.h"

#include "core/object.h"

namespace urge {

class IOService : public Singleton<IOService> {
 public:
  IOService(const std::string& argv0);
  ~IOService();

  bool SetWritePath(const std::string& path);

  int32_t AddLoadPath(const std::string& new_path,
                      const std::string& mount_point,
                      bool append = true);
  int32_t RemoveLoadPath(const std::string& old_path);
  bool Exists(const std::string& filename);
  std::vector<std::string> EnumDir(const std::string& dir);

  using OpenCallback = std::function<bool(SDL_IOStream*, const std::string&)>;
  void OpenRead(const std::string& file_path, OpenCallback callback);
  SDL_IOStream* OpenReadRaw(const std::string& filename);
  SDL_IOStream* OpenWrite(const std::string& filename);
};

}  // namespace urge

#endif  //! COMPONENTS_FILESYSTEM_IO_SERVICE_H_
