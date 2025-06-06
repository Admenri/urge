// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPURENDERDEVICE_H_
#define CONTENT_PUBLIC_ENGINE_GPURENDERDEVICE_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"
#include "content/public/engine_gpubuffer.h"
#include "content/public/engine_gpufence.h"
#include "content/public/engine_gpupipelinesignature.h"
#include "content/public/engine_gpupipelinestate.h"
#include "content/public/engine_gpuquery.h"
#include "content/public/engine_gpusampler.h"
#include "content/public/engine_gpushader.h"
#include "content/public/engine_gputexture.h"

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

  /*--urge(name:BlendFactor)--*/
  enum BlendFactor {
    BLEND_FACTOR_UNDEFINED = 0,
    BLEND_FACTOR_ZERO,
    BLEND_FACTOR_ONE,
    BLEND_FACTOR_SRC_COLOR,
    BLEND_FACTOR_INV_SRC_COLOR,
    BLEND_FACTOR_SRC_ALPHA,
    BLEND_FACTOR_INV_SRC_ALPHA,
    BLEND_FACTOR_DEST_ALPHA,
    BLEND_FACTOR_INV_DEST_ALPHA,
    BLEND_FACTOR_DEST_COLOR,
    BLEND_FACTOR_INV_DEST_COLOR,
    BLEND_FACTOR_SRC_ALPHA_SAT,
    BLEND_FACTOR_BLEND_FACTOR,
    BLEND_FACTOR_INV_BLEND_FACTOR,
    BLEND_FACTOR_SRC1_COLOR,
    BLEND_FACTOR_INV_SRC1_COLOR,
    BLEND_FACTOR_SRC1_ALPHA,
    BLEND_FACTOR_INV_SRC1_ALPHA,
    BLEND_FACTOR_NUM_FACTORS
  };

  /*--urge(name:BlendOperation)--*/
  enum BlendOperation {
    BLEND_OPERATION_UNDEFINED = 0,
    BLEND_OPERATION_ADD,
    BLEND_OPERATION_SUBTRACT,
    BLEND_OPERATION_REV_SUBTRACT,
    BLEND_OPERATION_MIN,
    BLEND_OPERATION_MAX,
    BLEND_OPERATION_NUM_OPERATIONS
  };

  /*--urge(name:ColorMask)--*/
  enum ColorMask {
    COLOR_MASK_NONE = 0u,
    COLOR_MASK_RED = 1u << 0u,
    COLOR_MASK_GREEN = 1u << 1u,
    COLOR_MASK_BLUE = 1u << 2u,
    COLOR_MASK_ALPHA = 1u << 3u,
    COLOR_MASK_RGB = COLOR_MASK_RED | COLOR_MASK_GREEN | COLOR_MASK_BLUE,
    COLOR_MASK_ALL = (COLOR_MASK_RGB | COLOR_MASK_ALPHA)
  };

  /*--urge(name:LogicOperation)--*/
  enum LogicOperation {
    LOGIC_OP_CLEAR = 0,
    LOGIC_OP_SET,
    LOGIC_OP_COPY,
    LOGIC_OP_COPY_INVERTED,
    LOGIC_OP_NOOP,
    LOGIC_OP_INVERT,
    LOGIC_OP_AND,
    LOGIC_OP_NAND,
    LOGIC_OP_OR,
    LOGIC_OP_NOR,
    LOGIC_OP_XOR,
    LOGIC_OP_EQUIV,
    LOGIC_OP_AND_REVERSE,
    LOGIC_OP_AND_INVERTED,
    LOGIC_OP_OR_REVERSE,
    LOGIC_OP_OR_INVERTED,
    LOGIC_OP_NUM_OPERATIONS
  };

  /*--urge(name:StencilOp)--*/
  enum StencilOp {
    STENCIL_OP_UNDEFINED = 0,
    STENCIL_OP_KEEP = 1,
    STENCIL_OP_ZERO = 2,
    STENCIL_OP_REPLACE = 3,
    STENCIL_OP_INCR_SAT = 4,
    STENCIL_OP_DECR_SAT = 5,
    STENCIL_OP_INVERT = 6,
    STENCIL_OP_INCR_WRAP = 7,
    STENCIL_OP_DECR_WRAP = 8,
    STENCIL_OP_NUM_OPS
  };

  /*--urge(name:ValueType)--*/
  enum ValueType {
    VT_UNDEFINED = 0,
    VT_INT8,
    VT_INT16,
    VT_INT32,
    VT_UINT8,
    VT_UINT16,
    VT_UINT32,
    VT_FLOAT16,
    VT_FLOAT32,
    VT_FLOAT64,
  };

  /*--urge(name:InputElementFrequency)--*/
  enum InputElementFrequency {
    INPUT_ELEMENT_FREQUENCY_UNDEFINED = 0,
    INPUT_ELEMENT_FREQUENCY_PER_VERTEX,
    INPUT_ELEMENT_FREQUENCY_PER_INSTANCE,
    INPUT_ELEMENT_FREQUENCY_NUM_FREQUENCIES
  };

  /*--urge(name:ShaderResourceType)--*/
  enum ShaderResourceType {
    SHADER_RESOURCE_TYPE_UNKNOWN = 0,
    SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
    SHADER_RESOURCE_TYPE_TEXTURE_SRV,
    SHADER_RESOURCE_TYPE_BUFFER_SRV,
    SHADER_RESOURCE_TYPE_TEXTURE_UAV,
    SHADER_RESOURCE_TYPE_BUFFER_UAV,
    SHADER_RESOURCE_TYPE_SAMPLER,
    SHADER_RESOURCE_TYPE_INPUT_ATTACHMENT,
    SHADER_RESOURCE_TYPE_ACCEL_STRUCT,
    SHADER_RESOURCE_TYPE_LAST = SHADER_RESOURCE_TYPE_ACCEL_STRUCT
  };

  /*--urge(name:ShaderResourceVariableType)--*/
  enum ShaderResourceVariableType {
    SHADER_RESOURCE_VARIABLE_TYPE_STATIC = 0,
    SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE,
    SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC,
    SHADER_RESOURCE_VARIABLE_TYPE_NUM_TYPES
  };

  /*--urge(name:BufferDesc)--*/
  struct BufferDesc {
    uint64_t size = 0;
    BindFlags bind_flags = BIND_NONE;
    Usage usage = USAGE_DEFAULT;
    CPUAccessFlags cpu_access_flags = CPU_ACCESS_NONE;
    BufferMode mode = BUFFER_MODE_UNDEFINED;
    uint32_t element_byte_stride = 0;
    uint64_t immediate_context_mask = 1;
  };

  /*--urge(name:ShaderCreateInfo)--*/
  struct ShaderCreateInfo {
    base::String source;
    base::String entry_point = "main";
    ShaderType type = SHADER_TYPE_UNKNOWN;
    bool combined_texture_samplers = true;
    base::String combined_sampler_suffix = "_sampler";
    ShaderSourceLanguage language = SHADER_SOURCE_LANGUAGE_DEFAULT;
  };

  /*--urge(name:TextureDesc)--*/
  struct TextureDesc {
    ResourceDimension type = RESOURCE_DIM_UNDEFINED;
    uint32_t width = 0;
    uint32_t height = 0;
    uint32_t depth_or_array_size = 1;
    TextureFormat format = TEX_FORMAT_UNKNOWN;
    uint32_t mip_levels = 1;
    uint32_t sample_count = 1;
    BindFlags bind_flags = BIND_NONE;
    Usage usage = USAGE_DEFAULT;
    CPUAccessFlags cpu_access_flags = CPU_ACCESS_NONE;
    uint64_t immediate_context_mask = 1;
  };

  /*--urge(name:TextureSubResData)--*/
  struct TextureSubResData {
    base::String data;
    uint64_t stride = 0;
    uint64_t depth_stride = 0;
  };

  /*--urge(name:SamplerDesc)--*/
  struct SamplerDesc {
    FilterType min_filter = FILTER_TYPE_LINEAR;
    FilterType mag_filter = FILTER_TYPE_LINEAR;
    FilterType mip_filter = FILTER_TYPE_LINEAR;
    TextureAddressMode address_u = TEXTURE_ADDRESS_CLAMP;
    TextureAddressMode address_v = TEXTURE_ADDRESS_CLAMP;
    TextureAddressMode address_w = TEXTURE_ADDRESS_CLAMP;
  };

  /*--urge(name:RenderTargetBlendDesc)--*/
  struct RenderTargetBlendDesc {
    bool blend_enable = false;
    bool logic_operation_enable = false;
    BlendFactor src_blend = BLEND_FACTOR_ONE;
    BlendFactor dest_blend = BLEND_FACTOR_ZERO;
    BlendOperation blend_op = BLEND_OPERATION_ADD;
    BlendFactor src_blend_alpha = BLEND_FACTOR_ONE;
    BlendFactor dest_blend_alpha = BLEND_FACTOR_ZERO;
    BlendOperation blend_op_alpha = BLEND_OPERATION_ADD;
    LogicOperation logic_op = LOGIC_OP_NOOP;
    ColorMask render_target_write_mask = COLOR_MASK_ALL;
  };

  /*--urge(name:BlendStateDesc)--*/
  struct BlendStateDesc {
    bool alpha_to_coverage_enable = false;
    bool independent_blend_enable = false;
    base::Vector<RenderTargetBlendDesc> render_targets;
  };

  /*--urge(name:RasterizerStateDesc)--*/
  struct RasterizerStateDesc {
    FillMode fill_mode = FILL_MODE_SOLID;
    CullMode cull_mode = CULL_MODE_BACK;
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
    StencilOp stencil_fail_op = STENCIL_OP_KEEP;
    StencilOp stencil_depth_fail_op = STENCIL_OP_KEEP;
    StencilOp stencil_pass_op = STENCIL_OP_KEEP;
    ComparisonFunction stencil_func = COMPARISON_FUNC_ALWAYS;
  };

  /*--urge(name:DepthStencilStateDesc)--*/
  struct DepthStencilStateDesc {
    bool depth_enable = false;
    bool depth_write_enable = false;
    ComparisonFunction depth_func = COMPARISON_FUNC_LESS;
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
    ValueType value_type = VT_FLOAT32;
    bool is_normalized = true;
    uint32_t relative_offset = 0xFFFFFFFFU;
    uint32_t stride = 0xFFFFFFFFU;
    InputElementFrequency frequency = INPUT_ELEMENT_FREQUENCY_PER_VERTEX;
    uint32_t instance_data_step_rate = 1;
  };

  /*--urge(name:GraphicsPipelineDesc)--*/
  struct GraphicsPipelineDesc {
    std::optional<BlendStateDesc> blend_desc;
    uint32_t sample_mask;
    std::optional<RasterizerStateDesc> rasterizer_desc;
    std::optional<DepthStencilStateDesc> depth_stencil_desc;
    std::optional<InputLayoutElement> input_layout;
    PrimitiveTopology primitive_topology = PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    uint8_t num_viewports = 1;
    uint8_t num_render_targets = 0;
    uint8_t subpass_index = 0;
    base::Vector<TextureFormat> rtv_formats;
    TextureFormat dsv_format = TEX_FORMAT_UNKNOWN;
    bool readonly_dsv = false;
    uint8_t multisample_count = 1;
    uint8_t multisample_quality = 0;
    uint32_t node_mask = 0;
  };

  /*--urge(name:PipelineResourceDesc)--*/
  struct PipelineResourceDesc {
    base::String name;
    ShaderType shader_stages = SHADER_TYPE_UNKNOWN;
    uint32_t array_size = 1;
    ShaderResourceType resource_type = SHADER_RESOURCE_TYPE_UNKNOWN;
    ShaderResourceVariableType var_type = SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE;
  };

  /*--urge(name:ImmutableSamplerDesc)--*/
  struct ImmutableSamplerDesc {
    ShaderType shader_stages = SHADER_TYPE_UNKNOWN;
    base::String sampler_name;
    std::optional<SamplerDesc> desc;
  };

  /*--urge(name:create_buffer)--*/
  virtual scoped_refptr<GPUBuffer> CreateBuffer(
      const std::optional<BufferDesc>& desc,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_buffer)--*/
  virtual scoped_refptr<GPUBuffer> CreateBuffer(
      const std::optional<BufferDesc>& desc,
      const base::String& data,
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
      const base::Vector<TextureSubResData>& subresources,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_sampler)--*/
  virtual scoped_refptr<GPUSampler> CreateSampler(
      const std::optional<SamplerDesc>& desc,
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
      FenceType type,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_query)--*/
  virtual scoped_refptr<GPUQuery> CreateQuery(
      QueryType type,
      ExceptionState& exception_state) = 0;

  /*--urge(name:create_pipeline_signature)--*/
  virtual scoped_refptr<GPUPipelineSignature> CreatePipelineSignature(
      const base::Vector<PipelineResourceDesc>& resources,
      const base::Vector<ImmutableSamplerDesc>& samplers,
      uint8_t binding_index,
      bool use_combined_texture_samplers,
      const base::String& combined_sampler_suffix,
      ExceptionState& exception_state) = 0;
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPURENDERDEVICE_H_
