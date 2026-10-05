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
#include <cstddef>
#include <cstdint>
#include <span>

#include "core/definition.h"
#include "core/exception.h"
#include "core/logger.h"

namespace urge {

namespace {

//! Converts an 8-bit color component to the normalized [0, 1] range.
constexpr float UnpackColor8(std::uint8_t value) {
  return static_cast<float>(value) / 255.f;
}

//! Returns the component of a packed 0xAABBGGRR color at the specified shift.
constexpr std::uint8_t UnpackPackedColor(std::uint32_t argb, int shift) {
  return static_cast<std::uint8_t>((argb >> shift) & 0xFFu);
}

}  // namespace

PrimitiveEmitter::PrimitiveEmitter() : PrimitiveEmitter(kDefaultCapacity) {}

PrimitiveEmitter::PrimitiveEmitter(std::size_t capacity) {
  Reserve(capacity);
}

PrimitiveEmitter& PrimitiveEmitter::Begin(PrimitiveType type) {
  if (active_)
    throw Exception(
        Exception::kRGSSError,
        "primitive emitter already has an active batch, call End() first.");

  type_ = type;
  active_ = true;
  pending_size_ = 0;
  // The batch starts where the storage ends, so earlier batches are kept
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

PrimitiveEmitter& PrimitiveEmitter::Color4u(std::uint8_t r,
                                            std::uint8_t g,
                                            std::uint8_t b,
                                            std::uint8_t a) {
  return Color4f(UnpackColor8(r), UnpackColor8(g), UnpackColor8(b),
                 UnpackColor8(a));
}

PrimitiveEmitter& PrimitiveEmitter::Color4u(std::uint32_t argb) {
  return Color4u(UnpackPackedColor(argb, 0), UnpackPackedColor(argb, 8),
                 UnpackPackedColor(argb, 16), UnpackPackedColor(argb, 24));
}

PrimitiveEmitter& PrimitiveEmitter::ClearColor() {
  color_ = glm::vec4(1.f, 1.f, 1.f, 1.f);
  return *this;
}

PrimitiveEmitter& PrimitiveEmitter::Texcoord2f(float u, float v) {
  return Texcoord2f(glm::vec2(u, v));
}

PrimitiveEmitter& PrimitiveEmitter::Texcoord2f(const glm::vec2& texcoord) {
  texcoord_ = texcoord;
  return *this;
}

PrimitiveEmitter& PrimitiveEmitter::ClearTexcoord() {
  texcoord_ = glm::vec2(0.f, 0.f);
  return *this;
}

PrimitiveEmitter& PrimitiveEmitter::Vertex2f(float x, float y) {
  return EmitVertex(x, y, 0.f, 1.f);
}

PrimitiveEmitter& PrimitiveEmitter::Vertex2f(const glm::vec2& position) {
  return Vertex2f(position.x, position.y);
}

PrimitiveEmitter& PrimitiveEmitter::Vertex3f(float x, float y, float z) {
  return EmitVertex(x, y, z, 1.f);
}

PrimitiveEmitter& PrimitiveEmitter::Vertex3f(const glm::vec3& position) {
  return Vertex3f(position.x, position.y, position.z);
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
  if (!active_)
    throw Exception(
        Exception::kRGSSError,
        "primitive emitter has no active batch, call Begin() first.");

  const glm::vec4 position(x, y, z, w);

  switch (type_) {
    case PrimitiveType::kTriangle:
      PushVertex(position);
      break;

    case PrimitiveType::kQuad:
      // The given corners are top-left, top-right, bottom-left and
      // bottom-right. Two of them (top-right and bottom-left) are shared by
      // both triangles, so the six vertices can only be emitted once all four
      // corners are known, see ExpandQuad().
      if (pending_size_ < kQuadCorners) {
        pending_corners_[pending_size_] = position;
        pending_texcoords_[pending_size_] = texcoord_;
        pending_colors_[pending_size_] = color_;
        ++pending_size_;
      }
      if (pending_size_ == kQuadCorners) {
        ExpandQuad();
        pending_size_ = 0;
      }
      return *this;
  }

  return *this;
}

void PrimitiveEmitter::ExpandQuad() {
  // Copy the corners first, the pending buffer is reused in case another quad
  // follows in the same batch.
  const VertexData top_left{pending_corners_[0], pending_texcoords_[0],
                            pending_colors_[0]};
  const VertexData top_right{pending_corners_[1], pending_texcoords_[1],
                             pending_colors_[1]};
  const VertexData bottom_left{pending_corners_[2], pending_texcoords_[2],
                               pending_colors_[2]};
  const VertexData bottom_right{pending_corners_[3], pending_texcoords_[3],
                                pending_colors_[3]};

  // The two triangles of the quad: top left, top right, bottom left, followed
  // by the bottom left corner again, the bottom right corner and the repeated
  // top right corner. Every vertex keeps the state of its corner, so the
  // shared corners interpolate like the outer ones.
  vertices_.push_back(top_left);
  vertices_.push_back(top_right);
  vertices_.push_back(bottom_left);
  vertices_.push_back(bottom_left);
  vertices_.push_back(bottom_right);
  vertices_.push_back(top_right);
}

PrimitiveEmitter& PrimitiveEmitter::Rect(float x,
                                         float y,
                                         float width,
                                         float height) {
  return Rect(x, y, width, height, RectF(0.f, 0.f, 1.f, 1.f));
}

PrimitiveEmitter& PrimitiveEmitter::Rect(const RectF& rect) {
  return Rect(rect.x, rect.y, rect.width, rect.height);
}

PrimitiveEmitter& PrimitiveEmitter::Rect(float x,
                                         float y,
                                         float width,
                                         float height,
                                         const RectF& texcoord) {
  return Rect(RectF(x, y, width, height), texcoord);
}

PrimitiveEmitter& PrimitiveEmitter::Rect(const RectF& rect,
                                         const RectF& texcoord) {
  if (!active_)
    BeginQuad();

  const float right = rect.x + rect.width;
  const float bottom = rect.y + rect.height;
  const float right_u = texcoord.x + texcoord.width;
  const float bottom_v = texcoord.y + texcoord.height;

  // The quad expansion of EmitVertex() repeats the shared corners and derives
  // the bottom right texture coordinate, so the four corners are enough.
  texcoord_ = glm::vec2(texcoord.x, texcoord.y);
  Vertex2f(rect.x, rect.y);
  texcoord_ = glm::vec2(right_u, texcoord.y);
  Vertex2f(right, rect.y);
  texcoord_ = glm::vec2(texcoord.x, bottom_v);
  Vertex2f(rect.x, bottom);
  texcoord_ = glm::vec2(right_u, bottom_v);
  Vertex2f(right, bottom);
  return *this;
}

PrimitiveEmitter& PrimitiveEmitter::EmitQuad(const RectF& rect,
                                             const RectF& texcoord,
                                             const glm::vec4& top_left,
                                             const glm::vec4& top_right,
                                             const glm::vec4& bottom_left,
                                             const glm::vec4& bottom_right) {
  const float left = rect.x;
  const float top = rect.y;
  const float right = rect.x + rect.width;
  const float bottom = rect.y + rect.height;
  const float right_u = texcoord.x + texcoord.width;
  const float bottom_v = texcoord.y + texcoord.height;

  return BeginQuad()
      .Texcoord2f(texcoord.x, texcoord.y)
      .Color4f(top_left)
      .Vertex2f(left, top)
      .Texcoord2f(right_u, texcoord.y)
      .Color4f(top_right)
      .Vertex2f(right, top)
      .Texcoord2f(texcoord.x, bottom_v)
      .Color4f(bottom_left)
      .Vertex2f(left, bottom)
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

PrimitiveEmitter& PrimitiveEmitter::EmitTriangle(const glm::vec2& position0,
                                                 const glm::vec2& position1,
                                                 const glm::vec2& position2,
                                                 const glm::vec2& texcoord0,
                                                 const glm::vec2& texcoord1,
                                                 const glm::vec2& texcoord2,
                                                 const glm::vec4& color0,
                                                 const glm::vec4& color1,
                                                 const glm::vec4& color2) {
  return BeginTriangle()
      .Texcoord2f(texcoord0)
      .Color4f(color0)
      .Vertex2f(position0)
      .Texcoord2f(texcoord1)
      .Color4f(color1)
      .Vertex2f(position1)
      .Texcoord2f(texcoord2)
      .Color4f(color2)
      .Vertex2f(position2);
}

PrimitiveEmitter& PrimitiveEmitter::EmitTriangle(const glm::vec2& position0,
                                                 const glm::vec2& position1,
                                                 const glm::vec2& position2) {
  return BeginTriangle().Vertex2f(position0).Vertex2f(position1).Vertex2f(
      position2);
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
  // A batch left open is finished by the upload
  End();

  const std::size_t count = vertices_.size();
  if (!count)
    return 0;

  const std::size_t bytes = count * sizeof(VertexData);
  EnsureVertexBuffer(bytes);
  GPUDevice::Get().queue().WriteBuffer(vertex_buffer_, 0, vertices_.data(),
                                       bytes);

  // The vertices live in the buffer from here on, the storage is reused
  vertices_.clear();
  batch_first_ = 0;
  return static_cast<std::uint32_t>(count);
}

void PrimitiveEmitter::EnsureVertexBuffer(std::size_t bytes) {
  const std::uint64_t current = buffer_size();
  if (current >= bytes)
    return;

  // The device validates against the limits it was created with
  wgpu::Limits limits = {};
  g_device.GetLimits(&limits);

  // A batch which does not fit is not cut short, its draw would read garbage
  if (bytes > limits.maxBufferSize)
    throw Exception(
        Exception::kGPUError,
        "a primitive emitter batch of {} bytes does not fit into the "
        "{} byte buffers of this device.",
        bytes, limits.maxBufferSize);

  const std::uint64_t size = std::min(
      std::max<std::uint64_t>(current * 2, bytes), limits.maxBufferSize);

  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.usage = wgpu::BufferUsage::Vertex | wgpu::BufferUsage::CopyDst;
  buffer_desc.size = size;
  vertex_buffer_ = g_device.CreateBuffer(&buffer_desc);

  if (!vertex_buffer_)
    throw Exception(Exception::kGPUError,
                    "the device rejected a vertex buffer of {} bytes.", size);
}

void PrimitiveEmitter::PushVertex(const glm::vec4& position) {
  VertexData vertex;
  vertex.position = position;
  vertex.texcoord = texcoord_;
  vertex.color = color_;
  vertices_.push_back(vertex);
}

/* ----- QuadVertexManager ----- */

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
