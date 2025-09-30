// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_PIPELINE_RENDER_PIPELINE_H_
#define RENDERER_PIPELINE_RENDER_PIPELINE_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/PipelineResourceSignature.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsTools/interface/GraphicsUtilities.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"

#include "renderer/layout/vertex_layout.h"
#include "renderer/pipeline/render_binding.h"
#include "renderer/renderer_config.h"

namespace renderer {

/// Pipeline create info
///
struct PipelineInitParams {
  Diligent::IRenderDevice* render_device;
  Diligent::SamplerDesc immutable_sampler;
};

/// Pipeline base manager
///
class RenderPipelineBase {
 public:
  struct ShaderSource {
    std::string vertex_shader;
    std::string pixel_shader;
    std::string name = "generic.shader";
    std::vector<Diligent::ShaderMacro> macros = {};
    std::string vertex_entry = "VSMain";
    std::string pixel_entry = "PSMain";
  };

  virtual ~RenderPipelineBase() = default;

  RenderPipelineBase(const RenderPipelineBase&) = default;
  RenderPipelineBase& operator=(const RenderPipelineBase&) = default;

  void BuildPipeline(
      Diligent::IPipelineState** output,
      const Diligent::BlendStateDesc& blend_state,
      const Diligent::RasterizerStateDesc& rasterizer_state,
      const Diligent::DepthStencilStateDesc& depth_stencil_state,
      Diligent::PRIMITIVE_TOPOLOGY topology,
      const std::vector<Diligent::TEXTURE_FORMAT>& target_formats,
      Diligent::TEXTURE_FORMAT depth_stencil_format,
      const Diligent::SampleDesc& sample_state);

  Diligent::IPipelineResourceSignature* GetSignatureAt(size_t index) const {
    return resource_signatures_[index];
  }

 protected:
  RenderPipelineBase(Diligent::IRenderDevice* device);

  void SetupPipelineBasis(
      const ShaderSource& shader_source,
      const std::vector<Diligent::LayoutElement>& input_layout,
      const std::vector<RRefPtr<Diligent::IPipelineResourceSignature>>&
          signatures);

  RRefPtr<Diligent::IPipelineResourceSignature> MakeResourceSignature(
      const std::vector<Diligent::PipelineResourceDesc>& variables,
      const std::vector<Diligent::ImmutableSamplerDesc>& samplers,
      uint8_t binding_index);

  template <typename Ty>
  Ty CreateBindingAt(size_t index) {
    RRefPtr<Diligent::IShaderResourceBinding> binding;
    resource_signatures_[index]->CreateShaderResourceBinding(&binding);
    return RenderBindingBase::Create<Ty>(binding);
  }

 private:
  RRefPtr<Diligent::IRenderDevice> device_;

  std::string pipeline_name_;
  RRefPtr<Diligent::IShader> vertex_shader_object_, pixel_shader_object_;
  std::vector<Diligent::LayoutElement> input_layout_;
  std::vector<RRefPtr<Diligent::IPipelineResourceSignature>>
      resource_signatures_;
};

/// Internal pipeline set
///

#define PIPELINE_DEFINE(name, ...)                          \
  class Pipeline_##name : public RenderPipelineBase {       \
   public:                                                  \
    Pipeline_##name(const PipelineInitParams& init_params); \
    __VA_ARGS__                                             \
  };
#define MAKE_BINDING_FUNCTION(ty, x) \
  ty CreateBinding() {               \
    return CreateBindingAt<ty>(x);   \
  }

PIPELINE_DEFINE(Base, MAKE_BINDING_FUNCTION(Binding_Base, 0););
PIPELINE_DEFINE(BitmapBlt, MAKE_BINDING_FUNCTION(Binding_BitmapBlt, 0););
PIPELINE_DEFINE(BitmapClipBlt,
                MAKE_BINDING_FUNCTION(Binding_BitmapClipBlt, 0););
PIPELINE_DEFINE(BitmapHue, MAKE_BINDING_FUNCTION(Binding_BitmapFilter, 0););
PIPELINE_DEFINE(Color, MAKE_BINDING_FUNCTION(Binding_Color, 0););
PIPELINE_DEFINE(Flat, MAKE_BINDING_FUNCTION(Binding_Flat, 0););
PIPELINE_DEFINE(Sprite, MAKE_BINDING_FUNCTION(Binding_Sprite, 0);
                bool storage_buffer_support = false;);
PIPELINE_DEFINE(AlphaTransition, MAKE_BINDING_FUNCTION(Binding_AlphaTrans, 0););
PIPELINE_DEFINE(VagueTransition, MAKE_BINDING_FUNCTION(Binding_VagueTrans, 0););
PIPELINE_DEFINE(Tilemap, MAKE_BINDING_FUNCTION(Binding_Tilemap, 0););
PIPELINE_DEFINE(Tilemap2, MAKE_BINDING_FUNCTION(Binding_Tilemap2, 0););
PIPELINE_DEFINE(YUV, MAKE_BINDING_FUNCTION(Binding_YUV, 0););

#undef PIPELINE_DEFINE
#undef MAKE_BINDING_FUNCTION

/// Internal pipeline set
///

struct PipelineSet {
  Pipeline_Base base;
  Pipeline_BitmapBlt bitmapblt;
  Pipeline_BitmapClipBlt bitmapclipblt;
  Pipeline_Color color;
  Pipeline_Flat viewport;
  Pipeline_Sprite sprite;
  Pipeline_AlphaTransition alphatrans;
  Pipeline_VagueTransition mappedtrans;
  Pipeline_Tilemap tilemap;
  Pipeline_Tilemap2 tilemap2;
  Pipeline_BitmapHue bitmaphue;
  Pipeline_YUV yuv;

  PipelineSet(const PipelineInitParams& init_params)
      : base(init_params),
        bitmapblt(init_params),
        bitmapclipblt(init_params),
        color(init_params),
        viewport(init_params),
        sprite(init_params),
        alphatrans(init_params),
        mappedtrans(init_params),
        tilemap(init_params),
        tilemap2(init_params),
        bitmaphue(init_params),
        yuv(init_params) {}
};

}  // namespace renderer

#endif  //! RENDERER_PIPELINE_RENDER_PIPELINE_H_
