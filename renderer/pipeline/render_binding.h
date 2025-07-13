// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_PIPELINE_RENDER_BINDING_H_
#define RENDERER_PIPELINE_RENDER_BINDING_H_

#include "Common/interface/RefCntAutoPtr.hpp"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/PipelineState.h"
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsTools/interface/GraphicsUtilities.h"
#include "Graphics/GraphicsTools/interface/MapHelper.hpp"

#include "base/math/rectangle.h"
#include "base/math/vector.h"
#include "base/memory/allocator.h"
#include "renderer/renderer_config.h"

namespace renderer {

class RenderBindingBase {
 public:
  using ShaderBinding = Diligent::IShaderResourceBinding;
  using ShaderVariable = Diligent::IShaderResourceVariable;

  virtual ~RenderBindingBase() = default;

  RenderBindingBase(const RenderBindingBase&) = default;
  RenderBindingBase& operator=(const RenderBindingBase&) = default;

  template <typename Ty>
  static Ty Create(ShaderBinding* binding) {
    return Ty(binding);
  }

  bool operator()() const { return binding_; }

  Diligent::IShaderResourceBinding* operator->() const { return binding_; }
  Diligent::IShaderResourceBinding* operator*() const { return binding_; }

 protected:
  RenderBindingBase() = default;
  RenderBindingBase(ShaderBinding* binding);

 private:
  RRefPtr<ShaderBinding> binding_;
};

///
// Binding Define
///

class Binding_Base : public RenderBindingBase {
 public:
  Binding_Base() = default;

  RRefPtr<ShaderVariable> u_transform;
  RRefPtr<ShaderVariable> u_texture;

 private:
  friend class RenderBindingBase;
  Binding_Base(ShaderBinding* binding);
};

class Binding_BitmapBlt : public RenderBindingBase {
 public:
  Binding_BitmapBlt() = default;

  RRefPtr<ShaderVariable> u_transform;
  RRefPtr<ShaderVariable> u_texture;
  RRefPtr<ShaderVariable> u_dst_texture;

 private:
  friend class RenderBindingBase;
  Binding_BitmapBlt(ShaderBinding* binding);
};

class Binding_Color : public RenderBindingBase {
 public:
  Binding_Color() = default;

  RRefPtr<ShaderVariable> u_transform;

 private:
  friend class RenderBindingBase;
  Binding_Color(ShaderBinding* binding);
};

class Binding_Flat : public RenderBindingBase {
 public:
  struct Params {
    base::Vec4 Color;
    base::Vec4 Tone;
  };

  Binding_Flat() = default;

  RRefPtr<ShaderVariable> u_transform;
  RRefPtr<ShaderVariable> u_texture;
  RRefPtr<ShaderVariable> u_params;

 private:
  friend class RenderBindingBase;
  Binding_Flat(ShaderBinding* binding);
};

class Binding_Sprite : public RenderBindingBase {
 public:
  struct Params {
    base::Vec4 Color;
    base::Vec4 Tone;
    base::Vec4 Position;
    base::Vec4 Origin;
    base::Vec4 Scale;
    base::Vec4 Rotation;
    base::Vec4 Opacity;
    base::Vec4 BushDepthAndOpacity;
  };

  Binding_Sprite() = default;

  RRefPtr<ShaderVariable> u_transform;
  RRefPtr<ShaderVariable> u_params;
  RRefPtr<ShaderVariable> u_texture;

 private:
  friend class RenderBindingBase;
  Binding_Sprite(ShaderBinding* binding);
};

class Binding_AlphaTrans : public RenderBindingBase {
 public:
  Binding_AlphaTrans() = default;

  RRefPtr<ShaderVariable> u_frozen_texture;
  RRefPtr<ShaderVariable> u_current_texture;

 private:
  friend class RenderBindingBase;
  Binding_AlphaTrans(ShaderBinding* binding);
};

class Binding_VagueTrans : public RenderBindingBase {
 public:
  Binding_VagueTrans() = default;

  RRefPtr<ShaderVariable> u_frozen_texture;
  RRefPtr<ShaderVariable> u_current_texture;
  RRefPtr<ShaderVariable> u_trans_texture;

 private:
  friend class RenderBindingBase;
  Binding_VagueTrans(ShaderBinding* binding);
};

class Binding_Tilemap : public RenderBindingBase {
 public:
  struct Params {
    base::Vec4 OffsetAndTexSize;
    base::Vec4 AnimateIndexAndTileSize;
  };

  Binding_Tilemap() = default;

  RRefPtr<ShaderVariable> u_transform;
  RRefPtr<ShaderVariable> u_params;
  RRefPtr<ShaderVariable> u_texture;

 private:
  friend class RenderBindingBase;
  Binding_Tilemap(ShaderBinding* binding);
};

class Binding_Tilemap2 : public RenderBindingBase {
 public:
  struct Params {
    base::Vec4 OffsetAndTexSize;
    base::Vec4 AnimationOffsetAndTileSize;
  };

  Binding_Tilemap2() = default;

  RRefPtr<ShaderVariable> u_transform;
  RRefPtr<ShaderVariable> u_params;
  RRefPtr<ShaderVariable> u_texture;

 private:
  friend class RenderBindingBase;
  Binding_Tilemap2(ShaderBinding* binding);
};

class Binding_BitmapFilter : public RenderBindingBase {
 public:
  Binding_BitmapFilter() = default;

  RRefPtr<ShaderVariable> u_texture;

 private:
  friend class RenderBindingBase;
  Binding_BitmapFilter(ShaderBinding* binding);
};

class Binding_YUV : public RenderBindingBase {
 public:
  Binding_YUV() = default;

  RRefPtr<ShaderVariable> u_texture_y;
  RRefPtr<ShaderVariable> u_texture_u;
  RRefPtr<ShaderVariable> u_texture_v;

 private:
  friend class RenderBindingBase;
  Binding_YUV(ShaderBinding* binding);
};

}  // namespace renderer

#endif  // !RENDERER_PIPELINE_RENDER_BINDING_H_
