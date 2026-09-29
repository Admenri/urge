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
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

#include "webgpu/webgpu_cpp.hpp"

#include "core/object.h"
#include "core/pipeline.h"

namespace urge {

/**
\brief Bulk uniform buffer which carries one kind of per-drawable data of a
frame, addressed with a dynamic offset.

A pipeline whose set is built with a dynamic offset -- which the object set and
the sprite parameter set of the sprite pipeline are, see Pipeline::Pipeline() --
binds a whole byte range of a buffer instead of the data of a single drawable:
the bind group of the set is made once for the buffer and every draw chooses what
it reads with a dynamic offset. This pool is what turns that into one buffer for
a whole scene instead of one buffer per drawable.

The byte range of a chunk is cut into slots whose distance is
minUniformBufferOffsetAlignment and whose size is at least one element, so a slot
is a legal dynamic offset and two slots can never overlap.

A frame goes through BeginFrame(), then one Acquire() per drawable, then Flush(),
all of them before the command buffer holding the draws is submitted.
UniformManager drives that for every pool of the engine at once, and Node::Render
calls it around the prepare stage of the tree.

A slot of the current frame stays valid until the next BeginFrame(), which is
what lets the draw stage read the slot its prepare stage was handed. A chunk
fills up sequentially and a chunk a frame exhausts is followed by another one, so
the pool grows with a scene which needs more slots than fit into one uniform
buffer binding.
*/
class UniformBlockPool {
 public:
  //! The chunk index of a slot which does not belong to the pool.
  static constexpr uint32_t kInvalidChunk = 0xFFFFFFFFu;

  //! One buffer of the pool together with the bind group which covers it.
  struct Chunk {
    //! The buffer holding the slots, of chunk_size() bytes.
    wgpu::Buffer buffer;
    /*! The bind group of the set, created once for the whole buffer. Its size
        is element_size(), which is the size of one slot of this pool. */
    wgpu::BindGroup group;
    //! The number of slots the buffer holds.
    uint32_t capacity = 0;
    //! The number of slots handed out in the current frame, reset by
    //! BeginFrame().
    uint32_t used = 0;
  };

  //! The place of one element inside the pool.
  struct Slot {
    //! The entry of chunks() which owns the slot, kInvalidChunk when the pool
    //! could not accept the element.
    uint32_t chunk = kInvalidChunk;
    //! The byte offset of the slot inside the buffer of its chunk, which is the
    //! dynamic offset the set has to be bound with. It is always a whole
    //! multiple of slot_stride() and therefore of the alignment of the device.
    uint32_t offset = 0;
  };

  /**
  \brief Creates a pool of buffers which hold elements of \p element_size bytes.
  \param[in] layout The bind group layout of the set which is built with a
  dynamic offset, i.e. what Pipeline::Pipeline() was told. A bind group the pool
  makes from it fits every pipeline the layout belongs to.
  \param[in] element_size The size of one element, which is also the size of the
  buffer binding of a slot. It has at least to be the minimum binding size the
  shader declares for the set, which is the size of the uniform block as std140
  lays it out and normally is the size of the struct itself: a pool whose
  elements are smaller than that makes every draw fail validation.
  \param[in] name A label for the log, the engine owns more than one pool.
  \remarks The slot distance and the chunk size follow the limits of the device,
  which are the stricter ones of the limits it was requested with: a chunk holds
  as many slots of minUniformBufferOffsetAlignment as fit into
  maxUniformBufferBindingSize, up to kMaxChunkSize.
  */
  UniformBlockPool(wgpu::BindGroupLayout layout,
                   uint32_t element_size,
                   std::string_view name = {});
  ~UniformBlockPool();

  //! Starts a frame: every chunk loses its slots and the buffers are reused, so
  //! the pool only allocates again when a frame needs more slots than before.
  void BeginFrame();

  /**
  \brief Reserves a slot and stages \p size bytes at \p data in it.
  \return The slot which the draw of the element is bound with; its chunk is
  kInvalidChunk when the pool could not grow, and Acquire() throws when \p size
  does not fit into element_size().
  */
  Slot Acquire(const void* data, uint32_t size);

