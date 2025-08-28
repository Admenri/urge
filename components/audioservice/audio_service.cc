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

#ifndef MA_NO_DEVICE_IO
#error URGE audio service required SDL backend
#endif  // !MA_NO_DEVICE_IO

namespace audioservice {

struct ServiceKernelData {
  SDL_AudioDeviceID primary_device;
  SDL_AudioStream* engine_stream;
  ma_resource_manager_config resource_config;
  struct VirtualFileSystem {
    ma_vfs_callbacks callbacks;
    filesystem::IOService* io_service;
  } vfs;
  ma_resource_manager resource_manager;
  ma_engine_config engine_config;
  ma_engine engine;
  ma_log logger;
};

static ma_result VFSOpen(ma_vfs* pVFS,
                         const char* pFilePath,
                         ma_uint32 openMode,
                         ma_vfs_file* pFile) {
  auto* vfs = static_cast<ServiceKernelData::VirtualFileSystem*>(pVFS);

  if (openMode == (MA_OPEN_MODE_READ | MA_OPEN_MODE_WRITE))
    return MA_INVALID_ARGS;

  filesystem::IOState io_state;
  SDL_IOStream* io_stream = nullptr;
  if (openMode == MA_OPEN_MODE_READ) {
    auto open_closure = base::BindRepeating(
        [](SDL_IOStream** out, SDL_IOStream* stream,
           const std::string&) -> bool {
          *out = stream;
          return true;
        },
        &io_stream);

    vfs->io_service->OpenRead(pFilePath, open_closure, &io_state);
  }
  if (openMode == MA_OPEN_MODE_WRITE)
    io_stream = vfs->io_service->OpenWrite(pFilePath, &io_state);

  if (!io_stream || io_state.error_count)
    return ma_result::MA_DOES_NOT_EXIST;

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

static ma_result VFSClose(ma_vfs*, ma_vfs_file file) {
  auto* io_stream = static_cast<SDL_IOStream*>(file);
  return SDL_CloseIO(io_stream) ? MA_SUCCESS : MA_ERROR;
}

static ma_result VFSRead(ma_vfs*,
                         ma_vfs_file file,
                         void* pDst,
                         size_t sizeInBytes,
                         size_t* pBytesRead) {
  auto* io_stream = static_cast<SDL_IOStream*>(file);
  *pBytesRead = SDL_ReadIO(io_stream, pDst, sizeInBytes);
  return *pBytesRead > 0 ? MA_SUCCESS : MA_ERROR;
}

static ma_result VFSWrite(ma_vfs*,
                          ma_vfs_file file,
                          const void* pSrc,
                          size_t sizeInBytes,
                          size_t* pBytesWritten) {
  auto* io_stream = static_cast<SDL_IOStream*>(file);
  *pBytesWritten = SDL_WriteIO(io_stream, pSrc, sizeInBytes);
  return *pBytesWritten > 0 ? MA_SUCCESS : MA_ERROR;
}

static ma_result VFSSeek(ma_vfs*,
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

static ma_result VFSTell(ma_vfs*, ma_vfs_file file, ma_int64* pCursor) {
  auto* io_stream = static_cast<SDL_IOStream*>(file);

  *pCursor = SDL_TellIO(io_stream);
  return *pCursor >= 0 ? MA_SUCCESS : MA_ERROR;
}

static ma_result VFSInfo(ma_vfs*, ma_vfs_file file, ma_file_info* pInfo) {
  auto* io_stream = static_cast<SDL_IOStream*>(file);

  pInfo->sizeInBytes = SDL_GetIOSize(io_stream);
  return pInfo->sizeInBytes >= 0 ? MA_SUCCESS : MA_ERROR;
}

static void* AllocatorMalloc(size_t sz, void*) {
  return std::malloc(sz);
}

static void* AllocatorRealloc(void* p, size_t sz, void*) {
  return std::realloc(p, sz);
}

static void AllocatorFree(void* p, void*) {
  return std::free(p);
}

static void LogOutput(void*, ma_uint32 level, const char* pMessage) {
  std::string message(pMessage);
  if (!message.empty()) {
    message[message.size() - 1] = '\0';

    switch (level) {
#ifdef URGE_DEBUG
      case MA_LOG_LEVEL_DEBUG:
      case MA_LOG_LEVEL_INFO:
        LOG(INFO) << "[AudioService] " << message;
        break;
#endif  // !URGE_DEBUG
      case MA_LOG_LEVEL_WARNING:
        LOG(WARNING) << "[AudioService] " << message;
        break;
      case MA_LOG_LEVEL_ERROR:
        LOG(ERROR) << "[AudioService] " << message;
        break;
      default:
        break;
    }
  }
}

static std::array<ma_decoding_backend_vtable*, 2>
    g_audioservice_decoding_backend_vtable = {
        ma_decoding_backend_libvorbis,
        ma_decoding_backend_libopus,
};

static void AudioStreamDataCallback(void* userdata,
                                    SDL_AudioStream* stream,
                                    int additional_amount,
                                    int total_amount) {
  ServiceKernelData* self = static_cast<ServiceKernelData*>(userdata);

  (void)total_amount;

  if (additional_amount > 0) {
    if (auto* buffer = SDL_malloc(additional_amount)) {
      ma_uint32 buffer_size_in_frames =
          static_cast<ma_uint32>(additional_amount) /
          ma_get_bytes_per_frame(ma_format_f32,
                                 ma_engine_get_channels(&self->engine));
      ma_engine_read_pcm_frames(&self->engine, buffer, buffer_size_in_frames,
                                nullptr);
      SDL_PutAudioStreamData(stream, buffer, additional_amount);
      SDL_free(buffer);
    }
  }
}

std::unique_ptr<AudioService> AudioService::Create(
    filesystem::IOService* io_service) {
#if defined(OS_EMSCRIPTEN)
  LOG(INFO) << "[AudioService] Disable audio module on emscripten.";
  return nullptr;
#else
  ServiceKernelData* kernel_data = new ServiceKernelData;

  // Device sped
  SDL_AudioSpec device_spec;
  if (!SDL_GetAudioDeviceFormat(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &device_spec,
                                nullptr))
    return nullptr;

  // Set to f32
  device_spec.format = SDL_AUDIO_F32;

  // Init SDL device
  kernel_data->primary_device =
      SDL_OpenAudioDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &device_spec);
  if (!kernel_data->primary_device)
    return nullptr;

  // Initialize the resource manager configuration
  kernel_data->resource_config = ma_resource_manager_config_init();

  // Custom decoding backend vtable
  kernel_data->resource_config.ppCustomDecodingBackendVTables =
      g_audioservice_decoding_backend_vtable.data();
  kernel_data->resource_config.customDecodingBackendCount =
      static_cast<uint32_t>(g_audioservice_decoding_backend_vtable.size());

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
  CHECK(ma_resource_manager_init(&kernel_data->resource_config,
                                 &kernel_data->resource_manager) == MA_SUCCESS);

  // Logging
  CHECK(ma_log_init(&kernel_data->resource_config.allocationCallbacks,
                    &kernel_data->logger) == MA_SUCCESS);
  CHECK(ma_log_register_callback(&kernel_data->logger,
                                 ma_log_callback{LogOutput, nullptr}) ==
        MA_SUCCESS);

  // Engine config
  kernel_data->engine_config = ma_engine_config_init();
  kernel_data->engine_config.pResourceManager = &kernel_data->resource_manager;
  kernel_data->engine_config.pLog = &kernel_data->logger;
  kernel_data->engine_config.sampleRate = device_spec.freq;
  kernel_data->engine_config.channels = device_spec.channels;
  kernel_data->engine_config.noDevice = MA_TRUE;

  // Create canonical engine
  CHECK(ma_engine_init(&kernel_data->engine_config, &kernel_data->engine) ==
        MA_SUCCESS);

  // Create audio stream
  kernel_data->engine_stream =
      SDL_CreateAudioStream(&device_spec, &device_spec);
  if (!kernel_data->engine_stream) {
    SDL_CloseAudioDevice(kernel_data->primary_device);
    ma_engine_uninit(&kernel_data->engine);
    return nullptr;
  }

  // Setup audio stream
  CHECK(SDL_SetAudioStreamGetCallback(kernel_data->engine_stream,
                                      AudioStreamDataCallback, kernel_data));
  CHECK(SDL_BindAudioStream(kernel_data->primary_device,
                            kernel_data->engine_stream));

  return std::unique_ptr<AudioService>(
      new AudioService(std::move(kernel_data)));
#endif  //! OS_EMSCRIPTEN
}

std::unique_ptr<AudioStream> AudioService::CreateStream() {
  return std::unique_ptr<AudioStream>(new AudioStream(&kernel_->engine));
}

std::unique_ptr<SoundEmit> AudioService::CreateEmitter() {
  return std::unique_ptr<SoundEmit>(new SoundEmit(&kernel_->engine));
}

float AudioService::GetVolume() {
  return ma_engine_get_volume(&kernel_->engine);
}

void AudioService::SetVolume(float volume) {
  ma_engine_set_volume(&kernel_->engine, volume);
}

void AudioService::PauseDevice() {
  SDL_PauseAudioDevice(kernel_->primary_device);
}

void AudioService::ResumeDevice() {
  SDL_ResumeAudioDevice(kernel_->primary_device);
}

SDL_AudioDeviceID AudioService::GetDeviceID() const {
  return kernel_->primary_device;
}

ma_engine* AudioService::GetRawEngine() {
  return &kernel_->engine;
}

AudioService::AudioService(ServiceKernelData* kernel) : kernel_(kernel) {}

AudioService::~AudioService() {
  SDL_DestroyAudioStream(kernel_->engine_stream);
  SDL_CloseAudioDevice(kernel_->primary_device);

  ma_resource_manager_uninit(&kernel_->resource_manager);
  ma_engine_uninit(&kernel_->engine);
  ma_log_uninit(&kernel_->logger);
  delete kernel_;
}

}  // namespace audioservice
