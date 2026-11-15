// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.

#pragma once

#include <memory>
#include <type_traits>
#include <utility>

#include "core/common/garbage_collector.h"
#include "core/common/persistent.h"

namespace urge {

// A strong graph edge, NOT a root. Copying/clearing it does not retain, release,
// or delete its target. The owning object's Trace must enumerate this field.
template <typename T>
class Member {
 public:
  constexpr Member() = default;
  constexpr Member(std::nullptr_t) {}
  constexpr Member(T* value) : value_(value) {}

  template <typename U>
    requires std::is_convertible_v<U*, T*>
  Member(const Persistent<U>& value) : value_(value.get()) {}

  template <typename U>
    requires std::is_convertible_v<U*, T*>
  constexpr Member(const Member<U>& value) : value_(value.get()) {}

  T* get() const { return value_; }
  T* operator->() const { return value_; }
  T& operator*() const { return *value_; }
  explicit operator bool() const { return value_ != nullptr; }
  void reset() { value_ = nullptr; }
  Member& operator=(T* value) { value_ = value; return *this; }
  Member& operator=(std::nullptr_t) { reset(); return *this; }

  template <typename U>
    requires std::is_convertible_v<U*, T*>
  Member& operator=(const Persistent<U>& value) {
    value_ = value.get();
    return *this;
  }

 private:
  T* value_ = nullptr;
};

// A non-owning edge. The collector clears it BEFORE preparing/deleting its
// target. Slot registration follows copies/moves, including container moves.
template <typename T>
class WeakMember {
 public:
  WeakMember() = default;
  WeakMember(std::nullptr_t) {}
  WeakMember(T* value) { reset(value); }
  WeakMember(const Persistent<T>& value) { reset(value.get()); }
  WeakMember(const Member<T>& value) { reset(value.get()); }
  WeakMember(const WeakMember& value) { reset(value.get()); }
  WeakMember(WeakMember&& value) { reset(value.get()); value.reset(); }
  ~WeakMember() { GarbageCollector::Get().UnregisterWeak(this); }

  WeakMember& operator=(const WeakMember& value) {
    reset(value.get());
    return *this;
  }
  WeakMember& operator=(WeakMember&& value) {
    if (this != &value) {
      reset(value.get());
      value.reset();
    }
    return *this;
  }
  WeakMember& operator=(T* value) { reset(value); return *this; }
  WeakMember& operator=(std::nullptr_t) { reset(); return *this; }

  T* get() const { return value_; }
  T* operator->() const { return value_; }
  T& operator*() const { return *value_; }
  explicit operator bool() const { return value_ != nullptr; }

  void reset(T* value = nullptr) {
    static_assert(std::is_base_of_v<GarbageCollected, T>);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    T* previous = value_;
#endif
    value_ = value;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    try {
#endif
      // RegisterWeak may synchronously clear a target already sealed for death.
      GarbageCollector::Get().RegisterWeak(this, value, Clear);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    } catch (...) {
      value_ = previous;
      throw;
    }
#endif
  }

 private:
  static void Clear(void* slot) noexcept {
    static_cast<WeakMember*>(slot)->value_ = nullptr;
  }
  T* value_ = nullptr;
};

template <typename T, typename... Args>
[[nodiscard]] T* MakeGarbageCollected(Args&&... args) {
  static_assert(std::is_base_of_v<GarbageCollected, T>);
  GarbageCollector::ConstructionScope construction;
  std::unique_ptr<T> object(new T(std::forward<Args>(args)...));
  GarbageCollector::Root root(object.get());
  // Do not publish partially constructed objects or an unrooted allocation.
  // After registry insertion, callbacks can publish this fully constructed object;
  // from that point collection, not this factory, must determine its lifetime.
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  try {
#endif
    GarbageCollector::Get().Adopt(object.get());
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  } catch (...) {
    if (GarbageCollector::Get().Contains(object.get())) {
      // A failed observer/Trace may already have published strong references.
      // Keep the adopted object registered; unwinding releases only our root.
      object.release();
    } else {
      // Registry insertion failed before callbacks could publish the object.
      GarbageCollector::Get().Forget(object.get());
      object->PrepareForCollection();
    }
    throw;
  }
#endif
  return object.release();
}

}
