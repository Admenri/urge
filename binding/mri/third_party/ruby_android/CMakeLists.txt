cmake_minimum_required(VERSION 3.18.0)

# Include directories
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include ${CMAKE_CURRENT_SOURCE_DIR})

# Source files
set(MRI_SOURCE_FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/main.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dmydln.c
    ${CMAKE_CURRENT_SOURCE_DIR}/miniinit.c
    ${CMAKE_CURRENT_SOURCE_DIR}/miniprelude.c
    ${CMAKE_CURRENT_SOURCE_DIR}/array.c
    ${CMAKE_CURRENT_SOURCE_DIR}/bignum.c
    ${CMAKE_CURRENT_SOURCE_DIR}/class.c
    ${CMAKE_CURRENT_SOURCE_DIR}/compar.c
    ${CMAKE_CURRENT_SOURCE_DIR}/complex.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dir.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dln_find.c
    ${CMAKE_CURRENT_SOURCE_DIR}/encoding.c
    ${CMAKE_CURRENT_SOURCE_DIR}/enum.c
    ${CMAKE_CURRENT_SOURCE_DIR}/enumerator.c
    ${CMAKE_CURRENT_SOURCE_DIR}/error.c
    ${CMAKE_CURRENT_SOURCE_DIR}/eval.c
    ${CMAKE_CURRENT_SOURCE_DIR}/load.c
    ${CMAKE_CURRENT_SOURCE_DIR}/proc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/file.c
    ${CMAKE_CURRENT_SOURCE_DIR}/gc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/hash.c
    ${CMAKE_CURRENT_SOURCE_DIR}/inits.c
    ${CMAKE_CURRENT_SOURCE_DIR}/io.c
    ${CMAKE_CURRENT_SOURCE_DIR}/marshal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/math.c
    ${CMAKE_CURRENT_SOURCE_DIR}/node.c
    ${CMAKE_CURRENT_SOURCE_DIR}/numeric.c
    ${CMAKE_CURRENT_SOURCE_DIR}/object.c
    ${CMAKE_CURRENT_SOURCE_DIR}/pack.c
    ${CMAKE_CURRENT_SOURCE_DIR}/parse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/process.c
    ${CMAKE_CURRENT_SOURCE_DIR}/random.c
    ${CMAKE_CURRENT_SOURCE_DIR}/range.c
    ${CMAKE_CURRENT_SOURCE_DIR}/rational.c
    ${CMAKE_CURRENT_SOURCE_DIR}/re.c
    ${CMAKE_CURRENT_SOURCE_DIR}/regcomp.c
    ${CMAKE_CURRENT_SOURCE_DIR}/regenc.c
    ${CMAKE_CURRENT_SOURCE_DIR}/regerror.c
    ${CMAKE_CURRENT_SOURCE_DIR}/regexec.c
    ${CMAKE_CURRENT_SOURCE_DIR}/regparse.c
    ${CMAKE_CURRENT_SOURCE_DIR}/regsyntax.c
    ${CMAKE_CURRENT_SOURCE_DIR}/ruby.c
    ${CMAKE_CURRENT_SOURCE_DIR}/safe.c
    ${CMAKE_CURRENT_SOURCE_DIR}/signal.c
    ${CMAKE_CURRENT_SOURCE_DIR}/sprintf.c
    ${CMAKE_CURRENT_SOURCE_DIR}/st.c
    ${CMAKE_CURRENT_SOURCE_DIR}/strftime.c
    ${CMAKE_CURRENT_SOURCE_DIR}/string.c
    ${CMAKE_CURRENT_SOURCE_DIR}/struct.c
    ${CMAKE_CURRENT_SOURCE_DIR}/symbol.c
    ${CMAKE_CURRENT_SOURCE_DIR}/time.c
    ${CMAKE_CURRENT_SOURCE_DIR}/transcode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/util.c
    ${CMAKE_CURRENT_SOURCE_DIR}/variable.c
    ${CMAKE_CURRENT_SOURCE_DIR}/version.c
    ${CMAKE_CURRENT_SOURCE_DIR}/compile.c
    ${CMAKE_CURRENT_SOURCE_DIR}/debug.c
    ${CMAKE_CURRENT_SOURCE_DIR}/iseq.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vm_dump.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vm_backtrace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/vm_trace.c
    ${CMAKE_CURRENT_SOURCE_DIR}/thread.c
    ${CMAKE_CURRENT_SOURCE_DIR}/cont.c
    ${CMAKE_CURRENT_SOURCE_DIR}/enc/ascii.c
    ${CMAKE_CURRENT_SOURCE_DIR}/enc/us_ascii.c
    ${CMAKE_CURRENT_SOURCE_DIR}/enc/unicode.c
    ${CMAKE_CURRENT_SOURCE_DIR}/enc/utf_8.c
    ${CMAKE_CURRENT_SOURCE_DIR}/newline.c
    ${CMAKE_CURRENT_SOURCE_DIR}/missing/setproctitle.c
    ${CMAKE_CURRENT_SOURCE_DIR}/missing/strlcat.c
    ${CMAKE_CURRENT_SOURCE_DIR}/missing/strlcpy.c
    ${CMAKE_CURRENT_SOURCE_DIR}/missing/crypt.c
    ${CMAKE_CURRENT_SOURCE_DIR}/addr2line.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dmyext.c
    ${CMAKE_CURRENT_SOURCE_DIR}/dmyenc.c
)

# Define the library to be built and its sources
add_library(mri_ruby SHARED ${MRI_SOURCE_FILES})

# Conditional flags based on target architecture
if(${CMAKE_SIZEOF_VOID_P} EQUAL 4)
  target_compile_options(mri_ruby PUBLIC -DARCH_32BIT -DRUBY_EXPORT -pthread)
  message("${ANDROID_ABI} ruby 32 bit build")
else()
  target_compile_options(mri_ruby PUBLIC -DARCH_64BIT -DRUBY_EXPORT -pthread)
  message("${ANDROID_ABI} ruby 64 bit build")
endif()
