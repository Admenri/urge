// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/pipeline_collection.h"

namespace content {

namespace {

Diligent::RenderTargetBlendDesc GetBlendState(BlendType type) {
  Diligent::RenderTargetBlendDesc state;
  switch (type) {
    default:
    case BLEND_TYPE_NORMAL:
      state.BlendEnable = Diligent::True;
      state.BlendOp = Diligent::BLEND_OPERATION_ADD;
      state.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
      state.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      state.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
      state.DestBlendAlpha = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      break;
    case BLEND_TYPE_ADDITION:
      state.BlendEnable = Diligent::True;
      state.BlendOp = Diligent::BLEND_OPERATION_ADD;
      state.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
      state.DestBlend = Diligent::BLEND_FACTOR_ONE;
      state.SrcBlendAlpha = Diligent::BLEND_FACTOR_ONE;
      state.DestBlendAlpha = Diligent::BLEND_FACTOR_ONE;
      break;
    case BLEND_TYPE_SUBSTRACTION:
      state.BlendEnable = Diligent::True;
      state.BlendOp = Diligent::BLEND_OPERATION_REV_SUBTRACT;
      state.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
      state.DestBlend = Diligent::BLEND_FACTOR_ONE;
      state.SrcBlendAlpha = Diligent::BLEND_FACTOR_ZERO;
      state.DestBlendAlpha = Diligent::BLEND_FACTOR_ONE;
      break;
    case BLEND_TYPE_MULTIPLY:
      state.BlendEnable = Diligent::True;
      state.BlendOp = Diligent::BLEND_OPERATION_ADD;
      state.SrcBlend = Diligent::BLEND_FACTOR_DEST_COLOR;
      state.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      state.SrcBlendAlpha = Diligent::BLEND_FACTOR_ZERO;
      state.DestBlendAlpha = Diligent::BLEND_FACTOR_ONE;
      break;
    case BLEND_TYPE_KEEP_ALPHA:
      state.BlendEnable = Diligent::True;
      state.BlendOp = Diligent::BLEND_OPERATION_ADD;
      state.SrcBlend = Diligent::BLEND_FACTOR_SRC_ALPHA;
      state.DestBlend = Diligent::BLEND_FACTOR_INV_SRC_ALPHA;
      state.SrcBlendAlpha = Diligent::BLEND_FACTOR_ZERO;
      state.DestBlendAlpha = Diligent::BLEND_FACTOR_ONE;
      break;
    case BLEND_TYPE_NO_BLEND:
      state.BlendEnable = Diligent::False;
      break;
  }

  return state;
}

Diligent::DepthStencilStateDesc GetDefaultDepthStencilState(bool enable_depth) {
  Diligent::DepthStencilStateDesc depth_stencil_state;
  if (enable_depth) {
    depth_stencil_state.DepthFunc = Diligent::COMPARISON_FUNC_LESS_EQUAL;
    depth_stencil_state.DepthEnable = Diligent::True;
  } else {
    depth_stencil_state.DepthEnable = Diligent::False;
  }

  return depth_stencil_state;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// PipelineCollection Implement

PipelineCollection::PipelineCollection(renderer::PipelineSet* loader) {
  constexpr auto target_format = Diligent::TEX_FORMAT_RGBA8_UNORM;
  constexpr auto depth_stencil_format = Diligent::TEX_FORMAT_D24_UNORM_S8_UINT;
  constexpr auto primitive_topology =
      Diligent::PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  constexpr Diligent::SampleDesc default_sample;

  // Bitmap (canvas)
  {
    // Base (blend + no depth)
    for (size_t i = 0; i < BLEND_TYPE_NUMS; ++i) {
      Diligent::BlendStateDesc blend_state;
      blend_state.RenderTargets[0] = GetBlendState(static_cast<BlendType>(i));

      // Disable depth test
      Diligent::DepthStencilStateDesc depth_stencil_state =
          GetDefaultDepthStencilState(false);

      Diligent::RasterizerStateDesc rasterizer_state;
      rasterizer_state.CullMode = Diligent::CULL_MODE_FRONT;
      rasterizer_state.FrontCounterClockwise = Diligent::True;
      rasterizer_state.ScissorEnable = Diligent::True;

      loader->base.BuildPipeline(&bitmap.base[i], blend_state, rasterizer_state,
                                 depth_stencil_state, primitive_topology,
                                 {target_format}, Diligent::TEX_FORMAT_UNKNOWN,
                                 default_sample);
    }

    // No depth test
    Diligent::DepthStencilStateDesc depth_stencil_state =
        GetDefaultDepthStencilState(false);

    Diligent::RasterizerStateDesc rasterizer_state;
    rasterizer_state.CullMode = Diligent::CULL_MODE_FRONT;
    rasterizer_state.FrontCounterClockwise = Diligent::True;
    rasterizer_state.ScissorEnable = Diligent::False;  // No scissor test

    // Color no blend
    loader->color.BuildPipeline(&bitmap.color, Diligent::BlendStateDesc(),
                                rasterizer_state, depth_stencil_state,
                                primitive_topology, {target_format},
                                Diligent::TEX_FORMAT_UNKNOWN, default_sample);

    // Blt no blend
    loader->bitmapblt.BuildPipeline(
        &bitmap.bitmapblt, Diligent::BlendStateDesc(), rasterizer_state,
        depth_stencil_state, primitive_topology, {target_format},
        Diligent::TEX_FORMAT_UNKNOWN, default_sample);

    // Clip no blend
    loader->bitmapclipblt.BuildPipeline(
        &bitmap.bitmapclip, Diligent::BlendStateDesc(), rasterizer_state,
        depth_stencil_state, primitive_topology, {target_format},
        Diligent::TEX_FORMAT_UNKNOWN, default_sample);

    // Hue no blend
    loader->bitmaphue.BuildPipeline(
        &bitmap.bitmaphue, Diligent::BlendStateDesc(), rasterizer_state,
        depth_stencil_state, primitive_topology, {target_format},
        Diligent::TEX_FORMAT_UNKNOWN, default_sample);
  }

  // Window Flat (viewport)
  for (size_t i = 0; i < BLEND_TYPE_NUMS; ++i) {
    // With blend
    Diligent::BlendStateDesc blend_state;
    blend_state.RenderTargets[0] = GetBlendState(static_cast<BlendType>(i));

    // Enable depth test
    Diligent::DepthStencilStateDesc depth_stencil_state =
        GetDefaultDepthStencilState(false);

    Diligent::RasterizerStateDesc rasterizer_state;
    rasterizer_state.CullMode = Diligent::CULL_MODE_FRONT;
    rasterizer_state.FrontCounterClockwise = Diligent::True;
    rasterizer_state.ScissorEnable = Diligent::True;  // With scissor test

    loader->viewport.BuildPipeline(
        &viewport[i], blend_state, rasterizer_state, depth_stencil_state,
        primitive_topology, {target_format}, Diligent::TEX_FORMAT_UNKNOWN,
        default_sample);
  }

  // Sprite
  for (size_t i = 0; i < BLEND_TYPE_NUMS; ++i) {
    // With blend
    Diligent::BlendStateDesc blend_state;
    blend_state.RenderTargets[0] = GetBlendState(static_cast<BlendType>(i));

    // Enable depth test
    Diligent::DepthStencilStateDesc depth_stencil_state =
        GetDefaultDepthStencilState(true);

    Diligent::RasterizerStateDesc rasterizer_state;
    rasterizer_state.CullMode = Diligent::CULL_MODE_FRONT;
    rasterizer_state.FrontCounterClockwise = Diligent::True;
    rasterizer_state.ScissorEnable = Diligent::True;  // With scissor test

    loader->sprite.BuildPipeline(&sprite[i], blend_state, rasterizer_state,
                                 depth_stencil_state, primitive_topology,
                                 {target_format}, depth_stencil_format,
                                 default_sample);
  }

  // Viewport (flat)
  {
    // No blend
    Diligent::BlendStateDesc blend_state;

    // Disable depth test
    Diligent::DepthStencilStateDesc depth_stencil_state =
        GetDefaultDepthStencilState(false);

    Diligent::RasterizerStateDesc rasterizer_state;
    rasterizer_state.CullMode = Diligent::CULL_MODE_FRONT;
    rasterizer_state.FrontCounterClockwise = Diligent::True;
    rasterizer_state.ScissorEnable = Diligent::True;  // Optional scissor test

    // Base with blend
    loader->viewport.BuildPipeline(
        &viewport_flat, blend_state, rasterizer_state, depth_stencil_state,
        primitive_topology, {target_format}, Diligent::TEX_FORMAT_UNKNOWN,
        default_sample);
  }

  {  // Brightness (effect)
    // With blend
    Diligent::BlendStateDesc blend_state;
    blend_state.RenderTargets[0] = GetBlendState(BLEND_TYPE_NORMAL);

    // Disable depth test
    Diligent::DepthStencilStateDesc depth_stencil_state =
        GetDefaultDepthStencilState(true);

    Diligent::RasterizerStateDesc rasterizer_state;
    rasterizer_state.CullMode = Diligent::CULL_MODE_FRONT;
    rasterizer_state.FrontCounterClockwise = Diligent::True;
    rasterizer_state.ScissorEnable = Diligent::False;  // No scissor test

    loader->color.BuildPipeline(&brightness, blend_state, rasterizer_state,
                                depth_stencil_state, primitive_topology,
                                {target_format}, depth_stencil_format,
                                default_sample);
  }

  {  // Tilemap
    // With blend
    Diligent::BlendStateDesc blend_state;
    blend_state.RenderTargets[0] = GetBlendState(BLEND_TYPE_NORMAL);

    // Enable depth test
    Diligent::DepthStencilStateDesc depth_stencil_state =
        GetDefaultDepthStencilState(true);

    Diligent::RasterizerStateDesc rasterizer_state;
    rasterizer_state.CullMode = Diligent::CULL_MODE_FRONT;
    rasterizer_state.FrontCounterClockwise = Diligent::True;
    rasterizer_state.ScissorEnable = Diligent::True;  // With scissor test

    loader->tilemap.BuildPipeline(&tilemap, blend_state, rasterizer_state,
                                  depth_stencil_state, primitive_topology,
                                  {target_format}, depth_stencil_format,
                                  default_sample);
    loader->tilemap2.BuildPipeline(&tilemap2, blend_state, rasterizer_state,
                                   depth_stencil_state, primitive_topology,
                                   {target_format}, depth_stencil_format,
                                   default_sample);
  }

  {  // Transition
    // No blend
    Diligent::BlendStateDesc blend_state;

    // Disable depth test
    Diligent::DepthStencilStateDesc depth_stencil_state =
        GetDefaultDepthStencilState(false);

    Diligent::RasterizerStateDesc rasterizer_state;
    rasterizer_state.CullMode = Diligent::CULL_MODE_FRONT;
    rasterizer_state.FrontCounterClockwise = Diligent::True;
    rasterizer_state.ScissorEnable = Diligent::False;  // With scissor test

    loader->alphatrans.BuildPipeline(
        &alpha_transition, blend_state, rasterizer_state, depth_stencil_state,
        primitive_topology, {target_format}, Diligent::TEX_FORMAT_UNKNOWN,
        default_sample);
    loader->mappedtrans.BuildPipeline(
        &vague_transition, blend_state, rasterizer_state, depth_stencil_state,
        primitive_topology, {target_format}, Diligent::TEX_FORMAT_UNKNOWN,
        default_sample);
  }

  {  // YUV
    // No blend
    Diligent::BlendStateDesc blend_state;

    // Disable depth test
    Diligent::DepthStencilStateDesc depth_stencil_state =
        GetDefaultDepthStencilState(false);

    Diligent::RasterizerStateDesc rasterizer_state;
    rasterizer_state.CullMode = Diligent::CULL_MODE_FRONT;
    rasterizer_state.FrontCounterClockwise = Diligent::True;
    rasterizer_state.ScissorEnable = Diligent::False;  // With scissor test

    loader->yuv.BuildPipeline(&yuv, blend_state, rasterizer_state,
                              depth_stencil_state, primitive_topology,
                              {target_format}, Diligent::TEX_FORMAT_UNKNOWN,
                              default_sample);
  }

  {  // Window (1 + 2)
    // With blend
    Diligent::BlendStateDesc blend_state;
    blend_state.RenderTargets[0] = GetBlendState(BLEND_TYPE_NORMAL);

    // Enable depth test
    Diligent::DepthStencilStateDesc depth_stencil_state =
        GetDefaultDepthStencilState(true);

    Diligent::RasterizerStateDesc rasterizer_state;
    rasterizer_state.CullMode = Diligent::CULL_MODE_FRONT;
    rasterizer_state.FrontCounterClockwise = Diligent::True;
    rasterizer_state.ScissorEnable = Diligent::True;  // With scissor test

    // Base with blend
    loader->base.BuildPipeline(&window, blend_state, rasterizer_state,
                               depth_stencil_state, primitive_topology,
                               {target_format}, depth_stencil_format,
                               default_sample);
  }

  {  // Window (2)
    // With blend
    Diligent::BlendStateDesc blend_state;
    blend_state.RenderTargets[0] = GetBlendState(BLEND_TYPE_NORMAL);

    // Enable depth test
    Diligent::DepthStencilStateDesc depth_stencil_state =
        GetDefaultDepthStencilState(false);

    Diligent::RasterizerStateDesc rasterizer_state;
    rasterizer_state.CullMode = Diligent::CULL_MODE_FRONT;
    rasterizer_state.FrontCounterClockwise = Diligent::True;
    rasterizer_state.ScissorEnable = Diligent::True;  // With scissor test

    loader->viewport.BuildPipeline(
        &window2.viewport, blend_state, rasterizer_state, depth_stencil_state,
        primitive_topology, {target_format}, Diligent::TEX_FORMAT_UNKNOWN,
        default_sample);
    loader->base.BuildPipeline(&window2.base, blend_state, rasterizer_state,
                               depth_stencil_state, primitive_topology,
                               {target_format}, Diligent::TEX_FORMAT_UNKNOWN,
                               default_sample);

    // Keep alpha
    blend_state.RenderTargets[0] = GetBlendState(BLEND_TYPE_KEEP_ALPHA);

    loader->viewport.BuildPipeline(
        &window2.viewport_alpha, blend_state, rasterizer_state,
        depth_stencil_state, primitive_topology, {target_format},
        Diligent::TEX_FORMAT_UNKNOWN, default_sample);
  }
}

}  // namespace content
