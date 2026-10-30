# ---------------------------------------------------------------------------
# Profile-guided optimization.
#
# PGO is a two-pass build.  The first pass compiles the engine with
# instrumentation and links it so that the compiler records, for every branch
# and every call site it emitted, how often the program actually took it; the
# second pass feeds those counts back and re-emits the code around them -- hot
# blocks fall through and stay in the instruction cache, cold ones move out of
# line, and inlining and register allocation are decided on measured sizes
# instead of on a guess.
#
# It is a switch and not the default for two reasons.  The profile is tied to
# the toolchain that wrote it (an MSVC .pgd is not readable by another MSVC
# version, let alone by clang), and it is tied to the workload that produced it:
# a profile collected while a menu is open will lay the code out for the menu.
# And it only pays off once the code is already good -- PGO rearranges what the
# compiler emits, it cannot fix an algorithm.  Which is why the source-level
# profile in app/platform/profiler.cc is the first thing to read, and this is
# what to do afterwards.
#
#   cmake --preset win64-vendored-ruby -DURGE_PGO=generate
#   cmake --build --preset win64-vendored-ruby --target Game
#   ... run the game, then close it ...
#   cmake --preset win64-vendored-ruby -DURGE_PGO=use
#   cmake --build --preset win64-vendored-ruby --target Game
#
# Both modes compile identically -- the whole-program flag is on in both -- so
# switching from `generate` to `use` relinks and does not recompile.  Switching
# to `use` also merges whatever the instrumented run left behind; that is what
# cmake/pgo_merge.cmake does before the link, and the raw counters are deleted
# once they are merged, so a later relink cannot count the same run twice.
#
# `off` is the default, and it touches nothing: a build that never asks for PGO
# is byte for byte the build it was before this file existed.
#
# What is instrumented is the engine itself -- urge-core, urge-binding and the
# host.  third_party is left alone: it is upstream code, it is by far the
# largest part of the link, and laying Ruby, SDL and wgpu out for one game's
# workload is not this project's call to make.
# ---------------------------------------------------------------------------

set(URGE_PGO "off" CACHE STRING
  "Profile-guided optimization: off, generate (instrumented run) or use")
set_property(CACHE URGE_PGO PROPERTY STRINGS off generate use)

if(NOT URGE_PGO MATCHES "^(off|generate|use)$")
  message(FATAL_ERROR
    "URGE_PGO is '${URGE_PGO}'; it has to be off, generate or use.")
endif()

if(URGE_PGO STREQUAL "off")
  return()
endif()

set(URGE_PGO_DIR "${CMAKE_BINARY_DIR}/pgo" CACHE PATH
  "Where the instrumented run writes its profile")

# The linker writes the counters into this directory and does not create it; a
# path that does not exist surfaces as C1301 ("invalid format") from the code
# generator inside LTCG, which reads as a corrupt profile rather than a missing
# directory.
file(MAKE_DIRECTORY "${URGE_PGO_DIR}")

if(URGE_PGO STREQUAL "generate")
  message(STATUS "URGE: PGO = generate (instrumented run, profile in ${URGE_PGO_DIR})")
else()
  message(STATUS "URGE: PGO = use (profile from ${URGE_PGO_DIR})")
endif()

set(URGE_PGO_TARGETS urge-core urge-binding Game)

