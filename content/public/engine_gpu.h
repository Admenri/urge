// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_ENGINE_GPU_H_
#define CONTENT_PUBLIC_ENGINE_GPU_H_

#include "base/memory/ref_counted.h"
#include "content/content_config.h"
#include "content/context/exception_state.h"

namespace content {

/*--urge(name:GPU,is_module)--*/
class URGE_RUNTIME_API GPU : public base::RefCounted<GPU> {
 public:
  virtual ~GPU() = default;

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

  /*--urge(name:ResourceStateTransitionMode)--*/
  enum ResourceStateTransitionMode {
    RESOURCE_STATE_TRANSITION_MODE_NONE = 0,
    RESOURCE_STATE_TRANSITION_MODE_TRANSITION,
    RESOURCE_STATE_TRANSITION_MODE_VERIFY,
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

  /*--urge(name:ClearDepthStencilFlags)--*/
  enum ClearDepthStencilFlags {
    CLEAR_DEPTH_FLAG_NONE = 0x00,
    CLEAR_DEPTH_FLAG = 0x01,
    CLEAR_STENCIL_FLAG = 0x02,
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

  /*--urge(name:BufferViewType)--*/
  enum BufferViewType {
    BUFFER_VIEW_UNDEFINED = 0,
    BUFFER_VIEW_SHADER_RESOURCE,
    BUFFER_VIEW_UNORDERED_ACCESS,
    BUFFER_VIEW_NUM_VIEWS,
  };

  /*--urge(name:ResourceState)--*/
  enum ResourceState {
    RESOURCE_STATE_UNKNOWN = 0,
    RESOURCE_STATE_UNDEFINED = 1u << 0,
    RESOURCE_STATE_VERTEX_BUFFER = 1u << 1,
    RESOURCE_STATE_CONSTANT_BUFFER = 1u << 2,
    RESOURCE_STATE_INDEX_BUFFER = 1u << 3,
    RESOURCE_STATE_RENDER_TARGET = 1u << 4,
    RESOURCE_STATE_UNORDERED_ACCESS = 1u << 5,
    RESOURCE_STATE_DEPTH_WRITE = 1u << 6,
    RESOURCE_STATE_DEPTH_READ = 1u << 7,
    RESOURCE_STATE_SHADER_RESOURCE = 1u << 8,
    RESOURCE_STATE_STREAM_OUT = 1u << 9,
    RESOURCE_STATE_INDIRECT_ARGUMENT = 1u << 10,
    RESOURCE_STATE_COPY_DEST = 1u << 11,
    RESOURCE_STATE_COPY_SOURCE = 1u << 12,
    RESOURCE_STATE_RESOLVE_DEST = 1u << 13,
    RESOURCE_STATE_RESOLVE_SOURCE = 1u << 14,
    RESOURCE_STATE_INPUT_ATTACHMENT = 1u << 15,
    RESOURCE_STATE_PRESENT = 1u << 16,
    RESOURCE_STATE_BUILD_AS_READ = 1u << 17,
    RESOURCE_STATE_BUILD_AS_WRITE = 1u << 18,
    RESOURCE_STATE_RAY_TRACING = 1u << 19,
    RESOURCE_STATE_COMMON = 1u << 20,
    RESOURCE_STATE_SHADING_RATE = 1u << 21,
    RESOURCE_STATE_MAX_BIT = RESOURCE_STATE_SHADING_RATE,
    RESOURCE_STATE_GENERIC_READ =
        RESOURCE_STATE_VERTEX_BUFFER | RESOURCE_STATE_CONSTANT_BUFFER |
        RESOURCE_STATE_INDEX_BUFFER | RESOURCE_STATE_SHADER_RESOURCE |
        RESOURCE_STATE_INDIRECT_ARGUMENT | RESOURCE_STATE_COPY_SOURCE,
  };

  /*--urge(name:PipelineStateStatus)--*/
  enum PipelineStateStatus {
    PIPELINE_STATE_STATUS_UNINITIALIZED = 0,
    PIPELINE_STATE_STATUS_COMPILING,
    PIPELINE_STATE_STATUS_READY,
    PIPELINE_STATE_STATUS_FAILED,
  };

  /*--urge(name:ShaderCodeBasicType)--*/
  enum ShaderCodeBasicType {
    SHADER_CODE_BASIC_TYPE_UNKNOWN = 0,
    SHADER_CODE_BASIC_TYPE_VOID,
    SHADER_CODE_BASIC_TYPE_BOOL,
    SHADER_CODE_BASIC_TYPE_INT,
    SHADER_CODE_BASIC_TYPE_INT8,
    SHADER_CODE_BASIC_TYPE_INT16,
    SHADER_CODE_BASIC_TYPE_INT64,
    SHADER_CODE_BASIC_TYPE_UINT,
    SHADER_CODE_BASIC_TYPE_UINT8,
    SHADER_CODE_BASIC_TYPE_UINT16,
    SHADER_CODE_BASIC_TYPE_UINT64,
    SHADER_CODE_BASIC_TYPE_FLOAT,
    SHADER_CODE_BASIC_TYPE_FLOAT16,
    SHADER_CODE_BASIC_TYPE_DOUBLE,
    SHADER_CODE_BASIC_TYPE_MIN8FLOAT,
    SHADER_CODE_BASIC_TYPE_MIN10FLOAT,
    SHADER_CODE_BASIC_TYPE_MIN16FLOAT,
    SHADER_CODE_BASIC_TYPE_MIN12INT,
    SHADER_CODE_BASIC_TYPE_MIN16INT,
    SHADER_CODE_BASIC_TYPE_MIN16UINT,
    SHADER_CODE_BASIC_TYPE_STRING,
    SHADER_CODE_BASIC_TYPE_LAST = SHADER_CODE_BASIC_TYPE_STRING,
  };

