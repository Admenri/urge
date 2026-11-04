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

#include "bind/win32api-shim/win32api_memory.h"
#include <algorithm>
#include <cstring>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <new>

namespace binding::win32api::memory {
namespace {
constexpr size_t kMaxAllocation = 32 * 1024 * 1024;
constexpr size_t kMaxTotal = 128 * 1024 * 1024;
constexpr size_t kMaxBlocks = 65536;
constexpr uint64_t kAddressLimit = 0x70000000;
struct Block {
  std::unique_ptr<unsigned char[]> data;
  size_t size;
  size_t capacity;
  bool global = false;
};
struct GlobalBlock {
  uint32_t address;
  size_t size;
  bool movable;
  uint8_t locks = 0;
};
struct Heap {
  std::mutex mutex;
  std::map<uint32_t, Block> blocks;
  std::map<uint32_t, GlobalBlock> globals;
  uint64_t next = 0x10000000;
  size_t total = 0;
  uint32_t Reserve(size_t size) {
    if (size + 32 > kAddressLimit - next) {
      SetLastError(8);
      return 0;
    }
    const uint32_t address = static_cast<uint32_t>(next);
    // Never reuse tokens, including movable handles and discarded objects.
    next += ((size + 15) & ~uint64_t{15}) + 16;
    return address;
  }
  uint32_t Allocate(size_t size, bool global = false) {
    size = std::max(size, size_t{1});
    if (blocks.size() + globals.size() >= kMaxBlocks || size > kMaxAllocation ||
        size > kMaxTotal - total || size + 32 > kAddressLimit - next) {
      SetLastError(8);
      return 0;
    }
    std::unique_ptr<unsigned char[]> bytes(
        new (std::nothrow) unsigned char[size]());
    if (!bytes) {
      SetLastError(8);
      return 0;
    }
    const uint32_t address = Reserve(size);
    if (!address)
      return 0;
    total += size;
    blocks.emplace(address, Block{std::move(bytes), size, size, global});
    return address;
  }
  unsigned char* View(const Argument& value,
                      size_t count,
                      size_t* available = nullptr) {
    if (value.buffer) {
      if (count > value.size)
        return nullptr;
      if (available)
        *available = value.size;
      return reinterpret_cast<unsigned char*>(value.data);
    }
    if (value.number <= 0 ||
        static_cast<uint64_t>(value.number) >= kAddressLimit)
      return nullptr;
    const auto address = static_cast<uint32_t>(value.number);
    auto found = blocks.upper_bound(address);
    if (found == blocks.begin())
      return nullptr;
    --found;
    const size_t offset = address - found->first;
    if (offset >= found->second.size || count > found->second.size - offset)
      return nullptr;
    if (available)
      *available = found->second.size - offset;
    return found->second.data.get() + offset;
  }
  bool Free(int64_t address) {
    if (!address)
      return true;
    if (address < 0 || static_cast<uint64_t>(address) >= kAddressLimit)
      return false;
    const auto found = blocks.find(static_cast<uint32_t>(address));
    if (found == blocks.end())
      return false;
    // A shrink retains storage; charge its entire capacity until release.
    total -= found->second.capacity;
    blocks.erase(found);
    return true;
  }
};
Heap& GetHeap() {
  static Heap heap;
  return heap;
}
Result Invalid() {
  SetLastError(487);
  return Result::Failure(
      "memory access rejected: unmanaged, freed, or out-of-bounds "
      "address/buffer (original RGSS object layouts are unsupported)");
}
bool Length(const Argument& argument, size_t& count) {
  if (argument.buffer || argument.number < 0 ||
      static_cast<uint64_t>(argument.number) > kMaxAllocation)
    return false;
  count = static_cast<size_t>(argument.number);
  return true;
}
Result Destination(const Argument& argument) {
  Result result{argument.number};
  result.destination = 0;
  return result;
}
Result Allocate(std::span<const Argument> args) {
  size_t size;
  if (!Length(args[0], size)) {
    SetLastError(8);
    return {};
  }
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  return Result{heap.Allocate(size)};
}
Result Callocate(std::span<const Argument> args) {
  size_t count, size;
  if (!Length(args[0], count) || !Length(args[1], size) ||
      (count && size > kMaxAllocation / count)) {
    SetLastError(8);
    return {};
  }
  const Argument total{static_cast<int64_t>(count * size)};
  return Allocate(std::span<const Argument>(&total, 1));
}
Result Free(std::span<const Argument> args) {
  if (args[0].buffer)
    return Invalid();
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  const auto found = heap.blocks.find(static_cast<uint32_t>(args[0].number));
  if (args[0].number != 0 &&
      (args[0].number < 0 ||
       static_cast<uint64_t>(args[0].number) >= kAddressLimit ||
       found == heap.blocks.end() || found->second.global))
    return Invalid();
  return heap.Free(args[0].number) ? Result{} : Invalid();
}
Result Reallocate(std::span<const Argument> args) {
  if (args[0].null())
    return Allocate(args.subspan(1));
  if (args[0].buffer)
    return Invalid();
  size_t size;
  if (!Length(args[1], size)) {
    SetLastError(8);
    return {};
  }
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  if (args[0].number <= 0 ||
      static_cast<uint64_t>(args[0].number) >= kAddressLimit)
    return Invalid();
  const auto found = heap.blocks.find(static_cast<uint32_t>(args[0].number));
  if (found == heap.blocks.end() || found->second.global)
    return Invalid();
  if (!size) {
    heap.Free(args[0].number);
    return {};
  }
  const uint32_t address = heap.Allocate(size);
  if (!address)
    return {};  // The original allocation remains valid on failure.
  std::memcpy(heap.blocks.at(address).data.get(), found->second.data.get(),
              std::min(size, found->second.size));
  heap.Free(args[0].number);
  return Result{address};
}
Result Copy(std::span<const Argument> args, bool overlap) {
  size_t size;
  if (!Length(args[2], size))
    return Invalid();
  if (!size)
    return Destination(args[0]);
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  auto* destination = heap.View(args[0], size);
  auto* source = heap.View(args[1], size);
  if (!destination || !source)
    return Invalid();
  if (!overlap) {
    const auto first = reinterpret_cast<uintptr_t>(destination),
               second = reinterpret_cast<uintptr_t>(source);
    if (first < second ? second - first < size : first - second < size)
      return Result::Failure("memcpy ranges overlap; use memmove");
  }
  std::memmove(destination, source, size);
  return Destination(args[0]);
}
Result Set(std::span<const Argument> args) {
  size_t size;
  if (!Length(args[2], size))
    return Invalid();
  if (!size)
    return Destination(args[0]);
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  auto* destination = heap.View(args[0], size);
  if (!destination)
    return Invalid();
  std::memset(destination, static_cast<unsigned char>(args[1].number), size);
  return Destination(args[0]);
}
Result Compare(std::span<const Argument> args) {
  size_t size;
  if (!Length(args[2], size))
    return Invalid();
  if (!size)
    return {};
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  auto* first = heap.View(args[0], size);
  auto* second = heap.View(args[1], size);
  if (!first || !second)
    return Invalid();
  return Result{std::memcmp(first, second, size)};
}
bool CString(Heap& heap,
             const Argument& argument,
             const unsigned char*& data,
             size_t& length) {
  size_t available = 0;
  data = heap.View(argument, 0, &available);
  if (!data)
    return false;
  const auto* end =
      static_cast<const unsigned char*>(std::memchr(data, 0, available));
  if (!end && !argument.buffer)
    return false;
  length = end ? static_cast<size_t>(end - data) : available;
  return true;
}
Result StringLength(std::span<const Argument> args) {
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  const unsigned char* data;
  size_t size;
  return CString(heap, args[0], data, size) ? Result{static_cast<int64_t>(size)}
                                            : Invalid();
}
Result StringCopy(std::span<const Argument> args) {
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  const unsigned char* source;
  size_t length;
  if (!CString(heap, args[1], source, length) || length >= kMaxAllocation)
    return Invalid();
  auto* destination = heap.View(args[0], length + 1);
  if (!destination)
    return Invalid();
  std::memmove(destination, source, length);
  destination[length] = 0;
  return Destination(args[0]);
}
// A movable HGLOBAL is a separate opaque token, never a readable address.
// All bookkeeping and copies share the CRT heap lock and bounds checks.
Result GlobalFailure(uint32_t code, int64_t value = 0) {
  SetLastError(code);
  return Result{value};
}
auto FindGlobal(Heap& heap, const Argument& argument) {
  if (argument.buffer || argument.number <= 0 ||
      static_cast<uint64_t>(argument.number) >= kAddressLimit)
    return heap.globals.end();
  return heap.globals.find(static_cast<uint32_t>(argument.number));
}
bool GlobalOptions(const Argument& argument, uint32_t& flags) {
  if (argument.buffer || argument.number < 0 ||
      static_cast<uint64_t>(argument.number) > UINT32_MAX)
    return false;
  flags = static_cast<uint32_t>(argument.number);
  // Accept documented obsolete allocation bits as no-ops. Reject GMEM_MODIFY
  // and unknown flags rather than pretending to implement attribute changes.
  constexpr uint32_t allowed =
      0x0002 | 0x0010 | 0x0020 | 0x0040 | 0x0100 | 0x1000 | 0x2000 | 0x4000;
  return !(flags & ~allowed);
}
Result GlobalAllocate(std::span<const Argument> args) {
  uint32_t flags;
  size_t size;
  if (!GlobalOptions(args[0], flags))
    return GlobalFailure(87);
  if (!Length(args[1], size))
    return GlobalFailure(8);
  const bool movable = (flags & 2) != 0;
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  if (heap.blocks.size() + heap.globals.size() >= kMaxBlocks)
    return GlobalFailure(8);
  const uint32_t handle = movable ? heap.Reserve(1) : 0;
  if (movable && !handle)
    return {};
  const uint32_t address = size || !movable ? heap.Allocate(size, true) : 0;
  if ((size || !movable) && !address)
    return {};
  const uint32_t result = movable ? handle : address;
  heap.globals.emplace(result, GlobalBlock{address, size, movable});
  return Result{result};
}
Result GlobalFree(std::span<const Argument> args) {
  if (args[0].null())
    return {};
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  const auto found = FindGlobal(heap, args[0]);
  if (found == heap.globals.end())
    return GlobalFailure(6, args[0].number);
  heap.Free(found->second.address);
  heap.globals.erase(found);
  return {};
}
Result GlobalLock(std::span<const Argument> args) {
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  const auto found = FindGlobal(heap, args[0]);
  if (found == heap.globals.end())
    return GlobalFailure(6);
  auto& object = found->second;
  if (!object.size)
    return GlobalFailure(157);  // ERROR_DISCARDED
  if (object.movable) {
    if (object.locks == UINT8_MAX)
      return GlobalFailure(8);
    ++object.locks;
  }
  return Result{object.address};
}
Result GlobalUnlock(std::span<const Argument> args) {
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  const auto found = FindGlobal(heap, args[0]);
  if (found == heap.globals.end())
    return GlobalFailure(6);
  auto& object = found->second;
  if (!object.movable)
    return Result{1};
  if (!object.locks)
    return GlobalFailure(158);  // ERROR_NOT_LOCKED
  --object.locks;
  if (object.locks)
    return Result{1};
  // Zero also denotes successful final unlock; GetLastError distinguishes it.
  SetLastError(0);
  return {};
}
Result GlobalSize(std::span<const Argument> args) {
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  const auto found = FindGlobal(heap, args[0]);
  if (found == heap.globals.end())
    return GlobalFailure(6);
  return Result{static_cast<int64_t>(found->second.size)};
}
Result GlobalFlags(std::span<const Argument> args) {
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  const auto found = FindGlobal(heap, args[0]);
  if (found == heap.globals.end())
    return GlobalFailure(6, 0x8000);  // GMEM_INVALID_HANDLE
  return Result{found->second.address ? found->second.locks : 0x4000};
}
Result GlobalHandle(std::span<const Argument> args) {
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  if (!args[0].buffer && args[0].number > 0 &&
      static_cast<uint64_t>(args[0].number) < kAddressLimit) {
    for (const auto& [handle, object] : heap.globals)
      if (object.address == static_cast<uint32_t>(args[0].number))
        return Result{handle};
  }
  return GlobalFailure(6);
}
Result GlobalReallocate(std::span<const Argument> args) {
  uint32_t flags;
  size_t size;
  if (!GlobalOptions(args[2], flags))
    return GlobalFailure(87);
  if (!Length(args[1], size))
    return GlobalFailure(8);
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  const auto found = FindGlobal(heap, args[0]);
  if (found == heap.globals.end())
    return GlobalFailure(6);
  auto& object = found->second;
  if (!size && object.movable) {
    if (object.locks)
      return GlobalFailure(212);  // ERROR_LOCKED
    heap.Free(object.address);
    object.address = 0;
    object.size = 0;
    return Result{found->first};
  }
  const size_t allocation = std::max(size, size_t{1});
  // Preserve numeric pointer validity for an in-place shrink/regrowth. The
  // original reserved virtual range must never overlap a subsequent block.
  auto old = heap.blocks.find(object.address);
  if (old != heap.blocks.end() && allocation <= old->second.capacity) {
    auto& block = old->second;
    if (allocation > block.size)
      std::memset(block.data.get() + block.size, 0, allocation - block.size);
    block.size = allocation;
    object.size = size;
    return Result{found->first};
  }
  if (old != heap.blocks.end() && (!object.movable || object.locks) &&
      !(flags & 2))
    return GlobalFailure(8);  // The caller did not permit moving this pointer.
  const uint32_t address = heap.Allocate(allocation, true);
  if (!address)
    return {};  // Original handle, bytes and lock count remain intact.
  if (old != heap.blocks.end())
    std::memcpy(heap.blocks.at(address).data.get(), old->second.data.get(),
                std::min(size, object.size));
  const uint32_t previous = object.address;
  object.address = address;
  object.size = size;
  if (object.movable) {
    heap.Free(previous);
    return Result{found->first};
  }
  // Fixed handles equal the pointer; relocating one invalidates the old token.
  const auto replacement = object;
  heap.globals.emplace(address, replacement);
  heap.globals.erase(found);
  heap.Free(previous);
  return Result{address};
}
}  // namespace

bool Read(const Argument& source,
          void* output,
          size_t size,
          std::string& error) {
  if (!size)
    return true;
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  const auto* data = heap.View(source, size);
  if (!data) {
    error = Invalid().error;
    return false;
  }
  std::memcpy(output, data, size);
  return true;
}
bool Write(const Argument& destination,
           const void* input,
           size_t size,
           std::string& error) {
  if (!size)
    return true;
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  auto* data = heap.View(destination, size);
  if (!data) {
    error = Invalid().error;
    return false;
  }
  std::memcpy(data, input, size);
  return true;
}
bool String(const Argument& source, std::string& output, std::string& error) {
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  const unsigned char* data;
  size_t length;
  if (!CString(heap, source, data, length) || length > kMaxAllocation) {
    error = Invalid().error;
    return false;
  }
  output.assign(reinterpret_cast<const char*>(data), length);
  return true;
}

Result Move(std::span<const Argument> args) {
  return Copy(args, true);
}
void Reset() {
  auto& heap = GetHeap();
  std::lock_guard lock(heap.mutex);
  heap.globals.clear();
  heap.blocks.clear();
  heap.total = 0;
}
void Register(Registry& registry) {
  registry.Add("kernel32", "GlobalAlloc", 2, 0, GlobalAllocate);
  registry.Add("kernel32", "GlobalReAlloc", 3, 0, GlobalReallocate);
  registry.Add("kernel32", "GlobalFree", 1, 0, GlobalFree);
  registry.Add("kernel32", "GlobalLock", 1, 0, GlobalLock);
  registry.Add("kernel32", "GlobalUnlock", 1, 0, GlobalUnlock);
  registry.Add("kernel32", "GlobalSize", 1, 0, GlobalSize);
  registry.Add("kernel32", "GlobalFlags", 1, 0, GlobalFlags);
  registry.Add("kernel32", "GlobalHandle", 1, 0, GlobalHandle);
  const auto move_memory = [](std::span<const Argument> args) {
    auto result = Move(args);
    result.destination = -1;
    result.number = 0;
    return result;
  };
  registry.Add("kernel32", "RtlMoveMemory", 3, 1, move_memory);
  registry.Add("kernel32", "MoveMemory", 3, 1, move_memory);

  for (const char* name : {"msvcrt", "ucrtbase"}) {
    registry.Add(name, "malloc", 1, 0, Allocate);
    registry.Add(name, "calloc", 2, 0, Callocate);
    registry.Add(name, "realloc", 2, 0, Reallocate);
    registry.Add(name, "free", 1, 0, Free);
    registry.Add(name, "memcpy", 3, 1, [](std::span<const Argument> args) {
      return Copy(args, false);
    });
    registry.Add(name, "memmove", 3, 1, Move);
    registry.Add(name, "memset", 3, 1, Set);
    registry.Add(name, "memcmp", 3, 0, Compare);
    registry.Add(name, "strlen", 1, 0, StringLength);
    registry.Add(name, "strcpy", 2, 1, StringCopy);
  }
}
}  // namespace binding::win32api::memory
