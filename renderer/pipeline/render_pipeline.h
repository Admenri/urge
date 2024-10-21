// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_PIPELINE_RENDER_PIPELINE_H_
#define RENDERER_PIPELINE_RENDER_PIPELINE_H_

#include "BasicMath.hpp"
#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsTools/interface/GraphicsUtilities.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"

#include "renderer/drawable/vertex_layout.h"

#include <functional>
#include <unordered_map>

namespace renderer {

using namespace Diligent;

enum class BlendType {
  NoBlend = -2,
  KeepDestAlpha = -1,
  Normal = 0,
  Addition = 1,
  Substraction = 2,
};

using ShaderCreateParams = struct {
  std::string source;
  std::string name;
  std::string entry;
};

using PipelineState = struct {
  RefCntAutoPtr<IPipelineState> pso;
  RefCntAutoPtr<IShaderResourceBinding> srb;
};

class RenderPipelineBase {
 public:
  RenderPipelineBase(RefCntAutoPtr<IRenderDevice> device);

  RenderPipelineBase(const RenderPipelineBase&) = delete;
  RenderPipelineBase& operator=(const RenderPipelineBase&) = delete;

  PipelineState* GetPSOFor(BlendType color_blend);

 protected:
  using StaticVariableInfo = struct {
    std::string name;
    SHADER_TYPE type;
    RefCntAutoPtr<IDeviceObject> var;
  };

  using StaticVariablesBlockFunc = std::function<void(IPipelineState*)>;
  void BuildGraphicsPipeline(
      const ShaderCreateParams& vs,
      const ShaderCreateParams& ps,
      const std::vector<LayoutElement>& layout,
      const std::vector<ShaderResourceVariableDesc>& variables,
      const std::vector<ImmutableSamplerDesc>& samplers,
      StaticVariablesBlockFunc set_static_variables_func,
      const std::string& pipeline_name,
      TEXTURE_FORMAT target_format);

  PipelineState* CurrentState() { return current_state_; }

 private:
  RefCntAutoPtr<IRenderDevice> device_;
  std::unordered_map<BlendType, PipelineState> pipeline_;
  PipelineState* current_state_;
};

class PipelineInstance_Base : public RenderPipelineBase {
 public:
  using VSUniform = struct {
    float projMat[16];
    base::Vec2 transOffset;
    base::Vec2 texSize;
  };

  PipelineInstance_Base(RefCntAutoPtr<IRenderDevice> device,
                        TEXTURE_FORMAT texfmt);

  RefCntAutoPtr<IBuffer> GetVSUniform();

  void SetTexture(ITextureView* view);

 private:
  RefCntAutoPtr<IBuffer> vs_uniform_;
};

class PipelineInstance_Blt : public RenderPipelineBase {
 public:
  using VSUniform = struct {
    float projMat[16];
    base::Vec2 transOffset;
    base::Vec2 texSize;
  };

  using PSUniform = struct {
    base::Vec4 offsetScale;
    float opacity;
  };

  PipelineInstance_Blt(RefCntAutoPtr<IRenderDevice> device,
                       TEXTURE_FORMAT texfmt);

  RefCntAutoPtr<IBuffer> GetVSUniform();
  RefCntAutoPtr<IBuffer> GetPSUniform();

  void SetTexture(ITextureView* view);
  void SetDstTexture(ITextureView* view);

 private:
  RefCntAutoPtr<IBuffer> vs_uniform_;
  RefCntAutoPtr<IBuffer> ps_uniform_;
};

class PipelineInstance_Color : public RenderPipelineBase {
 public:
  using VSUniform = struct {
    float projMat[16];
    base::Vec2 transOffset;
  };

  PipelineInstance_Color(RefCntAutoPtr<IRenderDevice> device,
                         TEXTURE_FORMAT texfmt);

  RefCntAutoPtr<IBuffer> GetVSUniform();

 private:
  RefCntAutoPtr<IBuffer> vs_uniform_;
};

class PipelineInstance_Sprite : public RenderPipelineBase {
 public:
  using VSUniform = struct {
    float projMat[16];
    float transformMat[16];
    base::Vec2 texSize;
  };

  using PSUniform = struct {
    base::Vec2 texSize;
    base::Vec4 color;
    base::Vec4 tone;
    float opacity;
    float bushDepth;
    float bushOpacity;
  };

  PipelineInstance_Sprite(RefCntAutoPtr<IRenderDevice> device,
                          TEXTURE_FORMAT texfmt);

  RefCntAutoPtr<IBuffer> GetVSUniform();
  RefCntAutoPtr<IBuffer> GetPSUniform();

  void SetTexture(ITextureView* view);

 private:
  RefCntAutoPtr<IBuffer> vs_uniform_;
  RefCntAutoPtr<IBuffer> ps_uniform_;
};

class PipelineInstance_BaseSprite : public RenderPipelineBase {
 public:
  using VSUniform = struct {
    float projMat[16];
    float transformMat[16];
    base::Vec2 texSize;
  };

