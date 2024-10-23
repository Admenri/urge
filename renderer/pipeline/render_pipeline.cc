// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/pipeline/render_pipeline.h"

namespace renderer {

#include "renderer/hlsl/alphatrans_ps.hlsl.xxd"
#include "renderer/hlsl/base_ps.hlsl.xxd"
#include "renderer/hlsl/base_vs.hlsl.xxd"
#include "renderer/hlsl/basealpha_ps.hlsl.xxd"
#include "renderer/hlsl/basealpha_vs.hlsl.xxd"
#include "renderer/hlsl/basecolor_ps.hlsl.xxd"
#include "renderer/hlsl/basecolor_vs.hlsl.xxd"
#include "renderer/hlsl/blt_ps.hlsl.xxd"
#include "renderer/hlsl/flat_ps.hlsl.xxd"
#include "renderer/hlsl/sprite_ps.hlsl.xxd"
#include "renderer/hlsl/tilemap2_vs.hlsl.xxd"
#include "renderer/hlsl/tilemap_vs.hlsl.xxd"
#include "renderer/hlsl/transform_vs.hlsl.xxd"
#include "renderer/hlsl/vaguetrans_ps.hlsl.xxd"
#include "renderer/hlsl/viewport_ps.hlsl.xxd"

static void MakeColorBlend(BlendType type, RenderTargetBlendDesc& blend) {
  switch (type) {
    default:
    case BlendType::NoBlend:
      blend.BlendEnable = False;
      break;
    case BlendType::KeepDestAlpha:
      blend.BlendEnable = True;
      blend.BlendOp = BLEND_OPERATION_ADD;
      blend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
      blend.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
      blend.SrcBlendAlpha = BLEND_FACTOR_ZERO;
      blend.DestBlendAlpha = BLEND_FACTOR_ONE;
      break;
    case BlendType::Normal:
      blend.BlendEnable = True;
      blend.BlendOp = BLEND_OPERATION_ADD;
      blend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
      blend.DestBlend = BLEND_FACTOR_INV_SRC_ALPHA;
      blend.SrcBlendAlpha = BLEND_FACTOR_ONE;
      blend.DestBlendAlpha = BLEND_FACTOR_INV_SRC_ALPHA;
      break;
    case BlendType::Addition:
      blend.BlendEnable = True;
      blend.BlendOp = BLEND_OPERATION_ADD;
      blend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
      blend.DestBlend = BLEND_FACTOR_ONE;
      blend.SrcBlendAlpha = BLEND_FACTOR_ONE;
      blend.DestBlendAlpha = BLEND_FACTOR_ONE;
      break;
    case BlendType::Substraction:
      blend.BlendEnable = True;
      blend.BlendOp = BLEND_OPERATION_REV_SUBTRACT;
      blend.SrcBlend = BLEND_FACTOR_SRC_ALPHA;
      blend.DestBlend = BLEND_FACTOR_ONE;
      blend.SrcBlendAlpha = BLEND_FACTOR_ZERO;
      blend.DestBlendAlpha = BLEND_FACTOR_ONE;
      break;
  }
}

RenderPipelineBase::RenderPipelineBase(RefCntAutoPtr<IRenderDevice> device)
    : device_(device), current_state_(nullptr) {}

PipelineState* RenderPipelineBase::GetPSOFor(BlendType color_blend) {
  current_state_ = nullptr;

  auto iter = pipeline_.find(color_blend);
  if (iter != pipeline_.end())
    current_state_ = &iter->second;

  return current_state_;
}

void RenderPipelineBase::BuildGraphicsPipeline(
    const ShaderCreateParams& vs,
    const ShaderCreateParams& ps,
    const std::vector<LayoutElement>& layout,
    const std::vector<ShaderResourceVariableDesc>& variables,
    const std::vector<ImmutableSamplerDesc>& samplers,
    StaticVariablesBlockFunc set_static_variables_func,
    const std::string& pipeline_name,
    TEXTURE_FORMAT target_format) {
  GraphicsPipelineStateCreateInfo PSOCreateInfo;
  PSOCreateInfo.PSODesc.Name = pipeline_name.c_str();
  PSOCreateInfo.PSODesc.PipelineType = PIPELINE_TYPE_GRAPHICS;

  PSOCreateInfo.GraphicsPipeline.NumRenderTargets = 1;
  PSOCreateInfo.GraphicsPipeline.RTVFormats[0] = target_format;
  PSOCreateInfo.GraphicsPipeline.PrimitiveTopology =
      PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  PSOCreateInfo.GraphicsPipeline.RasterizerDesc.CullMode = CULL_MODE_NONE;
  PSOCreateInfo.GraphicsPipeline.RasterizerDesc.ScissorEnable = True;
  PSOCreateInfo.GraphicsPipeline.DepthStencilDesc.DepthEnable = False;

  ShaderCreateInfo ShaderCI;
  RefCntAutoPtr<IShader> pVS, pPS;
  ShaderCI.SourceLanguage = SHADER_SOURCE_LANGUAGE_HLSL;
  ShaderCI.Desc.UseCombinedTextureSamplers = true;
  ShaderCI.CompileFlags = SHADER_COMPILE_FLAG_PACK_MATRIX_ROW_MAJOR;

  {
    ShaderCI.Desc.ShaderType = SHADER_TYPE_VERTEX;
    ShaderCI.EntryPoint = vs.entry.c_str();
    ShaderCI.Desc.Name = vs.name.c_str();
    ShaderCI.Source = vs.source.c_str();
    device_->CreateShader(ShaderCI, &pVS);

    ShaderCI.Desc.ShaderType = SHADER_TYPE_PIXEL;
    ShaderCI.EntryPoint = ps.entry.c_str();
    ShaderCI.Desc.Name = ps.name.c_str();
    ShaderCI.Source = ps.source.c_str();
    device_->CreateShader(ShaderCI, &pPS);
  }

  PSOCreateInfo.pVS = pVS;
  PSOCreateInfo.pPS = pPS;

  PSOCreateInfo.GraphicsPipeline.InputLayout.LayoutElements = layout.data();
  PSOCreateInfo.GraphicsPipeline.InputLayout.NumElements = layout.size();

  PSOCreateInfo.PSODesc.ResourceLayout.Variables = variables.data();
  PSOCreateInfo.PSODesc.ResourceLayout.NumVariables = variables.size();

  PSOCreateInfo.PSODesc.ResourceLayout.ImmutableSamplers = samplers.data();
  PSOCreateInfo.PSODesc.ResourceLayout.NumImmutableSamplers = samplers.size();

  for (int i = static_cast<int>(BlendType::NoBlend);
       i <= static_cast<int>(BlendType::Substraction); ++i) {
    auto& target_blend =
        PSOCreateInfo.GraphicsPipeline.BlendDesc.RenderTargets[0];
    MakeColorBlend((BlendType)i, target_blend);

    RefCntAutoPtr<IPipelineState> pso;
    device_->CreateGraphicsPipelineState(PSOCreateInfo, &pso);

    if (set_static_variables_func)
      set_static_variables_func(pso.RawPtr());

    RefCntAutoPtr<IShaderResourceBinding> srb;
    pso->CreateShaderResourceBinding(&srb, True);

    pipeline_.insert(
        std::make_pair(static_cast<BlendType>(i), PipelineState{pso, srb}));
  }
}

/// <summary>
/// Base
/// </summary>
/// <param name="device"></param>

PipelineInstance_Base::PipelineInstance_Base(
    RefCntAutoPtr<IRenderDevice> device,
    TEXTURE_FORMAT texfmt)
    : RenderPipelineBase(device) {
  ShaderCreateParams vs, ps;
  vs.source = std::string((const char*)base_vs_hlsl, base_vs_hlsl_len);
  vs.name = "base_vs";
  vs.entry = "main";

  ps.source = std::string((const char*)base_ps_hlsl, base_ps_hlsl_len);
  ps.name = "base_ps";
  ps.entry = "main";

  std::vector<ShaderResourceVariableDesc> vars = {
      {SHADER_TYPE_PIXEL, "u_Texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  SamplerDesc SamLinearClampDesc{FILTER_TYPE_POINT,     FILTER_TYPE_POINT,
                                 FILTER_TYPE_POINT,     TEXTURE_ADDRESS_CLAMP,
                                 TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};

  std::vector<ImmutableSamplerDesc> samplers = {
      {SHADER_TYPE_PIXEL, "u_Texture", SamLinearClampDesc},
  };

  CreateUniformBuffer(device, sizeof(VSUniform), "base.vs.ubo", &vs_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), vars, samplers,
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
      },
      "base.pso", texfmt);
}

