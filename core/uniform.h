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

class UniformBlockPool {
 public:
  static constexpr uint32_t kInvalidChunk = 0xFFFFFFFFu;

  struct Chunk {
    wgpu::Buffer buffer;
    wgpu::BindGroup group;
    uint32_t capacity = 0;
    uint32_t used = 0;
  };

  struct Slot {
    uint32_t chunk = kInvalidChunk;
    uint32_t offset = 0;
  };

  UniformBlockPool(wgpu::BindGroupLayout layout,
                   uint32_t element_size,
                   std::string_view name = {});
  ~UniformBlockPool();

  void BeginFrame();
  Slot Acquire(const void* data, uint32_t size);
  template <typename Ty>
  Slot Acquire(const Ty& data) {
    static_assert(std::is_trivially_copyable_v<Ty>,
                  "a slot only holds plain data");
    return Acquire(&data, sizeof(Ty));
  }

  void Flush();

  const std::string& name() const { return name_; }
  uint32_t element_size() const { return element_size_; }
  uint32_t slot_stride() const { return slot_stride_; }
  uint32_t chunk_size() const { return chunk_size_; }
  uint32_t slots_per_chunk() const { return slots_per_chunk_; }
  std::size_t chunk_count() const { return chunks_.size(); }
  const Chunk& chunk(uint32_t index) const { return chunks_[index]; }

 private:
  void CreateChunk();

  wgpu::BindGroupLayout layout_;
  std::string name_;
  uint32_t element_size_ = 0;
  uint32_t slot_stride_ = 0;
  uint32_t chunk_size_ = 0;
  uint32_t slots_per_chunk_ = 0;
  std::vector<Chunk> chunks_;
  std::vector<std::vector<uint8_t>> staging_;
  uint32_t active_chunk_ = 0;
};

class UniformManager : public Singleton<UniformManager> {
 public:
  UniformManager();
  ~UniformManager();

  void BeginFrame();
  void Flush();

  UniformBlockPool& object_uniforms() { return object_uniforms_; }
  UniformBlockPool& sprite_uniforms() { return sprite_uniforms_; }

 private:
  UniformBlockPool object_uniforms_;
  UniformBlockPool sprite_uniforms_;
};

}  // namespace urge
