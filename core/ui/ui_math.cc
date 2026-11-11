// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#include "core/ui/ui_math.h"

#include <algorithm>
#include <cmath>

namespace urge::ui_math {
namespace {
constexpr float kEpsilon = 1.0e-6f;
bool Finite(const glm::vec4& p) {
  return std::isfinite(p.x) && std::isfinite(p.y) && std::isfinite(p.z) &&
         std::isfinite(p.w);
}
std::vector<glm::vec4> Corners(const glm::vec4& r) {
  return {{r.x, r.y, 0, 1},
          {r.x + r.z, r.y, 0, 1},
          {r.x + r.z, r.y + r.w, 0, 1},
          {r.x, r.y + r.w, 0, 1}};
}
}  // namespace

bool Invert(const glm::mat4& matrix, glm::mat4* inverse) {
  const float determinant = glm::determinant(matrix);
  if (!std::isfinite(determinant) || determinant == 0) return false;
  *inverse = glm::inverse(matrix);
  for (int i = 0; i < 4; ++i)
    if (!Finite((*inverse)[i])) return false;
  return true;
}

bool ScreenRay(const glm::vec2& point, const glm::vec2& size,
               const glm::mat4& vp, Ray* ray) {
  if (size.x <= 0 || size.y <= 0 || point.x < 0 || point.y < 0 ||
      point.x > size.x || point.y > size.y)
    return false;
  glm::mat4 inverse;
  if (!Invert(vp, &inverse)) return false;
  const glm::vec2 ndc(point.x / size.x * 2 - 1, 1 - point.y / size.y * 2);
  // WebGPU clips to z in [0,w], including the engine's default 2D camera.
  glm::vec4 near = inverse * glm::vec4(ndc, 0, 1);
  glm::vec4 far = inverse * glm::vec4(ndc, 1, 1);
  if (!Finite(near) || !Finite(far) || std::abs(near.w) < kEpsilon ||
      std::abs(far.w) < kEpsilon)
    return false;
  near /= near.w;
  far /= far.w;
  const glm::vec3 direction = glm::vec3(far - near);
  const float length = glm::length(direction);
  if (!std::isfinite(length) || length < kEpsilon) return false;
  ray->origin = glm::vec3(near);
  ray->direction = direction / length;
  ray->max_distance = length;
  return true;
}

bool Intersect(const Ray& ray, const glm::mat4& model, const glm::vec4& rect,
               glm::vec3* local, float* distance, bool double_sided) {
  if (!(rect.z > 0 && rect.w > 0)) return false;
  glm::mat4 inverse;
  if (!Invert(model, &inverse)) return false;
  const glm::vec4 origin = inverse * glm::vec4(ray.origin, 1);
  const glm::vec4 direction = inverse * glm::vec4(ray.direction, 0);
  if (!Finite(origin) || !Finite(direction) || std::abs(direction.z) < kEpsilon)
    return false;
  if (!double_sided) {
    const glm::vec3 normal =
        glm::cross(glm::vec3(model[0]), glm::vec3(model[1]));
    if (glm::dot(normal, ray.direction) >= 0) return false;
  }
  // Do not normalize after inverse scaling: t stays in world-ray units.
  const float t = -origin.z / direction.z;
  if (!std::isfinite(t) || t < -kEpsilon || t > ray.max_distance + kEpsilon)
    return false;
  const glm::vec3 p = glm::vec3(origin) + t * glm::vec3(direction);
  if (p.x < rect.x - kEpsilon || p.y < rect.y - kEpsilon ||
      p.x > rect.x + rect.z + kEpsilon || p.y > rect.y + rect.w + kEpsilon)
    return false;
  *local = p;
  *distance = std::max(0.0f, t);
  return true;
}

bool MaskPlanes(const glm::mat4& mvp, const glm::vec4& rect,
                std::vector<glm::vec3>* planes) {
  auto corners = Corners(rect);
  for (auto& point : corners) {
    point = mvp * point;
    if (!Finite(point)) return false;
  }
  // Clip the mask before projection. A rotated world-space panel can cross
  // the near/eye plane; its visible portion still masks descendants correctly.
  for (const auto& clip : std::vector<glm::vec4>{{1, 0, 0, 1},
                                                 {-1, 0, 0, 1},
                                                 {0, 1, 0, 1},
                                                 {0, -1, 0, 1},
                                                 {0, 0, 1, 0},
                                                 {0, 0, -1, 1}}) {
    if (corners.empty()) return false;
    std::vector<glm::vec4> output;
    auto previous = corners.back();
    float a = glm::dot(clip, previous);
    for (auto current : corners) {
      float b = glm::dot(clip, current);
      if ((a >= 0) != (b >= 0))
        output.push_back(glm::mix(previous, current, a / (a - b)));
      if (b >= 0) output.push_back(current);
      previous = current;
      a = b;
    }
    corners = std::move(output);
  }
  if (corners.size() < 3) return false;
  glm::vec3 center(0);
  for (auto point : corners) {
    if (point.w < kEpsilon) return false;
    center += glm::vec3(point.x / point.w, point.y / point.w, 1);
  }
  center /= static_cast<float>(corners.size());
  std::vector<glm::vec3> result;
  for (size_t i = 0; i < corners.size(); ++i) {
    const auto a = corners[i], b = corners[(i + 1) % corners.size()];
    glm::vec3 plane =
        glm::cross(glm::vec3(a.x, a.y, a.w), glm::vec3(b.x, b.y, b.w));
    const float length = glm::length(plane);
    if (length < kEpsilon) continue;
    plane /= length;
    if (glm::dot(plane, center) < 0) plane = -plane;
    result.push_back(plane);
  }
  if (result.size() < 3) return false;
  planes->insert(planes->end(), result.begin(), result.end());
  return true;
}

bool ProjectedContains(const glm::vec2& ndc, const glm::mat4& mvp,
                       const glm::vec4& rect) {
  std::vector<glm::vec3> planes;
  if (!MaskPlanes(mvp, rect, &planes)) return false;
  for (const auto& plane : planes)
    if (glm::dot(plane, glm::vec3(ndc, 1)) < -kEpsilon) return false;
  return true;
}

std::vector<Vertex> ClipQuad(const glm::vec4& r, const glm::mat4& mvp,
                             const std::vector<glm::vec3>& screen_planes) {
  std::vector<Vertex> polygon = {{{r.x, r.y, 0}, {0, 0}},
                                 {{r.x + r.z, r.y, 0}, {1, 0}},
                                 {{r.x + r.z, r.y + r.w, 0}, {1, 1}},
                                 {{r.x, r.y + r.w, 0}, {0, 1}}};
  std::vector<glm::vec4> planes = {{1, 0, 0, 1},  {-1, 0, 0, 1}, {0, 1, 0, 1},
                                   {0, -1, 0, 1}, {0, 0, 1, 0},  {0, 0, -1, 1}};
  for (auto p : screen_planes) planes.emplace_back(p.x, p.y, 0, p.z);
  for (const auto& plane : planes) {
    std::vector<Vertex> output;
    if (polygon.empty()) break;
    Vertex previous = polygon.back();
    float a = glm::dot(plane, mvp * glm::vec4(previous.position, 1));
    for (auto current : polygon) {
      const float b = glm::dot(plane, mvp * glm::vec4(current.position, 1));
      if ((a >= 0) != (b >= 0)) {
        const float t = a / (a - b);
        output.push_back({glm::mix(previous.position, current.position, t),
                          glm::mix(previous.uv, current.uv, t)});
      }
      if (b >= 0) output.push_back(current);
      previous = current;
      a = b;
    }
    polygon = std::move(output);
  }
  return polygon;
}
}  // namespace urge::ui_math
