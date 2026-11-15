// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
#include "bind/native_gc.h"
#include "bind/native_script_vm.h"

#include <atomic>
#include <cstdlib>
#include <list>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "ruby/debug.h"
#include "ruby/internal/event.h"

// Vendored CRuby private APIs (gc.c), deliberately not its internal/gc.h.
// Keep real references: the static ruby library must actually export these.
extern "C" void rb_gc_mark_weak(VALUE*);
extern "C" void rb_gc_remove_weak(VALUE, VALUE*);
extern "C" VALUE rb_callable_receiver(VALUE);

namespace binding {
namespace {
using urge::GarbageCollected;
using urge::GarbageCollector;

struct Capsule {
  GarbageCollected* native = nullptr;  // Registered native weak slot, NOT a root.
  GarbageCollected* key = nullptr;
  VALUE wrapper = Qundef;             // Address never moves while registered.
  uint64_t generation = 0;
  uint64_t weak_boundary = 0;
  bool finalized = false;
};
struct ExportedType {
  VALUE klass = Qnil;                 // Classes, unlike instances, are roots.
  const rb_data_type_t* datatype = nullptr;
  const std::type_info* type = nullptr;
  NativeTypeMatch match = nullptr;
};
struct StopScope;
struct StopFiber {
  VALUE fiber = Qundef;  // Weak: never keeps an abandoned Fiber/C stack alive.
  uint64_t weak_boundary = 0;
  StopScope* scope = nullptr;
  bool suppress = false;
};
struct Bridge {
  bool initialized = false;
  bool shutting_down = false;
  bool exit_requested = false;
  bool exit_hook = false;
  bool driving = false;
  bool entered = false;
  bool armed = false;
  bool native_open = false;
  bool authorized = false;
  bool roots_seen = false;
  bool mark_done = false;
  bool sweep_done = false;
  uint64_t token = 0;
  uint64_t claim = 0;
  uint64_t cycle = 0;
  uint64_t claim_cycle = 0;
  uint64_t slice = 0;
  uint64_t claim_slice = 0;
  uint64_t epoch = 0;
  uint64_t mark_boundary = 0;
  uint64_t generation = 0;
  size_t baseline = 0;
  size_t frames = 0;
  std::atomic<bool> requested{false};
  std::atomic<size_t> retired{0};
  std::unordered_map<GarbageCollected*, Capsule*> canonical;
  std::list<std::unique_ptr<Capsule>> capsules;
  std::list<std::unique_ptr<ExportedType>> types;
  std::unordered_set<GarbageCollected*> visited;
  std::vector<GarbageCollected*> pending;
  std::list<std::unique_ptr<StopFiber>> stop_fibers;
  // Exact method-entry identities, not names: redefining a builtin must not
  // accidentally authorize throwing through an unknown native callback.
  std::unordered_set<VALUE> safe_cframes;
  // Audited binding entries registered after builtin calibration. Function
  // pointers are not Ruby roots: complemented singleton CMEs must not pin their
  // receiver/Fiber ancestry. Redefinitions with a different CFUNC stay unsafe.
  std::vector<urge_script_entry> safe_pod_entries;
  bool draining = false;
};
// Stable storage spans the entire VM lifetime, including finalization. Never
// register a Ruby root at an address owned by BindingMain or its Fiber stack.
Bridge g;
VALUE g_sentinel = Qnil;
VALUE g_stop_token = Qnil;
VALUE g_finalizer_class = Qnil;

// A scope lives only on its own Ruby machine stack. Weak StopFiber records may
// outlive a GC-discarded stack, but their scope pointer is never dereferenced
// after the weak Fiber slot has been cleared. rb_ensure pops live scopes on ALL
// jumps, including ordinary errors and user throw/break/return.
struct StopScope {
  StopFiber* owner;
  StopScope* parent;
  bool delivered;
  bool finalizer;
  VALUE (*body)(VALUE);
  VALUE data;
};

void PurgeStopFibers() {
  for (auto it = g.stop_fibers.begin(); it != g.stop_fibers.end();) {
    auto* record = it->get();
    if (record->fiber == Qundef && record->weak_boundary <= g.mark_boundary) {
      rb_gc_remove_weak(g_sentinel, &record->fiber);
      it = g.stop_fibers.erase(it);  // Never dereference an abandoned scope.
    } else {
      ++it;
    }
  }
}
StopFiber* FindStopFiber(VALUE fiber, bool create) {
  if (create)
    PurgeStopFibers();  // Ruby-only Fiber churn must not accumulate dead records.
  for (const auto& record : g.stop_fibers)
    if (record->fiber == fiber)
      return record.get();
  if (!create)
    return nullptr;
  StopFiber* record = nullptr;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  try {
#endif
    auto entry = std::make_unique<StopFiber>();
    entry->fiber = fiber;
    record = entry.get();
    g.stop_fibers.push_back(std::move(entry));
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  } catch (const std::bad_alloc&) {
    record = nullptr;
  }
  if (!record)
    rb_memerror();
#endif
  return record;
}

StopScope* CurrentStopScope() {
  auto* record = FindStopFiber(rb_fiber_current(), false);
  return record ? record->scope : nullptr;
}

void DeliverStop() {
  auto* scope = CurrentStopScope();
  if (g.exit_requested && scope && !scope->delivered && !scope->owner->suppress) {
    scope->delivered = true;
    rb_throw_obj(g_stop_token, Qnil);
  }
}

bool SafeCFrame(VALUE frame) {
  if (g.safe_cframes.count(frame))
    return true;
  for (auto entry : g.safe_pod_entries)
    if (urge_profile_frame_matches_entry(frame, entry))
      return true;
  return false;
}

void ExitEvent(rb_event_flag_t, VALUE, VALUE, ID, VALUE) {
  auto* scope = CurrentStopScope();
  if (!g.exit_requested || !scope || scope->delivered || scope->finalizer ||
      scope->owner->suppress)
    return;
  // LINE can occur in a Ruby callback under a native binding's C++ RAII, or
  // while a finalizer is still inside gc.c's protection frame. Fail closed for
  // every unrecognized C frame. RB_FUNC's normal safe points handle those
  // calls after their owning locals return. Only calibrated CRuby-only
  // eval/loop/Proc.call and explicitly audited POD-only binding entries (scope,
  // Fiber body, rgss_main) are allowed here.
  VALUE frames[32];
  for (int start = 0;; start += 32) {
    const int count = rb_profile_frames(start, 32, frames, nullptr);
    for (int i = 0; i < count; ++i)
      if (NIL_P(rb_profile_frame_path(frames[i])) &&
          !SafeCFrame(frames[i]))
        return;
    if (count < 32)
      break;
  }
  DeliverStop();
}

void ArmExitEvent() {
  if (!g.exit_hook) {
    rb_add_event_hook(ExitEvent, RUBY_EVENT_LINE, Qnil);
    g.exit_hook = true;
  }
}

VALUE StopCatchBody(VALUE, VALUE opaque, int, const VALUE*, VALUE) {
  auto* scope = reinterpret_cast<StopScope*>(opaque);
  return scope->body(scope->data);
}
VALUE StopCatch(VALUE opaque) {
  auto* scope = reinterpret_cast<StopScope*>(opaque);
  VALUE result = rb_catch_obj(g_stop_token, StopCatchBody, opaque);
  // Arm while this scope still suppresses redelivery: hook registration can
  // allocate. The event never performs native collection, only private throw.
  if (g.exit_requested && scope->delivered)
    ArmExitEvent();
  RB_GC_GUARD(result);
  return result;
}
VALUE PopStopScope(VALUE opaque) {
  auto* scope = reinterpret_cast<StopScope*>(opaque);
  scope->owner->scope = scope->parent;
  return Qnil;  // No Ruby APIs, even during nested native ensure cleanup.
}
VALUE RunStopScope(VALUE (*body)(VALUE), VALUE data, bool finalizer) {
  VALUE fiber = rb_fiber_current();
  auto* owner = FindStopFiber(fiber, true);
  StopScope scope{owner, owner->scope,
                  owner->suppress || (owner->scope && owner->scope->delivered),
                  finalizer, body, data};
  owner->scope = &scope;
  const VALUE opaque = reinterpret_cast<VALUE>(&scope);
  VALUE result = rb_ensure(StopCatch, opaque, PopStopScope, opaque);
  RB_GC_GUARD(fiber);
  return result;
}

void RecordSafeCFrame() {
  VALUE frame;
  if (rb_profile_frames(0, 1, &frame, nullptr) == 1 &&
      NIL_P(rb_profile_frame_path(frame)))
    g.safe_cframes.insert(frame);
}

struct ProcContext {
  VALUE proc;
  int argc;
  const VALUE* argv;
  VALUE block;
  int keywords;
};
VALUE InvokeProc(VALUE opaque) {
  auto* context = reinterpret_cast<ProcContext*>(opaque);
  return rb_proc_call_with_block_kw(context->proc, context->argc, context->argv,
                                    context->block, context->keywords);
}
VALUE RunFiberBody(int argc, VALUE* argv, VALUE) {
  rb_check_arity(argc, 1, UNLIMITED_ARGUMENTS);
  // One-time first-entry correction, NOT a save/restore around Proc.call.
  // The user's Fiber root must also remain selected while suspended and during
  // every user ensure/nonlocal exit. root_svar remains entirely CRuby-owned.
  urge_fiber_set_root_proc(argv[0]);
  RecordSafeCFrame();
  ProcContext context{argv[0], argc - 1, argv + 1,
                      rb_block_given_p() ? rb_block_proc() : Qnil,
                      rb_keyword_given_p()};
  VALUE result = RunStopScope(InvokeProc, reinterpret_cast<VALUE>(&context), false);
  RB_GC_GUARD(context.proc);
  RB_GC_GUARD(context.block);
  return result;
}

struct SwitchContext {
  int argc;
  const VALUE* argv;
  int keywords;
  StopFiber* target;
  bool previous_suppress;
};
VALUE SwitchFiberBody(VALUE opaque) {
  auto* context = reinterpret_cast<SwitchContext*>(opaque);
  // rb_call_super_kw forwards the original block as well as keyword status.
  VALUE result = rb_call_super_kw(context->argc, context->argv, context->keywords);
  // A Fiber's local catch has already run all its ensures. Propagate into its
  // resumer's catch, rather than exposing an exception to user rescue clauses.
  DeliverStop();
  RB_GC_GUARD(result);
  return result;
}
VALUE RestoreFiberSuppression(VALUE opaque) {
  auto* context = reinterpret_cast<SwitchContext*>(opaque);
  context->target->suppress = context->previous_suppress;
  return Qnil;
}
VALUE SwitchFiber(int argc, VALUE* argv, VALUE self) {
  // Do not pin this dynamic method-entry: singleton Fiber ancestry could make
  // it a strong root back to that Fiber. No LINE delivery is needed while this
  // POD-only switch frame is active; its return boundary relays explicitly.
  DeliverStop();
  auto* scope = CurrentStopScope();
  auto* target = FindStopFiber(self, true);
  SwitchContext context{argc, argv, rb_keyword_given_p(), target, target->suppress};
  target->suppress = scope && scope->delivered;
  const VALUE opaque = reinterpret_cast<VALUE>(&context);
  VALUE result = rb_ensure(SwitchFiberBody, opaque, RestoreFiberSuppression, opaque);
  RB_GC_GUARD(self);
  return result;
}

struct Finalizer {
  VALUE callable;
};
void FinalizerMark(void* payload) {
  if (payload)
    rb_gc_mark_movable(static_cast<Finalizer*>(payload)->callable);
}
void FinalizerCompact(void* payload) {
  if (payload) {
    auto* finalizer = static_cast<Finalizer*>(payload);
    finalizer->callable = rb_gc_location(finalizer->callable);
  }
}
const rb_data_type_t kFinalizerType = {
    "URGE::PrivateFinalizer",
    {FinalizerMark, RUBY_TYPED_DEFAULT_FREE, nullptr, FinalizerCompact, {}},
    nullptr, nullptr, RUBY_TYPED_FREE_IMMEDIATELY};
VALUE FinalizerInspect(VALUE self) {
  return rb_inspect(static_cast<Finalizer*>(RTYPEDDATA_DATA(self))->callable);
}
VALUE FinalizerEqual(VALUE self, VALUE other) {
  if (!rb_typeddata_is_kind_of(other, &kFinalizerType))
    return Qfalse;
  return rb_equal(static_cast<Finalizer*>(RTYPEDDATA_DATA(self))->callable,
                  static_cast<Finalizer*>(RTYPEDDATA_DATA(other))->callable);
}
struct FinalizerContext {
  VALUE callable;
  VALUE objid;
};
VALUE InvokeFinalizer(VALUE opaque) {
  auto* context = reinterpret_cast<FinalizerContext*>(opaque);
  // Same private-call and method_missing semantics as gc.c, not Proc#call.
  return rb_check_funcall(context->callable, rb_intern("call"), 1, &context->objid);
}
VALUE CallFinalizer(VALUE self, VALUE objid) {
  FinalizerContext context{static_cast<Finalizer*>(RTYPEDDATA_DATA(self))->callable,
                           objid};
  VALUE result = RunStopScope(InvokeFinalizer, reinterpret_cast<VALUE>(&context), true);
  RB_GC_GUARD(context.callable);
  RB_GC_GUARD(self);
  return result;
}
VALUE DefineFinalizer(int argc, VALUE* argv, VALUE) {
  VALUE object, callable;
  rb_scan_args(argc, argv, "11", &object, &callable);
  if (argc == 1)
    callable = rb_block_proc();
  // Match Ruby 4 gc.c's validation/warning ordering before substituting our
  // private callable. Keep the public [0, original_callable].freeze contract.
  if (rb_callable_receiver(callable) == object)
    rb_warn("finalizer references object to be finalized");
  if (!RB_FL_ABLE(object))
    rb_raise(rb_eArgError, "cannot define finalizer for %s", rb_obj_classname(object));
  rb_check_frozen(object);
  if (!rb_obj_respond_to(callable, rb_intern("call"), TRUE))
    rb_raise(rb_eArgError, "wrong type argument %" PRIsVALUE " (should be callable)",
             rb_obj_class(callable));
  Finalizer* payload;
  VALUE wrapper = TypedData_Make_Struct(g_finalizer_class, Finalizer,
                                       &kFinalizerType, payload);
  payload->callable = callable;
  VALUE registered = rb_define_finalizer(object, wrapper);
  VALUE callback = rb_ary_entry(registered, 1);
  // A preexisting finalizer installed directly through the C API may win a
  // user-defined equality comparison. Never reinterpret an arbitrary payload.
  VALUE original = rb_typeddata_is_kind_of(callback, &kFinalizerType)
                       ? static_cast<Finalizer*>(RTYPEDDATA_DATA(callback))->callable
                       : callback;
  VALUE result = rb_obj_freeze(rb_ary_new3(2, INT2FIX(0), original));
  RB_GC_GUARD(wrapper);
  RB_GC_GUARD(callable);
  return result;
}

VALUE CaptureSafeFrames(VALUE) {
  VALUE frames[16];
  const int count = rb_profile_frames(0, 16, frames, nullptr);
  for (int i = 0; i < count; ++i)
    if (NIL_P(rb_profile_frame_path(frames[i])))
      g.safe_cframes.insert(frames[i]);
  return Qnil;
}

bool Live(const Capsule* cap) {
  if (!cap || cap->finalized || !cap->native ||
      RB_SPECIAL_CONST_P(cap->wrapper))
    return false;
  // Weak slots are cleared to Qundef BEFORE zombie conversion. Also validate
  // payload identity: a late old dfree cannot affect an address-reused wrapper.
  return RB_TYPE_P(cap->wrapper, T_DATA) && RTYPEDDATA_P(cap->wrapper) &&
         RTYPEDDATA_DATA(cap->wrapper) == cap;
}

class HostVisitor final : public urge::GCVisitor {
 public:
  void Visit(GarbageCollected* object) override {
    if (object && g.visited.insert(object).second)
      g.pending.push_back(object);
  }
  void Drain() {
    if (g.draining)
      return;
    g.draining = true;
    while (!g.pending.empty()) {
      auto* object = g.pending.back();
      g.pending.pop_back();
      if (g.native_open && g.authorized)
        GarbageCollector::Get().Mark(object);  // observer is always null
      const auto found = g.canonical.find(object);
      if (found != g.canonical.end() && Live(found->second))
        rb_gc_mark(found->second->wrapper);
      // No wrapper manufacture, native epochs or persistent native roots in
      // automatic Ruby GC. The independent visitor owns this slice's closure.
      object->Trace(*this);
    }
    g.draining = false;
  }
};

void PrepareDmark() {
  if (!g.entered) {
    // CRuby skips event delivery when ec->cfp is absent (notably VM teardown).
    // Keep reachability correct without trusting a stale shared visited set;
    // an eventless pass can never authorize native sweep.
    g.authorized = false;
    g.visited.clear();
    g.pending.clear();
  }
}

void SentinelMark(void*) {
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  try {
#endif
    PrepareDmark();
    for (const auto& record : g.stop_fibers) {
      if (!RB_SPECIAL_CONST_P(record->fiber)) {
        rb_gc_mark_weak(&record->fiber);
        record->weak_boundary = g.mark_boundary + 1;
      }
    }
    for (VALUE frame : g.safe_cframes)
      rb_gc_mark(frame);  // Pin internal method entries; no Fiber/Proc roots.
    for (const auto& cap : g.capsules) {
      if (Live(cap.get())) {
        rb_gc_mark_weak(&cap->wrapper);
        cap->weak_boundary = g.mark_boundary + 1;
      }
    }
    HostVisitor visitor;
    GarbageCollector::Get().TraceRoots(visitor);
    visitor.Drain();
    if (g.native_open && g.authorized)
      g.roots_seen = true;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  } catch (...) {
    // Continuing a partially traced host graph would lose Ruby ivars. A mark
    // callback cannot safely raise/recover; fail closed rather than reclaim it.
    std::abort();
  }
#endif
}
void SentinelCompact(void*) {
  for (const auto& record : g.stop_fibers)
    if (!RB_SPECIAL_CONST_P(record->fiber))
      record->fiber = rb_gc_location(record->fiber);
  for (const auto& cap : g.capsules)
    if (!cap->finalized && !RB_SPECIAL_CONST_P(cap->wrapper))
      cap->wrapper = rb_gc_location(cap->wrapper);
}
const rb_data_type_t kSentinelType = {
    "URGE::NativeRoots",
    {SentinelMark, nullptr, nullptr, SentinelCompact, {}},
    nullptr, nullptr, 0};  // WB-unprotected: minor and final incremental rescan.

void Invalidate() {
  // A failed/reentered/eventless host pass must not leave native marking open,
  // even if a worker entered a construction scope. This neither traces native
  // objects nor calls Ruby, and also revokes an unconsumed completed epoch.
  if (g.native_open || g.authorized)
    GarbageCollector::Get().CancelMark();
  g.native_open = g.authorized = false;
}

void GCEvent(rb_event_flag_t event, VALUE, VALUE, ID, VALUE) {
  // No Ruby API, allocation of Ruby objects, marking of VALUEs, or destruction
  // of resources in ANY internal hook. EXIT still runs with during_gc=true.
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  try {
#endif
    if (event == RUBY_INTERNAL_EVENT_GC_ENTER) {
      if (g.entered)
        Invalidate();
      g.entered = true;
      ++g.slice;
      g.visited.clear();
      g.pending.clear();
      g.draining = false;
    } else if (event == RUBY_INTERNAL_EVENT_GC_START) {
      ++g.cycle;
      Invalidate();
      if (g.driving && g.armed && g.entered && !g.shutting_down) {
        g.armed = false;  // old incremental rest never emits START
        g.claim = g.token;
        g.claim_cycle = g.cycle;
        g.claim_slice = g.slice;
        g.roots_seen = g.mark_done = g.sweep_done = false;
        auto& gc = GarbageCollector::Get();
        const auto previous = gc.Epoch();
        gc.BeginMark();
        g.epoch = gc.Epoch();
        g.native_open = g.epoch != previous;
        g.authorized = g.native_open;
      }
    } else if (event == RUBY_INTERNAL_EVENT_GC_END_MARK) {
      ++g.mark_boundary;  // CRuby has now cleared ALL registered weak addresses.
      if (g.native_open) {
        const bool complete = g.entered && g.authorized && g.roots_seen &&
                              g.claim_cycle == g.cycle &&
                              g.claim_slice == g.slice;
        auto& gc = GarbageCollector::Get();
        const bool native_complete = complete && gc.EndMark() &&
                                     gc.CompletedEpoch() == g.epoch;
        if (!native_complete)
          gc.CancelMark();
        g.native_open = false;
        g.authorized = g.mark_done = native_complete;
      }
    } else if (event == RUBY_INTERNAL_EVENT_GC_END_SWEEP) {
      if (g.entered && g.authorized && g.mark_done &&
          g.claim_cycle == g.cycle)
        g.sweep_done = true;
    } else if (event == RUBY_INTERNAL_EVENT_GC_EXIT) {
      if (!g.entered)
        Invalidate();
      // Leaving before END_MARK would be a native incremental cycle, which
      // this collector explicitly does not support. Never span mutator gaps.
      if (g.native_open)
        Invalidate();
      g.entered = false;
    }
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  } catch (...) {
    // Hooks cannot longjmp through CRuby internals. Stop rather than leave a
    // half native mark or permit a sweep after a tracing failure.
    std::abort();
  }
#endif
}

void RequestCollection(void*) { g.requested.store(true); }
bool CanReclaim(GarbageCollected* object, void*) {
  const auto found = g.canonical.find(object);
  return found == g.canonical.end() || found->second->native != object ||
         found->second->finalized;
}
void ClearNativeSlot(void* payload) {
  static_cast<Capsule*>(payload)->native = nullptr;
}

Capsule* Attach(VALUE wrapper, GarbageCollected* native) {
  // This helper has C++ RAII, but calls NO Ruby API and cannot Ruby-longjmp.
  auto cap = std::make_unique<Capsule>();
  cap->native = cap->key = native;
  cap->wrapper = wrapper;
  cap->generation = ++g.generation;
  auto* result = cap.get();
  g.capsules.push_back(std::move(cap));
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  try {
#endif
    g.canonical.insert_or_assign(native, result);
    GarbageCollector::Get().RegisterWeak(result, native, ClearNativeSlot);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  } catch (...) {
    const auto found = g.canonical.find(native);
    if (found != g.canonical.end() && found->second == result)
      g.canonical.erase(found);
    g.capsules.pop_back();
    throw;
  }
#endif
  return result;
}

void PurgeRetired() {
  PurgeStopFibers();
  for (auto it = g.capsules.begin(); it != g.capsules.end();) {
    auto* cap = it->get();
    // Never free an address still registered in an incremental weak list.
    // remove_weak removes just one registration; our slice rescans can have
    // registered duplicates, so a completed END_MARK boundary is mandatory.
    if (cap->finalized && !cap->native &&
        cap->weak_boundary <= g.mark_boundary) {
      rb_gc_remove_weak(g_sentinel, &cap->wrapper);
      GarbageCollector::Get().UnregisterWeak(cap);
      const auto found = g.canonical.find(cap->key);
      if (found != g.canonical.end() && found->second == cap)
        g.canonical.erase(found);
      it = g.capsules.erase(it);
    } else {
      ++it;
    }
  }
}

VALUE StartRubyGC(VALUE) {
  rb_gc_start();  // full + immediate mark; GC.stress can still make sweep lazy.
  if (!g.entered && !g.armed && g.authorized && g.mark_done && !g.sweep_done &&
      g.claim == g.token && g.claim_cycle == g.cycle) {
    // Vendored CRuby's rb_gc_disable synchronously gc_rest()s the CURRENT
    // cycle before disabling GC. Unlike another rb_gc_start, this emits no new
    // START and cannot replace our claim with yet another lazy-sweep cycle.
    // Restore the enabled state immediately: rb_gc_enable cannot raise, and
    // rb_gc_disable changes the setting only AFTER gc_rest returns. No Ruby
    // code runs in the disabled interval; GC.stress/config are never changed.
    const VALUE was_disabled = rb_gc_disable();
    if (!RTEST(was_disabled))
      rb_gc_enable();
  }
  return Qnil;
}
void DriveCollection() {
  g.driving = true;
  g.armed = true;
  ++g.token;
  g.claim = 0;
  g.authorized = g.roots_seen = g.mark_done = g.sweep_done = false;
  int state = 0;
  rb_protect(StartRubyGC, Qnil, &state);
  // This is the first actual Ruby/native safe point, NOT GC_EXIT. Finalizers
  // have run and may have triggered a different GC, invalidating our token.
  auto& gc = GarbageCollector::Get();
  const bool sweep = !state && !g.entered && !g.armed && g.authorized &&
                     g.claim == g.token && g.claim_cycle == g.cycle &&
                     g.roots_seen && g.mark_done && g.sweep_done &&
                     g.epoch == gc.CompletedEpoch();
  g.armed = false;
  bool swept = false;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  try {
#endif
    swept = sweep && gc.Sweep(g.epoch);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  } catch (...) {
    Invalidate();
    g.driving = false;
    g.requested.store(true);
    throw;
  }
#endif
  Invalidate();
  PurgeRetired();
  g.driving = false;
  if (swept) {
    g.baseline = gc.ObjectCount();
    g.retired.store(0);
  } else {
    // No END_MARK, an intervening START/finalizer, or a worker construction
    // barrier is not a consumed collection. Keep pressure and retry at the
    // next safe point; never silently accept this heap as the new baseline.
    g.requested.store(true);
  }
  if (state)
    rb_jump_tag(state);
}

#if defined(URGE_GC_TEST_EXPORTS)
VALUE NativeGCStats(VALUE) {
  // Snapshot before allocating the result: its Ruby allocations may themselves
  // collect. Only integer counts escape, never roots to all native wrappers.
  const size_t objects = GarbageCollector::Get().ObjectCount();
  const size_t capsules = g.capsules.size();
  size_t live = 0, retired = 0;
  for (const auto& cap : g.capsules) {
    live += Live(cap.get());
    retired += cap->finalized;
  }
  VALUE result = rb_hash_new();
  rb_hash_aset(result, ID2SYM(rb_intern("native_objects")), SIZET2NUM(objects));
  rb_hash_aset(result, ID2SYM(rb_intern("capsules")), SIZET2NUM(capsules));
  rb_hash_aset(result, ID2SYM(rb_intern("live_wrappers")), SIZET2NUM(live));
  rb_hash_aset(result, ID2SYM(rb_intern("retired_capsules")), SIZET2NUM(retired));
  return result;
}
VALUE CollectNativeGCForTest(VALUE) {
  g.requested.store(true);
  // The Ruby method entry owns no native RAII, just like an RB_FUNC boundary.
  // A finalizer's reentrant request stays queued for the next legal boundary.
  NativeGCSafePoint();
  return Qnil;
}
#endif

// Internal borrowed operation. Only OwnedBody may call it across Ruby
// allocation: the context's Root protects native and its complete C++ graph.
VALUE WrapNativeObjectBody(GarbageCollected* native,
                           const rb_data_type_t& fallback);
struct OwnedContext {
  GarbageCollector::Root* root;
  VALUE self;
  const rb_data_type_t* datatype;
  bool allocate_self;
};
VALUE OwnedBody(VALUE opaque) {
  auto* context = reinterpret_cast<OwnedContext*>(opaque);
  auto* native = context->root ? context->root->get() : nullptr;
  if (context->datatype)
    return WrapNativeObjectBody(native, *context->datatype);
  VALUE self = context->allocate_self ? rb_obj_alloc(context->self)
                                     : context->self;
  VALUE result = SetupNativeSelf(self, native);
  RB_GC_GUARD(self);
  return result;
}
VALUE DeleteOwnedRoot(VALUE opaque) {
  auto* context = reinterpret_cast<OwnedContext*>(opaque);
  delete context->root;
  context->root = nullptr;
  return Qnil;
}
}  // namespace

void InitializeNativeGC() {
  if (g.initialized)
    return;
  g.shutting_down = g.exit_requested = g.exit_hook = false;
  // Non-empty payload: CRuby does not dmark a TypedData with null data.
  rb_gc_register_address(&g_sentinel);
  g_sentinel = rb_data_typed_object_wrap(rb_cObject, &g, &kSentinelType);
  rb_gc_register_address(&g_stop_token);
  g_stop_token = rb_obj_alloc(rb_cObject);
  rb_add_event_hook(GCEvent,
                    RUBY_INTERNAL_EVENT_GC_ENTER | RUBY_INTERNAL_EVENT_GC_EXIT |
                    RUBY_INTERNAL_EVENT_GC_START | RUBY_INTERNAL_EVENT_GC_END_MARK |
                    RUBY_INTERNAL_EVENT_GC_END_SWEEP, Qnil);
  GarbageCollector::Get().SetHost(nullptr, RequestCollection, CanReclaim, &g);
  g.baseline = GarbageCollector::Get().ObjectCount();
  g.initialized = true;
#if defined(URGE_GC_TEST_EXPORTS)
  VALUE mod = rb_define_module("URGE");
  rb_define_module_function(mod, "__native_gc_stats", NativeGCStats, 0);
  rb_define_module_function(mod, "__collect_native_gc", CollectNativeGCForTest, 0);
#endif
}
void InitializeNativeScriptControl() {
  rb_gc_register_address(&g_finalizer_class);
  g_finalizer_class = rb_class_new(rb_cObject);
  rb_undef_alloc_func(g_finalizer_class);
  rb_define_method(g_finalizer_class, "call", CallFinalizer, 1);
  rb_define_method(g_finalizer_class, "==", FinalizerEqual, 1);
  rb_define_method(g_finalizer_class, "inspect", FinalizerInspect, 0);
  VALUE finalizers = rb_module_new();
  rb_define_method(finalizers, "define_finalizer", DefineFinalizer, -1);
  VALUE objectspace = rb_const_get(rb_cObject, rb_intern("ObjectSpace"));
  rb_prepend_module(rb_singleton_class(objectspace), finalizers);
  // ObjectSpace's module_function also has a private instance entry.
  VALUE instance_finalizers = rb_module_new();
  rb_define_private_method(instance_finalizers, "define_finalizer", DefineFinalizer, -1);
  rb_prepend_module(objectspace, instance_finalizers);

  VALUE helper = rb_module_new();
  rb_define_singleton_method(helper, "run_fiber", RunFiberBody, -1);
  rb_define_singleton_method(helper, "capture_safe_frames", CaptureSafeFrames, 0);
  VALUE installer = rb_eval_string(R"ruby(
    ->(bridge) {
      # Calibrate exact pure-CRuby C frame identities before user redefinition.
      Kernel.eval('Kernel.loop { proc { bridge.capture_safe_frames }.call; break }; loop { bridge.capture_safe_frames; break }', binding)
      eval('loop { bridge.capture_safe_frames; break }', binding)
      binding.eval('loop { bridge.capture_safe_frames; break }')
      fibers = Module.new do
        define_method(:initialize) do |*args, **keywords, &body|
          if body
            wrapped = proc do |*values, **options, &block|
              bridge.run_fiber(body, *values, **options, &block)
            end
            super(*args, **keywords, &wrapped)
          else
            super(*args, **keywords)
          end
        end
        private :initialize
      end
      Fiber.prepend(fibers)
      fibers
    }
  )ruby");
  VALUE fibers = rb_funcall(installer, rb_intern("call"), 1, helper);
  rb_define_method(fibers, "resume", SwitchFiber, -1);
  rb_define_method(fibers, "transfer", SwitchFiber, -1);
  RB_GC_GUARD(helper);
  RB_GC_GUARD(installer);
  RB_GC_GUARD(fibers);
  RB_GC_GUARD(finalizers);
  RB_GC_GUARD(instance_finalizers);
}
void NativeGCSafePoint() {
  // One delivery per execution scope, never repeated within its ensure unwind.
  // Fibers need their own catch/tag chain; finalizers need a catch INSIDE gc.c's
  // swallowing protection frame. Each local catch relays at a safe outer scope.
  DeliverStop();
  if (!g.initialized || g.shutting_down || g.driving || g.entered)
    return;
  const auto count = GarbageCollector::Get().ObjectCount();
  if (g.requested.exchange(false) || count >= g.baseline + 128 ||
      g.retired.load() >= 64)
    DriveCollection();
}
void NativeGCFrame() {
  if (++g.frames >= 60) {
    g.frames = 0;
    g.requested.store(true);
  }
  // This callback can be inside a native Graphics call with C++ RAII locals.
  // Submit only; the enclosing RB_FUNC boundary runs Ruby GC after it returns.
}
void RequestNativeGCExit() { g.exit_requested = true; }
bool NativeGCExitRequested() { return g.exit_requested; }
VALUE RunNativeScriptCatch(VALUE (*body)(VALUE), VALUE data) {
  return RunStopScope(body, data, false);
}
void RegisterNativeScriptPODEntry(VALUE (*entry)(int, VALUE*, VALUE)) {
  g.safe_pod_entries.push_back(entry);
}
void BeginNativeGCShutdown() {
  g.shutting_down = true;
  g.exit_requested = false;  // at_exit/finalizers may call the engine
  if (g.exit_hook)
    rb_remove_event_hook(ExitEvent);
  g.exit_hook = false;
  g.requested.store(false);
  Invalidate();
}
void FinishNativeGCShutdown() {
  // ruby_cleanup destroys the VM's event hook list and registered root list.
  // Calling rb_remove_event_hook/unregister_address now would use a dead VM.
  GarbageCollector::Get().ClearHost();
  for (const auto& cap : g.capsules)
    GarbageCollector::Get().UnregisterWeak(cap.get());
  g.canonical.clear();
  g.capsules.clear();
  g.types.clear();
  g.visited.clear();
  g.pending.clear();
  g.stop_fibers.clear();
  g.safe_cframes.clear();
  g.safe_pod_entries.clear();
  g_sentinel = g_stop_token = g_finalizer_class = Qnil;
  g.initialized = g.entered = g.armed = g.driving = g.native_open = false;
  GarbageCollector::Get().Collect();  // backend is still alive, external roots remain
}
size_t NativeGCWrapperCount() {
  size_t count = 0;
  for (const auto& cap : g.capsules)
    count += Live(cap.get());
  return count;
}
void NativeWrapperMark(void* payload) {
  auto* cap = static_cast<Capsule*>(payload);
  if (!cap || !cap->native || cap->finalized)
    return;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  try {
#endif
    PrepareDmark();
    HostVisitor visitor;
    visitor.Visit(cap->native);
    visitor.Drain();
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  } catch (...) { std::abort(); }
#endif
}
void NativeWrapperFree(void* payload) {
  auto* cap = static_cast<Capsule*>(payload);
  if (cap && !cap->finalized) {
    cap->finalized = true;
    // Keep the unique capsule, its weak slot and its native association until
    // native Sweep and a weak-list boundary allow safe retirement. No root
    // mutation, Dispose, join, delete, registry erase or Ruby API in dfree.
    g.retired.fetch_add(1);
  }
}
void NativeWrapperCompact(void* payload) {
  auto* cap = static_cast<Capsule*>(payload);
  if (cap && !RB_SPECIAL_CONST_P(cap->wrapper) && !cap->finalized)
    cap->wrapper = rb_gc_location(cap->wrapper);
}
size_t NativeWrapperSize(const void* payload) {
  return payload ? sizeof(Capsule) : 0;
}
GarbageCollected* NativeInstanceData(void* payload) {
  auto* cap = static_cast<Capsule*>(payload);
  return cap && !cap->finalized ? cap->native : nullptr;
}
void CheckNativeSelfEmpty(VALUE self) {
  if (RTYPEDDATA_DATA(self))
    rb_raise(rb_eRuntimeError, "native instance is already initialized");
  rb_check_frozen(self);
}
VALUE SetupNativeSelf(VALUE self, GarbageCollected* native) {
  CheckNativeSelfEmpty(self);
  if (!native)
    return self;
  bool matched = false;
  for (auto* datatype = RTYPEDDATA_TYPE(self); datatype && !matched;
       datatype = datatype->parent) {
    for (const auto& entry : g.types) {
      if (entry->datatype != datatype)
        continue;
      if (!entry->match(native))
        rb_raise(rb_eTypeError, "native object does not match %s datatype",
                 datatype->wrap_struct_name);
      matched = true;
      break;
    }
  }
  if (!matched)
    rb_raise(rb_eTypeError, "unregistered native wrapper datatype");
  const auto found = g.canonical.find(native);
  if (found != g.canonical.end() && found->second->native == native)
    rb_raise(rb_eRuntimeError, "native instance already has a canonical wrapper");
  Capsule* cap = nullptr;
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  try {
#endif
    cap = Attach(self, native);
#if defined(__cpp_exceptions) || defined(_CPPUNWIND)
  } catch (const std::bad_alloc&) {
    // Attach's pure C++ frame has unwound before Ruby's longjmp.
  }
  if (!cap)
    rb_memerror();
#endif
  RTYPEDDATA_DATA(self) = cap;
  return self;
}
void RegisterNativeTypeImpl(VALUE klass, const rb_data_type_t& datatype,
                            const std::type_info& type, NativeTypeMatch match) {
  auto* entry = new ExportedType{Qnil, &datatype, &type, match};
  g.types.emplace_back(entry);
  rb_gc_register_address(&entry->klass);
  entry->klass = klass;
  RB_GC_GUARD(klass);
}
namespace {
VALUE WrapNativeObjectBody(GarbageCollected* native,
                           const rb_data_type_t& fallback) {
  if (!native)
    return Qnil;
  const auto found = g.canonical.find(native);
  if (found != g.canonical.end() && found->second->native == native)
    return Live(found->second) ? found->second->wrapper : Qnil;
  // Pending-finalization weak parents are a legitimate nil, not resurrection.
  // Most-derived exported dynamic type wins; unexported derived native types
  // fall back to their closest exported base rather than the getter's type.
  ExportedType* chosen = nullptr;
  size_t chosen_depth = 0;
  for (const auto& entry : g.types) {
    if (*entry->type == typeid(*native)) {
      chosen = entry.get();
      break;
    }
    if (entry->match(native)) {
      size_t depth = 0;
      for (auto* type = entry->datatype; type; type = type->parent)
        ++depth;
      if (!chosen || depth > chosen_depth) {
        chosen = entry.get();
        chosen_depth = depth;
      }
    }
  }
  if (!chosen)
    rb_raise(rb_eTypeError, "unregistered native type (expected %s)",
             fallback.wrap_struct_name);
  VALUE self = rb_data_typed_object_wrap(chosen->klass, nullptr, chosen->datatype);
  VALUE result = SetupNativeSelf(self, native);
  RB_GC_GUARD(self);
  return result;
}
}  // namespace
VALUE WrapNativeObject(GarbageCollected* native, const rb_data_type_t& fallback) {
  // Public borrowed wrapping may allocate a wrapper. Root it here rather than
  // relying on a caller-owned handle or keeping a C++ RAII local across longjmp.
  return WrapOwnedNative(new GarbageCollector::Root(native), fallback);
}
VALUE WrapOwnedNative(GarbageCollector::Root* root,
                      const rb_data_type_t& datatype) {
  OwnedContext context{root, Qnil, &datatype, false};
  const VALUE opaque = reinterpret_cast<VALUE>(&context);
  return rb_ensure(OwnedBody, opaque, DeleteOwnedRoot, opaque);
}
VALUE SetupOwnedNative(VALUE self, GarbageCollector::Root* root,
                       bool allocate_self) {
  OwnedContext context{root, self, nullptr, allocate_self};
  const VALUE opaque = reinterpret_cast<VALUE>(&context);
  return rb_ensure(OwnedBody, opaque, DeleteOwnedRoot, opaque);
}
}  // namespace binding
