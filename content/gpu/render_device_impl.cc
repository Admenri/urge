// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/gpu/render_device_impl.h"

#include "content/context/execution_context.h"
#include "content/gpu/buffer_impl.h"
#include "content/gpu/device_context_impl.h"
#include "content/gpu/fence_impl.h"
#include "content/gpu/pipeline_signature_impl.h"
#include "content/gpu/pipeline_state_impl.h"
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

DISPOSABLE_DEFINITION(RenderDeviceImpl);

scoped_refptr<GPURenderDeviceInfo> RenderDeviceImpl::GetDeviceInfo(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  auto& desc = object_->GetDeviceInfo();
  auto result = base::MakeRefCounted<GPURenderDeviceInfo>();
  result->type = static_cast<GPU::RenderDeviceType>(desc.Type);
  result->api_version = (static_cast<uint32_t>(desc.APIVersion.Major) << 16) +
                        static_cast<uint32_t>(desc.APIVersion.Minor);

  result->features = base::MakeRefCounted<GPUDeviceFeatures>();
  result->features->separable_programs =
      static_cast<GPU::DeviceFeatureState>(desc.Features.SeparablePrograms);
  result->features->shader_resource_queries =
      static_cast<GPU::DeviceFeatureState>(desc.Features.ShaderResourceQueries);
  result->features->wireframe_fill =
      static_cast<GPU::DeviceFeatureState>(desc.Features.WireframeFill);
  result->features->multithreaded_resource_creation =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.MultithreadedResourceCreation);
  result->features->compute_shaders =
      static_cast<GPU::DeviceFeatureState>(desc.Features.ComputeShaders);
  result->features->geometry_shaders =
      static_cast<GPU::DeviceFeatureState>(desc.Features.GeometryShaders);
  result->features->tessellation =
      static_cast<GPU::DeviceFeatureState>(desc.Features.Tessellation);
  result->features->mesh_shaders =
      static_cast<GPU::DeviceFeatureState>(desc.Features.MeshShaders);
  result->features->ray_tracing =
      static_cast<GPU::DeviceFeatureState>(desc.Features.RayTracing);
  result->features->bindless_resources =
      static_cast<GPU::DeviceFeatureState>(desc.Features.BindlessResources);
  result->features->occlusion_queries =
      static_cast<GPU::DeviceFeatureState>(desc.Features.OcclusionQueries);
  result->features->binary_occlusion_queries =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.BinaryOcclusionQueries);
  result->features->timestamp_queries =
      static_cast<GPU::DeviceFeatureState>(desc.Features.TimestampQueries);
  result->features->pipeline_statistics_queries =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.PipelineStatisticsQueries);
  result->features->duration_queries =
      static_cast<GPU::DeviceFeatureState>(desc.Features.DurationQueries);
  result->features->depth_bias_clamp =
      static_cast<GPU::DeviceFeatureState>(desc.Features.DepthBiasClamp);
  result->features->depth_clamp =
      static_cast<GPU::DeviceFeatureState>(desc.Features.DepthClamp);
  result->features->independent_blend =
      static_cast<GPU::DeviceFeatureState>(desc.Features.IndependentBlend);
  result->features->dual_source_blend =
      static_cast<GPU::DeviceFeatureState>(desc.Features.DualSourceBlend);
  result->features->multi_viewport =
      static_cast<GPU::DeviceFeatureState>(desc.Features.MultiViewport);
  result->features->texture_compression_bc =
      static_cast<GPU::DeviceFeatureState>(desc.Features.TextureCompressionBC);
  result->features->texture_compression_etc2 =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.TextureCompressionETC2);
  result->features->vertex_pipeline_uav_writes_and_atomics =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.VertexPipelineUAVWritesAndAtomics);
  result->features->pixel_uav_writes_and_atomics =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.PixelUAVWritesAndAtomics);
  result->features->texture_uav_extended_formats =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.TextureUAVExtendedFormats);
  result->features->shader_float16 =
      static_cast<GPU::DeviceFeatureState>(desc.Features.ShaderFloat16);
  result->features->resource_buffer_16bit_access =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.ResourceBuffer16BitAccess);
  result->features->uniform_buffer_16bit_access =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.UniformBuffer16BitAccess);
  result->features->shader_input_output_16 =
      static_cast<GPU::DeviceFeatureState>(desc.Features.ShaderInputOutput16);
  result->features->shader_int8 =
      static_cast<GPU::DeviceFeatureState>(desc.Features.ShaderInt8);
  result->features->resource_buffer_8bit_access =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.ResourceBuffer8BitAccess);
  result->features->uniform_buffer_8bit_access =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.UniformBuffer8BitAccess);
  result->features->shader_resource_static_arrays =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.ShaderResourceStaticArrays);
  result->features->shader_resource_runtime_arrays =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.ShaderResourceRuntimeArrays);
  result->features->wave_op =
      static_cast<GPU::DeviceFeatureState>(desc.Features.WaveOp);
  result->features->instance_data_step_rate =
      static_cast<GPU::DeviceFeatureState>(desc.Features.InstanceDataStepRate);
  result->features->native_fence =
      static_cast<GPU::DeviceFeatureState>(desc.Features.NativeFence);
  result->features->tile_shaders =
      static_cast<GPU::DeviceFeatureState>(desc.Features.TileShaders);
  result->features->transfer_queue_timestamp_queries =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.TransferQueueTimestampQueries);
  result->features->variable_rate_shading =
      static_cast<GPU::DeviceFeatureState>(desc.Features.VariableRateShading);
  result->features->sparse_resources =
      static_cast<GPU::DeviceFeatureState>(desc.Features.SparseResources);
  result->features->subpass_framebuffer_fetch =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.SubpassFramebufferFetch);
  result->features->texture_component_swizzle =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.TextureComponentSwizzle);
  result->features->texture_subresource_views =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.TextureSubresourceViews);
  result->features->native_multi_draw =
      static_cast<GPU::DeviceFeatureState>(desc.Features.NativeMultiDraw);
  result->features->async_shader_compilation =
      static_cast<GPU::DeviceFeatureState>(
          desc.Features.AsyncShaderCompilation);
  result->features->formatted_buffers =
      static_cast<GPU::DeviceFeatureState>(desc.Features.FormattedBuffers);

  return result;
}

