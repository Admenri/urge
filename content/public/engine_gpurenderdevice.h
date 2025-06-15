// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPURENDERDEVICE_H_
#define CONTENT_PUBLIC_ENGINE_GPURENDERDEVICE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpu.h"
#include "content/public/engine_gpubuffer.h"
#include "content/public/engine_gpudevicecontext.h"
#include "content/public/engine_gpufence.h"
#include "content/public/engine_gpupipelinesignature.h"
#include "content/public/engine_gpupipelinestate.h"
#include "content/public/engine_gpuquery.h"
#include "content/public/engine_gpuresourcemapping.h"
#include "content/public/engine_gpusampler.h"
#include "content/public/engine_gpushader.h"
#include "content/public/engine_gputexture.h"

namespace content {

/*--urge(name:GPURenderDevice)--*/
class URGE_RUNTIME_API GPURenderDevice
    : public base::RefCounted<GPURenderDevice> {
 public:
  virtual ~GPURenderDevice() = default;

  /*--urge(name:BufferDesc)--*/
  struct BufferDesc {
    uint64_t size = 0;
    GPU::BindFlags bind_flags = GPU::BIND_NONE;
    GPU::Usage usage = GPU::USAGE_DEFAULT;
    GPU::CPUAccessFlags cpu_access_flags = GPU::CPU_ACCESS_NONE;
    GPU::BufferMode mode = GPU::BUFFER_MODE_UNDEFINED;
    uint32_t element_byte_stride = 0;
    uint64_t immediate_context_mask = 1;
  };

  /*--urge(name:BufferData)--*/
  struct BufferData {
    uint64_t data_ptr;
    uint64_t data_size = 0;
    scoped_refptr<GPUDeviceContext> context;
  };

  /*--urge(name:ShaderCreateInfo)--*/
  struct ShaderCreateInfo {
    base::String source;
    base::String entry_point = "main";
    GPU::ShaderType type = GPU::SHADER_TYPE_UNKNOWN;
    bool combined_texture_samplers = true;
    base::String combined_sampler_suffix = "_sampler";
    GPU::ShaderSourceLanguage language = GPU::SHADER_SOURCE_LANGUAGE_DEFAULT;
  };

  /*--urge(name:TextureDesc)--*/
  struct TextureDesc {
    GPU::ResourceDimension type = GPU::RESOURCE_DIM_UNDEFINED;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth_or_array_size = 1;
    GPU::TextureFormat format = GPU::TEX_FORMAT_UNKNOWN;
    uint32_t mip_levels = 1;
    uint32_t sample_count = 1;
    GPU::BindFlags bind_flags = GPU::BIND_NONE;
    GPU::Usage usage = GPU::USAGE_DEFAULT;
    GPU::CPUAccessFlags cpu_access_flags = GPU::CPU_ACCESS_NONE;
    uint64_t immediate_context_mask = 1;
  };

  /*--urge(name:TextureSubResData)--*/
  struct TextureSubResData {
    uint64_t data_ptr;
    scoped_refptr<GPUBuffer> src_buffer;
    uint64_t src_offset = 0;
    uint64_t stride = 0;
    uint64_t depth_stride = 0;
  };

  /*--urge(name:TextureData)--*/
  struct TextureData {
    base::Vector<TextureSubResData> resources;
    scoped_refptr<GPUDeviceContext> context;
  };

  /*--urge(name:SamplerDesc)--*/
  struct SamplerDesc {
    GPU::FilterType min_filter = GPU::FILTER_TYPE_LINEAR;
    GPU::FilterType mag_filter = GPU::FILTER_TYPE_LINEAR;
    GPU::FilterType mip_filter = GPU::FILTER_TYPE_LINEAR;
    GPU::TextureAddressMode address_u = GPU::TEXTURE_ADDRESS_CLAMP;
    GPU::TextureAddressMode address_v = GPU::TEXTURE_ADDRESS_CLAMP;
    GPU::TextureAddressMode address_w = GPU::TEXTURE_ADDRESS_CLAMP;
  };

  /*--urge(name:RenderTargetBlendDesc)--*/
  struct RenderTargetBlendDesc {
    bool blend_enable = false;
    bool logic_operation_enable = false;
    GPU::BlendFactor src_blend = GPU::BLEND_FACTOR_ONE;
    GPU::BlendFactor dest_blend = GPU::BLEND_FACTOR_ZERO;
    GPU::BlendOperation blend_op = GPU::BLEND_OPERATION_ADD;
    GPU::BlendFactor src_blend_alpha = GPU::BLEND_FACTOR_ONE;
    GPU::BlendFactor dest_blend_alpha = GPU::BLEND_FACTOR_ZERO;
    GPU::BlendOperation blend_op_alpha = GPU::BLEND_OPERATION_ADD;
    GPU::LogicOperation logic_op = GPU::LOGIC_OP_NOOP;
    GPU::ColorMask render_target_write_mask = GPU::COLOR_MASK_ALL;
  };

  /*--urge(name:BlendStateDesc)--*/
  struct BlendStateDesc {
    bool alpha_to_coverage_enable = false;
    bool independent_blend_enable = false;
    base::Vector<RenderTargetBlendDesc> render_targets;
  };

  /*--urge(name:RasterizerStateDesc)--*/
  struct RasterizerStateDesc {
    GPU::FillMode fill_mode = GPU::FILL_MODE_SOLID;
    GPU::CullMode cull_mode = GPU::CULL_MODE_BACK;
    bool front_counter_clockwise = false;
    bool depth_clip_enable = true;
    bool scissor_enable = false;
    bool antialiased_line_enable = false;
    int32_t depth_bias = 0;
    float depth_bias_clamp = 0.0f;
    float slope_scaled_depth_bias = 0.0f;
  };

  /*--urge(name:StencilOpDesc)--*/
  struct StencilOpDesc {
    GPU::StencilOp stencil_fail_op = GPU::STENCIL_OP_KEEP;
    GPU::StencilOp stencil_depth_fail_op = GPU::STENCIL_OP_KEEP;
    GPU::StencilOp stencil_pass_op = GPU::STENCIL_OP_KEEP;
    GPU::ComparisonFunction stencil_func = GPU::COMPARISON_FUNC_ALWAYS;
  };

  /*--urge(name:DepthStencilStateDesc)--*/
  struct DepthStencilStateDesc {
    bool depth_enable = false;
    bool depth_write_enable = false;
    GPU::ComparisonFunction depth_func = GPU::COMPARISON_FUNC_LESS;
    bool stencil_enable = false;
    uint8_t stencil_read_mask = 0xFF;
    uint8_t stencil_write_mask = 0xFF;
    std::optional<StencilOpDesc> front_face;
    std::optional<StencilOpDesc> back_face;
  };

  /*--urge(name:InputLayoutElement)--*/
  struct InputLayoutElement {
    base::String hlsl_semantic = "ATTRIB";
    uint32_t input_index = 0;
    uint32_t buffer_slot = 0;
    uint32_t num_components = 0;
    GPU::ValueType value_type = GPU::VT_FLOAT32;
    bool is_normalized = true;
    uint32_t relative_offset = 0xFFFFFFFFU;
    uint32_t stride = 0xFFFFFFFFU;
    GPU::InputElementFrequency frequency =
        GPU::INPUT_ELEMENT_FREQUENCY_PER_VERTEX;
    uint32_t instance_data_step_rate = 1;
  };

  /*--urge(name:GraphicsPipelineDesc)--*/
  struct GraphicsPipelineDesc {
    std::optional<BlendStateDesc> blend_desc;
    uint32_t sample_mask;
    std::optional<RasterizerStateDesc> rasterizer_desc;
    std::optional<DepthStencilStateDesc> depth_stencil_desc;
    base::Vector<InputLayoutElement> input_layout;
    GPU::PrimitiveTopology primitive_topology =
        GPU::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    uint8_t num_viewports = 1;
    uint8_t num_render_targets = 0;
    base::Vector<GPU::TextureFormat> rtv_formats;
    GPU::TextureFormat dsv_format = GPU::TEX_FORMAT_UNKNOWN;
    bool readonly_dsv = false;
    uint8_t multisample_count = 1;
    uint8_t multisample_quality = 0;
    uint32_t node_mask = 0;
  };

  /*--urge(name:PipelineResourceDesc)--*/
  struct PipelineResourceDesc {
    base::String name;
    GPU::ShaderType shader_stages = GPU::SHADER_TYPE_UNKNOWN;
    uint32_t array_size = 1;
    GPU::ShaderResourceType resource_type = GPU::SHADER_RESOURCE_TYPE_UNKNOWN;
    GPU::ShaderResourceVariableType var_type =
        GPU::SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
  };

  /*--urge(name:ImmutableSamplerDesc)--*/
  struct ImmutableSamplerDesc {
    GPU::ShaderType shader_stages = GPU::SHADER_TYPE_UNKNOWN;
    base::String sampler_name;
    std::optional<SamplerDesc> desc;
  };

  /*--urge(name:PipelineSignatureDesc)--*/
  struct PipelineSignatureDesc {
    base::Vector<PipelineResourceDesc> resources;
    base::Vector<ImmutableSamplerDesc> samplers;
    uint8_t binding_index = 0;
    bool use_combined_texture_samplers = true;
    base::String combined_sampler_suffix = "_sampler";
    uint32_t srb_allocation_granularity = 1;
  };

  /*--urge(name:ResourceMappingEntry)--*/
  struct ResourceMappingEntry {
    base::String name;
    uint64_t device_object = 0;
    uint32_t array_index = 0;
  };

  /*--urge(name:dispose)--*/
  virtual void Dispose(ExceptionState& exception_state) = 0;

  /*--urge(name:disposed?)--*/
  virtual bool IsDisposed(ExceptionState& exception_state) = 0;

  /*--urge(name:create_buffer)--*/
  virtual scoped_refptr<GPUBuffer> CreateBuffer(
      const std::optional<BufferDesc>& desc,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_buffer)--*/
  virtual scoped_refptr<GPUBuffer> CreateBuffer(
      const std::optional<BufferDesc>& desc,
      const std::optional<BufferData>& data,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_shader)--*/
  virtual scoped_refptr<GPUShader> CreateShader(
      const std::optional<ShaderCreateInfo>& create_info,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_texture)--*/
  virtual scoped_refptr<GPUTexture> CreateTexture(
      const std::optional<TextureDesc>& desc,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_texture)--*/
  virtual scoped_refptr<GPUTexture> CreateTexture(
      const std::optional<TextureDesc>& desc,
      const std::optional<TextureData>& data,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_sampler)--*/
  virtual scoped_refptr<GPUSampler> CreateSampler(
      const std::optional<SamplerDesc>& desc,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_resource_mapping)--*/
  virtual scoped_refptr<GPUResourceMapping> CreateResourceMapping(
      const base::Vector<ResourceMappingEntry>& entries,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_graphics_pipeline_state)--*/
  virtual scoped_refptr<GPUPipelineState> CreateGraphicsPipelineState(
      const base::Vector<scoped_refptr<GPUPipelineSignature>>& signatures,
      const std::optional<GraphicsPipelineDesc>& graphics_pipeline_desc,
      scoped_refptr<GPUShader> vertex_shader,
      scoped_refptr<GPUShader> pixel_shader,
      scoped_refptr<GPUShader> domain_shader,
      scoped_refptr<GPUShader> hull_shader,
      scoped_refptr<GPUShader> geometry_shader,
      uint64_t immediate_context_mask,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_compute_pipeline_state)--*/
  virtual scoped_refptr<GPUPipelineState> CreateComputePipelineState(
      const base::Vector<scoped_refptr<GPUPipelineSignature>>& signatures,
      scoped_refptr<GPUShader> compute_shader,
      uint64_t immediate_context_mask,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_fence)--*/
  virtual scoped_refptr<GPUFence> CreateFence(
      GPU::FenceType type,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_query)--*/
  virtual scoped_refptr<GPUQuery> CreateQuery(
      GPU::QueryType type,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_pipeline_signature)--*/
  virtual scoped_refptr<GPUPipelineSignature> CreatePipelineSignature(
      const std::optional<PipelineSignatureDesc>& desc,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_deferred_context)--*/
  virtual scoped_refptr<GPUDeviceContext> CreateDeferredContext(
      ExceptionState& exception_state) = 0;

  /*--urge(name:idle_gpu)--*/
  virtual void IdleGPU(ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPURENDERDEVICE_H_
