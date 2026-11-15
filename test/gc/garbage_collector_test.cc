// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <latch>
#include <memory>
#include <new>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "core/common/member.h"
#include "core/common/persistent.h"

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
namespace allocation_failure {
thread_local int remaining = -1;
thread_local size_t failed_size = 0;
struct Scope {
  explicit Scope(int successful_allocations) {
    assert(remaining == -1);
    remaining = successful_allocations;
    failed_size = 0;
  }
  ~Scope() { remaining = -1; }
  Scope(const Scope&) = delete;
  Scope& operator=(const Scope&) = delete;
};
}

// One-shot allocation faults are test-only and disabled during error cleanup.
void* operator new(size_t size) {
  if (allocation_failure::remaining >= 0 &&
      allocation_failure::remaining-- == 0) {
    allocation_failure::remaining = -1;
    allocation_failure::failed_size = size;
    throw std::bad_alloc();
  }
  if (void* result = std::malloc(size ? size : 1))
    return result;
  throw std::bad_alloc();
}
void* operator new[](size_t size) { return ::operator new(size); }
void operator delete(void* object) noexcept { std::free(object); }
void operator delete[](void* object) noexcept { std::free(object); }
void operator delete(void* object, size_t) noexcept { std::free(object); }
void operator delete[](void* object, size_t) noexcept { std::free(object); }
#endif

namespace {
using urge::GarbageCollected;
using urge::GarbageCollector;
using urge::GCVisitor;
using urge::MakeGarbageCollected;
using urge::Member;
using urge::Persistent;
using urge::WeakMember;

struct Node final : GarbageCollected {
  explicit Node(Node* value = nullptr) : next(value) {}
  Member<Node> next;
  std::vector<Member<Node>> children;
  bool prepared = false;
  inline static int destroyed = 0;

  void Trace(GCVisitor& visitor) const override {
    visitor.Trace(next);
    visitor.Trace(children);
  }
  void PrepareForCollection() override { prepared = true; }
  ~Node() override { assert(prepared); ++destroyed; }
};

void TestFactoryReturnsUnrootedPointer() {
  static_assert(std::is_same_v<decltype(MakeGarbageCollected<Node>()), Node*>);
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  const auto destroyed = Node::destroyed;
  auto* object = MakeGarbageCollected<Node>();
  WeakMember<Node> weak(object);
  Member<Node> local_edge(object);  // Neither raw pointers nor local edges root.
  assert(gc.Contains(object) && local_edge.get() == object);
  gc.Collect();
  assert(!weak && Node::destroyed == destroyed + 1);
  assert(gc.ObjectCount() == before);
  local_edge.reset();  // Never dereference the now-unrooted raw address.
}

void TestRootCopyMoveAndReset() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> first = MakeGarbageCollected<Node>();
  Persistent<Node> second = MakeGarbageCollected<Node>();
  WeakMember<Node> weak_first(first), weak_second(second);
  GarbageCollector::Root source(first.get());
  GarbageCollector::Root copy(source);
  GarbageCollector::Root assigned;
  assigned = copy;
  GarbageCollector::Root moved(std::move(copy));
  assert(!copy && source.get() == moved.get());
  source.reset();
  assigned.reset();
  GarbageCollector::Root destination(second.get());
  first.reset();
  second.reset();
  destination = std::move(moved);  // Unlink its old target, relink the new address.
  assert(!moved && destination.get() == weak_first.get());
  destination = destination;
  destination = std::move(destination);
  destination.reset(destination.get());
  gc.Collect();
  assert(weak_first && !weak_second && gc.ObjectCount() == before + 1);
  assigned = destination;
  destination.reset();
  gc.Collect();
  assert(weak_first && assigned.get() == weak_first.get());
  assigned.reset();
  assert(weak_first && gc.ObjectCount() == before + 1);
  gc.Collect();
  assert(!weak_first && gc.ObjectCount() == before);
}

void TestPersistentCopyMoveAndConversions() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> first = MakeGarbageCollected<Node>();
  Persistent<Node> second = MakeGarbageCollected<Node>();
  WeakMember<Node> weak_first(first), weak_second(second);
  Persistent<GarbageCollected> copied(first);
  Persistent<GarbageCollected> moved(std::move(first));
  assert(!first && copied.get() == moved.get());
  Persistent<GarbageCollected> destination(std::move(second));
  assert(!second);
  destination = std::move(moved);
  assert(!moved && destination.get() == weak_first.get());
  destination = destination;
  destination = std::move(destination);
  copied.reset();
  gc.Collect();
  assert(weak_first && !weak_second && gc.ObjectCount() == before + 1);
  Persistent<GarbageCollected> assigned;
  assigned = destination;
  destination.swap(assigned);
  destination.reset();
  gc.Collect();
  assert(weak_first && assigned.get() == weak_first.get());
  assigned.reset();
  assert(weak_first && gc.ObjectCount() == before + 1);
  gc.Collect();
  assert(!weak_first && gc.ObjectCount() == before);
}

template <typename Handle>
void TestRootContainerAddressRelocation() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> first = MakeGarbageCollected<Node>();
  Persistent<Node> second = MakeGarbageCollected<Node>();
  WeakMember<Node> weak_first(first), weak_second(second);
  std::vector<Handle> roots;
  roots.reserve(1);
  for (int i = 0; i != 64; ++i)
    roots.emplace_back(first.get());
  roots.emplace_back(second.get());
  const auto* old_storage = roots.data();
  roots.reserve(roots.capacity() + 1);  // Guaranteed relocation, not just a hint.
  assert(roots.data() != old_storage);
  first.reset();
  second.reset();
  gc.Collect();
  assert(weak_first && weak_second && gc.ObjectCount() == before + 2);
  roots.erase(roots.begin());  // Move assignment and tail destruction update links.
  auto moved = std::move(roots);
  assert(moved.size() == 64 && moved.back().get() == weak_second.get());
  moved.front() = std::move(moved.front());
  std::swap(moved.front(), moved.back());
  std::swap(moved.front(), moved.back());
  moved.resize(1);
  moved.shrink_to_fit();
  gc.Collect();
  assert(weak_first && !weak_second && gc.ObjectCount() == before + 1);
  auto copied = moved;
  moved.clear();
  gc.Collect();
  assert(weak_first && copied.front().get() == weak_first.get());
  copied.clear();
  assert(weak_first && gc.ObjectCount() == before + 1);
  gc.Collect();
  assert(!weak_first && gc.ObjectCount() == before);
}

