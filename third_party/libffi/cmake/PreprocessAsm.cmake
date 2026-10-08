# ---------------------------------------------------------------------------
# Preprocess a MASM-syntax `.S` into a plain `.asm`.
#
#     cmake -DCL=<compiler> -DSRC=<in.S> -DOUT=<out.asm>
#           -DINCLUDES=<dir|dir|...> -DDEFINES=<name|name|...>
#           -P cmake/PreprocessAsm.cmake
#
# msvcc.sh does this with a shell redirect --
#
#     cl -nologo -EP $includes $defines $src > $outdir/$base.asm
#     ml64 -nologo -c $outdir/$base.asm
#
# -- because MSVC has no assembler that reads `.S`: the file is C-preprocessed
# (`#include <fficonfig.h>`, `#include <ffi.h>`, the `SEH()`/`C()`/`cfi_*`
# macros) and only then handed to ml64, which wants the expanded text.
#
# The redirect is the reason this is a script and not an `add_custom_command`:
# `cl /EP` writes the translation unit to stdout, and `execute_process`'s
# `OUTPUT_FILE` is the only way to capture it without a shell in the middle --
# which matters because the compiler path this tree uses has spaces in it
# (".../Microsoft Visual Studio/18/Community/...").  `cl /Fi` would name a file
# directly, but it is only defined for `/P`/`/E`, not `/EP`, and `/P` emits
# `#line` directives that ml64 does not understand.
#
# INCLUDES and DEFINES arrive `|`-separated: a `;`-separated value would be
# re-split by CMake's own `-D` parsing before this script ever sees it.
# ---------------------------------------------------------------------------

if(NOT CL OR NOT SRC OR NOT OUT)
  message(FATAL_ERROR "PreprocessAsm.cmake: CL, SRC and OUT are all required")
endif()

if(NOT EXISTS "${SRC}")
  message(FATAL_ERROR "PreprocessAsm.cmake: no such source ${SRC}")
endif()

string(REPLACE "|" ";" includes "${INCLUDES}")
string(REPLACE "|" ";" defines "${DEFINES}")

set(command "${CL}" /nologo /EP)
foreach(dir IN LISTS includes)
  if(dir)
    list(APPEND command "/I${dir}")
  endif()
endforeach()
foreach(def IN LISTS defines)
  if(def)
    list(APPEND command "/D${def}")
  endif()
endforeach()
list(APPEND command "${SRC}")

execute_process(
  COMMAND ${command}
  OUTPUT_FILE "${OUT}"
  ERROR_VARIABLE error
  RESULT_VARIABLE result
)

if(NOT result EQUAL 0)
  file(REMOVE "${OUT}")
  message(FATAL_ERROR
    "PreprocessAsm.cmake: ${CL} /EP failed on ${SRC} (${result})\n${error}")
endif()
