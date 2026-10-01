cmake_minimum_required(VERSION 3.30)

# ---------------------------------------------------------------------------
# Regenerate the binding glue from core/*.h, but only when it would change.
#
#     cmake -P bind/gen_binding.cmake
#
# Run from the top-level CMakeLists.txt at configure time, and by hand after a
# header change.  The pipeline is
#
#     core/*.h --gen_api_json.py--> api_reference.json --generate_binding.py--> binding_*.{h,cc}
#
# and every step is skipped unless its inputs really differ.  That matters for
# more than speed: `binding_*.cc` are compiled into `urge-binding`, and a file
# an IDE is told to "touch" every configure is a file the compiler will rebuild
# every build.  Nothing here writes a generated file whose content would not
# change.
#
# Two kinds of state, and the distinction is the whole point:
#
#   * the *inputs*  are `bind/.gen_stamp` -- the digest of the IR the headers
#     produce right now, plus an md5 per generator script dug out of the
#     repository so that editing a generator is noticed too.
#   * the *outputs* are `bind/.glue_stamp` -- the md5 of each `binding_*` that
#     was actually written, which is what lets the hook answer "on disk the glue
#     no longer corresponds to the IR" independently of the inputs.
#
# A stale IR is *not* compared by digest alone: the file on disk is read again
# with `generated_at` removed (`gen_api_json.py --digest` does exactly that to
# the document it would write), so a hand-edited or otherwise out-of-date IR is
# caught even though the headers did not move.
#
# This script runs in CMake's script mode, which means no project is loaded and
# nothing is implicit.  Nothing here globs: the input identity is the content
# digest of the headers, and the output set comes from the `binding_*` files
# that are actually on disk -- neither needs a file pattern, and script mode's
# `file(GLOB <relative>)` silently matches nothing anyway.
# ---------------------------------------------------------------------------

set(BIND_DIR "${CMAKE_CURRENT_LIST_DIR}")
get_filename_component(REPO_ROOT "${BIND_DIR}" DIRECTORY)

set(CORE_DIR "${REPO_ROOT}/core")
set(IR_FILE "${BIND_DIR}/api_reference.json")
set(STAMP_FILE "${BIND_DIR}/.gen_stamp")
set(GLUE_STAMP_FILE "${BIND_DIR}/.glue_stamp")

set(GEN_IR_SCRIPT "${BIND_DIR}/gen_api_json.py")
set(GEN_GLUE_SCRIPT "${BIND_DIR}/generate_binding.py")

# ---------------------------------------------------------------------------
# Interpreter discovery: build GEN_PY
# ---------------------------------------------------------------------------

find_program(GEN_PY NAMES python python3 py)
if(NOT GEN_PY)
  message(FATAL_ERROR
    "gen_binding.cmake: no Python interpreter on PATH; the binding glue in "
    "${BIND_DIR} cannot be generated.")
endif()

# ---------------------------------------------------------------------------
# Input digest helpers
# ---------------------------------------------------------------------------

# md5 of the generator scripts.  They are inputs of the pipeline just as the
# headers are: a changed generator has to invalidate the glue it produces, or
# the change silently does nothing until someone remembers to delete the IR.
function(script_digest out path)
  if(NOT EXISTS "${path}")
    message(FATAL_ERROR "gen_binding.cmake: missing generator ${path}")
  endif()
  file(MD5 "${path}" digest)
  set(${out} "${digest}" PARENT_SCOPE)
endfunction()

