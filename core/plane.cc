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

#include "core/plane.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

#include "core/gpu.h"
#include "core/logger.h"
#include "core/pipeline.h"
#include "core/uniform.h"

namespace urge {

namespace {

/*! The smallest determinant the linear part of the transform of a plane may
    have. The tiles of a plane are placed with the inverse of that part, which a
    transform that collapses the plane onto a line or a point does not have --
    and such a plane has no surface to show a tile on either. */
constexpr float kMinLinearDeterminant = 1e-6f;

/*! The smallest zoom a plane is drawn with. A zoom of zero divides the texture
    coordinate of every pixel of the plane by it, and a tile scaled below this
    is smaller than a millionth of a pixel. */
constexpr float kMinZoom = 1e-6f;

}  // namespace

Plane::Plane(RefPtr<Viewport> viewport)
    : Node(viewport, ZValue()),
      color_(MakeRefCounted<Color>()),
      tone_(MakeRefCounted<Tone>()) {
  Node::SetupTrait(this);
  CreateEffectBindings();
}

Plane::~Plane() {
  Disposable::Dispose();
}

ATTR_DEF(Plane, RefPtr<Viewport>, Viewport) {
  auto parent_value = Node::Attr_Parent(value);
  if (parent_value.has_value()) {
    auto parent = *parent_value;
    Viewport* viewport = parent ? parent->TryCast<Viewport>() : nullptr;
    return RefPtr<Viewport>(viewport);
  } else {
    return std::nullopt;
  }
}

ATTR_DEF(Plane, RefPtr<Bitmap>, Bitmap) {
  if (value.has_value()) {
    bitmap_ = *value;
    return std::nullopt;
  } else {
    return bitmap_;
  }
}

ATTR_DEF(Plane, int32_t, OX) {
  if (value.has_value()) {
    ox_ = *value;
    return std::nullopt;
  } else {
    return ox_;
  }
}

ATTR_DEF(Plane, int32_t, OY) {
  if (value.has_value()) {
    oy_ = *value;
    return std::nullopt;
  } else {
    return oy_;
  }
}

ATTR_DEF(Plane, float, ZoomX) {
  if (value.has_value()) {
    zoom_x_ = *value;
    return std::nullopt;
  } else {
    return zoom_x_;
  }
}

ATTR_DEF(Plane, float, ZoomY) {
  if (value.has_value()) {
    zoom_y_ = *value;
    return std::nullopt;
  } else {
    return zoom_y_;
  }
}

ATTR_DEF(Plane, int32_t, Opacity) {
  if (value.has_value()) {
    opacity_ = std::clamp<int32_t>(*value, 0, 255);
    return std::nullopt;
  } else {
    return opacity_;
  }
}

ATTR_DEF(Plane, int32_t, BlendType) {
  if (value.has_value()) {
    /* The blend type indexes the pipeline states of the plane shader, so it is
       kept inside the range of the blend types the engine knows. */
    blend_type_ = std::clamp(*value, static_cast<int32_t>(BLEND_NONE),
                             static_cast<int32_t>(BLEND_SUBTRACT));
    return std::nullopt;
  } else {
    return blend_type_;
  }
}

ATTR_DEF(Plane, RefPtr<Color>, Color) {
  if (value.has_value()) {
    color_->Set(*value);
    return std::nullopt;
  } else {
    return color_;
  }
}

ATTR_DEF(Plane, RefPtr<Tone>, Tone) {
  if (value.has_value()) {
    tone_->Set(*value);
    return std::nullopt;
  } else {
    return tone_;
  }
}

void Plane::DisposeObject() {
  Node::DisposeObject();

  bitmap_.reset();
  tint_uniform_ = nullptr;
  tint_group_ = nullptr;
}

bool Plane::Prepare(DrawParam param) {
  primitive_slot_ = {};

  // A plane without a bitmap, or with one which was disposed, draws nothing
  if (!Disposable::Check(bitmap_))
    return false;

  // The geometry goes into the vertex batch of the frame, see QuadVertexManager
  primitive_slot_ = EmitGeometryInternal(*param->vertices, param);
  // A plane whose bitmap, zoom or transform leaves nothing to tile
  if (!primitive_slot_.count)
    return false;

  UniformManager& uniforms = UniformManager::Get();

  /* The quad of a plane is emitted in the pixels of the render target and the
     tiles follow from the texture coordinates of its corners, so the object
     transform it is drawn with is the identity and the transform of the node
     hierarchy is not part of it, see EmitGeometryInternal(). */
  const ObjectData object_data = {glm::mat4(1.0f)};
  object_slot_ = uniforms.object_uniforms().Acquire(object_data);

  /* The color and the tone of the plane travel in a buffer of its own, which is
     written before the command buffer of this frame is submitted. */
  PlaneBase::PlaneParam plane_param = {};
  plane_param.blend_color = color_->Normalize();
  plane_param.blend_tone = tone_->Normalize();
  GPUDevice::Get().queue().WriteBuffer(tint_uniform_, 0, &plane_param,
                                       sizeof(plane_param));

  return object_slot_.chunk != UniformBlockPool::kInvalidChunk;
}

bool Plane::DoDraw(DrawParam param) {
  UniformManager& uniforms = UniformManager::Get();
  const UniformBlockPool::Chunk& object_chunk =
      uniforms.object_uniforms().chunk(object_slot_.chunk);

  param->pass.SetPipeline(ShaderSet::Get().state.plane_blends.at(
      static_cast<BlendType>(blend_type_)));
  param->pass.SetBindGroup(0, param->scene, 0, nullptr);
  param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);
  param->pass.SetBindGroup(2, bitmap_->texture_group(), 0, nullptr);
  param->pass.SetBindGroup(3, tint_group_, 0, nullptr);
  // The batch of the frame holds the vertices, this draw takes its own range
  param->pass.SetVertexBuffer(0, param->vertices->buffer(), 0, WGPU_WHOLE_SIZE);
  param->pass.Draw(primitive_slot_.count, 1, primitive_slot_.first, 0);

