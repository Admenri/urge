// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/sprite.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>

#include "glm/gtc/matrix_transform.hpp"

#include "core/config.h"
#include "core/gpu.h"
#include "core/logger.h"
#include "core/pipeline.h"
#include "core/uniform.h"

namespace urge {

namespace {

// Legacy PI macro value from the raylib headers.
constexpr float kPi = 3.14159265358979323846f;

//! Height of one strip a waved sprite is bent in, in pixels.
constexpr int32_t kWaveBlockAlign = 8;

//! The shortest wavelength a wave is evaluated with, a length of zero would
//! divide the phase of a block by it.
constexpr int32_t kMinimumWaveLength = 1;

//! Converts degrees into radians, the unit the sine of a wave is taken in.
float DegreesToRadians(float degrees) {
  return degrees * (kPi / 180.0f);
}

}  // namespace

Sprite::Sprite(RefPtr<Viewport> viewport)
    : Node(viewport, ZValue()),
      src_rect_(MakeRefCounted<Rect>()),
      // The default attribute values of RGSS: no color and no tone blend
      color_(MakeRefCounted<Color>()),
      tone_(MakeRefCounted<Tone>()),
      rgssvx_style_(Config::Get().vx() || Config::Get().vxa()) {
  Node::SetupTrait(this);
}

Sprite::~Sprite() {
  Disposable::Dispose();
}

void Sprite::Flash(RefPtr<Color> color, int32_t duration) {
  flash_.color = color ? color->Normalize() : glm::vec4{};
  flash_.step = duration > 0 ? (flash_.color.w / duration) : 0.0f;
}

void Sprite::Update() {
  wave_phase_ += wave_speed_ / 180.0f;
  wave_phase_ = std::fmod(wave_phase_, 360.0f);

  flash_.color.w -= flash_.step;
  if (flash_.color.w <= 0) {
    flash_.color = {};
    flash_.step = 0.0f;
  }
}

int32_t Sprite::Width() {
  return src_rect_->data.width;
}

int32_t Sprite::Height() {
  return src_rect_->data.height;
}

ATTR_DEF(Sprite, RefPtr<Viewport>, Viewport) {
  auto parent_value = Node::Attr_Parent(value);
  if (parent_value.has_value()) {
    auto parent = *parent_value;
    Viewport* viewport = parent ? parent->TryCast<Viewport>() : nullptr;
    return RefPtr<Viewport>(viewport);
  } else {
    return std::nullopt;
  }
}

ATTR_DEF(Sprite, RefPtr<Bitmap>, Bitmap) {
  if (value.has_value()) {
    bitmap_ = *value;
    if (bitmap_)
      src_rect_->Set(bitmap_->GetRect());
    return std::nullopt;
  } else {
    return bitmap_;
  }
}

ATTR_DEF(Sprite, RefPtr<Rect>, SrcRect) {
  if (value.has_value()) {
    src_rect_->Set(*value);
    return std::nullopt;
  } else {
    return src_rect_;
  }
}

ATTR_DEF(Sprite, int32_t, X) {
  if (value.has_value()) {
    x_ = *value;
    return std::nullopt;
  } else {
    return x_;
  }
}

ATTR_DEF(Sprite, int32_t, Y) {
  if (value.has_value()) {
    y_ = *value;
    if (rgssvx_style_) {
      ZValue zvalue = Node::GetOrder();
      zvalue.sorting = y_;
      Node::SortWith(zvalue);
    }
    return std::nullopt;
  } else {
    return y_;
  }
}

ATTR_DEF(Sprite, int32_t, OX) {
  if (value.has_value()) {
    ox_ = *value;
    return std::nullopt;
  } else {
    return ox_;
  }
}

ATTR_DEF(Sprite, int32_t, OY) {
  if (value.has_value()) {
    oy_ = *value;
    return std::nullopt;
  } else {
    return oy_;
  }
}

ATTR_DEF(Sprite, float, ZoomX) {
  if (value.has_value()) {
    zoom_x_ = *value;
    return std::nullopt;
  } else {
    return zoom_x_;
  }
}

ATTR_DEF(Sprite, float, ZoomY) {
  if (value.has_value()) {
    zoom_y_ = *value;
    return std::nullopt;
  } else {
    return zoom_y_;
  }
}

ATTR_DEF(Sprite, float, Angle) {
  if (value.has_value()) {
    angle_ = *value;
    return std::nullopt;
  } else {
    return angle_;
  }
}

ATTR_DEF(Sprite, int32_t, WaveAmp) {
  if (value.has_value()) {
    wave_amp_ = *value;
    return std::nullopt;
  } else {
    return wave_amp_;
  }
}

ATTR_DEF(Sprite, int32_t, WaveLength) {
  if (value.has_value()) {
    wave_length_ = *value;
    return std::nullopt;
  } else {
    return wave_length_;
  }
}

ATTR_DEF(Sprite, int32_t, WaveSpeed) {
  if (value.has_value()) {
    wave_speed_ = *value;
    return std::nullopt;
  } else {
    return wave_speed_;
  }
}

ATTR_DEF(Sprite, float, WavePhase) {
  if (value.has_value()) {
    wave_phase_ = *value;
    return std::nullopt;
  } else {
    return wave_phase_;
  }
}

ATTR_DEF(Sprite, bool, Mirror) {
  if (value.has_value()) {
    mirror_ = *value;
    return std::nullopt;
  } else {
    return mirror_;
  }
}

ATTR_DEF(Sprite, int32_t, BushDepth) {
  if (value.has_value()) {
    bush_depth_ = *value;
    return std::nullopt;
  } else {
    return bush_depth_;
  }
}

ATTR_DEF(Sprite, int32_t, BushOpacity) {
  if (value.has_value()) {
    bush_opacity_ = std::clamp<int32_t>(*value, 0, 255);
    return std::nullopt;
  } else {
    return bush_opacity_;
  }
}

ATTR_DEF(Sprite, int32_t, Opacity) {
  if (value.has_value()) {
    opacity_ = std::clamp<int32_t>(*value, 0, 255);
    return std::nullopt;
  } else {
    return opacity_;
  }
}

ATTR_DEF(Sprite, int32_t, BlendType) {
  if (value.has_value()) {
    /* The blend type indexes the pipeline states of the sprite shader, so it is
       kept inside the range of the blend types the engine knows. */
    blend_type_ = std::clamp(*value, static_cast<int32_t>(BLEND_NONE),
                             static_cast<int32_t>(BLEND_SUBTRACT));
    return std::nullopt;
  } else {
    return blend_type_;
  }
}

ATTR_DEF(Sprite, RefPtr<Color>, Color) {
  if (value.has_value()) {
    color_->Set(*value);
    return std::nullopt;
  } else {
    return color_;
  }
}

ATTR_DEF(Sprite, RefPtr<Tone>, Tone) {
  if (value.has_value()) {
    tone_->Set(*value);
    return std::nullopt;
  } else {
    return tone_;
  }
}

ATTR_DEF(Sprite, RefPtr<Effect>, Effect) {
  if (value.has_value()) {
    effect_ = *value;
    return std::nullopt;
  } else {
    return effect_;
  }
}

void Sprite::DisposeObject() {
  Node::DisposeObject();

  bitmap_.reset();
  effect_.reset();
}

bool Sprite::Prepare(DrawParam param) {
  primitive_slot_ = {};

  // A sprite without a bitmap, or with one which was disposed, draws nothing
  if (!Disposable::Check(bitmap_))
    return false;

  // The geometry goes into the vertex batch of the frame, see QuadVertexManager
  primitive_slot_ = EmitGeometryInternal(*param->vertices);
  // A source rectangle which is empty after being limited to the texture
  if (!primitive_slot_.count)
    return false;

  /* The transform of a sprite is the one of the node hierarchy with its own on
     top: the sprite is positioned at (x, y), the origin is the point it is
     scaled and rotated around, so the origin is subtracted before the scale and
     the rotation and the position is added after them. The rotation is
     negated, the y axis of the engine points downwards. */
  const glm::mat4 transform =
      world_transform() *
      glm::translate(glm::mat4(1.0f), glm::vec3(static_cast<float>(x_),
                                                static_cast<float>(y_), 0.0f)) *
      glm::rotate(glm::mat4(1.0f), DegreesToRadians(-angle_),
                  glm::vec3(0.0f, 0.0f, 1.0f)) *
      glm::scale(glm::mat4(1.0f), glm::vec3(zoom_x_, zoom_y_, 1.0f)) *
      glm::translate(
          glm::mat4(1.0f),
          glm::vec3(static_cast<float>(-ox_), static_cast<float>(-oy_), 0.0f));

  /* Both uniforms of a sprite travel in a buffer it shares with every other
     sprite of the frame instead of in a buffer of its own, so the draw only has
     to bind the slots handed out here. */
  UniformManager& uniforms = UniformManager::Get();

  ObjectData object_data;
  object_data.model_mat = transform;
  object_slot_ = uniforms.object_uniforms().Acquire(object_data);
  param_slot_ = uniforms.sprite_uniforms().Acquire(MakeParamInternal());

  return object_slot_.chunk != UniformBlockPool::kInvalidChunk &&
         param_slot_.chunk != UniformBlockPool::kInvalidChunk;
}

bool Sprite::DoDraw(DrawParam param) {
  UniformManager& uniforms = UniformManager::Get();
  const UniformBlockPool::Chunk& object_chunk =
      uniforms.object_uniforms().chunk(object_slot_.chunk);
  const UniformBlockPool::Chunk& param_chunk =
      uniforms.sprite_uniforms().chunk(param_slot_.chunk);

  /* An effect replaces the shader and the blend of the draw while the geometry
     and the transform stay the ones of this sprite: the scene of the target,
     the object transform of the frame and the custom bind group the effect was
     given with are what its pipeline reads, see Effect. */
  if (effect_) {
    param->pass.SetPipeline(effect_->AcquirePipeline());
    param->pass.SetBindGroup(0, param->scene, 0, nullptr);
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetBindGroup(2, effect_->AcquireBindGroup(), 0, nullptr);
    param->pass.SetVertexBuffer(0, param->vertices->buffer(), 0,
                                WGPU_WHOLE_SIZE);
    param->pass.Draw(primitive_slot_.count, 1, primitive_slot_.first, 0);
    return false;
  }

  param->pass.SetPipeline(ShaderSet::Get().state.sprite_blends.at(
      static_cast<BlendType>(blend_type_)));
  // The scene of the render target, its object set is not the one of a sprite
  param->pass.SetBindGroup(0, param->scene, 0, nullptr);
  // The object transform of this sprite, bound with the offset of its slot
  param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
  param->pass.SetBindGroup(2, bitmap_->texture_group(), 0, nullptr);
  // The parameter of this sprite, bound with the offset of its slot
  param->pass.SetBindGroup(3, param_chunk.group, 1, &param_slot_.offset);
  // The batch of the frame holds the vertices, this draw takes its own range
  param->pass.SetVertexBuffer(0, param->vertices->buffer(), 0, WGPU_WHOLE_SIZE);
  param->pass.Draw(primitive_slot_.count, 1, primitive_slot_.first, 0);

  return false;
}

SpriteBase::SpriteParam Sprite::MakeParamInternal() {
  /* The blend color of a sprite is its color, or the color of the flash while
     it is the stronger one of the two; a sprite which is not flashing keeps
     the color of its Color attribute. */
  glm::vec4 blend_color = color_->Normalize();
  if (flash_.color.w > 0.0f && flash_.color.w > blend_color.w)
    blend_color = flash_.color;

  /* The bush cuts the sprite off below a line of its source rectangle, which
     the shader compares against the texture coordinate of a pixel, so the depth
     is normalized the way a texture coordinate is. */
  const float texture_height =
      static_cast<float>(std::max(1, bitmap_->size().y));
  const RectI src = src_rect_->data;

  SpriteBase::SpriteParam param = {};
  param.blend_color = blend_color;
  param.blend_tone = tone_->Normalize();
  param.bush_depth =
      static_cast<float>(src.y + src.height - bush_depth_) / texture_height;
  param.bush_opacity = static_cast<float>(bush_opacity_) / 255.0f;
  return param;
}

PrimitiveEmitter::Slot Sprite::EmitGeometryInternal(
    PrimitiveEmitter& primitive) {
  const int32_t texture_width = bitmap_->size().x;
  const int32_t texture_height = bitmap_->size().y;

  // The source rectangle of a sprite is limited to the bitmap it reads from
  RectI src = src_rect_->data;
  src.width = std::clamp(src.width, 0, std::max(0, texture_width - src.x));
  src.height = std::clamp(src.height, 0, std::max(0, texture_height - src.y));
  if (src.width == 0 || src.height == 0)
    return {};

  const glm::vec2 texture_size(static_cast<float>(texture_width),
                               static_cast<float>(texture_height));
  /* The blend state of the engine and the contents of a bitmap store
     premultiplied alpha, so the opacity of a sprite scales all four channels of
     the vertex color instead of the alpha channel alone. */
  const glm::vec4 color(static_cast<float>(opacity_) / 255.0f);

  if (wave_amp_ == 0) {
    /* The vertices of a plain quad are the source rectangle at the origin, the
       position and the origin of the sprite are part of its model matrix. */
    RectI texcoord = src;
    if (mirror_)
      texcoord = RectI(src.x + src.width, src.y, -src.width, src.height);

    primitive.EmitQuad(RectF(0.0f, 0.0f, static_cast<float>(src.width),
                             static_cast<float>(src.height)),
                       MakeNorm(RectF(texcoord), texture_size), color);
  } else {
    /* A wave bends the sprite in strips: every strip of kWaveBlockAlign pixels
       is moved sideways by the sine of the phase it is at, which is the wave of
       a flag. The phase of a strip follows from the phase of the sprite and the
       part of a wave length the strip is at, see Update(). */
    const float phase = DegreesToRadians(wave_phase_);
    const float length =
        static_cast<float>(std::max<int32_t>(kMinimumWaveLength, wave_length_));

    const auto emit_block = [&](int32_t block_y, int32_t block_height) {
      const float offset =
          std::sin(phase + (static_cast<float>(block_y) / length) * kPi) *
          static_cast<float>(wave_amp_);

      RectI texcoord(src.x, src.y + block_y, src.width, block_height);
      if (mirror_)
        texcoord = RectI(texcoord.x + texcoord.width, texcoord.y,
                         -texcoord.width, texcoord.height);

      primitive.Rect(RectF(offset, static_cast<float>(block_y),
                           static_cast<float>(src.width),
                           static_cast<float>(block_height)),
                     MakeNorm(RectF(texcoord), texture_size));
    };

    primitive.BeginQuad().Color4f(color);

    const int32_t whole_blocks = src.height / kWaveBlockAlign;
    for (int32_t index = 0; index < whole_blocks; ++index)
      emit_block(index * kWaveBlockAlign, kWaveBlockAlign);

    const int32_t last_block = src.height % kWaveBlockAlign;
    if (last_block)
      emit_block(whole_blocks * kWaveBlockAlign, last_block);
  }

  return primitive.End();
}

}  // namespace urge
