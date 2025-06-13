// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/texture_impl.h"

#include "content/context/execution_context.h"
#include "content/gpu/sampler_impl.h"

namespace content {

TextureViewImpl::TextureViewImpl(
    ExecutionContext* context,
    Diligent::RefCntAutoPtr<Diligent::ITextureView> object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

TextureViewImpl::~TextureViewImpl() {
  ExceptionState exception_state;
  Disposable::Dispose(exception_state);
}

void TextureViewImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool TextureViewImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

uint64_t TextureViewImpl::GetDeviceObject(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  Diligent::IDeviceObject* device_object = object_;
  return reinterpret_cast<uint64_t>(device_object);
}

scoped_refptr<GPUTexture> TextureViewImpl::GetTexture(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  return base::MakeRefCounted<TextureImpl>(context(), object_->GetTexture());
}

scoped_refptr<GPUSampler> TextureViewImpl::Get_Sampler(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  return base::MakeRefCounted<SamplerImpl>(context(), object_->GetSampler());
}

void TextureViewImpl::Put_Sampler(const scoped_refptr<GPUSampler>& value,
                                  ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN();

  auto* raw_sampler = static_cast<SamplerImpl*>(value.get());
  auto* object_sampler = raw_sampler ? raw_sampler->AsRawPtr() : nullptr;

  object_->SetSampler(object_sampler);
}

void TextureViewImpl::OnObjectDisposed() {
  object_.Release();
}

TextureImpl::TextureImpl(ExecutionContext* context,
                         Diligent::RefCntAutoPtr<Diligent::ITexture> object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

TextureImpl::~TextureImpl() {
  ExceptionState exception_state;
  Disposable::Dispose(exception_state);
}

void TextureImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool TextureImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

uint64_t TextureImpl::GetDeviceObject(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  Diligent::IDeviceObject* device_object = object_;
  return reinterpret_cast<uint64_t>(device_object);
}

scoped_refptr<GPUTextureView> TextureImpl::CreateView(
    std::optional<TextureViewDesc> desc,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::TextureViewDesc createinfo_desc;
  if (desc) {
    createinfo_desc.ViewType =
        static_cast<Diligent::TEXTURE_VIEW_TYPE>(desc->view_type);
    createinfo_desc.TextureDim =
        static_cast<Diligent::RESOURCE_DIMENSION>(desc->texture_dim);
    createinfo_desc.MostDetailedMip = desc->most_detailed_mip;
    createinfo_desc.NumMipLevels = desc->num_mip_levels;
    createinfo_desc.FirstArraySlice = desc->first_array_depth_slice;
    createinfo_desc.NumArraySlices = desc->num_array_depth_slices;
    createinfo_desc.AccessFlags =
        static_cast<Diligent::UAV_ACCESS_FLAG>(desc->access_flags);
    createinfo_desc.Flags =
        static_cast<Diligent::TEXTURE_VIEW_FLAGS>(desc->flags);
  }

  Diligent::RefCntAutoPtr<Diligent::ITextureView> result;
  object_->CreateView(createinfo_desc, &result);
  return base::MakeRefCounted<TextureViewImpl>(context(), result);
}

scoped_refptr<GPUTextureView> TextureImpl::GetDefaultView(
    GPU::TextureViewType view_type,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto* result = object_->GetDefaultView(
      static_cast<Diligent::TEXTURE_VIEW_TYPE>(view_type));

  return base::MakeRefCounted<TextureViewImpl>(context(), result);
}

GPU::ResourceState TextureImpl::Get_State(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(GPU::ResourceState());

  return static_cast<GPU::ResourceState>(object_->GetState());
}

void TextureImpl::Put_State(const GPU::ResourceState& value,
                            ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN();

  object_->SetState(static_cast<Diligent::RESOURCE_STATE>(value));
}

void TextureImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content
