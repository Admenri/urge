// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/viewport.h"

#include <limits>

#include "glm/gtc/matrix_access.hpp"
#include "glm/gtc/matrix_inverse.hpp"

#include "content/render/frustum.h"
#include "content/render/graphics.h"

namespace content {

///
/// RenderContext
///

RenderContext::RenderContext(World* world,
                             scoped_refptr<GPUQueue> queue,
                             scoped_refptr<GPUTextureView> rtv,
                             scoped_refptr<GPUTextureView> dsv)
    : world_(world),
      queue_(queue),
      render_target_view_(rtv),
      depth_stencil_view_(dsv) {}

scoped_refptr<GPUQueue> RenderContext::GetQueue(URGE_EXCEPTION) {
  return queue_;
}

scoped_refptr<GPUTextureView> RenderContext::GetRenderTargetView(
    URGE_EXCEPTION) {
  return render_target_view_;
}

scoped_refptr<GPUTextureView> RenderContext::GetDepthStencilView(
    URGE_EXCEPTION) {
  return depth_stencil_view_;
}

scoped_refptr<CullingResults> RenderContext::Cull(scoped_refptr<Camera> camera,
                                                  URGE_EXCEPTION) {
  auto results = Object::Create<CullingResults>();
  if (!camera)
    return results;

  // Camera world position (double) + rotation-only view for origin-centered
  // frustum
  auto* camera_transform = camera->transform();
  const auto camera_position = camera_transform->position();
  const glm::mat4 camera_view =
      glm::affineInverse(camera_transform->GetForwardMatrix());
  const glm::mat4 camera_view_projection =
      camera->GetProjectionMatrix() * camera_view;

  // Frustum planes centered at origin (camera-relative world space)
  Frustum frustum;
  frustum.ExtractFromMatrix(camera_view_projection);

  for (auto* renderer : world_->renderers_) {
    // 1. Fast reject: culling mask
    if (!(renderer->layer() & camera->culling_mask()))
      continue;

    // 2. Compute camera-relative model: translation offset only
    const glm::mat4 rel_model = renderer->GetModelMatrix(camera_position);

    // 3. Transform AABB to camera-relative space (double precision)
    AABB renderer_local_aabb(renderer->bounds_min_data(),
                             renderer->bounds_max_data());
    renderer_local_aabb.Transform(rel_model);

    // 4. Frustum cull against origin-centered planes (single precision)
    if (frustum.IntersectsAABB(renderer_local_aabb)) {
      Renderable renderable;
      renderable.host_node = renderer;
      renderable.cast_camera = camera.get();
      renderable.relative_transform = glm::mat4(rel_model);
      results->visible_renderers_.push_back(std::move(renderable));
    }
  }

  return results;
}

void RenderContext::DrawRenderers(
    scoped_refptr<GPURenderPassEncoder> pass,
    scoped_refptr<CullingResults> culling_results,
    scoped_refptr<DrawingSettings> drawing_settings,
    scoped_refptr<FilteringSettings> filtering_settings,
    URGE_EXCEPTION) {
  // Filter material
  uint64_t culling_mask =
      filtering_settings ? filtering_settings->cullingMask : 0;
  uint32_t min_render_queue =
      filtering_settings ? filtering_settings->minRenderQueue : 0;
  uint32_t max_render_queue = filtering_settings
                                  ? filtering_settings->maxRenderQueue
                                  : std::numeric_limits<uint32_t>::max();

  for (auto& renderable : culling_results->visible_renderers_) {
    auto* renderer = renderable.host_node;
    // Culling mask
    if (renderer->layer() & culling_mask) {
      for (auto& material : renderer->materials()) {
        // Render queue
        if (material->render_queue() >= min_render_queue &&
            material->render_queue() <= max_render_queue) {
          for (auto& pass : material->passes()) {
            // Pass name
            if (pass->passName == drawing_settings->passName) {
              // Collect renderable
            }
          }
        }
      }
    }
  }

  // Sorting by criteria
  if (drawing_settings->sortingSettings) {
  }
}

///
/// CullingResults
///

CullingResults::CullingResults() {}

uint32_t CullingResults::GetVisibleObjectCount(URGE_EXCEPTION) {
  return static_cast<uint32_t>(visible_renderers_.size());
}

scoped_refptr<MeshRenderer> CullingResults::GetVisibleObjectAt(uint32_t index,
                                                               URGE_EXCEPTION) {
  if (index < visible_renderers_.size())
    return visible_renderers_[index].host_node;
  return nullptr;
}

///
/// Viewport
///

// static
scoped_refptr<Viewport> Viewport::New(URGE_EXCEPTION) {
  return Object::Create<Viewport>();
}

Viewport::Viewport() {}

URGE_ATTRIBUTE_DEFINE(
    Viewport,
    World,
    scoped_refptr<World>,
    { return world_; },
    { world_ = value; });

URGE_BINDING()
void Viewport::SetupRenderProcess(RenderCallback callback, URGE_EXCEPTION) {
  render_process_ = callback;
}

void Viewport::Render(scoped_refptr<GPUTextureView> render_target,
                      scoped_refptr<GPUTextureView> depth_stencil,
                      URGE_EXCEPTION) {
  auto* graphics = Graphics::Instance();
  auto* gfx = graphics->gfx();

  if (!render_process_.is_null()) {
    // Prepare for frame
    PrepareFrame(gfx);

    // Render context
    auto render_context = Object::Create<RenderContext>(
        world_.get(), gfx->queue(), render_target, depth_stencil);

    // Collected cameras (viewports)
    std::vector<scoped_refptr<Camera>> cameras(world_->cameras_.begin(),
                                               world_->cameras_.end());

    // Calling on custom rendering process
    render_process_.Run(render_context, std::move(cameras));
  } else {
    exception_state.Throw(ExceptionCode::CONTENT_ERROR, "no renderer setup.");
    return;
  }
}

void Viewport::PrepareFrame(renderer::RenderDevice* gfx) {
  // Vertex buffer / Index buffer
  for (auto& renderer : world_->renderers_)
    if (auto* mesh = renderer->mesh(); mesh)
      mesh->UpdateGPUBuffer(gfx);
}

}  // namespace content
