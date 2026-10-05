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

#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/common.h"
#include "core/device.h"
#include "core/object.h"

namespace urge {

// Vertex layout shared by every primitive: position (location 0), texcoord
// (location 1) and color (location 2), 40 bytes in total.
struct VertexData {
  glm::vec4 position = glm::vec4(0.f, 0.f, 0.f, 1.f);
  glm::vec2 texcoord = glm::vec2(0.f, 0.f);
  glm::vec4 color = glm::vec4(1.f, 1.f, 1.f, 1.f);
};

static_assert(sizeof(VertexData) == 40, "unexpected VertexData padding");
static_assert(offsetof(VertexData, texcoord) == 16,
              "unexpected texcoord offset");
static_assert(offsetof(VertexData, color) == 24, "unexpected color offset");

// Primitive Emitter: assemble vertex data into a drawcall.
class PrimitiveEmitter {
 public:
  enum class PrimitiveType {
    kTriangle,
    kQuad,
  };

  // One quad takes its four corners from four EmitVertex() calls in this order.
  enum class QuadVertex {
    kTopLeft = 0,
    kTopRight = 1,
    kBottomLeft = 2,
    kBottomRight = 3,
  };

  // A range of the emitted vertices, which addresses a draw after Upload():
  // Draw(count, 1, first, 0).
  struct Slot {
    std::uint32_t first = 0;
    std::uint32_t count = 0;
  };

  static constexpr std::size_t kDefaultCapacity = 1024;

  PrimitiveEmitter();
  explicit PrimitiveEmitter(std::size_t capacity);

  // Batch control
  PrimitiveEmitter& Begin(PrimitiveType type = PrimitiveType::kTriangle);
  PrimitiveEmitter& BeginTriangle();
  PrimitiveEmitter& BeginQuad();
  Slot End();

  // State applied to the following vertices
  PrimitiveEmitter& Color4f(float r, float g, float b, float a = 1.f);
  PrimitiveEmitter& Color4f(const glm::vec4& color);
  PrimitiveEmitter& Texcoord2f(float u, float v);
  PrimitiveEmitter& Texcoord2f(const glm::vec2& texcoord);

  // Vertex output
  PrimitiveEmitter& Vertex2f(float x, float y);
  PrimitiveEmitter& Vertex2f(const glm::vec2& position);
  PrimitiveEmitter& Vertex4f(float x, float y, float z, float w);
  PrimitiveEmitter& Vertex4f(const glm::vec4& position);
  PrimitiveEmitter& EmitVertex(float x, float y, float z, float w = 1.f);

  // Shape helpers
  PrimitiveEmitter& Rect(const RectF& rect, const RectF& texcoord);
  PrimitiveEmitter& EmitQuad(const RectF& rect,
                             const RectF& texcoord,
                             const glm::vec4& top_left,
                             const glm::vec4& top_right,
                             const glm::vec4& bottom_left,
                             const glm::vec4& bottom_right);
  PrimitiveEmitter& EmitQuad(const RectF& rect,
                             const RectF& texcoord,
                             const glm::vec4& color);
  PrimitiveEmitter& EmitQuad(const RectF& rect, const glm::vec4& color);

  // Storage
  std::size_t size() const { return vertices_.size(); }
  void Reserve(std::size_t capacity);
  std::size_t capacity() const { return vertices_.capacity(); }
  void Clear();
  void Reset();

  // Vertex buffer
  std::uint32_t Upload();
  const wgpu::Buffer& buffer() const { return vertex_buffer_; }
  std::uint64_t buffer_size() const {
    return vertex_buffer_ ? vertex_buffer_.GetSize() : 0;
  }

 private:
  static constexpr std::size_t kQuadCorners = 4;

  void PushVertex(const glm::vec4& position);
  void ExpandQuad();
  bool EnsureVertexBuffer(std::size_t bytes);

  std::vector<VertexData> vertices_;
  std::size_t batch_first_ = 0;
  wgpu::Buffer vertex_buffer_;
  PrimitiveType type_ = PrimitiveType::kTriangle;
  glm::vec4 color_ = glm::vec4(1.f, 1.f, 1.f, 1.f);
  glm::vec2 texcoord_ = glm::vec2(0.f, 0.f);
  glm::vec4 pending_corners_[kQuadCorners];
  glm::vec2 pending_texcoords_[kQuadCorners];
  glm::vec4 pending_colors_[kQuadCorners];
  std::size_t pending_size_ = 0;
  bool active_ = false;
};

// Quad Vertex Manager: the frame vertex batch every drawable appends to, so a
// frame is one upload into one buffer instead of one per drawable.
class QuadVertexManager : public Singleton<QuadVertexManager> {
 public:
  QuadVertexManager();
  ~QuadVertexManager();

  void BeginFrame();

  PrimitiveEmitter& emitter() { return emitter_; }
  std::uint32_t Upload();

  const wgpu::Buffer& buffer() { return emitter_.buffer(); }
  std::uint32_t vertex_count() const { return vertex_count_; }
  std::uint64_t buffer_size() { return emitter_.buffer_size(); }

 private:
  PrimitiveEmitter emitter_;
  std::uint32_t vertex_count_ = 0;
};

}  // namespace urge
