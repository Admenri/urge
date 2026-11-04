# RGSS3's native encoding modules, from the matching Ruby release. The
# generated conversion tables are checked in, so cross builds need no Ruby
# interpreter, generator, iconv, JNI or platform code-page dependencies.
option(RUBY_STATIC_RGSS_ENCODINGS
  "Statically link RGSS3's single-byte, UTF-16/32 and Shift_JIS codecs" ON)

set(RUBY_ENCDB_SOURCE "${RUBY_SOURCE_DIR}/cmake/encdb.h")
if(NOT RUBY_STATIC_RGSS_ENCODINGS)
  return()
endif()
set(RUBY_ENCDB_SOURCE "${RUBY_SOURCE_DIR}/cmake/encdb_rgss.h")
set(_ruby_extra_encodings
  shift_jis
  windows_31j
  utf_16be
  utf_16le
  utf_32be
  utf_32le
  iso_8859_1
  iso_8859_2
  iso_8859_3
  iso_8859_4
  iso_8859_5
  iso_8859_6
  iso_8859_7
  iso_8859_8
  iso_8859_9
  iso_8859_10
  iso_8859_11
  iso_8859_13
  iso_8859_14
  iso_8859_15
  iso_8859_16
  windows_1250
  windows_1251
  windows_1252
  windows_1253
  windows_1254
  windows_1257
  koi8_r
  koi8_u
)
set(_ruby_encoding_initializers "")
foreach(_name IN LISTS _ruby_extra_encodings)
  set(_source "${RUBY_SOURCE_DIR}/enc/${_name}.c")
  list(APPEND RUBY_CORE_SOURCES "${_source}")
  set_source_files_properties("${_source}" PROPERTIES
    COMPILE_DEFINITIONS "ONIG_ENC_REGISTER=rb_enc_register")
  string(APPEND _ruby_encoding_initializers "RUBY_INIT_ENCODING(${_name})\n")
endforeach()
# Keep initialization and the compile list in sync without a host generator.
file(CONFIGURE OUTPUT "${RUBY_BINARY_DIR}/encinit_rgss.h"
  CONTENT "${_ruby_encoding_initializers}" @ONLY)
foreach(_name single_byte utf_16_32 japanese_sjis)
  set(_source "${RUBY_SOURCE_DIR}/enc/trans/${_name}.c")
  list(APPEND RUBY_CORE_SOURCES "${_source}")
  set_source_files_properties("${_source}" PROPERTIES
    COMPILE_DEFINITIONS "EXTSTATIC=1")
endforeach()
set_source_files_properties("${RUBY_SOURCE_DIR}/cmake/encinit_builtin.c"
  PROPERTIES COMPILE_DEFINITIONS "RUBY_STATIC_RGSS_ENCODINGS=1")
unset(_ruby_extra_encodings)
unset(_ruby_encoding_initializers)
unset(_name)
unset(_source)