  //! Reserves a slot and stages \p data in it, see Acquire().
  template <typename Ty>
  Slot Acquire(const Ty& data) {
    static_assert(std::is_trivially_copyable_v<Ty>,
                  "a slot only holds plain data");
    return Acquire(&data, sizeof(Ty));
  }

  /*! Writes the slots of every chunk used in this frame into its buffer.
      \remarks Has to run before the command buffer holding the draws is
      submitted, because a queue write is ordered before the submissions which
      follow it. */
  void Flush();

  //! The label of this pool.
  const std::string& name() const { return name_; }
  //! The size of one element, i.e. of one slot binding.
  uint32_t element_size() const { return element_size_; }
  //! The distance between two slots of a chunk, in bytes.
  uint32_t slot_stride() const { return slot_stride_; }
  //! The size of every chunk buffer, in bytes.
  uint32_t chunk_size() const { return chunk_size_; }
  //! The number of slots which fit into one chunk.
  uint32_t slots_per_chunk() const { return slots_per_chunk_; }
  //! The number of chunks the pool holds.
  std::size_t chunk_count() const { return chunks_.size(); }
  //! The chunk of index, which Slot::chunk addresses.
  const Chunk& chunk(uint32_t index) const { return chunks_[index]; }

 private:
  //! Appends a chunk, which becomes the one Acquire() fills next.
  void CreateChunk();

  //! The bind group layout of the set, which has to be built with a dynamic
  //! offset.
  wgpu::BindGroupLayout layout_;
  //! The label of this pool.
  std::string name_;
  //! The size of one element, which is also what a slot binding is given.
  uint32_t element_size_ = 0;
  /*! The distance between two slots, i.e. the greater one of
      minUniformBufferOffsetAlignment and one element, so two slots never
      overlap. */
  uint32_t slot_stride_ = 0;
  //! The size of every chunk buffer, a whole multiple of slot_stride_.
  uint32_t chunk_size_ = 0;
  //! The number of slots which fit into one chunk.
  uint32_t slots_per_chunk_ = 0;
  //! The chunks of the pool.
  std::vector<Chunk> chunks_;
  //! The staged bytes of a chunk, indexed like chunks_.
  std::vector<std::vector<uint8_t>> staging_;
  //! The chunk Acquire() fills next.
  uint32_t active_chunk_ = 0;
};

/**
\brief The uniform pools of the engine, one per kind of per-drawable data.

A pool is the property of the device and of the shaders it was built for, so the
manager shares the lifetime of Graphics, and it is what Node::Render drives once
per frame:

\code
auto& uniforms = UniformManager::Get();
uniforms.BeginFrame();     // drop the slots of the previous frame
ExecutePrepare(&context);  // one Acquire() per drawable
uniforms.Flush();          // copy the staged slots into their buffers
\endcode

Every pool of a frame has to be filled in the same frame, so a caller which
Acquires from one of them does it between BeginFrame() and Flush().
*/
class UniformManager : public Singleton<UniformManager> {
 public:
  UniformManager();
  ~UniformManager();

  //! Opens a frame of every pool, see UniformBlockPool::BeginFrame().
  void BeginFrame();
  //! Uploads the slots every pool staged, see UniformBlockPool::Flush().
  void Flush();

  /*! The object transform of a drawable, which the pipelines bind at set 1:
      ObjectData. */
  UniformBlockPool& object_uniforms() { return object_uniforms_; }
  /*! The parameter of a sprite, which the sprite pipeline binds at set 3:
      SpriteBase::SpriteParam. Sprites read this out of one shared buffer so
      they can be batched, which is why it is a pool of its own. */
  UniformBlockPool& sprite_uniforms() { return sprite_uniforms_; }

 private:
  //! The set 1 uniform of every pipeline built with a dynamic object set.
  UniformBlockPool object_uniforms_;
  //! The set 3 uniform of the sprite pipeline.
  UniformBlockPool sprite_uniforms_;
};

}  // namespace urge
