// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/pipeline/render_pipeline.h"

#include "base/debug/logging.h"
#include "renderer/pipeline/builtin_hlsl.h"

namespace renderer {

namespace {

constexpr char kShaderGAMMA2LINEAR[] =
    "((Gamma) < 0.04045 ? (Gamma) / 12.92 : pow(max((Gamma) + 0.055, 0.0) / "
    "1.055, 2.4))";

constexpr char kShaderSRGBA2LINEAR[] =
    "col.r = GAMMA_TO_LINEAR(col.r); "
    "col.g = GAMMA_TO_LINEAR(col.g); "
    "col.b = GAMMA_TO_LINEAR(col.b); "
    "col.a = 1.0 - GAMMA_TO_LINEAR(1.0 - col.a);";

constexpr char kShaderNdcProcessor[] = R"(
#if defined(DESKTOP_GL) || defined(GL_ES)
#define URGE_NDC_PROCESS(var) var.y = -var.y
#else
#define URGE_NDC_PROCESS(x)
#endif // ! DESKTOP_GL || GL_ES
)";

Diligent::RenderTargetBlendDesc GetBlendState(BlendType type) {
  Diligent::RenderTargetBlendDesc state;
  switch (type) {
    default:
    case BLEND_TYPE_NORMAL:
      state.BlendEnable = Diligent::True;
      state.BlendOp = Diligent::BLEND_OPERATION_ADD;
      state.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
      state.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      state.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
      state.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      break;
    case BLEND_TYPE_ADDITION:
      state.BlendEnable = Diligent::True;
      state.BlendOp = Diligent::BLEND_OPERATION_ADD;
      state.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
      state.DestBlend = Diligent::BLEND_FACTOR_ONE;
      state.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
      state.DestBlendAlpha = Diligent::BLEND_FACTOR_ONE;
      break;
    case BLEND_TYPE_SUBSTRACTION:
      state.BlendEnable = Diligent::True;
      state.BlendOp = Diligent::BLEND_OPERATION_REV_SUBTRACT;
      state.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
      state.DestBlend = Diligent::BLEND_FACTOR_ONE;
      state.SrcBlendAlpha = Diligent::BLEND_FACTOR_ZERO;
      state.DestBlendAlpha = Diligent::BLEND_FACTOR_ONE;
      break;
    case BLEND_TYPE_MULTIPLY:
      state.BlendEnable = Diligent::True;
      state.BlendOp = Diligent::BLEND_OPERATION_ADD;
      state.SrcBlend = Diligent::BLEND_FACTOR_DEST_COLOR;
      state.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      state.SrcBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      state.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      break;
    case BLEND_TYPE_SCREEN:
      state.BlendEnable = Diligent::True;
      state.BlendOp = Diligent::BLEND_OPERATION_ADD;
      state.SrcBlend = Diligent::BLEND_FACTOR_ONE;
      state.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      state.SrcBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      state.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      break;
    case BLEND_TYPE_KEEP_ALPHA:
      state.BlendEnable = Diligent::True;
      state.BlendOp = Diligent::BLEND_OPERATION_ADD;
      state.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
      state.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      state.SrcBlendAlpha = Diligent::BLEND_FACTOR_ZERO;
      state.DestBlendAlpha = Diligent::BLEND_FACTOR_ONE;
      break;
    case BLEND_TYPE_NORMAL_PMA:
      state.BlendEnable = Diligent::True;
      state.BlendOp = Diligent::BLEND_OPERATION_ADD;
      state.SrcBlend = Diligent::BLEND_FACTOR_ONE;
      state.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      state.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
      state.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      break;
    case BLEND_TYPE_ADDITION_PMA:
      state.BlendEnable = Diligent::True;
      state.BlendOp = Diligent::BLEND_OPERATION_ADD;
      state.SrcBlend = Diligent::BLEND_FACTOR_ONE;
      state.DestBlend = Diligent::BLEND_FACTOR_ONE;
      state.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
      state.DestBlendAlpha = Diligent::BLEND_FACTOR_ONE;
      break;
    case BLEND_TYPE_NO_BLEND:
      state.BlendEnable = Diligent::False;
      break;
  }

  return state;
}

}  // namespace

