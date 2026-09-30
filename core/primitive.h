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
#include <span>
#include <vector>

#include "core/common.h"

namespace urge {

/**
\brief Vertex layout shared by every primitive emitted by PrimitiveEmitter.

The members are arranged for LLGL::VertexAttribute (40 bytes stride):
- \c position: \c Format::RGBA32Float, offset 0, location 0
- \c texcoord: \c Format::RG32Float, offset 16, location 1
- \c color: \c Format::RGBA32Float, offset 24, location 2

\remarks The vector and rectangle aliases of core/common.h are spelled with
their namespace qualifier: MSVC 19.51 (Visual Studio 2026) fails to resolve an
unqualified alias of an enclosing namespace inside a class member declaration.
*/
struct VertexData {
  //! Vertex position in pixels, the z/w components default to 0/1.
  glm::vec4 position = glm::vec4(0.f, 0.f, 0.f, 1.f);
  //! Texture coordinate, normalized to [0, 1].
  glm::vec2 texcoord = glm::vec2(0.f, 0.f);
  //! Vertex color, normalized to [0, 1].
  glm::vec4 color = glm::vec4(1.f, 1.f, 1.f, 1.f);
};

static_assert(sizeof(VertexData) == 40, "unexpected VertexData padding");
static_assert(offsetof(VertexData, texcoord) == 16,
              "unexpected texcoord offset");
static_assert(offsetof(VertexData, color) == 24, "unexpected color offset");

/**
\brief Immediate-mode mesh builder for simple shapes (triangle, quad, ...).

The emitter keeps the color and the texture coordinate as a state which is
applied to every vertex emitted afterwards, so a whole batch can be described
with a handful of calls:

\code
render::PrimitiveEmitter emitter(64);
emitter.Begin(render::PrimitiveEmitter::PrimitiveType::kQuad)
    .Color4f(1.f, 0.f, 0.f)
    .Texcoord2f(0.f, 1.f)
    .Vertex2f(10.f, 20.f)
    .Texcoord2f(1.f, 1.f)
    .Vertex2f(74.f, 20.f)
    .Texcoord2f(0.f, 0.f)
    .Vertex2f(10.f, 84.f)
    .Texcoord2f(1.f, 0.f)
    .Vertex2f(74.f, 84.f);
std::span<const render::VertexData> vertices = emitter.End();
\endcode

Begin() starts a batch and End() commits it and returns the emitted vertices,
which stay valid until the next Begin()/Clear()/Reset() call. A quad takes its
four corners from four Vertex*() calls and expands them into six vertices (two
triangles), see EmitVertex(). Emitting more vertices than the reserved capacity
keeps working, the storage just grows like any other std::vector.

PrimitiveType::kTriangle picks up its three vertices from the next three
Vertex*() calls, every further triplet starts another triangle: a batch emits
as many primitives as it has complete vertex groups.
*/
class PrimitiveEmitter {
 public:
  //! Primitive kinds which can be emitted by one batch.
  enum class PrimitiveType {
    kTriangle,  //!< Emits one triangle per three vertices.
    kQuad,      //!< Emits one quad per four vertices (two triangles).
  };

  //! Vertex order of a quad in the order of the four corners which have to be
  //! emitted: top-left, top-right, bottom-left, bottom-right. The texture
  //! coordinates of a quad follow a top-left origin, i.e. v grows downwards.
  //! The corners are collected until the fourth one arrived, only a complete
  //! quad emits vertices, see EmitVertex().
  enum class QuadVertex {
    kTopLeft = 0,
    kTopRight = 1,
    kBottomLeft = 2,
    kBottomRight = 3,
  };

  //! Default capacity of the vertex storage, in vertices (about 40 KiB).
  static constexpr std::size_t kDefaultCapacity = 1024;

  //! Constructs an emitter with kDefaultCapacity reserved vertices.
  PrimitiveEmitter();
  //! Constructs an emitter and reserves storage for \c capacity vertices.
  explicit PrimitiveEmitter(std::size_t capacity);

  /* ----- Batch control ----- */