template <typename Handle>
void TestWorkerRootTransferAtSafePoint() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> original = MakeGarbageCollected<Node>();
  WeakMember<Node> weak(original);
  std::latch ready{1}, finish{1};
  std::thread worker([owned = Handle(original.get()), &ready, &finish]() mutable {
    Handle copied(owned);
    Handle moved(std::move(copied));
    assert(!copied && moved.get() == owned.get());
    std::vector<Handle> roots;
    roots.reserve(1);
    for (int i = 0; i != 64; ++i)
      roots.emplace_back(moved.get());
    owned.reset();
    moved.reset();
    ready.count_down();
    finish.wait();  // Stop all handle mutators for the entire main-thread GC.
    roots.erase(roots.begin());
    roots.clear();
  });
  ready.wait();
  original.reset();
  gc.Collect();
  assert(weak && gc.ObjectCount() == before + 1);
  finish.count_down();
  worker.join();
  assert(weak && gc.ObjectCount() == before + 1);
  gc.Collect();
  assert(!weak && gc.ObjectCount() == before);
}

void TestWorkerRawFactoryRootHandoff(bool completed_mark) {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  if (completed_mark) {
    gc.BeginMark();
    assert(gc.EndMark());
  }
  const auto epoch = gc.Epoch();
  const auto completed = gc.CompletedEpoch();
  Persistent<Node> received;
  WeakMember<Node> weak;
  std::latch raw_ready{1}, register_root{1};
  std::thread worker([&] {
    received = [&]() -> Persistent<Node> {
      // Match the curl event handoff: the caller, not the raw factory, covers
      // the interval between factory teardown and registration of the new root.
      GarbageCollector::ConstructionScope construction;
      auto* raw = MakeGarbageCollected<Node>();
      weak.reset(raw);
      raw_ready.count_down();
      register_root.wait();
      Persistent<Node> event(raw);
      return event;  // NRVO or move preserves the address registration.
    }();
  });
  raw_ready.wait();  // The factory's own Root and ConstructionScope are gone.
  assert(weak && gc.ObjectCount() == before + 1);
  gc.Collect();
  assert(gc.Epoch() == epoch && gc.CompletedEpoch() == completed);
  gc.BeginMark();  // A new epoch must not start in the unrooted handoff window.
  assert(gc.Epoch() == epoch && !gc.EndMark());
  if (completed_mark)
    assert(!gc.Sweep(completed) && gc.CompletedEpoch() == completed);
  assert(weak && gc.ObjectCount() == before + 1);
  register_root.count_down();
  worker.join();
  assert(received.get() == weak.get());
  gc.Collect();  // The returned root protects the object after the guard exits.
  assert(weak && gc.ObjectCount() == before + 1);
  received.reset();
  assert(weak && gc.ObjectCount() == before + 1);
  gc.Collect();
  assert(!weak && gc.ObjectCount() == before);
}

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
void TestRootRegistrationNeedsNoAllocation() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> owner = MakeGarbageCollected<Node>();
  WeakMember<Node> weak(owner);
  GarbageCollector::Root survivor;
  {
    allocation_failure::Scope fail_next_allocation(0);
    GarbageCollector::Root first(owner.get());
    GarbageCollector::Root copied(first);
    GarbageCollector::Root moved(std::move(copied));
    survivor = std::move(moved);
    Persistent<Node> typed(owner);
    Persistent<GarbageCollected> base(std::move(typed));
    base.reset();
    first.reset();
  }
  owner.reset();
  gc.Collect();
  assert(weak && gc.ObjectCount() == before + 1);
  survivor.reset();
  gc.Collect();
  assert(!weak && gc.ObjectCount() == before);
}
#endif

void TestNativeRootsAndMembers() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> parent = MakeGarbageCollected<Node>();
  Persistent<Node> child = MakeGarbageCollected<Node>();
  parent->children.push_back(child);
  WeakMember<Node> weak(child);
  child.reset();
  gc.Collect();
  assert(gc.ObjectCount() == before + 2);
  assert(weak);

  parent->children.clear();
  gc.Collect();
  assert(!weak);
  assert(gc.ObjectCount() == before + 1);
  parent.reset();
  gc.Collect();
  assert(gc.ObjectCount() == before);
}

void TestUnrootedCycleAndWeakMoves() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> first = MakeGarbageCollected<Node>();
  Persistent<Node> second = MakeGarbageCollected<Node>();
  first->next = second;
  second->next = first;
  std::vector<WeakMember<Node>> weak;
  weak.emplace_back(first);
  weak.emplace_back(second);
  auto copy = weak.front();
  auto moved = std::move(copy);
  assert(!copy);
  first.reset();
  second.reset();
  gc.Collect();
  assert(!weak[0] && !weak[1] && !moved);
  assert(gc.ObjectCount() == before);
}

void TestAllocationAfterMarkAndStaleSweep() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> old = MakeGarbageCollected<Node>();
  old.reset();
  gc.BeginMark();
  gc.MarkRoots();
  gc.EndMark();
  const auto epoch = gc.Epoch();

  Persistent<Node> recent = MakeGarbageCollected<Node>();
  WeakMember<Node> weak(recent);
  recent.reset();
  gc.Sweep(epoch);
  assert(weak);
  assert(gc.ObjectCount() == before + 1);

  gc.BeginMark();
  gc.MarkRoots();
  gc.EndMark();
  gc.Sweep(epoch);  // Cannot use an older host marking result.
  assert(weak);
  gc.Sweep(gc.Epoch());
  assert(!weak);
  assert(gc.ObjectCount() == before);
}

void TestLateRootAndDeferredGraph() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> parent = MakeGarbageCollected<Node>();
  Persistent<Node> child = MakeGarbageCollected<Node>();
  parent->next = child;
  Node* raw = parent.get();
  WeakMember<Node> weak_child(child);
  parent.reset();
  child.reset();

  gc.BeginMark();
  gc.MarkRoots();
  gc.EndMark();
  Persistent<Node> late_root(raw);
  gc.Sweep(gc.Epoch());
  assert(weak_child);
  assert(gc.ObjectCount() == before + 2);
  late_root.reset();

  gc.SetHost(nullptr, nullptr,
      [](GarbageCollected* object, void* deferred) {
        return object != deferred;
      }, raw);
  gc.Collect();
  assert(weak_child);
  assert(gc.ObjectCount() == before + 2);
  gc.ClearHost();
  gc.Collect();
  assert(!weak_child);
  assert(gc.ObjectCount() == before);
}

void TestLateEdgesAndNewGraph() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> parent = MakeGarbageCollected<Node>();
  Persistent<Node> child = MakeGarbageCollected<Node>();
  Persistent<Node> leaf = MakeGarbageCollected<Node>();
  child->next = leaf;
  WeakMember<Node> weak_child(child), weak_leaf(leaf);
  Node* child_raw = child.get();
  child.reset();
  leaf.reset();

  gc.BeginMark();
  gc.MarkRoots();
  gc.EndMark();
  parent->next = child_raw;  // The parent is already black.
  gc.Sweep(gc.Epoch());
  assert(weak_child && weak_leaf);
  parent->next.reset();
  gc.Collect();
  assert(!weak_child && !weak_leaf);

  Persistent<Node> old = MakeGarbageCollected<Node>();
  Persistent<Node> old_leaf = MakeGarbageCollected<Node>();
  old->next = old_leaf;
  Node* old_raw = old.get();
  WeakMember<Node> weak_old(old), weak_old_leaf(old_leaf);
  old.reset();
  old_leaf.reset();
  gc.BeginMark();
  gc.MarkRoots();
  gc.EndMark();
  Persistent<Node> recent = MakeGarbageCollected<Node>(old_raw);
  WeakMember<Node> weak_recent(recent);
  recent.reset();
  gc.Sweep(gc.Epoch());
  assert(weak_recent && weak_old && weak_old_leaf);
  gc.Collect();
  assert(!weak_recent && !weak_old && !weak_old_leaf);
  parent.reset();
  gc.Collect();
  assert(gc.ObjectCount() == before);
}

