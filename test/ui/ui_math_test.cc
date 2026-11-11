// Copyright (c) 2026 Admenri Adev. SPDX-License-Identifier: MIT
#include "core/ui/ui_math.h"

#include <cmath>
#include <iostream>
#include <stdexcept>

#include "glm/gtc/matrix_transform.hpp"
using namespace urge::ui_math;
void Require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
int main() {
  try {
    const glm::vec4 rect(-2, -1, 4, 2);
    auto model =
        glm::translate(glm::mat4(1), glm::vec3(4, 7, -6)) *
        glm::rotate(glm::mat4(1), 0.73f, glm::normalize(glm::vec3(1, 2, 3))) *
        glm::scale(glm::mat4(1), glm::vec3(-2, 0.3f, 4));
    glm::vec3 point(model * glm::vec4(0.6f, -0.4f, 0, 1));
    glm::vec3 normal =
        glm::normalize(glm::cross(glm::vec3(model[0]), glm::vec3(model[1])));
    Ray ray{point + normal * 5.0f, -normal, 10};
    glm::vec3 local;
    float distance;
    Require(Intersect(ray, model, rect, &local, &distance),
            "transformed rectangle hit");
    Require(glm::length(local - glm::vec3(0.6f, -0.4f, 0)) < 1e-4f,
            "inverse local point");
    Require(std::abs(distance - 5) < 1e-4f,
            "world distance survives nonuniform mirror scale");
    ray.max_distance = 4;
    Require(!Intersect(ray, model, rect, &local, &distance), "range rejected");
    Require(!Intersect(ray, glm::scale(glm::mat4(1), glm::vec3(1, 0, 1)), rect,
                       &local, &distance),
            "singular matrix rejected");
    Require(!Intersect({{0, 0, 1}, {1, 0, 0}, 10}, glm::mat4(1), rect, &local,
                       &distance),
            "parallel ray rejected");
    Require(!Intersect({{0, 0, -1}, {0, 0, -1}, 10}, glm::mat4(1), rect, &local,
                       &distance),
            "behind ray rejected");
    auto tiny = glm::scale(glm::mat4(1), glm::vec3(0.00001f));
    Require(
        Intersect({{0, 0, 1}, {0, 0, -1}, 10}, tiny, rect, &local, &distance),
        "small invertible transform");
    auto vp = glm::perspective(glm::radians(60.0f), 1.5f, 0.1f, 100.0f);
    Require(ScreenRay({300, 200}, {600, 400}, vp, &ray),
            "perspective screen ray");
    Require(std::abs(ray.origin.z + 0.1f) < 1e-5f, "WebGPU near plane");
    auto panel = glm::translate(glm::mat4(1), glm::vec3(0, 0, -3)) *
                 glm::rotate(glm::mat4(1), 0.5f, glm::vec3(0, 1, 0));
    Require(Intersect(ray, panel, rect, &local, &distance),
            "perspective panel hit");
    Require(!ScreenRay({-1, 0}, {600, 400}, vp, &ray),
            "outside viewport rejected");
    auto mask = glm::rotate(glm::mat4(1), 0.5f, glm::vec3(0, 0, 1));
    std::vector<glm::vec3> planes;
    Require(MaskPlanes(mask, {-0.5f, -0.2f, 1, 0.4f}, &planes), "rotated mask");
    Require(ProjectedContains({0, 0}, mask, {-0.5f, -0.2f, 1, 0.4f}),
            "mask center");
    Require(!ProjectedContains({0.5f, 0.2f}, mask, {-0.5f, -0.2f, 1, 0.4f}),
            "AABB false positive rejected");
    auto polygon = ClipQuad({-1, -1, 2, 2}, glm::mat4(1), planes);
    Require(polygon.size() >= 3, "masked polygon survives");
    for (auto vertex : polygon) {
      for (auto plane : planes)
        Require(glm::dot(plane, glm::vec3(vertex.position.x, vertex.position.y,
                                          1)) >= -1e-5f,
                "clipped vertex within mask");
      Require(glm::length(vertex.uv -
                          (glm::vec2(vertex.position) + 1.0f) / 2.0f) < 1e-5f,
              "UV interpolation");
    }
    planes.clear();
    auto crossing = vp * glm::translate(glm::mat4(1), glm::vec3(0, 0, -0.4f)) *
                    glm::rotate(glm::mat4(1), 1.0f, glm::vec3(0, 1, 0));
    Require(MaskPlanes(crossing, {-1, -1, 2, 2}, &planes),
            "mask crossing eye plane");
    std::cout << "UI math: all checks passed\n";
    return 0;
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
