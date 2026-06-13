// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include "content/common/utility.h"
#include "content/scene/node.h"

namespace content {

URGE_BINDING()
class Light : public Node {
 public:
  Light();

  Light(const Light&) = delete;
  Light& operator=(const Light&) = delete;

 public:
  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Color, scoped_refptr<Vector3>);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Intensity, float);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(CastShadow, bool);

 private:
};

URGE_BINDING()
class DirectionalLight : public Light {
 public:
  DirectionalLight();

  DirectionalLight(const DirectionalLight&) = delete;
  DirectionalLight& operator=(const DirectionalLight&) = delete;

 public:
  URGE_BINDING()
  static scoped_refptr<DirectionalLight> New(URGE_EXCEPTION);

 private:
};

URGE_BINDING()
class PointLight : public Light {
 public:
  PointLight();

  PointLight(const PointLight&) = delete;
  PointLight& operator=(const PointLight&) = delete;

 public:
  URGE_BINDING()
  static scoped_refptr<PointLight> New(URGE_EXCEPTION);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Range, float);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Attenuation, float);

 private:
};

URGE_BINDING()
class SpotLight : public Light {
 public:
  SpotLight();

  SpotLight(const SpotLight&) = delete;
  SpotLight& operator=(const SpotLight&) = delete;

 public:
  URGE_BINDING()
  static scoped_refptr<SpotLight> New(URGE_EXCEPTION);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Range, float);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Attenuation, float);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(Angle, float);

  URGE_BINDING()
  URGE_ATTRIBUTE_DECLARE(AngleAttenuation, float);

 private:
};

}  // namespace content
