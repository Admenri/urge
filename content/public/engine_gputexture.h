// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPUTEXTURE_H_
#define CONTENT_PUBLIC_ENGINE_GPUTEXTURE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpu.h"
#include "content/public/engine_gpusampler.h"

namespace content {

class GPUTexture;

/*--urge(name:GPUTextureView)--*/
class URGE_RUNTIME_API GPUTextureView
    : public base::RefCounted<GPUTextureView> {
 public:
  virtual ~GPUTextureView() = default;

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:device_object)--*/
  virtual uint64_t GetDeviceObject(ExceptionState& exception_state) = 0;

  /*--urge(name:texture)--*/
  virtual scoped_refptr<GPUTexture> GetTexture(
      ExceptionState& exception_state) = 0;

  /*--urge(name:sampler)--*/
  URGE_EXPORT_ATTRIBUTE(Sampler, scoped_refptr<GPUSampler>);
};

/*--urge(name:GPUTexture)--*/
class URGE_RUNTIME_API GPUTexture : public base::RefCounted<GPUTexture> {
 public:
  virtual ~GPUTexture() = default;

  /*--urge(name:TextureViewDesc)--*/
  struct TextureViewDesc {
    GPU::TextureViewType view_type = GPU::TEXTURE_VIEW_UNDEFINED;
    GPU::ResourceDimension texture_dim = GPU::RESOURCE_DIM_UNDEFINED;
    GPU::TextureFormat format = GPU::TEX_FORMAT_UNKNOWN;
    uint32_t most_detailed_mip = 0;
    uint32_t num_mip_levels = 0;
    uint32_t first_array_depth_slice = 0;
    uint32_t num_array_depth_slices = 0;
    GPU::UAVAccessFlag access_flags = GPU::UAV_ACCESS_UNSPECIFIED;
    GPU::TextureViewFlags flags = GPU::TEXTURE_VIEW_FLAG_NONE;
  };

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:device_object)--*/
  virtual uint64_t GetDeviceObject(ExceptionState& exception_state) = 0;

  /*--urge(name:create_view)--*/
  virtual scoped_refptr<GPUTextureView> CreateView(
      base::Optional<TextureViewDesc> desc,
      ExceptionState& exception_state) = 0;

  /*--urge(name:default_view)--*/
  virtual scoped_refptr<GPUTextureView> GetDefaultView(
      GPU::TextureViewType view_type,
      ExceptionState& exception_state) = 0;

  /*--urge(name:state)--*/
  URGE_EXPORT_ATTRIBUTE(State, GPU::ResourceState);
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPUTEXTURE_H_
