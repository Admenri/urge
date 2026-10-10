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
#include "core/device.h"
#include "core/logger.h"
#include "core/pipeline.h"
#include "core/uniform.h"

namespace urge {

namespace {

constexpr float kPi = 3.14159265358979323846f;

constexpr int32_t kWaveBlockAlign = 8;

constexpr int32_t kMinimumWaveLength = 1;

float DegreesToRadians(float degrees) {
  return degrees * (kPi / 180.0f);
}

}  // namespace

Sprite::Sprite(RefPtr<Viewport> viewport)
    : Node(viewport, ZValue()),
      src_rect_(MakeRefCounted<Rect>()),

      color_(MakeRefCounted<Color>()),
      tone_(MakeRefCounted<Tone>()),
      rgssvx_style_(Config::Get().vx() || Config::Get().vxa()) {
  Node::SetupTrait(this);
}

Sprite::~Sprite() {
  Disposable::Dispose();
}

void Sprite::Flash(RefPtr<Color> color, int32_t duration) {
  Disposable::Guard();

  std::optional<glm::vec4> flash_color = std::nullopt;
  if (color)
    flash_color = color->Normalize();
  flashing_.Setup(flash_color, duration);
}

void Sprite::Update() {
  Disposable::Guard();

  wave_phase_ += wave_speed_ / 180.0f;
  wave_phase_ = std::fmod(wave_phase_, 360.0f);

  flashing_.Update();
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
  object_slot_ = {};

  if (!IsDrawableInternal())
    return false;

  PrimitiveEmitter& emitter = SpriteBatch::Get().emitter();
  primitive_slot_ = EmitGeometryInternal(emitter);
  if (!primitive_slot_.count)
    return false;

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

  if (effect_) {
    ObjectData object_data;
    object_data.model_mat = transform;
    object_slot_ = UniformManager::Get().object_uniforms().Acquire(object_data);
    return object_slot_.chunk != UniformBlockPool::kInvalidChunk;
  }

  SpriteBatch::Get().SetParam(primitive_slot_.first,
                              MakeParamInternal(transform));
  return true;
}

bool Sprite::DoDraw(DrawParam param) {
  if (effect_) {
    UniformManager& uniforms = UniformManager::Get();
    const UniformBlockPool::Chunk& object_chunk =
        uniforms.object_uniforms().chunk(object_slot_.chunk);

    param->pass.SetPipeline(effect_->AcquirePipeline());
    param->pass.SetBindGroup(0, param->scene, 0, nullptr);
    param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
    param->pass.SetBindGroup(2, effect_->AcquireBindGroup(), 0, nullptr);
    param->pass.SetVertexBuffer(0, SpriteBatch::Get().emitter().buffer(), 0,
                                WGPU_WHOLE_SIZE);
    param->pass.Draw(primitive_slot_.count, 1, primitive_slot_.first, 0);
    return false;
  }

  SpriteBatch::Run& run = SpriteBatch::Get().run();

  const bool extends_run = run.active && run.texture.get() == bitmap_.get() &&
                           run.blend_type == blend_type_ &&
                           run.scene.Get() == param->scene.Get() &&
                           run.end_vertex == primitive_slot_.first;

  if (!extends_run) {
    FlushSpriteBatch(param);
    run.active = true;
    run.texture = bitmap_;
    run.blend_type = blend_type_;
    run.scene = param->scene;
    run.first_vertex = primitive_slot_.first;
  }

  run.end_vertex = primitive_slot_.first + primitive_slot_.count;

  if (!NextIsBatchable())
    FlushSpriteBatch(param);

  return false;
}

bool Sprite::NextIsBatchable() {
  if (effect_ || SpriteBatch::Get().disabled())
    return false;

  Drawable* next = NextDrawable();
  if (!next)
    return false;

  Sprite* next_sprite = next->TryCast<Sprite>();
  return next_sprite && next->visible() && next_sprite->CanBatchWith(*this);
}

bool Sprite::CanBatchWith(const Sprite& other) const {
  if (effect_ || other.effect_)
    return false;

  if (!IsDrawableInternal())
    return false;

  return bitmap_.get() == other.bitmap_.get() &&
         blend_type_ == other.blend_type_;
}

bool Sprite::IsDrawableInternal() const {
  if (!Disposable::Check(bitmap_))
    return false;

  if (flashing_.IsFlashing() && flashing_.IsInvalid())
    return false;

  const RectI src = ClampSrcRectInternal();
  return src.width > 0 && src.height > 0;
}

RectI Sprite::ClampSrcRectInternal() const {
  const int32_t texture_width = bitmap_->size().x;
  const int32_t texture_height = bitmap_->size().y;

  RectI src = src_rect_->data;
  src.width = std::clamp(src.width, 0, std::max(0, texture_width - src.x));
  src.height = std::clamp(src.height, 0, std::max(0, texture_height - src.y));
  return src;
}

SpriteParam Sprite::MakeParamInternal(const glm::mat4& transform) {
  glm::vec4 blend_color = color_->Normalize();
  glm::vec4 flash_color = flashing_.GetColor();
  if (flashing_.IsFlashing())
    blend_color = (flash_color.w > blend_color.w ? flash_color : blend_color);

  const float texture_height =
      static_cast<float>(std::max(1, bitmap_->size().y));
  const RectI src = src_rect_->data;

  SpriteParam param;
  param.model_mat = transform;
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

  RectI src = ClampSrcRectInternal();
  if (src.width == 0 || src.height == 0)
    return {};

  const glm::vec2 texture_size(static_cast<float>(texture_width),
                               static_cast<float>(texture_height));

  const glm::vec4 color(static_cast<float>(opacity_) / 255.0f);

  if (wave_amp_ == 0) {
    RectI texcoord = src;
    if (mirror_)
      texcoord = RectI(src.x + src.width, src.y, -src.width, src.height);

    primitive.EmitQuad(RectF(0.0f, 0.0f, static_cast<float>(src.width),
                             static_cast<float>(src.height)),
                       MakeNorm(RectF(texcoord), texture_size), color);
  } else {
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