void TestAdoptionWhileMarking() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> root = MakeGarbageCollected<Node>();
  Persistent<Node> child = MakeGarbageCollected<Node>();
  WeakMember<Node> weak_child(child);
  struct Context {
    Node* child;
    bool allocated = false;
    int visits = 0;
  } context{child.get()};
  child.reset();
  gc.SetHost([](GarbageCollected*, void* data) {
    auto& context = *static_cast<Context*>(data);
    ++context.visits;
    if (!context.allocated) {
      context.allocated = true;
      Persistent<Node> recent = MakeGarbageCollected<Node>(context.child);
      // Rehash the registry during root traversal without publishing objects.
      for (int i = 0; i != 256; ++i)
        (void)MakeGarbageCollected<Node>();
      GarbageCollector::Get().Collect();  // Reentry must not start a cycle.
    }
  }, nullptr, nullptr, &context);
  gc.Collect();
  assert(weak_child);
  assert(context.visits == 259);  // Root, child, new parent, 256 new nodes.
  gc.ClearHost();
  root.reset();
  gc.Collect();
  assert(!weak_child && gc.ObjectCount() == before);
}

struct RootSnapshotVisitor final : GCVisitor {
  std::unordered_set<GarbageCollected*> roots;
  void Visit(GarbageCollected* object) override {
    assert(object && GarbageCollector::Get().Contains(object));
    assert(roots.insert(object).second);
  }
};

struct RootScanningConstructor final : GarbageCollected {
  Member<Node> child;
  int& traces;
  RootScanningConstructor(RootSnapshotVisitor& snapshot, Node* old, int& traces)
      : traces(traces) {
    Persistent<Node> local = MakeGarbageCollected<Node>(old);
    child = local;
    local.reset();  // Only the unadopted parent owns this completed child.
    auto& gc = GarbageCollector::Get();
    assert(!gc.Contains(this));
    gc.TraceRoots(snapshot);
    assert(snapshot.roots.contains(child.get()));
    assert(snapshot.roots.contains(old));
    assert(!snapshot.roots.contains(this) && traces == 0);
  }
  void Trace(GCVisitor& visitor) const override {
    ++traces;
    visitor.Trace(child);
  }
};

void TestTraceRootsDuringConstruction() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  const auto epoch = gc.Epoch();
  int observations = 0, traces = 0;
  gc.SetHost([](GarbageCollected*, void* data) {
    ++*static_cast<int*>(data);
  }, nullptr, nullptr, &observations);
  Persistent<Node> old = MakeGarbageCollected<Node>();
  Node* raw = old.get();
  WeakMember<Node> weak_old(old);
  old.reset();
  RootSnapshotVisitor during;
  Persistent<RootScanningConstructor> parent = MakeGarbageCollected<RootScanningConstructor>(during, raw, traces);
  assert(during.roots.size() == 2 && gc.ObjectCount() == before + 3);
  assert(gc.Epoch() == epoch && observations == 0 && traces == 0);
  RootSnapshotVisitor after;
  gc.TraceRoots(after);
  assert(after.roots.size() == 1 && after.roots.contains(parent.get()));
  assert(gc.Epoch() == epoch && observations == 0 && traces == 0);
  gc.ClearHost();
  gc.Collect();
  assert(weak_old);
  parent.reset();
  gc.Collect();
  assert(!weak_old && gc.ObjectCount() == before);
}

void TestLateRootAddressTransfer() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> parent = MakeGarbageCollected<Node>();
  Persistent<Node> child = MakeGarbageCollected<Node>();
  parent->next = child;
  Node* raw = parent.get();
  WeakMember<Node> weak_parent(parent), weak_child(child);
  parent.reset();
  child.reset();
  gc.BeginMark();
  gc.MarkRoots();
  assert(gc.EndMark());
  const auto epoch = gc.CompletedEpoch();
  GarbageCollector::Root late(raw);
  GarbageCollector::Root copied(late);
  GarbageCollector::Root moved(std::move(copied));
  assert(!copied);
  late.reset();
  RootSnapshotVisitor snapshot;
  gc.TraceRoots(snapshot);
  assert(snapshot.roots.size() == 1 && snapshot.roots.contains(raw));
  assert(gc.Sweep(epoch) && weak_parent && weak_child);
  assert(gc.ObjectCount() == before + 2);
  moved.reset();
  assert(weak_parent && weak_child && gc.ObjectCount() == before + 2);
  gc.Collect();
  assert(!weak_parent && !weak_child && gc.ObjectCount() == before);
}

struct ConstructionBarrier {
  std::latch start{1};
  std::latch constructed{1};
  std::latch finish{1};
  GarbageCollected* unfinished = nullptr;
  Node* child = nullptr;
};

struct WorkerConstructingNode final : GarbageCollected {
  Member<Node> child;
  WorkerConstructingNode(ConstructionBarrier& barrier, Node* old) {
    Persistent<Node> local = MakeGarbageCollected<Node>(old);
    child = local;
    local.reset();
    // The outer factory's scope protects this child while the worker pauses
    // before adoption. No existing graph/root is mutated during the host scan.
    assert(!GarbageCollector::Get().Contains(this));
    barrier.unfinished = this;
    barrier.child = child.get();
    barrier.constructed.count_down();
    barrier.finish.wait();
  }
  void Trace(GCVisitor& visitor) const override { visitor.Trace(child); }
};