  /**
  \brief Starts a batch of the specified primitive type.
  \remarks Clears the vertices of the previous batch and resets the color and
  texture coordinate state. Throws Exception if a batch is active.
  */
  PrimitiveEmitter& Begin(PrimitiveType type = PrimitiveType::kTriangle);
  //! Starts a triangle batch, see Begin().
  PrimitiveEmitter& BeginTriangle();
  //! Starts a quad batch, see Begin().
  PrimitiveEmitter& BeginQuad();

  /**
  \brief Ends the active batch and returns the vertices emitted since Begin().
  \remarks Logs a warning and returns an empty span if no batch is active. The
  returned span is invalidated by the next state changing emitter call.
  */
  std::span<const VertexData> End();

  /* ----- State applied to the following vertices ----- */

  //! Sets the vertex color from normalized [0, 1] components.
  PrimitiveEmitter& Color4f(float r, float g, float b, float a = 1.f);
  //! Sets the vertex color from normalized [0, 1] components.
  PrimitiveEmitter& Color4f(const glm::vec4& color);
  //! Sets the vertex color from 8-bit components, 0 to 255.
  PrimitiveEmitter& Color4u(std::uint8_t r,
                            std::uint8_t g,
                            std::uint8_t b,
                            std::uint8_t a = 0xFF);
  //! Sets the vertex color from a packed 0xAABBGGRR value.
  PrimitiveEmitter& Color4u(std::uint32_t argb);
  //! Restores the default white color.
  PrimitiveEmitter& ClearColor();

  //! Sets the texture coordinate of the following vertices.
  PrimitiveEmitter& Texcoord2f(float u, float v);
  //! Sets the texture coordinate of the following vertices.
  PrimitiveEmitter& Texcoord2f(const glm::vec2& texcoord);
  //! Restores the default (0, 0) texture coordinate.
  PrimitiveEmitter& ClearTexcoord();

  /* ----- Vertex output ----- */

  //! Emits a vertex at (x, y, 0), see EmitVertex().
  PrimitiveEmitter& Vertex2f(float x, float y);
  //! Emits a vertex at (x, y, 0), see EmitVertex().
  PrimitiveEmitter& Vertex2f(const glm::vec2& position);
  //! Emits a vertex, see EmitVertex().
  PrimitiveEmitter& Vertex3f(float x, float y, float z);
  //! Emits a vertex, see EmitVertex().
  PrimitiveEmitter& Vertex3f(const glm::vec3& position);
  //! Emits a vertex, see EmitVertex().
  PrimitiveEmitter& Vertex4f(float x, float y, float z, float w);
  //! Emits a vertex, see EmitVertex().
  PrimitiveEmitter& Vertex4f(const glm::vec4& position);

  /**
  \brief Emits a vertex with the current color and texture coordinate state.

  For PrimitiveType::kTriangle this appends one vertex. For
  PrimitiveType::kQuad the call describes one corner of the quad in QuadVertex
  order, including its color and texture coordinate. The six vertices of the
  quad are emitted as soon as the fourth corner arrived: both triangles reuse
  the top right and the bottom left corner, every vertex keeps the state of the
  corner it belongs to, so a quad can be emitted with a color or texture
  coordinate gradient.

  \remarks Throws Exception if no batch is active.
  */
  PrimitiveEmitter& EmitVertex(float x, float y, float z, float w = 1.f);

  /* ----- Shape helpers ----- */

  /**
  \brief Emits a quad over the specified rectangle.
  \remarks Uses the current color and the current texture coordinate as the top
  left corner of the quad: the other corners are the rectangle corners mapped to
  [0, 1] relative to it.
  */
  PrimitiveEmitter& Rect(float x, float y, float width, float height);
  //! Emits a quad over the specified rectangle, see Rect().
  PrimitiveEmitter& Rect(const RectF& rect);
  //! Emits a quad with the specified texture coordinate rectangle, see Rect().
  PrimitiveEmitter& Rect(float x,
                         float y,
                         float width,
                         float height,
                         const RectF& texcoord);
  //! Emits a quad with the specified texture coordinate rectangle, see Rect().
  PrimitiveEmitter& Rect(const RectF& rect, const RectF& texcoord);

  /* ----- Built-in shapes ----- */