  return false;
}

PrimitiveEmitter::Slot Plane::EmitGeometryInternal(PrimitiveEmitter& primitive,
                                                   DrawParam param) {
  const glm::ivec2 texture_size = bitmap_->size();
  // A bitmap without a pixel holds no tile a plane could repeat
  if (texture_size.x <= 0 || texture_size.y <= 0)
    return {};

  // A zoom of zero collapses every tile of the bitmap onto a line
  if (std::abs(zoom_x_) < kMinZoom || std::abs(zoom_y_) < kMinZoom)
    return {};

  /* A plane covers whatever it is drawn into: the quad is emitted over the
     whole render target and a viewport the plane belongs to clips it to its
     rect, which is what makes a plane fill the region of that viewport. */
  const RectI region(param->target->size());
  if (!region())
    return {};

  /* The root point of the plane on the render target: the position the node
     hierarchy placed the plane at, scrolled by the origin of the plane. The
     tiles of the bitmap are anchored there and repeat over the whole target
     from it, which is what a plane of RGSS is: a scrolling tiled surface. */
  const glm::mat4 transform = world_transform();
  const glm::vec2 root =
      ExtractPosition(transform) -
      glm::vec2(static_cast<float>(ox_), static_cast<float>(oy_));

  /* A pixel of the target reads the tile at the local point which the transform
     of the node hierarchy maps the offset of that pixel from the root to, so
     the scale and the rotation of the node act on the surface of the plane as
     well as its position does. */
  const glm::mat2 linear(transform);
  if (std::abs(glm::determinant(linear)) < kMinLinearDeterminant)
    return {};
  const glm::mat2 inverse_linear = glm::inverse(linear);

  /* The size one tile of the bitmap has on the render target: the zoom scales
     the bitmap, and the texture coordinate of a tile runs from zero to one over
     it, which is what the plane shader repeats. */
  const glm::vec2 tile_size(static_cast<float>(texture_size.x) * zoom_x_,
                            static_cast<float>(texture_size.y) * zoom_y_);

  /* The blend state of the engine and the contents of a bitmap store
     premultiplied alpha, so the opacity of a plane scales all four channels of
     the vertex color instead of the alpha channel alone. */
  const glm::vec4 color(static_cast<float>(opacity_) / 255.0f);

  // The corners of the quad in the order the emitter collects a quad in
  const auto emit_corner = [&](float x, float y) {
    const glm::vec2 corner(x, y);
    primitive.Texcoord2f(inverse_linear * (corner - root) / tile_size);
    primitive.Vertex2f(corner);
  };

  primitive.BeginQuad().Color4f(color);
  emit_corner(static_cast<float>(region.x), static_cast<float>(region.y));
  emit_corner(static_cast<float>(region.x + region.width),
              static_cast<float>(region.y));
  emit_corner(static_cast<float>(region.x),
              static_cast<float>(region.y + region.height));
  emit_corner(static_cast<float>(region.x + region.width),
              static_cast<float>(region.y + region.height));

  return primitive.End();
}

void Plane::CreateEffectBindings() {
  const wgpu::RenderPipeline& pipeline =
      ShaderSet::Get().state.plane_blends.at(BLEND_NORMAL);

  wgpu::BufferDescriptor tint_desc;
  tint_desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  tint_desc.size = sizeof(PlaneBase::PlaneParam);
  tint_uniform_ = GPUDevice::Get().device().CreateBuffer(&tint_desc);

  /* The tint of a plane is written during the prepare stage of every frame the
     plane is drawn in, so the contents of the buffer are not staged here. */
  tint_group_ = CreateWGroup(pipeline.GetBindGroupLayout(3),
                             {{0, WBufferSet(tint_uniform_)}});
}

}  // namespace urge