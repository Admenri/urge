// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/filesystem.h"

#include "SDL3/SDL_system.h"
#include "physfs.h"

#include "core/archive.h"
#include "core/exception.h"

#if __ANDROID__
#include <jni.h>
#endif

namespace urge {

namespace {

void ToLower(std::string& str) {
  for (size_t i = 0; i < str.size(); ++i)
    str[i] = tolower(str[i]);
}

const char* FindFileExtName(const char* filename) {
  for (size_t i = strlen(filename); i > 0; --i) {
    if (filename[i] == '/')
      break;
    if (filename[i] == '.')
      return filename + i + 1;
  }

  return nullptr;
}

SDL_IOStream* WrapperRWops(PHYSFS_File* handle) {
  auto PHYS_RWopsSize = [](void* userdata) -> Sint64 {
    auto* f = static_cast<PHYSFS_File*>(userdata);
    return PHYSFS_fileLength(f);
  };

  auto PHYS_RWopsSeek = [](void* userdata, int64_t offset,
                           SDL_IOWhence whence) -> Sint64 {
    auto* f = static_cast<PHYSFS_File*>(userdata);
    int64_t base = 0;
    switch (whence) {
      default:
      case SDL_IO_SEEK_SET:
        base = 0;
        break;
      case SDL_IO_SEEK_CUR:
        base = PHYSFS_tell(f);
        break;
      case SDL_IO_SEEK_END:
        base = PHYSFS_fileLength(f);
        break;
    }
    int result = PHYSFS_seek(f, base + offset);
    return (result != 0) ? PHYSFS_tell(f) : -1;
  };

  auto PHYS_RWopsRead = [](void* userdata, void* buffer, size_t size,
                           SDL_IOStatus* status) -> size_t {
    auto* f = static_cast<PHYSFS_File*>(userdata);
    PHYSFS_sint64 result = PHYSFS_readBytes(f, buffer, size);
    return (result != -1) ? result : 0;
  };

  auto PHYS_RWopsWrite = [](void* userdata, const void* buffer, size_t size,
                            SDL_IOStatus* status) -> size_t {
    PHYSFS_File* f = static_cast<PHYSFS_File*>(userdata);
    PHYSFS_sint64 result = PHYSFS_writeBytes(f, buffer, size);
    return (result != -1) ? result : 0;
  };

  auto PHYS_RWopsClose = [](void* userdata) -> bool {
    PHYSFS_File* f = static_cast<PHYSFS_File*>(userdata);
    int result = PHYSFS_close(f);
    return result != 0;
  };

  SDL_IOStreamInterface iface;
  SDL_INIT_INTERFACE(&iface);
  iface.size = PHYS_RWopsSize;
  iface.seek = PHYS_RWopsSeek;
  iface.read = PHYS_RWopsRead;
  iface.write = PHYS_RWopsWrite;
  iface.close = PHYS_RWopsClose;

  return SDL_OpenIO(&iface, handle);
}

struct OpenReadEnumData {
  IOService::OpenCallback callback;
  std::string full_path;
  std::string dir_path;
  std::string file_name;
  size_t last_dot = std::string::npos;

  int match_count = 0;
  std::string physfs_error;

  OpenReadEnumData() = default;
};

PHYSFS_EnumerateCallbackResult OpenReadEnumCallback(void* data,
                                                    const char* origdir,
                                                    const char* fname) {
  OpenReadEnumData* enum_data = static_cast<OpenReadEnumData*>(data);
  std::string filename(fname);

  ToLower(filename);

  if (filename != enum_data->file_name) {
    std::string filename_noext = filename.substr(0, filename.rfind('.'));
    if (filename_noext != enum_data->file_name) {
      return PHYSFS_ENUM_OK;
    }
  }

  std::string fullpath;
  if (*origdir) {
    fullpath += std::string(origdir);
    fullpath += "/";
  }
  fullpath += fname;

  PHYSFS_File* file = PHYSFS_openRead(fullpath.c_str());
  if (!file) {
    enum_data->physfs_error = PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode());
    return PHYSFS_ENUM_ERROR;
  }