  /**
  \brief Starts a quad batch and emits it over the specified rectangle with the
  specified texture coordinate rectangle and one color per corner.
  \param[in] top_left Color of the top left corner, the other colors belong to
  the corners in QuadVertex order.
  \remarks This is the shortcut for BeginQuad() followed by the four Vertex2f()
  calls of the corners, so a quad can be emitted with a color or texture
  coordinate gradient in one call.
  */
  PrimitiveEmitter& EmitQuad(const RectF& rect,
                             const RectF& texcoord,
                             const glm::vec4& top_left,
                             const glm::vec4& top_right,
                             const glm::vec4& bottom_left,
                             const glm::vec4& bottom_right);
  //! Emits a quad with one color for every corner, see EmitQuad().
  PrimitiveEmitter& EmitQuad(const RectF& rect,
                             const RectF& texcoord,
                             const glm::vec4& color);
  //! Emits a quad without texture coordinates, see EmitQuad().
  PrimitiveEmitter& EmitQuad(const RectF& rect, const glm::vec4& color);

  /**
  \brief Starts a triangle batch and emits the specified triangle with one
  texture coordinate and one color per point.
  \remarks This is the shortcut for BeginTriangle() followed by the three
  Vertex2f() calls of the points.
  */
  PrimitiveEmitter& EmitTriangle(const glm::vec2& position0,
                                 const glm::vec2& position1,
                                 const glm::vec2& position2,
                                 const glm::vec2& texcoord0,
                                 const glm::vec2& texcoord1,
                                 const glm::vec2& texcoord2,
                                 const glm::vec4& color0,
                                 const glm::vec4& color1,
                                 const glm::vec4& color2);
  //! Emits a triangle with the current color and texture coordinate state, see
  //! EmitTriangle().
  PrimitiveEmitter& EmitTriangle(const glm::vec2& position0,
                                 const glm::vec2& position1,
                                 const glm::vec2& position2);

  /* ----- Storage ----- */

  //! Returns the vertices emitted by the active/finished batch.
  std::span<const VertexData> vertices() const { return vertices_; }
  //! Returns the number of emitted vertices.
  std::size_t size() const { return vertices_.size(); }
  //! Returns true if the batch emitted no vertex.
  bool empty() const { return vertices_.empty(); }
  //! Returns true while a batch is active, i.e. between Begin() and End().
  bool active() const { return active_; }
  //! Returns the primitive type of the active/last batch.
  PrimitiveType primitive_type() const { return type_; }

  //! Reserves storage for \c capacity vertices without changing the state.
  void Reserve(std::size_t capacity);
  //! Returns the vertex storage capacity, in vertices.
  std::size_t capacity() const { return vertices_.capacity(); }
  //! Drops the emitted vertices, the color/texcoord state is kept.
  void Clear();
  //! Drops the emitted vertices and restores every state to its default.
  void Reset();

 private:
  //! Number of corners a quad is built from.
  static constexpr std::size_t kQuadCorners = 4;

  //! Appends one vertex built from the current color/texcoord state.
  void PushVertex(const glm::vec4& position);
  //! Expands the pending quad corners into the six vertices of two triangles.
  void ExpandQuad();

  //! Vertices emitted since the last Begin().
  std::vector<VertexData> vertices_;
  //! Primitive type of the active/last batch.
  PrimitiveType type_ = PrimitiveType::kTriangle;
  //! State applied to the next emitted vertex.
  glm::vec4 color_ = glm::vec4(1.f, 1.f, 1.f, 1.f);
  //! State applied to the next emitted vertex.
  glm::vec2 texcoord_ = glm::vec2(0.f, 0.f);
  //! Corners of the quad which is currently being collected.
  glm::vec4 pending_corners_[kQuadCorners];
  //! Texture coordinates belonging to the collected quad corners.
  glm::vec2 pending_texcoords_[kQuadCorners];
  //! Colors belonging to the collected quad corners.
  glm::vec4 pending_colors_[kQuadCorners];
  //! Number of collected quad corners.
  std::size_t pending_size_ = 0;
  //! True while a batch is active, i.e. between Begin() and End().
  bool active_ = false;
};

}  // namespace urge
