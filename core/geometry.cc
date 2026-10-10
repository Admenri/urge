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

#include "core/geometry.h"

#include <algorithm>
#include <cstdint>

#include "core/device.h"
#include "core/gpu_utils.h"
#include "core/logger.h"
#include "core/pipeline.h"
#include "core/uniform.h"

namespace urge {

namespace {

constexpr int32_t kDefaultTextureSize = 1;

class DefaultTexture {
 public:
  static const wgpu::BindGroup& Get() {
    static DefaultTexture instance;
    return instance.group_;
  }

 private:
  DefaultTexture() {
    wgpu::TextureDescriptor texture_desc;
    texture_desc.usage =
        wgpu::TextureUsage::TextureBinding | wgpu::TextureUsage::CopyDst;
    texture_desc.dimension = wgpu::TextureDimension::e2D;
    texture_desc.size.width = kDefaultTextureSize;
    texture_desc.size.height = kDefaultTextureSize;
    texture_desc.format = wgpu::TextureFormat::RGBA8Unorm;
    texture_ = GPUDevice::Get().device().CreateTexture(&texture_desc);

    const std::uint8_t white[4] = {0xFF, 0xFF, 0xFF, 0xFF};

    wgpu::TexelCopyTextureInfo destination;
    destination.texture = texture_;

    wgpu::TexelCopyBufferLayout layout;
    layout.bytesPerRow = sizeof(white);
    layout.rowsPerImage = kDefaultTextureSize;

    wgpu::Extent3D size;
    size.width = kDefaultTextureSize;
    size.height = kDefaultTextureSize;
    size.depthOrArrayLayers = 1;
    GPUDevice::Get().queue().WriteTexture(&destination, white, sizeof(white),
                                          &layout, &size);

    wgpu::SamplerDescriptor sampler_desc;
    sampler_desc.addressModeU = wgpu::AddressMode::ClampToEdge;
    sampler_desc.addressModeV = wgpu::AddressMode::ClampToEdge;
    sampler_desc.addressModeW = wgpu::AddressMode::ClampToEdge;
    sampler_desc.magFilter = wgpu::FilterMode::Nearest;
    sampler_desc.minFilter = wgpu::FilterMode::Nearest;
    sampler_ = GPUDevice::Get().device().CreateSampler(&sampler_desc);

    const wgpu::RenderPipeline& pipeline =
        ShaderSet::Get().state.geometry.geometry_blends.at(BLEND_NORMAL);
    group_ = util::CreateBindGroup(
        pipeline.GetBindGroupLayout(2),
        {{0, util::TextureViewSet(texture_.CreateView(nullptr))},
         {1, util::SamplerSet(sampler_)}});

    LOGGER_DEBUG("geometry fallback texture: {}x{} white texel",
                 kDefaultTextureSize, kDefaultTextureSize);
  }

