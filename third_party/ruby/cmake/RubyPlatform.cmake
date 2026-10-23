# ---------------------------------------------------------------------------
# RubyPlatform.cmake -- platform naming, compiler and linker helpers
#
# Kept free of any hard-coded OS assumptions: everything that differs between
# systems is decided by feature tests in RubyUnix.cmake / RubyMSVC.cmake.
# ---------------------------------------------------------------------------

# --- pointer size ----------------------------------------------------------
# Some generators (notably Visual Studio) do not populate CMAKE_SIZEOF_VOIDP,
# so determine it with a compile test when it is missing.
if(NOT CMAKE_SIZEOF_VOIDP)
  include(CheckCSourceCompiles)
  check_c_source_compiles(
    "int main(void){char a[sizeof(void*)==8?1:-1];(void)a;return 0;}"
    RUBY_CV_POINTER_IS_64BIT)
  if(RUBY_CV_POINTER_IS_64BIT)
    set(CMAKE_SIZEOF_VOIDP 8)
  else()
    set(CMAKE_SIZEOF_VOIDP 4)
  endif()
endif()
set(RUBY_SIZEOF_VOIDP "${CMAKE_SIZEOF_VOIDP}")

# --- Ruby style architecture name ------------------------------------------
# Ruby calls this "arch"; it shows up in RbConfig and in lib/ruby/<ver>/<arch>.
set(_ruby_proc "${CMAKE_SYSTEM_PROCESSOR}")
string(TOLOWER "${_ruby_proc}" _ruby_proc_l)

if(MSVC)
  if(CMAKE_SIZEOF_VOIDP EQUAL 8)
    set(RUBY_ARCH "x64-mswin64")
  else()
    set(RUBY_ARCH "x86-mswin32")
  endif()
elseif(WIN32)
  if(CMAKE_SIZEOF_VOIDP EQUAL 8)
    set(RUBY_ARCH "x64-mingw-ucrt")
  else()
    set(RUBY_ARCH "i386-mingw32")
  endif()
elseif(APPLE)
  # Ruby uses e.g. "arm64-darwin23"; CMAKE_SYSTEM_VERSION is the Darwin kernel
  # version (x.y.z), whose major component is what Ruby appends.
  string(REGEX MATCH "^[0-9]+" _ruby_darwin_major "${CMAKE_SYSTEM_VERSION}")
  if(_ruby_darwin_major STREQUAL "")
    set(_ruby_darwin_major "0")
  endif()
  if(_ruby_proc_l MATCHES "^(aarch64|arm64)$")
    set(_ruby_proc_l "arm64")
  endif()
  set(RUBY_ARCH "${_ruby_proc_l}-darwin${_ruby_darwin_major}")
elseif(EMSCRIPTEN)
  set(RUBY_ARCH "wasm32-unknown-emscripten")
else()
  # Generic unix: <cpu>-<os>.  This is exactly what configure does for an
  # unrecognised target, so an unknown OS still gets a sane arch string.
  string(TOLOWER "${CMAKE_SYSTEM_NAME}" _ruby_os)
  set(RUBY_ARCH "${_ruby_proc_l}-${_ruby_os}")
endif()

message(STATUS "Ruby arch: ${RUBY_ARCH}")

# --- compiler flags --------------------------------------------------------
# Only flags that are known to be harmless are added.  Feature specific
# behaviour (visibility, attributes, ...) is handled through config.h so that
# unknown compilers still work with the defaults.
function(ruby_apply_compile_options tgt)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang|IntelLLVM")
    target_compile_options(${tgt} PRIVATE
      -fno-strict-overflow
      -fvisibility=hidden)
    if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
      target_compile_options(${tgt} PRIVATE -fexcess-precision=standard)
    endif()
    # Ruby's own build uses these; they are supported by every GCC/Clang.
    target_compile_options(${tgt} PRIVATE
      -Wall
      -Wno-unused-parameter
      -Wno-parentheses
      -Wno-missing-field-initializers
      -Wno-implicit-fallthrough
      -Wno-array-bounds
      -Wno-alloc-size-larger-than)
  elseif(MSVC)
    target_compile_options(${tgt} PRIVATE /W3 /wd4996 /wd4100 /wd4244 /wd4267)
    # Ruby sources assume UTF-8 source encoding.
    target_compile_options(${tgt} PRIVATE /utf-8)
  endif()

  if(NOT MSVC)
    # A position independent executable, as in the original build.
    set_target_properties(${tgt} PROPERTIES POSITION_INDEPENDENT_CODE ON)
  endif()
endfunction()

# --- link libraries --------------------------------------------------------
# RUBY_LINK_LIBS is filled in by the platform module.
function(ruby_link_libraries tgt)
  if(RUBY_LINK_LIBS)
    target_link_libraries(${tgt} PRIVATE ${RUBY_LINK_LIBS})
  endif()
  if(UNIX AND NOT APPLE)
    # Let the interpreter export its symbols to C extensions.
    target_link_options(${tgt} PRIVATE "-rdynamic")
  endif()
endfunction()
