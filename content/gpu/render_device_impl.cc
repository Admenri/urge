// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/render_device_impl.h"

#include "content/context/execution_context.h"
#include "content/gpu/buffer_impl.h"
#include "content/gpu/device_context_impl.h"
#include "content/gpu/fence_impl.h"
#include "content/gpu/query_impl.h"
#include "content/gpu/resource_mapping_impl.h"
#include "content/gpu/sampler_impl.h"
#include "content/gpu/shader_impl.h"
#include "content/gpu/texture_impl.h"

namespace content {

RenderDeviceImpl::RenderDeviceImpl(ExecutionContext* context,
                                   Diligent::IRenderDevice* object)
    : EngineObject(context),
      Disposable(context->disposable_parent),
      object_(object) {}

RenderDeviceImpl::~RenderDeviceImpl() {
  ExceptionState exception_state;
  Disposable::Dispose(exception_state);
}

void RenderDeviceImpl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool RenderDeviceImpl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

scoped_refptr<GPUBuffer> RenderDeviceImpl::CreateBuffer(
    const std::optional<BufferDesc>& desc,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::BufferDesc create_desc;
  if (desc) {
    create_desc.Size = desc->size;
    create_desc.BindFlags = static_cast<Diligent::BIND_FLAGS>(desc->bind_flags);
    create_desc.Usage = static_cast<Diligent::USAGE>(desc->usage);
    create_desc.CPUAccessFlags =
        static_cast<Diligent::CPU_ACCESS_FLAGS>(desc->cpu_access_flags);
    create_desc.Mode = static_cast<Diligent::BUFFER_MODE>(desc->mode);
    create_desc.ElementByteStride = desc->element_byte_stride;
    create_desc.ImmediateContextMask = desc->immediate_context_mask;
  }

  Diligent::RefCntAutoPtr<Diligent::IBuffer> result;
  object_->CreateBuffer(create_desc, nullptr, &result);

  return base::MakeRefCounted<BufferImpl>(context(), result);
}

scoped_refptr<GPUBuffer> RenderDeviceImpl::CreateBuffer(
    const std::optional<BufferDesc>& desc,
    const std::optional<BufferData>& data,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::BufferDesc create_desc;
  if (desc) {
    create_desc.Size = desc->size;
    create_desc.BindFlags = static_cast<Diligent::BIND_FLAGS>(desc->bind_flags);
    create_desc.Usage = static_cast<Diligent::USAGE>(desc->usage);
    create_desc.CPUAccessFlags =
        static_cast<Diligent::CPU_ACCESS_FLAGS>(desc->cpu_access_flags);
    create_desc.Mode = static_cast<Diligent::BUFFER_MODE>(desc->mode);
    create_desc.ElementByteStride = desc->element_byte_stride;
    create_desc.ImmediateContextMask = desc->immediate_context_mask;
  }

  Diligent::BufferData create_data;
  if (data) {
    auto* raw_context = static_cast<DeviceContextImpl*>(data->context.get());

    create_data.pData = reinterpret_cast<void*>(data->data_ptr);
    create_data.DataSize = data->data_size;
    create_data.pContext = raw_context ? raw_context->AsRawPtr() : nullptr;
  }

  Diligent::RefCntAutoPtr<Diligent::IBuffer> result;
  object_->CreateBuffer(create_desc, &create_data, &result);

  return base::MakeRefCounted<BufferImpl>(context(), result);
}

scoped_refptr<GPUShader> RenderDeviceImpl::CreateShader(
    const std::optional<ShaderCreateInfo>& create_info,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::ShaderCreateInfo create_desc;
  if (create_info) {
    create_desc.Source = create_info->source.c_str();
    create_desc.EntryPoint = create_info->entry_point.c_str();
    create_desc.Desc.ShaderType =
        static_cast<Diligent::SHADER_TYPE>(create_info->type);
    create_desc.Desc.UseCombinedTextureSamplers =
        create_info->combined_texture_samplers;
    create_desc.Desc.CombinedSamplerSuffix =
        create_info->combined_sampler_suffix.c_str();
    create_desc.SourceLanguage =
        static_cast<Diligent::SHADER_SOURCE_LANGUAGE>(create_info->language);
  }

  Diligent::RefCntAutoPtr<Diligent::IShader> result;
  object_->CreateShader(create_desc, &result);

  return base::MakeRefCounted<ShaderImpl>(context(), result);
}

scoped_refptr<GPUTexture> RenderDeviceImpl::CreateTexture(
    const std::optional<TextureDesc>& desc,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::TextureDesc create_desc;
  if (desc) {
    create_desc.Type = static_cast<Diligent::RESOURCE_DIMENSION>(desc->type);
    create_desc.Width = desc->width;
    create_desc.Height = desc->height;
    create_desc.Depth = desc->depth_or_array_size;
    create_desc.Format = static_cast<Diligent::TEXTURE_FORMAT>(desc->format);
    create_desc.MipLevels = desc->mip_levels;
    create_desc.SampleCount = desc->sample_count;
    create_desc.BindFlags = static_cast<Diligent::BIND_FLAGS>(desc->bind_flags);
    create_desc.Usage = static_cast<Diligent::USAGE>(desc->usage);
    create_desc.CPUAccessFlags =
        static_cast<Diligent::CPU_ACCESS_FLAGS>(desc->cpu_access_flags);
    create_desc.ImmediateContextMask = desc->immediate_context_mask;
  }

  Diligent::RefCntAutoPtr<Diligent::ITexture> result;
  object_->CreateTexture(create_desc, nullptr, &result);

  return base::MakeRefCounted<TextureImpl>(context(), result);
}

scoped_refptr<GPUTexture> RenderDeviceImpl::CreateTexture(
    const std::optional<TextureDesc>& desc,
    const std::optional<TextureData>& data,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::TextureDesc create_desc;
  if (desc) {
    create_desc.Type = static_cast<Diligent::RESOURCE_DIMENSION>(desc->type);
    create_desc.Width = desc->width;
    create_desc.Height = desc->height;
    create_desc.Depth = desc->depth_or_array_size;
    create_desc.Format = static_cast<Diligent::TEXTURE_FORMAT>(desc->format);
    create_desc.MipLevels = desc->mip_levels;
    create_desc.SampleCount = desc->sample_count;
    create_desc.BindFlags = static_cast<Diligent::BIND_FLAGS>(desc->bind_flags);
    create_desc.Usage = static_cast<Diligent::USAGE>(desc->usage);
    create_desc.CPUAccessFlags =
        static_cast<Diligent::CPU_ACCESS_FLAGS>(desc->cpu_access_flags);
    create_desc.ImmediateContextMask = desc->immediate_context_mask;
  }

  base::Vector<Diligent::TextureSubResData> subresources;

  Diligent::TextureData create_data;
  if (data) {
    auto* raw_context = static_cast<DeviceContextImpl*>(data->context.get());

    for (auto& it : data->resources) {
      auto* raw_src_buffer = static_cast<BufferImpl*>(it.src_buffer.get());

      Diligent::TextureSubResData res;
      res.pData = reinterpret_cast<void*>(it.data_ptr);
      res.pSrcBuffer = raw_src_buffer ? raw_src_buffer->AsRawPtr() : nullptr;
      res.SrcOffset = it.src_offset;
      res.Stride = it.stride;
      res.DepthStride = it.depth_stride;

      subresources.push_back(res);
    }

    create_data.NumSubresources = subresources.size();
    create_data.pSubResources = subresources.data();
    create_data.pContext = raw_context ? raw_context->AsRawPtr() : nullptr;
  }

  Diligent::RefCntAutoPtr<Diligent::ITexture> result;
  object_->CreateTexture(create_desc, &create_data, &result);

  return base::MakeRefCounted<TextureImpl>(context(), result);
}

scoped_refptr<GPUSampler> RenderDeviceImpl::CreateSampler(
    const std::optional<SamplerDesc>& desc,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::SamplerDesc create_desc;
  if (desc) {
    create_desc.MinFilter =
        static_cast<Diligent::FILTER_TYPE>(desc->min_filter);
    create_desc.MagFilter =
        static_cast<Diligent::FILTER_TYPE>(desc->mag_filter);
    create_desc.MipFilter =
        static_cast<Diligent::FILTER_TYPE>(desc->mip_filter);
    create_desc.AddressU =
        static_cast<Diligent::TEXTURE_ADDRESS_MODE>(desc->address_u);
    create_desc.AddressV =
        static_cast<Diligent::TEXTURE_ADDRESS_MODE>(desc->address_v);
    create_desc.AddressW =
        static_cast<Diligent::TEXTURE_ADDRESS_MODE>(desc->address_w);
  }

  Diligent::RefCntAutoPtr<Diligent::ISampler> result;
  object_->CreateSampler(create_desc, &result);

  return base::MakeRefCounted<SamplerImpl>(context(), result);
}

scoped_refptr<GPUResourceMapping> RenderDeviceImpl::CreateResourceMapping(
    const base::Vector<ResourceMappingEntry>& entries,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  base::Vector<Diligent::ResourceMappingEntry> object_entries;

  Diligent::ResourceMappingCreateInfo create_desc;
  for (auto& it : entries) {
    Diligent::ResourceMappingEntry entry;
    entry.Name = it.name.c_str();
    entry.pObject =
        reinterpret_cast<Diligent::IDeviceObject*>(it.device_object);
    entry.ArrayIndex = it.array_index;

    object_entries.push_back(entry);
  }
  create_desc.NumEntries = object_entries.size();
  create_desc.pEntries = object_entries.data();

  Diligent::RefCntAutoPtr<Diligent::IResourceMapping> result;
  object_->CreateResourceMapping(create_desc, &result);

  return base::MakeRefCounted<ResourceMappingImpl>(context(), result);
}

scoped_refptr<GPUPipelineState> RenderDeviceImpl::CreateGraphicsPipelineState(
    const base::Vector<scoped_refptr<GPUPipelineSignature>>& signatures,
    const std::optional<GraphicsPipelineDesc>& graphics_pipeline_desc,
    scoped_refptr<GPUShader> vertex_shader,
    scoped_refptr<GPUShader> pixel_shader,
    scoped_refptr<GPUShader> domain_shader,
    scoped_refptr<GPUShader> hull_shader,
    scoped_refptr<GPUShader> geometry_shader,
    uint64_t immediate_context_mask,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  return scoped_refptr<GPUPipelineState>();
}

scoped_refptr<GPUPipelineState> RenderDeviceImpl::CreateComputePipelineState(
    const base::Vector<scoped_refptr<GPUPipelineSignature>>& signatures,
    scoped_refptr<GPUShader> compute_shader,
    uint64_t immediate_context_mask,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  return scoped_refptr<GPUPipelineState>();
}

scoped_refptr<GPUFence> RenderDeviceImpl::CreateFence(
    GPU::FenceType type,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::FenceDesc create_desc;
  create_desc.Type = static_cast<Diligent::FENCE_TYPE>(type);

  Diligent::RefCntAutoPtr<Diligent::IFence> result;
  object_->CreateFence(create_desc, &result);

  return base::MakeRefCounted<FenceImpl>(context(), result);
}

scoped_refptr<GPUQuery> RenderDeviceImpl::CreateQuery(
    GPU::QueryType type,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::QueryDesc create_desc;
  create_desc.Type = static_cast<Diligent::QUERY_TYPE>(type);

  Diligent::RefCntAutoPtr<Diligent::IQuery> result;
  object_->CreateQuery(create_desc, &result);

  return base::MakeRefCounted<QueryImpl>(context(), result);
}

scoped_refptr<GPUPipelineSignature> RenderDeviceImpl::CreatePipelineSignature(
    const std::optional<PipelineSignatureDesc>& desc,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  return scoped_refptr<GPUPipelineSignature>();
}

void RenderDeviceImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content
