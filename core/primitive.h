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
#include "core/device.h"
#include "core/object.h"

namespace urge {

/**
\brief Vertex layout shared by every primitive emitted by PrimitiveEmitter.

One 40 byte stride: \c position as RGBA32Float at location 0 (offset 0),
\c texcoord as RG32Float at location 1 (offset 16) and \c color as RGBA32Float
at location 2 (offset 24).
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
with a handful of calls. A quad takes its four corners from four Vertex*() calls
and expands them into six vertices (two triangles), see EmitVertex();
PrimitiveType::kTriangle picks up its three vertices from the next three
Vertex*() calls, every further triplet starts another triangle. Emitting more
vertices than the reserved capacity keeps working, the storage just grows like
any other std::vector.

\code
PrimitiveEmitter emitter(64);
emitter.BeginQuad()
    .Color4f(1.f, 0.f, 0.f)
    .Texcoord2f(0.f, 1.f)
    .Vertex2f(10.f, 20.f);   // ... and the other three corners
PrimitiveEmitter::Slot slot = emitter.End();
emitter.Upload();
// The six vertices of the quad are read from emitter.buffer() from here on
pass.SetVertexBuffer(0, emitter.buffer(), 0, WGPU_WHOLE_SIZE);
pass.Draw(slot.count, 1, slot.first, 0);
\endcode

The vertices are one append-only storage: Begin() starts a batch at the end of
it and End() answers the range that batch emitted, but neither drops what an
earlier batch wrote. That is what lets several batches share one upload -- the
frame batch of QuadVertexManager appends one batch per drawable and writes all
of them with a single Upload() -- and an emitter which uploads after every batch
(Bitmap, PresentInternal) never grows. Upload() creates or grows the vertex
buffer of the emitter and copies the storage into it with one queue write, so
from then on a draw reads the vertices from buffer() and addresses its own with
the Slot its End() answered. Only that upload and Clear()/Reset() drop the
storage, the latter two release the buffer as well.
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

  //! A range of the vertices of the emitter, in the order they were emitted.
  //! The same indices hold in the vertex buffer of the emitter after Upload(),
  //! so the range is what addresses a draw: Draw(count, 1, first, 0).
  struct Slot {
    //! The index of the first vertex of the batch.
    std::uint32_t first = 0;
    //! The number of vertices of the batch.
    std::uint32_t count = 0;
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
  \remarks The batch starts at the end of the storage, the vertices of an
  earlier batch are kept: only Clear()/Reset() and Upload() drop them. Resets
  the color and the texture coordinate state. Throws Exception if a batch is
  active.
  */
  PrimitiveEmitter& Begin(PrimitiveType type = PrimitiveType::kTriangle);
  //! Starts a triangle batch, see Begin().
  PrimitiveEmitter& BeginTriangle();
  //! Starts a quad batch, see Begin().
  PrimitiveEmitter& BeginQuad();

  /**
  \brief Ends the active batch and answers the range of vertices it emitted.
  \remarks Returns an empty slot when the batch emitted nothing, which is what a
  caller whose geometry collapsed has to test. The range stays valid until the
  vertices are uploaded or cleared.
  */
  Slot End();

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

  //! Returns the vertices which are waiting for an Upload(), i.e. everything
  //! emitted since the last Upload()/Clear()/Reset().
  std::span<const VertexData> vertices() const { return vertices_; }
  //! Returns the number of vertices which are waiting for an Upload().
  std::size_t size() const { return vertices_.size(); }
  //! Returns true if no vertex is waiting for an Upload().
  bool empty() const { return vertices_.empty(); }
  //! Returns true while a batch is active, i.e. between Begin() and End().
  bool active() const { return active_; }
  //! Returns the primitive type of the active/last batch.
  PrimitiveType primitive_type() const { return type_; }

  //! Reserves storage for \c capacity vertices without changing the state.
  void Reserve(std::size_t capacity);
  //! Returns the vertex storage capacity, in vertices.
  std::size_t capacity() const { return vertices_.capacity(); }
  //! Drops the vertices waiting for an Upload(), the color/texcoord state and
  //! the vertex buffer are kept.
  void Clear();
  //! Drops the vertices waiting for an Upload(), restores every state to its
  //! default and releases the vertex buffer.
  void Reset();

