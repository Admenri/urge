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

//! Bulk uniform buffer for one kind of per-drawable data, addressed with a
//! dynamic offset; a frame is BeginFrame() -> Acquire() -> Flush().
class UniformBlockPool {
 public:
  //! The chunk index of a slot which does not belong to the pool.
  static constexpr uint32_t kInvalidChunk = 0xFFFFFFFFu;

  //! One buffer of the pool together with the bind group which covers it.
  struct Chunk {
    //! The buffer holding the slots, of chunk_size() bytes.
    wgpu::Buffer buffer;
    //! The bind group of the set, built once for the whole buffer.
    wgpu::BindGroup group;
    //! The number of slots the buffer holds.
    uint32_t capacity = 0;
    //! Slots handed out this frame, reset by BeginFrame().
    uint32_t used = 0;
  };

  //! The place of one element inside the pool.
  struct Slot {
    //! Chunk which owns the slot, kInvalidChunk when the pool was full.
    uint32_t chunk = kInvalidChunk;
    //! Byte offset of the slot, i.e. the dynamic offset for the set.
    uint32_t offset = 0;
  };

  //! Creates a pool of buffers of \p element_size-byte elements; \p layout is
  //! the bind group layout of the dynamic-offset set the pool serves.
  UniformBlockPool(wgpu::BindGroupLayout layout,
                   uint32_t element_size,
                   std::string_view name = {});
  ~UniformBlockPool();

  //! Starts a frame: every chunk loses its slots and its buffer is reused.
  void BeginFrame();

  //! Reserves a slot and stages \p size bytes at \p data in it; the returned
  //! Slot::chunk is kInvalidChunk when the pool could not grow.
  Slot Acquire(const void* data, uint32_t size);

  //! Reserves a slot and stages \p data in it, see Acquire().
  template <typename Ty>
  Slot Acquire(const Ty& data) {
    static_assert(std::is_trivially_copyable_v<Ty>,
                  "a slot only holds plain data");
    return Acquire(&data, sizeof(Ty));
  }

  //! Writes the slots of every chunk used this frame into its buffer.
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

  //! Bind group layout of the set, built with a dynamic offset.
  wgpu::BindGroupLayout layout_;
  //! The label of this pool.
  std::string name_;
  //! The size of one element, which is also what a slot binding is given.
  uint32_t element_size_ = 0;
  //! Distance between two slots, i.e. max(alignment, one element).
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

//! The uniform pools of the engine, one per kind of per-drawable data, driven
//! once per frame by Node::Render between BeginFrame() and Flush().
class UniformManager : public Singleton<UniformManager> {
 public:
  UniformManager();
  ~UniformManager();

  //! Opens a frame of every pool, see UniformBlockPool::BeginFrame().
  void BeginFrame();
  //! Uploads the slots every pool staged, see UniformBlockPool::Flush().
  void Flush();

  //! The object transform of a drawable, bound by the pipelines at set 1.
  UniformBlockPool& object_uniforms() { return object_uniforms_; }
  //! The sprite parameter, bound by the sprite pipeline at set 3.
  UniformBlockPool& sprite_uniforms() { return sprite_uniforms_; }

 private:
  //! The set 1 uniform of every pipeline built with a dynamic object set.
  UniformBlockPool object_uniforms_;
  //! The set 3 uniform of the sprite pipeline.
  UniformBlockPool sprite_uniforms_;
};

}  // namespace urge