if(MSVC)
  # /GL keeps the compiler's intermediate representation in the object file
  # instead of machine code, which is what lets the second pass re-optimise
  # across translation units; /Gy puts every function in a section of its own,
  # without which a hot function cannot be moved away from the cold code it was
  # compiled next to.  Both are needed by both passes.
  foreach(target IN LISTS URGE_PGO_TARGETS)
    target_compile_options(${target} PRIVATE /GL /Gy)
  endforeach()

  set(URGE_PGO_PROFILE "${URGE_PGO_DIR}/Game.pgd")

  # `cl` and `link` take the path of the profile as part of the option itself,
  # not as a separate argument, so it cannot be quoted -- and a forward slash
  # is what CMake writes everywhere and what LNK1117 rejects.  Both the option
  # and the merge step get the native form.
  file(TO_NATIVE_PATH "${URGE_PGO_PROFILE}" URGE_PGO_PROFILE_NATIVE)
  file(TO_NATIVE_PATH "${URGE_PGO_DIR}" URGE_PGO_DIR_NATIVE)

  # pgomgr and the PGO runtime both ship beside cl.exe.
  get_filename_component(URGE_PGO_COMPILER_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)

  if(URGE_PGO STREQUAL "generate")
    # /GENPROFILE re-runs the code generator inside the linker, instrumented.
    # The counters are written to Game.pgc and folded into the .pgd named here.
    target_link_options(Game PRIVATE /LTCG "/GENPROFILE:PGD=${URGE_PGO_PROFILE_NATIVE}")

    # The instrumented build calls into the PGO runtime to write its counters,
    # and with the DLL runtime the engine is built with (/MD, which the
    # interpreter requires) that runtime is a DLL rather than a static library.
    # Without it beside the executable the process dies in the loader before a
    # single line of the engine runs, which reads as "the game does nothing".
    file(GLOB URGE_PGO_RUNTIME "${URGE_PGO_COMPILER_DIR}/pgort*.dll")
    if(URGE_PGO_RUNTIME)
      list(GET URGE_PGO_RUNTIME 0 URGE_PGO_RUNTIME)
      add_custom_command(TARGET Game POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${URGE_PGO_RUNTIME}" "$<TARGET_FILE_DIR:Game>"
        COMMENT "URGE: copying the PGO runtime beside the instrumented build"
        VERBATIM)
    else()
      message(WARNING
        "URGE: no pgort*.dll beside cl.exe; the instrumented Game.exe will not "
        "start unless the PGO runtime is on PATH.")
    endif()
  else()
    # The merge has to happen before the link that reads the profile, and it is
    # allowed to find nothing: a relink of an already-merged profile is the
    # normal case, not an error.
    find_program(URGE_PGOMGR NAMES pgomgr pgomgr.exe
      HINTS "${URGE_PGO_COMPILER_DIR}")
    if(NOT URGE_PGOMGR)
      message(FATAL_ERROR
        "URGE_PGO=use needs pgomgr.exe beside cl.exe (looked in "
        "'${URGE_PGO_COMPILER_DIR}'); it ships with the MSVC toolset.")
    endif()

    add_custom_command(TARGET Game PRE_LINK
      COMMAND ${CMAKE_COMMAND}
              "-DPGOMGR=${URGE_PGOMGR}"
              "-DPGO_DIR=${URGE_PGO_DIR_NATIVE}"
              "-DPGO_PROFILE=${URGE_PGO_PROFILE_NATIVE}"
              -P "${CMAKE_CURRENT_LIST_DIR}/pgo_merge.cmake"
      COMMENT "URGE: merging the PGO counters from the instrumented run"
      VERBATIM)

    target_link_options(Game PRIVATE /LTCG "/USEPROFILE:PGD=${URGE_PGO_PROFILE_NATIVE}")
  endif()
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  # clang's equivalent is an LLVM profile rather than a .pgd, and the merge is
  # `llvm-profdata` rather than `pgomgr`.  One difference matters on the web and
  # on Android: the profile is written into the program's own filesystem, so a
  # wasm or APK run leaves it somewhere that has to be brought back to the build
  # machine by hand before `use` can find it.
  set(URGE_PGO_PROFILE "${URGE_PGO_DIR}/Game.profdata")

  if(URGE_PGO STREQUAL "generate")
    foreach(target IN LISTS URGE_PGO_TARGETS)
      target_compile_options(${target} PRIVATE
        "-fprofile-instr-generate=${URGE_PGO_DIR}/%p.profraw")
    endforeach()
    target_link_options(Game PRIVATE
      "-fprofile-instr-generate=${URGE_PGO_DIR}/%p.profraw")
  else()
    find_program(URGE_LLVM_PROFDATA NAMES llvm-profdata llvm-profdata.exe
      HINTS "${URGE_PGO_COMPILER_DIR}")
    if(NOT URGE_LLVM_PROFDATA)
      message(FATAL_ERROR
        "URGE_PGO=use needs llvm-profdata on PATH or beside the compiler.")
    endif()

    add_custom_command(TARGET Game PRE_LINK
      COMMAND ${CMAKE_COMMAND}
              "-DPROFDATA=${URGE_LLVM_PROFDATA}"
              "-DPGO_DIR=${URGE_PGO_DIR}"
              "-DPGO_PROFILE=${URGE_PGO_PROFILE}"
              -P "${CMAKE_CURRENT_LIST_DIR}/pgo_merge.cmake"
      COMMENT "URGE: merging the PGO counters from the instrumented run"
      VERBATIM)

    foreach(target IN LISTS URGE_PGO_TARGETS)
      target_compile_options(${target} PRIVATE
        "-fprofile-instr-use=${URGE_PGO_PROFILE}")
    endforeach()
  endif()
else()
  message(FATAL_ERROR
    "URGE_PGO needs MSVC or clang; ${CMAKE_CXX_COMPILER_ID} is neither.")
endif()
