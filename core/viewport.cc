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
#include <span>

#include "core/gpu.h"
#include "core/graphics.h"
#include "core/pipeline.h"

namespace urge {

Viewport::Viewport(int32_t x, int32_t y, int32_t width, int32_t height)
    : Node(nullptr, ZValue()),
      rect_(MakeRefCounted<Rect>(x, y, width, height)),
      color_(MakeRefCounted<Color>()),
      tone_(MakeRefCounted<Tone>()) {
  rect_->on_change = [&]() { ResetTransform(); };
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
  flash_.color = color ? color->Normalize() : glm::vec4();
  flash_.step =
      duration > 0 ? (flash_.color.w / static_cast<float>(duration)) : 0.0f;
}

void Viewport::Update() {
  flash_.color.w -= flash_.step;
  if (flash_.color.w <= 0) {
    flash_.color = {};
    flash_.step = 0.0f;
  }
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
    /* Whether an effect is set decides where the children of this viewport are
       placed -- in the render target or in the texture the effect filters --,
       so the offset of the node changes with it, see ResetTransform(). */
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
  object_uniform_ = nullptr;
  object_group_ = nullptr;
  tint_uniform_ = nullptr;
  tint_group_ = nullptr;
  primitive_.Reset();
}

bool Viewport::Prepare(DrawParam param) {
  glm::vec4 blend_color = color_->Normalize();
  const glm::vec4 blend_tone = tone_->Normalize();
  if (flash_.color.w > 0.0f && flash_.color.w > blend_color.w)
    blend_color = flash_.color;

  TintBase::TintParam tint = {};
  tint.blend_color = blend_color;
  tint.blend_tone = blend_tone;
  GPUDevice::Get().queue().WriteBuffer(tint_uniform_, 0, &tint, sizeof(tint));

  return true;
}

bool Viewport::DoDraw(DrawParam param) {
  const RectI current_scissor = param->scissors.top();

  /* The rect of a viewport is in the coordinates of the render target, it is
     where a child of it is drawn, and the origin scrolls the content inside of
     that rect without moving it, see ResetTransform(). The rect therefore has
     to be placed the way the parent hierarchy placed the node. That placement
     is recovered by removing the offset the node itself adds from its world
     transform: the offset is the rect less the origin for a plain viewport and
     the origin alone for one whose region an effect filters, see
     ResetTransform(). */
  const glm::ivec2 offset = effect_ ? glm::ivec2(-origin_.x, -origin_.y)
                                    : glm::ivec2(rect_->data.x - origin_.x,
                                                 rect_->data.y - origin_.y);
  const glm::ivec2 parent_position =
      glm::ivec2(ExtractPosition(world_transform())) - offset;

  const RectI self_scissor(parent_position.x + rect_->data.x,
                           parent_position.y + rect_->data.y, rect_->data.width,
                           rect_->data.height);

  /* A viewport with an effect hands its region to that effect: the children
     draw into a texture of their own and the effect composites it back, instead
     of drawing straight into the render target the way a plain one -- and its
     own tint pass -- expect. */
  if (effect_)
    return BeginFilter(param, current_scissor, self_scissor);

  RectI result_scissor = MakeIntersect(current_scissor, self_scissor);
  if (!result_scissor()) {
    /* An empty scissor is legal and clips every draw inside of it away, which
       is what a viewport its parent covers nothing of shows. Its origin is
       dropped because the region of a viewport can lie outside of the target,
       which the scissor of a pass may not, and PostDraw() leaves a region of no
       pixels alone. */
    result_scissor = RectI();
  }

  param->scissors.push(result_scissor);
  param->pass.SetScissorRect(result_scissor.x, result_scissor.y,
                             result_scissor.width, result_scissor.height);

  return true;
}

bool Viewport::BeginFilter(DrawParam param,
                           const RectI& parent_scissor,
                           const RectI& screen_scissor) {
  const RectI visible = MakeIntersect(parent_scissor, screen_scissor);
  if (!visible()) {
    /* None of the region lies in anything its parent shows, so no pixel of it
       can reach the target and no texture has to be rendered, see PostDraw().
       An empty scissor is legal and clips every child away. */
    param->scissors.push(RectI());
    param->pass.SetScissorRect(0, 0, 0, 0);
    return true;
  }

  /* The children are moved off the render target into a texture of the size of
     the region, so the pass of the parent is closed and one on that texture is
     opened in its place. It is cleared because the children are the only source
     of its pixels and a texel they leave bare has to stay transparent for the
     effect to composite the target through it. */
  AcquireOffscreen(screen_scissor);

  param->pass.End();

  wgpu::RenderPassColorAttachment color_attachment = {
      .view = offscreen_->texture_view(),
      .loadOp = wgpu::LoadOp::Clear,
      .storeOp = wgpu::StoreOp::Store,
      .clearValue = {0.0, 0.0, 0.0, 0.0},
  };
  wgpu::RenderPassDepthStencilAttachment depth_stencil_attachment = {
      .view = offscreen_->depth_stencil_view(),
      .depthLoadOp = wgpu::LoadOp::Clear,
      .depthStoreOp = wgpu::StoreOp::Discard,
      .depthClearValue = 1.0f,
      .stencilLoadOp = wgpu::LoadOp::Clear,
      .stencilStoreOp = wgpu::StoreOp::Discard,
      .stencilClearValue = 0,
  };
  wgpu::RenderPassDescriptor render_pass_desc = {
      .colorAttachmentCount = 1,
      .colorAttachments = &color_attachment,
      .depthStencilAttachment = &depth_stencil_attachment,
  };
  param->pass = param->command.BeginRenderPass(&render_pass_desc);

  /* The children are placed in the coordinates of the texture rather than of
     the render target -- the region starts at its corner --, so both the scene
     they project with and the target they draw into change with them, see
     ResetTransform(). */
  filter_target_ = param->target;
  filter_scene_ = param->scene;
  param->target = offscreen_;
  param->scene = offscreen_->scene_group();

  /* The clip the children see is the visible part of the region, taken in the
     coordinates of the texture. */
  const RectI texture_scissor(visible.x - screen_scissor.x,
                              visible.y - screen_scissor.y, visible.width,
                              visible.height);
  param->scissors.push(texture_scissor);
  param->pass.SetScissorRect(texture_scissor.x, texture_scissor.y,
                             texture_scissor.width, texture_scissor.height);

  filter_region_ = screen_scissor;
  filter_scissor_ = visible;
  filtering_ = true;
  return true;
}

void Viewport::PostDraw(DrawParam param) {
  if (effect_) {
    /* An effect replaces the tint pass entirely: it composites the region the
       children filled instead of copying the target into the scratch texture
       and tinting that copy back, so the color and the tone of the viewport do
       not take part while one is set. */
    if (filtering_)
      FinishFilter(param);
  } else {
    glm::vec4 blend_color = color_->Normalize();
    const glm::vec4 blend_tone = tone_->Normalize();
    if (flash_.color.w > 0.0f && flash_.color.w > blend_color.w)
      blend_color = flash_.color;
    const bool post_process =
        (blend_color.a != 0 || blend_tone != glm::vec4(0.0f));

    if (post_process) {
      const RectI viewport_region = param->scissors.top();
      param->pass.End();

      // Allocate ping-pong texture and copy target data
      AcquireOffscreen(viewport_region);
      wgpu::TexelCopyTextureInfo source;
      source.texture = param->target->texture();
      source.origin.x = viewport_region.x;
      source.origin.y = viewport_region.y;
      wgpu::TexelCopyTextureInfo destination;
      destination.texture = offscreen_->texture();
      wgpu::Extent3D copy_size;
      copy_size.width = viewport_region.width;
      copy_size.height = viewport_region.height;
      param->command.CopyTextureToTexture(&source, &destination, &copy_size);

      // The batch of the frame is already written, this quad needs its own
      // emitter
      primitive_.EmitQuad(
          viewport_region,
          MakeNorm(RectI(viewport_region.Size()), offscreen_->size()),
          glm::vec4(1.0f));
      const std::uint32_t vertex_count = primitive_.Upload();

      // Begin original render target
      wgpu::RenderPassColorAttachment color_attachment = {
          .view = param->target->texture_view(),
          .loadOp = wgpu::LoadOp::Load,
          .storeOp = wgpu::StoreOp::Store,
      };
      wgpu::RenderPassDepthStencilAttachment depth_stencil_attachment = {
          .view = param->target->depth_stencil_view(),
          .depthLoadOp = wgpu::LoadOp::Load,
          .depthStoreOp = wgpu::StoreOp::Store,
          .stencilLoadOp = wgpu::LoadOp::Load,
          .stencilStoreOp = wgpu::StoreOp::Store,
      };
      wgpu::RenderPassDescriptor render_pass_desc = {
          .colorAttachmentCount = 1,
          .colorAttachments = &color_attachment,
          .depthStencilAttachment = &depth_stencil_attachment,
      };
      param->pass = param->command.BeginRenderPass(&render_pass_desc);

      // Draw post process
      param->pass.SetPipeline(
          ShaderSet::Get().state.tint_blends.at(BLEND_NORMAL));
      param->pass.SetBindGroup(0, param->scene, 0, nullptr);
      const uint32_t object_offset = 0;
      param->pass.SetBindGroup(1, object_group_, 1, &object_offset);
      param->pass.SetBindGroup(2, offscreen_->texture_group(), 0, nullptr);
      param->pass.SetBindGroup(3, tint_group_, 0, nullptr);
      param->pass.SetVertexBuffer(0, primitive_.buffer(), 0, WGPU_WHOLE_SIZE);
      param->pass.Draw(vertex_count, 1, 0, 0);
    }
  }

  // Restore scissor stack
  param->scissors.pop();
  const RectI current_scissor = param->scissors.top();
  param->pass.SetScissorRect(current_scissor.x, current_scissor.y,
                             current_scissor.width, current_scissor.height);
}

void Viewport::FinishFilter(DrawParam param) {
  // The children are done, close the pass of the texture they filled
  param->pass.End();

  /* Back to the render target the effect composites into, which the children
     borrowed from this node, and the quad of the region is emitted in the
     coordinates of that target, where the effect draws it back. */
  param->target = filter_target_;
  param->scene = filter_scene_;

  primitive_.EmitQuad(
      filter_region_,
      MakeNorm(RectI(filter_region_.Size()), offscreen_->size()),
      glm::vec4(1.0f));
  const std::uint32_t vertex_count = primitive_.Upload();

  wgpu::RenderPassColorAttachment color_attachment = {
      .view = param->target->texture_view(),
      .loadOp = wgpu::LoadOp::Load,
      .storeOp = wgpu::StoreOp::Store,
  };
  wgpu::RenderPassDepthStencilAttachment depth_stencil_attachment = {
      .view = param->target->depth_stencil_view(),
      .depthLoadOp = wgpu::LoadOp::Load,
      .depthStoreOp = wgpu::StoreOp::Store,
      .stencilLoadOp = wgpu::LoadOp::Load,
      .stencilStoreOp = wgpu::StoreOp::Store,
  };
  wgpu::RenderPassDescriptor render_pass_desc = {
      .colorAttachmentCount = 1,
      .colorAttachments = &color_attachment,
      .depthStencilAttachment = &depth_stencil_attachment,
  };
  param->pass = param->command.BeginRenderPass(&render_pass_desc);

  /* The shader of the effect reads the region the children filled from the
     custom set of the effect -- the texture at binding 0 and its sampler at
     binding 1 -- which the engine hands over here, see
     Effect::SetFilterSource. */
  Effect* effect = effect_.get();
  effect->SetFilterSource(offscreen_);

  param->pass.SetPipeline(effect->AcquirePipeline());
  param->pass.SetBindGroup(0, param->scene, 0, nullptr);
  const uint32_t object_offset = 0;
  param->pass.SetBindGroup(1, object_group_, 1, &object_offset);
  param->pass.SetBindGroup(2, effect->AcquireBindGroup(), 0, nullptr);
  param->pass.SetVertexBuffer(0, primitive_.buffer(), 0, WGPU_WHOLE_SIZE);
  /* The effect paints the region back where it was taken from and touches
     nothing the parent does not show of it, so it is clipped the way the
     children were. */
  param->pass.SetScissorRect(filter_scissor_.x, filter_scissor_.y,
                             filter_scissor_.width, filter_scissor_.height);
  param->pass.Draw(vertex_count, 1, 0, 0);

  filtering_ = false;
}

void Viewport::ResetTransform() {
  /* The origin scrolls the content of a viewport inside of its rect without
     moving the rect, so the offset the node places its children with has it
     subtracted.

     A viewport without an effect draws those children straight into the render
     target, at the rect placed by the parent hierarchy. One whose region an
     effect filters draws them into a texture of the size of the region instead,
     which starts at its own corner, so the rect itself drops out of the offset
     and only the origin scrolls inside of the texture, see BeginFilter(). */
  const glm::vec3 offset =
      effect_ ? glm::vec3(static_cast<float>(-origin_.x),
                          static_cast<float>(-origin_.y), 0.0f)
              : glm::vec3(static_cast<float>(rect_->data.x - origin_.x),
                          static_cast<float>(rect_->data.y - origin_.y), 0.0f);

  /* The z of the offset has to be spelled out: a glm vector left to its default
     constructor keeps whatever the memory held, and a non-zero z of the
     position travels down the transform chain of the children as the z of every
     vertex they emit, which the projection of the engine then clips away. */
  Attr_Position(MakeRefCounted<Vector3>(offset));
}

void Viewport::CreateEffectBindings() {
  const wgpu::RenderPipeline& pipeline =
      ShaderSet::Get().state.tint_blends.at(BLEND_NORMAL);
  wgpu::Device device = GPUDevice::Get().device();

  wgpu::BufferDescriptor object_desc;
  object_desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  object_desc.size = sizeof(ObjectData);
  object_uniform_ = device.CreateBuffer(&object_desc);

  /* The quad of an effect is emitted in the pixels of the render target, so the
     transform it is drawn with, and therefore the model matrix of the object
     set, is the identity. Set 1 of the tint pipeline is bound with a dynamic
     offset, so the group of it covers this one matrix and the draw binds it at
     offset zero; a group which carries the buffer without the dynamic offset is
     not compatible with the layout and the device rejects the draw. */
  const ObjectData object_data = {glm::mat4(1.0f)};
  GPUDevice::Get().queue().WriteBuffer(object_uniform_, 0, &object_data,
                                       sizeof(object_data));

  WBufferSet object_binding(object_uniform_);
  object_binding.size = sizeof(ObjectData);
  object_group_ =
      CreateWGroup(pipeline.GetBindGroupLayout(1), {{0, object_binding}});

  wgpu::BufferDescriptor tint_desc;
  tint_desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  tint_desc.size = sizeof(TintBase::TintParam);
  tint_uniform_ = device.CreateBuffer(&tint_desc);

  // Written during the post processing stage of every frame which runs an
  // effect, so the contents of the buffer are not staged here
  tint_group_ = CreateWGroup(pipeline.GetBindGroupLayout(3),
                             {{0, WBufferSet(tint_uniform_)}});
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
