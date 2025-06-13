// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_GPU_TEXTURE_IMPL_H_
#define CONTENT_GPU_TEXTURE_IMPL_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/Texture.h"
#include "Graphics/GraphicsEngine/interface/TextureView.h"

#include "content/context/disposable.h"
#include "content/context/engine_object.h"
#include "content/public/engine_gputexture.h"

namespace content {

class TextureViewImpl : public GPUTextureView,
                        public EngineObject,
                        public Disposable {
 public:
  TextureViewImpl(ExecutionContext* context, Diligent::ITextureView* object);
  ~TextureViewImpl() override;

  TextureViewImpl(const TextureViewImpl&) = delete;
  TextureViewImpl& operator=(const TextureViewImpl&) = delete;

  Diligent::ITextureView* AsRawPtr() const { return object_; }

 protected:
  // GPUTextureView interface
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  uint64_t GetDeviceObject(ExceptionState& exception_state) override;
  scoped_refptr<GPUTexture> GetTexture(
      ExceptionState& exception_state) override;
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(Sampler, scoped_refptr<GPUSampler>);

 private:
  void OnObjectDisposed() override;
  base::String DisposedObjectName() override { return "GPU.TextureView"; }

  Diligent::RefCntAutoPtr<Diligent::ITextureView> object_;
};

class TextureImpl : public GPUTexture, public EngineObject, public Disposable {
 public:
  TextureImpl(ExecutionContext* context, Diligent::ITexture* object);
  ~TextureImpl() override;

  TextureImpl(const TextureImpl&) = delete;
  TextureImpl& operator=(const TextureImpl&) = delete;

  Diligent::ITexture* AsRawPtr() const { return object_; }

 protected:
  // GPUTexture interface
  void Dispose(ExceptionState& exception_state) override;
  bool IsDisposed(ExceptionState& exception_state) override;
  uint64_t GetDeviceObject(ExceptionState& exception_state) override;
  scoped_refptr<GPUTextureView> CreateView(
      std::optional<TextureViewDesc> desc,
      ExceptionState& exception_state) override;
  scoped_refptr<GPUTextureView> GetDefaultView(
      GPU::TextureViewType view_type,
      ExceptionState& exception_state) override;
  URGE_DECLARE_OVERRIDE_ATTRIBUTE(State, GPU::ResourceState);

 private:
  void OnObjectDisposed() override;
  base::String DisposedObjectName() override { return "GPU.Texture"; }

  Diligent::RefCntAutoPtr<Diligent::ITexture> object_;
};

}  // namespace content

#endif  //! CONTENT_GPU_TEXTURE_IMPL_H_
