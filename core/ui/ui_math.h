// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#pragma once

#include <vector>

#include "glm/glm.hpp"

namespace urge::ui_math {

struct Ray {
  glm::vec3 origin;
  glm::vec3 direction;
  float max_distance = 1.0e30f;
};
struct Vertex {
  glm::vec3 position;
  glm::vec2 uv;
};

bool Invert(const glm::mat4& matrix, glm::mat4* inverse);
bool ScreenRay(const glm::vec2& point, const glm::vec2& size,
               const glm::mat4& view_projection, Ray* ray);
bool Intersect(const Ray& ray, const glm::mat4& model, const glm::vec4& rect,
               glm::vec3* local, float* distance, bool double_sided = true);
bool ProjectedContains(const glm::vec2& ndc, const glm::mat4& mvp,
                       const glm::vec4& rect);
std::vector<Vertex> ClipQuad(const glm::vec4& rect, const glm::mat4& mvp,
                             const std::vector<glm::vec3>& screen_planes);
bool MaskPlanes(const glm::mat4& mvp, const glm::vec4& rect,
                std::vector<glm::vec3>* planes);

}  // namespace urge::ui_math
