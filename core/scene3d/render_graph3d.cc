// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "core/scene3d/render_graph3d.h"

#include <algorithm>
#include <set>
#include <utility>

#include "core/common/exception.h"
#include "core/common/logger.h"
#include "core/scene3d/camera3d.h"
#include "core/scene3d/render_context3d.h"
#include "glm/gtc/matrix_transform.hpp"
#include "core/scene3d/render_target3d.h"

namespace urge {

namespace {

bool IsFinalTarget(const std::string& name) {
  return name.empty() || name == "screen" || name == "output";
}

}

RenderGraphPass3D::RenderGraphPass3D(std::string name)
    : name_(std::move(name)) {}

RefPtr<GPUTextureView> RenderGraphPass3D::GetTexture(std::string name) {
  if (!graph_)
    return nullptr;
  RefPtr<RenderTarget3D> target = graph_->Target(name);
  return target ? MakeRefCounted<GPUTextureView>(target->color_view()) : nullptr;
}

RefPtr<GPURenderPassEncoder> RenderGraphPass3D::GetPass() {
  return context_ ? context_->GetPass() : nullptr;
}

RefPtr<RenderContext3D> RenderGraphPass3D::GetContext() {
  return context_;
}

int32_t RenderGraphPass3D::Width() {
  if (!context_ || !context_->GetTarget())
    return 0;
  return context_->GetTarget()->size().x;
}

int32_t RenderGraphPass3D::Height() {
  if (!context_ || !context_->GetTarget())
    return 0;
  return context_->GetTarget()->size().y;
}

RenderGraph3D::RenderGraph3D() = default;

RenderGraph3D::~RenderGraph3D() = default;

RefPtr<RenderGraphPass3D> RenderGraph3D::AddPass(std::string name) {
  if (name.empty())
    throw Exception(Exception::kRGSSError,
                    "render graph 3d: a pass needs a name.");

  for (const RefPtr<RenderGraphPass3D>& pass : passes_) {
    if (pass->GetName() == name)
      throw Exception(Exception::kRGSSError,
                      "render graph 3d: the pass '{}' already exists.", name);
  }

  auto pass = MakeRefCounted<RenderGraphPass3D>(std::move(name));
  pass->SetOwner(this);
  passes_.push_back(pass);
  return pass;
}

RefPtr<RenderGraphPass3D> RenderGraph3D::GetPass(std::string name) {
  for (const RefPtr<RenderGraphPass3D>& pass : passes_)
    if (pass->GetName() == name)
      return pass;
  return nullptr;
}

void RenderGraph3D::RemovePass(std::string name) {
  passes_.erase(
      std::remove_if(passes_.begin(), passes_.end(),
                     [&name](const RefPtr<RenderGraphPass3D>& pass) {
                       return pass->GetName() == name;
                     }),
      passes_.end());
}

void RenderGraph3D::Clear() {
  passes_.clear();
  targets_.clear();
}

std::vector<glm::mat4> BuildPointShadowFaces(const glm::vec3& eye,
                                             float range) {
  const float near_plane = 0.05f;
  const float far_plane = std::max(range, near_plane + 0.1f);
  const glm::mat4 projection =
      glm::perspective(glm::radians(90.0f), 1.0f, near_plane, far_plane);

  const glm::vec3 directions[6] = {{1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
                                   {0, -1, 0}, {0, 0, 1},  {0, 0, -1}};
  const glm::vec3 ups[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1},
                            {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};

  std::vector<glm::mat4> faces(6, glm::mat4(1.0f));
  for (int32_t i = 0; i < 6; ++i)
    faces[i] = projection * glm::lookAt(eye, eye + directions[i], ups[i]);
  return faces;
}

RefPtr<RenderTarget3D> RenderGraph3D::ShadowTarget(const std::string& name,
                                                  int32_t layers) {
  if (IsFinalTarget(name))
    return nullptr;

  auto it = shadow_targets_.find(name);
  if (it != shadow_targets_.end())
    return it->second;

  auto target = MakeRefCounted<RenderTarget3D>(shadow_size_, shadow_size_,
                                               false, std::max(1, layers));
  shadow_targets_[name] = target;
  return target;
}

RefPtr<RenderTarget3D> RenderGraph3D::Target(const std::string& name) {
  if (IsFinalTarget(name))
    return final_target_;

  if (shadow_targets_.find(name) != shadow_targets_.end())
    return nullptr;

  auto it = targets_.find(name);
  if (it != targets_.end())
    return it->second;

  if (!final_target_)
    return nullptr;

  const glm::ivec2 size = final_target_->size();
  auto target =
      MakeRefCounted<RenderTarget3D>(size.x, size.y, true);
  targets_[name] = target;
  return target;
}

std::vector<size_t> RenderGraph3D::BuildOrder() const {
  std::vector<size_t> order;
  order.reserve(passes_.size());

  std::vector<bool> emitted(passes_.size(), false);
  std::set<std::string> written;

  for (size_t step = 0; step < passes_.size(); ++step) {
    bool progressed = false;
    for (size_t index = 0; index < passes_.size(); ++index) {
      if (emitted[index])
        continue;

      const RenderGraphPass3D* pass = passes_[index].get();
      bool ready = true;
      for (const std::string& read : pass->reads()) {
        if (IsFinalTarget(read))
          continue;
        if (written.find(read) == written.end()) {
          ready = false;
          break;
        }
      }

      if (!ready)
        continue;

      order.push_back(index);
      emitted[index] = true;
      if (!IsFinalTarget(pass->color()))
        written.insert(pass->color());
      progressed = true;
    }

    if (!progressed)
      break;
  }

  for (size_t index = 0; index < passes_.size(); ++index)
    if (!emitted[index])
      order.push_back(index);

  return order;
}

void RenderGraph3D::Execute(RenderContext3D* context) {
  if (!context || !context->GetTarget())
    return;

  final_target_ = context->GetTarget().get();
  if (camera_)
    context->SetViewProjection(camera_);
  if (ambient_)
    context->SetAmbient(ambient_);

  const RefPtr<Camera3D> main_camera = context->GetCamera();
  const std::vector<size_t> order = BuildOrder();

  for (size_t index : order) {
    RenderGraphPass3D* pass = passes_[index].get();

    RefPtr<Camera3D> pass_camera = pass->camera();
    if (!pass_camera)
      pass_camera = camera_ ? camera_ : main_camera;
    if (pass_camera)
      context->SetViewProjection(pass_camera);

    if (pass->depth_only()) {
      if (pass->point_shadow_light()) {
        RefPtr<Light3D> light = pass->point_shadow_light();
        const glm::vec3 eye(light->world_matrix()[3]);
        const float range = std::max(light->range(), 0.1f);

        int32_t light_index = -1;
        if (context->GetQueue()) {
          const std::vector<RenderQueue3D::LightItem>& lights =
              context->GetQueue()->lights();
          for (size_t i = 0; i < lights.size(); ++i) {
            if (lights[i].type == 1 &&
                glm::length(lights[i].position - eye) < 0.001f) {
              light_index = static_cast<int32_t>(i);
              break;
            }
          }
        }

        RefPtr<RenderTarget3D> target = ShadowTarget(pass->color(), 6);
        if (!target)
          continue;

        const std::vector<glm::mat4> faces = BuildPointShadowFaces(eye, range);
        context->SetPointShadow(target, faces, light_index, range);

        for (int32_t face = 0; face < 6; ++face) {
          context->SetViewOverride(faces[face]);
          context->BeginPass(target, pass->clear_color(), pass->clear_depth(),
                             face);

          for (const std::string& tag : pass->tags())
            context->DrawTag(tag);

          if (pass->GetProc()) {
            pass->SetContext(context);
            pass->GetProc()->Invoke(pass);
            pass->SetContext(nullptr);
          }

          context->EndPass();
        }

        if (pass_camera)
          context->SetViewProjection(pass_camera);
        continue;
      }

      const int32_t cascades = std::clamp(pass->cascades(), 1, 4);
      RefPtr<RenderTarget3D> shadow = ShadowTarget(pass->color(), cascades);
      if (!shadow)
        continue;

      const bool use_cascades =
          cascades > 1 && pass_camera && pass->shadow_light();
      if (use_cascades) {
        const glm::vec3 light_direction =
            -glm::normalize(glm::vec3(pass->shadow_light()->world_matrix()[2]));
        const glm::ivec2 size = final_target_ ? final_target_->size()
                                              : glm::ivec2(1);
        const std::vector<glm::mat4> matrices = pass_camera->ComputeCascades(
            light_direction, cascades, pass->split_lambda(), size,
            pass->shadow_distance(), shadow->Width());

        context->SetShadowCascades(cascades);
        context->SetShadowFade(pass->cascade_fade());
        context->SetShadowSplits(pass_camera->ComputeSplits(
            cascades, pass->split_lambda(), pass->shadow_distance()));

        for (int32_t cascade = 0; cascade < cascades; ++cascade) {
          context->SetShadow(shadow, matrices[cascade], cascade);
          context->SetViewOverride(matrices[cascade]);
          context->BeginPass(shadow, pass->clear_color(), pass->clear_depth(),
                             cascade);

          for (const std::string& tag : pass->tags())
            context->DrawTag(tag);

          if (pass->GetProc()) {
            pass->SetContext(context);
            pass->GetProc()->Invoke(pass);
            pass->SetContext(nullptr);
          }

          context->EndPass();
        }

        context->SetViewProjection(pass_camera);
        continue;
      }

      if (pass_camera) {
        context->SetShadow(shadow,
                           pass_camera->view_projection(shadow->size()));
      }

      context->BeginPass(shadow, pass->clear_color(), pass->clear_depth());

      for (const std::string& tag : pass->tags())
        context->DrawTag(tag);

      if (pass->GetProc()) {
        pass->SetContext(context);
        pass->GetProc()->Invoke(pass);
        pass->SetContext(nullptr);
      }

      context->EndPass();
      continue;
    }

    RefPtr<RenderTarget3D> target = Target(pass->color());
    if (!target)
      continue;

    if (pass->sort_back_to_front()) {
      for (const std::string& tag : pass->tags())
        context->GetQueue()->SortByDistance(tag, true);
    }

    context->BeginPass(target, pass->clear_color(), pass->clear_depth());

    for (const std::string& tag : pass->tags())
      context->DrawTag(tag);

    if (pass->GetProc()) {
      pass->SetContext(context);
      pass->GetProc()->Invoke(pass);
      pass->SetContext(nullptr);
    }

    context->EndPass();
  }

  final_target_ = nullptr;
}

}
