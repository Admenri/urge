cmake_minimum_required(VERSION 3.16)

# Extract version from meson.build
file(READ "${CMAKE_CURRENT_LIST_DIR}/meson.build" meson_build)
if(NOT meson_build MATCHES "version: '([0-9.]+)'")
    message(FATAL_ERROR "Could not extract version from meson.build")
endif()
set(dav1d_meson_version ${CMAKE_MATCH_1})

if(NOT meson_build MATCHES "dav1d_soname_version[ \t]*=[ \t]*'([0-9]+)\\.([0-9]+)\\.([0-9]+)'")
    message(FATAL_ERROR "Could not extract soname from meson.build")
endif()
set(dav1d_soversion_major "${CMAKE_MATCH_1}")
set(dav1d_soversion_minor "${CMAKE_MATCH_2}")
set(dav1d_soversion_patch "${CMAKE_MATCH_3}")
set(dav1d_soversion "${dav1d_soversion_major}.${dav1d_soversion_minor}.${dav1d_soversion_patch}")

message(DEBUG "Extracted dav1d version: ${dav1d_meson_version}")
message(DEBUG "Extracted soversion:     ${dav1d_soversion} (major=${dav1d_soversion_major} minor=${dav1d_soversion_minor} patch=${dav1d_soversion_patch})")

project(dav1d LANGUAGES C VERSION "${dav1d_meson_version}")

set(DAV1D_VERSION ${PROJECT_VERSION})
set(DAV1D_API_VERSION_MAJOR "${dav1d_soversion_major}")
set(DAV1D_API_VERSION_MINOR "${dav1d_soversion_minor}")
set(DAV1D_API_VERSION_PATCH "${dav1d_soversion_patch}")

include(CheckIncludeFile)
include(CheckCCompilerFlag)
include(CheckCSourceCompiles)
include(CheckSymbolExists)
include(CMakePushCheckState)
include(TestBigEndian)

include("${CMAKE_CURRENT_LIST_DIR}/CheckCPUArchitecture.cmake")
if(CMAKE_VERSION VERSION_LESS 3.18)
    function(check_linker_flag LANG FLAG VAR)
      cmake_push_check_state()
      list(APPEND CMAKE_REQUIRED_LINK_OPTIONS ${FLAG} )
      if(LANG STREQUAL "C")
        include(CheckCSourceCompiles)
        check_c_source_compiles("int main(int argc,char*argv[]){(void)argc;(void)argv;return 0;}" ${VAR} FAIL_REGEX "warning")
      elseif(LANG STREQUAL "CXX")
        include(CheckCXXSourceCompiles)
        check_cxx_source_compiles("int main(int argc,char*argv[]){(void)argc;(void)argv;return 0;}" ${VAR} FAIL_REGEX "warning")
      else()
        message(FATAL_ERROR "Unsupported language: ${LANG}")
      endif()
    cmake_pop_check_state()
  endfunction()
else()
  cmake_policy(SET CMP0057 NEW)  # Support new if() IN_LIST operator. (used inside check_linker_flag, used in CMake 3.18)
  include(CheckLinkerFlag)
endif()

set(config_variables)
set(config_variables_value)
set(config01_variables)

check_cpu_architecture(x64 "DAV1D_CPU_X64")
check_cpu_architecture(x86 "DAV1D_CPU_X86")
check_cpu_architecture(arm32 "DAV1D_CPU_ARM32")
check_cpu_architecture(arm64 "DAV1D_CPU_ARM64")
check_cpu_architecture(powerpc64 "DAV1D_CPU_PPC64")
set(ARCH_X86_64 ${DAV1D_CPU_X64})
set(ARCH_X86_32 ${DAV1D_CPU_X86})
if(ARCH_X86_64 OR ARCH_X86_32)
  set(ARCH_X86 1)
else()
  set(ARCH_X86 0)
endif()
set(ARCH_X86_32 ${DAV1D_CPU_X86})
set(ARCH_ARM ${DAV1D_CPU_ARM32})
set(ARCH_AARCH64 ${DAV1D_CPU_ARM64})
set(ARCH_PPC64LE ${DAV1D_CPU_PPC64})
list(APPEND config01_variables ARCH_X86 ARCH_X86_32 ARCH_X86_64 ARCH_ARM ARCH_AARCH64 ARCH_PPC64LE)

set(DAV1D_ASM_DEFAULT OFF)
if(DAV1D_CPU_X64 OR DAV1D_CPU_X86 OR DAV1D_CPU_ARM32 OR DAV1D_CPU_ARM64 OR DAV1D_CPU_PPC64)
  set(DAV1D_ASM_DEFAULT ON)