void TestWorkerConstructionDuringRootSnapshot() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> root = MakeGarbageCollected<Node>();
  Persistent<Node> old = MakeGarbageCollected<Node>();
  Node* old_raw = old.get();
  WeakMember<Node> weak_old(old);
  old.reset();
  int observations = 0;
  gc.SetHost([](GarbageCollected*, void* data) {
    ++*static_cast<int*>(data);
  }, nullptr, nullptr, &observations);
  gc.BeginMark();
  const auto epoch = gc.Epoch();
  assert(gc.CompletedEpoch() == 0);

  ConstructionBarrier barrier;
  std::thread worker([&] {
    barrier.start.wait();
    (void)MakeGarbageCollected<WorkerConstructingNode>(barrier, old_raw);
  });
  struct Visitor final : GCVisitor {
    ConstructionBarrier& barrier;
    std::unordered_set<GarbageCollected*> roots;
    explicit Visitor(ConstructionBarrier& barrier) : barrier(barrier) {}
    void Visit(GarbageCollected* object) override {
      assert(roots.insert(object).second);
      if (roots.size() == 1) {
        barrier.start.count_down();
        barrier.constructed.wait();  // Worker adoption needs the registry lock.
      }
    }
  } snapshot(barrier);
  gc.TraceRoots(snapshot);
  assert(snapshot.roots.size() == 1 && snapshot.roots.contains(root.get()));
  assert(observations == 2);  // Worker child adoption traces child + old.
  const int visits = observations;
  RootSnapshotVisitor during;
  gc.TraceRoots(during);
  assert(during.roots.size() == 3 && during.roots.contains(root.get()));
  assert(during.roots.contains(old_raw) && during.roots.contains(barrier.child));
  assert(!during.roots.contains(barrier.unfinished) && observations == visits);
  assert(gc.Epoch() == epoch && gc.CompletedEpoch() == 0);

  // Reproduce END_MARK while the worker constructor is still active. It must
  // cancel, not leave a native pass hanging across the host mutator boundary.
  assert(!gc.EndMark() && gc.CompletedEpoch() == 0);
  gc.CancelMark();  // Cancellation is idempotent, even under construction.
  assert(!gc.Sweep(epoch) && weak_old && gc.ObjectCount() == before + 3);
  gc.BeginMark();  // Still blocked by construction, not by orphaned marking.
  assert(gc.Epoch() == epoch);
  Persistent<Node> recent = MakeGarbageCollected<Node>();
  WeakMember<Node> weak_recent(recent);
  recent.reset();
  assert(observations == visits);  // Cancelled passes cannot observe adoption.
  barrier.finish.count_down();
  worker.join();
  assert(observations == visits && gc.ObjectCount() == before + 5);
  gc.ClearHost();
  gc.BeginMark();
  assert(gc.Epoch() != epoch && gc.EndMark());
  assert(gc.CompletedEpoch() == gc.Epoch() && gc.Sweep(gc.Epoch()));
  assert(!weak_old && !weak_recent && gc.ObjectCount() == before + 1);
  root.reset();
  gc.Collect();
  assert(gc.ObjectCount() == before);
}

void TestCancellationAndCompletedEpoch() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> object = MakeGarbageCollected<Node>();
  WeakMember<Node> weak(object);
  Node* raw = object.get();
  object.reset();
  gc.BeginMark();
  const auto cancelled = gc.Epoch();
  gc.Mark(raw);
  gc.CancelMark();
  assert(!gc.EndMark() && gc.CompletedEpoch() == 0 && !gc.Sweep(cancelled));
  assert(weak && gc.ObjectCount() == before + 1);

  gc.BeginMark();
  const auto completed = gc.Epoch();
  assert(completed != cancelled && gc.EndMark());
  assert(gc.CompletedEpoch() == completed && !gc.EndMark());
  gc.CancelMark();  // A completed but unconsumed host claim can also be revoked.
  assert(gc.CompletedEpoch() == 0 && !gc.Sweep(completed) && weak);
  gc.CancelMark();
  gc.BeginMark();
  assert(gc.EndMark() && !gc.Sweep(completed));
  assert(gc.Sweep(gc.CompletedEpoch()) && !weak);
  assert(gc.CompletedEpoch() == 0 && !gc.Sweep(gc.Epoch()));
  assert(gc.ObjectCount() == before);
}

void TestConstructionDefersCompletedSweep() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> old = MakeGarbageCollected<Node>();
  Node* raw = old.get();
  WeakMember<Node> weak_old(old);
  old.reset();
  gc.BeginMark();
  assert(gc.EndMark());
  const auto epoch = gc.CompletedEpoch();
  ConstructionBarrier barrier;
  std::thread worker([&] {
    barrier.start.wait();
    (void)MakeGarbageCollected<WorkerConstructingNode>(barrier, raw);
  });
  barrier.start.count_down();
  barrier.constructed.wait();
  assert(!gc.Sweep(epoch) && gc.CompletedEpoch() == epoch && weak_old);
  barrier.finish.count_down();
  worker.join();
  assert(gc.Sweep(epoch) && gc.CompletedEpoch() == 0 && weak_old);
  gc.Collect();  // Objects born after EndMark get one completed pass of grace.
  assert(!weak_old && gc.ObjectCount() == before);
}

struct ConstructingNode final : GarbageCollected {
  Member<Node> child;
  explicit ConstructingNode(uint64_t completed_epoch) {
    Persistent<Node> local = MakeGarbageCollected<Node>();
    child = local;
    WeakMember<Node> weak(local);
    local.reset();
    auto& gc = GarbageCollector::Get();
    gc.Collect();
    gc.Sweep(completed_epoch);
    assert(weak);  // The outer object has not been adopted and cannot be traced.
  }
  void Trace(GCVisitor& visitor) const override { visitor.Trace(child); }
};

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
struct ThrowingConstructor final : GarbageCollected {
  explicit ThrowingConstructor(WeakMember<GarbageCollected>& self,
                               GarbageCollector::Root* external = nullptr,
                               Persistent<GarbageCollected>* typed = nullptr) {
    self.reset(this);
    if (external)
      external->reset(this);
    if (typed)
      typed->reset(this);
    GarbageCollector::Root temporary(this);
    GarbageCollector::Root moved(std::move(temporary));
    assert(!temporary && moved.get() == this);
    Persistent<Node> child = MakeGarbageCollected<Node>();
    child.reset();
    GarbageCollector::Get().Collect();
    throw std::runtime_error("constructor failure");
  }
  void Trace(GCVisitor&) const override { assert(false); }
};
#endif

void TestConstructionAndUnwinding() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  gc.BeginMark();
  gc.MarkRoots();
  gc.EndMark();
  Persistent<ConstructingNode> parent = MakeGarbageCollected<ConstructingNode>(gc.Epoch());
  gc.Collect();
  assert(gc.ObjectCount() == before + 2);
  parent.reset();
  gc.Collect();
  assert(gc.ObjectCount() == before);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  WeakMember<GarbageCollected> self;
  GarbageCollector::Root external;
  Persistent<GarbageCollected> typed;
  bool caught = false;
  try {
    (void)MakeGarbageCollected<ThrowingConstructor>(self, &external, &typed);
  } catch (const std::runtime_error&) {
    caught = true;
  }
  assert(caught && !self && !external && !typed);
  gc.Collect();
  assert(gc.ObjectCount() == before);
#endif
}

struct BatchState {
  int prepared = 0;
  int destroyed = 0;
  int failed_children = 0;
  WeakMember<GarbageCollected> late_weak;
};