  wgpu::Texture texture_;
  wgpu::Sampler sampler_;
  wgpu::BindGroup group_;
};

}  // namespace

Geometry::Geometry(RefPtr<Viewport> viewport) : Node(viewport, ZValue()) {
  Node::SetupTrait(this);
}

Geometry::~Geometry() {
  Disposable::Dispose();
}

void Geometry::SetPosition(int32_t triangle,
                           int32_t point,
                           RefPtr<Vector3> position) {
  Disposable::Guard();

  if (triangle < 0 || triangle >= static_cast<int32_t>(data_.size()) ||
      point < 0 || point >= 3)
    throw Exception(Exception::kRGSSError, "invalid range");

  if (!position)
    throw Exception(Exception::kRGSSError, "invalid data.");

  auto& v = data_[triangle];
  v.vertex[point].position = glm::vec4(position->data, 1.0f);
}

void Geometry::SetTexcoord(int32_t triangle,
                           int32_t point,
                           RefPtr<Vector2> texcoord) {
  Disposable::Guard();

  if (triangle < 0 || triangle >= static_cast<int32_t>(data_.size()) ||
      point < 0 || point >= 3)
    throw Exception(Exception::kRGSSError, "invalid range");

  if (!texcoord)
    throw Exception(Exception::kRGSSError, "invalid data.");

  auto& v = data_[triangle];
  v.vertex[point].texcoord = texcoord->data;
}

void Geometry::SetColor(int32_t triangle, int32_t point, RefPtr<Color> color) {
  Disposable::Guard();

  if (triangle < 0 || triangle >= static_cast<int32_t>(data_.size()) ||
      point < 0 || point >= 3)
    throw Exception(Exception::kRGSSError, "invalid range");

  if (!color)
    throw Exception(Exception::kRGSSError, "invalid data.");

  const glm::vec4 normalized = color->Normalize();
  const float alpha = normalized.a;
  auto& v = data_[triangle];
  v.vertex[point].color = glm::vec4(normalized.r * alpha, normalized.g * alpha,
                                    normalized.b * alpha, alpha);
}

ATTR_DEF(Geometry, RefPtr<Viewport>, Viewport) {
  auto parent_value = Node::Attr_Parent(value);
  if (parent_value.has_value()) {
    auto parent = *parent_value;
    Viewport* viewport = parent ? parent->TryCast<Viewport>() : nullptr;
    return RefPtr<Viewport>(viewport);
  } else {
    return std::nullopt;
  }
}

ATTR_DEF(Geometry, int32_t, Capacity) {
  if (value.has_value()) {
    if (*value < 0)
      throw Exception(Exception::kRGSSError, "invalid capacity value.");

    data_.resize(*value);
    return std::nullopt;
  } else {
    return static_cast<int32_t>(data_.size());
  }
}

ATTR_DEF(Geometry, RefPtr<Bitmap>, Bitmap) {
  if (value.has_value()) {
    bitmap_ = *value;
    return std::nullopt;
  } else {
    return bitmap_;
  }
}

ATTR_DEF(Geometry, int32_t, BlendType) {
  if (value.has_value()) {
    blend_type_ = std::clamp(*value, static_cast<int32_t>(BLEND_NONE),
                             static_cast<int32_t>(BLEND_SUBTRACT));
    return std::nullopt;
  } else {
    return blend_type_;
  }
}

ATTR_DEF(Geometry, RefPtr<Effect>, Effect) {
  if (value.has_value()) {
    effect_ = *value;
    return std::nullopt;
  } else {
    return effect_;
  }
}

void Geometry::DisposeObject() {
  Node::DisposeObject();

  bitmap_.reset();
  effect_.reset();
  data_.clear();
}

bool Geometry::Prepare(DrawParam param) {
  primitive_slot_ = {};

  primitive_slot_ = EmitGeometryInternal(*param->vertices);

  if (!primitive_slot_.count)
    return false;

  UniformManager& uniforms = UniformManager::Get();
  ObjectData object_data;
  object_data.model_mat = world_transform();
  object_slot_ = uniforms.object_uniforms().Acquire(object_data);

  DefaultTexture::Get();

  return object_slot_.chunk != UniformBlockPool::kInvalidChunk;
}

bool Geometry::DoDraw(DrawParam param) {
  UniformManager& uniforms = UniformManager::Get();
  const UniformBlockPool::Chunk& object_chunk =
      uniforms.object_uniforms().chunk(object_slot_.chunk);

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

  param->pass.SetPipeline(ShaderSet::Get().state.geometry.geometry_blends.at(
      static_cast<BlendType>(blend_type_)));

  param->pass.SetBindGroup(0, param->scene, 0, nullptr);

  param->pass.SetBindGroup(1, object_chunk.group, 1, &object_slot_.offset);

  const wgpu::BindGroup texture_group = Disposable::Check(bitmap_)
                                            ? bitmap_->texture_group()
                                            : DefaultTexture::Get();
  param->pass.SetBindGroup(2, texture_group, 0, nullptr);

  param->pass.SetVertexBuffer(0, param->vertices->buffer(), 0, WGPU_WHOLE_SIZE);
  param->pass.Draw(primitive_slot_.count, 1, primitive_slot_.first, 0);

  return false;
}

PrimitiveEmitter::Slot Geometry::EmitGeometryInternal(
    PrimitiveEmitter& emitter) {
  if (data_.empty())
    return {};

  emitter.BeginTriangle();
  for (const TriangleData& triangle : data_) {
    for (const VertexData& vertex : triangle.vertex) {
      emitter.Color4f(vertex.color);
      emitter.Texcoord2f(vertex.texcoord);
      emitter.Vertex4f(vertex.position);
    }
  }

  return emitter.End();
}

}  // namespace urge