RenderPipelineBase::RenderPipelineBase(Diligent::IRenderDevice* device)
    : device_(device) {}

void RenderPipelineBase::BuildPipeline(
    const ShaderSource& shader_source,
    const std::vector<Diligent::LayoutElement>& input_layout,
    const std::vector<RRefPtr<Diligent::IPipelineResourceSignature>>&
        signatures,
    Diligent::TEXTURE_FORMAT target_format,
    Diligent::TEXTURE_FORMAT depth_stencil_format) {
  // Make pipeline debug name
  std::string pipeline_name = "pipeline<" + shader_source.name + ">";

  // Make graphics pipeline state
  Diligent::GraphicsPipelineStateCreateInfo pipeline_state_desc;
  pipeline_state_desc.PSODesc.Name = pipeline_name.c_str();
  pipeline_state_desc.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;

  pipeline_state_desc.GraphicsPipeline.NumRenderTargets = 1;
  pipeline_state_desc.GraphicsPipeline.RTVFormats[0] = target_format;
  pipeline_state_desc.GraphicsPipeline.PrimitiveTopology =
      Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  pipeline_state_desc.GraphicsPipeline.RasterizerDesc.CullMode =
      Diligent::CULL_MODE_NONE;
  pipeline_state_desc.GraphicsPipeline.RasterizerDesc.ScissorEnable =
      Diligent::True;

  // Make vertex shader and pixel shader
  Diligent::ShaderCreateInfo shader_desc;
  RRefPtr<Diligent::IShader> vertex_shader_object, pixel_shader_object;
  shader_desc.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
  shader_desc.Desc.UseCombinedTextureSamplers = Diligent::True;
  shader_desc.CompileFlags =
      Diligent::SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

  {
    // Preprocess shader source
    std::string processed_vertex_shader = kShaderNdcProcessor;
    processed_vertex_shader.push_back('\n');
    processed_vertex_shader += shader_source.vertex_shader;

    // Vertex shader
    shader_desc.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    shader_desc.EntryPoint = shader_source.vertex_entry.c_str();
    shader_desc.Desc.Name = shader_source.name.c_str();
    shader_desc.Source = processed_vertex_shader.c_str();
    shader_desc.SourceLength = processed_vertex_shader.size();
    shader_desc.Macros.Count = shader_source.macros.size();
    shader_desc.Macros.Elements = shader_source.macros.data();
    device_->CreateShader(shader_desc, &vertex_shader_object);

    // Pixel shader
    shader_desc.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    shader_desc.EntryPoint = shader_source.pixel_entry.c_str();
    shader_desc.Desc.Name = shader_source.name.c_str();
    shader_desc.Source = shader_source.pixel_shader.c_str();
    shader_desc.SourceLength = shader_source.pixel_shader.size();
    shader_desc.Macros.Count = shader_source.macros.size();
    shader_desc.Macros.Elements = shader_source.macros.data();
    device_->CreateShader(shader_desc, &pixel_shader_object);
  }

  pipeline_state_desc.pVS = vertex_shader_object;
  pipeline_state_desc.pPS = pixel_shader_object;

  // Setup input attribute elements
  pipeline_state_desc.GraphicsPipeline.InputLayout.LayoutElements =
      input_layout.data();
  pipeline_state_desc.GraphicsPipeline.InputLayout.NumElements =
      input_layout.size();

  // Setup resource signature
  resource_signatures_ = signatures;
  std::vector<Diligent::IPipelineResourceSignature*> raw_signatures;
  for (const auto& it : resource_signatures_)
    raw_signatures.push_back(it);
  pipeline_state_desc.ResourceSignaturesCount = resource_signatures_.size();
  pipeline_state_desc.ppResourceSignatures = raw_signatures.data();

  // Make all color blend type pipelines
  for (int32_t blend_index = 0; blend_index < BLEND_TYPE_NUMS; ++blend_index) {
    for (int32_t depth_index = 0; depth_index < 2; ++depth_index) {
      // Color blend
      pipeline_state_desc.GraphicsPipeline.BlendDesc.RenderTargets[0] =
          GetBlendState(static_cast<BlendType>(blend_index));

      // Depth stencil
      if (depth_index) {
        pipeline_state_desc.GraphicsPipeline.DSVFormat = depth_stencil_format;
        pipeline_state_desc.GraphicsPipeline.DepthStencilDesc.DepthFunc =
            Diligent::COMPARISON_FUNC_LESS_EQUAL;
        pipeline_state_desc.GraphicsPipeline.DepthStencilDesc.DepthEnable =
            Diligent::True;
      } else {
        pipeline_state_desc.GraphicsPipeline.DSVFormat =
            Diligent::TEX_FORMAT_UNKNOWN;
        pipeline_state_desc.GraphicsPipeline.DepthStencilDesc.DepthEnable =
            Diligent::False;
      }

      // Make pipeline state
      RRefPtr<Diligent::IPipelineState> pipeline_state;
      device_->CreateGraphicsPipelineState(pipeline_state_desc,
                                           &pipeline_state);

      pipelines_[blend_index][depth_index] = pipeline_state;
    }
  }
}

