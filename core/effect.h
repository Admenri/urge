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
#include "core/object.h"
#include "core/pipeline.h"
#include "core/shader.h"

namespace urge {

/*! A user authored shader a sprite or a geometry draws with.

    An effect compiles a vertex and a fragment stage of GLSL and builds the
    state of a draw out of them with the same flow the built in pipelines use:
    a triangle list into the single RGBA8Unorm target every pass carries, with
    the depth-stencil state of a pass which ignores the attachment and a blend
    state read from \p blend_states, see Pipeline::MakeDefaultState and
    ParseBlendState. The two stages therefore read the vertex stream of the
    engine (location 0 is the position, 1 the texture coordinate and 2 the
    color of a vertex), the camera at set 0 and the object transform of the draw
    at set 1 -- which the engine binds with a dynamic offset -- so a node which
    carries an effect keeps its geometry, its transform and its place in the
    tree and only the shader and the blend of its draw change.

    The remaining bind group, set 2, is the custom set of the effect and belongs
    to the caller: every binding the two stages declare there is filled with
    SetFloat/SetInt/SetBlock (a buffer binding, staged as raw bytes),
    SetTexture (a sampled texture) or SetSampler (a sampler, taken from a
    bitmap). A binding the caller never fills reads a neutral default -- a
    zeroed buffer, a white texel, a nearest sampler -- so a stage which does not
    read every binding it declares still draws.

    A copy of an effect (its Ruby dup/clone) reuses the compiled stages and the
    state of the original and only owns its custom bind group, which it rebuilds
    from the resources it was copied with. */
class Effect : public Object {
 public:
  /*-export.begin-*/
  Effect(std::string vs_glsl,
         std::string fs_glsl,
         std::string blend_states = {});
  Effect(RefPtr<Effect> other);
  ~Effect();

  void SetFloat(uint32_t slot, std::vector<float> data);
  void SetInt(uint32_t slot, std::vector<int32_t> data);
  void SetTexture(uint32_t slot, RefPtr<Bitmap> texture);
  void SetSampler(uint32_t slot, RefPtr<Bitmap> texture);
  void SetBlock(uint32_t slot, std::string uniform_block);
  /*-export.end-*/

  //! The state of the draw of this effect, made once in the constructor.
  wgpu::RenderPipeline AcquirePipeline();

  //! The custom bind group (set 2) of this effect, rebuilt from the resources
  //! the setters staged when they changed since the last call.
  // 0 -> SceneGroup
  // 1 -> ObjectGroup (dynamic)
  // 2 -> CustomGroup (self)
  wgpu::BindGroup AcquireBindGroup();

  /*! Hands the shader of this effect the region it filters, sampled at the
      custom set -- the texture at binding 0 and its sampler at binding 1, the
      locations the built in shaders read a texture at. It is what a Viewport
      sets before it draws with the effect, so the shader reads the render the
      engine made for it, see Viewport::FinishFilter. A stage which declares
      neither entry reads no input and is left untouched, and every other
      binding of the custom set stays with the caller. */
  void SetFilterSource(RefPtr<Bitmap> texture);

 private:
  /*! The compiled stages and the state of a draw, shared by the copies of an
      effect: a shader is immutable, so a copy reuses the pipeline of the
      original instead of compiling it again. The render pipeline keeps the
      layout and the modules it was built from alive on its own. */
  struct PipelineHolder : public RefCounted<PipelineHolder> {
    //! The bindings the stages declare in set 2, keyed by their binding.
    std::map<uint32_t, ShaderBinding> custom_bindings;
    //! The state of the draw, built from the two stages.
    wgpu::RenderPipeline pipeline;
  };

  //! What the caller staged for one binding of the custom set.
  struct SlotResource {
    //! The buffer of a buffer binding, uploaded when it was set; empty for a
    //! texture or a sampler binding.
    wgpu::Buffer buffer;
    //! The bitmap a texture or a sampler binding reads; empty for a buffer.
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
