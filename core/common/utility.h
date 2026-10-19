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

#include <functional>

#include "core/common.h"

#include "core/definition.h"
#include "core/exception.h"
#include "core/object.h"

namespace urge {

struct ValueSlot {
  void ValueNotify() {
    if (change)
      change();
  }

  std::function<void()> change;
};

URGE_BINDING()
class Rect : public Object {
 public:
  URGE_BINDING()
  Rect(int32_t xv, int32_t yv, int32_t wv, int32_t hv) : data(xv, yv, wv, hv) {}
  URGE_BINDING()
  Rect(RefPtr<Rect> o) : data(o->data) {}
  URGE_BINDING()
  Rect() : data() {}

  URGE_BINDING()
  MARSHAL_DUMP(Rect);
  URGE_BINDING()
  MARSHAL_LOAD(Rect);

  URGE_BINDING()
  void Set(int32_t xv, int32_t yv, int32_t wv, int32_t hv) {
    data = RectI(xv, yv, wv, hv);
    slot.ValueNotify();
  }

  URGE_BINDING()
  void Set(RefPtr<Rect> rect) {
    if (rect) {
      data = rect->data;
      slot.ValueNotify();
    } else {
      throw Exception(Exception::kRGSSError, "cannot set null to rect.");
    }
  }

  URGE_BINDING()
  void Empty() {
    data = RectI();
    slot.ValueNotify();
  }

  URGE_BINDING()
  ATTR(int32_t, X) {
    if (value) {
      data.x = *value;
      slot.ValueNotify();
      return std::nullopt;
    }
    return data.x;
  }

  URGE_BINDING()
  ATTR(int32_t, Y) {
    if (value) {
      data.y = *value;
      slot.ValueNotify();
      return std::nullopt;
    }
    return data.y;
  }

  URGE_BINDING()
  ATTR(int32_t, Width) {
    if (value) {
      data.width = *value;
      slot.ValueNotify();
      return std::nullopt;
    }
    return data.width;
  }

  URGE_BINDING()
  ATTR(int32_t, Height) {
    if (value) {
      data.height = *value;
      slot.ValueNotify();
      return std::nullopt;
    }
    return data.height;
  }

  RectI data;
  ValueSlot slot;
};

URGE_BINDING()
class Color : public Object {
 public:
  static inline RefPtr<Color> White() {
    return MakeRefCounted<Color>(255.0f, 255.0f, 255.0f, 255.0f);
  }

  static inline RefPtr<Color> Black() {
    return MakeRefCounted<Color>(0.0f, 0.0f, 0.0f, 255.0f);
  }

  URGE_BINDING()
  Color(float r, float g, float b, float a = 255.f) : data(r, g, b, a) {}
  URGE_BINDING()
  Color(RefPtr<Color> o) : data(o->data) {}
  URGE_BINDING()
  Color() : data(0.0f) {}

  URGE_BINDING()
  MARSHAL_DUMP(Color);
  URGE_BINDING()
  MARSHAL_LOAD(Color);

  URGE_BINDING()
  void Set(float r, float g, float b, float a = 255.f) {
    data = glm::vec4(r, g, b, a);
  }

  URGE_BINDING()
  void Set(RefPtr<Color> color) {
    if (color) {
      data = color->data;
    } else {
      throw Exception(Exception::kRGSSError, "cannot set null to color.");
    }
  }

  URGE_BINDING()
  ATTR(float, Red) {
    if (value)
      data.r = *value;
    return data.r;
  }

  URGE_BINDING()
  ATTR(float, Green) {
    if (value)
      data.g = *value;
    return data.g;
  }

  URGE_BINDING()
  ATTR(float, Blue) {
    if (value)
      data.b = *value;
    return data.b;
  }

  URGE_BINDING()
  ATTR(float, Alpha) {
    if (value)
      data.a = *value;
    return data.a;
  }

  glm::vec4 Normalize() {
    glm::vec4 result = data;
    result.r /= 255.0f;
    result.g /= 255.0f;
    result.b /= 255.0f;
    result.a /= 255.0f;
    return result;
  }

