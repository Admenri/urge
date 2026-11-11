// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#include "core/ui/ui_raycaster.h"

#include <functional>

#include "core/graphics/graphics.h"
#include "core/ui/ui.h"
#include "core/ui/ui_state.h"
namespace urge {
int64_t UIRaycastHit::TargetId() const {
  return target && target->ui_state() ? target->ui_state()->Id() : 0;
}
RefPtr<Vector3> UIRaycastHit::Position() const {
  return MakeRefCounted<Vector3>(position.x, position.y, position.z);
}
RefPtr<Vector3> UIRaycastHit::LocalPosition() const {
  return MakeRefCounted<Vector3>(local_position.x, local_position.y,
                                 local_position.z);
}
RefPtr<Vector2> UIRaycastHit::UV() const {
  return MakeRefCounted<Vector2>(uv.x, uv.y);
}
UIRaycaster::UIRaycaster(RefPtr<Node> root)
    : root_(root.get()), owned_root_(root) {}
UIRaycaster::UIRaycaster(Node* root, bool internal) : root_(root) {
  (void)internal;
}
RefPtr<UIRaycastHit> UIRaycaster::RaycastScreen(float x, float y) {
  glm::vec2 size(640, 480);
  if (root_ && root_->ui_state())
    size = root_->ui_state()->target_size;
  else if (Graphics::TryGet())
    size = {Graphics::Get().Width(), Graphics::Get().Height()};
  return RaycastScreen(x, y, static_cast<int>(size.x),
                       static_cast<int>(size.y));
}
RefPtr<UIRaycastHit> UIRaycaster::RaycastScreen(float x, float y, int32_t width,
                                                int32_t height) {
  if (!root_ || root_->IsDisposed()) return nullptr;
  Node* layout_root = root_;
  while (layout_root->scene_parent()) layout_root = layout_root->scene_parent();
  UI::Get().PrepareTree(layout_root, glm::vec2(width, height));
  const glm::vec2 point(x, y);
  return Cast(nullptr, &point, glm::vec2(width, height));
}
RefPtr<UIRaycastHit> UIRaycaster::Raycast(RefPtr<Vector3> origin,
                                          RefPtr<Vector3> direction,
                                          float maximum) {
  if (!root_ || root_->IsDisposed() || !origin || !direction) return nullptr;
  const float length = glm::length(direction->data);
  if (length < 1e-6f || maximum < 0) return nullptr;
  Node* layout_root = root_;
  while (layout_root->scene_parent()) layout_root = layout_root->scene_parent();
  UI::Get().PrepareTree(layout_root, root_->ui_state()
                                         ? root_->ui_state()->target_size
                                         : glm::vec2(640, 480));
  const ui_math::Ray ray{origin->data, direction->data / length, maximum};
  return Cast(
      &ray, nullptr,
      root_->ui_state() ? root_->ui_state()->target_size : glm::vec2(640, 480));
}
RefPtr<UIRaycastHit> UIRaycaster::Cast(const ui_math::Ray* world_ray,
                                       const glm::vec2* point,
                                       const glm::vec2& size) {
  RefPtr<UIRaycastHit> result;
  std::function<void(Node*)> visit = [&](Node* node) {
    if (!node->visible_in_tree()) return;
    UIState* state = node->ui_state();
    if (state && !state->blocks_raycasts_) return;
    if (state && state->raycast_target_) {
      ui_math::Ray ray;
      const auto vp = UIViewProjection(node, size);
      const bool valid_ray = world_ray
                                 ? (ray = *world_ray, true)
                                 : ui_math::ScreenRay(*point, size, vp, &ray);
      glm::vec3 local;
      float distance;
      if (valid_ray && ui_math::Intersect(ray, node->resolved_world_matrix(),
                                          state->hit_bounds(), &local,
                                          &distance, state->double_sided_)) {
        const auto world = node->resolved_world_matrix() * glm::vec4(local, 1);
        const auto clip = vp * world;
        bool visible = clip.w > 1e-6f && clip.z >= -1e-5f &&
                       clip.z <= clip.w + 1e-5f &&
                       std::abs(clip.x) <= clip.w + 1e-5f &&
                       std::abs(clip.y) <= clip.w + 1e-5f;
        bool masks_valid = true;
        const auto planes = state->clip_planes(size, &masks_valid);
        for (auto plane : planes)
          if (glm::dot(plane, glm::vec3(clip.x, clip.y, clip.w)) < -1e-5f)
            visible = false;
        if (visible && masks_valid &&
            (!sort_by_distance_ || !result || distance <= result->distance)) {
          result = MakeRefCounted<UIRaycastHit>();
          result->target = node;
          result->position = glm::vec3(world);
          result->local_position = local;
          result->distance = distance;
          const auto rect = state->hit_bounds();
          result->uv =
              (glm::vec2(local) - glm::vec2(rect)) / glm::vec2(rect.z, rect.w);
        }
      }
    }
    for (Node* child : node->children_raw()) visit(child);
  };
  if (root_ && !root_->IsDisposed()) visit(root_);
  return result;
}
}  // namespace urge
