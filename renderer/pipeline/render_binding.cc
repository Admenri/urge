// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "renderer/pipeline/render_binding.h"

namespace renderer {

RenderBindingBase::RenderBindingBase(ShaderBinding* binding)
    : binding_(binding) {}

Binding_Base::Binding_Base(ShaderBinding* binding)
    : RenderBindingBase(binding) {
  u_transform = (*this)->GetVariableByName(Diligent::SHADER_TYPE_VERTEX,
                                           "WorldMatrixBuffer");
  u_texture =
      (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "u_Texture");
}

Binding_BitmapBlt::Binding_BitmapBlt(ShaderBinding* binding)
    : RenderBindingBase(binding) {
  u_transform = (*this)->GetVariableByName(Diligent::SHADER_TYPE_VERTEX,
                                           "WorldMatrixBuffer");
  u_texture =
      (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "u_Texture");
  u_dst_texture =
      (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "u_DstTexture");
}

Binding_Color::Binding_Color(ShaderBinding* binding)
    : RenderBindingBase(binding) {
  u_transform = (*this)->GetVariableByName(Diligent::SHADER_TYPE_VERTEX,
                                           "WorldMatrixBuffer");
}

Binding_Flat::Binding_Flat(ShaderBinding* binding)
    : RenderBindingBase(binding) {
  u_transform = (*this)->GetVariableByName(Diligent::SHADER_TYPE_VERTEX,
                                           "WorldMatrixBuffer");
  u_texture =
      (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "u_Texture");
  u_params = (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL,
                                        "FlatUniformConstants");
}

Binding_Sprite::Binding_Sprite(ShaderBinding* binding)
    : RenderBindingBase(binding) {
  u_transform = (*this)->GetVariableByName(Diligent::SHADER_TYPE_VERTEX,
                                           "WorldMatrixBuffer");
  u_params =
      (*this)->GetVariableByName(Diligent::SHADER_TYPE_VERTEX, "u_Params");
  u_effect = (*this)->GetVariableByName(Diligent::SHADER_TYPE_VERTEX,
                                        "SpriteUniformConstants");
  u_texture =
      (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "u_Texture");
}

Binding_AlphaTrans::Binding_AlphaTrans(ShaderBinding* binding)
    : RenderBindingBase(binding) {
  u_frozen_texture = (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL,
                                                "u_FrozenTexture");
  u_current_texture = (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL,
                                                 "u_CurrentTexture");
}

Binding_VagueTrans::Binding_VagueTrans(ShaderBinding* binding)
    : RenderBindingBase(binding) {
  u_frozen_texture = (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL,
                                                "u_FrozenTexture");
  u_current_texture = (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL,
                                                 "u_CurrentTexture");
  u_trans_texture =
      (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "u_TransTexture");
}

Binding_Tilemap::Binding_Tilemap(ShaderBinding* binding)
    : RenderBindingBase(binding) {
  u_transform = (*this)->GetVariableByName(Diligent::SHADER_TYPE_VERTEX,
                                           "WorldMatrixBuffer");
  u_params = (*this)->GetVariableByName(Diligent::SHADER_TYPE_VERTEX,
                                        "TilemapUniformBuffer");
  u_texture =
      (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "u_Texture");
}

Binding_Tilemap2::Binding_Tilemap2(ShaderBinding* binding)
    : RenderBindingBase(binding) {
  u_transform = (*this)->GetVariableByName(Diligent::SHADER_TYPE_VERTEX,
                                           "WorldMatrixBuffer");
  u_params = (*this)->GetVariableByName(Diligent::SHADER_TYPE_VERTEX,
                                        "Tilemap2UniformBuffer");
  u_texture =
      (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "u_Texture");
}

Binding_BitmapFilter::Binding_BitmapFilter(ShaderBinding* binding)
    : RenderBindingBase(binding) {
  u_texture =
      (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "u_Texture");
}

Binding_YUV::Binding_YUV(ShaderBinding* binding) : RenderBindingBase(binding) {
  u_texture_y =
      (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "u_TextureY");
  u_texture_u =
      (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "u_TextureU");
  u_texture_v =
      (*this)->GetVariableByName(Diligent::SHADER_TYPE_PIXEL, "u_TextureV");
}

}  // namespace renderer