  /*--urge(name:ShaderCodeVariableClass)--*/
  enum ShaderCodeVariableClass {
    SHADER_CODE_VARIABLE_CLASS_UNKNOWN = 0,
    SHADER_CODE_VARIABLE_CLASS_SCALAR,
    SHADER_CODE_VARIABLE_CLASS_VECTOR,
    SHADER_CODE_VARIABLE_CLASS_MATRIX_ROWS,
    SHADER_CODE_VARIABLE_CLASS_MATRIX_COLUMNS,
    SHADER_CODE_VARIABLE_CLASS_STRUCT,
    SHADER_CODE_VARIABLE_CLASS_LAST = SHADER_CODE_VARIABLE_CLASS_STRUCT,
  };

  /*--urge(name:ShaderStatus)--*/
  enum ShaderStatus {
    SHADER_STATUS_UNINITIALIZED = 0,
    SHADER_STATUS_COMPILING,
    SHADER_STATUS_READY,
    SHADER_STATUS_FAILED,
  };

  /*--urge(name:TextureViewType)--*/
  enum TextureViewType {
    TEXTURE_VIEW_UNDEFINED = 0,
    TEXTURE_VIEW_SHADER_RESOURCE,
    TEXTURE_VIEW_RENDER_TARGET,
    TEXTURE_VIEW_DEPTH_STENCIL,
    TEXTURE_VIEW_READ_ONLY_DEPTH_STENCIL,
    TEXTURE_VIEW_UNORDERED_ACCESS,
    TEXTURE_VIEW_SHADING_RATE,
    TEXTURE_VIEW_LAST = TEXTURE_VIEW_SHADING_RATE,
  };

  /*--urge(name:UAVAccessFlag)--*/
  enum UAVAccessFlag {
    UAV_ACCESS_UNSPECIFIED = 0x00,
    UAV_ACCESS_FLAG_READ = 0x01,
    UAV_ACCESS_FLAG_WRITE = 0x02,
    UAV_ACCESS_FLAG_READ_WRITE = UAV_ACCESS_FLAG_READ | UAV_ACCESS_FLAG_WRITE,
    UAV_ACCESS_FLAG_LAST = UAV_ACCESS_FLAG_READ_WRITE,
  };

  /*--urge(name:TextureViewFlags)--*/
  enum TextureViewFlags {
    TEXTURE_VIEW_FLAG_NONE = 0,
    TEXTURE_VIEW_FLAG_ALLOW_MIP_MAP_GENERATION = 1u << 0,
    TEXTURE_VIEW_FLAG_LAST = TEXTURE_VIEW_FLAG_ALLOW_MIP_MAP_GENERATION,
  };

  /*--urge(name:ShaderResourceVariableTypeFlags)--*/
  enum ShaderResourceVariableTypeFlags {
    SHADER_RESOURCE_VARIABLE_TYPE_FLAG_NONE = 0x00,
    SHADER_RESOURCE_VARIABLE_TYPE_FLAG_STATIC =
        (0x01 << SHADER_RESOURCE_VARIABLE_TYPE_STATIC),
    SHADER_RESOURCE_VARIABLE_TYPE_FLAG_MUTABLE =
        (0x01 << SHADER_RESOURCE_VARIABLE_TYPE_MUTABLE),
    SHADER_RESOURCE_VARIABLE_TYPE_FLAG_DYNAMIC =
        (0x01 << SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC),
    SHADER_RESOURCE_VARIABLE_TYPE_FLAG_MUT_DYN =
        SHADER_RESOURCE_VARIABLE_TYPE_FLAG_MUTABLE |
        SHADER_RESOURCE_VARIABLE_TYPE_FLAG_DYNAMIC,
    SHADER_RESOURCE_VARIABLE_TYPE_FLAG_ALL =
        SHADER_RESOURCE_VARIABLE_TYPE_FLAG_STATIC |
        SHADER_RESOURCE_VARIABLE_TYPE_FLAG_MUTABLE |
        SHADER_RESOURCE_VARIABLE_TYPE_FLAG_DYNAMIC,
    SHADER_RESOURCE_VARIABLE_TYPE_FLAG_LAST =
        SHADER_RESOURCE_VARIABLE_TYPE_FLAG_ALL,
  };

