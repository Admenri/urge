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

#include "core/scene3d/scene3d.h"

#include <utility>

namespace urge {

Scene3D::Scene3D() = default;

Scene3D::~Scene3D() = default;

void Scene3D::Add(RefPtr<Node3D> node) {
  AddChild(std::move(node));
}

void Scene3D::CollectCameras(Node3D* node,
                             std::vector<RefPtr<Camera3D>>& out) {
  if (Camera3D* camera = dynamic_cast<Camera3D*>(node))
    out.push_back(RefPtr<Camera3D>(camera));

  for (const RefPtr<Node3D>& child : node->children())
    CollectCameras(child.get(), out);
}

std::vector<RefPtr<Camera3D>> Scene3D::Cameras() {
  std::vector<RefPtr<Camera3D>> cameras;
  CollectCameras(this, cameras);
  return cameras;
}

int32_t Scene3D::CountNodes(const Node3D* node) const {
  int32_t count = 1;
  for (const RefPtr<Node3D>& child : node->children())
    count += CountNodes(child.get());
  return count;
}

int32_t Scene3D::NodeCount() const {
  return CountNodes(this) - 1;
}

}
