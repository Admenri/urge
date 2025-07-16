// Copyright (c) 2025 Admenri Adev.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the name Chromium Embedded
// Framework nor the names of its contributors may be used to endorse
// or promote products derived from this software without specific prior
// written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef BASE_MEMORY_ALLOCATOR_H_
#define BASE_MEMORY_ALLOCATOR_H_

#include <list>
#include <memory>
#include <queue>
#include <stack>
#include <string>
#include <type_traits>
#include <vector>

#include "mimalloc.h"

namespace base {

struct Allocator {
  template <typename Ty, typename... Args>
  static inline Ty* New(Args&&... args) {
    void* mem = mi_malloc(sizeof(Ty));
    if (!mem)
      return nullptr;
    return ::new (mem) Ty(std::forward<Args>(args)...);
  }

  template <typename Ty>
  static inline void Delete(Ty* ptr) {
    if (ptr) {
      ptr->~Ty();
      mi_free(ptr);
    }
  }

  template <typename Ty>
  static inline void Delete(const Ty* ptr) {
    if (ptr) {
      ptr->~Ty();
      mi_free(const_cast<Ty*>(ptr));
    }
  }
};

template <typename _Ty>
struct AllocatorDelete {
  constexpr AllocatorDelete() noexcept = default;

  template <typename _Ty2>
  inline AllocatorDelete(const AllocatorDelete<_Ty2>&) noexcept {}

  inline void operator()(_Ty* _Ptr) const noexcept { Allocator::Delete(_Ptr); }
};

template <typename Ty>
using OwnedPtr = std::unique_ptr<Ty, AllocatorDelete<Ty>>;

template <typename T, typename... Args>
OwnedPtr<T> MakeOwnedPtr(Args&&... args) {
  T* obj = Allocator::New<T>(std::forward<Args>(args)...);
  return OwnedPtr<T>(obj);
}

template <typename T>
class STLAllocator {
 public:
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using size_type = std::size_t;
  using difference_type = std::ptrdiff_t;

  using propagate_on_container_move_assignment = std::true_type;
  using is_always_equal = std::true_type;

  constexpr STLAllocator() noexcept = default;
  constexpr STLAllocator(const STLAllocator&) noexcept = default;

  template <typename U>
  constexpr STLAllocator(const STLAllocator<U>&) noexcept {}

  ~STLAllocator() noexcept = default;

  STLAllocator& operator=(const STLAllocator&) noexcept = default;

  [[nodiscard]] T* allocate(size_type n) {
    return static_cast<T*>(mi_malloc(n * sizeof(T)));
  }

  void deallocate(T* p, size_type) noexcept { mi_free(p); }

  constexpr size_type max_size() const noexcept {
    return (size_type(-1) / sizeof(value_type));
  }

  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
  }

  template <typename U>
  void destroy(U* p) noexcept {
    p->~U();
  }
};

template <typename T, typename U>
constexpr bool operator==(const STLAllocator<T>&,
                          const STLAllocator<U>&) noexcept {
  return true;
}

template <typename T, typename U>
constexpr bool operator!=(const STLAllocator<T>&,
                          const STLAllocator<U>&) noexcept {
  return false;
}

using String =
    std::basic_string<char, std::char_traits<char>, STLAllocator<char>>;

template <typename Ty>
using Vector = std::vector<Ty, STLAllocator<Ty>>;

template <typename Ty>
using List = std::list<Ty, STLAllocator<Ty>>;

template <typename Ty>
using Deque = std::deque<Ty, STLAllocator<Ty>>;

template <typename Ty>
using Stack = std::stack<Ty, Deque<Ty>>;

template <typename Ty>
using Queue = std::queue<Ty, Deque<Ty>>;

}  // namespace base

#endif  //! BASE_MEMORY_ALLOCATOR_H_
