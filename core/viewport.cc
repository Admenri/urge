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

void Viewport::DisposeObject() {
  Node::DisposeObject();

  pingpong_.reset();
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
     to be placed the way the parent hierarchy placed the node, and the origin
     is added back because the transform of the node has it subtracted. */
  RectI self_scissor = rect_->data;
  const glm::ivec2 world_position = ExtractPosition(world_transform());
  self_scissor.x = world_position.x + origin_.x;
  self_scissor.y = world_position.y + origin_.y;

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

void Viewport::PostDraw(DrawParam param) {
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
    AcquirePingPong(viewport_region);
    wgpu::TexelCopyTextureInfo source;
    source.texture = param->target->texture();
    source.origin.x = viewport_region.x;
    source.origin.y = viewport_region.y;
    wgpu::TexelCopyTextureInfo destination;
    destination.texture = pingpong_->texture();
    wgpu::Extent3D copy_size;
    copy_size.width = viewport_region.width;
    copy_size.height = viewport_region.height;
    param->command.CopyTextureToTexture(&source, &destination, &copy_size);

    // The batch of the frame is already written, this quad needs its own
    // emitter
    primitive_.EmitQuad(
        viewport_region,
        MakeNorm(RectI(viewport_region.Size()), pingpong_->size()),
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
    param->pass.SetBindGroup(2, pingpong_->texture_group(), 0, nullptr);
    param->pass.SetBindGroup(3, tint_group_, 0, nullptr);
    param->pass.SetVertexBuffer(0, primitive_.buffer(), 0, WGPU_WHOLE_SIZE);
    param->pass.Draw(vertex_count, 1, 0, 0);
  }

  // Restore scissor stack
  param->scissors.pop();
  const RectI current_scissor = param->scissors.top();
  param->pass.SetScissorRect(current_scissor.x, current_scissor.y,
                             current_scissor.width, current_scissor.height);
}  // namespace urge

void Viewport::ResetTransform() {
  /* The z of the offset has to be spelled out: a glm vector left to its default
     constructor keeps whatever the memory held, and a non-zero z of the
     position travels down the transform chain of the children as the z of every
     vertex they emit, which the projection of the engine then clips away. */
  const glm::vec3 offset(static_cast<float>(rect_->data.x - origin_.x),
                         static_cast<float>(rect_->data.y - origin_.y), 0.0f);
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

void Viewport::AcquirePingPong(const RectI& region) {
  if (pingpong_ && pingpong_->size().x >= region.width &&
      pingpong_->size().y >= region.height)
    return;

  const int32_t width =
      std::max(region.width, pingpong_ ? pingpong_->size().x : 0);
  const int32_t height =
      std::max(region.height, pingpong_ ? pingpong_->size().y : 0);
  pingpong_ = MakeRefCounted<Bitmap>(width, height);
}

}  // namespace urge