struct BatchNode final : GarbageCollected {
  explicit BatchNode(BatchState& state) : state(state) {}
  BatchState& state;
  Member<BatchNode> next;
  WeakMember<BatchNode> weak;
  void Trace(GCVisitor& visitor) const override {
    visitor.Trace(next);
    visitor.Trace(weak);  // Explicit weak tracing is a no-op.
  }
  void PrepareForCollection() override {
    assert(state.destroyed == 0 && !weak);
    assert(next && GarbageCollector::Get().Contains(next.get()));
    state.late_weak.reset(next.get());  // Must not recreate a dangling weak slot.
    assert(!state.late_weak);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    WeakMember<GarbageCollected> failed_self;
    try {
      (void)MakeGarbageCollected<ThrowingConstructor>(failed_self);
    } catch (const std::runtime_error&) {
      // Not every destructor during Sweep belongs to the precleared batch.
      assert(!failed_self);
      ++state.failed_children;
    }
#endif
    ++state.prepared;
    std::thread worker([] { (void)GarbageCollector::Get().ObjectCount(); });
    worker.join();  // Disposal must not keep the registry mutex locked.
    GarbageCollector::Get().Collect();
    GarbageCollector::Get().Sweep(GarbageCollector::Get().Epoch());
  }
  ~BatchNode() override {
    assert(state.prepared == 2 && !state.late_weak);
    ++state.destroyed;
  }
};

void TestWeakUnlinkAndPreparationPhases() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  BatchState state;
  Persistent<BatchNode> first = MakeGarbageCollected<BatchNode>(state);
  Persistent<BatchNode> second = MakeGarbageCollected<BatchNode>(state);
  first->next = second;
  second->next = first;
  first->weak.reset(second.get());
  second->weak.reset(first.get());
  first.reset();
  second.reset();
  gc.Collect();
  assert(state.prepared == 2 && state.destroyed == 2);
  assert(!state.late_weak && gc.ObjectCount() == before + state.failed_children);
  gc.Collect();  // Children allocated during disposal get one cycle of grace.
  assert(gc.ObjectCount() == before);
}

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
struct ThrowingTrace final : GarbageCollected {
  bool fail = true;
  void Trace(GCVisitor&) const override {
    if (fail)
      throw std::runtime_error("trace failure");
  }
};

void TestCallbackExceptionRecovery() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<ThrowingTrace> root = MakeGarbageCollected<ThrowingTrace>();
  bool caught = false;
  try {
    gc.Collect();
  } catch (const std::runtime_error&) {
    caught = true;
  }
  assert(caught);
  root->fail = false;
  root.reset();
  gc.Collect();
  assert(gc.ObjectCount() == before);

  Persistent<Node> node = MakeGarbageCollected<Node>();
  WeakMember<Node> weak(node);
  node.reset();
  gc.SetHost(nullptr, nullptr, [](GarbageCollected*, void*) -> bool {
    throw std::runtime_error("filter failure");
  }, nullptr);
  caught = false;
  try {
    gc.Collect();
  } catch (const std::runtime_error&) {
    caught = true;
  }
  assert(caught && weak);
  gc.ClearHost();
  gc.Collect();
  assert(!weak && gc.ObjectCount() == before);
}

struct AdoptionFailure final : GarbageCollected {
  WeakMember<GarbageCollected>& self;
  int& prepared;
  int& destroyed;
  AdoptionFailure(WeakMember<GarbageCollected>& self, int& prepared, int& destroyed)
      : self(self), prepared(prepared), destroyed(destroyed) {
    self.reset(this);
  }
  void Trace(GCVisitor&) const override {
    throw std::runtime_error("adoption trace failure");
  }
  void PrepareForCollection() override {
    assert(!self);
    ++prepared;
  }
  ~AdoptionFailure() override {
    assert(prepared == 1);
    ++destroyed;
  }
};

void TestAdoptionFailureCleanup() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  WeakMember<GarbageCollected> self;
  int prepared = 0, destroyed = 0;
  bool caught = false;
  gc.BeginMark();
  try {
    (void)MakeGarbageCollected<AdoptionFailure>(self, prepared, destroyed);
  } catch (const std::runtime_error&) {
    caught = true;
  }
  assert(caught && self && prepared == 0 && destroyed == 0);
  assert(gc.Contains(self.get()) && gc.ObjectCount() == before + 1);
  gc.Sweep(gc.Epoch());  // An aborted mark cannot be consumed.
  assert(self && prepared == 0 && destroyed == 0);
  gc.Collect();  // A failed but adopted allocation now belongs to normal GC.
  assert(!self && prepared == 1 && destroyed == 1);
  assert(gc.ObjectCount() == before);
}
struct PublishedOwner final : GarbageCollected {
  explicit PublishedOwner(GarbageCollected* object) : child(object) {}
  Member<GarbageCollected> child;
  void Trace(GCVisitor& visitor) const override { visitor.Trace(child); }
};

struct PublicationState {
  Persistent<PublishedOwner> owner;
  Persistent<GarbageCollected> external;
  WeakMember<GarbageCollected> weak;
  bool from_trace = false;
  bool fail = true;
  int prepared = 0;
  int destroyed = 0;
};

void PublishThenThrow(PublicationState& state, GarbageCollected* object) {
  state.owner = MakeGarbageCollected<PublishedOwner>(object);
  state.external = Persistent<GarbageCollected>(object);
  throw std::runtime_error("failure after publication");
}

struct PublishedFailure final : GarbageCollected {
  explicit PublishedFailure(PublicationState& state) : state(state) {
    state.weak.reset(this);
  }
  PublicationState& state;
  void Trace(GCVisitor&) const override {
    if (state.from_trace && state.fail)
      PublishThenThrow(state, const_cast<PublishedFailure*>(this));
  }
  void PrepareForCollection() override {
    assert(!state.weak);
    ++state.prepared;
  }
  ~PublishedFailure() override {
    assert(state.prepared == 1);
    ++state.destroyed;
  }
};

void TestPublishedAdoptionFailure() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  for (int mode = 0; mode != 2; ++mode) {
    PublicationState state;
    state.from_trace = mode != 0;
    if (!state.from_trace)
      gc.SetHost([](GarbageCollected* object, void* data) {
        auto& state = *static_cast<PublicationState*>(data);
        if (object == state.weak.get() && state.fail)
          PublishThenThrow(state, object);
      }, nullptr, nullptr, &state);
    bool caught = false;
    gc.BeginMark();
    try {
      (void)MakeGarbageCollected<PublishedFailure>(state);
    } catch (const std::runtime_error&) {
      caught = true;
    }
    gc.ClearHost();
    assert(caught && state.weak && state.owner && state.external);
    assert(state.owner->child.get() == state.weak.get());
    assert(state.external.get() == state.weak.get());
    assert(state.prepared == 0 && state.destroyed == 0);
    assert(gc.ObjectCount() == before + 2 && gc.Contains(state.weak.get()));
    state.fail = false;
    state.external.reset();
    gc.Collect();  // Publication through the new owner's Member alone suffices.
    assert(state.weak && state.prepared == 0 && state.destroyed == 0);
    state.owner.reset();
    gc.Collect();
    assert(!state.weak && state.prepared == 1 && state.destroyed == 1);
    assert(gc.ObjectCount() == before);
  }
}