RefCntAutoPtr<IBuffer> PipelineInstance_Base::GetVSUniform() {
  return vs_uniform_;
}

void PipelineInstance_Base::SetTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_Texture")
      ->Set(view);
}

/// <summary>
/// Blt
/// </summary>
/// <param name="device"></param>

PipelineInstance_Blt::PipelineInstance_Blt(RefCntAutoPtr<IRenderDevice> device,
                                           TEXTURE_FORMAT texfmt)
    : RenderPipelineBase(device) {
  ShaderCreateParams vs, ps;
  vs.source = std::string((const char*)base_vs_hlsl, base_vs_hlsl_len);
  vs.name = "base_vs";
  vs.entry = "main";

  ps.source = std::string((const char*)blt_ps_hlsl, blt_ps_hlsl_len);
  ps.name = "blt_ps";
  ps.entry = "main";

  std::vector<ShaderResourceVariableDesc> vars = {
      {SHADER_TYPE_PIXEL, "u_Texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_PIXEL, "u_DstTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  SamplerDesc SamLinearClampDesc{FILTER_TYPE_POINT,     FILTER_TYPE_POINT,
                                 FILTER_TYPE_POINT,     TEXTURE_ADDRESS_CLAMP,
                                 TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};

  std::vector<ImmutableSamplerDesc> samplers = {
      {SHADER_TYPE_PIXEL, "u_Texture", SamLinearClampDesc},
      {SHADER_TYPE_PIXEL, "u_DstTexture", SamLinearClampDesc},
  };

  CreateUniformBuffer(device, sizeof(VSUniform), "blt.vs.ubo", &vs_uniform_);
  CreateUniformBuffer(device, sizeof(PSUniform), "blt.ps.ubo", &ps_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), vars, samplers,
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
        pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PSConstants")
            ->Set(ps_uniform_);
      },
      "blt.pso", texfmt);
}

