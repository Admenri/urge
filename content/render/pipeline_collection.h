// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDER_PIPELINE_COLLECTION_H_
#define CONTENT_RENDER_PIPELINE_COLLECTION_H_

#include <array>

#include "renderer/pipeline/render_pipeline.h"

namespace content {

/// Color Blend Type of Pipeline
///
enum BlendType {
  BLEND_TYPE_NORMAL = 0,
  BLEND_TYPE_ADDITION,
  BLEND_TYPE_SUBSTRACTION,
  BLEND_TYPE_MULTIPLY,
  BLEND_TYPE_KEEP_ALPHA,
  BLEND_TYPE_NO_BLEND,
  BLEND_TYPE_NUMS,
};

struct PipelineCollection {
  using PipelineObject = RRefPtr<Diligent::IPipelineState>;

  struct {
    std::array<PipelineObject, BLEND_TYPE_NUMS> base;
    PipelineObject color;
    PipelineObject bitmapblt;
    PipelineObject bitmapclip;
    PipelineObject bitmaphue;
  } bitmap;

  std::array<PipelineObject, BLEND_TYPE_NUMS> plane;
  std::array<PipelineObject, BLEND_TYPE_NUMS> sprite;

  PipelineObject viewport_flat;
  PipelineObject brightness;

  PipelineObject tilemap;
  PipelineObject tilemap2;

  PipelineObject alpha_transition;
  PipelineObject vague_transition;

  PipelineObject yuv;

  PipelineObject window;
  struct {
    PipelineObject viewport;
    PipelineObject viewport_alpha;
    PipelineObject base;
  } window2;

  PipelineCollection(renderer::PipelineSet* loader);
};

}  // namespace content

#endif  //! CONTENT_RENDER_PIPELINE_COLLECTION_H_
