// Copyright 2018-2025 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/window2_impl.h"

#include "content/composite/tilequad.h"
#include "renderer/utils/texture_utils.h"

namespace content {

namespace {

void GPUCreateWindowInternal(renderer::RenderDevice* device,
                             Window2Agent* agent) {
  agent->background_batch = renderer::QuadBatch::Make(**device);
  agent->controls_batch = renderer::QuadBatch::Make(**device);

  agent->uniform_buffer =
      renderer::CreateUniformBuffer<renderer::ViewportFragmentUniform>(
          **device, "window2.uniform", wgpu::BufferUsage::CopyDst);
  agent->uniform_binding = renderer::ViewportFragmentUniform::CreateGroup(
      **device, agent->uniform_buffer);
}

void GPUDestroyWindowInternal(Window2Agent* agent) {
  delete agent;
}

void GPUCompositeWindowQuadsInternal(renderer::RenderDevice* device,
                                     wgpu::CommandEncoder* encoder,
                                     Window2Agent* agent,
                                     const base::Rect& bound,
                                     int32_t scale,
                                     bool pause,
                                     bool active,
                                     bool arrows_visible,
                                     int32_t opacity,
                                     int32_t openness,
                                     int32_t back_opacity,
                                     int32_t contents_opacity,
                                     int32_t pause_index,
                                     int32_t cursor_opacity,
                                     TextureAgent* contents,
                                     TextureAgent* windowskin,
                                     const base::Vec2i& origin,
                                     const base::Rect& cursor_rect,
                                     const base::Rect& padding_rect,
                                     const base::Vec4& tone,
                                     bool rgss3,
                                     bool background_dirty) {
  // Create texture if need
  if (windowskin) {
    if (!agent->background_texture ||
        agent->background_texture.GetWidth() != bound.width ||
        agent->background_texture.GetHeight() != bound.height) {
      agent->background_texture =
          renderer::CreateTexture2D(**device, "window2.background",
                                    wgpu::TextureUsage::RenderAttachment |
                                        wgpu::TextureUsage::TextureBinding,
                                    bound.Size());

      renderer::TextureBindingUniform uniform;
      uniform.texture_size = base::MakeInvert(bound.Size());
      wgpu::Buffer texture_uniform = renderer::CreateUniformBuffer(
          **device, "window2.background", wgpu::BufferUsage::None, &uniform);

      renderer::WorldMatrixUniform world_matrix;
      renderer::MakeProjectionMatrix(world_matrix.projection, bound.Size());
      renderer::MakeIdentityMatrix(world_matrix.transform);

      wgpu::Buffer world_uniform = renderer::CreateUniformBuffer(
          **device, "window2.world", wgpu::BufferUsage::None, &world_matrix);
      agent->world_binding =
          renderer::WorldMatrixUniform::CreateGroup(**device, world_uniform);

      agent->background_binding = renderer::TextureBindingUniform::CreateGroup(
          **device, agent->background_texture.CreateView(),
          (*device)->CreateSampler(), texture_uniform);

      background_dirty = true;
    }

    // Render background texture if need
    if (background_dirty) {
      background_dirty = false;
      int32_t quad_count = 0;

      // Background part
      {
        // Background 1
        quad_count += 1;
        // Background 2
        const int32_t horizon_tiles =
            CalculateQuadTileCount(32 * scale, bound.width - 2 * scale);
        const int32_t vertical_tiles =
            CalculateQuadTileCount(32 * scale, bound.height - 2 * scale);
        quad_count += horizon_tiles * vertical_tiles;

        // Corners
        quad_count += 4;
        // Frames
        const int32_t horizon_frame_tiles =
            CalculateQuadTileCount(16 * scale, bound.width - 16 * scale);
        const int32_t vertical_frame_tiles =
            CalculateQuadTileCount(16 * scale, bound.height - 16 * scale);
        quad_count += horizon_frame_tiles * 2 + vertical_frame_tiles * 2;
      }

      std::vector<renderer::Quad> quads;
      quads.resize(quad_count);
      renderer::Quad* quad_ptr = quads.data();

      // Generate quads
      const float back_opacity_norm = back_opacity / 255.0f;
      int32_t background_draw_count = 0;

      // Background part
      {
        // Stretch layer + Tile layer
        const base::Rect background1_src(0, 0, 32 * scale, 32 * scale);
        const base::Rect background2_src(0, 32 * scale, 32 * scale, 32 * scale);
        const base::Rect background_dest(scale, scale, bound.width - 2 * scale,
                                         bound.height - 2 * scale);
        renderer::Quad::SetPositionRect(quad_ptr, background_dest);
        renderer::Quad::SetTexCoordRect(quad_ptr, background1_src);
        renderer::Quad::SetColor(quad_ptr, base::Vec4(back_opacity_norm));
        background_draw_count++;
        quad_ptr++;

        int32_t tiles_count =
            BuildTiles(background2_src, background_dest,
                       base::Vec4(back_opacity_norm), quad_ptr);
        background_draw_count += tiles_count;
        quad_ptr += tiles_count;

        // Corners
        const base::Rect corner_left_top_src(32 * scale, 0, 8 * scale,
                                             8 * scale);
        const base::Rect corner_right_top_src(56 * scale, 0, 8 * scale,
                                              8 * scale);
        const base::Rect corner_left_bottom_src(32 * scale, 24 * scale,
                                                8 * scale, 8 * scale);
        const base::Rect corner_right_bottom_src(56 * scale, 24 * scale,
                                                 8 * scale, 8 * scale);

        const base::Rect corner_left_top_dest(0, 0, 8 * scale, 8 * scale);
        const base::Rect corner_right_top_dest(bound.width - 8 * scale, 0,
                                               8 * scale, 8 * scale);
        const base::Rect corner_left_bottom_dest(0, bound.height - 8 * scale,
                                                 8 * scale, 8 * scale);
        const base::Rect corner_right_bottom_dest(bound.width - 8 * scale,
                                                  bound.height - 8 * scale,
                                                  8 * scale, 8 * scale);

        renderer::Quad::SetPositionRect(quad_ptr, corner_left_top_dest);
        renderer::Quad::SetTexCoordRect(quad_ptr, corner_left_top_src);
        quad_ptr++;
        renderer::Quad::SetPositionRect(quad_ptr, corner_right_top_dest);
        renderer::Quad::SetTexCoordRect(quad_ptr, corner_right_top_src);
        quad_ptr++;
        renderer::Quad::SetPositionRect(quad_ptr, corner_left_bottom_dest);
        renderer::Quad::SetTexCoordRect(quad_ptr, corner_left_bottom_src);
        quad_ptr++;
        renderer::Quad::SetPositionRect(quad_ptr, corner_right_bottom_dest);
        renderer::Quad::SetTexCoordRect(quad_ptr, corner_right_bottom_src);
        quad_ptr++;

        // Tiles frame
        const base::Rect frame_up_src(40 * scale, 0, 16 * scale, 8 * scale);
        const base::Rect frame_down_src(40 * scale, 24 * scale, 16 * scale,
                                        8 * scale);
        const base::Rect frame_left_src(32 * scale, 8 * scale, 8 * scale,
                                        16 * scale);
        const base::Rect frame_right_src(56 * scale, 8 * scale, 8 * scale,
                                         16 * scale);

        quad_ptr += BuildTilesAlongAxis(
            TileAxis::HORIZONTAL, frame_up_src, base::Vec2i(8 * scale, 0),
            base::Vec4(1.0f), bound.width - 16 * scale, quad_ptr);
        quad_ptr += BuildTilesAlongAxis(
            TileAxis::HORIZONTAL, frame_down_src,
            base::Vec2i(8 * scale, bound.height - 8 * scale), base::Vec4(1.0f),
            bound.width - 16 * scale, quad_ptr);
        quad_ptr += BuildTilesAlongAxis(
            TileAxis::VERTICAL, frame_left_src, base::Vec2i(0, 8 * scale),
            base::Vec4(1.0f), bound.height - 16 * scale, quad_ptr);
        quad_ptr += BuildTilesAlongAxis(
            TileAxis::VERTICAL, frame_right_src,
            base::Vec2i(bound.width - 8 * scale, 8 * scale), base::Vec4(1.0f),
            bound.height - 16 * scale, quad_ptr);
      }

      agent->background_batch->QueueWrite(*encoder, quads.data(), quads.size());

      renderer::ViewportFragmentUniform uniform;
      uniform.color = base::Vec4();
      uniform.tone = tone;
      encoder->WriteBuffer(agent->uniform_buffer, 0,
                           reinterpret_cast<uint8_t*>(&uniform),
                           sizeof(uniform));

      {
        auto& pipeline_set_tone = device->GetPipelines()->viewport;
        auto* pipeline_tone =
            pipeline_set_tone.GetPipeline(renderer::BlendType::NORMAL);
        auto& pipeline_set_base = device->GetPipelines()->base;
        auto* pipeline_base =
            pipeline_set_base.GetPipeline(renderer::BlendType::NORMAL);

        wgpu::RenderPassColorAttachment attachment;
        attachment.view = agent->background_texture.CreateView();
        attachment.loadOp = wgpu::LoadOp::Clear;
        attachment.storeOp = wgpu::StoreOp::Store;
        attachment.clearValue = {0, 0, 0, 0};

        wgpu::RenderPassDescriptor renderpass_desc;
        renderpass_desc.colorAttachmentCount = 1;
        renderpass_desc.colorAttachments = &attachment;

        device->GetQuadIndex()->Allocate(quads.size() + 20);

        auto renderpass = encoder->BeginRenderPass(&renderpass_desc);
        renderpass.SetVertexBuffer(0, **agent->background_batch);
        renderpass.SetIndexBuffer(**device->GetQuadIndex(),
                                  device->GetQuadIndex()->format());
        renderpass.SetBindGroup(0, agent->world_binding);
        renderpass.SetBindGroup(1, windowskin->binding);
        renderpass.SetBindGroup(2, agent->uniform_binding);

        renderpass.SetPipeline(*pipeline_tone);
        renderpass.DrawIndexed(background_draw_count * 6);

        renderpass.SetPipeline(*pipeline_base);
        renderpass.DrawIndexed((quads.size() - background_draw_count) * 6, 1,
                               background_draw_count * 6);

        renderpass.End();
      }
    }
  }

  // Reset draw count
  agent->background_draw_count = 0;
  agent->controls_draw_count = 0;
  agent->cursor_draw_count = 0;
  agent->contents_draw_count = 0;

  // Render background and controls
  {
    std::vector<renderer::Quad> quads;
    // Background + Pause Anime + Arrows + Cursor + Contents
    quads.resize(1 + 1 + 4 + 9 + 1);

    renderer::Quad* quad_ptr = quads.data();
    const base::Vec2i offset = bound.Position();
    float openness_norm = openness / 255.0f;

    if (windowskin) {
      // Background
      const base::Rect background_dest(
          offset.x,
          static_cast<float>(offset.y) +
              static_cast<float>(bound.height / 2.0f) * (1.0f - openness_norm),
          bound.width, bound.height * openness_norm);
      renderer::Quad::SetPositionRect(quad_ptr, background_dest);
      renderer::Quad::SetTexCoordRect(quad_ptr, base::Rect(bound.Size()));
      renderer::Quad::SetColor(quad_ptr, base::Vec4(opacity / 255.0f));
      agent->background_draw_count += 1;
      quad_ptr += 1;
    }

    if (openness >= 255) {
      base::Vec2i content_offset = offset + padding_rect.Position();

      if (windowskin) {
        const base::Vec2i arrow_size =
            (bound.Size() - base::Vec2i(8 * scale)) / base::Vec2i(scale);

        if (arrows_visible) {
          // Arrows
          const base::Rect arrow_up_dest(offset.x + arrow_size.x,
                                         offset.y + 2 * scale, 8 * scale,
                                         4 * scale);
          const base::Rect arrow_down_dest(offset.x + arrow_size.x,
                                           offset.y + bound.height - 6 * scale,
                                           8 * scale, 4 * scale);
          const base::Rect arrow_left_dest(offset.x + 2 * scale,
                                           offset.y + arrow_size.y, 4 * scale,
                                           8 * scale);
          const base::Rect arrow_right_dest(offset.x + bound.width - 6 * scale,
                                            offset.y + arrow_size.y, 4 * scale,
                                            8 * scale);

          const base::Rect arrow_up_src(44 * scale, 8 * scale, 8 * scale,
                                        4 * scale);
          const base::Rect arrow_down_src(44 * scale, 20 * scale, 8 * scale,
                                          4 * scale);
          const base::Rect arrow_left_src(40 * scale, 12 * scale, 4 * scale,
                                          8 * scale);
          const base::Rect arrow_right_src(52 * scale, 12 * scale, 4 * scale,
                                           8 * scale);

          if (contents && arrows_visible) {
            if (origin.x > 0) {
              renderer::Quad::SetPositionRect(quad_ptr, arrow_left_dest);
              renderer::Quad::SetTexCoordRect(quad_ptr, arrow_left_src);
              agent->controls_draw_count += 1;
              quad_ptr += 1;
            }
            if (origin.y > 0) {
              renderer::Quad::SetPositionRect(quad_ptr, arrow_up_dest);
              renderer::Quad::SetTexCoordRect(quad_ptr, arrow_up_src);
              agent->controls_draw_count += 1;
              quad_ptr += 1;
            }
            if (padding_rect.width < (contents->size.x - origin.x)) {
              renderer::Quad::SetPositionRect(quad_ptr, arrow_right_dest);
              renderer::Quad::SetTexCoordRect(quad_ptr, arrow_right_src);
              agent->controls_draw_count += 1;
              quad_ptr += 1;
            }
            if (padding_rect.height < (contents->size.y - origin.y)) {
              renderer::Quad::SetPositionRect(quad_ptr, arrow_down_dest);
              renderer::Quad::SetTexCoordRect(quad_ptr, arrow_down_src);
              agent->controls_draw_count += 1;
              quad_ptr += 1;
            }
          }
        }

        // Pause
        const base::Rect pause_src[] = {
            {48 * scale, 32 * scale, 8 * scale, 8 * scale},
            {56 * scale, 32 * scale, 8 * scale, 8 * scale},
            {48 * scale, 40 * scale, 8 * scale, 8 * scale},
            {56 * scale, 40 * scale, 8 * scale, 8 * scale},
        };

        if (pause) {
          const base::Rect pause_dest(offset.x + arrow_size.x,
                                      offset.y + bound.height - 8 * scale,
                                      8 * scale, 8 * scale);
          renderer::Quad::SetPositionRect(quad_ptr, pause_dest);
          renderer::Quad::SetTexCoordRect(quad_ptr, pause_src[pause_index / 8]);
          agent->controls_draw_count += 1;
          quad_ptr += 1;
        }

        // Cursor
        if (cursor_rect.width > 0 && cursor_rect.height > 0) {
          auto build_cursor_internal = [&](const base::Rect& rect,
                                           base::Rect quad_rects[9]) {
            int32_t w = rect.width;
            int32_t h = rect.height;
            int32_t x1 = rect.x;
            int32_t x2 = x1 + w;
            int32_t y1 = rect.y;
            int32_t y2 = y1 + h;

            int32_t i = 0;
            quad_rects[i++] = base::Rect(x1, y1, scale, scale);
            quad_rects[i++] = base::Rect(x2 - scale, y1, scale, scale);
            quad_rects[i++] = base::Rect(x2 - scale, y2 - scale, scale, scale);
            quad_rects[i++] = base::Rect(x1, y2 - scale, scale, scale);

            quad_rects[i++] = base::Rect(x1, y1 + scale, scale, h - scale * 2);
            quad_rects[i++] =
                base::Rect(x2 - scale, y1 + scale, scale, h - scale * 2);
            quad_rects[i++] =
                base::Rect(x1 + scale, y1, w - scale * scale, scale);
            quad_rects[i++] =
                base::Rect(x1 + scale, y2 - scale, w - scale * 2, scale);

            quad_rects[i++] = base::Rect(x1 + scale, y1 + scale, w - scale * 2,
                                         h - scale * 2);
          };

          auto build_cursor_quads = [&](const base::Rect& src,
                                        const base::Rect& dst,
                                        renderer::Quad vert[9]) {
            base::Rect quad_rects[9];

            build_cursor_internal(src, quad_rects);
            for (int32_t i = 0; i < 9; ++i)
              renderer::Quad::SetTexCoordRect(&vert[i], quad_rects[i]);

            build_cursor_internal(dst, quad_rects);
            for (int32_t i = 0; i < 9; ++i)
              renderer::Quad::SetPositionRect(&vert[i], quad_rects[i]);

            const base::Vec4 cursor_opacity_norm(
                static_cast<float>(cursor_opacity) / 255.0f);
            for (int32_t i = 0; i < 9; ++i)
              renderer::Quad::SetColor(&vert[i], cursor_opacity_norm);

            return 9;
          };

          const base::Rect cursor_src(32 * scale, 32 * scale, 16 * scale,
                                      16 * scale);
          if (cursor_rect.width > 0 && cursor_rect.height > 0) {
            base::Rect cursor_dest(content_offset + cursor_rect.Position(),
                                   cursor_rect.Size());

            if (rgss3) {
              cursor_dest.x -= origin.x;
              cursor_dest.y -= origin.y;
            }

            int32_t count =
                build_cursor_quads(cursor_src, cursor_dest, quad_ptr);
            agent->cursor_draw_count += count;
            quad_ptr += count;
          }
        }
      }

      // Contents
      if (contents) {
        renderer::Quad::SetPositionRect(
            quad_ptr, base::Rect(content_offset - origin, contents->size));
        renderer::Quad::SetTexCoordRect(quad_ptr, base::Rect(contents->size));
        renderer::Quad::SetColor(quad_ptr,
                                 base::Vec4(contents_opacity / 255.0f));
        agent->contents_draw_count += 1;
      }
    }

    // Uplopad data
    agent->controls_batch->QueueWrite(*encoder, quads.data(), quads.size());
  }
}

void GPURenderWindowQuadsInternal(renderer::RenderDevice* device,
                                  wgpu::RenderPassEncoder* encoder,
                                  wgpu::BindGroup* world_binding,
                                  Window2Agent* agent,
                                  TextureAgent* windowskin,
                                  TextureAgent* contents,
                                  const base::Rect& bound,
                                  const base::Rect& padding_rect,
                                  const base::Rect& last_viewport) {
  auto& pipeline_set = device->GetPipelines()->base;
  auto* pipeline = pipeline_set.GetPipeline(renderer::BlendType::NORMAL);

  encoder->SetPipeline(*pipeline);
  encoder->SetVertexBuffer(0, **agent->controls_batch);
  encoder->SetIndexBuffer(**device->GetQuadIndex(),
                          device->GetQuadIndex()->format());
  encoder->SetBindGroup(0, *world_binding);

  int32_t count = 0;
  if (agent->background_draw_count > 0) {
    encoder->SetBindGroup(1, agent->background_binding);
    encoder->DrawIndexed(agent->background_draw_count * 6, 1, count * 6);
    count += agent->background_draw_count;
  }

  if (agent->controls_draw_count > 0) {
    encoder->SetBindGroup(1, windowskin->binding);
    encoder->DrawIndexed(agent->controls_draw_count * 6, 1, count * 6);
    count += agent->controls_draw_count;
  }

  const base::Rect contents_region(bound.x + padding_rect.x,
                                   bound.y + padding_rect.y, padding_rect.width,
                                   padding_rect.height);
  auto scissor_region = base::MakeIntersect(last_viewport, contents_region);
  if (!scissor_region.width || !scissor_region.height)
    return;

  encoder->SetScissorRect(scissor_region.x, scissor_region.y,
                          scissor_region.width, scissor_region.height);

  if (agent->cursor_draw_count > 0) {
    encoder->SetBindGroup(1, windowskin->binding);
    encoder->DrawIndexed(agent->cursor_draw_count * 6, 1, count * 6);
    count += agent->cursor_draw_count;
  }

  if (agent->contents_draw_count > 0) {
    encoder->SetBindGroup(1, contents->binding);
    encoder->DrawIndexed(agent->contents_draw_count * 6, 1, count * 6);
  }

  encoder->SetScissorRect(last_viewport.x, last_viewport.y, last_viewport.width,
                          last_viewport.height);
}

}  // namespace

scoped_refptr<Window2> Window2::New(ExecutionContext* execution_context,
                                    ExceptionState& exception_state) {
  return new Window2Impl(execution_context->graphics, nullptr, base::Rect(), 2);
}

scoped_refptr<Window2> Window2::New(ExecutionContext* execution_context,
                                    scoped_refptr<Viewport> viewport,
                                    ExceptionState& exception_state) {
  return new Window2Impl(execution_context->graphics,
                         ViewportImpl::From(viewport), base::Rect(), 2);
}

scoped_refptr<Window2> Window2::New(ExecutionContext* execution_context,
                                    scoped_refptr<Viewport> viewport,
                                    int32_t scale,
                                    ExceptionState& exception_state) {
  return new Window2Impl(execution_context->graphics,
                         ViewportImpl::From(viewport), base::Rect(), scale);
}

scoped_refptr<Window2> Window2::New(ExecutionContext* execution_context,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height,
                                    ExceptionState& exception_state) {
  return new Window2Impl(execution_context->graphics, nullptr,
                         base::Rect(x, y, width, height), 2);
}

scoped_refptr<Window2> Window2::New(ExecutionContext* execution_context,
                                    int32_t x,
                                    int32_t y,
                                    int32_t width,
                                    int32_t height,
                                    int32_t scale,
                                    ExceptionState& exception_state) {
  return new Window2Impl(execution_context->graphics, nullptr,
                         base::Rect(x, y, width, height), scale);
}

Window2Impl::Window2Impl(RenderScreenImpl* screen,
                         scoped_refptr<ViewportImpl> parent,
                         const base::Rect& bound,
                         int32_t scale)
    : GraphicsChild(screen),
      Disposable(screen),
      rgss3_style_(screen->GetAPIVersion() ==
                   ContentProfile::APIVersion::RGSS3),
      node_(parent ? parent->GetDrawableController()
                   : screen->GetDrawableController(),
            SortKey(rgss3_style_ ? 100 : 0,
                    rgss3_style_ ? std::numeric_limits<int64_t>::max() : 0)),
      scale_(scale),
      viewport_(parent),
      cursor_rect_(new RectImpl(base::Rect())),
      bound_(bound),
      back_opacity_(rgss3_style_ ? 192 : 255),
      tone_(new ToneImpl(base::Vec4())) {
  node_.RegisterEventHandler(base::BindRepeating(
      &Window2Impl::DrawableNodeHandlerInternal, base::Unretained(this)));

  tone_->AddObserver(base::BindRepeating(
      &Window2Impl::ToneValueObserverInternal, base::Unretained(this)));

  ExceptionState exception_state;
  contents_ = CanvasImpl::Create(screen->GetCanvasScheduler(), screen,
                                 screen->GetScopedFontContext(), base::Vec2i(1),
                                 exception_state);

  agent_ = new Window2Agent;
  screen->PostTask(
      base::BindOnce(&GPUCreateWindowInternal, screen->GetDevice(), agent_));
}

Window2Impl::~Window2Impl() {
  ExceptionState exception_state;
  Dispose(exception_state);
}

void Window2Impl::Dispose(ExceptionState& exception_state) {
  Disposable::Dispose(exception_state);
}

bool Window2Impl::IsDisposed(ExceptionState& exception_state) {
  return Disposable::IsDisposed(exception_state);
}

void Window2Impl::Update(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  if (!active_)
    return;

  pause_index_ = ++pause_index_ % 32;
  cursor_opacity_ += cursor_fade_ ? -8 : 8;
  if (cursor_opacity_ > 255) {
    cursor_opacity_ = 255;
    cursor_fade_ = true;
  } else if (cursor_opacity_ < 128) {
    cursor_opacity_ = 128;
    cursor_fade_ = false;
  }
}

void Window2Impl::Move(int32_t x,
                       int32_t y,
                       int32_t width,
                       int32_t height,
                       ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bound_ = base::Rect(x, y, width, height);
}

bool Window2Impl::IsOpened(ExceptionState& exception_state) {
  return openness_ == 255;
}

bool Window2Impl::IsClosed(ExceptionState& exception_state) {
  return openness_ == 0;
}

scoped_refptr<Viewport> Window2Impl::Get_Viewport(
    ExceptionState& exception_state) {
  return viewport_;
}

void Window2Impl::Put_Viewport(const scoped_refptr<Viewport>& value,
                               ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  if (viewport_ == value)
    return;

  viewport_ = ViewportImpl::From(value);
  node_.RebindController(viewport_->GetDrawableController());
}

scoped_refptr<Bitmap> Window2Impl::Get_Windowskin(
    ExceptionState& exception_state) {
  return windowskin_;
}

void Window2Impl::Put_Windowskin(const scoped_refptr<Bitmap>& value,
                                 ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  windowskin_ = CanvasImpl::FromBitmap(value);
}

scoped_refptr<Bitmap> Window2Impl::Get_Contents(
    ExceptionState& exception_state) {
  return contents_;
}

void Window2Impl::Put_Contents(const scoped_refptr<Bitmap>& value,
                               ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  contents_ = CanvasImpl::FromBitmap(value);
}

scoped_refptr<Rect> Window2Impl::Get_CursorRect(
    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return cursor_rect_;
}

void Window2Impl::Put_CursorRect(const scoped_refptr<Rect>& value,
                                 ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  *cursor_rect_ = *RectImpl::From(value);
}

bool Window2Impl::Get_Active(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return active_;
}

void Window2Impl::Put_Active(const bool& value,
                             ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  active_ = value;
}

bool Window2Impl::Get_Visible(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return node_.GetVisibility();
}

void Window2Impl::Put_Visible(const bool& value,
                              ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  node_.SetNodeVisibility(value);
}

bool Window2Impl::Get_ArrowsVisible(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return arrows_visible_;
}

void Window2Impl::Put_ArrowsVisible(const bool& value,
                                    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  arrows_visible_ = value;
}

bool Window2Impl::Get_Pause(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return false;

  return pause_;
}

void Window2Impl::Put_Pause(const bool& value,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  pause_ = value;
}

int32_t Window2Impl::Get_X(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return bound_.x;
}

void Window2Impl::Put_X(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bound_.x = value;
}

int32_t Window2Impl::Get_Y(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return bound_.y;
}

void Window2Impl::Put_Y(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bound_.y = value;
}

int32_t Window2Impl::Get_Width(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return bound_.width;
}

void Window2Impl::Put_Width(const int32_t& value,
                            ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bound_.width = value;
}

int32_t Window2Impl::Get_Height(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return bound_.height;
}

void Window2Impl::Put_Height(const int32_t& value,
                             ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  bound_.height = value;
}

int32_t Window2Impl::Get_Z(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return node_.GetSortKeys()->weight[0];
}

void Window2Impl::Put_Z(const int32_t& value, ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  node_.SetNodeSortWeight(value);
}

int32_t Window2Impl::Get_Ox(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return origin_.x;
}

void Window2Impl::Put_Ox(const int32_t& value,
                         ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  origin_.x = value;
}

int32_t Window2Impl::Get_Oy(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return origin_.y;
}

void Window2Impl::Put_Oy(const int32_t& value,
                         ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  origin_.y = value;
}

int32_t Window2Impl::Get_Padding(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return padding_;
}

void Window2Impl::Put_Padding(const int32_t& value,
                              ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  padding_ = value;
}

int32_t Window2Impl::Get_PaddingBottom(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return padding_bottom_;
}

void Window2Impl::Put_PaddingBottom(const int32_t& value,
                                    ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  padding_bottom_ = value;
}

int32_t Window2Impl::Get_Opacity(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return opacity_;
}

void Window2Impl::Put_Opacity(const int32_t& value,
                              ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  opacity_ = std::clamp(value, 0, 255);
}

int32_t Window2Impl::Get_BackOpacity(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return back_opacity_;
}

void Window2Impl::Put_BackOpacity(const int32_t& value,
                                  ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  back_opacity_ = std::clamp(value, 0, 255);
  background_dirty_ = true;
}

int32_t Window2Impl::Get_ContentsOpacity(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return contents_opacity_;
}

void Window2Impl::Put_ContentsOpacity(const int32_t& value,
                                      ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  contents_opacity_ = std::clamp(value, 0, 255);
}

int32_t Window2Impl::Get_Openness(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return 0;

  return openness_;
}

void Window2Impl::Put_Openness(const int32_t& value,
                               ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  openness_ = std::clamp(value, 0, 255);
}

scoped_refptr<Tone> Window2Impl::Get_Tone(ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return nullptr;

  return tone_;
}

void Window2Impl::Put_Tone(const scoped_refptr<Tone>& value,
                           ExceptionState& exception_state) {
  if (CheckDisposed(exception_state))
    return;

  *tone_ = *ToneImpl::From(value);
  background_dirty_ = true;
}

void Window2Impl::OnObjectDisposed() {
  node_.DisposeNode();

  screen()->PostTask(base::BindOnce(&GPUDestroyWindowInternal, agent_));
  agent_ = nullptr;
}

void Window2Impl::DrawableNodeHandlerInternal(
    DrawableNode::RenderStage stage,
    DrawableNode::RenderControllerParams* params) {
  if (bound_.width <= 4 || bound_.height <= 4)
    return;

  TextureAgent* windowskin_agent =
      windowskin_ ? windowskin_->GetAgent() : nullptr;
  TextureAgent* contents_agent = contents_ ? contents_->GetAgent() : nullptr;

  base::Rect padding_rect =
      base::Rect(padding_, padding_, std::max(0, bound_.width - padding_ * 2),
                 std::max(0, bound_.height - (padding_ + padding_bottom_)));

  if (stage == DrawableNode::RenderStage::BEFORE_RENDER) {
    screen()->PostTask(base::BindOnce(
        &GPUCompositeWindowQuadsInternal, params->device,
        params->command_encoder, agent_, bound_, scale_, pause_, active_,
        arrows_visible_, opacity_, openness_, back_opacity_, contents_opacity_,
        pause_index_, cursor_opacity_, contents_agent, windowskin_agent,
        origin_, cursor_rect_->AsBaseRect(), padding_rect, tone_->AsNormColor(),
        rgss3_style_, background_dirty_));
    background_dirty_ = false;
  } else if (stage == DrawableNode::RenderStage::ON_RENDERING) {
    LOG(INFO) << params->viewport;

    screen()->PostTask(
        base::BindOnce(&GPURenderWindowQuadsInternal, params->device,
                       params->renderpass_encoder, params->world_binding,
                       agent_, windowskin_agent, contents_agent, bound_,
                       padding_rect, params->viewport));
  }
}

void Window2Impl::ToneValueObserverInternal() {
  background_dirty_ = true;
}

}  // namespace content