RefCntAutoPtr<IBuffer> PipelineInstance_Blt::GetVSUniform() {
  return vs_uniform_;
}

RefCntAutoPtr<IBuffer> PipelineInstance_Blt::GetPSUniform() {
  return ps_uniform_;
}

void PipelineInstance_Blt::SetTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_Texture")
      ->Set(view);
}

void PipelineInstance_Blt::SetDstTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_DstTexture")
      ->Set(view);
}

/// <summary>
/// Vertex Color
/// </summary>
/// <param name="device"></param>

PipelineInstance_Color::PipelineInstance_Color(
    RefCntAutoPtr<IRenderDevice> device,
    TEXTURE_FORMAT texfmt)
    : RenderPipelineBase(device) {
  ShaderCreateParams vs, ps;
  vs.source =
      std::string((const char*)basecolor_vs_hlsl, basecolor_vs_hlsl_len);
  vs.name = "basecolor_vs";
  vs.entry = "main";

  ps.source =
      std::string((const char*)basecolor_ps_hlsl, basecolor_ps_hlsl_len);
  ps.name = "basecolor_ps";
  ps.entry = "main";

  CreateUniformBuffer(device, sizeof(VSUniform), "basecolor.vs.ubo",
                      &vs_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), {}, {},
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
      },
      "basecolor.pso", texfmt);
}

RefCntAutoPtr<IBuffer> PipelineInstance_Color::GetVSUniform() {
  return vs_uniform_;
}

/// <summary>
/// Sprite
/// </summary>
/// <param name="device"></param>

