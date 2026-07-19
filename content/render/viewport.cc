// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/render/viewport.h"

#include <limits>

#include "glm/gtc/matrix_access.hpp"

namespace content {

namespace {

// Extract 6 frustum planes (world space) from view-projection matrix.
// Uses Gribb-Hartmann method.
void ExtractFrustumPlanes(const glm::mat4& vp, glm::vec4 planes[6]) {
  glm::vec4 row0 = glm::row(vp, 0);
  glm::vec4 row1 = glm::row(vp, 1);
  glm::vec4 row2 = glm::row(vp, 2);
  glm::vec4 row3 = glm::row(vp, 3);

  // Left:   row3 + row0
  // Right:  row3 - row0
  // Bottom: row3 + row1
  // Top:    row3 - row1
  // Near:   row3 + row2
  // Far:    row3 - row2
  planes[0] = row3 + row0;
  planes[1] = row3 - row0;
  planes[2] = row3 + row1;
  planes[3] = row3 - row1;
  planes[4] = row3 + row2;
  planes[5] = row3 - row2;

  for (int i = 0; i < 6; i++) {
    float len = glm::length(glm::vec3(planes[i]));
    planes[i] /= len;
  }
}

// Test AABB (in world space) against frustum planes.
// Returns true if AABB is inside or intersects the frustum.
bool AABBInFrustum(const glm::vec3& world_min,
                   const glm::vec3& world_max,
                   const glm::vec4 planes[6]) {
  for (int p = 0; p < 6; p++) {
    const glm::vec3 n(planes[p]);

    // n-vertex: the AABB corner furthest in the negative normal direction
    glm::vec3 nvert;
    nvert.x = (n.x > 0.f) ? world_min.x : world_max.x;
    nvert.y = (n.y > 0.f) ? world_min.y : world_max.y;
    nvert.z = (n.z > 0.f) ? world_min.z : world_max.z;

    float d = glm::dot(n, nvert) + planes[p].w;
    if (d < 0.f)
      return false;  // entirely outside this plane
  }
  return true;
}

// Transform object-space AABB to world space via model matrix.
void TransformAABB(const glm::vec3& obj_min,
                   const glm::vec3& obj_max,
                   const glm::mat4& model,
                   glm::vec3& world_min,
                   glm::vec3& world_max) {
  const glm::vec3 corners[8] = {
      {obj_min.x, obj_min.y, obj_min.z}, {obj_max.x, obj_min.y, obj_min.z},
      {obj_min.x, obj_max.y, obj_min.z}, {obj_max.x, obj_max.y, obj_min.z},
      {obj_min.x, obj_min.y, obj_max.z}, {obj_max.x, obj_min.y, obj_max.z},
      {obj_min.x, obj_max.y, obj_max.z}, {obj_max.x, obj_max.y, obj_max.z},
  };

  world_min = glm::vec3(std::numeric_limits<float>::max());
  world_max = glm::vec3(std::numeric_limits<float>::lowest());

  for (const auto& corner : corners) {
    glm::vec3 pt(model * glm::vec4(corner, 1.f));
    world_min = glm::min(world_min, pt);
    world_max = glm::max(world_max, pt);
  }
}

}  // namespace

///
/// RenderContext
///

RenderContext::RenderContext(World* world,
                             scoped_refptr<GPUTextureView> rtv,
                             scoped_refptr<GPUTextureView> dsv)
    : world_(world), render_target_view_(rtv), depth_stencil_view_(dsv) {}

scoped_refptr<GPUQueue> RenderContext::GetQueue(URGE_EXCEPTION) {
  return nullptr;
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

  // 1. Extract frustum planes (world space) from view-projection matrix
  glm::mat4 vp = camera->GetViewProjectionMatrix();
  glm::vec4 planes[6];
  ExtractFrustumPlanes(vp, planes);

  // 2. Test each renderer's world-space AABB against the frustum
  for (auto* renderer : world_->renderers_) {
    const glm::vec3& obj_min = renderer->bounds_min_data();
    const glm::vec3& obj_max = renderer->bounds_max_data();

    // Transform AABB from object to world space
    const glm::mat4 model = glm::mat4(renderer->GetModelMatrix());
    glm::vec3 world_min, world_max;
    TransformAABB(obj_min, obj_max, model, world_min, world_max);

    if (AABBInFrustum(world_min, world_max, planes))
      results->visible_renderers_.push_back(renderer);
  }

  return results;
}

void RenderContext::DrawRenderers(
    scoped_refptr<GPURenderPassEncoder> pass,
    scoped_refptr<CullingResults> culling_results,
    scoped_refptr<DrawingSettings> drawing_settings,
    scoped_refptr<FilteringSettings> filtering_settings,
    URGE_EXCEPTION) {}

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
    return visible_renderers_[index];
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
  if (!render_process_.is_null()) {
    // Prepare for frame
    PrepareFrame();

    // Render context
    auto render_context = Object::Create<RenderContext>(
        world_.get(), render_target, depth_stencil);

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

void Viewport::PrepareFrame() {
  // Vertex buffer / Index buffer
  for (auto& renderer : world_->renderers_)
    if (auto* mesh = renderer->mesh(); mesh)
      mesh->UpdateGPUBuffer();
}

}  // namespace content