scoped_refptr<GPUBuffer> RenderDeviceImpl::CreateBuffer(
    scoped_refptr<GPUBufferDesc> desc,
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
  if (!result)
    return nullptr;

  return base::MakeRefCounted<BufferImpl>(context(), result);
}

scoped_refptr<GPUBuffer> RenderDeviceImpl::CreateBuffer(
    scoped_refptr<GPUBufferDesc> desc,
    scoped_refptr<GPUBufferData> data,
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
  if (!result)
    return nullptr;

  return base::MakeRefCounted<BufferImpl>(context(), result);
}

scoped_refptr<GPUShader> RenderDeviceImpl::CreateShader(
    scoped_refptr<GPUShaderCreateInfo> create_info,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::ShaderCreateInfo create_desc;
  std::vector<Diligent::ShaderMacro> shader_macros;
  if (create_info) {
    for (auto& macro : create_info->macros) {
      Diligent::ShaderMacro result_macro;
      result_macro.Name = macro->name.c_str();
      result_macro.Definition = macro->definition.c_str();
      shader_macros.push_back(std::move(result_macro));
    }

    create_desc.Source = create_info->source.c_str();
    create_desc.EntryPoint = create_info->entry_point.c_str();
    create_desc.Macros.Elements = shader_macros.data();
    create_desc.Macros.Count = create_info->macros.size();
    create_desc.Desc.ShaderType =
        static_cast<Diligent::SHADER_TYPE>(create_info->type);
    create_desc.Desc.UseCombinedTextureSamplers =
        create_info->combined_texture_samplers;
    create_desc.Desc.CombinedSamplerSuffix =
        create_info->combined_sampler_suffix.c_str();
    create_desc.SourceLanguage =
        static_cast<Diligent::SHADER_SOURCE_LANGUAGE>(create_info->language);
    create_desc.CompileFlags =
        static_cast<Diligent::SHADER_COMPILE_FLAGS>(create_info->compile_flags);
    create_desc.LoadConstantBufferReflection =
        create_info->load_constant_buffer_reflection;
    if (!create_info->glsl_extensions.empty())
      create_desc.GLSLExtensions = create_info->glsl_extensions.c_str();
  }

  Diligent::RefCntAutoPtr<Diligent::IShader> result;
  object_->CreateShader(create_desc, &result);
  if (!result)
    return nullptr;

  return base::MakeRefCounted<ShaderImpl>(context(), result);
}

