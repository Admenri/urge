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

#include "core/uniform.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "core/exception.h"
#include "core/gpu.h"
#include "core/logger.h"

namespace urge {

namespace {

//! The slot distance used when the device reports no alignment.
constexpr uint32_t kDefaultSlotStride = 256;

//! The chunk size used when the device reports no binding limit.
constexpr uint32_t kDefaultChunkSize = 64 * 1024;

/* A chunk larger than this only wastes host and device memory: it would hold
   more slots than a frame draws, and a frame which needs more slots than fit
   into one chunk simply continues in the next one. */
constexpr uint32_t kMaxChunkSize = 1024 * 1024;

}  // namespace

UniformBlockPool::UniformBlockPool(wgpu::BindGroupLayout layout,
                                   uint32_t element_size,
                                   std::string_view name)
    : layout_(layout), name_(name), element_size_(element_size) {
  if (layout_ == nullptr)
    throw Exception(Exception::kGPUError,
                    "the uniform pool '{}' was given no bind group layout.",
                    name_);
  if (element_size_ == 0)
    throw Exception(Exception::kGPUError,
                    "the uniform pool '{}' was given an empty element.", name_);

  wgpu::Limits limits = {};
  /* The limits of the device are the ones it validates against, and they are
     the ones the requested limits resolved to, which are the defaults of the
     backend while GPUDevice::CreateDevice requests no higher ones. They may
     therefore be stricter than what the adapter reports, so the device is what
     is asked here. */
  GPUDevice::Get().device().GetLimits(&limits);

  const uint32_t alignment =
      limits.minUniformBufferOffsetAlignment == WGPU_LIMIT_U32_UNDEFINED
          ? kDefaultSlotStride
          : limits.minUniformBufferOffsetAlignment;

  /* Two slots of one chunk may never overlap, so the distance between them is
     the greater one of the alignment and the size of the data they hold. Both
     are powers of two, so the distance stays a whole multiple of the
     alignment. */
  slot_stride_ = std::max(alignment, element_size_);

  const uint64_t binding_limit =
      limits.maxUniformBufferBindingSize == WGPU_LIMIT_U64_UNDEFINED
          ? kDefaultChunkSize
          : limits.maxUniformBufferBindingSize;
  const uint64_t chunk_limit =
      std::min<uint64_t>(std::max<uint64_t>(binding_limit, slot_stride_),
                         kMaxChunkSize);

  slots_per_chunk_ =
      static_cast<uint32_t>(std::max<uint64_t>(1, chunk_limit / slot_stride_));
  // The buffer is a whole multiple of the slot distance, a partial slot at the
  // end of a chunk would never be used
  chunk_size_ = slots_per_chunk_ * slot_stride_;

  LOGGER_DEBUG(
      "uniform pool '{}': {} byte slots, stride {}, {} slots per chunk of {} "
      "bytes",
      name_, element_size_, slot_stride_, slots_per_chunk_, chunk_size_);
  LOGGER_TRACE("uniform pool '{}': alignment {} / binding limit {}", name_,
               alignment, binding_limit);

  CreateChunk();
  BeginFrame();
}

UniformBlockPool::~UniformBlockPool() = default;

void UniformBlockPool::BeginFrame() {
  for (Chunk& chunk : chunks_)
    chunk.used = 0;
  active_chunk_ = 0;
}

UniformBlockPool::Slot UniformBlockPool::Acquire(const void* data,
                                                 uint32_t size) {
  if (size > element_size_)
    throw Exception(Exception::kGPUError,
                    "the uniform pool '{}' holds {} byte elements but was "
                    "given {} bytes.",
                    name_, element_size_, size);

  // The chunk which is being filled, a chunk of an earlier frame is empty again
  while (active_chunk_ < chunks_.size() &&
         chunks_[active_chunk_].used == chunks_[active_chunk_].capacity)
    ++active_chunk_;

  if (active_chunk_ >= chunks_.size())
    CreateChunk();

  const uint32_t offset = chunks_[active_chunk_].used++ * slot_stride_;
  std::memcpy(staging_[active_chunk_].data() + offset, data, size);

  Slot slot;
  slot.chunk = active_chunk_;
  slot.offset = offset;
  return slot;
}

void UniformBlockPool::Flush() {
  const wgpu::Queue queue = GPUDevice::Get().queue();

  for (std::size_t index = 0; index < chunks_.size(); ++index) {
    const uint32_t used = chunks_[index].used;
    if (!used)
      continue;

    queue.WriteBuffer(chunks_[index].buffer, 0, staging_[index].data(),
                      static_cast<std::size_t>(used) * slot_stride_);
  }
}

void UniformBlockPool::CreateChunk() {
  wgpu::BufferDescriptor buffer_desc;
  buffer_desc.usage = wgpu::BufferUsage::Uniform | wgpu::BufferUsage::CopyDst;
  buffer_desc.size = chunk_size_;

  Chunk chunk;
  chunk.buffer = GPUDevice::Get().device().CreateBuffer(&buffer_desc);
  chunk.capacity = slots_per_chunk_;

  /* The bind group covers one element at the start of the buffer and the slot a
     draw reads is chosen with the dynamic offset of the draw. Binding the whole
     buffer instead would make every dynamic offset beyond the first slot
     invalid, because the offset of a binding is added to the size of it. */
  WBufferSet binding(chunk.buffer);
  binding.size = element_size_;
  chunk.group = CreateWGroup(layout_, {{0, binding}});

  if (chunk.buffer == nullptr || chunk.group == nullptr) {
    LOGGER_ERROR("uniform pool '{}': the device rejected a chunk of {} bytes",
                 name_, chunk_size_);
    throw Exception(Exception::kGPUError,
                    "the device rejected a chunk of the uniform pool '{}'.",
                    name_);
  }

  chunks_.push_back(std::move(chunk));
  staging_.emplace_back(chunk_size_);
  active_chunk_ = static_cast<uint32_t>(chunks_.size() - 1);
}

/* ----- UniformManager ----- */

UniformManager::UniformManager()
    /* The sprite and the tint pipelines are the ones built with a dynamic object
       set, either of them describes the layout the object pool binds; the sprite
       pipeline is the only one which takes a dynamic sprite parameter. */
    : object_uniforms_(
          ShaderSet::Get().state.sprite_blends.at(BLEND_NORMAL)
              .GetBindGroupLayout(1),
          sizeof(ObjectData), "object"),
      sprite_uniforms_(
          ShaderSet::Get().state.sprite_blends.at(BLEND_NORMAL)
              .GetBindGroupLayout(3),
          sizeof(SpriteBase::SpriteParam), "sprite") {}

UniformManager::~UniformManager() = default;

void UniformManager::BeginFrame() {
  object_uniforms_.BeginFrame();
  sprite_uniforms_.BeginFrame();
}

void UniformManager::Flush() {
  object_uniforms_.Flush();
  sprite_uniforms_.Flush();
}

}  // namespace urge
