# ---------------------------------------------------------------------------
# Merges the counters an instrumented run left behind into the profile the
# `use` pass reads.  cmake/pgo.cmake runs this as a PRE_LINK step of the `use`
# build, which is the only moment the merged profile is needed.
#
# Inputs, all passed on the command line:
#
#   PGOMGR / PGO_DIR / PGO_PROFILE     the MSVC shape: .pgc files to merge
#   PROFDATA / PGO_DIR / PGO_PROFILE   the clang shape: .profraw files
#
# Finding nothing is not a failure.  Every relink of a `use` build comes through
# here, and only the first one -- the one after the instrumented run -- has
# anything to merge.  What is merged is then deleted: the counters of one run
# are additive, so merging the same file twice would double the weight of that
# run, and the second pass would lay the code out for a workload that never
# happened.
# ---------------------------------------------------------------------------

if(NOT DEFINED PGO_DIR OR NOT DEFINED PGO_PROFILE)
  message(FATAL_ERROR "pgo_merge.cmake needs PGO_DIR and PGO_PROFILE.")
endif()

if(PGOMGR)
  file(GLOB _raw "${PGO_DIR}/*.pgc")
  if(NOT _raw)
    message(STATUS "URGE: no new PGO counters to merge; using the profile as is")
    return()
  endif()

  execute_process(
    COMMAND "${PGOMGR}" /merge ${_raw} "${PGO_PROFILE}"
    RESULT_VARIABLE _result)
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR
      "pgomgr failed (${_result}) merging into '${PGO_PROFILE}'.")
  endif()

  list(LENGTH _raw _count)
  file(REMOVE ${_raw})
  message(STATUS "URGE: merged ${_count} PGO counter file(s) into ${PGO_PROFILE}")
  return()
endif()

if(PROFDATA)
  file(GLOB _raw "${PGO_DIR}/*.profraw")
  if(NOT _raw)
    message(STATUS "URGE: no new PGO counters to merge; using the profile as is")
    return()
  endif()

  execute_process(
    COMMAND "${PROFDATA}" merge -output="${PGO_PROFILE}" ${_raw}
    RESULT_VARIABLE _result)
  if(NOT _result EQUAL 0)
    message(FATAL_ERROR
      "llvm-profdata failed (${_result}) merging into '${PGO_PROFILE}'.")
  endif()

  list(LENGTH _raw _count)
  file(REMOVE ${_raw})
  message(STATUS "URGE: merged ${_count} PGO counter file(s) into ${PGO_PROFILE}")
  return()
endif()

message(FATAL_ERROR "pgo_merge.cmake needs PGOMGR or PROFDATA.")