endif()


option(DAV1D_ASM "Build asm files, if available" ON)
set(DAV1D_BITDEPTHS "8;16" CACHE STRING "enabled bitdepths")
option(DAV1D_LOG "logging" TRUE)

list(APPEND config_variables PREFIX)
if(DAV1D_ASM)
  if(DAV1D_CPU_X64 OR DAV1D_CPU_X86)
    # Enable ASM as well to avoid the following CMake error in the SDL_image super project:
    #   CMake Error: Error required internal CMake variable not set, cmake may not be built correctly.
    #   Missing variable is:
    #   CMAKE_ASM_COMPILE_OBJECT
    # (reproducer args: -DSDL3IMAGE_VENDORED=ON -DSDL3IMAGE_JPG=ON -DSDL3IMAGE_BACKEND_STB=OFF -DSDL3IMAGE_AVIF=ON -DDAV1D_ASM=ON)
    enable_language(ASM)
    enable_language(ASM_NASM)
    execute_process(COMMAND ${CMAKE_ASM_NASM_COMPILER} -v
      RESULT_VARIABLE nasm_rc
      OUTPUT_VARIABLE nasm_stdout
    )
    if(nasm_rc EQUAL 0 AND nasm_stdout MATCHES "version ([0-9][0-9.]+)")
      set(nasm_version "${CMAKE_MATCH_1}")
      if(nasm_version VERSION_LESS 2.14)
        message(FATAL_ERROR "nasm 2.14 or later is required, found ${nasm_version}")
      endif()
    else()
      message(FATAL_ERROR "Failed to determine nasm version")
    endif()
    check_c_source_compiles("int main(void) {\n#ifndef __AVX__\nerror no avx\n#endif\nreturn 0;}" DAV1D_WITH_AVX)
  elseif(DAV1D_CPU_ARM32 OR DAV1D_CPU_ARM64)
    enable_language(ASM)

    check_c_source_compiles("__asm__ (\n\".func cmake_test\"\n\".endfunc\"\n);\n" HAVE_AS_FUNC)
    list(APPEND config01_variables HAVE_AS_FUNC)

    check_c_source_compiles("int main(void) {
      #if defined(PIC)
      #error PIC already defined
      #elif !(defined(__PIC__) || defined(__pic__))
      #error no pic
      #endif
      return 0;
    }" DAV1D_ARM_ASM_PIC )
  endif()
  if(APPLE OR (WIN32 AND DAV1D_CPU_X86))
    set(PREFIX 1)
  endif()
endif()

foreach(depth 8 16)
  list(APPEND config01_variables CONFIG_${depth}BPC)
  if("${depth}" IN_LIST DAV1D_BITDEPTHS)
    set(DAV1D_BITDEPTH_${depth} 1)
  else()
    set(DAV1D_BITDEPTH_${depth} 0)
  endif()
  set(CONFIG_${depth}BPC ${DAV1D_BITDEPTH_${depth}})
endforeach()

list(APPEND config01_variables HAVE_ASM)
if(DAV1D_ASM AND (DAV1D_CPU_X86 OR DAV1D_CPU_X64 OR DAV1D_CPU_ARM32 OR DAV1D_CPU_ARM64))
  set(HAVE_ASM 1)
else()
  set(HAVE_ASM 0)
endif()

list(APPEND config01_variables CONFIG_LOG)
if(DAV1D_LOG)
  set(CONFIG_LOG 1)
else()
  set(CONFIG_LOG 0)
endif()

# TRIM_DSP_FUNCTIONS=1 if Release or Debug mode
add_definitions(-DTRIM_DSP_FUNCTIONS=$<BOOL:$<OR:$<CONFIG:Release>,$<CONFIG:MinSizeRel>>>)

if(DAV1D_CPU_X64 OR APPLE OR CMAKE_SYSTEM_NAME MATCHES ".*Linux")
    set(DAV1D_STACK_ALIGNMENT_DEFAULT 16)
else()
    set(DAV1D_STACK_ALIGNMENT_DEFAULT 4)
endif()
set(DAV1D_STACK_ALIGNMENT "${DAV1D_STACK_ALIGNMENT_DEFAULT}" CACHE STRING "Dav1d stack alignment")
list(APPEND config_variables_value STACK_ALIGNMENT)
set(STACK_ALIGNMENT ${DAV1D_STACK_ALIGNMENT})