  /* ----- Vertex buffer ----- */

  /**
  \brief Ends an active batch, writes the pending vertices into the vertex
  buffer of the emitter and drops them.
  \return The number of vertices written, zero when the storage is empty.

  \remarks The buffer is created for the vertices, or grown when it is smaller
  than them, and it is kept afterwards, so a frame which stays at the size of
  the previous one writes into the same buffer. Throws Exception if the vertices
  do not fit into one buffer of the device.
  */
  std::uint32_t Upload();
  //! Returns the buffer Upload() writes, which is empty before the first one.
  const wgpu::Buffer& buffer() const { return vertex_buffer_; }
  //! Returns the size of the vertex buffer, in bytes, zero before the first
  //! Upload().
  std::uint64_t buffer_size() const {
    return vertex_buffer_ ? vertex_buffer_.GetSize() : 0;
  }

 private:
  //! Number of corners a quad is built from.
  static constexpr std::size_t kQuadCorners = 4;

  //! Appends one vertex built from the current color/texcoord state.
  void PushVertex(const glm::vec4& position);
  //! Expands the pending quad corners into the six vertices of two triangles.
  void ExpandQuad();
  //! Creates the vertex buffer for \c bytes, or grows it when it is smaller.
  void EnsureVertexBuffer(std::size_t bytes);

  //! Vertices emitted since the last Upload(), Clear() or Reset(), the storage
  //! of every batch which was not uploaded yet.
  std::vector<VertexData> vertices_;
  //! Index of the first vertex of the batch started by the last Begin().
  std::size_t batch_first_ = 0;
  //! The vertex buffer the last Upload() wrote, created and grown on demand.
  wgpu::Buffer vertex_buffer_;
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

/**
\brief The vertex batch of one frame: every drawable which emits a quad per
frame appends into one emitter, so the frame is one upload into one vertex
buffer instead of one buffer and one upload per drawable.

A drawable emits into emitter() during the prepare stage and keeps the Slot its
End() answered; its draw stage turns that slot into a draw against buffer():

\code
slot_ = param->vertices->End();                          // prepare stage
...
param->pass.SetVertexBuffer(0, param->vertices->buffer(), 0, WGPU_WHOLE_SIZE);
param->pass.Draw(slot_.count, 1, slot_.first, 0);        // draw stage
\endcode

Driven by Node::Render, which opens and closes one frame of it around the
prepare stage -- like the uniform pools of UniformManager, and for the same
reason: a queue write is ordered before the submissions that follow it, so the
vertices have to be uploaded after the last drawable staged them and before the
command buffer that draws them is submitted.
*/
class QuadVertexManager : public Singleton<QuadVertexManager> {
 public:
  QuadVertexManager();
  ~QuadVertexManager();

  //! Opens a frame: the vertices of the previous one are dropped, the vertex
  //! buffer and its capacity are kept.
  void BeginFrame();

  //! The emitter the drawables of the frame append to, see DrawContext.
  PrimitiveEmitter& emitter() { return emitter_; }
  /*! Writes the vertices every drawable of the frame appended, in one upload.
      \return The number of vertices the frame emitted. */
  std::uint32_t Upload();

  //! The vertex buffer of the frame, which every draw of it binds at vertex
  //! slot 0.
  const wgpu::Buffer& buffer() { return emitter_.buffer(); }
  //! The number of vertices the last frame emitted, zero before the first one.
  std::uint32_t vertex_count() const { return vertex_count_; }
  //! The size of the vertex buffer of the frame, in bytes.
  std::uint64_t buffer_size() { return emitter_.buffer_size(); }

 private:
  //! The vertices of the frame, in one storage and one buffer.
  PrimitiveEmitter emitter_;
  //! The number of vertices the last frame emitted.
  std::uint32_t vertex_count_ = 0;
};

}  // namespace urge
