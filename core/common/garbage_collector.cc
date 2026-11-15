// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
// Copyright (c) 2026 Admenri Adev <admenri0504@gmail.com>.

#include "core/common/garbage_collector.h"

#include <cassert>
#include <exception>
#include <mutex>
#include <unordered_map>
#include <unordered_set>

namespace urge {

struct GarbageCollector::State {
  struct WeakSlot { GarbageCollected* object; WeakClear clear; };
  std::recursive_mutex mutex;
  std::unordered_set<GarbageCollected*> objects;
  std::unordered_map<void*, WeakSlot> weak_slots;
  std::unordered_set<GarbageCollected*> doomed;
  std::vector<GarbageCollected*> pending;
  GCVisitor* preserving = nullptr;
  MarkObserver observer = nullptr;
  CollectDriver driver = nullptr;
  ReclaimFilter reclaim_filter = nullptr;
  void* host = nullptr;
  uint64_t epoch = 0;
  uint64_t completed_epoch = 0;
  size_t constructions = 0;
  bool marking = false;
  bool draining = false;
  bool sweeping = false;
  bool collecting = false;
};

GarbageCollector::State& GarbageCollector::state() {
  // Outlives native service/static handles. Collection, not static destruction
  // order, determines the lifetime of managed objects.
  static State* value = new State;
  return *value;
}

GarbageCollector& GarbageCollector::Get() {
  static GarbageCollector value;
  return value;
}

GarbageCollector::ConstructionScope::ConstructionScope() {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  ++s.constructions;
}

GarbageCollector::ConstructionScope::~ConstructionScope() {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  assert(s.constructions != 0);
  --s.constructions;
}

GarbageCollected::~GarbageCollected() {
  GarbageCollector::Get().Forget(this);
}

GarbageCollector::Root::Root(GarbageCollected* object) {
  reset(object);
}

GarbageCollector::Root::Root(const Root& other) {
  reset(other.get());
}

GarbageCollector::Root::Root(Root&& other) noexcept {
  MoveRoot(node_, other.node_);
}

GarbageCollector::Root::~Root() {
  reset();
}

GarbageCollector::Root& GarbageCollector::Root::operator=(const Root& other) {
  if (this != &other)
    reset(other.get());
  return *this;
}

GarbageCollector::Root& GarbageCollector::Root::operator=(Root&& other) noexcept {
  if (this != &other) {
    reset();
    MoveRoot(node_, other.node_);
  }
  return *this;
}

void GarbageCollector::Root::reset(GarbageCollected* object) {
  ResetRoot(node_, object);
}

void GarbageCollector::ResetRoot(detail::RootNode& root,
                                  GarbageCollected* object) {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  if (root.object == object)
    return;
  // Do not resurrect memory whose weak-unlink/prepare/delete was sealed.
  assert(!object || !object->reclaiming_);
  if (object && object->reclaiming_)
    std::terminate();
  if (root.object) {
    if (root.previous)
      root.previous->next = root.next;
    else
      root.object->root_head_ = root.next;
    if (root.next)
      root.next->previous = root.previous;
  }
  root.object = object;
  root.previous = nullptr;
  root.next = nullptr;
  if (object) {
    root.next = object->root_head_;
    if (root.next)
      root.next->previous = &root;
    object->root_head_ = &root;
  }
}

void GarbageCollector::MoveRoot(detail::RootNode& destination,
                                 detail::RootNode& source) {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  assert(destination.object == nullptr);
  destination = source;
  if (destination.object) {
    if (destination.previous)
      destination.previous->next = &destination;
    else
      destination.object->root_head_ = &destination;
    if (destination.next)
      destination.next->previous = &destination;
  }
  source = {};
}

void GarbageCollector::Adopt(GarbageCollected* object) {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  assert(object && object->root_head_ != nullptr);
  if (!object || object->root_head_ == nullptr)
    std::terminate();
  const bool inserted = s.objects.insert(object).second;
  assert(inserted);
  if (!inserted)
    std::terminate();

  if (s.marking) {
    // Born live is not enough: its outgoing edges and existing host wrapper
    // must participate in this mark. The factory has finished construction.
    object->marked_epoch_ = 0;
    Mark(object);
  } else {
    // Conservatively keep allocations until the next mark, including those
    // made after EndMark or from a reclaim filter. Sweep still traces them.
    object->marked_epoch_ = s.epoch;
    if (s.preserving)
      s.preserving->Visit(object);
  }
}

void GarbageCollector::Forget(GarbageCollected* object) {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  s.objects.erase(object);
  // Also make outstanding registrations safe if unadopted construction fails.
  // A valid rooted adopted object is never selected by Sweep.
  while (object->root_head_) {
    auto* root = object->root_head_;
    object->root_head_ = root->next;
    *root = {};
  }
  // Only the sealed batch has had its slots cleared, not every destructor
  // running during a sweep (e.g. a failed constructor from a disposal hook).
  if (s.doomed.erase(object))
    return;
  for (auto it = s.weak_slots.begin(); it != s.weak_slots.end();) {
    if (it->second.object == object) {
      const auto slot = it->first;
      const auto clear = it->second.clear;
      it = s.weak_slots.erase(it);
      clear(slot);
    } else {
      ++it;
    }
  }
}

bool GarbageCollector::Contains(const GarbageCollected* object) const {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  return s.objects.contains(const_cast<GarbageCollected*>(object));
}

size_t GarbageCollector::ObjectCount() const {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  return s.objects.size();
}

void GarbageCollector::SetHost(MarkObserver observer, CollectDriver driver,
                               ReclaimFilter reclaim_filter, void* data) {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  s.observer = observer;
  s.driver = driver;
  s.reclaim_filter = reclaim_filter;
  s.host = data;
}

void GarbageCollector::ClearHost() {
  SetHost(nullptr, nullptr, nullptr, nullptr);
}

void GarbageCollector::RegisterWeak(void* slot, GarbageCollected* object,
                                    WeakClear clear) {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  assert(slot && clear);
  if (object && !s.doomed.contains(object)) {
    // Unadopted targets are allowed for self/peer weak links in constructors.
    // The base destructor clears them if construction throws.
    s.weak_slots.insert_or_assign(slot, State::WeakSlot{object, clear});
  } else {
    s.weak_slots.erase(slot);
    clear(slot);
  }
}

void GarbageCollector::UnregisterWeak(void* slot) {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  s.weak_slots.erase(slot);
}

void GarbageCollector::TraceRoots(GCVisitor& visitor) const {
  auto& s = state();
  std::vector<GarbageCollected*> roots;
  {
    std::lock_guard lock(s.mutex);
    roots.reserve(s.objects.size());
    for (auto* object : s.objects)
      if (!object->reclaiming_ &&
          (s.constructions != 0 ||
           object->root_head_ != nullptr))
        roots.push_back(object);
  }
  // No registry iterator or mutex crosses into an independent host traversal.
  // Lifetime safety beyond this snapshot is the caller's stop-the-world duty.
  for (auto* object : roots)
    visitor.Visit(object);
}

void GarbageCollector::BeginMark() {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  if (s.marking || s.sweeping || s.constructions != 0)
    return;
  if (++s.epoch == 0) {
    ++s.epoch;
    for (auto* object : s.objects)
      object->marked_epoch_ = 0;
  }
  s.completed_epoch = 0;
  s.pending.clear();
  s.marking = true;
  s.draining = false;
}

void GarbageCollector::CancelMark() {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  s.pending.clear();
  s.draining = false;
  s.marking = false;
  s.completed_epoch = 0;
}

void GarbageCollector::Mark(GarbageCollected* object) {
  if (!object)
    return;
  auto& s = state();
  std::lock_guard lock(s.mutex);
  if (!s.marking || !s.objects.contains(object) ||
      object->marked_epoch_ == s.epoch)
    return;

#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  try {
#endif
    s.pending.push_back(object);
    object->marked_epoch_ = s.epoch;
    if (s.draining)
      return;

    class MarkVisitor final : public GCVisitor {
     public:
      void Visit(GarbageCollected* value) override {
        GarbageCollector::Get().Mark(value);
      }
    } visitor;

    s.draining = true;
    while (!s.pending.empty()) {
      auto* next = s.pending.back();
      s.pending.pop_back();
      if (!s.objects.contains(next))
        continue;
      if (s.observer)
        s.observer(next, s.host);
      next->Trace(visitor);
    }
    s.draining = false;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  } catch (...) {
    // A failed host callback or Trace cannot leave a half-mark usable by Sweep
    // or permanently prevent all future collections.
    CancelMark();
    throw;
  }
#endif
}

