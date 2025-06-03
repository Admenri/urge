// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_PIPELINE_RENDER_PIPELINE_H_
#define RENDERER_PIPELINE_RENDER_PIPELINE_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsTools/interface/GraphicsUtilities.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"

#include "renderer/layout/vertex_layout.h"
#include "renderer/pipeline/render_binding.h"

namespace renderer {

#define MAKE_BINDING_FUNCTION(ty, x)          \
  inline base::OwnedPtr<ty> CreateBinding() { \
    return CreateBindingAt<ty>(x);            \
  }

/// Color Blend Type of Pipeline
///
enum BlendType {
  NORMAL = 0,
  ADDITION,
  SUBSTRACTION,
  MULTIPLY,
  SCREEN,
  KEEP_ALPHA,

  NORMAL_PMA,
  ADDITION_PMA,

  NO_BLEND,

  TYPE_NUMS,
};

/// Pipeline base manager
///
class RenderPipelineBase {
 public:
  struct ShaderSource {
    base::String source;
    base::String name = "generic.shader";
    base::Vector<Diligent::ShaderMacro> macros = {};
    base::String vertex_entry = "VSMain";
    base::String pixel_entry = "PSMain";
  };

  virtual ~RenderPipelineBase() = default;

  RenderPipelineBase(const RenderPipelineBase&) = delete;
  RenderPipelineBase& operator=(const RenderPipelineBase&) = delete;

  inline Diligent::IPipelineState* GetPipeline(BlendType blend) const {
    return pipelines_[blend];
  }

  inline Diligent::IPipelineResourceSignature* GetSignatureAt(
      size_t index) const {
    return resource_signatures_[index];
  }

 protected:
  RenderPipelineBase(Diligent::IRenderDevice* device);

  void BuildPipeline(const ShaderSource& shader_source,
                     const base::Vector<Diligent::LayoutElement>& input_layout,
                     const base::Vector<Diligent::RefCntAutoPtr<
                         Diligent::IPipelineResourceSignature>>& signatures,
                     Diligent::TEXTURE_FORMAT target_format);

  Diligent::RefCntAutoPtr<Diligent::IPipelineResourceSignature>
  MakeResourceSignature(
      const base::Vector<Diligent::PipelineResourceDesc>& variables,
      const base::Vector<Diligent::ImmutableSamplerDesc>& samplers,
      uint8_t binding_index);

  template <typename Ty>
  base::OwnedPtr<Ty> CreateBindingAt(size_t index) {
    Diligent::RefCntAutoPtr<Diligent::IShaderResourceBinding> binding;
    resource_signatures_[index]->CreateShaderResourceBinding(&binding);
    return RenderBindingBase::Create<Ty>(binding);
  }

 private:
  Diligent::IRenderDevice* device_;
  base::Vector<Diligent::RefCntAutoPtr<Diligent::IPipelineResourceSignature>>
      resource_signatures_;
  base::Vector<Diligent::RefCntAutoPtr<Diligent::IPipelineState>> pipelines_;
};

class Pipeline_Base : public RenderPipelineBase {
 public:
  Pipeline_Base(Diligent::IRenderDevice* device,
                Diligent::TEXTURE_FORMAT target_format);

  MAKE_BINDING_FUNCTION(Binding_Base, 0);
};

class Pipeline_Color : public RenderPipelineBase {
 public:
  Pipeline_Color(Diligent::IRenderDevice* device,
                 Diligent::TEXTURE_FORMAT target_format);

  MAKE_BINDING_FUNCTION(Binding_Color, 0);
};

class Pipeline_Flat : public RenderPipelineBase {
 public:
  Pipeline_Flat(Diligent::IRenderDevice* device,
                Diligent::TEXTURE_FORMAT target_format);

  MAKE_BINDING_FUNCTION(Binding_Flat, 0);
};

class Pipeline_Sprite : public RenderPipelineBase {
 public:
  Pipeline_Sprite(Diligent::IRenderDevice* device,
                  Diligent::TEXTURE_FORMAT target_format);

  MAKE_BINDING_FUNCTION(Binding_Sprite, 0);
};

class Pipeline_AlphaTransition : public RenderPipelineBase {
 public:
  Pipeline_AlphaTransition(Diligent::IRenderDevice* device,
                           Diligent::TEXTURE_FORMAT target_format);

  MAKE_BINDING_FUNCTION(Binding_AlphaTrans, 0);
};

class Pipeline_VagueTransition : public RenderPipelineBase {
 public:
  Pipeline_VagueTransition(Diligent::IRenderDevice* device,
                           Diligent::TEXTURE_FORMAT target_format);

  MAKE_BINDING_FUNCTION(Binding_VagueTrans, 0);
};

class Pipeline_Tilemap : public RenderPipelineBase {
 public:
  Pipeline_Tilemap(Diligent::IRenderDevice* device,
                   Diligent::TEXTURE_FORMAT target_format);

  MAKE_BINDING_FUNCTION(Binding_Tilemap, 0);
};

class Pipeline_Tilemap2 : public RenderPipelineBase {
 public:
  Pipeline_Tilemap2(Diligent::IRenderDevice* device,
                    Diligent::TEXTURE_FORMAT target_format);

  MAKE_BINDING_FUNCTION(Binding_Tilemap2, 0);
};

class Pipeline_BitmapHue : public RenderPipelineBase {
 public:
  Pipeline_BitmapHue(Diligent::IRenderDevice* device,
                     Diligent::TEXTURE_FORMAT target_format);

  MAKE_BINDING_FUNCTION(Binding_BitmapFilter, 0);
};

class Pipeline_Spine2D : public RenderPipelineBase {
 public:
  Pipeline_Spine2D(Diligent::IRenderDevice* device,
                   Diligent::TEXTURE_FORMAT target_format);

  MAKE_BINDING_FUNCTION(Binding_Base, 0);
};

class Pipeline_Present : public RenderPipelineBase {
 public:
  Pipeline_Present(Diligent::IRenderDevice* device,
                   Diligent::TEXTURE_FORMAT target_format,
                   bool manual_srgb);

  MAKE_BINDING_FUNCTION(Binding_Base, 0);
};

class Pipeline_YUV : public RenderPipelineBase {
 public:
  Pipeline_YUV(Diligent::IRenderDevice* device,
               Diligent::TEXTURE_FORMAT target_format);

  MAKE_BINDING_FUNCTION(Binding_YUV, 0);
};

}  // namespace renderer

#endif  //! RENDERER_PIPELINE_RENDER_PIPELINE_H_
