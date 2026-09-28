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

#include <stdint.h>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <utility>

namespace urge {

template <typename T>
struct DefaultRefCountedTraits {
  static void Destruct(const T* x) { delete x; }
};

template <class T,
          typename Traits = DefaultRefCountedTraits<T>,
          typename CountTy = size_t>
class RefCounted {
 public:
  RefCounted() : ref_count_(0) {}

  RefCounted(const RefCounted&) = delete;
  RefCounted& operator=(const RefCounted&) = delete;

  void AddRef() const { ref_count_.fetch_add(1, std::memory_order_relaxed); }

  bool Release() const {
    if (ref_count_.fetch_sub(1, std::memory_order_release) == 1) {
      // Acquire fence ensures that the destructor sees all changes
      // made by other threads before they released.
      std::atomic_thread_fence(std::memory_order_acquire);
      // Delete resource
      Traits::Destruct(static_cast<const T*>(this));
      // Released
      return true;
    }
    return false;
  }

  // Returns the current reference count (mostly for debugging).
  CountTy RefCount() const {
    return ref_count_.load(std::memory_order_relaxed);
  }

 protected:
  ~RefCounted() = default;

 private:
  mutable std::atomic<CountTy> ref_count_;
};

template <class T>
class RefPtr {
 public:
  typedef T element_type;

  constexpr RefPtr() = default;
  constexpr RefPtr(std::nullptr_t) {}

  RefPtr(T* p) : ptr_(p) {
    if (ptr_)
      AddRef(ptr_);
  }

  ~RefPtr() {
    if (ptr_)
      Release(ptr_);
  }

  // Copy constructor. This is required in addition to the copy conversion
  // constructor below.
  RefPtr(const RefPtr& r) : RefPtr(r.ptr_) {}

  // Copy conversion constructor.
  template <typename U,
            typename = typename std::enable_if<
                std::is_convertible<U*, T*>::value>::type>
  RefPtr(const RefPtr<U>& r) : RefPtr(r.ptr_) {}

  // Move constructor. This is required in addition to the move conversion
  // constructor below.
  RefPtr(RefPtr&& r) noexcept : ptr_(r.ptr_) { r.ptr_ = nullptr; }

  // Move conversion constructor.
  template <typename U,
            typename = typename std::enable_if<
                std::is_convertible<U*, T*>::value>::type>
  RefPtr(RefPtr<U>&& r) noexcept : ptr_(r.ptr_) {
    r.ptr_ = nullptr;
  }

  T* get() const { return ptr_; }
  T& operator*() const { return *ptr_; }
  T* operator->() const { return ptr_; }

  RefPtr& operator=(T* p) { return *this = RefPtr(p); }
  RefPtr& operator=(std::nullptr_t) {
    reset();
    return *this;
  }

  // Unified assignment operator.
  RefPtr& operator=(RefPtr r) noexcept {
    swap(r);
    return *this;
  }

  // Sets managed object to null and releases reference to the previous managed
  // object, if it existed.
  void reset() { RefPtr().swap(*this); }

  // Returns the owned pointer (if any), releasing ownership to the caller. The
  // caller is responsible for managing the lifetime of the reference.
  [[nodiscard]] T* release();

  void swap(RefPtr& r) noexcept { std::swap(ptr_, r.ptr_); }

  explicit operator bool() const { return ptr_ != nullptr; }

  template <typename U>
  bool operator==(const RefPtr<U>& rhs) const {
    return ptr_ == rhs.get();
  }

  template <typename U>
  bool operator!=(const RefPtr<U>& rhs) const {
    return !operator==(rhs);
  }

  template <typename U>
  bool operator<(const RefPtr<U>& rhs) const {
    return ptr_ < rhs.get();
  }

 protected:
  T* ptr_ = nullptr;

 private:
  // Friend required for move constructors that set r.ptr_ to null.
  template <typename U>
  friend class RefPtr;

  static void AddRef(T* ptr);
  static void Release(T* ptr);
};

template <typename T>
T* RefPtr<T>::release() {
  T* ptr = ptr_;
  ptr_ = nullptr;
  return ptr;
}

// static
template <typename T>
void RefPtr<T>::AddRef(T* ptr) {
  ptr->AddRef();
}

// static
template <typename T>
void RefPtr<T>::Release(T* ptr) {
  ptr->Release();
}

template <typename T, typename U>
bool operator==(const RefPtr<T>& lhs, const U* rhs) {
  return lhs.get() == rhs;
}

template <typename T, typename U>
bool operator==(const T* lhs, const RefPtr<U>& rhs) {
  return lhs == rhs.get();
}

template <typename T>
bool operator==(const RefPtr<T>& lhs, std::nullptr_t null) {
  return !static_cast<bool>(lhs);
}

template <typename T>
bool operator==(std::nullptr_t null, const RefPtr<T>& rhs) {
  return !static_cast<bool>(rhs);
}

template <typename T, typename U>
bool operator!=(const RefPtr<T>& lhs, const U* rhs) {
  return !operator==(lhs, rhs);
}

template <typename T, typename U>
bool operator!=(const T* lhs, const RefPtr<U>& rhs) {
  return !operator==(lhs, rhs);
}

template <typename T>
bool operator!=(const RefPtr<T>& lhs, std::nullptr_t null) {
  return !operator==(lhs, null);
}

template <typename T>
bool operator!=(std::nullptr_t null, const RefPtr<T>& rhs) {
  return !operator==(null, rhs);
}

template <typename T>
std::ostream& operator<<(std::ostream& out, const RefPtr<T>& p) {
  return out << p.get();
}

template <typename T>
void swap(RefPtr<T>& lhs, RefPtr<T>& rhs) noexcept {
  lhs.swap(rhs);
}

// Constructs an instance of T, which is a ref counted type, and wraps the
// object into a scoped_refptr<T>.
template <typename T, typename... Args>
RefPtr<T> MakeRefCounted(Args&&... args) {
  T* obj = new T(std::forward<Args>(args)...);
  return RefPtr<T>(obj);
}

}  // namespace urge