  PipelineInstance_BaseSprite(RefCntAutoPtr<IRenderDevice> device,
                              TEXTURE_FORMAT texfmt);

  RefCntAutoPtr<IBuffer> GetVSUniform();

  void SetTexture(ITextureView* view);

 private:
  RefCntAutoPtr<IBuffer> vs_uniform_;
};

class PipelineInstance_Viewport : public RenderPipelineBase {
 public:
  using VSUniform = struct {
    float projMat[16];
    base::Vec2 transOffset;
    base::Vec2 texSize;
  };

  using PSUniform = struct {
    base::Vec4 color;
    base::Vec4 tone;
    float opacity;
  };

  PipelineInstance_Viewport(RefCntAutoPtr<IRenderDevice> device,
                            TEXTURE_FORMAT texfmt);

  RefCntAutoPtr<IBuffer> GetVSUniform();
  RefCntAutoPtr<IBuffer> GetPSUniform();

  void SetTexture(ITextureView* view);

 private:
  RefCntAutoPtr<IBuffer> vs_uniform_;
  RefCntAutoPtr<IBuffer> ps_uniform_;
};

class PipelineInstance_AlphaTrans : public RenderPipelineBase {
 public:
  using VSUniform = struct {
    float projMat[16];
    base::Vec2 transOffset;
    base::Vec2 texSize;
  };

  using PSUniform = struct { float progress; };

  PipelineInstance_AlphaTrans(RefCntAutoPtr<IRenderDevice> device,
                              TEXTURE_FORMAT texfmt);

  RefCntAutoPtr<IBuffer> GetVSUniform();
  RefCntAutoPtr<IBuffer> GetPSUniform();

  void SetFrozenTexture(ITextureView* view);
  void SetCurrentTexture(ITextureView* view);

 private:
  RefCntAutoPtr<IBuffer> vs_uniform_;
  RefCntAutoPtr<IBuffer> ps_uniform_;
};

class PipelineInstance_VagueTrans : public RenderPipelineBase {
 public:
  using VSUniform = struct {
    float projMat[16];
    base::Vec2 transOffset;
    base::Vec2 texSize;
  };

  using PSUniform = struct {
    float progress;
    float vague;
  };

  PipelineInstance_VagueTrans(RefCntAutoPtr<IRenderDevice> device,
                              TEXTURE_FORMAT texfmt);

  RefCntAutoPtr<IBuffer> GetVSUniform();
  RefCntAutoPtr<IBuffer> GetPSUniform();

  void SetFrozenTexture(ITextureView* view);
  void SetCurrentTexture(ITextureView* view);
  void SetTransTexture(ITextureView* view);

 private:
  RefCntAutoPtr<IBuffer> vs_uniform_;
  RefCntAutoPtr<IBuffer> ps_uniform_;
};

class PipelineInstance_BaseAlpha : public RenderPipelineBase {
 public:
  using VSUniform = struct {
    float projMat[16];
    base::Vec2 transOffset;
    base::Vec2 texSize;
  };

  PipelineInstance_BaseAlpha(RefCntAutoPtr<IRenderDevice> device,
                             TEXTURE_FORMAT texfmt);

  RefCntAutoPtr<IBuffer> GetVSUniform();

  void SetTexture(ITextureView* view);

 private:
  RefCntAutoPtr<IBuffer> vs_uniform_;
};

class PipelineInstance_Tilemap : public RenderPipelineBase {
 public:
  using VSUniform = struct {
    float projMat[16];
    base::Vec2 transOffset;
    base::Vec2 texSize;
    float animateIndex;
    float tileSize;
  };

  PipelineInstance_Tilemap(RefCntAutoPtr<IRenderDevice> device,
                           TEXTURE_FORMAT texfmt);

  RefCntAutoPtr<IBuffer> GetVSUniform();

  void SetTexture(ITextureView* view);

 private:
  RefCntAutoPtr<IBuffer> vs_uniform_;
};

class PipelineInstance_Tilemap2 : public RenderPipelineBase {
 public:
  using VSUniform = struct {
    float projMat[16];
    base::Vec2 transOffset;
    base::Vec2 texSize;
    base::Vec2 autotileAnimationOffset;
    float tileSize;
  };

  PipelineInstance_Tilemap2(RefCntAutoPtr<IRenderDevice> device,
                            TEXTURE_FORMAT texfmt);

  RefCntAutoPtr<IBuffer> GetVSUniform();

  void SetTexture(ITextureView* view);

 private:
  RefCntAutoPtr<IBuffer> vs_uniform_;
};

}  // namespace renderer

#endif  //! RENDERER_PIPELINE_RENDER_PIPELINE_H_