if(WIN32)
  add_compile_definitions(
    _WIN32_WINNT=0x0601
    UNICODE=1
    _UNICODE=1
    __USE_MINGW_ANSI_STDIO=1
    _CRT_DECLARE_NONSTDC_NAMES=1
  )
  check_symbol_exists(fseeko stdio.h STDIO_H_PROVIDES_FSEEKO)
  if(STDIO_H_PROVIDES_FSEEKO)
    add_compile_definitions(_FILE_OFFSET_BITS=64)
  else()
    add_compile_definitions(fseeko=_fseeki64 ftello=_ftelli64)
  endif()
endif()

list(APPEND config_variables HAVE_C11_GENERIC)
check_c_source_compiles("int main() {int x = _Generic(0, default: 0); return x;}" HAVE_C11_GENERIC)

check_include_file(stdatomic.h HAVE_STDATOMIC_H)
if(NOT HAVE_STDATOMIC_H)
  if(MSVC)
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/compat/msvc")
  else()
    check_c_source_compiles("int main() { int v = 0; return __atomic_fetch_add(&v, 1, __ATOMIC_SEQ_CST); return 0; }" DAV1D_COMPILER_SUPPORTS_GCC_STYLE_ATOMICS)
    if(DAV1D_COMPILER_SUPPORTS_GCC_STYLE_ATOMICS)
      include_directories("${CMAKE_CURRENT_SOURCE_DIR}/include/compat/gcc")
    else()
      message(FATAL_ERROR "Atomics not supported")
    endif()
  endif()
endif()

list(APPEND config_variables HAVE_UNISTD_H HAVE_IO_H HAVE_PTHREAD_NP_H)
list(APPEND config_variables HAVE_ALIGNED_MALLOC HAVE_POSIX_MEMALIGN HAVE_MEMALIGN)
list(APPEND config_variables HAVE_PTHREAD_GETAFFINITY_NP HAVE_PTHREAD_SETAFFINITY_NP)
if(WIN32)
  check_include_file(io.h HAVE_IO_H)
  check_symbol_exists(_aligned_malloc "malloc.h" HAVE_ALIGNED_MALLOC)
else()
  check_include_file(unistd.h HAVE_UNISTD_H)
  check_include_file(pthread_np.h HAVE_PTHREAD_NP_H)
  set(pthread_np_headers "pthread.h")
  if(HAVE_PTHREAD_NP_H)
    list(APPEND pthread_np_headers "pthread_np.h")
  endif()
  check_symbol_exists(pthread_getaffinity_np "${pthread_np_headers}" HAVE_PTHREAD_GETAFFINITY_NP)
  check_symbol_exists(pthread_setaffinity_np "${pthread_np_headers}" HAVE_PTHREAD_SETAFFINITY_NP)
  check_symbol_exists(memalign "malloc.h" HAVE_MEMALIGN)
  check_symbol_exists(posix_memalign "stdlib.h" HAVE_POSIX_MEMALIGN)
endif()

list(APPEND config01_variables ENDIANNESS_BIG)
test_big_endian(ENDIANNESS_BIG)

function(add_compile_option_if_supported)
  cmake_push_check_state(RESET)
  foreach(opt IN LISTS ARGV)
    check_c_compiler_flag("${opt}" COMPILER_SUPPORTS_${opt})
    if(COMPILER_SUPPORTS_${opt})
      add_compile_options(${opt})
    endif()
  endforeach()
  cmake_pop_check_state()
endfunction()
if(MSVC)
  add_compile_option_if_supported(
    -wd4028   # parameter different from declaration
    -wd4090   # broken with arrays of pointers
    -wd4996   # use of POSIX functions
  )
else()
  add_compile_option_if_supported(
    -Wundef
    -Werror=vla
    -Wno-maybe-uninitialized
    -Wno-missing-field-initializers
    -Wno-unused-parameter
    -Wstrict-prototypes
    -Werror=missing-prototypes
    -Wshorten-64-to-32
  )
endif()

set(cfg_h)
foreach(cfg01 IN LISTS config01_variables)
  string(APPEND cfg_h "#cmakedefine01 ${cfg01}\n")
endforeach()
foreach(cfg IN LISTS config_variables_value)
  string(APPEND cfg_h "#cmakedefine ${cfg} @${cfg}@\n")
endforeach()
foreach(cfg IN LISTS config_variables)
  string(APPEND cfg_h "#cmakedefine ${cfg}\n")
