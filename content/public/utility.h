// Copyright 2024 Admenri.
// Use of this source code is governed by a BSD - style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_UTILITY_H_
#define CONTENT_PUBLIC_UTILITY_H_

#include <algorithm>

#include "base/bind/callback_list.h"
#include "base/math/rectangle.h"
#include "base/memory/ref_counted.h"
#include "content/public/serializable.h"

#include "SDL3/SDL_pixels.h"

namespace content {

class ValueNotification {
 public:
  virtual ~ValueNotification() = default;

  base::CallbackListSubscription AddChangedObserver(
      base::RepeatingClosure observer) {
    return observers_.Add(std::move(observer));
  }

 protected:
  virtual void UpdateData() { observers_.Notify(); }

 private:
  base::RepeatingClosureList observers_;
};

class Rect : public base::RefCounted<Rect>,
             public Serializable,
             public ValueNotification {
 public:
  Rect() = default;
  Rect(const base::Rect& rect) : data_(rect) {}
  Rect(const Rect& other) { data_ = other.data_; }

  Rect& operator=(const Rect& other) {
    Set(other.data_);
    return *this;
  }

  bool IsSame(const Rect& other) { return data_ == other.data_; }
  void Set(const base::Rect& rect) {
    base::Rect tmp_rect(data_);
    data_ = rect;
    if (!(tmp_rect == data_))
      UpdateData();
  }

  void Set(scoped_refptr<Rect> rect) {
    base::Rect tmp_rect(data_);
    data_ = rect->data_;
    if (!(tmp_rect == data_))
      UpdateData();
  }

  void Empty() { Set(base::Rect()); }

  int GetX() const { return data_.x; }
  void SetX(int x) {
    if (data_.x == x)
      return;
    data_.x = x;
    UpdateData();
  }

  int GetY() const { return data_.y; }
  void SetY(int y) {
    if (data_.y == y)
      return;
    data_.y = y;
    UpdateData();
  }

  int GetWidth() const { return data_.width; }
  void SetWidth(int width) {
    if (data_.width == width)
      return;
    data_.width = width;
    UpdateData();
  }

  int GetHeight() const { return data_.height; }
  void SetHeight(int height) {
    if (data_.height == height)
      return;
    data_.height = height;
    UpdateData();
  }

  inline base::Rect AsBase() const { return data_; }

  std::string Serialize() override;
  static scoped_refptr<Rect> Deserialize(const std::string& data);

 private:
  base::Rect data_;
};

class Tone : public base::RefCounted<Tone>,
             public Serializable,
             public ValueNotification {
 public:
  Tone() : red_(0.0), green_(0.0), blue_(0.0), gray_(0.0), valid_(false) {}
  Tone(double red, double green, double blue, double gray = 255.0)
      : red_(std::clamp(red, -255.0, 255.0)),
        green_(std::clamp(green, -255.0, 255.0)),
        blue_(std::clamp(blue, -255.0, 255.0)),
        gray_(std::clamp(gray, 0.0, 255.0)),
        valid_(!!red_ || !!green_ || !!blue_ || !!gray_) {}

  Tone(const Tone& other) {
    red_ = other.red_;
    green_ = other.green_;
    blue_ = other.blue_;
    gray_ = other.gray_;
  }

  Tone& operator=(const Tone& other) {
    Set(other.red_, other.green_, other.blue_, other.gray_);
    return *this;
  }

  bool IsSame(const Tone& other) {
    return other.red_ == red_ && other.green_ == green_ &&
           other.blue_ == blue_ && other.gray_ == gray_;
  }

  void Set(double red, double green, double blue, double gray = 255.0f) {
    bool changed = false;
    double tmp;

    tmp = std::clamp(red, -255.0, 255.0);
    if (red_ != tmp)
      changed = true;
    red_ = tmp;
    tmp = std::clamp(green, -255.0, 255.0);
    if (green_ != tmp)
      changed = true;
    green_ = tmp;
    tmp = std::clamp(blue, -255.0, 255.0);
    if (blue_ != tmp)
      changed = true;
    blue_ = tmp;
    tmp = std::clamp(gray, -255.0, 255.0);
    if (gray_ != tmp)
      changed = true;
    gray_ = tmp;

    if (changed)
      UpdateData();
  }

  void Set(scoped_refptr<Tone> tone) {
    Set(tone->red_, tone->green_, tone->blue_, tone->gray_);
  }

  double GetRed() const { return red_; }
  void SetRed(double red) {
    double tmp = std::clamp(red, -255.0, 255.0);
    if (tmp == red_)
      return;
    red_ = tmp;
    UpdateData();
  }

  double GetGreen() const { return green_; }
  void SetGreen(double green) {
    double tmp = std::clamp(green, -255.0, 255.0);
    if (tmp == green_)
      return;
    green_ = tmp;
    UpdateData();
  }

  double GetBlue() const { return blue_; }
  void SetBlue(double blue) {
    double tmp = std::clamp(blue, -255.0, 255.0);
    if (tmp == blue_)
      return;
    blue_ = tmp;
    UpdateData();
  }

  double GetGray() const { return gray_; }
  void SetGray(double gray) {
    double tmp = std::clamp(gray, 0.0, 255.0);
    if (tmp == gray_)
      return;
    gray_ = tmp;
    UpdateData();
  }

  base::Vec4 AsBase() {
    return base::Vec4((float)red_ / 255.0f, (float)green_ / 255.0f,
                      (float)blue_ / 255.0f, (float)gray_ / 255.0f);
  }

  inline bool IsValid() const { return valid_; }

  std::string Serialize() override;
  static scoped_refptr<Tone> Deserialize(const std::string& data);

 private:
  void UpdateData() override {
    ValueNotification::UpdateData();
    valid_ = !!red_ || !!green_ || !!blue_ || !!gray_;
  }

  double red_;
  double green_;
  double blue_;
  double gray_;
  bool valid_;
};

class Color : public base::RefCounted<Color>,
              public Serializable,
              public ValueNotification {
 public:
  Color() : red_(0.0), green_(0.0), blue_(0.0), alpha_(0.0) {}
  Color(double red, double green, double blue, double alpha = 255.0)
      : red_(std::clamp(red, 0.0, 255.0)),
        green_(std::clamp(green, 0.0, 255.0)),
        blue_(std::clamp(blue, 0.0, 255.0)),
        alpha_(std::clamp(alpha, 0.0, 255.0)) {}

  Color(const Color& other) {
    red_ = other.red_;
    green_ = other.green_;
    blue_ = other.blue_;
    alpha_ = other.alpha_;
  }

  Color& operator=(const Color& other) {
    Set(other.red_, other.green_, other.blue_, other.alpha_);
    return *this;
  }

  bool IsSame(const Color& other) {
    return other.red_ == red_ && other.green_ == green_ &&
           other.blue_ == blue_ && other.alpha_ == alpha_;
  }

  void Set(double red, double green, double blue, double alpha) {
    bool changed = false;
    double tmp;

    tmp = std::clamp(red, 0.0, 255.0);
    if (tmp != red_)
      changed = true;
    red_ = tmp;

    tmp = std::clamp(green, 0.0, 255.0);
    if (tmp != green_)
      changed = true;
    green_ = tmp;

    tmp = std::clamp(blue, 0.0, 255.0);
    if (tmp != blue_)
      changed = true;
    blue_ = tmp;

    tmp = std::clamp(alpha, 0.0, 255.0);
    if (tmp != alpha_)
      changed = true;
    alpha_ = tmp;

    if (changed)
      UpdateData();
  }

  void Set(scoped_refptr<Color> color) {
    Set(color->red_, color->green_, color->blue_, color->alpha_);
  }

  double GetRed() const { return red_; }
  void SetRed(double red) {
    double tmp = std::clamp(red, 0.0, 255.0);
    if (tmp == red_)
      return;
    red_ = tmp;
    UpdateData();
  }

  double GetGreen() const { return green_; }
  void SetGreen(double green) {
    double tmp = std::clamp(green, 0.0, 255.0);
    if (tmp == green_)
      return;
    green_ = tmp;
    UpdateData();
  }

  double GetBlue() const { return blue_; }
  void SetBlue(double blue) {
    double tmp = std::clamp(blue, 0.0, 255.0);
    if (tmp == blue_)
      return;
    blue_ = tmp;
    UpdateData();
  }

  double GetAlpha() const { return alpha_; }
  void SetAlpha(double alpha) {
    double tmp = std::clamp(alpha, 0.0, 255.0);
    if (tmp == alpha_)
      return;
    alpha_ = tmp;
    UpdateData();
  }

  base::Vec4 AsBase() {
    return base::Vec4((float)red_ / 255.0f, (float)green_ / 255.0f,
                      (float)blue_ / 255.0f, (float)alpha_ / 255.0f);
  }

  base::Vec4 AsNormal() {
    return base::Vec4(static_cast<float>(red_), static_cast<float>(green_),
                      static_cast<float>(blue_), static_cast<float>(alpha_));
  }

  SDL_Color AsSDLColor() {
    return SDL_Color{static_cast<uint8_t>(red_), static_cast<uint8_t>(green_),
                     static_cast<uint8_t>(blue_), static_cast<uint8_t>(alpha_)};
  }

  inline bool IsValid() const { return alpha_ != 0; }
  std::string Serialize() override;
  static scoped_refptr<Color> Deserialize(const std::string& data);

 private:
  double red_;
  double green_;
  double blue_;
  double alpha_;
};

}  // namespace content

#endif  // !CONTENT_PUBLIC_UTILITY_H_
