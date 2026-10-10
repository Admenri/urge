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

#include "core/viewport.h"

#include <algorithm>

#include "core/device.h"
#include "core/gpu_utils.h"
#include "core/graphics.h"
#include "core/pipeline.h"
#include "core/sprite_batch.h"

namespace urge {

Viewport::Viewport(int32_t x, int32_t y, int32_t width, int32_t height)
    : Node(nullptr, ZValue()),
      rect_(MakeRefCounted<Rect>(x, y, width, height)),
      color_(MakeRefCounted<Color>()),
      tone_(MakeRefCounted<Tone>()) {
  rect_->slot.change = [&]() { ResetTransform(); };
  Node::SetupTrait(this);
  ResetTransform();
  CreateEffectBindings();
}

Viewport::Viewport(RefPtr<Rect> rect)
    : Viewport(rect->data.x,
               rect->data.y,
               rect->data.width,
               rect->data.height) {}

Viewport::Viewport()
    : Viewport(0, 0, Graphics::Get().Width(), Graphics::Get().Height()) {}

Viewport::~Viewport() {
  Disposable::Dispose();
}

void Viewport::Flash(RefPtr<Color> color, int32_t duration) {
  Disposable::Guard();

  std::optional<glm::vec4> flash_color = std::nullopt;
  if (color)
    flash_color = color->Normalize();
  flashing_.Setup(flash_color, duration);
}

void Viewport::Update() {
  Disposable::Guard();

  flashing_.Update();
}

ATTR_DEF(Viewport, RefPtr<Rect>, Rect) {
  if (value.has_value()) {
    rect_->Set(*value);
    ResetTransform();
    return std::nullopt;
  } else {
    return rect_;
  }
}

ATTR_DEF(Viewport, int32_t, OX) {
  if (value.has_value()) {
    origin_.x = *value;
    ResetTransform();
    return std::nullopt;
  } else {
    return origin_.x;
  }
}

ATTR_DEF(Viewport, int32_t, OY) {
  if (value.has_value()) {
    origin_.y = *value;
    ResetTransform();
    return std::nullopt;
  } else {
    return origin_.y;
  }
}

ATTR_DEF(Viewport, RefPtr<Color>, Color) {
  if (value.has_value()) {
    color_->Set(*value);
    return std::nullopt;
  } else {
    return color_;
  }
}

ATTR_DEF(Viewport, RefPtr<Tone>, Tone) {
  if (value.has_value()) {
    tone_->Set(*value);
    return std::nullopt;
  } else {
    return tone_;
  }
}

ATTR_DEF(Viewport, RefPtr<Effect>, Effect) {
  if (value.has_value()) {
    effect_ = *value;

    ResetTransform();
    return std::nullopt;
  } else {
    return effect_;
  }
}

void Viewport::DisposeObject() {
  Node::DisposeObject();

  effect_.reset();
  offscreen_.reset();
  filter_target_.reset();
  object_uniform_ = nullptr;
  object_group_ = nullptr;
  tint_uniform_ = nullptr;
  tint_group_ = nullptr;
  filter_scene_ = nullptr;
  primitive_.Reset();
}

bool Viewport::Prepare(DrawParam param) {
  const glm::vec4 blend_tone = tone_->Normalize();
  glm::vec4 blend_color = color_->Normalize();
  const glm::vec4 flash_color = flashing_.GetColor();
  if (flashing_.IsFlashing() && flash_color.w > blend_color.w)
    blend_color = flash_color;

  TintBase::TintParam tint = {};
  tint.blend_color = blend_color;
  tint.blend_tone = blend_tone;
  g_queue.WriteBuffer(tint_uniform_, 0, &tint, sizeof(tint));

  filtering_ = false;
  post_process_ = false;
  vertex_count_ = 0;

  parent_scissor_ = param->scissors.top();

  const glm::ivec2 offset = effect_ ? glm::ivec2(-origin_.x, -origin_.y)
                                    : glm::ivec2(rect_->data.x - origin_.x,
                                                 rect_->data.y - origin_.y);
  const glm::ivec2 parent_position =
      glm::ivec2(ExtractPosition(world_transform())) - offset;
  const RectI self_scissor(parent_position.x + rect_->data.x,
                           parent_position.y + rect_->data.y, rect_->data.width,
                           rect_->data.height);

  if (effect_) {
    const RectI visible = MakeIntersect(parent_scissor_, self_scissor);
    if (!visible()) {
      region_ = RectI();
      param->scissors.push(region_);
      return true;
    }

    AcquireOffscreen(self_scissor);

    region_ = RectI(visible.x - self_scissor.x, visible.y - self_scissor.y,
                    visible.width, visible.height);
    filter_scissor_ = visible;
    filtering_ = true;

    primitive_.EmitQuad(
        self_scissor, MakeNorm(RectI(self_scissor.Size()), offscreen_->size()),
        glm::vec4(1.0f));
    vertex_count_ = primitive_.Upload();

    param->scissors.push(region_);
    return true;
  }

  region_ = MakeIntersect(parent_scissor_, self_scissor);
  if (!region_())
    region_ = RectI();
  param->scissors.push(region_);

  post_process_ = blend_color.a != 0.0f || blend_tone != glm::vec4(0.0f);
  if (post_process_) {
    AcquireOffscreen(region_);

    primitive_.EmitQuad(region_,
                        MakeNorm(RectI(region_.Size()), offscreen_->size()),
                        glm::vec4(1.0f));
    vertex_count_ = primitive_.Upload();
  }

  return true;
}

void Viewport::PostPrepare(DrawParam param) {
  param->scissors.pop();
}

bool Viewport::DoDraw(DrawParam param) {
  FlushSpriteBatch(param);

  if (effect_) {
    if (!filtering_) {
      param->pass.SetScissorRect(0, 0, 0, 0);
      return true;
    }

    param->pass.End();
    param->pass = offscreen_->BeginRendering(param->command, glm::vec4(0.0f));

    filter_target_ = param->target;
    filter_scene_ = param->scene;
    param->target = offscreen_;
    param->scene = offscreen_->scene_group();
  }

  param->pass.SetScissorRect(region_.x, region_.y, region_.width,
                             region_.height);
  return true;
}

void Viewport::PostDraw(DrawParam param) {
  FlushSpriteBatch(param);

  if (effect_ && filtering_) {
    param->pass.End();

    param->target = filter_target_;
    param->scene = filter_scene_;
    param->pass = param->target->BeginRendering(param->command);

    effect_->SetFilterSource(offscreen_);

    param->pass.SetPipeline(effect_->AcquirePipeline());
    param->pass.SetBindGroup(0, param->target->scene_group(), 0, nullptr);
    const uint32_t object_offset = 0;
    param->pass.SetBindGroup(1, object_group_, 1, &object_offset);
    param->pass.SetBindGroup(2, effect_->AcquireBindGroup(), 0, nullptr);
    param->pass.SetVertexBuffer(0, primitive_.buffer(), 0, WGPU_WHOLE_SIZE);
    param->pass.SetScissorRect(filter_scissor_.x, filter_scissor_.y,
                               filter_scissor_.width, filter_scissor_.height);
    param->pass.Draw(vertex_count_, 1, 0, 0);
  } else if (post_process_) {
    wgpu::TexelCopyTextureInfo source;
    source.texture = param->target->texture();
    source.origin.x = region_.x;
    source.origin.y = region_.y;

    wgpu::TexelCopyTextureInfo destination;
    destination.texture = offscreen_->texture();

    wgpu::Extent3D copy_size;
    copy_size.width = region_.width;
    copy_size.height = region_.height;

    param->pass.End();
    param->command.CopyTextureToTexture(&source, &destination, &copy_size);
    param->pass = param->target->BeginRendering(param->command);

    param->pass.SetPipeline(
        ShaderSet::Get().state.viewport.tint_blends.at(BLEND_NORMAL));
    param->pass.SetBindGroup(0, param->target->scene_group(), 0, nullptr);
    const uint32_t object_offset = 0;
    param->pass.SetBindGroup(1, object_group_, 1, &object_offset);
    param->pass.SetBindGroup(2, offscreen_->texture_group(), 0, nullptr);
    param->pass.SetBindGroup(3, tint_group_, 0, nullptr);
    param->pass.SetVertexBuffer(0, primitive_.buffer(), 0, WGPU_WHOLE_SIZE);
    param->pass.Draw(vertex_count_, 1, 0, 0);
  }

  param->pass.SetScissorRect(parent_scissor_.x, parent_scissor_.y,
                             parent_scissor_.width, parent_scissor_.height);
}

void Viewport::ResetTransform() {
  const glm::vec3 offset =
      effect_ ? glm::vec3(static_cast<float>(-origin_.x),
                          static_cast<float>(-origin_.y), 0.0f)
              : glm::vec3(static_cast<float>(rect_->data.x - origin_.x),
                          static_cast<float>(rect_->data.y - origin_.y), 0.0f);

  Attr_Position(MakeRefCounted<Vector3>(offset));
}

void Viewport::CreateEffectBindings() {
  const wgpu::RenderPipeline& pipeline =
      ShaderSet::Get().state.viewport.tint_blends.at(BLEND_NORMAL);
  wgpu::BufferDescriptor object_desc;
  object_desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  object_desc.size = sizeof(ObjectData);
  object_uniform_ = g_device.CreateBuffer(&object_desc);

  const ObjectData object_data = {glm::mat4(1.0f)};
  g_queue.WriteBuffer(object_uniform_, 0, &object_data, sizeof(object_data));

  util::BufferSet object_binding(object_uniform_);
  object_binding.size = sizeof(ObjectData);
  object_group_ = util::CreateBindGroup(pipeline.GetBindGroupLayout(1),
                                        {{0, object_binding}});

  wgpu::BufferDescriptor tint_desc;
  tint_desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  tint_desc.size = sizeof(TintBase::TintParam);
  tint_uniform_ = g_device.CreateBuffer(&tint_desc);

  tint_group_ = util::CreateBindGroup(pipeline.GetBindGroupLayout(3),
                                      {{0, util::BufferSet(tint_uniform_)}});
}

void Viewport::AcquireOffscreen(const RectI& region) {
  if (offscreen_ && offscreen_->size().x >= region.width &&
      offscreen_->size().y >= region.height)
    return;

  const int32_t width =
      std::max(region.width, offscreen_ ? offscreen_->size().x : 0);
  const int32_t height =
      std::max(region.height, offscreen_ ? offscreen_->size().y : 0);
  offscreen_ = MakeRefCounted<Bitmap>(width, height);
}

}  // namespace urge