PipelineInstance_Sprite::PipelineInstance_Sprite(
    RefCntAutoPtr<IRenderDevice> device,
    TEXTURE_FORMAT texfmt)
    : RenderPipelineBase(device) {
  ShaderCreateParams vs, ps;
  vs.source =
      std::string((const char*)transform_vs_hlsl, transform_vs_hlsl_len);
  vs.name = "transform_vs";
  vs.entry = "main";

  ps.source = std::string((const char*)sprite_ps_hlsl, sprite_ps_hlsl_len);
  ps.name = "sprite_ps";
  ps.entry = "main";

  std::vector<ShaderResourceVariableDesc> vars = {
      {SHADER_TYPE_PIXEL, "u_Texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  SamplerDesc SamLinearClampDesc{FILTER_TYPE_POINT,     FILTER_TYPE_POINT,
                                 FILTER_TYPE_POINT,     TEXTURE_ADDRESS_CLAMP,
                                 TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};

  std::vector<ImmutableSamplerDesc> samplers = {
      {SHADER_TYPE_PIXEL, "u_Texture", SamLinearClampDesc},
  };

  CreateUniformBuffer(device, sizeof(VSUniform), "sprite.vs.ubo", &vs_uniform_);
  CreateUniformBuffer(device, sizeof(PSUniform), "sprite.ps.ubo", &ps_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), vars, samplers,
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
        pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PSConstants")
            ->Set(ps_uniform_);
      },
      "sprite.pso", texfmt);
}

RefCntAutoPtr<IBuffer> PipelineInstance_Sprite::GetVSUniform() {
  return vs_uniform_;
}

RefCntAutoPtr<IBuffer> PipelineInstance_Sprite::GetPSUniform() {
  return ps_uniform_;
}

void PipelineInstance_Sprite::SetTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_Texture")
      ->Set(view);
}

/// <summary>
/// Base Sprite
/// </summary>
/// <param name="device"></param>

PipelineInstance_BaseSprite::PipelineInstance_BaseSprite(
    RefCntAutoPtr<IRenderDevice> device,
    TEXTURE_FORMAT texfmt)
    : RenderPipelineBase(device) {
  ShaderCreateParams vs, ps;
  vs.source =
      std::string((const char*)transform_vs_hlsl, transform_vs_hlsl_len);
  vs.name = "transform_vs";
  vs.entry = "main";

  ps.source = std::string((const char*)base_ps_hlsl, base_ps_hlsl_len);
  ps.name = "base_ps";
  ps.entry = "main";

  std::vector<ShaderResourceVariableDesc> vars = {
      {SHADER_TYPE_PIXEL, "u_Texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  SamplerDesc SamLinearClampDesc{FILTER_TYPE_POINT,     FILTER_TYPE_POINT,
                                 FILTER_TYPE_POINT,     TEXTURE_ADDRESS_CLAMP,
                                 TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};

  std::vector<ImmutableSamplerDesc> samplers = {
      {SHADER_TYPE_PIXEL, "u_Texture", SamLinearClampDesc},
  };

  CreateUniformBuffer(device, sizeof(VSUniform), "basesprite.vs.ubo",
                      &vs_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), vars, samplers,
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
      },
      "basesprite.pso", texfmt);
}

RefCntAutoPtr<IBuffer> PipelineInstance_BaseSprite::GetVSUniform() {
  return vs_uniform_;
}

void PipelineInstance_BaseSprite::SetTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_Texture")
      ->Set(view);
}

/// <summary>
/// Viewport
/// </summary>
/// <param name="device"></param>

PipelineInstance_Viewport::PipelineInstance_Viewport(
    RefCntAutoPtr<IRenderDevice> device,
    TEXTURE_FORMAT texfmt)
    : RenderPipelineBase(device) {
  ShaderCreateParams vs, ps;
  vs.source = std::string((const char*)base_vs_hlsl, base_vs_hlsl_len);
  vs.name = "base_vs";
  vs.entry = "main";

  ps.source = std::string((const char*)viewport_ps_hlsl, viewport_ps_hlsl_len);
  ps.name = "viewport_ps";
  ps.entry = "main";

  std::vector<ShaderResourceVariableDesc> vars = {
      {SHADER_TYPE_PIXEL, "u_Texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  SamplerDesc SamLinearClampDesc{FILTER_TYPE_POINT,     FILTER_TYPE_POINT,
                                 FILTER_TYPE_POINT,     TEXTURE_ADDRESS_CLAMP,
                                 TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};

  std::vector<ImmutableSamplerDesc> samplers = {
      {SHADER_TYPE_PIXEL, "u_Texture", SamLinearClampDesc},
  };

  CreateUniformBuffer(device, sizeof(VSUniform), "viewport.vs.ubo",
                      &vs_uniform_);
  CreateUniformBuffer(device, sizeof(PSUniform), "viewport.ps.ubo",
                      &ps_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), vars, samplers,
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
        pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PSConstants")
            ->Set(ps_uniform_);
      },
      "viewport.pso", texfmt);
}

