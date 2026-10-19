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

#include "core/bitmap.h"
#include "core/definition.h"
#include "core/object.h"
#include "core/pipeline.h"
#include "core/shader.h"

namespace urge {

URGE_BINDING()
class Effect : public Object {
 public:
  // Both stages are WGSL, and each one enters at `vs_main` / `fs_main` -- the
  // same names the built-in shaders use.  They have to declare the scene
  // uniform at `@group(0) @binding(0)` and the object uniform at
  // `@group(1) @binding(0)`, because those two groups are the engine's own.
  //
  // Everything else is up to the source: the pipeline is built with a null
  // layout, so the device derives one from the two stages, and whatever the
  // shader declared under `@group(2)` is then read back off the pipeline with
  // `GetBindGroupLayout(2)`.  That is also why every binding the shader
  // declares has to be set before the first draw: the engine cannot know what
  // the shader asked for until the device has looked at it, so there is
  // nothing to fall back on for a slot the script never touched.
  //
  // Which setter is used is what says what a slot holds -- `SetTexture` fills
  // a texture view, `SetSampler` a sampler, `SetFloat`/`SetInt`/`SetBlock` a
  // uniform buffer.
  URGE_BINDING()
  Effect(std::string vs_wgsl,
         std::string fs_wgsl,
         std::string blend_states = {});
  URGE_BINDING()
  Effect(RefPtr<Effect> other);
  URGE_BINDING()
  ~Effect();

  URGE_BINDING()
  void SetFloat(uint32_t slot, std::vector<float> data);
  URGE_BINDING()
  void SetInt(uint32_t slot, std::vector<int32_t> data);
  URGE_BINDING()
  void SetTexture(uint32_t slot, RefPtr<Bitmap> texture);
  URGE_BINDING()
  void SetSampler(uint32_t slot, RefPtr<Bitmap> texture);
  URGE_BINDING()
  void SetBlock(uint32_t slot, std::string uniform_block);

  wgpu::RenderPipeline AcquirePipeline();
  wgpu::BindGroup AcquireBindGroup();
  void SetFilterSource(RefPtr<Bitmap> texture);

 private:
  struct PipelineHolder : public RefCounted<PipelineHolder> {
    wgpu::RenderPipeline pipeline;
  };

  // What a slot carries.  The engine no longer reads the shader, so the kind
  // comes from the setter that was used on the slot: there is no other place
  // that knows whether `@group(2) @binding(3)` is a texture or a buffer.
  enum class SlotKind { kTexture, kSampler, kBuffer };

  struct SlotResource {
    SlotKind kind = SlotKind::kBuffer;
    wgpu::Buffer buffer;
    RefPtr<Bitmap> bitmap;
    uint64_t size = 0;
  };

  void CreateInternal(std::string vs_wgsl,
                      std::string fs_wgsl,
                      std::string blend_states);
  void SetBufferBytes(uint32_t slot, const void* data, uint32_t size);
  void RebuildBindGroup();

  RefPtr<PipelineHolder> holder_;
  std::map<uint32_t, SlotResource> resources_;
  wgpu::BindGroup binding_;

  bool binding_dirty_ = false;
};

}  // namespace urge
