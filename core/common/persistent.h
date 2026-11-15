// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.

#pragma once

#include <type_traits>
#include <utility>

#include "core/common/garbage_collector.h"

namespace urge {

// An explicitly registered external/local GC root. This has no reference
// counter or ownership deleter: copying registers another address, moving
// transfers registration, and destroying it never destroys the target.
// Managed object fields must use Member instead.
template <typename T>
class Persistent {
 public:
  using element_type = T;

  Persistent() = default;
  Persistent(std::nullptr_t) {}
  Persistent(T* object) : root_(object) {
    static_assert(std::is_base_of_v<GarbageCollected, T>);
  }
  Persistent(const Persistent&) = default;
  Persistent(Persistent&&) noexcept = default;

  template <typename U>
    requires std::is_convertible_v<U*, T*>
  Persistent(const Persistent<U>& other) : root_(other.root_) {}

  template <typename U>
    requires std::is_convertible_v<U*, T*>
  Persistent(Persistent<U>&& other) noexcept : root_(std::move(other.root_)) {}

  template <typename U>
    requires std::is_convertible_v<U*, T*>
  Persistent(const Member<U>& other) : root_(other.get()) {}

  template <typename U>
    requires std::is_convertible_v<U*, T*>
  Persistent(const WeakMember<U>& other) : root_(other.get()) {}

  Persistent& operator=(const Persistent&) = default;
  Persistent& operator=(Persistent&&) noexcept = default;
  Persistent& operator=(T* object) { reset(object); return *this; }
  Persistent& operator=(std::nullptr_t) { reset(); return *this; }

  T* get() const { return static_cast<T*>(root_.get()); }
  T* operator->() const { return get(); }
  T& operator*() const { return *get(); }
  explicit operator bool() const { return static_cast<bool>(root_); }
  void reset(T* object = nullptr) { root_.reset(object); }
  void swap(Persistent& other) noexcept { std::swap(root_, other.root_); }

  template <typename U>
  bool operator==(const Persistent<U>& other) const { return get() == other.get(); }
  template <typename U>
  bool operator!=(const Persistent<U>& other) const { return !(*this == other); }
  template <typename U>
  bool operator<(const Persistent<U>& other) const { return get() < other.get(); }

 private:
  template <typename U> friend class Persistent;
  GarbageCollector::Root root_;
};

template <typename T, typename U>
bool operator==(const Persistent<T>& root, const U* object) {
  return root.get() == object;
}
template <typename T, typename U>
bool operator==(const T* object, const Persistent<U>& root) {
  return object == root.get();
}
template <typename T>
bool operator==(const Persistent<T>& root, std::nullptr_t) { return !root; }
template <typename T>
bool operator==(std::nullptr_t, const Persistent<T>& root) { return !root; }
template <typename T, typename U>
bool operator!=(const Persistent<T>& root, const U* object) {
  return !(root == object);
}
template <typename T, typename U>
bool operator!=(const T* object, const Persistent<U>& root) {
  return !(object == root);
}
template <typename T>
bool operator!=(const Persistent<T>& root, std::nullptr_t) { return !!root; }
template <typename T>
bool operator!=(std::nullptr_t, const Persistent<T>& root) { return !!root; }

}