endforeach()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/config.h.in" "${cfg_h}")
configure_file("${CMAKE_CURRENT_BINARY_DIR}/config.h.in"
  "${CMAKE_CURRENT_BINARY_DIR}/include/config.h"
)
fiLE(READ "${CMAKE_CURRENT_BINARY_DIR}/include/config.h" config_h)
string(REGEX REPLACE "#define" "%define" config_asm "${config_h}")
string(REGEX REPLACE "/[*]([ a-zA-Z0-9_#]+)[*]/" ";\\1" config_asm "${config_asm}")
if(DAV1D_CPU_X86 OR DAV1D_CPU_X64)
  string(APPEND config_asm "%define private_prefix dav1d\n")
  string(APPEND config_asm "%define PIC 1\n")
  if(DAV1D_WITH_AVX)
    string(APPEND config_asm "%define FORCE_VEX_ENCODING 1\n")
  endif()
endif()
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/include/config.asm" "${config_asm}")
if(DAV1D_ARM_ASM_PIC)
  file(APPEND "${CMAKE_CURRENT_BINARY_DIR}/include/config.h" "#define PIC 3\n")
endif()
configure_file(include/dav1d/version.h.in include/dav1d/version.h)
configure_file(include/vcs_version.h.in include/vcs_version.h)

function(dav1d_add_include_directories TARGET)
  target_include_directories(${TARGET}
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}"
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include/dav1d>"
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include/dav1d>"
    PUBLIC "$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>"
  )
endfunction()

set(dav1d_bitdepth_sources
  src/cdef_apply_tmpl.c
  src/cdef_tmpl.c
  src/fg_apply_tmpl.c
  src/filmgrain_tmpl.c
  src/ipred_prepare_tmpl.c
  src/ipred_tmpl.c
  src/itx_tmpl.c
  src/lf_apply_tmpl.c
  src/loopfilter_tmpl.c
  src/looprestoration_tmpl.c
  src/lr_apply_tmpl.c
  src/mc_tmpl.c
  src/recon_tmpl.c
)
if(DAV!D_ASM AND DAV1D_CPU_PPC64)
  list(APPEND dav1d_bitdepth_sources
    src/ppc/cdef_tmpl.c
    src/ppc/looprestoration_tmpl.c
  )
endif()
set(dav1d_depth_objects)
foreach(depth IN LISTS DAV1D_BITDEPTHS)
  add_library(dav1d_depth_${depth} OBJECT ${dav1d_bitdepth_sources})
  dav1d_add_include_directories(dav1d_depth_${depth})
  target_compile_definitions(dav1d_depth_${depth} PRIVATE BITDEPTH=${depth})
  list(APPEND dav1d_depth_objects $<TARGET_OBJECTS:dav1d_depth_${depth}>)
endforeach()

add_library(dav1d
  src/cdf.c
  src/cpu.c
  src/data.c
  src/decode.c
  src/dequant_tables.c
  src/getbits.c
  src/intra_edge.c
  src/itx_1d.c
  src/lf_mask.c
  src/lib.c
  src/log.c
  src/mem.c
  src/msac.c
  src/obu.c
  src/picture.c
  src/qm.c
  src/ref.c
  src/refmvs.c
  src/scan.c
  src/tables.c
  src/thread_task.c
  src/warpmv.c
  src/wedge.c
  ${dav1d_depth_objects}
)
if(WIN32)
  target_sources(dav1d PRIVATE src/win32/thread.c)
else()
  find_package(Threads)
  if(Threads_FOUND)
    target_link_libraries(dav1d PRIVATE Threads::Threads)
  endif()
endif()
dav1d_add_include_directories(dav1d)
set_property(TARGET dav1d PROPERTY VERSION "${dav1d_soversion}")
set_property(TARGET dav1d PROPERTY SOVERSION "${dav1d_soversion_major}")
set_property(TARGET dav1d PROPERTY DEFINE_SYMBOL "DAV1D_BUILDING_DLL")

if(APPLE)
  target_link_options(dav1d PRIVATE "-Wl,-undefined,error")
elseif(NOT CMAKE_SYSTEM_NAME MATCHES "kOpenBSD.*|OpenBSD.*")
  check_linker_flag(C "-Wl,--no-undefined" LINKER_SUPPORTS_NO_UNDEFINED)
  if(LINKER_SUPPORTS_NO_UNDEFINED)
    target_link_options(dav1d PRIVATE "-Wl,--no-undefined")
  endif()
endif()

