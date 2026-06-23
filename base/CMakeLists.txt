# base components
cmake_minimum_required(VERSION 3.25)

#--------------------------------------------------------------------------------
# base components
#--------------------------------------------------------------------------------

add_library(engine_base STATIC)

target_sources(engine_base
 PUBLIC
  bind/bind.h
  bind/bind_internal.h
  bind/callback.h
  bind/callback_forward.h
  bind/callback_helpers.cc
  bind/callback_helpers.h
  bind/callback_internal.cc
  bind/callback_internal.h
  bind/callback_list.cc
  bind/callback_list.h
  bind/cancelable_callback.h
  buildflags/build.h
  buildflags/compiler_specific.h
  debug/logging.cc
  debug/logging.h
  memory/allocator.cc
  memory/atomic_flag.cc
  memory/atomic_flag.h
  memory/atomic_ref_count.h
  memory/auto_reset.h
  memory/lock.h
  memory/lock_impl.cc
  memory/lock_impl.h
  memory/raw_scoped_refptr_mismatch_checker.h
  memory/ref_counted.cc
  memory/ref_counted.h
  memory/scoped_refptr.h
  memory/stack_allocated.h
  memory/weak_ptr.cc
  memory/weak_ptr.h
  template/linked_list.cc
  template/linked_list.h
  thread/thread_checker.cc
  thread/thread_checker.h
  template_util.h
)

target_link_libraries(engine_base
 PUBLIC
  glm::glm
  spdlog::spdlog
  magic_enum
  mimalloc-static
)

if(MSVC)
  set(COMPILER_OPTIONS /WX)
else()
  set(COMPILER_OPTIONS -Werror)
endif()

target_compile_options(engine_base PUBLIC ${COMPILER_OPTIONS})

if(BUILD_SHARED_LIBS)
  target_link_libraries(engine_base PUBLIC SDL3::SDL3-shared)
else()
  target_link_libraries(engine_base PUBLIC SDL3::SDL3-static)
endif()
