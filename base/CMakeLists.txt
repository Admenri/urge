# base components
cmake_minimum_required(VERSION 3.20.1)

#--------------------------------------------------------------------------------
# base components
#--------------------------------------------------------------------------------

set(ENGINE_BASE_SRC
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
  containers/linked_list.cc
  containers/linked_list.h
  debug/logging.cc
  debug/logging.h
  math/rectangle.cc
  math/rectangle.h
  math/vector.h
  memory/allocator.h
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
  thread/thread_checker.cc
  thread/thread_checker.h
  worker/thread_worker.cc
  worker/thread_worker.h
  template_util.h
)

add_library(engine_base STATIC ${ENGINE_BASE_SRC})
target_link_libraries(engine_base
 PUBLIC
  Diligent-BuildSettings
  Diligent-Common
  mimalloc-static
  spdlog::spdlog
)

target_compile_definitions(engine_base
 PUBLIC
  $<$<CONFIG:Debug>:URGE_DEBUG>
)

if(SDL_STATIC)
  target_link_libraries(engine_base PUBLIC SDL3::SDL3-static)
else()
  target_link_libraries(engine_base PUBLIC SDL3::SDL3-shared)
endif()

if (MSVC)
  target_compile_options(engine_base PRIVATE /WX)
else ()
  target_compile_options(engine_base PRIVATE -Werror)
endif ()
