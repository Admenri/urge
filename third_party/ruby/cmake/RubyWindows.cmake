# ---------------------------------------------------------------------------
# RubyWindows.cmake -- bits shared by all Windows toolchains
#
# Both MSVC and MinGW link the same win32/ compatibility layer and use the
# Win32 thread implementation.  Only the coroutine backend differs (MASM for
# MSVC, GNU as for MinGW).
# ---------------------------------------------------------------------------

set(RUBY_THREAD_MODEL "win32")
set(RUBY_THREAD_IMPL_H "thread_win32.h")
set(RUBY_THREAD_IMPL_SRC "thread_win32.c")

if(CMAKE_SIZEOF_VOID_P EQUAL 8)
  set(_ruby_coroutine_dir "win64")
else()
  set(_ruby_coroutine_dir "win32")
endif()

set(RUBY_COROUTINE_H "coroutine/${_ruby_coroutine_dir}/Context.h")

if(MSVC)
  set(RUBY_COROUTINE_SRC "coroutine/${_ruby_coroutine_dir}/Context.asm")
  enable_language(ASM_MASM)
  # `Context.asm` is MASM syntax.  The extension alone does not say so: another
  # project in the same tree may have enabled ASM_NASM (dav1d does), and then
  # CMake hands every `.asm` file to nasm -- which rejects MASM directives and
  # is invoked with the host's default object format anyway.  Naming the
  # language on the file keeps it on ml64 no matter who else is in the build.
  set_source_files_properties("${RUBY_SOURCE_DIR}/${RUBY_COROUTINE_SRC}"
    PROPERTIES LANGUAGE ASM_MASM)
else()
  set(RUBY_COROUTINE_SRC "coroutine/${_ruby_coroutine_dir}/Context.S")
  enable_language(ASM)
endif()

list(APPEND RUBY_PLATFORM_SOURCES
  "${RUBY_SOURCE_DIR}/${RUBY_COROUTINE_SRC}"
  "${RUBY_SOURCE_DIR}/win32/win32.c"
  "${RUBY_SOURCE_DIR}/win32/file.c")

message(STATUS "Windows coroutine: ${RUBY_COROUTINE_SRC}")
