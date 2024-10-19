// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/pipeline/render_pipeline.h"

namespace renderer {

#include "renderer/hlsl/base_ps.hlsl.xxd"
#include "renderer/hlsl/base_vs.hlsl.xxd"
#include "renderer/hlsl/basecolor_ps.hlsl.xxd"
#include "renderer/hlsl/basecolor_vs.hlsl.xxd"
#include "renderer/hlsl/blt_ps.hlsl.xxd"

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

PipelineInstance_Base::PipelineInstance_Base(
    RefCntAutoPtr<IRenderDevice> device)
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

  SamplerDesc SamLinearClampDesc{FILTER_TYPE_LINEAR,    FILTER_TYPE_LINEAR,
                                 FILTER_TYPE_LINEAR,    TEXTURE_ADDRESS_CLAMP,
                                 TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};

  std::vector<ImmutableSamplerDesc> samplers = {
      {SHADER_TYPE_PIXEL, "u_Texture", SamLinearClampDesc},
  };

  CreateUniformBuffer(device, sizeof(VSUniform), "Base vs uniform buffer",
                      &vs_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), vars, samplers,
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
      },
      "base_pso");
}

RefCntAutoPtr<IBuffer> PipelineInstance_Base::GetVSUniform() {
  return vs_uniform_;
}

void PipelineInstance_Base::SetTexture(ITextureView* view) {
  RenderPipelineBase::CurrentState()
      ->srb->GetVariableByName(SHADER_TYPE_PIXEL, "u_Texture")
      ->Set(view);
}

PipelineInstance_Blt::PipelineInstance_Blt(RefCntAutoPtr<IRenderDevice> device)
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

  SamplerDesc SamLinearClampDesc{FILTER_TYPE_LINEAR,    FILTER_TYPE_LINEAR,
                                 FILTER_TYPE_LINEAR,    TEXTURE_ADDRESS_CLAMP,
                                 TEXTURE_ADDRESS_CLAMP, TEXTURE_ADDRESS_CLAMP};

  std::vector<ImmutableSamplerDesc> samplers = {
      {SHADER_TYPE_PIXEL, "u_Texture", SamLinearClampDesc},
      {SHADER_TYPE_PIXEL, "u_DstTexture", SamLinearClampDesc},
  };

  CreateUniformBuffer(device, sizeof(VSUniform), "Blt vs uniform buffer",
                      &vs_uniform_);
  CreateUniformBuffer(device, sizeof(PSUniform), "Blt ps uniform buffer",
                      &ps_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), vars, samplers,
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
        pso->GetStaticVariableByName(SHADER_TYPE_PIXEL, "PSConstants")
            ->Set(ps_uniform_);
      },
      "blt_pso");
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

PipelineInstance_Color::PipelineInstance_Color(
    RefCntAutoPtr<IRenderDevice> device)
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

  CreateUniformBuffer(device, sizeof(VSUniform), "Basecolor vs uniform buffer",
                      &vs_uniform_);

  RenderPipelineBase::BuildGraphicsPipeline(
      vs, ps, GeometryVertexLayout::GetLayout(), {}, {},
      [&](IPipelineState* pso) {
        pso->GetStaticVariableByName(SHADER_TYPE_VERTEX, "VSConstants")
            ->Set(vs_uniform_);
      },
      "basecolor_pso");
}

RefCntAutoPtr<IBuffer> PipelineInstance_Color::GetVSUniform() {
  return vs_uniform_;
}

}  // namespace renderer