void TestDirectMarkSnapshotFailure() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> root = MakeGarbageCollected<Node>();
  struct Context { bool valid = true; int calls = 0; } context;
  gc.SetHost([](GarbageCollected*, void* data) {
    auto& context = *static_cast<Context*>(data);
    ++context.calls;
    if (!context.valid)
      throw std::runtime_error("observer used outside aborted mark");
  }, nullptr, nullptr, &context);
  gc.BeginMark();
  const auto epoch = gc.Epoch();
  bool caught = false;
  try {
    allocation_failure::Scope fail_snapshot(0);
    gc.MarkRoots();  // The root vector, not Mark or Collect, must handle failure.
  } catch (const std::bad_alloc&) {
    caught = true;
  }
  assert(caught && context.calls == 0 && gc.Epoch() == epoch);
  context.valid = false;
  Persistent<Node> recent = MakeGarbageCollected<Node>();
  WeakMember<Node> weak(recent);
  recent.reset();  // Adoption must not invoke the now-invalid observer.
  assert(context.calls == 0);
  gc.EndMark();
  gc.Sweep(epoch);
  assert(weak && gc.ObjectCount() == before + 2);
  gc.ClearHost();
  root.reset();
  gc.Collect();
  assert(!weak && gc.ObjectCount() == before);
}

struct FaultPreservedParent final : GarbageCollected {
  Member<Node> child;
  int& prepared;
  int& destroyed;
  FaultPreservedParent(Node* child, FaultPreservedParent*& created,
                       int& prepared, int& destroyed)
      : child(child), prepared(prepared), destroyed(destroyed) {
    created = this;
  }
  void Trace(GCVisitor& visitor) const override { visitor.Trace(child); }
  void PrepareForCollection() override { ++prepared; }
  ~FaultPreservedParent() override {
    assert(prepared == 1);
    ++destroyed;
  }
};

void TestPreserveQueueAllocationFailure() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  bool injected_queue_failure = false;
  bool reached_success = false;
  for (int budget = 0; budget != 16 && !reached_success; ++budget) {
    Persistent<Node> child = MakeGarbageCollected<Node>();
    WeakMember<Node> weak_child(child);
    struct Context {
      Node* child;
      int budget;
      FaultPreservedParent* created = nullptr;
      WeakMember<FaultPreservedParent> weak_parent;
      int prepared = 0;
      int destroyed = 0;
      bool attempted = false;
      bool caught = false;
      bool registered = false;
      size_t failed_size = 0;
    } context{child.get(), budget};
    child.reset();
    gc.SetHost(nullptr, nullptr, [](GarbageCollected*, void* data) {
      auto& context = *static_cast<Context*>(data);
      if (context.attempted)
        return true;
      context.attempted = true;
      try {
        allocation_failure::Scope failure(context.budget);
        (void)MakeGarbageCollected<FaultPreservedParent>(
            context.child, context.created, context.prepared, context.destroyed);
      } catch (const std::bad_alloc&) {
        context.caught = true;
        context.failed_size = allocation_failure::failed_size;
      }
      context.registered = context.created &&
          GarbageCollector::Get().Contains(context.created);
      if (context.registered)
        context.weak_parent.reset(context.created);
      return true;
    }, &context);
    gc.Collect();
    assert(context.attempted);
    if (context.registered) {
      // A caught PreserveVisitor enqueue failure must be retried by Sweep's
      // live-seed pass, even though the adopted parent was already born black.
      assert(weak_child && context.weak_parent);
      assert(context.prepared == 0 && context.destroyed == 0);
      assert(gc.ObjectCount() == before + 2);
      if (context.caught && context.failed_size == sizeof(GarbageCollected*))
        injected_queue_failure = true;  // First pending-vector allocation.
    } else {
      assert(!weak_child && !context.weak_parent && gc.ObjectCount() == before);
      assert(context.created == nullptr ||
             (context.prepared == 1 && context.destroyed == 1));
    }
    reached_success = !context.caught;
    gc.ClearHost();
    gc.Collect();
    assert(!weak_child && !context.weak_parent && gc.ObjectCount() == before);
  }
  assert(reached_success && injected_queue_failure);
}
#endif

void TestAllocationInReclaimFilter() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> child = MakeGarbageCollected<Node>();
  Persistent<Node> leaf = MakeGarbageCollected<Node>();
  child->next = leaf;
  WeakMember<Node> weak_child(child), weak_leaf(leaf);
  struct Context {
    Node* child;
    bool allocated = false;
    WeakMember<Node> recent;
  } context{child.get()};
  child.reset();
  leaf.reset();
  gc.SetHost(nullptr, nullptr, [](GarbageCollected*, void* data) {
    auto& context = *static_cast<Context*>(data);
    if (!context.allocated) {
      context.allocated = true;
      Persistent<Node> recent = MakeGarbageCollected<Node>(context.child);
      context.recent.reset(recent.get());
      for (int i = 0; i != 256; ++i)
        (void)MakeGarbageCollected<Node>();
    }
    return true;
  }, &context);
  gc.Collect();
  assert(context.recent && weak_child && weak_leaf);
  assert(gc.ObjectCount() == before + 259);
  gc.ClearHost();
  gc.Collect();
  assert(!context.recent && !weak_child && !weak_leaf);
  assert(gc.ObjectCount() == before);
}

void TestDriverReentryAndFailure() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> root = MakeGarbageCollected<Node>();
  int calls = 0;
  gc.SetHost(nullptr, [](void* data) {
    ++*static_cast<int*>(data);
    auto& gc = GarbageCollector::Get();
    gc.Collect();  // Must not recursively invoke this driver.
    gc.BeginMark();
    gc.MarkRoots();
    gc.EndMark();
    gc.Sweep(gc.Epoch());
  }, nullptr, &calls);
  gc.Collect();
  assert(calls == 1);
  root.reset();
  gc.Collect();
  assert(calls == 2 && gc.ObjectCount() == before);

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  gc.SetHost(nullptr, [](void*) {
    GarbageCollector::Get().BeginMark();
    throw std::runtime_error("driver failure");
  }, nullptr, nullptr);
  bool caught = false;
  try {
    gc.Collect();
  } catch (const std::runtime_error&) {
    caught = true;
  }
  assert(caught);
  gc.ClearHost();
  Persistent<Node> unrooted = MakeGarbageCollected<Node>();
  WeakMember<Node> weak(unrooted);
  unrooted.reset();
  gc.Collect();
  assert(!weak && gc.ObjectCount() == before);
#endif
  gc.ClearHost();
}

