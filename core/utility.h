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

#include "core/common.h"

#include "core/definition.h"
#include "core/exception.h"
#include "core/object.h"

namespace urge {

class Rect : public Object {
 public:
  /*-export.begin-*/
  Rect(int32_t xv, int32_t yv, int32_t wv, int32_t hv) : data(xv, yv, wv, hv) {}
  Rect(RefPtr<Rect> o) : data(o->data) {}
  Rect() : data() {}

  MARSHAL_DUMP(Rect);
  MARSHAL_LOAD(Rect);

  void Set(int32_t xv, int32_t yv, int32_t wv, int32_t hv) {
    data = RectI(xv, yv, wv, hv);
    if (on_change)
      on_change();
  }

  void Set(RefPtr<Rect> rect) {
    if (rect) {
      data = rect->data;
      if (on_change)
        on_change();
    } else {
      throw Exception(Exception::kRGSSError, "cannot set null to rect.");
    }
  }

  void Empty() {
    data = RectI();
    if (on_change)
      on_change();
  }

  ATTR(int32_t, X) {
    if (value) {
      data.x = *value;
      if (on_change)
        on_change();
      return std::nullopt;
    }
    return data.x;
  }

  ATTR(int32_t, Y) {
    if (value) {
      data.y = *value;
      if (on_change)
        on_change();
      return std::nullopt;
    }
    return data.y;
  }

  ATTR(int32_t, Width) {
    if (value) {
      data.width = *value;
      if (on_change)
        on_change();
      return std::nullopt;
    }
    return data.width;
  }

  ATTR(int32_t, Height) {
    if (value) {
      data.height = *value;
      if (on_change)
        on_change();
      return std::nullopt;
    }
    return data.height;
  }
  /*-export.end-*/

  RectI data;
  std::function<void()> on_change;
};

class Color : public Object {
 public:
  static inline RefPtr<Color> White() {
    return MakeRefCounted<Color>(255.0f, 255.0f, 255.0f, 255.0f);
  }

  static inline RefPtr<Color> Black() {
    return MakeRefCounted<Color>(0.0f, 0.0f, 0.0f, 255.0f);
  }

  /*-export.begin-*/
  Color(float r, float g, float b, float a = 255.f) : data(r, g, b, a) {}
  Color(RefPtr<Color> o) : data(o->data) {}
  Color() : data(0.0f) {}

  MARSHAL_DUMP(Color);
  MARSHAL_LOAD(Color);

  void Set(float r, float g, float b, float a = 255.f) {
    data = glm::vec4(r, g, b, a);
  }

  void Set(RefPtr<Color> color) {
    if (color) {
      data = color->data;
    } else {
      throw Exception(Exception::kRGSSError, "cannot set null to color.");
    }
  }

  ATTR(float, Red) {
    if (value)
      data.r = *value;
    return data.r;
  }

  ATTR(float, Green) {
    if (value)
      data.g = *value;
    return data.g;
  }

  ATTR(float, Blue) {
    if (value)
      data.b = *value;
    return data.b;
  }

  ATTR(float, Alpha) {
    if (value)
      data.a = *value;
    return data.a;
  }
  /*-export.end-*/

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

class Tone : public Object {
 public:
  /*-export.begin-*/
  Tone(float r, float g, float b, float a = 0.f) : data(r, g, b, a) {}
  Tone(RefPtr<Tone> o) : data(o->data) {}
  Tone() : data(0.0f) {}

  MARSHAL_DUMP(Tone);
  MARSHAL_LOAD(Tone);

  void Set(float r, float g, float b, float a = 0.f) {
    data = glm::vec4(r, g, b, a);
  }

  void Set(RefPtr<Tone> tone) {
    if (tone) {
      data = tone->data;
    } else {
      throw Exception(Exception::kRGSSError, "cannot set null to tone.");
    }
  }

  ATTR(float, Red) {
    if (value)
      data.r = *value;
    return data.r;
  }

  ATTR(float, Green) {
    if (value)
      data.g = *value;
    return data.g;
  }

  ATTR(float, Blue) {
    if (value)
      data.b = *value;
    return data.b;
  }

  ATTR(float, Gray) {
    if (value)
      data.a = *value;
    return data.a;
  }
  /*-export.end-*/

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

class Vector2 : public Object {
 public:
  Vector2(glm::vec2 d) : data(d) {}

  /*-export.begin-*/
  Vector2(float xv, float yv) : data(xv, yv) {}
  Vector2(RefPtr<Vector2> o) : data(o->data) {}
  Vector2(float v) : data(v) {}
  Vector2() : data(0.0f) {}

  void Set(float xv, float yv) { data = glm::vec2(xv, yv); }

  void Set(RefPtr<Vector2> v) {
    if (v) {
      data = v->data;
    } else {
      throw Exception(Exception::kRGSSError, "cannot set null to vector.");
    }
  }
  /*-export.end-*/

  glm::vec2 data;
};

class Vector3 : public Object {
 public:
  Vector3(glm::vec3 d) : data(d) {}

  /*-export.begin-*/
  Vector3(float xv, float yv, float zv) : data(xv, yv, zv) {}
  Vector3(RefPtr<Vector3> o) : data(o->data) {}
  Vector3(float v) : data(v) {}
  Vector3() : data(0.0f) {}

  void Set(float xv, float yv, float zv) { data = glm::vec3(xv, yv, zv); }

  void Set(RefPtr<Vector3> v) {
    if (v) {
      data = v->data;
    } else {
      throw Exception(Exception::kRGSSError, "cannot set null to vector.");
    }
  }
  /*-export.end-*/

  glm::vec3 data;
};

class Vector4 : public Object {
 public:
  Vector4(glm::vec4 d) : data(d) {}

  /*-export.begin-*/
  Vector4(float xv, float yv, float zv, float wv) : data(xv, yv, zv, wv) {}
  Vector4(RefPtr<Vector4> o) : data(o->data) {}
  Vector4(float v) : data(v) {}
  Vector4() : data(0.0f) {}

  void Set(float xv, float yv, float zv, float wv) {
    data = glm::vec4(xv, yv, zv, wv);
  }

  void Set(RefPtr<Vector4> v) {
    if (v) {
      data = v->data;
    } else {
      throw Exception(Exception::kRGSSError, "cannot set null to vector.");
    }
  }
  /*-export.end-*/

  glm::vec4 data;
};

}  // namespace urge