scoped_refptr<GPUTexture> RenderDeviceImpl::CreateTexture(
    scoped_refptr<GPUTextureDesc> desc,
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
  if (!result)
    return nullptr;

  return base::MakeRefCounted<TextureImpl>(context(), result);
}

scoped_refptr<GPUTexture> RenderDeviceImpl::CreateTexture(
    scoped_refptr<GPUTextureDesc> desc,
    scoped_refptr<GPUTextureData> data,
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

  std::vector<Diligent::TextureSubResData> subresources;

  Diligent::TextureData create_data;
  if (data) {
    auto* raw_context = static_cast<DeviceContextImpl*>(data->context.get());

    for (auto& it : data->resources) {
      auto* raw_src_buffer = static_cast<BufferImpl*>(it->src_buffer.get());

      Diligent::TextureSubResData res;
      res.pData = reinterpret_cast<void*>(it->data_ptr);
      res.pSrcBuffer = raw_src_buffer ? raw_src_buffer->AsRawPtr() : nullptr;
      res.SrcOffset = it->src_offset;
      res.Stride = it->stride;
      res.DepthStride = it->depth_stride;

      subresources.push_back(res);
    }

    create_data.NumSubresources = subresources.size();
    create_data.pSubResources = subresources.data();
    create_data.pContext = raw_context ? raw_context->AsRawPtr() : nullptr;
  }

  Diligent::RefCntAutoPtr<Diligent::ITexture> result;
  object_->CreateTexture(create_desc, &create_data, &result);
  if (!result)
    return nullptr;

  return base::MakeRefCounted<TextureImpl>(context(), result);
}

scoped_refptr<GPUSampler> RenderDeviceImpl::CreateSampler(
    scoped_refptr<GPUSamplerDesc> desc,
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
  if (!result)
    return nullptr;

  return base::MakeRefCounted<SamplerImpl>(context(), result);
}

scoped_refptr<GPUResourceMapping> RenderDeviceImpl::CreateResourceMapping(
    const std::vector<scoped_refptr<GPUResourceMappingEntry>>& entries,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  std::vector<Diligent::ResourceMappingEntry> object_entries;

  Diligent::ResourceMappingCreateInfo create_desc;
  for (auto& it : entries) {
    Diligent::ResourceMappingEntry entry;
    entry.Name = it->name.c_str();
    entry.pObject =
        reinterpret_cast<Diligent::IDeviceObject*>(it->device_object);
    entry.ArrayIndex = it->array_index;

    object_entries.push_back(entry);
  }
  create_desc.NumEntries = object_entries.size();
  create_desc.pEntries = object_entries.data();

  Diligent::RefCntAutoPtr<Diligent::IResourceMapping> result;
  object_->CreateResourceMapping(create_desc, &result);
  if (!result)
    return nullptr;

  return base::MakeRefCounted<ResourceMappingImpl>(context(), result);
}

