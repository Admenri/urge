// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace urge {

template <typename T> class Persistent;
namespace detail { struct RootNode; }
template <typename T> class Member;
template <typename T> class WeakMember;
class GCVisitor;

// Non-moving traced storage. Member fields describe the graph; explicit GC
// roots protect native locals and external owners. Roots register addresses,
// not reference counts. Only the collector deletes adopted objects.
class GarbageCollected {
 public:
  virtual ~GarbageCollected();
  GarbageCollected(const GarbageCollected&) = delete;
  GarbageCollected& operator=(const GarbageCollected&) = delete;

  virtual void Trace(GCVisitor& visitor) const = 0;
  // Unlink registrations while the entire garbage batch is still allocated.
  // Destructors must not follow managed pointers into other garbage objects.
  // This hook must not throw or resurrect any object in the garbage batch.
  virtual void PrepareForCollection() {}

 protected:
  GarbageCollected() = default;

 private:
  friend class GarbageCollector;
  detail::RootNode* root_head_ = nullptr;
  uint64_t marked_epoch_ = 0;
  bool reclaiming_ = false;
};

namespace detail {
// Intrusive registration lives in the root object, never in a control block.
struct RootNode {
  GarbageCollected* object = nullptr;
  RootNode* previous = nullptr;
  RootNode* next = nullptr;
};
}

class GCVisitor {
 public:
  virtual ~GCVisitor() = default;
  virtual void Visit(GarbageCollected* object) = 0;

  template <typename T>
  void Trace(const Member<T>& value) {
    static_assert(std::is_base_of_v<GarbageCollected, T>);
    Visit(value.get());
  }

  // Weak edges are registered independently, never part of the strong graph.
  template <typename T>
  void Trace(const WeakMember<T>&) {}

  // A rooted handle inside a managed object would turn cycles into roots.
  template <typename T>
  void Trace(const Persistent<T>&) = delete;

  template <typename T, typename Allocator>
  void Trace(const std::vector<T, Allocator>& values) {
    for (const auto& value : values)
      Trace(value);
  }

  template <typename Key, typename Value, typename Compare, typename Allocator>
  void Trace(const std::map<Key, Value, Compare, Allocator>& values) {
    for (const auto& [key, value] : values) {
      Trace(key);
      Trace(value);
    }
  }

  template <typename First, typename Second>
  void Trace(const std::pair<First, Second>& value) {
    Trace(value.first);
    Trace(value.second);
  }

  template <typename T>
  void Trace(const std::optional<T>& value) {
    if (value)
      Trace(*value);
  }

  template <typename T>
    requires requires(const T& value, GCVisitor& visitor) { value.Trace(visitor); }
  void Trace(const T& value) {
    value.Trace(*this);
  }

  template <typename Char, typename Traits, typename Allocator>
  void Trace(const std::basic_string<Char, Traits, Allocator>&) {}

  template <typename Char, typename Traits>
  void Trace(const std::basic_string_view<Char, Traits>&) {}

  template <typename T>
    requires(std::is_arithmetic_v<T> || std::is_enum_v<T>)
  void Trace(const T&) {}
};

// No runtime-specific handles are stored here. A host contributes roots during
// marking and receives visits to reachable native objects. This lets a bridge
// retain an existing external wrapper without manufacturing wrappers for every
// native allocation. All mutators (including root/weak handle operations) must
// be stopped during marking and sweeping: the registry mutex is not a write
// barrier. Trace must enumerate edges without mutating existing graphs, and may
// run again during Sweep without notifying the host. Observer/driver/filter
// callbacks may allocate fully constructed objects but must not delete objects
// or mutate existing graphs during traversal.
//
// BeginMark/EndMark describe uninterrupted marking, NOT an incremental host
// cycle with mutator gaps. Adopt during marking can immediately call observer;
// revisiting an already black object with Mark does not observe or Trace again.
// There is no separate mark-cycle/mark-pass state or incremental write barrier.
// Member writes never invoke observer. A host bridge must obey these boundaries.
//
// Between EndMark and a delayed Sweep, mutators may change roots/Member edges
// and allocate. Sweep retraces the native closure of marked objects, new objects,
// current roots and host vetoes. It does NOT reopen host marking: a bridge must
// protect any late-added host references itself. Stale/consumed epochs are inert.
class GarbageCollector {
 public:
  using MarkObserver = void (*)(GarbageCollected*, void*);
  using CollectDriver = void (*)(void*);
  using ReclaimFilter = bool (*)(GarbageCollected*, void*);
  using WeakClear = void (*)(void*);

