// Copyright 2018-2026 Admenri.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#pragma once

#include <memory>
#include <typeindex>

#include "base/bind/callback.h"
#include "base/memory/ref_counted.h"

namespace content {

class Object : public base::RefCounted<Object> {
 public:
  Object() : top_level_type_(typeid(Object)) {}
  virtual ~Object() {}

  Object(const Object&) = delete;
  Object& operator=(const Object&) = delete;

  // Engine top-level type metadata
  std::type_index& top_level_type() { return top_level_type_; }

  template <typename T, typename... Args>
  static scoped_refptr<T> Create(Args&&... args) {
    auto instance = base::MakeRefCounted<T>(std::forward<Args>(args)...);
    instance->top_level_type() = std::type_index(typeid(T));
    return instance;
  }

 private:
  std::type_index top_level_type_;
};

class Constant : public Object {
 public:
  Constant() : on_change_() {}

  Constant(const Constant&) = delete;
  Constant& operator=(const Constant&) = delete;

 public:
  void on_change() {
    if (on_change_)
      on_change_.Run();
  }

  void set_change_handler(const base::RepeatingClosure& closure) {
    on_change_ = closure;
  }

 private:
  base::RepeatingClosure on_change_;
};

template <typename Ty>
class Singleton {
 public:
  // Reset singleton instance
  static void ResetInstance(Ty* instance) { instance_.reset(instance); }

  // Static global instance
  static Ty* GetInstance() { return instance_.get(); }

 private:
  inline static std::unique_ptr<Ty> instance_;
};

}  // namespace content
