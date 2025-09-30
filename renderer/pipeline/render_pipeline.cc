// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/pipeline/render_pipeline.h"

#include "base/debug/logging.h"
#include "renderer/pipeline/builtin_hlsl.h"

namespace renderer {

RenderPipelineBase::RenderPipelineBase(Diligent::IRenderDevice* device)
    : device_(device) {}

void RenderPipelineBase::BuildPipeline(
    Diligent::IPipelineState** output,
    const Diligent::BlendStateDesc& blend_state,
    const Diligent::RasterizerStateDesc& rasterizer_state,
    const Diligent::DepthStencilStateDesc& depth_stencil_state,
    Diligent::PRIMITIVE_TOPOLOGY topology,
    const std::vector<Diligent::TEXTURE_FORMAT>& target_formats,
    Diligent::TEXTURE_FORMAT depth_stencil_format,
    const Diligent::SampleDesc& sample_state) {
  Diligent::GraphicsPipelineStateCreateInfo pipeline_state_desc;

  // Pipeline name
  pipeline_state_desc.PSODesc.Name = pipeline_name_.c_str();
  pipeline_state_desc.PSODesc.PipelineType = Diligent::PIPELINE_TYPE_GRAPHICS;

  // Shaders
  pipeline_state_desc.pVS = vertex_shader_object_;
  pipeline_state_desc.pPS = pixel_shader_object_;

  // Resource signature
  std::vector<Diligent::IPipelineResourceSignature*> signatures;
  for (auto& it : resource_signatures_)
    signatures.push_back(it.RawPtr());
  pipeline_state_desc.ResourceSignaturesCount = signatures.size();
  pipeline_state_desc.ppResourceSignatures = signatures.data();

  // Input assembly
  pipeline_state_desc.GraphicsPipeline.InputLayout.NumElements =
      input_layout_.size();
  pipeline_state_desc.GraphicsPipeline.InputLayout.LayoutElements =
      input_layout_.data();

  // User specific states
  pipeline_state_desc.GraphicsPipeline.BlendDesc = blend_state;
  pipeline_state_desc.GraphicsPipeline.RasterizerDesc = rasterizer_state;
  pipeline_state_desc.GraphicsPipeline.DepthStencilDesc = depth_stencil_state;

  // Primitive topology
  pipeline_state_desc.GraphicsPipeline.PrimitiveTopology = topology;

  // Render targets
  const size_t num_render_targets =
      std::min<size_t>(target_formats.size(), DILIGENT_MAX_RENDER_TARGETS);
  pipeline_state_desc.GraphicsPipeline.NumRenderTargets = target_formats.size();
  for (size_t i = 0; i < num_render_targets; ++i)
    pipeline_state_desc.GraphicsPipeline.RTVFormats[i] = target_formats[i];

  // DSV format
  pipeline_state_desc.GraphicsPipeline.DSVFormat = depth_stencil_format;

  // Multisample desc
  pipeline_state_desc.GraphicsPipeline.SmplDesc = sample_state;

  // Make pipeline
  device_->CreateGraphicsPipelineState(pipeline_state_desc, output);
}

void RenderPipelineBase::SetupPipelineBasis(
    const ShaderSource& shader_source,
    const std::vector<Diligent::LayoutElement>& input_layout,
    const std::vector<RRefPtr<Diligent::IPipelineResourceSignature>>&
        signatures) {
  // Make pipeline debug name
  pipeline_name_ = "internal.pipeline<" + shader_source.name + ">";

  // Make vertex shader and pixel shader
  Diligent::ShaderCreateInfo shader_desc;
  shader_desc.SourceLanguage = Diligent::SHADER_SOURCE_LANGUAGE_HLSL;
  shader_desc.Desc.UseCombinedTextureSamplers = Diligent::True;
  shader_desc.CompileFlags =
      Diligent::SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

  {  // Vertex shader
    shader_desc.Desc.ShaderType = Diligent::SHADER_TYPE_VERTEX;
    shader_desc.EntryPoint = shader_source.vertex_entry.c_str();
    shader_desc.Desc.Name = shader_source.name.c_str();
    shader_desc.Source = shader_source.vertex_shader.c_str();
    shader_desc.SourceLength = shader_source.vertex_shader.size();
    shader_desc.Macros.Count = shader_source.macros.size();
    shader_desc.Macros.Elements = shader_source.macros.data();
    device_->CreateShader(shader_desc, &vertex_shader_object_);
  }

  {  // Pixel shader
    shader_desc.Desc.ShaderType = Diligent::SHADER_TYPE_PIXEL;
    shader_desc.EntryPoint = shader_source.pixel_entry.c_str();
    shader_desc.Desc.Name = shader_source.name.c_str();
    shader_desc.Source = shader_source.pixel_shader.c_str();
    shader_desc.SourceLength = shader_source.pixel_shader.size();
    shader_desc.Macros.Count = shader_source.macros.size();
    shader_desc.Macros.Elements = shader_source.macros.data();
    device_->CreateShader(shader_desc, &pixel_shader_object_);
  }

  // Setup input attribute elements
  input_layout_ = input_layout;

  // Setup resource signature
  resource_signatures_ = signatures;
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

/// Pipeline Bake
///

#define PIPELINE_HEADER(name)                                             \
  Pipeline_##name::Pipeline_##name(const PipelineInitParams& init_params) \
      : RenderPipelineBase(init_params.render_device)

PIPELINE_HEADER(Base) {
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
          init_params.immutable_sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  SetupPipelineBasis(shader_source, Vertex::GetLayout(), {binding0});
}

PIPELINE_HEADER(BitmapBlt) {
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
          init_params.immutable_sampler,
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_DstTexture",
          init_params.immutable_sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  SetupPipelineBasis(shader_source, Vertex::GetLayout(), {binding0});
}

PIPELINE_HEADER(BitmapClipBlt) {
  const ShaderSource shader_source{kHLSL_BitmapClipBltRender_Vertex,
                                   kHLSL_BitmapClipBltRender_Pixel,
                                   "bitmapclipblt.render"};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_VERTEX, "WorldMatrixBuffer",
       Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_Texture",
       Diligent::SHADER_RESOURCE_TYPE_TEXTURE_SRV,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {Diligent::SHADER_TYPE_PIXEL, "u_ClipTexture",
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
          init_params.immutable_sampler,
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_ClipTexture",
          init_params.immutable_sampler,
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_DstTexture",
          init_params.immutable_sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  SetupPipelineBasis(shader_source, Vertex::GetLayout(), {binding0});
}

PIPELINE_HEADER(Color) {
  const ShaderSource shader_source{kHLSL_ColorRender_Vertex,
                                   kHLSL_ColorRender_Pixel, "color.render"};

  const std::vector<Diligent::PipelineResourceDesc> variables = {
      {Diligent::SHADER_TYPE_VERTEX, "WorldMatrixBuffer",
       Diligent::SHADER_RESOURCE_TYPE_CONSTANT_BUFFER,
       Diligent::SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  auto binding0 = MakeResourceSignature(variables, {}, 0);
  SetupPipelineBasis(shader_source, Vertex::GetLayout(), {binding0});
}

PIPELINE_HEADER(Flat) {
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
          init_params.immutable_sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  SetupPipelineBasis(shader_source, Vertex::GetLayout(), {binding0});
}

PIPELINE_HEADER(Sprite) {
  const auto& device_info = init_params.render_device->GetDeviceInfo();

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
          init_params.immutable_sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  SetupPipelineBasis(shader_source, Vertex::GetLayout(), {binding0});
}

PIPELINE_HEADER(AlphaTransition) {
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
          init_params.immutable_sampler,
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_CurrentTexture",
          init_params.immutable_sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  SetupPipelineBasis(shader_source, Vertex::GetLayout(), {binding0});
}

PIPELINE_HEADER(VagueTransition) {
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
          init_params.immutable_sampler,
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_CurrentTexture",
          init_params.immutable_sampler,
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_TransTexture",
          init_params.immutable_sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  SetupPipelineBasis(shader_source, Vertex::GetLayout(), {binding0});
}

PIPELINE_HEADER(Tilemap) {
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
          init_params.immutable_sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  SetupPipelineBasis(shader_source, Vertex::GetLayout(), {binding0});
}

Pipeline_Tilemap2::Pipeline_Tilemap2(const PipelineInitParams& init_params)
    : RenderPipelineBase(init_params.render_device) {
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
          init_params.immutable_sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  SetupPipelineBasis(shader_source, Vertex::GetLayout(), {binding0});
}

PIPELINE_HEADER(BitmapHue) {
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
          init_params.immutable_sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  SetupPipelineBasis(shader_source, Vertex::GetLayout(), {binding0});
}

PIPELINE_HEADER(YUV) {
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
          init_params.immutable_sampler,
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_TextureU",
          init_params.immutable_sampler,
      },
      {
          Diligent::SHADER_TYPE_PIXEL,
          "u_TextureV",
          init_params.immutable_sampler,
      },
  };

  auto binding0 = MakeResourceSignature(variables, samplers, 0);
  SetupPipelineBasis(shader_source, Vertex::GetLayout(), {binding0});
}

}  // namespace renderer