void GarbageCollector::MarkRoots() {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  if (!s.marking)
    return;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  try {
#endif
    // Observer callbacks can adopt objects and rehash the registry. Adopt marks
    // those new objects; this snapshot is only for roots present on entry.
    const std::vector<GarbageCollected*> objects(s.objects.begin(), s.objects.end());
    for (auto* object : objects)
      if (object->root_head_ != nullptr)
        Mark(object);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  } catch (...) {
    // Also cover snapshot allocation when a host drives marking directly.
    CancelMark();
    throw;
  }
#endif
}

bool GarbageCollector::EndMark() {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  if (!s.marking)
    return false;
  if (s.draining || s.constructions != 0) {
    CancelMark();
    return false;
  }
  MarkRoots();
  s.marking = false;
  s.completed_epoch = s.epoch;
  return true;
}

uint64_t GarbageCollector::Epoch() const {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  return s.epoch;
}

uint64_t GarbageCollector::CompletedEpoch() const {
  auto& s = state();
  std::lock_guard lock(s.mutex);
  return s.completed_epoch;
}

bool GarbageCollector::Sweep(uint64_t completed_epoch) {
  auto& s = state();
  std::vector<GarbageCollected*> garbage;
  std::unique_lock lock(s.mutex);
  if (s.marking || s.sweeping || s.constructions != 0 || completed_epoch == 0 ||
      completed_epoch != s.completed_epoch)
    return false;
  s.sweeping = true;
  s.completed_epoch = 0;  // A completed mark is consumed once, even on failure.

  struct SweepScope {
    State& s;
    ~SweepScope() {
      std::lock_guard lock(s.mutex);
      s.preserving = nullptr;
      for (auto* object : s.doomed)
        object->reclaiming_ = false;
      s.doomed.clear();
      s.sweeping = false;
    }
  } scope{s};

  // Preserve ALL live seeds with a separate visited set. Epoch marks alone
  // cannot tell whether a black object's edges changed since EndMark.
  // This deliberately never calls a host marking API after host marking ended.
  class PreserveVisitor final : public GCVisitor {
   public:
    PreserveVisitor(State& state, uint64_t epoch) : s_(state), epoch_(epoch) {}
    void Visit(GarbageCollected* object) override {
      if (!object || !s_.objects.contains(object))
        return;
      const auto [seen, inserted] = seen_.insert(object);
      if (!inserted)
        return;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
      try {
#endif
        pending_.push_back(object);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
      } catch (...) {
        // An allocation failure can be caught by the caller of Adopt while this
        // visitor remains active. A later retry must still traverse the object.
        seen_.erase(seen);
        throw;
      }
#endif
      object->marked_epoch_ = epoch_;
    }
    void Drain() {
      while (!pending_.empty()) {
        auto* object = pending_.back();
        pending_.pop_back();
        object->Trace(*this);
      }
    }
   private:
    State& s_;
    uint64_t epoch_;
    std::unordered_set<GarbageCollected*> seen_;
    std::vector<GarbageCollected*> pending_;
  } preserve(s, completed_epoch);

  s.preserving = &preserve;
  const std::vector<GarbageCollected*> objects(s.objects.begin(), s.objects.end());
  for (auto* object : objects) {
    if (object->marked_epoch_ != completed_epoch &&
        object->root_head_ == nullptr &&
        s.reclaim_filter && !s.reclaim_filter(object, s.host))
      preserve.Visit(object);
  }
  // No callbacks in this iteration. Include newly adopted allocations and any
  // roots acquired by the filter before draining their complete native graphs.
  for (auto* object : s.objects)
    if (object->marked_epoch_ == completed_epoch ||
        object->root_head_ != nullptr)
      preserve.Visit(object);
  preserve.Drain();
  s.preserving = nullptr;

  for (auto* object : s.objects)
    if (object->marked_epoch_ != completed_epoch)
      garbage.push_back(object);
  s.doomed.insert(garbage.begin(), garbage.end());
  for (auto* object : garbage)
    object->reclaiming_ = true;
  for (auto it = s.weak_slots.begin(); it != s.weak_slots.end();) {
    if (s.doomed.contains(it->second.object)) {
      const auto slot = it->first;
      const auto clear = it->second.clear;
      it = s.weak_slots.erase(it);
      clear(slot);
    } else {
      ++it;
    }
  }
  lock.unlock();

  // Never hold the registry lock while disposing resources or joining workers.
  // All weak slots are null and all garbage is allocated throughout preparation.
  for (auto* object : garbage)
    object->PrepareForCollection();
  for (auto* object : garbage)
    delete object;
  return true;
}

void GarbageCollector::Collect() {
  auto& s = state();
  CollectDriver driver;
  void* host;
  {
    std::lock_guard lock(s.mutex);
    if (s.marking || s.sweeping || s.collecting || s.constructions != 0)
      return;
    s.collecting = true;
    driver = s.driver;
    host = s.host;
  }
  struct CollectScope {
    State& s;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
    int exceptions = std::uncaught_exceptions();
#endif
    ~CollectScope() {
      std::lock_guard lock(s.mutex);
      s.collecting = false;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
      // A throwing driver invalidates both an open pass and a completed mark
      // whose result it failed to consume. Share direct-mark failure cleanup.
      if (std::uncaught_exceptions() > exceptions)
        GarbageCollector::Get().CancelMark();
#endif
    }
  } scope{s};
  if (driver) {
    driver(host);
    return;
  }
  BeginMark();
  MarkRoots();
  EndMark();
  Sweep(Epoch());
}

}