# Digest of the IR the current headers would produce.  Runs the generator in
# its side-effect-free mode and captures stdout; a failure here is reported
# instead of being let through, because a stamp that cannot be computed would
# otherwise look like "no change" on the next configure.
function(current_ir_digest out)
  execute_process(
    COMMAND "${GEN_PY}" "${GEN_IR_SCRIPT}" --digest
    OUTPUT_VARIABLE digest
    ERROR_VARIABLE error
    RESULT_VARIABLE result
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR
      "gen_binding.cmake: could not read the API digest from the headers.\n${error}")
  endif()
  string(STRIP "${digest}" digest)
  if(digest STREQUAL "")
    message(FATAL_ERROR "gen_binding.cmake: the API digest came back empty.")
  endif()
  set(${out} "${digest}" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Step 1 -- the IR
# ---------------------------------------------------------------------------

script_digest(GEN_IR_MD5 "${GEN_IR_SCRIPT}")
script_digest(GEN_GLUE_MD5 "${GEN_GLUE_SCRIPT}")
set(SCRIPT_PAIR "ir:${GEN_IR_MD5};glue:${GEN_GLUE_MD5}")

current_ir_digest(HEADER_DIGEST)
set(CURRENT_INPUT "${HEADER_DIGEST}+${SCRIPT_PAIR}")

set(PREVIOUS_INPUT "")
if(EXISTS "${STAMP_FILE}")
  file(READ "${STAMP_FILE}" PREVIOUS_INPUT)
  string(STRIP "${PREVIOUS_INPUT}" PREVIOUS_INPUT)
endif()

set(STAMP_OUTDATED FALSE)
if(NOT EXISTS "${IR_FILE}")
  message(STATUS "bind: api_reference.json is absent")
  set(STAMP_OUTDATED TRUE)
elseif(NOT PREVIOUS_INPUT STREQUAL CURRENT_INPUT)
  message(STATUS "bind: core headers or a generator changed since the last run")
  set(STAMP_OUTDATED TRUE)
endif()

# Even with fresh inputs, the IR on disk may not be the one the headers
# describe -- it was edited, or it came from a tree that never ran this script.
# Compare content, `generated_at` excluded, by asking the generator for the
# document it would write and digesting the on-disk one the same way.
if(NOT STAMP_OUTDATED)
  execute_process(
    COMMAND "${GEN_PY}" "${GEN_IR_SCRIPT}" --digest "${IR_FILE}"
    OUTPUT_VARIABLE IR_ON_DISK_DIGEST
    ERROR_VARIABLE error
    RESULT_VARIABLE result
  )
  if(result EQUAL 0)
    string(STRIP "${IR_ON_DISK_DIGEST}" IR_ON_DISK_DIGEST)
  else()
    # No --digest <file> mode: fall back to the md5 of the file as-is.  It
    # differs whenever `generated_at` does, which errs towards regenerating.
    unset(IR_ON_DISK_DIGEST)
    file(MD5 "${IR_FILE}" IR_ON_DISK_DIGEST)
  endif()
  set(EXPECTED_IR_DIGEST "${HEADER_DIGEST}")
  if(NOT IR_ON_DISK_DIGEST STREQUAL EXPECTED_IR_DIGEST)
    message(STATUS "bind: api_reference.json is stale against the headers")
    set(STAMP_OUTDATED TRUE)
  endif()
endif()

set(IR_REGENERATED FALSE)
if(STAMP_OUTDATED)
  execute_process(
    COMMAND "${GEN_PY}" "${GEN_IR_SCRIPT}"
    RESULT_VARIABLE result
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "gen_binding.cmake: gen_api_json.py failed (${result})")
  endif()
  set(IR_REGENERATED TRUE)
endif()

# ---------------------------------------------------------------------------
# Step 2 -- the glue
# ---------------------------------------------------------------------------

file(GLOB GLUE_OUTPUTS "${BIND_DIR}/binding_*.h" "${BIND_DIR}/binding_*.cc")
list(SORT GLUE_OUTPUTS)

# md5 of every generated glue file, in a stable order, plus the IR it came from.
set(GLUE_SIGNATURE "ir:${HEADER_DIGEST}\n")
foreach(glue IN LISTS GLUE_OUTPUTS)
  file(MD5 "${glue}" digest)
  get_filename_component(name "${glue}" NAME)
  string(APPEND GLUE_SIGNATURE "${name} ${digest}\n")
endforeach()

set(PREVIOUS_GLUE "")
if(EXISTS "${GLUE_STAMP_FILE}")
  file(READ "${GLUE_STAMP_FILE}" PREVIOUS_GLUE)
endif()

set(GLUE_OUTDATED FALSE)
if(NOT GLUE_OUTPUTS)
  message(STATUS "bind: no binding_* generated yet")
  set(GLUE_OUTDATED TRUE)
elseif(NOT PREVIOUS_GLUE STREQUAL GLUE_SIGNATURE)
  message(STATUS "bind: the glue no longer matches the IR it was generated from")
  set(GLUE_OUTDATED TRUE)
elseif(IR_REGENERATED)
  message(STATUS "bind: the IR was rewritten, refreshing the glue")
  set(GLUE_OUTDATED TRUE)
endif()

if(GLUE_OUTDATED)
  execute_process(
    COMMAND "${GEN_PY}" "${GEN_GLUE_SCRIPT}"
    RESULT_VARIABLE result
  )
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "gen_binding.cmake: generate_binding.py failed (${result})")
  endif()

  file(GLOB GLUE_OUTPUTS "${BIND_DIR}/binding_*.h" "${BIND_DIR}/binding_*.cc")
  list(SORT GLUE_OUTPUTS)
  set(GLUE_SIGNATURE "ir:${HEADER_DIGEST}\n")
  foreach(glue IN LISTS GLUE_OUTPUTS)
    file(MD5 "${glue}" digest)
    get_filename_component(name "${glue}" NAME)
    string(APPEND GLUE_SIGNATURE "${name} ${digest}\n")
  endforeach()
  file(WRITE "${GLUE_STAMP_FILE}" "${GLUE_SIGNATURE}")
else()
  message(STATUS "bind: glue up to date (${HEADER_DIGEST})")
endif()

# The input stamp is written last: it means "this digest was carried through to
# the glue", so a crash between the two steps leaves the work to be redone
# rather than recorded as done.
file(WRITE "${STAMP_FILE}" "${CURRENT_INPUT}")