scoped_refptr<GPUPipelineState> RenderDeviceImpl::CreateGraphicsPipelineState(
    const std::vector<scoped_refptr<GPUPipelineSignature>>& signatures,
    scoped_refptr<GPUGraphicsPipelineDesc> graphics_pipeline_desc,
    scoped_refptr<GPUShader> vertex_shader,
    scoped_refptr<GPUShader> pixel_shader,
    scoped_refptr<GPUShader> domain_shader,
    scoped_refptr<GPUShader> hull_shader,
    scoped_refptr<GPUShader> geometry_shader,
    uint64_t immediate_context_mask,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::GraphicsPipelineStateCreateInfo create_desc;

  std::vector<Diligent::LayoutElement> object_elements;
  if (graphics_pipeline_desc) {
    auto& desc = graphics_pipeline_desc;
    Diligent::GraphicsPipelineDesc object_desc;

    if (desc->blend_desc) {
      object_desc.BlendDesc.AlphaToCoverageEnable =
          desc->blend_desc->alpha_to_coverage_enable;
      object_desc.BlendDesc.IndependentBlendEnable =
          desc->blend_desc->independent_blend_enable;

      for (uint32_t i = 0;
           i < std::min<uint32_t>(desc->blend_desc->render_targets.size(),
                                  DILIGENT_MAX_RENDER_TARGETS);
           ++i) {
        auto& it = desc->blend_desc->render_targets[i];
        object_desc.BlendDesc.RenderTargets[i].BlendEnable = it->blend_enable;
        object_desc.BlendDesc.RenderTargets[i].LogicOperationEnable =
            it->logic_operation_enable;
        object_desc.BlendDesc.RenderTargets[i].SrcBlend =
            static_cast<Diligent::BLEND_FACTOR>(it->src_blend);
        object_desc.BlendDesc.RenderTargets[i].DestBlend =
            static_cast<Diligent::BLEND_FACTOR>(it->dest_blend);
        object_desc.BlendDesc.RenderTargets[i].BlendOp =
            static_cast<Diligent::BLEND_OPERATION>(it->blend_op);
        object_desc.BlendDesc.RenderTargets[i].SrcBlendAlpha =
            static_cast<Diligent::BLEND_FACTOR>(it->src_blend_alpha);
        object_desc.BlendDesc.RenderTargets[i].DestBlendAlpha =
            static_cast<Diligent::BLEND_FACTOR>(it->dest_blend_alpha);
        object_desc.BlendDesc.RenderTargets[i].BlendOpAlpha =
            static_cast<Diligent::BLEND_OPERATION>(it->blend_op_alpha);
        object_desc.BlendDesc.RenderTargets[i].LogicOp =
            static_cast<Diligent::LOGIC_OPERATION>(it->logic_op);
        object_desc.BlendDesc.RenderTargets[i].RenderTargetWriteMask =
            static_cast<Diligent::COLOR_MASK>(it->render_target_write_mask);
      }
    }

    if (desc->rasterizer_desc) {
      auto& it = desc->rasterizer_desc;
      object_desc.RasterizerDesc.FillMode =
          static_cast<Diligent::FILL_MODE>(it->fill_mode);
      object_desc.RasterizerDesc.CullMode =
          static_cast<Diligent::CULL_MODE>(it->cull_mode);
      object_desc.RasterizerDesc.FrontCounterClockwise =
          it->front_counter_clockwise;
      object_desc.RasterizerDesc.DepthClipEnable = it->depth_clip_enable;
      object_desc.RasterizerDesc.ScissorEnable = it->scissor_enable;
      object_desc.RasterizerDesc.AntialiasedLineEnable =
          it->antialiased_line_enable;
      object_desc.RasterizerDesc.DepthBias = it->depth_bias;
      object_desc.RasterizerDesc.DepthBiasClamp = it->depth_bias_clamp;
      object_desc.RasterizerDesc.SlopeScaledDepthBias =
          it->slope_scaled_depth_bias;
    }

    if (desc->depth_stencil_desc) {
      auto& it = desc->depth_stencil_desc;
      object_desc.DepthStencilDesc.DepthEnable = it->depth_enable;
      object_desc.DepthStencilDesc.DepthWriteEnable = it->depth_write_enable;
      object_desc.DepthStencilDesc.DepthFunc =
          static_cast<Diligent::COMPARISON_FUNCTION>(it->depth_func);
      object_desc.DepthStencilDesc.StencilEnable = it->stencil_enable;
      object_desc.DepthStencilDesc.StencilReadMask = it->stencil_read_mask;
      object_desc.DepthStencilDesc.StencilWriteMask = it->stencil_write_mask;

      if (it->front_face) {
        object_desc.DepthStencilDesc.FrontFace.StencilFailOp =
            static_cast<Diligent::STENCIL_OP>(it->front_face->stencil_fail_op);
        object_desc.DepthStencilDesc.FrontFace.StencilDepthFailOp =
            static_cast<Diligent::STENCIL_OP>(
                it->front_face->stencil_depth_fail_op);
        object_desc.DepthStencilDesc.FrontFace.StencilPassOp =
            static_cast<Diligent::STENCIL_OP>(it->front_face->stencil_pass_op);
        object_desc.DepthStencilDesc.FrontFace.StencilFunc =
            static_cast<Diligent::COMPARISON_FUNCTION>(
                it->front_face->stencil_func);
      }

      if (it->back_face) {
        object_desc.DepthStencilDesc.BackFace.StencilFailOp =
            static_cast<Diligent::STENCIL_OP>(it->back_face->stencil_fail_op);
        object_desc.DepthStencilDesc.BackFace.StencilDepthFailOp =
            static_cast<Diligent::STENCIL_OP>(
                it->back_face->stencil_depth_fail_op);
        object_desc.DepthStencilDesc.BackFace.StencilPassOp =
            static_cast<Diligent::STENCIL_OP>(it->back_face->stencil_pass_op);
        object_desc.DepthStencilDesc.BackFace.StencilFunc =
            static_cast<Diligent::COMPARISON_FUNCTION>(
                it->back_face->stencil_func);
      }
    }

    for (auto& it : desc->input_layout) {
      Diligent::LayoutElement element;
      element.HLSLSemantic = it->hlsl_semantic.c_str();
      element.InputIndex = it->input_index;
      element.BufferSlot = it->buffer_slot;
      element.NumComponents = it->num_components;
      element.ValueType = static_cast<Diligent::VALUE_TYPE>(it->value_type);
      element.IsNormalized = it->is_normalized;
      element.RelativeOffset = it->relative_offset;
      element.Stride = it->stride;
      element.Frequency =
          static_cast<Diligent::INPUT_ELEMENT_FREQUENCY>(it->frequency);
      element.InstanceDataStepRate = it->instance_data_step_rate;

      object_elements.push_back(std::move(element));
    }

    object_desc.InputLayout.NumElements = object_elements.size();
    object_desc.InputLayout.LayoutElements = object_elements.data();

    for (uint32_t i = 0; i < std::min<uint32_t>(desc->rtv_formats.size(),
                                                DILIGENT_MAX_RENDER_TARGETS);
         ++i)
      object_desc.RTVFormats[i] =
          static_cast<Diligent::TEXTURE_FORMAT>(desc->rtv_formats[i]);

    object_desc.SampleMask = desc->sample_mask;
    object_desc.PrimitiveTopology =
        static_cast<Diligent::PRIMITIVE_TOPOLOGY>(desc->primitive_topology);
    object_desc.NumViewports = desc->num_viewports;
    object_desc.NumRenderTargets = desc->num_render_targets;
    object_desc.DSVFormat =
        static_cast<Diligent::TEXTURE_FORMAT>(desc->dsv_format);
    object_desc.ReadOnlyDSV = desc->readonly_dsv;
    object_desc.SmplDesc.Count = desc->multisample_count;
    object_desc.SmplDesc.Quality = desc->multisample_quality;
    object_desc.NodeMask = desc->node_mask;

    create_desc.GraphicsPipeline = object_desc;
  }

  std::vector<Diligent::IPipelineResourceSignature*> object_signatures;
  for (auto& it : signatures) {
    auto* impl = static_cast<PipelineSignatureImpl*>(it.get());
    object_signatures.push_back(impl ? impl->AsRawPtr() : nullptr);
  }

  create_desc.ResourceSignaturesCount = object_signatures.size();
  create_desc.ppResourceSignatures = object_signatures.data();

  auto* raw_vertex_shader = static_cast<ShaderImpl*>(vertex_shader.get());
  create_desc.pVS = raw_vertex_shader ? raw_vertex_shader->AsRawPtr() : nullptr;
  auto* raw_pixel_shader = static_cast<ShaderImpl*>(pixel_shader.get());
  create_desc.pPS = raw_pixel_shader ? raw_pixel_shader->AsRawPtr() : nullptr;
  auto* raw_domian_shader = static_cast<ShaderImpl*>(domain_shader.get());
  create_desc.pDS = raw_domian_shader ? raw_domian_shader->AsRawPtr() : nullptr;
  auto* raw_hull_shader = static_cast<ShaderImpl*>(hull_shader.get());
  create_desc.pHS = raw_hull_shader ? raw_hull_shader->AsRawPtr() : nullptr;
  auto* raw_geometry_shader = static_cast<ShaderImpl*>(geometry_shader.get());
  create_desc.pGS =
      raw_geometry_shader ? raw_geometry_shader->AsRawPtr() : nullptr;

  create_desc.PSODesc.ImmediateContextMask = immediate_context_mask;

  Diligent::RefCntAutoPtr<Diligent::IPipelineState> result;
  object_->CreateGraphicsPipelineState(create_desc, &result);
  if (!result)
    return nullptr;

  return base::MakeRefCounted<PipelineStateImpl>(context(), result);
}