  SDL_IOStream* ops = WrapperRWops(file);
  if (enum_data->callback(ops, FindFileExtName(filename.c_str()))) {
    enum_data->match_count++;
    return PHYSFS_ENUM_STOP;
  }

  return PHYSFS_ENUM_OK;
}

}  // namespace

IOService::IOService(const std::string& argv0) {
  const char* init_data = argv0.c_str();

#if defined(OS_ANDROID)
  PHYSFS_AndroidInit ainit;
  ainit.jnienv = SDL_GetAndroidJNIEnv();
  ainit.context = SDL_GetAndroidActivity();
  init_data = (const char*)&ainit;
#endif

  if (!PHYSFS_init(init_data))
    throw Exception(Exception::kIOError, "failed to init PHYSFS.");

  /* The container archiver has to be in place before anything is mounted,
     which is why it is registered here rather than by whoever mounts a
     container.  A build with no `admenri/` drop-in registers nothing, and the
     call is a no-op. */
  archive::Register();
}

IOService::~IOService() {
  PHYSFS_deinit();
}

bool IOService::SetWritePath(const std::string& path) {
  return !!PHYSFS_setWriteDir(path.c_str());
}

int32_t IOService::AddLoadPath(const std::string& new_path,
                               const std::string& mount_point,
                               bool append) {
  return PHYSFS_mount(new_path.c_str(), mount_point.c_str(), append);
}

int32_t IOService::RemoveLoadPath(const std::string& old_path) {
  return PHYSFS_unmount(old_path.c_str());
}

bool IOService::MountArchive(const std::string& path, bool prepend) {
  return archive::Mount(path, prepend);
}

bool IOService::Exists(const std::string& filename) {
  return PHYSFS_exists(filename.c_str());
}

std::vector<std::string> IOService::EnumDir(const std::string& dir) {
  std::vector<std::string> files;

  PHYSFS_enumerate(
      dir.c_str(),
      [](void* data, const char* origdir,
         const char* fname) -> PHYSFS_EnumerateCallbackResult {
        std::vector<std::string>* files =
            static_cast<std::vector<std::string>*>(data);
        files->push_back(fname);
        return PHYSFS_ENUM_OK;
      },
      &files);

  return files;
}

void IOService::OpenRead(const std::string& file_path, OpenCallback callback) {
  std::string dir, file;
  const size_t last_slash_pos = file_path.find_last_of('/');
  if (last_slash_pos != std::string::npos) {
    dir = file_path.substr(0, last_slash_pos);
    file = file_path.substr(last_slash_pos + 1);
  } else {
    file = file_path;
  }

  OpenReadEnumData data;
  data.callback = callback;
  data.full_path = file_path;
  data.dir_path = dir;
  data.file_name = file;
  ToLower(data.file_name);
  data.last_dot = file.rfind('.');

  PHYSFS_enumerate(dir.c_str(), OpenReadEnumCallback, &data);

  if (!data.physfs_error.empty())
    throw Exception(Exception::kIOError, data.physfs_error);

  if (data.match_count <= 0)
    throw Exception(Exception::kIOError, "No file match: {}", file_path);
}

SDL_IOStream* IOService::OpenReadRaw(const std::string& filename) {
  PHYSFS_File* file = PHYSFS_openRead(filename.c_str());
  if (!file)
    throw Exception(Exception::kIOError, "{}: {}",
                    PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()), filename);

  return WrapperRWops(file);
}

SDL_IOStream* IOService::OpenWrite(const std::string& filename) {
  PHYSFS_File* file = PHYSFS_openWrite(filename.c_str());
  if (!file)
    throw Exception(Exception::kIOError, "{}: {}",
                    PHYSFS_getErrorByCode(PHYSFS_getLastErrorCode()), filename);

  return WrapperRWops(file);
}

}  // namespace urge
