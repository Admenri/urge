// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/audioservice/audio_service.h"

#include <array>

#include "miniaudio/decoders/libopus/miniaudio_libopus.h"
#include "miniaudio/decoders/libvorbis/miniaudio_libvorbis.h"
#include "physfs.h"

#include "components/audioservice/audio_stream.h"
#include "components/audioservice/sound_emit.h"

namespace audioservice {

struct VirtualFileSystem {
  ma_vfs_callbacks callbacks;
  filesystem::IOService* io_service;
};

struct ServiceKernelData {
  ma_resource_manager_config resource_config;
  VirtualFileSystem vfs;
  ma_resource_manager resource_manager;
  ma_engine_config engine_config;
  ma_engine engine;
  ma_log logger;
};

static ma_result VFSOpen(ma_vfs* pVFS,
                         const char* pFilePath,
                         ma_uint32 openMode,
                         ma_vfs_file* pFile) {
  auto* vfs = static_cast<VirtualFileSystem*>(pVFS);

  if (openMode == (MA_OPEN_MODE_READ | MA_OPEN_MODE_WRITE))
    return MA_INVALID_ARGS;

  filesystem::IOState io_state;
  SDL_IOStream* io_stream = nullptr;
  if (openMode == MA_OPEN_MODE_READ) {
    auto open_closure = base::BindRepeating(
        [](SDL_IOStream** out, SDL_IOStream* stream,
           const base::String& extname) -> bool {
          *out = stream;
          return true;
        },
        &io_stream);

    vfs->io_service->OpenRead(pFilePath, open_closure, &io_state);
  }
  if (openMode == MA_OPEN_MODE_WRITE)
    io_stream = vfs->io_service->OpenWrite(pFilePath, &io_state);

  if (!io_stream || io_state.error_count)
    return ma_result::MA_INVALID_FILE;

  *pFile = io_stream;
  return MA_SUCCESS;
}

static ma_result VFSOpenW(ma_vfs* pVFS,
                          const wchar_t* pFilePath,
                          ma_uint32 openMode,
                          ma_vfs_file* pFile) {
  char utf8_path[1024] = {0};
  PHYSFS_utf8FromUtf16((uint16_t*)pFilePath, utf8_path, sizeof(utf8_path));
  return VFSOpen(pVFS, utf8_path, openMode, pFile);
}

static ma_result VFSClose(ma_vfs* pVFS, ma_vfs_file file) {
  auto* io_stream = static_cast<SDL_IOStream*>(file);
  return SDL_CloseIO(io_stream) ? MA_SUCCESS : MA_ERROR;
}

static ma_result VFSRead(ma_vfs* pVFS,
                         ma_vfs_file file,
                         void* pDst,
                         size_t sizeInBytes,
                         size_t* pBytesRead) {
  auto* io_stream = static_cast<SDL_IOStream*>(file);
  *pBytesRead = SDL_ReadIO(io_stream, pDst, sizeInBytes);
  return *pBytesRead > 0 ? MA_SUCCESS : MA_ERROR;
}

static ma_result VFSWrite(ma_vfs* pVFS,
                          ma_vfs_file file,
                          const void* pSrc,
                          size_t sizeInBytes,
                          size_t* pBytesWritten) {
  auto* io_stream = static_cast<SDL_IOStream*>(file);
  *pBytesWritten = SDL_WriteIO(io_stream, pSrc, sizeInBytes);
  return *pBytesWritten > 0 ? MA_SUCCESS : MA_ERROR;
}

static ma_result VFSSeek(ma_vfs* pVFS,
                         ma_vfs_file file,
                         ma_int64 offset,
                         ma_seek_origin origin) {
  auto* io_stream = static_cast<SDL_IOStream*>(file);

  const SDL_IOWhence whence[] = {
      SDL_IO_SEEK_SET,
      SDL_IO_SEEK_CUR,
      SDL_IO_SEEK_END,
  };

  auto position = SDL_SeekIO(io_stream, offset, whence[origin]);
  return position >= 0 ? MA_SUCCESS : MA_ERROR;
}

static ma_result VFSTell(ma_vfs* pVFS, ma_vfs_file file, ma_int64* pCursor) {
  auto* io_stream = static_cast<SDL_IOStream*>(file);

  *pCursor = SDL_TellIO(io_stream);
  return *pCursor >= 0 ? MA_SUCCESS : MA_ERROR;
}

static ma_result VFSInfo(ma_vfs* pVFS, ma_vfs_file file, ma_file_info* pInfo) {
  auto* io_stream = static_cast<SDL_IOStream*>(file);

  pInfo->sizeInBytes = SDL_GetIOSize(io_stream);
  return pInfo->sizeInBytes >= 0 ? MA_SUCCESS : MA_ERROR;
}

static void* AllocatorMalloc(size_t sz, void* pUserData) {
  return mi_malloc(sz);
}

static void* AllocatorRealloc(void* p, size_t sz, void* pUserData) {
  return mi_realloc(p, sz);
}

static void AllocatorFree(void* p, void* pUserData) {
  return mi_free(p);
}

static void LogOutput(void* pUserData, ma_uint32 level, const char* pMessage) {
  std::string message(pMessage);
  if (!message.empty()) {
    message[message.size() - 1] = '\0';

    switch (level) {
      default:
      case MA_LOG_LEVEL_DEBUG:
      case MA_LOG_LEVEL_INFO:
        LOG(INFO) << "[AudioService] " << message;
        break;
      case MA_LOG_LEVEL_WARNING:
        LOG(WARNING) << "[AudioService] " << message;
        break;
      case MA_LOG_LEVEL_ERROR:
        LOG(ERROR) << "[AudioService] " << message;
        break;
    }
  }
}

static std::array<ma_decoding_backend_vtable*, 2>
    g_audioservice_decoding_backend_vtable = {
        ma_decoding_backend_libvorbis,
        ma_decoding_backend_libopus,
};

base::OwnedPtr<AudioService> AudioService::CreateServer(
    filesystem::IOService* io_service) {
  ServiceKernelData* kernel_data = base::Allocator::New<ServiceKernelData>();

  // Initialize the resource manager configuration
  kernel_data->resource_config = ma_resource_manager_config_init();

  // Custom decoding backend vtable
  kernel_data->resource_config.ppCustomDecodingBackendVTables =
      g_audioservice_decoding_backend_vtable.data();
  kernel_data->resource_config.customDecodingBackendCount =
      g_audioservice_decoding_backend_vtable.size();

  // Custom allocators
  kernel_data->resource_config.allocationCallbacks.onMalloc = AllocatorMalloc;
  kernel_data->resource_config.allocationCallbacks.onRealloc = AllocatorRealloc;
  kernel_data->resource_config.allocationCallbacks.onFree = AllocatorFree;

  // Custom virtual file system
  kernel_data->vfs.callbacks.onOpen = VFSOpen;
  kernel_data->vfs.callbacks.onOpenW = VFSOpenW;
  kernel_data->vfs.callbacks.onClose = VFSClose;
  kernel_data->vfs.callbacks.onRead = VFSRead;
  kernel_data->vfs.callbacks.onWrite = VFSWrite;
  kernel_data->vfs.callbacks.onSeek = VFSSeek;
  kernel_data->vfs.callbacks.onTell = VFSTell;
  kernel_data->vfs.callbacks.onInfo = VFSInfo;
  kernel_data->vfs.io_service = io_service;
  kernel_data->resource_config.pVFS = &kernel_data->vfs;

  // Create resource manager
  if (ma_resource_manager_init(&kernel_data->resource_config,
                               &kernel_data->resource_manager) != MA_SUCCESS)
    return nullptr;

  // Logging
  ma_log_init(&kernel_data->resource_config.allocationCallbacks,
              &kernel_data->logger);
  ma_log_register_callback(&kernel_data->logger,
                           ma_log_callback{LogOutput, nullptr});

  // Engine config
  kernel_data->engine_config = ma_engine_config_init();
  kernel_data->engine_config.pResourceManager = &kernel_data->resource_manager;
  kernel_data->engine_config.pLog = &kernel_data->logger;

  // Create canonical engine
  if (ma_engine_init(&kernel_data->engine_config, &kernel_data->engine) !=
      MA_SUCCESS)
    return nullptr;

  return base::MakeOwnedPtr<AudioService>(std::move(kernel_data));
}

base::OwnedPtr<AudioStream> AudioService::CreateStream() {
  return base::MakeOwnedPtr<AudioStream>(&kernel_->engine);
}

base::OwnedPtr<SoundEmit> AudioService::CreateEmitter() {
  return base::MakeOwnedPtr<SoundEmit>(&kernel_->engine);
}

AudioService::AudioService(ServiceKernelData* kernel) : kernel_(kernel) {}

AudioService::~AudioService() {
  ma_resource_manager_uninit(&kernel_->resource_manager);
  ma_engine_uninit(&kernel_->engine);
  ma_log_uninit(&kernel_->logger);
  base::Allocator::Delete(kernel_);
}

}  // namespace audioservice