RRefPtr<Diligent::IPipelineResourceSignature>
RenderPipelineBase::MakeResourceSignature(
    const std::vector<Diligent::PipelineResourceDesc>& variables,
    const std::vector<Diligent::ImmutableSamplerDesc>& samplers,
    uint8_t binding_index) {
  Diligent::PipelineResourceSignatureDesc resource_signature_desc;
  resource_signature_desc.Resources = variables.data();
  resource_signature_desc.NumResources = variables.size();
  resource_signature_desc.ImmutableSamplers = samplers.data();
  resource_signature_desc.NumImmutableSamplers = samplers.size();
  resource_signature_desc.BindingIndex = binding_index;
  resource_signature_desc.UseCombinedTextureSamplers = Diligent::True;

  RRefPtr<Diligent::IPipelineResourceSignature> signature;
  device_->CreatePipelineResourceSignature(resource_signature_desc, &signature);
  return signature;
}

Pipeline_Base::Pipeline_Base(const PipelineInitParams& init_params)
    : RenderPipelineBase(init_params.device) {
  const ShaderSource shader_source{kHLSL_BaseRender_Vertex,
                                   kHLSL_BaseRender_Pixel, "base.render"};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_VERTEX, "WorldMatrixBuffer",
       Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_Texture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  const std::vector<Diligent::ImmutableSamplerDesc> samplers = {
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_Texture",
          init_params.sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  BuildPipeline(shader_source, Vertex::GetLayout(), {binding0},
                init_params.target_format, init_params.depth_stencil_format);
}

Pipeline_BitmapBlt::Pipeline_BitmapBlt(const PipelineInitParams& init_params)
    : RenderPipelineBase(init_params.device) {
  const ShaderSource shader_source{kHLSL_BitmapBltRender_Vertex,
                                   kHLSL_BitmapBltRender_Pixel,
                                   "bitmapblt.render"};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_VERTEX, "WorldMatrixBuffer",
       Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_Texture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_DstTexture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  const std::vector<Diligent::ImmutableSamplerDesc> samplers = {
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_Texture",
          init_params.sampler,
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_DstTexture",
          init_params.sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  BuildPipeline(shader_source, Vertex::GetLayout(), {binding0},
                init_params.target_format, init_params.depth_stencil_format);
}

Pipeline_Color::Pipeline_Color(const PipelineInitParams& init_params)
    : RenderPipelineBase(init_params.device) {
  const ShaderSource shader_source{kHLSL_ColorRender_Vertex,
                                   kHLSL_ColorRender_Pixel, "color.render"};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_VERTEX, "WorldMatrixBuffer",
       Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  auto binding0 = MakeResourceSignature(variables, {}, 0);
  BuildPipeline(shader_source, Vertex::GetLayout(), {binding0},
                init_params.target_format, init_params.depth_stencil_format);
}

Pipeline_Flat::Pipeline_Flat(const PipelineInitParams& init_params)
    : RenderPipelineBase(init_params.device) {
  const ShaderSource shader_source{kHLSL_FlatRender_Vertex,
                                   kHLSL_FlatRender_Pixel, "flat.render"};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_VERTEX, "WorldMatrixBuffer",
       Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_Texture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "FlatUniformConstants",
       Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  const std::vector<Diligent::ImmutableSamplerDesc> samplers = {
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_Texture",
          init_params.sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  BuildPipeline(shader_source, Vertex::GetLayout(), {binding0},
                init_params.target_format, init_params.depth_stencil_format);
}

Pipeline_Sprite::Pipeline_Sprite(const PipelineInitParams& init_params)
    : RenderPipelineBase(init_params.device) {
  const auto& device_info = init_params.device->GetDeviceInfo();

  if (device_info.Type == Diligent::RENDER_DEVICE_TYPE_GLES) {
    // Disable batch on any OGLES platform
    storage_buffer_support = false;
  } else if (device_info.Type == Diligent::RENDER_DEVICE_TYPE_GL) {
    // Enable batch on available OGL desktop platform
    storage_buffer_support =
        device_info.Features.VertexPipelineUAVWritesAndAtomics > 0;
  } else {
    // Enable batch on D3D11, D3D12 and Vulkan backend
    storage_buffer_support = true;
  }

  if (!storage_buffer_support)
    LOG(INFO) << "[Pipeline] Disable sprite batch process.";

  Diligent::ShaderMacro vertex_macro = {"STORAGE_BUFFER_SUPPORT",
                                        storage_buffer_support ? "1" : "0"};

  const ShaderSource shader_source{kHLSL_SpriteRender_Vertex,
                                   kHLSL_SpriteRender_Pixel,
                                   "sprite.render",
                                   {vertex_macro}};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_VERTEX, "WorldMatrixBuffer",
       Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_Texture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_VERTEX,
       storage_buffer_support ? "u_Params" : "SpriteUniformConstants",
       storage_buffer_support ? Diligent::SHADER_RESOURCE_TYPE_BUFFER_SRV
                              : Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  const std::vector<Diligent::ImmutableSamplerDesc> samplers = {
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_Texture",
          init_params.sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  BuildPipeline(shader_source, Vertex::GetLayout(), {binding0},
                init_params.target_format, init_params.depth_stencil_format);
}

Pipeline_AlphaTransition::Pipeline_AlphaTransition(
    const PipelineInitParams& init_params)
    : RenderPipelineBase(init_params.device) {
  const ShaderSource shader_source{kHLSL_AlphaTransitionRender_Vertex,
                                   kHLSL_AlphaTransitionRender_Pixel,
                                   "alpha.trans.render"};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_PIXEL, "u_FrozenTexture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_CurrentTexture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  const std::vector<Diligent::ImmutableSamplerDesc> samplers = {
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_FrozenTexture",
          init_params.sampler,
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_CurrentTexture",
          init_params.sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  BuildPipeline(shader_source, Vertex::GetLayout(), {binding0},
                init_params.target_format, init_params.depth_stencil_format);
}

Pipeline_VagueTransition::Pipeline_VagueTransition(
    const PipelineInitParams& init_params)
    : RenderPipelineBase(init_params.device) {
  const ShaderSource shader_source{kHLSL_MappingTransitionRender_Vertex,
                                   kHLSL_MappingTransitionRender_Pixel,
                                   "vague.trans.render"};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_PIXEL, "u_FrozenTexture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_CurrentTexture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_TransTexture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  const std::vector<Diligent::ImmutableSamplerDesc> samplers = {
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_FrozenTexture",
          init_params.sampler,
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_CurrentTexture",
          init_params.sampler,
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_TransTexture",
          init_params.sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  BuildPipeline(shader_source, Vertex::GetLayout(), {binding0},
                init_params.target_format, init_params.depth_stencil_format);
}

Pipeline_Tilemap::Pipeline_Tilemap(const PipelineInitParams& init_params)
    : RenderPipelineBase(init_params.device) {
  const ShaderSource shader_source{kHLSL_TilemapRender_Vertex,
                                   kHLSL_TilemapRender_Pixel, "tilemap.render"};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_VERTEX, "WorldMatrixBuffer",
       Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_Texture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_VERTEX, "TilemapUniformBuffer",
       Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  const std::vector<Diligent::ImmutableSamplerDesc> samplers = {
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_Texture",
          init_params.sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  BuildPipeline(shader_source, Vertex::GetLayout(), {binding0},
                init_params.target_format, init_params.depth_stencil_format);
}

Pipeline_Tilemap2::Pipeline_Tilemap2(const PipelineInitParams& init_params)
    : RenderPipelineBase(init_params.device) {
  const ShaderSource shader_source{kHLSL_Tilemap2Render_Vertex,
                                   kHLSL_Tilemap2Render_Pixel,
                                   "tilemap2.render"};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_VERTEX, "WorldMatrixBuffer",
       Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_Texture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_VERTEX, "Tilemap2UniformBuffer",
       Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  const std::vector<Diligent::ImmutableSamplerDesc> samplers = {
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_Texture",
          init_params.sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  BuildPipeline(shader_source, Vertex::GetLayout(), {binding0},
                init_params.target_format, init_params.depth_stencil_format);
}

Pipeline_BitmapHue::Pipeline_BitmapHue(const PipelineInitParams& init_params)
    : RenderPipelineBase(init_params.device) {
  const ShaderSource shader_source{kHLSL_BitmapHueRender_Vertex,
                                   kHLSL_BitmapHueRender_Pixel, "hue.render"};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_PIXEL, "u_Texture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  const std::vector<Diligent::ImmutableSamplerDesc> samplers = {
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_Texture",
          init_params.sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  BuildPipeline(shader_source, Vertex::GetLayout(), {binding0},
                init_params.target_format, init_params.depth_stencil_format);
}

Pipeline_Spine2D::Pipeline_Spine2D(const PipelineInitParams& init_params)
    : RenderPipelineBase(init_params.device) {
  const ShaderSource shader_source{kHLSL_Spine2DRender_Vertex,
                                   kHLSL_Spine2DRender_Pixel, "spine2d.render"};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_VERTEX, "WorldMatrixBuffer",
       Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_Texture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  const std::vector<Diligent::ImmutableSamplerDesc> samplers = {
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_Texture",
          init_params.sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  BuildPipeline(shader_source, SpineVertex::GetLayout(), {binding0},
                init_params.target_format, init_params.depth_stencil_format);
}

Pipeline_YUV::Pipeline_YUV(const PipelineInitParams& init_params)
    : RenderPipelineBase(init_params.device) {
  const ShaderSource shader_source{kHLSL_YUVRender_Vertex,
                                   kHLSL_YUVRender_Pixel, "yuv.render"};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_PIXEL, "u_TextureY",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_TextureU",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_TextureV",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  const std::vector<Diligent::ImmutableSamplerDesc> samplers = {
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_TextureY",
          init_params.sampler,
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_TextureU",
          init_params.sampler,
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_TextureV",
          init_params.sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  BuildPipeline(shader_source, Vertex::GetLayout(), {binding0},
                init_params.target_format, init_params.depth_stencil_format);
}

Pipeline_Present::Pipeline_Present(const PipelineInitParams& init_params,
                                   bool manual_srgb)
    : RenderPipelineBase(init_params.device) {
  std::vector<Diligent::ShaderMacro> pixel_macros = {
      {"GAMMA_TO_LINEAR(Gamma)", kShaderGAMMA2LINEAR},
      {"SRGBA_TO_LINEAR(col)", manual_srgb ? kShaderSRGBA2LINEAR : ""},
  };

  const ShaderSource shader_source{kHLSL_PresentRender_Vertex,
                                   kHLSL_PresentRender_Pixel, "present.render",
                                   pixel_macros};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_VERTEX, "WorldMatrixBuffer",
       Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_Texture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_Texture_sampler",
       Diligent::SHADER_RESOURCE_TYPE_SAMPLER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  auto binding0 = MakeResourceSignature(variables, {}, 0);
  BuildPipeline(shader_source, Vertex::GetLayout(), {binding0},
                init_params.target_format, init_params.depth_stencil_format);
}

}  // namespace renderer