void TestWeakContainerMoves() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<Node> first = MakeGarbageCollected<Node>();
  Persistent<Node> second = MakeGarbageCollected<Node>();
  std::vector<WeakMember<Node>> weak;
  weak.reserve(1);
  weak.emplace_back(first);
  weak.emplace_back(second);  // Relocates live slots.
  weak.emplace_back(first);
  weak.erase(weak.begin());  // Move-assigns live slots, destroys the tail.
  auto moved = std::move(weak);
  WeakMember<Node> other(first);
  std::swap(moved.front(), other);
  assert(moved.front().get() == first.get() && other.get() == second.get());
  moved.back() = std::move(moved.back());
  WeakMember<Node> copy(moved.front());
  // Destroy a registered slot while its target remains alive.
  moved.pop_back();
  gc.Collect();
  assert(moved.front() && other && copy);
  first.reset();
  gc.Collect();
  assert(!moved.front() && !copy && other);
  second.reset();
  gc.Collect();
  assert(!other && gc.ObjectCount() == before);
}

struct ContainerNode final : GarbageCollected {
  struct Compare {
    bool operator()(const Member<Node>& lhs, const Member<Node>& rhs) const {
      return std::less<Node*>{}(lhs.get(), rhs.get());
    }
  };
  std::map<Member<Node>, Member<Node>, Compare> edges;
  std::map<std::string, std::optional<Member<Node>>> named;
  std::vector<WeakMember<Node>> weak;
  void Trace(GCVisitor& visitor) const override {
    visitor.Trace(edges);
    visitor.Trace(named);
    visitor.Trace(weak);
  }
};

void TestContainerKeysAndWeakTrace() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Persistent<ContainerNode> owner = MakeGarbageCollected<ContainerNode>();
  Persistent<Node> key = MakeGarbageCollected<Node>();
  Persistent<Node> value = MakeGarbageCollected<Node>();
  Persistent<Node> weak_only = MakeGarbageCollected<Node>();
  WeakMember<Node> weak_key(key), weak_value(value), external_weak(weak_only);
  owner->edges.emplace(Member<Node>(key), Member<Node>(value));
  owner->named.emplace("child", Member<Node>(value));
  owner->weak.emplace_back(weak_only);
  key.reset();
  value.reset();
  weak_only.reset();
  gc.Collect();
  assert(weak_key && weak_value && !external_weak && !owner->weak.front());
  owner->edges.clear();
  gc.Collect();
  assert(!weak_key && weak_value);
  owner.reset();
  gc.Collect();
  assert(!weak_value && gc.ObjectCount() == before);
}

void TestDeepGraphAndTransferredRoot() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  constexpr size_t count = 20000;
  Persistent<Node> root = MakeGarbageCollected<Node>();
  Persistent<Node> tail = root;
  for (size_t i = 1; i != count; ++i) {
    Persistent<Node> next = MakeGarbageCollected<Node>();
    tail->next = next;
    tail = std::move(next);
  }
  WeakMember<Node> weak(tail);
  Persistent<GarbageCollected> transferred(std::move(root));
  assert(!root && transferred);
  tail.reset();
  gc.Collect();
  assert(weak && gc.ObjectCount() == before + count);
  transferred.reset();  // Removing the registration does not delete the graph.
  assert(weak && gc.ObjectCount() == before + count);
  gc.Collect();
  assert(!weak && gc.ObjectCount() == before);
}

struct CountingNode final : GarbageCollected {
  explicit CountingNode(int& traces) : traces(traces) {}
  int& traces;
  void Trace(GCVisitor&) const override { ++traces; }
};

void TestUninterruptedMarkContract() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  int observations = 0, traces = 0;
  gc.SetHost([](GarbageCollected*, void* data) {
    ++*static_cast<int*>(data);
  }, nullptr, nullptr, &observations);
  gc.BeginMark();
  Persistent<CountingNode> adopted = MakeGarbageCollected<CountingNode>(traces);
  // Adoption is inside the active pass, so it immediately observes and traces.
  assert(observations == 1 && traces == 1);
  const auto epoch = gc.Epoch();
  RootSnapshotVisitor snapshot;
  gc.TraceRoots(snapshot);  // Even inside marking, this is not native MarkRoots.
  assert(snapshot.roots.size() == 1 && snapshot.roots.contains(adopted.get()));
  assert(gc.Epoch() == epoch && observations == 1 && traces == 1);
  gc.Mark(adopted.get());
  gc.MarkRoots();
  assert(observations == 1 && traces == 1);  // Black revisits are skipped.
  gc.EndMark();
  gc.Sweep(gc.Epoch());
  assert(observations == 1 && traces == 2);  // Sweep is native-only retracing.
  gc.ClearHost();
  adopted.reset();
  gc.Collect();
  assert(gc.ObjectCount() == before);
}

// A stand-in for an independently collected host heap. This tests the bridge
// contract without embedding any runtime-specific code in the native heap.
struct Host {
  struct Wrapper {
    Node* native;
    int instance_variable;
    uint64_t marked = 0;
    std::vector<Wrapper*> references;
  };
  std::unordered_map<Node*, std::unique_ptr<Wrapper>> wrappers;
  std::vector<Wrapper*> roots;
  uint64_t epoch = 0;

  Wrapper* Wrap(Node* object, int initial_value = 0) {
    auto it = wrappers.find(object);
    if (it != wrappers.end())
      return it->second.get();
    auto value = std::make_unique<Wrapper>();
    value->native = object;
    value->instance_variable = initial_value;
    auto* result = value.get();
    wrappers.emplace(object, std::move(value));
    return result;
  }

  void Mark(Wrapper* wrapper) {
    if (wrapper->marked == epoch)
      return;
    wrapper->marked = epoch;
    GarbageCollector::Get().Mark(wrapper->native);
    for (auto* reference : wrapper->references)
      Mark(reference);
  }

  static void OnNativeMark(GarbageCollected* object, void* data) {
    auto& host = *static_cast<Host*>(data);
    auto it = host.wrappers.find(static_cast<Node*>(object));
    if (it != host.wrappers.end())
      host.Mark(it->second.get());
  }

  static void Collect(void* data) {
    auto& host = *static_cast<Host*>(data);
    auto& gc = GarbageCollector::Get();
    gc.BeginMark();
    host.epoch = gc.Epoch();
    gc.MarkRoots();
    for (auto* wrapper : host.roots)
      host.Mark(wrapper);
    gc.EndMark();
    for (auto it = host.wrappers.begin(); it != host.wrappers.end();) {
      if (it->second->marked != host.epoch)
        it = host.wrappers.erase(it);
      else
        ++it;
    }
    gc.Sweep(host.epoch);
  }
};

