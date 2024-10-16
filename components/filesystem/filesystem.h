// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_FILESYSTEM_FILESYSTEM_H_
#define COMPONENTS_FILESYSTEM_FILESYSTEM_H_

#include "base/bind/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/weak_ptr.h"

#include "SDL3/SDL_iostream.h"

namespace filesystem {

class Filesystem {
 public:
  using FileStream = SDL_IOStream;

  Filesystem(const std::string& argv0);
  ~Filesystem();

  Filesystem(const Filesystem&) = delete;
  Filesystem& operator=(const Filesystem&) = delete;

  void AddLoadPath(const std::string& path);
  bool Exists(const std::string& filename);
  std::vector<std::string> EnumDir(const std::string& dir);

  using OpenCallback =
      base::RepeatingCallback<bool(SDL_IOStream*, const std::string&)>;
  void OpenRead(const std::string& file_path, OpenCallback callback);
  SDL_IOStream* OpenReadRaw(const std::string& filename);
  SDL_IOStream* OpenWriteRaw(const std::string& filename);
};

}  // namespace filesystem

#endif  //! COMPONENTS_FILESYSTEM_FILESYSTEM_H_
