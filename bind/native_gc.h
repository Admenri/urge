// Licensed under the MIT License <http://opensource.org/licenses/MIT>.
// SPDX-License-Identifier: MIT
#pragma once

#include <cstddef>
#include <typeinfo>

#include "ruby.h"
#include "core/common/garbage_collector.h"

namespace binding {

// These entry points run on the Ruby execution stack with the GVL, with native
// graph mutators stopped. The host driver itself only submits an atomic request
// (including when GarbageCollector::Collect is called by a native worker).
void InitializeNativeGC();
void NativeGCSafePoint();
void NativeGCFrame();
void RequestNativeGCExit();
// Install after CRuby's builtin initializers, before executing user scripts.
// Adds private per-execution catches to Ruby-created Fibers and finalizers.
// This is not a termination guarantee for direct C-API Fiber/finalizer/callback
// bypasses, or a callback that never returns from an unknown native frame: LINE
// delivery fails closed to protect C++ RAII. Likewise an ensure that actively
// replaces stop with another jump, then rescues that jump and continues, can
// defeat shutdown. Never redeliver into that ensure merely to force termination.
void InitializeNativeScriptControl();
// Remains true throughout shutdown unwinding, including nested execution scopes.
bool NativeGCExitRequested();
VALUE RunNativeScriptCatch(VALUE (*body)(VALUE), VALUE data);
// Only register audited whole C entries with no C++ owning locals anywhere
// around Ruby callbacks. Matching is by exact CFUNC pointer AND arity, not name.
void RegisterNativeScriptPODEntry(VALUE (*entry)(int, VALUE*, VALUE));
void BeginNativeGCShutdown();
// After ruby_cleanup: entirely C++, never unregister Ruby roots from a dead VM.
void FinishNativeGCShutdown();
size_t NativeGCWrapperCount();

void NativeWrapperMark(void* payload);
void NativeWrapperFree(void* payload);
void NativeWrapperCompact(void* payload);
size_t NativeWrapperSize(const void* payload);
urge::GarbageCollected* NativeInstanceData(void* payload);
void CheckNativeSelfEmpty(VALUE self);
VALUE SetupNativeSelf(VALUE self, urge::GarbageCollected* native);
VALUE WrapNativeObject(urge::GarbageCollected* native,
                       const rb_data_type_t& fallback);

using NativeTypeMatch = bool (*)(urge::GarbageCollected*);
void RegisterNativeTypeImpl(VALUE klass, const rb_data_type_t& datatype,
                            const std::type_info& type, NativeTypeMatch match);
template <typename T>
void RegisterNativeType(VALUE klass, const rb_data_type_t& datatype) {
  RegisterNativeTypeImpl(klass, datatype, typeid(T),
                        [](urge::GarbageCollected* object) {
                          return dynamic_cast<T*>(object) != nullptr;
                        });
}

// Consume exactly one heap Root, on success AND Ruby longjmp. The root's
// deletion unregisters its address; it never deletes the managed native object.
// allocate_self puts rb_obj_alloc inside the ensure-protected body (_load).
VALUE WrapOwnedNative(urge::GarbageCollector::Root* root,
                      const rb_data_type_t& datatype);
VALUE SetupOwnedNative(VALUE self_or_class, urge::GarbageCollector::Root* root,
                       bool allocate_self = false);

}  // namespace binding
