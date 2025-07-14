// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/texture_impl.h"

#include "content/context/execution_context.h"
#include "content/gpu/sampler_impl.h"

namespace content {

TextureViewImpl::TextureViewImpl(ExecutionContext* context,
                                 Diligent::ITextureView* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

DISPOSABLE_DEFINITION(TextureViewImpl);

uint64_t TextureViewImpl::GetDeviceObject(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  Diligent::IDeviceObject* device_object = object_;
  return reinterpret_cast<uint64_t>(device_object);
}

scoped_refptr<GPUTextureViewDesc> TextureViewImpl::GetDesc(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto& object_desc = object_->GetDesc();
  auto result = base::MakeRefCounted<GPUTextureViewDesc>();

  result->view_type = static_cast<GPU::TextureViewType>(object_desc.ViewType);
  result->texture_dim =
      static_cast<GPU::ResourceDimension>(object_desc.TextureDim);
  result->format = static_cast<GPU::TextureFormat>(object_desc.Format);
  result->most_detailed_mip = object_desc.MostDetailedMip;
  result->num_mip_levels = object_desc.NumMipLevels;
  result->first_array_depth_slice = object_desc.FirstArrayOrDepthSlice();
  result->num_array_depth_slices = object_desc.NumArrayOrDepthSlices();
  result->access_flags =
      static_cast<GPU::UAVAccessFlag>(object_desc.AccessFlags);
  result->flags = static_cast<GPU::TextureViewFlags>(object_desc.Flags);

  return result;
}

scoped_refptr<GPUTexture> TextureViewImpl::GetTexture(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  if (!object_->GetTexture())
    return nullptr;

  return base::MakeRefCounted<TextureImpl>(context(), object_->GetTexture());
}

scoped_refptr<GPUSampler> TextureViewImpl::Get_Sampler(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  if (!object_->GetSampler())
    return nullptr;

  return base::MakeRefCounted<SamplerImpl>(context(), object_->GetSampler());
}

void TextureViewImpl::Put_Sampler(const scoped_refptr<GPUSampler>& value,
                                  ExceptionState& exception_state) {
  DISPOSE_CHECK;

  auto* raw_sampler = static_cast<SamplerImpl*>(value.get());
  auto* object_sampler = raw_sampler ? raw_sampler->AsRawPtr() : nullptr;

  object_->SetSampler(object_sampler);
}

void TextureViewImpl::OnObjectDisposed() {
  object_.Release();
}

TextureImpl::TextureImpl(ExecutionContext* context, Diligent::ITexture* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

DISPOSABLE_DEFINITION(TextureImpl);

uint64_t TextureImpl::GetDeviceObject(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(0);

  Diligent::IDeviceObject* device_object = object_;
  return reinterpret_cast<uint64_t>(device_object);
}

scoped_refptr<GPUTextureDesc> TextureImpl::GetDesc(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto& object_desc = object_->GetDesc();
  auto result = base::MakeRefCounted<GPUTextureDesc>();

  result->type = static_cast<GPU::ResourceDimension>(object_desc.Type);
  result->width = object_desc.Width;
  result->height = object_desc.Height;
  result->depth_or_array_size = object_desc.ArraySizeOrDepth();
  result->format = static_cast<GPU::TextureFormat>(object_desc.Format);
  result->mip_levels = object_desc.MipLevels;
  result->sample_count = object_desc.SampleCount;
  result->bind_flags = static_cast<GPU::BindFlags>(object_desc.BindFlags);
  result->usage = static_cast<GPU::Usage>(object_desc.Usage);
  result->cpu_access_flags =
      static_cast<GPU::CPUAccessFlags>(object_desc.CPUAccessFlags);
  result->immediate_context_mask = object_desc.ImmediateContextMask;

  return result;
}

scoped_refptr<GPUTextureView> TextureImpl::CreateView(
    scoped_refptr<GPUTextureViewDesc> desc,
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
  if (!result)
    return nullptr;

  return base::MakeRefCounted<TextureViewImpl>(context(), result);
}

scoped_refptr<GPUTextureView> TextureImpl::GetDefaultView(
    GPU::TextureViewType view_type,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto* result = object_->GetDefaultView(
      static_cast<Diligent::TEXTURE_VIEW_TYPE>(view_type));
  if (!result)
    return nullptr;

  return base::MakeRefCounted<TextureViewImpl>(context(), result);
}

GPU::ResourceState TextureImpl::Get_State(ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(GPU::ResourceState());

  return static_cast<GPU::ResourceState>(object_->GetState());
}

void TextureImpl::Put_State(const GPU::ResourceState& value,
                            ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->SetState(static_cast<Diligent::RESOURCE_STATE>(value));
}

void TextureImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content
