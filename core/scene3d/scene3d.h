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

#pragma once

#include <cstdint>
#include <vector>

#include "core/common/definition.h"
#include "core/common/object.h"
#include "core/common/utility.h"
#include "core/scene3d/camera3d.h"
#include "core/scene3d/node3d.h"

namespace urge {

URGE_BINDING()
class Scene3D : public Node3D {
 public:
  URGE_BINDING()
  Scene3D();
  ~Scene3D() override;

  URGE_BINDING(Name : "ambient")
  ATTR(MaybePtr<Color>, Ambient) {
    if (value.has_value()) {
      if (*value)
        ambient_ = *value;
      return std::nullopt;
    }
    return ambient_;
  }

  URGE_BINDING()
  void Add(MaybePtr<Node3D> node);
  URGE_BINDING()
  std::vector<RefPtr<Camera3D>> Cameras();
  URGE_BINDING()
  int32_t NodeCount() const;

  const RefPtr<Color>& ambient() const { return ambient_; }

 private:
  void CollectCameras(Node3D* node, std::vector<RefPtr<Camera3D>>& out);
  int32_t CountNodes(const Node3D* node) const;

  RefPtr<Color> ambient_ = MakeRefCounted<Color>(26.0f, 28.0f, 36.0f, 255.0f);
};

}
