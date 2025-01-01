// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef RENDERER_PIPELINE_RENDER_PIPELINE_H_
#define RENDERER_PIPELINE_RENDER_PIPELINE_H_

#include "renderer/vertex/vertex_layout.h"

namespace renderer {

enum BlendType {
  kNormal = 0,
  kAddition,
  kSubstraction,
  kNoBlend,

  kBlendNums,
};

class RenderPipelineBase {
 public:
  virtual ~RenderPipelineBase() = default;

  RenderPipelineBase(const RenderPipelineBase&) = delete;
  RenderPipelineBase& operator=(const RenderPipelineBase&) = delete;

  wgpu::RenderPipeline* GetPipeline(BlendType blend) {
    return &pipelines_[blend];
  }

 protected:
  RenderPipelineBase(const wgpu::Device& device);

  void BuildPipeline(const std::string& shader_source,
                     const std::string& vs_entry,
                     const std::string& fs_entry,
                     const std::vector<wgpu::VertexBufferLayout>& vertex_layout,
                     const std::vector<wgpu::BindGroupLayout>& bind_layout,
                     wgpu::TextureFormat target_format);

 private:
  wgpu::Device device_;
  std::vector<wgpu::RenderPipeline> pipelines_;
};

class Pipeline_Base : public RenderPipelineBase {
 public:
  using VertexType = FullVertexLayout;
  Pipeline_Base(const wgpu::Device& device, wgpu::TextureFormat target);
};

class Pipeline_Color : public RenderPipelineBase {
 public:
  using VertexType = FullVertexLayout;
  Pipeline_Color(const wgpu::Device& device, wgpu::TextureFormat target);
};

}  // namespace renderer

#endif  //! RENDERER_PIPELINE_RENDER_PIPELINE_H_
