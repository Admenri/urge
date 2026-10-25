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

#include <cstdint>
#include <map>
#include <string>
#include <vector>

#include "core/canvas/bitmap.h"
#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/render/pipeline.h"
#include "core/render/shader.h"

namespace urge {

URGE_BINDING()
class Effect : public Object {
 public:
  URGE_BINDING()
  Effect(URGE_EXCEPT, std::string vs_wgsl,
         std::string fs_wgsl,
         std::string blend_states = {});
  URGE_BINDING()
  Effect(URGE_EXCEPT, RefPtr<Effect> other);
  URGE_BINDING()
  ~Effect();

  URGE_BINDING()
  void SetFloat(URGE_EXCEPT, uint32_t slot, std::vector<float> data);
  URGE_BINDING()
  void SetInt(URGE_EXCEPT, uint32_t slot, std::vector<int32_t> data);
  URGE_BINDING()
  void SetTexture(uint32_t slot, MaybePtr<Bitmap> texture);
  URGE_BINDING()
  void SetSampler(uint32_t slot, MaybePtr<Bitmap> texture);
  URGE_BINDING()
  void SetBlock(URGE_EXCEPT, uint32_t slot, std::string uniform_block);

  wgpu::RenderPipeline AcquirePipeline(URGE_EXCEPT);
  wgpu::BindGroup AcquireBindGroup(URGE_EXCEPT);
  void SetFilterSource(RefPtr<Bitmap> texture);

 private:
  struct PipelineHolder : public RefCounted<PipelineHolder> {
    wgpu::RenderPipeline pipeline;
  };

  enum class SlotKind { kTexture, kSampler, kBuffer };

  struct SlotResource {
    SlotKind kind = SlotKind::kBuffer;
    wgpu::Buffer buffer;
    RefPtr<Bitmap> bitmap;
    uint64_t size = 0;
  };

  void CreateInternal(URGE_EXCEPT, std::string vs_wgsl,
                      std::string fs_wgsl,
                      std::string blend_states);
  void RebuildPipeline(URGE_EXCEPT);
  void SetBufferBytes(URGE_EXCEPT, uint32_t slot, const void* data, uint32_t size);
  void RebuildBindGroup(URGE_EXCEPT);

  std::string vs_wgsl_;
  std::string fs_wgsl_;
  std::string blend_states_;

  RefPtr<PipelineHolder> holder_;
  std::map<uint32_t, SlotResource> resources_;
  wgpu::BindGroup binding_;

  bool binding_dirty_ = false;
};

}
