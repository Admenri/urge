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

//! A user-authored shader an object draws with: a vertex and a fragment stage,
//! the engine vertex stream and sets 0/1, and a caller-filled custom set 2.
URGE_BINDING()
class Effect : public Object {
 public:
  URGE_BINDING()
  Effect(std::string vs_glsl,
         std::string fs_glsl,
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

  //! The state of the draw of this effect, made once in the constructor.
  wgpu::RenderPipeline AcquirePipeline();

  //! The custom bind group (set 2) of this effect, rebuilt when the staged
  //! resources changed since the last call.
  //! set 0 -> SceneGroup, 1 -> ObjectGroup (dynamic), 2 -> CustomGroup (self)
  wgpu::BindGroup AcquireBindGroup();

  //! Hands the shader the region it filters, as the texture at custom binding 0
  //! and its sampler at binding 1; a Viewport sets this before drawing with the
  //! effect, see Viewport::FinishFilter.
  void SetFilterSource(RefPtr<Bitmap> texture);

 private:
  //! The compiled stages and the draw state, shared by the copies of an effect:
  //! a shader is immutable, so a copy reuses the pipeline of the original.
  struct PipelineHolder : public RefCounted<PipelineHolder> {
    //! The bindings the stages declare in set 2, keyed by their binding.
    std::map<uint32_t, ShaderBinding> custom_bindings;
    //! The state of the draw, built from the two stages.
    wgpu::RenderPipeline pipeline;
  };

  //! What the caller staged for one binding of the custom set.
  struct SlotResource {
    //! The buffer of a buffer binding; empty for a texture or sampler binding.
    wgpu::Buffer buffer;
    //! The bitmap a texture or sampler binding reads; empty for a buffer.
    RefPtr<Bitmap> bitmap;
  };

  //! Compiles the two stages, reads the blend state and builds the pipeline.
  void CreateInternal(std::string vs_glsl,
                      std::string fs_glsl,
                      std::string blend_states);
  //! Stages \p size bytes at \p data into the buffer binding \p slot.
  void SetBufferBytes(uint32_t slot, const void* data, uint32_t size);
  //! Makes the bind group of set 2 out of the staged resources.
  void RebuildBindGroup();

  RefPtr<PipelineHolder> holder_;
  std::map<uint32_t, SlotResource> resources_;
  wgpu::BindGroup binding_;
  //! Whether \p binding_ has to be rebuilt before it is handed out.
  bool binding_dirty_ = false;
};

}  // namespace urge