  // A root owns only its registration, never the managed object. Heap-allocated
  // Root instances can cross a host conversion boundary and be deleted by that
  // host's guaranteed cleanup without coupling GC to its runtime.
  class Root {
   public:
    explicit Root(GarbageCollected* object = nullptr);
    Root(const Root& other);
    Root(Root&& other) noexcept;
    ~Root();
    Root& operator=(const Root& other);
    Root& operator=(Root&& other) noexcept;
    GarbageCollected* get() const { return node_.object; }
    explicit operator bool() const { return get() != nullptr; }
    void reset(GarbageCollected* object = nullptr);

   private:
    friend class GarbageCollector;
    detail::RootNode node_;
  };

  // Used by MakeGarbageCollected to keep ctor-owned children alive before the
  // outer object can safely be adopted/traced. Collection requests in a
  // construction scope are skipped, not automatically run from its destructor.
  class ConstructionScope {
   public:
    ConstructionScope();
    ~ConstructionScope();
    ConstructionScope(const ConstructionScope&) = delete;
    ConstructionScope& operator=(const ConstructionScope&) = delete;
  };

  static GarbageCollector& Get();
  void Adopt(GarbageCollected* object);
  void Forget(GarbageCollected* object);
  bool Contains(const GarbageCollected* object) const;
  size_t ObjectCount() const;

  void SetHost(MarkObserver observer, CollectDriver driver,
               ReclaimFilter reclaim_filter, void* data);
  void ClearHost();

  // Clear must only null its slot, must not throw, and must not mutate other
  // registrations. A slot targeting sealed garbage is cleared immediately.
  void RegisterWeak(void* slot, GarbageCollected* object, WeakClear clear);
  void UnregisterWeak(void* slot);

  // Snapshot adopted external GC roots for an independent caller visitor.
  // Works outside native marking; does not change epochs, mark objects, invoke
  // observer, or Trace their edges. While any construction scope is active, all
  // fully adopted, non-reclaiming objects are conservative roots; unfinished,
  // unadopted objects are never exposed. Visitor calls run after releasing the
  // snapshot lock. The caller must stop graph/root mutators and reclamation for
  // the entire scan; the visitor owns graph traversal and host marking policy.
  void TraceRoots(GCVisitor& visitor) const;

  void BeginMark();
  void Mark(GarbageCollected* object);
  void MarkRoots();
  // Always closes the pass. An active constructor or drain makes the pass
  // incomplete: cancel it rather than allowing marking across a mutator gap.
  bool EndMark();
  // Pure native cleanup, also invalidates an unconsumed completed pass. Hosts
  // call this at mark boundaries, not from inside Trace/observer callbacks.
  void CancelMark();
  uint64_t Epoch() const;
  uint64_t CompletedEpoch() const;
  // True only when this completed pass was consumed. A construction barrier
  // can defer sweeping without consuming it; stale/cancelled epochs are inert.
  bool Sweep(uint64_t completed_epoch);
  void Collect();

 private:
  GarbageCollector() = default;
  struct State;
  static State& state();
  static void ResetRoot(detail::RootNode& root, GarbageCollected* object);
  static void MoveRoot(detail::RootNode& destination, detail::RootNode& source);
};

}