  glm::vec4 data;
};

URGE_BINDING()
class Tone : public Object {
 public:
  URGE_BINDING()
  Tone(float r, float g, float b, float a = 0.f) : data(r, g, b, a) {}
  URGE_BINDING()
  Tone(RefPtr<Tone> o) : data(o->data) {}
  URGE_BINDING()
  Tone() : data(0.0f) {}

  URGE_BINDING()
  MARSHAL_DUMP(Tone);
  URGE_BINDING()
  MARSHAL_LOAD(Tone);

  URGE_BINDING()
  void Set(float r, float g, float b, float a = 0.f) {
    data = glm::vec4(r, g, b, a);
  }

  URGE_BINDING()
  void Set(RefPtr<Tone> tone) {
    if (tone) {
      data = tone->data;
    } else {
      throw Exception(Exception::kRGSSError, "cannot set null to tone.");
    }
  }

  URGE_BINDING()
  ATTR(float, Red) {
    if (value)
      data.r = *value;
    return data.r;
  }

  URGE_BINDING()
  ATTR(float, Green) {
    if (value)
      data.g = *value;
    return data.g;
  }

  URGE_BINDING()
  ATTR(float, Blue) {
    if (value)
      data.b = *value;
    return data.b;
  }

  URGE_BINDING()
  ATTR(float, Gray) {
    if (value)
      data.a = *value;
    return data.a;
  }

  glm::vec4 Normalize() {
    glm::vec4 result = data;
    result.r /= 255.0f;
    result.g /= 255.0f;
    result.b /= 255.0f;
    result.a /= 255.0f;
    return result;
  }

  glm::vec4 data;
};

URGE_BINDING()
class Vector2 : public Object {
 public:
  Vector2(glm::vec2 d) : data(d) {}

  URGE_BINDING()
  Vector2(float xv, float yv) : data(xv, yv) {}
  URGE_BINDING()
  Vector2(RefPtr<Vector2> o) : data(o->data) {}
  URGE_BINDING()
  Vector2(float v) : data(v) {}
  URGE_BINDING()
  Vector2() : data(0.0f) {}

  URGE_BINDING()
  void Set(float xv, float yv) { data = glm::vec2(xv, yv); }

  URGE_BINDING()
  void Set(RefPtr<Vector2> v) {
    if (v) {
      data = v->data;
    } else {
      throw Exception(Exception::kRGSSError, "cannot set null to vector.");
    }
  }

  glm::vec2 data;
};

URGE_BINDING()
class Vector3 : public Object {
 public:
  Vector3(glm::vec3 d) : data(d) {}

  URGE_BINDING()
  Vector3(float xv, float yv, float zv) : data(xv, yv, zv) {}
  URGE_BINDING()
  Vector3(RefPtr<Vector3> o) : data(o->data) {}
  URGE_BINDING()
  Vector3(float v) : data(v) {}
  URGE_BINDING()
  Vector3() : data(0.0f) {}

  URGE_BINDING()
  void Set(float xv, float yv, float zv) { data = glm::vec3(xv, yv, zv); }

  URGE_BINDING()
  void Set(RefPtr<Vector3> v) {
    if (v) {
      data = v->data;
    } else {
      throw Exception(Exception::kRGSSError, "cannot set null to vector.");
    }
  }

  glm::vec3 data;
};

URGE_BINDING()
class Vector4 : public Object {
 public:
  Vector4(glm::vec4 d) : data(d) {}

  URGE_BINDING()
  Vector4(float xv, float yv, float zv, float wv) : data(xv, yv, zv, wv) {}
  URGE_BINDING()
  Vector4(RefPtr<Vector4> o) : data(o->data) {}
  URGE_BINDING()
  Vector4(float v) : data(v) {}
  URGE_BINDING()
  Vector4() : data(0.0f) {}

  URGE_BINDING()
  void Set(float xv, float yv, float zv, float wv) {
    data = glm::vec4(xv, yv, zv, wv);
  }

  URGE_BINDING()
  void Set(RefPtr<Vector4> v) {
    if (v) {
      data = v->data;
    } else {
      throw Exception(Exception::kRGSSError, "cannot set null to vector.");
    }
  }

  glm::vec4 data;
};

}  // namespace urge
