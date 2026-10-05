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

#include "core/primitive.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "core/logger.h"

namespace urge {

PrimitiveEmitter::PrimitiveEmitter() : PrimitiveEmitter(kDefaultCapacity) {}

PrimitiveEmitter::PrimitiveEmitter(std::size_t capacity) {
  Reserve(capacity);
}

// Misuse has no exception path: it is reported and, in release, ignored.
PrimitiveEmitter& PrimitiveEmitter::Begin(PrimitiveType type) {
  if (active_) {
    LOGGER_ERROR("PrimitiveEmitter::Begin: a batch is already active");
    assert(false && "a primitive emitter batch is already active");
  }

  type_ = type;
  active_ = true;
  pending_size_ = 0;
  batch_first_ = vertices_.size();
  color_ = glm::vec4(1.f, 1.f, 1.f, 1.f);
  texcoord_ = glm::vec2(0.f, 0.f);
  return *this;
}

PrimitiveEmitter& PrimitiveEmitter::BeginTriangle() {
  return Begin(PrimitiveType::kTriangle);
}

PrimitiveEmitter& PrimitiveEmitter::BeginQuad() {
  return Begin(PrimitiveType::kQuad);
}

PrimitiveEmitter::Slot PrimitiveEmitter::End() {
  active_ = false;

  Slot slot;
  slot.first = static_cast<std::uint32_t>(batch_first_);
  slot.count = static_cast<std::uint32_t>(vertices_.size() - batch_first_);
  return slot;
}

PrimitiveEmitter& PrimitiveEmitter::Color4f(float r,
                                            float g,
                                            float b,
                                            float a) {
  return Color4f(glm::vec4(r, g, b, a));
}

PrimitiveEmitter& PrimitiveEmitter::Color4f(const glm::vec4& color) {
  color_ = color;
  return *this;
}

PrimitiveEmitter& PrimitiveEmitter::Texcoord2f(float u, float v) {
  return Texcoord2f(glm::vec2(u, v));
}

PrimitiveEmitter& PrimitiveEmitter::Texcoord2f(const glm::vec2& texcoord) {
  texcoord_ = texcoord;
  return *this;
}

PrimitiveEmitter& PrimitiveEmitter::Vertex2f(float x, float y) {
  return EmitVertex(x, y, 0.f, 1.f);
}

PrimitiveEmitter& PrimitiveEmitter::Vertex2f(const glm::vec2& position) {
  return Vertex2f(position.x, position.y);
}

PrimitiveEmitter& PrimitiveEmitter::Vertex4f(float x,
                                             float y,
                                             float z,
                                             float w) {
  return EmitVertex(x, y, z, w);
}

PrimitiveEmitter& PrimitiveEmitter::Vertex4f(const glm::vec4& position) {
  return Vertex4f(position.x, position.y, position.z, position.w);
}

PrimitiveEmitter& PrimitiveEmitter::EmitVertex(float x,
                                               float y,
                                               float z,
                                               float w) {
  if (!active_) {
    LOGGER_ERROR("PrimitiveEmitter::EmitVertex: no active batch");
    assert(false && "primitive emitter has no active batch");
    return *this;
  }

  const glm::vec4 position(x, y, z, w);
  if (type_ == PrimitiveType::kTriangle) {
    PushVertex(position);
    return *this;
  }

  // A quad only emits once its four corners are known: the top right and the
  // bottom left corner are shared by both triangles, see ExpandQuad().
  pending_corners_[pending_size_] = position;
  pending_texcoords_[pending_size_] = texcoord_;
  pending_colors_[pending_size_] = color_;
  if (++pending_size_ == kQuadCorners) {
    ExpandQuad();
    pending_size_ = 0;
  }
  return *this;
}

PrimitiveEmitter& PrimitiveEmitter::Rect(const RectF& rect,
                                         const RectF& texcoord) {
  if (!active_)
    BeginQuad();

  const float right = rect.x + rect.width;
  const float bottom = rect.y + rect.height;
  const float right_u = texcoord.x + texcoord.width;
  const float bottom_v = texcoord.y + texcoord.height;

  return Texcoord2f(texcoord.x, texcoord.y)
      .Vertex2f(rect.x, rect.y)
      .Texcoord2f(right_u, texcoord.y)
      .Vertex2f(right, rect.y)
      .Texcoord2f(texcoord.x, bottom_v)
      .Vertex2f(rect.x, bottom)
      .Texcoord2f(right_u, bottom_v)
      .Vertex2f(right, bottom);
}

PrimitiveEmitter& PrimitiveEmitter::EmitQuad(const RectF& rect,
                                             const RectF& texcoord,
                                             const glm::vec4& top_left,
                                             const glm::vec4& top_right,
                                             const glm::vec4& bottom_left,
                                             const glm::vec4& bottom_right) {
  const float right = rect.x + rect.width;
  const float bottom = rect.y + rect.height;
  const float right_u = texcoord.x + texcoord.width;
  const float bottom_v = texcoord.y + texcoord.height;

  return BeginQuad()
      .Texcoord2f(texcoord.x, texcoord.y)
      .Color4f(top_left)
      .Vertex2f(rect.x, rect.y)
      .Texcoord2f(right_u, texcoord.y)
      .Color4f(top_right)
      .Vertex2f(right, rect.y)
      .Texcoord2f(texcoord.x, bottom_v)
      .Color4f(bottom_left)
      .Vertex2f(rect.x, bottom)
      .Texcoord2f(right_u, bottom_v)
      .Color4f(bottom_right)
      .Vertex2f(right, bottom);
}

PrimitiveEmitter& PrimitiveEmitter::EmitQuad(const RectF& rect,
                                             const RectF& texcoord,
                                             const glm::vec4& color) {
  return EmitQuad(rect, texcoord, color, color, color, color);
}

PrimitiveEmitter& PrimitiveEmitter::EmitQuad(const RectF& rect,
                                             const glm::vec4& color) {
  return EmitQuad(rect, RectF(), color);
}

void PrimitiveEmitter::Reserve(std::size_t capacity) {
  vertices_.reserve(capacity);
}

void PrimitiveEmitter::Clear() {
  vertices_.clear();
  pending_size_ = 0;
  batch_first_ = 0;
}

void PrimitiveEmitter::Reset() {
  Clear();
  active_ = false;
  type_ = PrimitiveType::kTriangle;
  color_ = glm::vec4(1.f, 1.f, 1.f, 1.f);
  texcoord_ = glm::vec2(0.f, 0.f);
  vertex_buffer_ = nullptr;
}

std::uint32_t PrimitiveEmitter::Upload() {
  // A batch left open is finished by the upload.
  End();

  const std::size_t count = vertices_.size();
  if (!count)
    return 0;

  const std::size_t bytes = count * sizeof(VertexData);
  if (!EnsureVertexBuffer(bytes)) {
    vertices_.clear();
    batch_first_ = 0;
    return 0;
  }

  g_queue.WriteBuffer(vertex_buffer_, 0, vertices_.data(), bytes);

  // The vertices live in the buffer from here on, the storage is reused.
  vertices_.clear();
  batch_first_ = 0;
  return static_cast<std::uint32_t>(count);
}

void PrimitiveEmitter::PushVertex(const glm::vec4& position) {
  VertexData vertex;
  vertex.position = position;
  vertex.texcoord = texcoord_;
  vertex.color = color_;
  vertices_.push_back(vertex);
}

// The six vertices of the two triangles of a quad: top left, top right,
// bottom left, bottom left, bottom right, top right.
void PrimitiveEmitter::ExpandQuad() {
  const VertexData top_left{pending_corners_[0], pending_texcoords_[0],
                            pending_colors_[0]};
  const VertexData top_right{pending_corners_[1], pending_texcoords_[1],
                             pending_colors_[1]};
  const VertexData bottom_left{pending_corners_[2], pending_texcoords_[2],
                               pending_colors_[2]};
  const VertexData bottom_right{pending_corners_[3], pending_texcoords_[3],
                                pending_colors_[3]};

  vertices_.push_back(top_left);
  vertices_.push_back(top_right);
  vertices_.push_back(bottom_left);
  vertices_.push_back(bottom_left);
  vertices_.push_back(bottom_right);
  vertices_.push_back(top_right);
}

bool PrimitiveEmitter::EnsureVertexBuffer(std::size_t bytes) {
  if (buffer_size() >= bytes)
    return true;

  wgpu::Limits limits = {};
  g_device.GetLimits(&limits);
  if (bytes > limits.maxBufferSize) {
    LOGGER_ERROR("PrimitiveEmitter: a {} byte batch exceeds the {} byte limit",
                 bytes, limits.maxBufferSize);
    assert(false && "a primitive emitter batch does not fit into a buffer");
    return false;
  }

  const std::uint64_t size = std::min(
      std::max<std::uint64_t>(buffer_size() * 2, bytes), limits.maxBufferSize);

  wgpu::BufferDescriptor desc;
  desc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
  desc.size = size;
  vertex_buffer_ = g_device.CreateBuffer(&desc);

  if (!vertex_buffer_) {
    LOGGER_ERROR("PrimitiveEmitter: the device rejected a {} byte buffer",
                 size);
    assert(false && "the device rejected a vertex buffer");
    return false;
  }
  return true;
}

QuadVertexManager::QuadVertexManager() {
  emitter_.Reserve(4096);

  LOGGER_DEBUG("frame vertex batch: reserved {} vertices of {} bytes",
               emitter_.capacity(), sizeof(VertexData));
}

QuadVertexManager::~QuadVertexManager() {
  emitter_.Reset();
}

void QuadVertexManager::BeginFrame() {
  emitter_.Clear();
  vertex_count_ = 0;
}

std::uint32_t QuadVertexManager::Upload() {
  vertex_count_ = emitter_.Upload();

  LOGGER_TRACE("frame vertex batch: {} vertices of {} bytes in one upload",
               vertex_count_, emitter_.buffer_size());
  return vertex_count_;
}

}  // namespace urge