RefCntAutoPtr<IBuffer> PipelineInstance_Viewport::GetVSUniform() {
  return vs_uniform_;
}

RefCntAutoPtr<IBuffer> PipelineInstance_Viewport::GetPSUniform() {
  return ps_uniform_;
}

void PipelineInstance_Viewport::SetTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_Texture")
      ->Set(view);
}

/// <summary>
/// Alpha Transition
/// </summary>
/// <param name="device"></param>
/// <param name="texfmt"></param>

PipelineInstance_AlphaTrans::PipelineInstance_AlphaTrans(
    RefCntAutoPtr<IRenderDevice> device,
    TEXTURE_FORMAT texfmt)
    : RenderPipelineBase(device) {
  ShaderCreateParams vs, ps;
  vs.source = std::string((const char*)base_vs_hlsl, base_vs_hlsl_len);
  vs.name = "base_vs";
  vs.entry = "main";

  ps.source =
      std::string((const char*)alphatrans_ps_hlsl, alphatrans_ps_hlsl_len);
  ps.name = "alphatrans_ps";
  ps.entry = "main";

  std::vector<ShaderResourceVariableDesc> vars = {
      {SHADER_TYPE_PIXEL, "u_FrozenTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_PIXEL, "u_CurrentTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  SamplerDesc SamLinearClampDesc{FILTER_TYPE_POINT,     FILTER_TYPE_POINT,
                                 FILTER_TYPE_POINT,     TEXTURE_ADDRESS_CLAMP,
                                 TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};

  std::vector<ImmutableSamplerDesc> samplers = {
      {SHADER_TYPE_PIXEL, "u_FrozenTexture", SamLinearClampDesc},
      {SHADER_TYPE_PIXEL, "u_CurrentTexture", SamLinearClampDesc},
  };

  CreateUniformBuffer(device, sizeof(VSUniform), "alphatrans.vs.ubo",
                      &vs_uniform_);
  CreateUniformBuffer(device, sizeof(PSUniform), "alphatrans.ps.ubo",
                      &ps_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), vars, samplers,
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
        pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PSConstants")
            ->Set(ps_uniform_);
      },
      "alphatrans.pso", texfmt);
}

RefCntAutoPtr<IBuffer> PipelineInstance_AlphaTrans::GetVSUniform() {
  return vs_uniform_;
}

RefCntAutoPtr<IBuffer> PipelineInstance_AlphaTrans::GetPSUniform() {
  return ps_uniform_;
}

void PipelineInstance_AlphaTrans::SetFrozenTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_FrozenTexture")
      ->Set(view);
}

void PipelineInstance_AlphaTrans::SetCurrentTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_CurrentTexture")
      ->Set(view);
}

/// <summary>
/// Vague Transition
/// </summary>
/// <param name="device"></param>
/// <param name="texfmt"></param>