  /*--urge(name:BindShaderResourcesFlags)--*/
  enum BindShaderResourcesFlags {
    BIND_SHADER_RESOURCES_UPDATE_STATIC =
        SHADER_RESOURCE_VARIABLE_TYPE_FLAG_STATIC,
    BIND_SHADER_RESOURCES_UPDATE_MUTABLE =
        SHADER_RESOURCE_VARIABLE_TYPE_FLAG_MUTABLE,
    BIND_SHADER_RESOURCES_UPDATE_DYNAMIC =
        SHADER_RESOURCE_VARIABLE_TYPE_FLAG_DYNAMIC,
    BIND_SHADER_RESOURCES_UPDATE_ALL = SHADER_RESOURCE_VARIABLE_TYPE_FLAG_ALL,
    BIND_SHADER_RESOURCES_KEEP_EXISTING = 0x08,
    BIND_SHADER_RESOURCES_VERIFY_ALL_RESOLVED = 0x10,
    BIND_SHADER_RESOURCES_ALLOW_OVERWRITE = 0x20,
    BIND_SHADER_RESOURCES_FLAG_LAST = BIND_SHADER_RESOURCES_ALLOW_OVERWRITE,
  };

  /*--urge(name:SetShaderResourceFlags)--*/
  enum SetShaderResourceFlags {
    SET_SHADER_RESOURCE_FLAG_NONE = 0,
    SET_SHADER_RESOURCE_FLAG_ALLOW_OVERWRITE = 1u << 0,
  };

  /*--urge(name:StateTransitionType)--*/
  enum StateTransitionType {
    STATE_TRANSITION_TYPE_IMMEDIATE = 0,
    STATE_TRANSITION_TYPE_BEGIN,
    STATE_TRANSITION_TYPE_END,
  };

  /*--urge(name:StateTransitionFlags)--*/
  enum StateTransitionFlags {
    STATE_TRANSITION_FLAG_NONE = 0,
    STATE_TRANSITION_FLAG_UPDATE_STATE = 1u << 0,
    STATE_TRANSITION_FLAG_DISCARD_CONTENT = 1u << 1,
    STATE_TRANSITION_FLAG_ALIASING = 1u << 2,
  };

  /*--urge(name:MapType)--*/
  enum MapType {
    MAP_READ = 0x01,
    MAP_WRITE = 0x02,
    MAP_READ_WRITE = 0x03,
  };

  /*--urge(name:MapFlags)--*/
  enum MapFlags {
    MAP_FLAG_NONE = 0x000,
    MAP_FLAG_DO_NOT_WAIT = 0x001,
    MAP_FLAG_DISCARD = 0x002,
    MAP_FLAG_NO_OVERWRITE = 0x004,
  };

  /*--urge(name:SetVertexBuffersFlags)--*/
  enum SetVertexBuffersFlags {
    SET_VERTEX_BUFFERS_FLAG_NONE = 0x00,
    SET_VERTEX_BUFFERS_FLAG_RESET = 0x01,
  };

  /*--urge(name:CommandQueueType)--*/
  enum CommandQueueType {
    COMMAND_QUEUE_TYPE_UNKNOWN = 0,
    COMMAND_QUEUE_TYPE_TRANSFER = 1u << 0,
    COMMAND_QUEUE_TYPE_COMPUTE = (1u << 1) | COMMAND_QUEUE_TYPE_TRANSFER,
    COMMAND_QUEUE_TYPE_GRAPHICS = (1u << 2) | COMMAND_QUEUE_TYPE_COMPUTE,
    COMMAND_QUEUE_TYPE_PRIMARY_MASK = COMMAND_QUEUE_TYPE_TRANSFER |
                                      COMMAND_QUEUE_TYPE_COMPUTE |
                                      COMMAND_QUEUE_TYPE_GRAPHICS,
    COMMAND_QUEUE_TYPE_SPARSE_BINDING = (1u << 3),
    COMMAND_QUEUE_TYPE_LAST = COMMAND_QUEUE_TYPE_GRAPHICS,
    COMMAND_QUEUE_TYPE_MAX_BIT = COMMAND_QUEUE_TYPE_GRAPHICS,
  };

  /*--urge(name:ShaderCompileFlags)--*/
  enum ShaderCompileFlags {
    SHADER_COMPILE_FLAG_NONE = 0,
    SHADER_COMPILE_FLAG_ENABLE_UNBOUNDED_ARRAYS = 1u << 0u,
    SHADER_COMPILE_FLAG_SKIP_REFLECTION = 1u << 1u,
    SHADER_COMPILE_FLAG_ASYNCHRONOUS = 1u << 2u,
    SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR = 1u << 3u,
    SHADER_COMPILE_FLAG_HLSL_TO_SPIRV_VIA_GLSL = 1u << 4u,
    SHADER_COMPILE_FLAG_LAST = SHADER_COMPILE_FLAG_HLSL_TO_SPIRV_VIA_GLSL,
  };
};

}  // namespace content

#endif  //! CONTENT_PUBLIC_ENGINE_GPU_H_