scoped_refptr<GPUPipelineState> RenderDeviceImpl::CreateComputePipelineState(
    const std::vector<scoped_refptr<GPUPipelineSignature>>& signatures,
    scoped_refptr<GPUShader> compute_shader,
    uint64_t immediate_context_mask,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::ComputePipelineStateCreateInfo create_desc;

  std::vector<Diligent::IPipelineResourceSignature*> object_signatures;
  for (auto& it : signatures) {
    auto* impl = static_cast<PipelineSignatureImpl*>(it.get());
    object_signatures.push_back(impl ? impl->AsRawPtr() : nullptr);
  }

  create_desc.ResourceSignaturesCount = object_signatures.size();
  create_desc.ppResourceSignatures = object_signatures.data();

  auto* raw_shader = static_cast<ShaderImpl*>(compute_shader.get());
  create_desc.pCS = raw_shader ? raw_shader->AsRawPtr() : nullptr;

  create_desc.PSODesc.ImmediateContextMask = immediate_context_mask;

  Diligent::RefCntAutoPtr<Diligent::IPipelineState> result;
  object_->CreateComputePipelineState(create_desc, &result);
  if (!result)
    return nullptr;

  return base::MakeRefCounted<PipelineStateImpl>(context(), result);
}

scoped_refptr<GPUFence> RenderDeviceImpl::CreateFence(
    GPU::FenceType type,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::FenceDesc create_desc;
  create_desc.Type = static_cast<Diligent::FENCE_TYPE>(type);

  Diligent::RefCntAutoPtr<Diligent::IFence> result;
  object_->CreateFence(create_desc, &result);
  if (!result)
    return nullptr;

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
  if (!result)
    return nullptr;

  return base::MakeRefCounted<QueryImpl>(context(), result);
}

