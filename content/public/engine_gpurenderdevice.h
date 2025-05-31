// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPURENDERDEVICE_H_
#define CONTENT_PUBLIC_ENGINE_GPURENDERDEVICE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/context/execution_context.h"
#include "content/public/engine_gpublendstate.h"
#include "content/public/engine_gpubuffer.h"
#include "content/public/engine_gpufence.h"
#include "content/public/engine_gpuimmutablesampler.h"
#include "content/public/engine_gpulayoutelement.h"
#include "content/public/engine_gpupipelineresource.h"
#include "content/public/engine_gpupipelinesignature.h"
#include "content/public/engine_gpupipelinestate.h"
#include "content/public/engine_gpuquery.h"
#include "content/public/engine_gpusampler.h"
#include "content/public/engine_gpushader.h"
#include "content/public/engine_gputexture.h"
#include "content/public/engine_gputexturesubresdata.h"

namespace content {

// IDL generator format:
// Inhert: refcounted only.
/*--urge(name:GPURenderDevice)--*/
class URGE_RUNTIME_API GPURenderDevice
    : public base::RefCounted<GPURenderDevice> {
 public:
  virtual ~GPURenderDevice() = default;

  /*--urge(name:BindFlags)--*/
  enum BindFlags {
    BIND_NONE = 0,
    BIND_VERTEX_BUFFER = 1u << 0u,
    BIND_INDEX_BUFFER = 1u << 1u,
    BIND_UNIFORM_BUFFER = 1u << 2u,
    BIND_SHADER_RESOURCE = 1u << 3u,
    BIND_STREAM_OUTPUT = 1u << 4u,
    BIND_RENDER_TARGET = 1u << 5u,
    BIND_DEPTH_STENCIL = 1u << 6u,
    BIND_UNORDERED_ACCESS = 1u << 7u,
    BIND_INDIRECT_DRAW_ARGS = 1u << 8u,
    BIND_INPUT_ATTACHMENT = 1u << 9u,
    BIND_RAY_TRACING = 1u << 10u,
    BIND_SHADING_RATE = 1u << 11u,
  };

  /*--urge(name:Usage)--*/
  enum Usage {
    USAGE_IMMUTABLE = 0,
    USAGE_DEFAULT,
    USAGE_DYNAMIC,
    USAGE_STAGING,
    USAGE_UNIFIED,
    USAGE_SPARSE,
  };

  /*--urge(name:CPUAccessFlags)--*/
  enum CPUAccessFlags {
    CPU_ACCESS_NONE = 0u,
    CPU_ACCESS_READ = 1u << 0u,
    CPU_ACCESS_WRITE = 1u << 1u,
  };

  /*--urge(name:BufferMode)--*/
  enum BufferMode {
    BUFFER_MODE_UNDEFINED = 0,
    BUFFER_MODE_FORMATTED,
    BUFFER_MODE_STRUCTURED,
    BUFFER_MODE_RAW,
  };

  /*--urge(name:ShaderType)--*/
  enum ShaderType {
    SHADER_TYPE_UNKNOWN = 0x0000,
    SHADER_TYPE_VERTEX = 0x0001,
    SHADER_TYPE_PIXEL = 0x0002,
    SHADER_TYPE_GEOMETRY = 0x0004,
    SHADER_TYPE_HULL = 0x0008,
    SHADER_TYPE_DOMAIN = 0x0010,
    SHADER_TYPE_COMPUTE = 0x0020,
    SHADER_TYPE_AMPLIFICATION = 0x0040,
    SHADER_TYPE_MESH = 0x0080,
    SHADER_TYPE_RAY_GEN = 0x0100,
    SHADER_TYPE_RAY_MISS = 0x0200,
    SHADER_TYPE_RAY_CLOSEST_HIT = 0x0400,
    SHADER_TYPE_RAY_ANY_HIT = 0x0800,
    SHADER_TYPE_RAY_INTERSECTION = 0x1000,
    SHADER_TYPE_CALLABLE = 0x2000,
    SHADER_TYPE_TILE = 0x4000,
    SHADER_TYPE_LAST = SHADER_TYPE_TILE,
    SHADER_TYPE_ALL = SHADER_TYPE_LAST * 2 - 1,
  };

  /*--urge(name:ShaderSourceLanguage)--*/
  enum ShaderSourceLanguage {
    SHADER_SOURCE_LANGUAGE_DEFAULT = 0,
    SHADER_SOURCE_LANGUAGE_HLSL,
    SHADER_SOURCE_LANGUAGE_GLSL,
    SHADER_SOURCE_LANGUAGE_GLSL_VERBATIM,
    SHADER_SOURCE_LANGUAGE_MSL,
    SHADER_SOURCE_LANGUAGE_MSL_VERBATIM,
    SHADER_SOURCE_LANGUAGE_MTLB,
    SHADER_SOURCE_LANGUAGE_WGSL,
    SHADER_SOURCE_LANGUAGE_COUNT,
  };

  /*--urge(name:ResourceDimension)--*/
  enum ResourceDimension {
    RESOURCE_DIM_UNDEFINED = 0,
    RESOURCE_DIM_BUFFER,
    RESOURCE_DIM_TEX_1D,
    RESOURCE_DIM_TEX_1D_ARRAY,
    RESOURCE_DIM_TEX_2D,
    RESOURCE_DIM_TEX_2D_ARRAY,
    RESOURCE_DIM_TEX_3D,
    RESOURCE_DIM_TEX_CUBE,
    RESOURCE_DIM_TEX_CUBE_ARRAY,
    RESOURCE_DIM_NUM_DIMENSIONS,
  };

  /*--urge(name:TextureFormat)--*/
  enum TextureFormat {
    TEX_FORMAT_UNKNOWN = 0,
    TEX_FORMAT_RGBA32_TYPELESS,
    TEX_FORMAT_RGBA32_FLOAT,
    TEX_FORMAT_RGBA32_UINT,
    TEX_FORMAT_RGBA32_SINT,
    TEX_FORMAT_RGB32_TYPELESS,
    TEX_FORMAT_RGB32_FLOAT,
    TEX_FORMAT_RGB32_UINT,
    TEX_FORMAT_RGB32_SINT,
    TEX_FORMAT_RGBA16_TYPELESS,
    TEX_FORMAT_RGBA16_FLOAT,
    TEX_FORMAT_RGBA16_UNORM,
    TEX_FORMAT_RGBA16_UINT,
    TEX_FORMAT_RGBA16_SNORM,
    TEX_FORMAT_RGBA16_SINT,
    TEX_FORMAT_RG32_TYPELESS,
    TEX_FORMAT_RG32_FLOAT,
    TEX_FORMAT_RG32_UINT,
    TEX_FORMAT_RG32_SINT,
    TEX_FORMAT_R32G8X24_TYPELESS,
    TEX_FORMAT_D32_FLOAT_S8X24_UINT,
    TEX_FORMAT_R32_FLOAT_X8X24_TYPELESS,
    TEX_FORMAT_X32_TYPELESS_G8X24_UINT,
    TEX_FORMAT_RGB10A2_TYPELESS,
    TEX_FORMAT_RGB10A2_UNORM,
    TEX_FORMAT_RGB10A2_UINT,
    TEX_FORMAT_R11G11B10_FLOAT,
    TEX_FORMAT_RGBA8_TYPELESS,
    TEX_FORMAT_RGBA8_UNORM,
    TEX_FORMAT_RGBA8_UNORM_SRGB,
    TEX_FORMAT_RGBA8_UINT,
    TEX_FORMAT_RGBA8_SNORM,
    TEX_FORMAT_RGBA8_SINT,
    TEX_FORMAT_RG16_TYPELESS,
    TEX_FORMAT_RG16_FLOAT,
    TEX_FORMAT_RG16_UNORM,
    TEX_FORMAT_RG16_UINT,
    TEX_FORMAT_RG16_SNORM,
    TEX_FORMAT_RG16_SINT,
    TEX_FORMAT_R32_TYPELESS,
    TEX_FORMAT_D32_FLOAT,
    TEX_FORMAT_R32_FLOAT,
    TEX_FORMAT_R32_UINT,
    TEX_FORMAT_R32_SINT,
    TEX_FORMAT_R24G8_TYPELESS,
    TEX_FORMAT_D24_UNORM_S8_UINT,
    TEX_FORMAT_R24_UNORM_X8_TYPELESS,
    TEX_FORMAT_X24_TYPELESS_G8_UINT,
    TEX_FORMAT_RG8_TYPELESS,
    TEX_FORMAT_RG8_UNORM,
    TEX_FORMAT_RG8_UINT,
    TEX_FORMAT_RG8_SNORM,
    TEX_FORMAT_RG8_SINT,
    TEX_FORMAT_R16_TYPELESS,
    TEX_FORMAT_R16_FLOAT,
    TEX_FORMAT_D16_UNORM,
    TEX_FORMAT_R16_UNORM,
    TEX_FORMAT_R16_UINT,
    TEX_FORMAT_R16_SNORM,
    TEX_FORMAT_R16_SINT,
    TEX_FORMAT_R8_TYPELESS,
    TEX_FORMAT_R8_UNORM,
    TEX_FORMAT_R8_UINT,
    TEX_FORMAT_R8_SNORM,
    TEX_FORMAT_R8_SINT,
    TEX_FORMAT_A8_UNORM,
    TEX_FORMAT_R1_UNORM,
    TEX_FORMAT_RGB9E5_SHAREDEXP,
    TEX_FORMAT_RG8_B8G8_UNORM,
    TEX_FORMAT_G8R8_G8B8_UNORM,
    TEX_FORMAT_BC1_TYPELESS,
    TEX_FORMAT_BC1_UNORM,
    TEX_FORMAT_BC1_UNORM_SRGB,
    TEX_FORMAT_BC2_TYPELESS,
    TEX_FORMAT_BC2_UNORM,
    TEX_FORMAT_BC2_UNORM_SRGB,
    TEX_FORMAT_BC3_TYPELESS,
    TEX_FORMAT_BC3_UNORM,
    TEX_FORMAT_BC3_UNORM_SRGB,
    TEX_FORMAT_BC4_TYPELESS,
    TEX_FORMAT_BC4_UNORM,
    TEX_FORMAT_BC4_SNORM,
    TEX_FORMAT_BC5_TYPELESS,
    TEX_FORMAT_BC5_UNORM,
    TEX_FORMAT_BC5_SNORM,
    TEX_FORMAT_B5G6R5_UNORM,
    TEX_FORMAT_B5G5R5A1_UNORM,
    TEX_FORMAT_BGRA8_UNORM,
    TEX_FORMAT_BGRX8_UNORM,
    TEX_FORMAT_R10G10B10_XR_BIAS_A2_UNORM,
    TEX_FORMAT_BGRA8_TYPELESS,
    TEX_FORMAT_BGRA8_UNORM_SRGB,
    TEX_FORMAT_BGRX8_TYPELESS,
    TEX_FORMAT_BGRX8_UNORM_SRGB,
    TEX_FORMAT_BC6H_TYPELESS,
    TEX_FORMAT_BC6H_UF16,
    TEX_FORMAT_BC6H_SF16,
    TEX_FORMAT_BC7_TYPELESS,
    TEX_FORMAT_BC7_UNORM,
    TEX_FORMAT_BC7_UNORM_SRGB,
    TEX_FORMAT_ETC2_RGB8_UNORM,
    TEX_FORMAT_ETC2_RGB8_UNORM_SRGB,
    TEX_FORMAT_ETC2_RGB8A1_UNORM,
    TEX_FORMAT_ETC2_RGB8A1_UNORM_SRGB,
    TEX_FORMAT_ETC2_RGBA8_UNORM,
    TEX_FORMAT_ETC2_RGBA8_UNORM_SRGB,
    TEX_FORMAT_NUM_FORMATS,
  };

  /*--urge(name:FilterType)--*/
  enum FilterType {
    FILTER_TYPE_UNKNOWN = 0,
    FILTER_TYPE_POINT,
    FILTER_TYPE_LINEAR,
    FILTER_TYPE_ANISOTROPIC,
    FILTER_TYPE_COMPARISON_POINT,
    FILTER_TYPE_COMPARISON_LINEAR,
    FILTER_TYPE_COMPARISON_ANISOTROPIC,
    FILTER_TYPE_MINIMUM_POINT,
    FILTER_TYPE_MINIMUM_LINEAR,
    FILTER_TYPE_MINIMUM_ANISOTROPIC,
    FILTER_TYPE_MAXIMUM_POINT,
    FILTER_TYPE_MAXIMUM_LINEAR,
    FILTER_TYPE_MAXIMUM_ANISOTROPIC,
    FILTER_TYPE_NUM_FILTERS,
  };

  /*--urge(name:TextureAddressMode)--*/
  enum TextureAddressMode {
    TEXTURE_ADDRESS_UNKNOWN = 0,
    TEXTURE_ADDRESS_WRAP = 1,
    TEXTURE_ADDRESS_MIRROR = 2,
    TEXTURE_ADDRESS_CLAMP = 3,
    TEXTURE_ADDRESS_BORDER = 4,
    TEXTURE_ADDRESS_MIRROR_ONCE = 5,
    TEXTURE_ADDRESS_NUM_MODES,
  };

  /*--urge(name:FenceType)--*/
  enum FenceType {
    FENCE_TYPE_CPU_WAIT_ONLY = 0,
    FENCE_TYPE_GENERAL = 1,
    FENCE_TYPE_LAST = FENCE_TYPE_GENERAL,
  };

  /*--urge(name:QueryType)--*/
  enum QueryType {
    QUERY_TYPE_UNDEFINED = 0,
    QUERY_TYPE_OCCLUSION,
    QUERY_TYPE_BINARY_OCCLUSION,
    QUERY_TYPE_TIMESTAMP,
    QUERY_TYPE_PIPELINE_STATISTICS,
    QUERY_TYPE_DURATION,
    QUERY_TYPE_NUM_TYPES,
  };

  /*--urge(name:PrimitiveTopology)--*/
  enum PrimitiveTopology {
    PRIMITIVE_TOPOLOGY_UNDEFINED = 0,
    PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    PRIMITIVE_TOPOLOGY_POINT_LIST,
    PRIMITIVE_TOPOLOGY_LINE_LIST,
    PRIMITIVE_TOPOLOGY_LINE_STRIP,
    PRIMITIVE_TOPOLOGY_TRIANGLE_LIST_ADJ,
    PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP_ADJ,
    PRIMITIVE_TOPOLOGY_LINE_LIST_ADJ,
    PRIMITIVE_TOPOLOGY_LINE_STRIP_ADJ,
    PRIMITIVE_TOPOLOGY_1_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_2_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_5_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_6_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_7_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_8_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_9_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_10_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_11_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_12_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_13_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_14_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_15_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_16_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_17_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_18_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_19_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_20_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_21_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_22_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_23_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_24_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_25_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_26_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_27_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_28_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_29_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_30_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_31_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_32_CONTROL_POINT_PATCHLIST,
    PRIMITIVE_TOPOLOGY_NUM_TOPOLOGIES,
  };

  /*--urge(name:FillMode)--*/
  enum FillMode {
    FILL_MODE_UNDEFINED = 0,
    FILL_MODE_WIREFRAME,
    FILL_MODE_SOLID,
    FILL_MODE_NUM_MODES,
  };

  /*--urge(name:CullMode)--*/
  enum CullMode {
    CULL_MODE_UNDEFINED = 0,
    CULL_MODE_NONE,
    CULL_MODE_FRONT,
    CULL_MODE_BACK,
    CULL_MODE_NUM_MODES,
  };

  /*--urge(name:ComparisonFunction)--*/
  enum ComparisonFunction {
    COMPARISON_FUNC_UNKNOWN = 0,
    COMPARISON_FUNC_NEVER,
    COMPARISON_FUNC_LESS,
    COMPARISON_FUNC_EQUAL,
    COMPARISON_FUNC_LESS_EQUAL,
    COMPARISON_FUNC_GREATER,
    COMPARISON_FUNC_NOT_EQUAL,
    COMPARISON_FUNC_GREATER_EQUAL,
    COMPARISON_FUNC_ALWAYS,
    COMPARISON_FUNC_NUM_FUNCTIONS,
  };

  /*--urge(name:create_buffer)--*/
  virtual scoped_refptr<GPUBuffer> CreateBuffer(
      uint64_t size,
      BindFlags bind_flags,
      Usage usage,
      CPUAccessFlags cpu_access,
      BufferMode mode,
      uint32_t element_by_stride,
      uint64_t immediate_context_mask,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_buffer)--*/
  virtual scoped_refptr<GPUBuffer> CreateBuffer(
      uint64_t size,
      BindFlags bind_flags,
      Usage usage,
      CPUAccessFlags cpu_access,
      BufferMode mode,
      uint32_t element_by_stride,
      uint64_t immediate_context_mask,
      const base::String& buffer_data,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_shader)--*/
  virtual scoped_refptr<GPUShader> CreateShader(
      const base::String& source,
      const base::String& entry_point,
      ShaderType type,
      bool combined_texture_samplers,
      const base::String& combined_sampler_suffix,
      ShaderSourceLanguage language,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_texture)--*/
  virtual scoped_refptr<GPUTexture> CreateTexture(
      ResourceDimension dim,
      uint32_t width,
      uint32_t height,
      uint32_t array_size_or_depth,
      TextureFormat format,
      uint32_t mip_levels,
      uint32_t sample_count,
      BindFlags bind_flags,
      Usage usage,
      CPUAccessFlags cpu_access,
      uint64_t immediate_context_mask,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_texture)--*/
  virtual scoped_refptr<GPUTexture> CreateTexture(
      ResourceDimension dim,
      uint32_t width,
      uint32_t height,
      uint32_t array_size_or_depth,
      TextureFormat format,
      uint32_t mip_levels,
      uint32_t sample_count,
      BindFlags bind_flags,
      Usage usage,
      CPUAccessFlags cpu_access,
      uint64_t immediate_context_mask,
      const base::Vector<scoped_refptr<GPUTextureSubResData>>& subresources,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_sampler)--*/
  virtual scoped_refptr<GPUSampler> CreateSampler(
      FilterType min_filter,
      FilterType mag_filter,
      FilterType mip_filter,
      TextureAddressMode address_u,
      TextureAddressMode address_v,
      TextureAddressMode address_w,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_graphics_pipeline_state)--*/
  virtual scoped_refptr<GPUPipelineState> CreateGraphicsPipelineState(
      const base::Vector<scoped_refptr<GPUPipelineSignature>>& signatures,
      bool alpha_to_coverage_enable,
      bool independent_blend_enable,
      const base::Vector<scoped_refptr<GPUBlendState>>& rtv_blend_states,
      FillMode fill_mode,
      CullMode cull_mode,
      bool front_counter_clockwise,
      bool depth_clip_enable,
      bool scissor_enable,
      bool depth_enable,
      bool depth_write_enable,
      ComparisonFunction depth_func,
      bool stencil_enable,
      uint8_t stencil_read_mask,
      uint8_t stencil_write_mask,
      const base::Vector<scoped_refptr<GPULayoutElement>>& input_layouts,
      uint32_t sample_mask,
      PrimitiveTopology primitive_topology,
      uint8_t viewports_num,
      uint8_t render_targets_num,
      const base::Vector<TextureFormat>& rtv_formats,
      TextureFormat dsv_format,
      bool readonly_dsv,
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
      FenceType type,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_query)--*/
  virtual scoped_refptr<GPUQuery> CreateQuery(
      QueryType type,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_pipeline_signature)--*/
  virtual scoped_refptr<GPUPipelineSignature> CreatePipelineSignature(
      const base::Vector<scoped_refptr<GPUPipelineResource>>& resources,
      const base::Vector<scoped_refptr<GPUImmutableSampler>>& samplers,
      uint8_t binding_index,
      bool use_combined_texture_samplers,
      const base::String& combined_sampler_suffix,
      ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPURENDERDEVICE_H_