if(DAV1D_ASM)
  if(DAV1D_CPU_ARM64)
    target_sources(dav1d PRIVATE src/arm/cpu.c)
    target_sources(dav1d PRIVATE
      src/arm/64/itx.S
      src/arm/64/looprestoration_common.S
      src/arm/64/msac.S
      src/arm/64/refmvs.S
    )
    if(DAV1D_BITDEPTH_8)
      target_sources(dav1d PRIVATE
        src/arm/64/cdef.S
        src/arm/64/filmgrain.S
        src/arm/64/ipred.S
        src/arm/64/loopfilter.S
        src/arm/64/looprestoration.S
        src/arm/64/mc.S
      )
    endif()
    if(DAV1D_BITDEPTH_16)
      target_sources(dav1d PRIVATE
        src/arm/64/cdef16.S
        src/arm/64/filmgrain16.S
        src/arm/64/ipred16.S
        src/arm/64/itx16.S
        src/arm/64/loopfilter16.S
        src/arm/64/looprestoration16.S
        src/arm/64/mc16.S
      )
    endif()
  elseif(DAV1D_CPU_ARM32)
    target_sources(dav1d PRIVATE src/arm/cpu.c)
    target_sources(dav1d PRIVATE
      src/arm/32/itx.S
      src/arm/32/looprestoration_common.S
      src/arm/32/msac.S
      src/arm/32/refmvs.S
    )
    if(DAV1D_BITDEPTH_8)
      target_sources(dav1d PRIVATE
        src/arm/32/cdef.S
        src/arm/32/filmgrain.S
        src/arm/32/ipred.S
        src/arm/32/loopfilter.S
        src/arm/32/looprestoration.S
        src/arm/32/mc.S
      )
    endif()
    if(DAV1D_BITDEPTH_16)
      target_sources(dav1d PRIVATE
        src/arm/32/cdef16.S
        src/arm/32/filmgrain16.S
        src/arm/32/ipred16.S
        src/arm/32/itx16.S
        src/arm/32/loopfilter16.S
        src/arm/32/looprestoration16.S
        src/arm/32/mc16.S
      )
    endif()
  elseif(DAV1D_CPU_X86 OR DAV1D_CPU_X64)
    target_sources(dav1d PRIVATE
      src/x86/cpu.c
      src/x86/cpuid.asm
      src/x86/msac.asm
      src/x86/refmvs.asm
      src/x86/itx_avx512.asm
      src/x86/cdef_avx2.asm
      src/x86/itx_avx2.asm
      src/x86/looprestoration_avx2.asm
      src/x86/cdef_sse.asm
      src/x86/itx_sse.asm
    )
    if(DAV1D_BITDEPTH_8)
      target_sources(dav1d PRIVATE
        src/x86/cdef_avx512.asm
        src/x86/filmgrain_avx512.asm
        src/x86/ipred_avx512.asm
        src/x86/loopfilter_avx512.asm
        src/x86/looprestoration_avx512.asm
        src/x86/mc_avx512.asm
        src/x86/filmgrain_avx2.asm
        src/x86/ipred_avx2.asm
        src/x86/loopfilter_avx2.asm
        src/x86/mc_avx2.asm
        src/x86/filmgrain_sse.asm
        src/x86/ipred_sse.asm
        src/x86/loopfilter_sse.asm
        src/x86/looprestoration_sse.asm
        src/x86/mc_sse.asm
      )
    endif()
    if(DAV1D_BITDEPTH_16)
      target_sources(dav1d PRIVATE
        src/x86/cdef16_avx512.asm
        src/x86/filmgrain16_avx512.asm
        src/x86/ipred16_avx512.asm
        src/x86/itx16_avx512.asm
        src/x86/loopfilter16_avx512.asm
        src/x86/looprestoration16_avx512.asm
        src/x86/mc16_avx512.asm
        src/x86/cdef16_avx2.asm
        src/x86/filmgrain16_avx2.asm
        src/x86/ipred16_avx2.asm
        src/x86/itx16_avx2.asm
        src/x86/loopfilter16_avx2.asm
        src/x86/looprestoration16_avx2.asm
        src/x86/mc16_avx2.asm
        src/x86/cdef16_sse.asm
        src/x86/filmgrain16_sse.asm
        src/x86/ipred16_sse.asm
        src/x86/itx16_sse.asm
        src/x86/loopfilter16_sse.asm
        src/x86/looprestoration16_sse.asm
        src/x86/mc16_sse.asm
      )
    endif()
  elseif(DAV1D_CPU_PPC64)
    target_sources(dav1d PRIVATE src/ppc/cpu.c)
    target_link_options(dav1d INTERFACE -maltivec -mvsx)
  else()
    message(FATAL_ERROR "Unknown archivecture (configure with -DDAV1D_ASM=OFF)")
  endif()
endif()