scoped_refptr<GPUPipelineSignature> RenderDeviceImpl::CreatePipelineSignature(
    scoped_refptr<GPUPipelineSignatureDesc> desc,
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  std::vector<Diligent::PipelineResourceDesc> resources_desc;
  std::vector<Diligent::ImmutableSamplerDesc> samplers_desc;

  Diligent::PipelineResourceSignatureDesc create_desc;
  if (desc) {
    for (auto& it : desc->resources) {
      Diligent::PipelineResourceDesc element;
      element.Name = it->name.c_str();
      element.ShaderStages =
          static_cast<Diligent::SHADER_TYPE>(it->shader_stages);
      element.ArraySize = it->array_size;
      element.ResourceType =
          static_cast<Diligent::SHADER_RESOURCE_TYPE>(it->resource_type);
      element.VarType =
          static_cast<Diligent::SHADER_RESOURCE_VARIABLE_TYPE>(it->var_type);
      resources_desc.push_back(std::move(element));
    }

    for (auto& it : desc->samplers) {
      Diligent::ImmutableSamplerDesc element;
      element.ShaderStages =
          static_cast<Diligent::SHADER_TYPE>(it->shader_stages);
      element.SamplerOrTextureName = it->sampler_name.c_str();

      Diligent::SamplerDesc sampler_create_desc;
      if (it->desc) {
        sampler_create_desc.MinFilter =
            static_cast<Diligent::FILTER_TYPE>(it->desc->min_filter);
        sampler_create_desc.MagFilter =
            static_cast<Diligent::FILTER_TYPE>(it->desc->mag_filter);
        sampler_create_desc.MipFilter =
            static_cast<Diligent::FILTER_TYPE>(it->desc->mip_filter);
        sampler_create_desc.AddressU =
            static_cast<Diligent::TEXTURE_ADDRESS_MODE>(it->desc->address_u);
        sampler_create_desc.AddressV =
            static_cast<Diligent::TEXTURE_ADDRESS_MODE>(it->desc->address_v);
        sampler_create_desc.AddressW =
            static_cast<Diligent::TEXTURE_ADDRESS_MODE>(it->desc->address_w);
      }

      element.Desc = sampler_create_desc;
      samplers_desc.push_back(std::move(element));
    }

    create_desc.Resources = resources_desc.data();
    create_desc.NumResources = resources_desc.size();
    create_desc.ImmutableSamplers = samplers_desc.data();
    create_desc.NumImmutableSamplers = samplers_desc.size();

    create_desc.BindingIndex = desc->binding_index;
    create_desc.UseCombinedTextureSamplers =
        desc->use_combined_texture_samplers;
    create_desc.CombinedSamplerSuffix = desc->combined_sampler_suffix.c_str();
    create_desc.SRBAllocationGranularity = desc->srb_allocation_granularity;
  }

  Diligent::RefCntAutoPtr<Diligent::IPipelineResourceSignature> result;
  object_->CreatePipelineResourceSignature(create_desc, &result);
  if (!result)
    return nullptr;

  return base::MakeRefCounted<PipelineSignatureImpl>(context(), result);
}

scoped_refptr<GPUDeviceContext> RenderDeviceImpl::CreateDeferredContext(
    ExceptionState& exception_state) {
  DISPOSE_CHECK_RETURN(nullptr);

  Diligent::RefCntAutoPtr<Diligent::IDeviceContext> result;
  object_->CreateDeferredContext(&result);
  if (!result)
    return nullptr;

  return base::MakeRefCounted<DeviceContextImpl>(context(), result);
}

void RenderDeviceImpl::IdleGPU(ExceptionState& exception_state) {
  DISPOSE_CHECK;

  object_->IdleGPU();
}

void RenderDeviceImpl::OnObjectDisposed() {
  object_.Release();
}

}  // namespace content