PipelineInstance_VagueTrans::PipelineInstance_VagueTrans(
    RefCntAutoPtr<IRenderDevice> device,
    TEXTURE_FORMAT texfmt)
    : RenderPipelineBase(device) {
  ShaderCreateParams vs, ps;
  vs.source = std::string((const char*)base_vs_hlsl, base_vs_hlsl_len);
  vs.name = "base_vs";
  vs.entry = "main";

  ps.source =
      std::string((const char*)vaguetrans_ps_hlsl, vaguetrans_ps_hlsl_len);
  ps.name = "vaguetrans_ps";
  ps.entry = "main";

  std::vector<ShaderResourceVariableDesc> vars = {
      {SHADER_TYPE_PIXEL, "u_FrozenTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_PIXEL, "u_CurrentTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
      {SHADER_TYPE_PIXEL, "u_TransTexture",
       SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  SamplerDesc SamLinearClampDesc{FILTER_TYPE_POINT,     FILTER_TYPE_POINT,
                                 FILTER_TYPE_POINT,     TEXTURE_ADDRESS_CLAMP,
                                 TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};

  std::vector<ImmutableSamplerDesc> samplers = {
      {SHADER_TYPE_PIXEL, "u_FrozenTexture", SamLinearClampDesc},
      {SHADER_TYPE_PIXEL, "u_CurrentTexture", SamLinearClampDesc},
      {SHADER_TYPE_PIXEL, "u_TransTexture", SamLinearClampDesc},
  };

  CreateUniformBuffer(device, sizeof(VSUniform), "vaguetrans.vs.ubo",
                      &vs_uniform_);
  CreateUniformBuffer(device, sizeof(PSUniform), "vaguetrans.ps.ubo",
                      &ps_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), vars, samplers,
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
        pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PSConstants")
            ->Set(ps_uniform_);
      },
      "vaguetrans.pso", texfmt);
}

RefCntAutoPtr<IBuffer> PipelineInstance_VagueTrans::GetVSUniform() {
  return vs_uniform_;
}

RefCntAutoPtr<IBuffer> PipelineInstance_VagueTrans::GetPSUniform() {
  return ps_uniform_;
}

void PipelineInstance_VagueTrans::SetFrozenTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_FrozenTexture")
      ->Set(view);
}

void PipelineInstance_VagueTrans::SetCurrentTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_CurrentTexture")
      ->Set(view);
}

void PipelineInstance_VagueTrans::SetTransTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_TransTexture")
      ->Set(view);
}

/// <summary>
/// Base Alpha
/// </summary>
/// <param name="device"></param>
/// <param name="texfmt"></param>

PipelineInstance_BaseAlpha::PipelineInstance_BaseAlpha(
    RefCntAutoPtr<IRenderDevice> device,
    TEXTURE_FORMAT texfmt)
    : RenderPipelineBase(device) {
  ShaderCreateParams vs, ps;
  vs.source =
      std::string((const char*)basealpha_vs_hlsl, basealpha_vs_hlsl_len);
  vs.name = "basealpha_vs";
  vs.entry = "main";

  ps.source =
      std::string((const char*)basealpha_ps_hlsl, basealpha_ps_hlsl_len);
  ps.name = "basealpha_ps";
  ps.entry = "main";

  std::vector<ShaderResourceVariableDesc> vars = {
      {SHADER_TYPE_PIXEL, "u_Texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  SamplerDesc SamLinearClampDesc{FILTER_TYPE_POINT,     FILTER_TYPE_POINT,
                                 FILTER_TYPE_POINT,     TEXTURE_ADDRESS_CLAMP,
                                 TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};

  std::vector<ImmutableSamplerDesc> samplers = {
      {SHADER_TYPE_PIXEL, "u_Texture", SamLinearClampDesc},
  };

  CreateUniformBuffer(device, sizeof(VSUniform), "base.alpha.vs.ubo",
                      &vs_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), vars, samplers,
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
      },
      "basealpha.pso", texfmt);
}

RefCntAutoPtr<IBuffer> PipelineInstance_BaseAlpha::GetVSUniform() {
  return vs_uniform_;
}

void PipelineInstance_BaseAlpha::SetTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_Texture")
      ->Set(view);
}

/// <summary>
/// Tilemap
/// </summary>
/// <param name="device"></param>
/// <param name="texfmt"></param>

