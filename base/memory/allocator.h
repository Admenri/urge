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

template <typename Ty>
using STLAllocator = mi_stl_allocator<Ty>;

template <typename Ty>
using Vector = std::vector<Ty, STLAllocator<Ty>>;

template <typename Ty>
using List = std::list<Ty, STLAllocator<Ty>>;

template <typename Ty>
using Stack = std::stack<Ty, std::deque<Ty, STLAllocator<Ty>>>;

using String =
    std::basic_string<char, std::char_traits<char>, STLAllocator<char>>;

}  // namespace base

#endif  //! BASE_MEMORY_ALLOCATOR_H_
