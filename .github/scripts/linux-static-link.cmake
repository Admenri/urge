# Included only by the release CI, after project() and before dependencies.
# Defer until the imported Rust target and the Ruby target have been created.
function(_urge_ci_static_link)
  execute_process(COMMAND "${CMAKE_C_COMPILER}" -print-file-name=libgcc_eh.a
    OUTPUT_VARIABLE _unwind OUTPUT_STRIP_TRAILING_WHITESPACE
    COMMAND_ERROR_IS_FATAL ANY)
  if(NOT IS_ABSOLUTE "${_unwind}" OR NOT EXISTS "${_unwind}")
    message(FATAL_ERROR "Linux CI requires the compiler's static unwinder")
  endif()
  get_target_property(_rust_libs wgpu_native-static INTERFACE_LINK_LIBRARIES)
  list(TRANSFORM _rust_libs REPLACE "^gcc_s$" "${_unwind}")
  set_property(TARGET wgpu_native-static PROPERTY INTERFACE_LINK_LIBRARIES "${_rust_libs}")

  # Ruby's platform probes use bare -lz, outside FindZLIB's imported target.
  get_target_property(_ruby_libs ruby-static INTERFACE_LINK_LIBRARIES)
  list(TRANSFORM _ruby_libs REPLACE "^z$" "${URGE_CI_STATIC_DEPS_PREFIX}/lib/libz.a")
  string(REPLACE "$<LINK_ONLY:z>"
    "$<LINK_ONLY:${URGE_CI_STATIC_DEPS_PREFIX}/lib/libz.a>" _ruby_libs "${_ruby_libs}")
  set_property(TARGET ruby-static PROPERTY INTERFACE_LINK_LIBRARIES "${_ruby_libs}")
endfunction()

cmake_language(DEFER CALL _urge_ci_static_link)