PipelineInstance_Tilemap::PipelineInstance_Tilemap(
    RefCntAutoPtr<IRenderDevice> device,
    TEXTURE_FORMAT texfmt)
    : RenderPipelineBase(device) {
  ShaderCreateParams vs, ps;
  vs.source = std::string((const char*)tilemap_vs_hlsl, tilemap_vs_hlsl_len);
  vs.name = "tilemap_vs";
  vs.entry = "main";

  ps.source = std::string((const char*)base_ps_hlsl, base_ps_hlsl_len);
  ps.name = "base_ps";
  ps.entry = "main";

  std::vector<ShaderResourceVariableDesc> vars = {
      {SHADER_TYPE_PIXEL, "u_Texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  SamplerDesc SamLinearClampDesc{FILTER_TYPE_POINT,     FILTER_TYPE_POINT,
                                 FILTER_TYPE_POINT,     TEXTURE_ADDRESS_CLAMP,
                                 TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};

  std::vector<ImmutableSamplerDesc> samplers = {
      {SHADER_TYPE_PIXEL, "u_Texture", SamLinearClampDesc},
  };

  CreateUniformBuffer(device, sizeof(VSUniform), "tilemap.vs.ubo",
                      &vs_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), vars, samplers,
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
      },
      "tilemap.pso", texfmt);
}

RefCntAutoPtr<IBuffer> PipelineInstance_Tilemap::GetVSUniform() {
  return vs_uniform_;
}

void PipelineInstance_Tilemap::SetTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_Texture")
      ->Set(view);
}

/// <summary>
/// Tilemap2
/// </summary>
/// <param name="device"></param>
/// <param name="texfmt"></param>

PipelineInstance_Tilemap2::PipelineInstance_Tilemap2(
    RefCntAutoPtr<IRenderDevice> device,
    TEXTURE_FORMAT texfmt)
    : RenderPipelineBase(device) {
  ShaderCreateParams vs, ps;
  vs.source = std::string((const char*)tilemap2_vs_hlsl, tilemap2_vs_hlsl_len);
  vs.name = "tilemap2_vs";
  vs.entry = "main";

  ps.source = std::string((const char*)base_ps_hlsl, base_ps_hlsl_len);
  ps.name = "base_ps";
  ps.entry = "main";

  std::vector<ShaderResourceVariableDesc> vars = {
      {SHADER_TYPE_PIXEL, "u_Texture", SHADER_RESOURCE_VARIABLE_TYPE_DYNAMIC},
  };

  SamplerDesc SamLinearClampDesc{FILTER_TYPE_POINT,     FILTER_TYPE_POINT,
                                 FILTER_TYPE_POINT,     TEXTURE_ADDRESS_CLAMP,
                                 TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};

  std::vector<ImmutableSamplerDesc> samplers = {
      {SHADER_TYPE_PIXEL, "u_Texture", SamLinearClampDesc},
  };

  CreateUniformBuffer(device, sizeof(VSUniform), "tilemap2.vs.ubo",
                      &vs_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), vars, samplers,
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
      },
      "tilemap2.pso", texfmt);
}

RefCntAutoPtr<IBuffer> PipelineInstance_Tilemap2::GetVSUniform() {
  return vs_uniform_;
}

void PipelineInstance_Tilemap2::SetTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_Texture")
      ->Set(view);
}

/// <summary>
/// Flat
/// </summary>
/// <param name="device"></param>
/// <param name="texfmt"></param>

PipelineInstance_Flat::PipelineInstance_Flat(
    RefCntAutoPtr<IRenderDevice> device,
    TEXTURE_FORMAT texfmt)
    : RenderPipelineBase(device) {
  ShaderCreateParams vs, ps;
  vs.source = std::string((const char*)base_vs_hlsl, base_vs_hlsl_len);
  vs.name = "base_vs";
  vs.entry = "main";

  ps.source = std::string((const char*)flat_ps_hlsl, flat_ps_hlsl_len);
  ps.name = "flat_ps";
  ps.entry = "main";

  CreateUniformBuffer(device, sizeof(VSUniform), "flat.vs.ubo", &vs_uniform_);
  CreateUniformBuffer(device, sizeof(VSUniform), "flat.ps.ubo", &ps_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), {}, {},
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
        pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PSConstants")
            ->Set(ps_uniform_);
      },
      "flat.pso", texfmt);
}

RefCntAutoPtr<IBuffer> PipelineInstance_Flat::GetVSUniform() {
  return vs_uniform_;
}

RefCntAutoPtr<IBuffer> PipelineInstance_Flat::GetPSUniform() {
  return ps_uniform_;
}

}  // namespace renderer
