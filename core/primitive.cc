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

#include <cstddef>
#include <cstdint>
#include <span>

#include "core/definition.h"
#include "core/exception.h"

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
    throw urge::Exception(
        urge::Exception::kRGSSError,
        "primitive emitter already has an active batch, call End() first.");

  type_ = type;
  active_ = true;
  pending_size_ = 0;
  vertices_.clear();
  color_ = urge::Vec4(1.f, 1.f, 1.f, 1.f);
  texcoord_ = urge::Vec2(0.f, 0.f);
  return *this;
}

PrimitiveEmitter& PrimitiveEmitter::BeginTriangle() {
  return Begin(PrimitiveType::kTriangle);
}

PrimitiveEmitter& PrimitiveEmitter::BeginQuad() {
  return Begin(PrimitiveType::kQuad);
}

std::span<const VertexData> PrimitiveEmitter::End() {
  if (!active_)
    return {};

  active_ = false;
  return vertices_;
}

PrimitiveEmitter& PrimitiveEmitter::Color4f(float r,
                                            float g,
                                            float b,
                                            float a) {
  return Color4f(urge::Vec4(r, g, b, a));
}

PrimitiveEmitter& PrimitiveEmitter::Color4f(const urge::Vec4& color) {
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
  color_ = urge::Vec4(1.f, 1.f, 1.f, 1.f);
  return *this;
}

PrimitiveEmitter& PrimitiveEmitter::Texcoord2f(float u, float v) {
  return Texcoord2f(urge::Vec2(u, v));
}

PrimitiveEmitter& PrimitiveEmitter::Texcoord2f(const urge::Vec2& texcoord) {
  texcoord_ = texcoord;
  return *this;
}

PrimitiveEmitter& PrimitiveEmitter::ClearTexcoord() {
  texcoord_ = urge::Vec2(0.f, 0.f);
  return *this;
}

PrimitiveEmitter& PrimitiveEmitter::Vertex2f(float x, float y) {
  return EmitVertex(x, y, 0.f, 1.f);
}

PrimitiveEmitter& PrimitiveEmitter::Vertex2f(const urge::Vec2& position) {
  return Vertex2f(position.x, position.y);
}

PrimitiveEmitter& PrimitiveEmitter::Vertex3f(float x, float y, float z) {
  return EmitVertex(x, y, z, 1.f);
}

PrimitiveEmitter& PrimitiveEmitter::Vertex3f(const urge::Vec3& position) {
  return Vertex3f(position.x, position.y, position.z);
}

PrimitiveEmitter& PrimitiveEmitter::Vertex4f(float x,
                                             float y,
                                             float z,
                                             float w) {
  return EmitVertex(x, y, z, w);
}

PrimitiveEmitter& PrimitiveEmitter::Vertex4f(const urge::Vec4& position) {
  return Vertex4f(position.x, position.y, position.z, position.w);
}

PrimitiveEmitter& PrimitiveEmitter::EmitVertex(float x,
                                               float y,
                                               float z,
                                               float w) {
  if (!active_)
    throw urge::Exception(
        urge::Exception::kRGSSError,
        "primitive emitter has no active batch, call Begin() first.");

  const urge::Vec4 position(x, y, z, w);

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
  return Rect(x, y, width, height, urge::RectF(0.f, 0.f, 1.f, 1.f));
}

PrimitiveEmitter& PrimitiveEmitter::Rect(const urge::RectF& rect) {
  return Rect(rect.x, rect.y, rect.width, rect.height);
}

PrimitiveEmitter& PrimitiveEmitter::Rect(float x,
                                         float y,
                                         float width,
                                         float height,
                                         const urge::RectF& texcoord) {
  return Rect(urge::RectF(x, y, width, height), texcoord);
}

PrimitiveEmitter& PrimitiveEmitter::Rect(const urge::RectF& rect,
                                         const urge::RectF& texcoord) {
  if (!active_)
    BeginQuad();

  const float right = rect.x + rect.width;
  const float bottom = rect.y + rect.height;
  const float right_u = texcoord.x + texcoord.width;
  const float bottom_v = texcoord.y + texcoord.height;

  // The quad expansion of EmitVertex() repeats the shared corners and derives
  // the bottom right texture coordinate, so the four corners are enough.
  texcoord_ = urge::Vec2(texcoord.x, texcoord.y);
  Vertex2f(rect.x, rect.y);
  texcoord_ = urge::Vec2(right_u, texcoord.y);
  Vertex2f(right, rect.y);
  texcoord_ = urge::Vec2(texcoord.x, bottom_v);
  Vertex2f(rect.x, bottom);
  texcoord_ = urge::Vec2(right_u, bottom_v);
  Vertex2f(right, bottom);
  return *this;
}

PrimitiveEmitter& PrimitiveEmitter::EmitQuad(const urge::RectF& rect,
                                             const urge::RectF& texcoord,
                                             const urge::Vec4& top_left,
                                             const urge::Vec4& top_right,
                                             const urge::Vec4& bottom_left,
                                             const urge::Vec4& bottom_right) {
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

PrimitiveEmitter& PrimitiveEmitter::EmitQuad(const urge::RectF& rect,
                                             const urge::RectF& texcoord,
                                             const urge::Vec4& color) {
  return EmitQuad(rect, texcoord, color, color, color, color);
}

PrimitiveEmitter& PrimitiveEmitter::EmitQuad(const urge::RectF& rect,
                                             const urge::Vec4& color) {
  return EmitQuad(rect, urge::RectF(), color);
}

PrimitiveEmitter& PrimitiveEmitter::EmitTriangle(const urge::Vec2& position0,
                                                 const urge::Vec2& position1,
                                                 const urge::Vec2& position2,
                                                 const urge::Vec2& texcoord0,
                                                 const urge::Vec2& texcoord1,
                                                 const urge::Vec2& texcoord2,
                                                 const urge::Vec4& color0,
                                                 const urge::Vec4& color1,
                                                 const urge::Vec4& color2) {
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

PrimitiveEmitter& PrimitiveEmitter::EmitTriangle(const urge::Vec2& position0,
                                                 const urge::Vec2& position1,
                                                 const urge::Vec2& position2) {
  return BeginTriangle().Vertex2f(position0).Vertex2f(position1).Vertex2f(
      position2);
}

void PrimitiveEmitter::Reserve(std::size_t capacity) {
  vertices_.reserve(capacity);
}

void PrimitiveEmitter::Clear() {
  vertices_.clear();
  pending_size_ = 0;
}

void PrimitiveEmitter::Reset() {
  Clear();
  active_ = false;
  type_ = PrimitiveType::kTriangle;
  color_ = urge::Vec4(1.f, 1.f, 1.f, 1.f);
  texcoord_ = urge::Vec2(0.f, 0.f);
}

void PrimitiveEmitter::PushVertex(const urge::Vec4& position) {
  VertexData vertex;
  vertex.position = position;
  vertex.texcoord = texcoord_;
  vertex.color = color_;
  vertices_.push_back(vertex);
}

}  // namespace urge