void TestTraceRootsWithoutNativeMarking() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  const auto epoch = gc.Epoch();
  int callbacks = 0;
  gc.SetHost([](GarbageCollected*, void* data) {
    ++*static_cast<int*>(data);
  }, [](void* data) {
    ++*static_cast<int*>(data);
  }, [](GarbageCollected*, void* data) {
    ++*static_cast<int*>(data);
    return true;
  }, &callbacks);
  Persistent<Node> parent = MakeGarbageCollected<Node>();
  auto duplicate_root = parent;
  Persistent<Node> child = MakeGarbageCollected<Node>();
  Persistent<Node> disconnected = MakeGarbageCollected<Node>();
  parent->next = child;
  child->next = parent;  // Caller, not native GC, must deduplicate traversal.
  Node* child_raw = child.get();
  Node* disconnected_raw = disconnected.get();
  WeakMember<Node> weak_child(child), weak_disconnected(disconnected);
  Host host;
  host.epoch = 7;  // Independent of the native mark epoch.
  auto* canonical = host.Wrap(child_raw, 42);
  child.reset();
  disconnected.reset();

  class HostGraphVisitor final : public GCVisitor {
   public:
    explicit HostGraphVisitor(Host& host) : host(host) {}
    Host& host;
    std::unordered_set<GarbageCollected*> visited;
    std::vector<GarbageCollected*> pending;
    void Visit(GarbageCollected* object) override {
      if (!object || !visited.insert(object).second)
        return;
      if (visited.size() == 1) {
        std::thread worker([object] {
          assert(GarbageCollector::Get().Contains(object));
        });
        worker.join();  // TraceRoots must release its registry mutex first.
      }
      pending.push_back(object);
    }
    void Drain() {
      while (!pending.empty()) {
        auto* object = pending.back();
        pending.pop_back();
        // Mark only existing wrappers; never call Wrap or native Mark.
        auto it = host.wrappers.find(static_cast<Node*>(object));
        if (it != host.wrappers.end())
          it->second->marked = host.epoch;
        object->Trace(*this);
      }
    }
  } visitor(host);
  const GarbageCollector& independent_gc = gc;
  independent_gc.TraceRoots(visitor);
  assert(visitor.visited.size() == 1 && visitor.visited.contains(parent.get()));
  assert(!visitor.visited.contains(child_raw));  // No implicit graph Trace.
  assert(canonical->marked == 0 && callbacks == 0 && gc.Epoch() == epoch);
  visitor.Drain();
  assert(visitor.visited.size() == 2 && visitor.visited.contains(child_raw));
  assert(!visitor.visited.contains(disconnected_raw));
  assert(host.wrappers.size() == 1 && host.Wrap(child_raw) == canonical);
  assert(canonical->marked == host.epoch && canonical->instance_variable == 42);
  assert(callbacks == 0 && gc.Epoch() == epoch);
  assert(gc.ObjectCount() == before + 3);
  gc.BeginMark();  // Existing roots are white in this new native epoch.
  const auto active_epoch = gc.Epoch();
  RootSnapshotVisitor during_mark;
  gc.TraceRoots(during_mark);
  assert(during_mark.roots.size() == 1 && callbacks == 0);
  assert(gc.Epoch() == active_epoch);
  gc.ClearHost();
  gc.EndMark();
  gc.Sweep(active_epoch);
  assert(weak_child && !weak_disconnected);
  parent.reset();
  duplicate_root.reset();
  gc.Collect();
  assert(!weak_child && gc.ObjectCount() == before);
}

void TestHostIdentityAndCrossHeapCycle() {
  auto& gc = GarbageCollector::Get();
  const auto before = gc.ObjectCount();
  Host host;
  gc.SetHost(Host::OnNativeMark, Host::Collect, nullptr, &host);

  Persistent<Node> parent = MakeGarbageCollected<Node>();  // No host wrapper is needed.
  Persistent<Node> child = MakeGarbageCollected<Node>();
  Node* child_native = child.get();
  auto* original = host.Wrap(child_native, 42);
  parent->children.push_back(child);
  child.reset();
  gc.Collect();
  assert(host.wrappers.size() == 1);
  assert(host.Wrap(child_native) == original);
  assert(host.Wrap(child_native)->instance_variable == 42);

  Persistent<Node> peer = MakeGarbageCollected<Node>();
  auto* peer_wrapper = host.Wrap(peer.get(), 7);
  original->references.push_back(peer_wrapper);
  peer->next = parent;
  peer.reset();
  gc.Collect();
  assert(host.wrappers.size() == 2);
  assert(gc.ObjectCount() == before + 3);

  // native parent -> native child -> host wrapper -> another host wrapper ->
  // native peer -> native parent. With no roots, both heaps reclaim the cycle.
  parent.reset();
  gc.Collect();
  assert(host.wrappers.empty());
  assert(gc.ObjectCount() == before);

  // A host root alone must keep the corresponding native object alive.
  Persistent<Node> object = MakeGarbageCollected<Node>();
  host.roots.push_back(host.Wrap(object.get(), 99));
  object.reset();
  gc.Collect();
  assert(gc.ObjectCount() == before + 1);
  host.roots.clear();
  gc.Collect();
  assert(gc.ObjectCount() == before);
  gc.ClearHost();
}

}

int main() {
#ifdef _MSC_VER
  // An assertion in a command-line regression must not open a modal dialog.
  _set_error_mode(_OUT_TO_STDERR);
  _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
  TestFactoryReturnsUnrootedPointer();
  TestRootCopyMoveAndReset();
  TestPersistentCopyMoveAndConversions();
  TestRootContainerAddressRelocation<GarbageCollector::Root>();
  TestRootContainerAddressRelocation<Persistent<Node>>();
  TestWorkerRootTransferAtSafePoint<GarbageCollector::Root>();
  TestWorkerRootTransferAtSafePoint<Persistent<Node>>();
  TestWorkerRawFactoryRootHandoff(false);
  TestWorkerRawFactoryRootHandoff(true);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  TestRootRegistrationNeedsNoAllocation();
#endif
  TestNativeRootsAndMembers();
  TestUnrootedCycleAndWeakMoves();
  TestAllocationAfterMarkAndStaleSweep();
  TestLateRootAndDeferredGraph();
  TestLateRootAddressTransfer();
  TestLateEdgesAndNewGraph();
  TestAdoptionWhileMarking();
  TestConstructionAndUnwinding();
  TestWeakUnlinkAndPreparationPhases();
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  TestCallbackExceptionRecovery();
  TestAdoptionFailureCleanup();
  TestPublishedAdoptionFailure();
  TestDirectMarkSnapshotFailure();
  TestPreserveQueueAllocationFailure();
#endif
  TestAllocationInReclaimFilter();
  TestDriverReentryAndFailure();
  TestWeakContainerMoves();
  TestContainerKeysAndWeakTrace();
  TestDeepGraphAndTransferredRoot();
  TestUninterruptedMarkContract();
  TestTraceRootsDuringConstruction();
  TestWorkerConstructionDuringRootSnapshot();
  TestCancellationAndCompletedEpoch();
  TestConstructionDefersCompletedSweep();
  TestTraceRootsWithoutNativeMarking();
  TestHostIdentityAndCrossHeapCycle();
  assert(GarbageCollector::Get().ObjectCount() == 0);
  std::puts("native GC tests passed");
}
