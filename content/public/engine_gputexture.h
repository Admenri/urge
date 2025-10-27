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
class URGE_OBJECT(GPUTextureView) {
 public:
  virtual ~GPUTextureView() = default;

  /*--urge(disposable)--*/
  URGE_EXPORT_DISPOSABLE;

  /*--urge(name:device_object)--*/
  virtual uint64_t GetDeviceObject(ExceptionState& exception_state) = 0;

  /*--urge(name:desc)--*/
  virtual scoped_refptr<GPUTextureViewDesc> GetDesc(
      ExceptionState& exception_state) = 0;

  /*--urge(name:texture)--*/
  virtual scoped_refptr<GPUTexture> GetTexture(
      ExceptionState& exception_state) = 0;

  /*--urge(name:sampler)--*/
  URGE_EXPORT_ATTRIBUTE(Sampler, scoped_refptr<GPUSampler>);
};

/*--urge(name:GPUTexture)--*/
class URGE_OBJECT(GPUTexture) {
 public:
  virtual ~GPUTexture() = default;

  /*--urge(disposable)--*/
  URGE_EXPORT_DISPOSABLE;

  /*--urge(name:device_object)--*/
  virtual uint64_t GetDeviceObject(ExceptionState& exception_state) = 0;

  /*--urge(name:desc)--*/
  virtual scoped_refptr<GPUTextureDesc> GetDesc(
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_view)--*/
  virtual scoped_refptr<GPUTextureView> CreateView(
      scoped_refptr<GPUTextureViewDesc> desc,
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
