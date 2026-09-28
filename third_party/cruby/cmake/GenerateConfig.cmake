
# GenerateConfig.cmake
#
# Generates include/ruby/config.h for building Ruby 1.9.3 on an arbitrary
# POSIX-like target.
#
# Nothing in this file is tied to a specific platform: every capability
# (headers, type sizes, functions, struct layouts, compiler attributes, ABI
# details) is auto-detected with CMake's check_* helpers, and the generated
# config.h only enables what the build host actually provides.  Detection
# results are kept in normal variables so CMakeLists.txt (which includes this
# file at top level) can select missing/ sources and link libraries the same
# way.

include(CheckIncludeFile)
include(CheckTypeSize)
include(CheckSymbolExists)
include(CheckCSourceCompiles)
include(CheckCSourceRuns)
include(CheckLibraryExists)

function(_emit STR)
  string(APPEND HEADER_CONTENT "${STR}\n")
  set(HEADER_CONTENT "${HEADER_CONTENT}" PARENT_SCOPE)
endfunction()

function(_check_header HEADER_PATH MACRO_NAME)
  check_include_file(${HEADER_PATH} ${MACRO_NAME})
  if(${MACRO_NAME})
    string(APPEND HEADER_CONTENT "#define ${MACRO_NAME} 1\n")
  else()
    string(APPEND HEADER_CONTENT "/* #undef ${MACRO_NAME} */\n")
  endif()
  set(HEADER_CONTENT "${HEADER_CONTENT}" PARENT_SCOPE)
endfunction()

function(_check_sizeof TYPE_NAME MACRO_NAME)
  check_type_size(${TYPE_NAME} ${MACRO_NAME})
  # check_type_size leaves the variable empty when the type does not exist;
  # emit 0 in that case so `#if MACRO > 0` always has a valid operand.
  if("${${MACRO_NAME}}" STREQUAL "")
    set(_size_value 0)
  else()
    set(_size_value ${${MACRO_NAME}})
  endif()
  string(APPEND HEADER_CONTENT "#define ${MACRO_NAME} ${_size_value}\n")
  set(HEADER_CONTENT "${HEADER_CONTENT}" PARENT_SCOPE)
endfunction()

# Detect a function/macro declared in a set of headers.  Emits HAVE_<name>
# into config.h and leaves the result in the same-named variable so
# CMakeLists.txt can decide which missing/ sources to compile.
function(_detect_func SYMBOL HEADERS HAVE_MACRO)
  check_symbol_exists(${SYMBOL} "${HEADERS}" ${HAVE_MACRO})
  if(${HAVE_MACRO})
    string(APPEND HEADER_CONTENT "#define ${HAVE_MACRO} 1\n")
  else()
    string(APPEND HEADER_CONTENT "/* #undef ${HAVE_MACRO} */\n")
  endif()
  set(HEADER_CONTENT "${HEADER_CONTENT}" PARENT_SCOPE)
endfunction()

# Compile a snippet and emit a #define when it compiles.
function(_detect_compile CODE HAVE_MACRO)
  check_c_source_compiles("${CODE}" ${HAVE_MACRO})
  if(${HAVE_MACRO})
    string(APPEND HEADER_CONTENT "#define ${HAVE_MACRO} 1\n")
  else()
    string(APPEND HEADER_CONTENT "/* #undef ${HAVE_MACRO} */\n")
  endif()
  set(HEADER_CONTENT "${HEADER_CONTENT}" PARENT_SCOPE)
endfunction()

# Emit a bare #define when a size probe found the type.
function(_emit_have_if_sizeof SIZE_MACRO HAVE_MACRO)
  if(${SIZE_MACRO} GREATER 0)
    string(APPEND HEADER_CONTENT "#define ${HAVE_MACRO} 1\n")
  endif()
  set(HEADER_CONTENT "${HEADER_CONTENT}" PARENT_SCOPE)
endfunction()

# Emit the X2NUM/NUM2X/PRI_X_PREFIX conversion triple for a type alias, chosen
# by the detected size of the underlying C type.  A macro (not a function) so
# it runs in the caller's scope and _emit keeps working.
macro(_emit_conversions PREFIX SIZE_MACRO UNSIGNED)
  if(${SIZE_MACRO} EQUAL SIZEOF_INT)
    if(${UNSIGNED})
      _emit("#define ${PREFIX}2NUM(v) UINT2NUM(v)")
      _emit("#define NUM2${PREFIX}(v) NUM2UINT(v)")
    else()
      _emit("#define ${PREFIX}2NUM(v) INT2NUM(v)")
      _emit("#define NUM2${PREFIX}(v) NUM2INT(v)")
    endif()
    _emit("#define PRI_${PREFIX}_PREFIX PRI_INT_PREFIX")
  elseif(${SIZE_MACRO} EQUAL SIZEOF_LONG)
    if(${UNSIGNED})
      _emit("#define ${PREFIX}2NUM(v) ULONG2NUM(v)")
      _emit("#define NUM2${PREFIX}(v) NUM2ULONG(v)")
    else()
      _emit("#define ${PREFIX}2NUM(v) LONG2NUM(v)")
      _emit("#define NUM2${PREFIX}(v) NUM2LONG(v)")
    endif()
    _emit("#define PRI_${PREFIX}_PREFIX PRI_LONG_PREFIX")
  else()
    if(${UNSIGNED})
      _emit("#define ${PREFIX}2NUM(v) ULL2NUM(v)")
      _emit("#define NUM2${PREFIX}(v) NUM2ULL(v)")
    else()
      _emit("#define ${PREFIX}2NUM(v) LL2NUM(v)")
      _emit("#define NUM2${PREFIX}(v) NUM2LL(v)")
    endif()
    _emit("#define PRI_${PREFIX}_PREFIX PRI_LL_PREFIX")
  endif()
endmacro()

# Emit rb_<alias>_t <ctype> plus conversion macros only when the type exists.
# On Windows the POSIX id types (pid_t, uid_t, gid_t, mode_t, rlim_t) are
# absent from the CRT; Ruby's autoconf substitutes int for them (see
# RUBY_REPLACE_TYPE in configure.in), which is also what win32/Makefile.sub
# does (rb_pid_t int, rb_uid_t int, rb_gid_t int).  Emit the same fallback.
macro(_emit_alias ALIAS C_TYPE SIZE_MACRO UNSIGNED PREFIX)
  if(${SIZE_MACRO} GREATER 0)
    _emit("#define rb_${ALIAS}_t ${C_TYPE}")
    _emit_conversions(${PREFIX} ${SIZE_MACRO} ${UNSIGNED})
  elseif(WIN32)
    _emit("#define rb_${ALIAS}_t int")
    _emit_conversions(${PREFIX} SIZEOF_INT ${UNSIGNED})
  endif()
endmacro()

set(HEADER_CONTENT "")
_emit("#ifndef INCLUDE_RUBY_CONFIG_H")
_emit("#define INCLUDE_RUBY_CONFIG_H 1")

# Feature-test macros.  These are the same set autoconf's
# AC_USE_SYSTEM_EXTENSIONS enables: each macro is only meaningful on the
# systems that define it and is silently ignored everywhere else, so emitting
# all of them is safe on any target and exposes the full POSIX/GNU API where
# it exists.  They must be emitted before any system header is included.
foreach(_feature
    "__EXTENSIONS__ 1"
    "_ALL_SOURCE 1"
    "_GNU_SOURCE 1"
    "_POSIX_PTHREAD_SEMANTICS 1"
    "_TANDEM_SOURCE 1"
    "_DEFAULT_SOURCE 1"
    "_REENTRANT 1"
    "_THREAD_SAFE 1")
  _emit("#define ${_feature}")
endforeach()

# Let every check_* probe below see the same feature-test macros, and link
# libm so functions living there (acosh, cbrt, lgamma_r, ...) are detected.
set(CMAKE_REQUIRED_DEFINITIONS
  "-D__EXTENSIONS__" "-D_ALL_SOURCE" "-D_GNU_SOURCE"
  "-D_POSIX_PTHREAD_SEMANTICS" "-D_TANDEM_SOURCE" "-D_DEFAULT_SOURCE"
  "-D_REENTRANT" "-D_THREAD_SAFE")
# There is no libm on Windows - the math functions live in the CRT there, and
# forcing "m" made every probe fail at link time (LNK1104: cannot open m.lib).
if(NOT WIN32)
  set(CMAKE_REQUIRED_LIBRARIES "m")
endif()

_emit("#include <stdint.h>")
_emit("#include <setjmp.h>")

# ---- POSIX branch (GCC/Clang on Linux/BSD/macOS, ...) -----------------------
if(NOT WIN32)
_emit("// Header Check")

_emit("#define STDC_HEADERS 1")
_check_header("sys/types.h" HAVE_SYS_TYPES_H)
_check_header("sys/stat.h" HAVE_SYS_STAT_H)
_check_header("stdlib.h" HAVE_STDLIB_H)
_check_header("stddef.h" HAVE_STDDEF_H)
_check_header("string.h" HAVE_STRING_H)
_check_header("memory.h" HAVE_MEMORY_H)
_check_header("stdint.h" HAVE_STDINT_H)
_check_header("limits.h" HAVE_LIMITS_H)
_check_header("fcntl.h" HAVE_FCNTL_H)
_check_header("float.h" HAVE_FLOAT_H)
_check_header("time.h" HAVE_TIME_H)
_check_header("utime.h" HAVE_UTIME_H)
_check_header("sys/utime.h" HAVE_SYS_UTIME_H)
_check_header("sys/time.h" HAVE_SYS_TIME_H)
_check_header("sys/times.h" HAVE_SYS_TIMES_H)

# More headers; each is probed individually so absent ones simply stay off.
_check_header("unistd.h" HAVE_UNISTD_H)
_check_header("strings.h" HAVE_STRINGS_H)
_check_header("inttypes.h" HAVE_INTTYPES_H)
_check_header("dirent.h" HAVE_DIRENT_H)
_check_header("sys/wait.h" HAVE_SYS_WAIT_H)
_check_header("sys/file.h" HAVE_SYS_FILE_H)
_check_header("sys/ioctl.h" HAVE_SYS_IOCTL_H)
_check_header("sys/syscall.h" HAVE_SYS_SYSCALL_H)
_check_header("sys/select.h" HAVE_SYS_SELECT_H)
_check_header("sys/param.h" HAVE_SYS_PARAM_H)
_check_header("syscall.h" HAVE_SYSCALL_H)
_check_header("pwd.h" HAVE_PWD_H)
_check_header("grp.h" HAVE_GRP_H)
_check_header("a.out.h" HAVE_A_OUT_H)
_check_header("sys/resource.h" HAVE_SYS_RESOURCE_H)
_check_header("netinet/in_systm.h" HAVE_NETINET_IN_SYSTM_H)
_check_header("ucontext.h" HAVE_UCONTEXT_H)
_check_header("langinfo.h" HAVE_LANGINFO_H)
_check_header("locale.h" HAVE_LOCALE_H)
_check_header("sys/sendfile.h" HAVE_SYS_SENDFILE_H)
_check_header("sys/socket.h" HAVE_SYS_SOCKET_H)
_check_header("stdbool.h" HAVE_STDBOOL_H)
_check_header("sys/fcntl.h" HAVE_SYS_FCNTL_H)
_check_header("alloca.h" HAVE_ALLOCA_H)
_check_header("ieeefp.h" HAVE_IEEEFP_H)
_check_header("pthread.h" HAVE_PTHREAD_H)

_emit("// Sizeof")

_check_sizeof("int" SIZEOF_INT)
_check_sizeof("short" SIZEOF_SHORT)
_check_sizeof("long" SIZEOF_LONG)
_check_sizeof("long long" SIZEOF_LONG_LONG)
_check_sizeof("__int64" SIZEOF___INT64)
_check_sizeof("void*" SIZEOF_VOIDP)
_check_sizeof("float" SIZEOF_FLOAT)
_check_sizeof("double" SIZEOF_DOUBLE)
_check_sizeof("size_t" SIZEOF_SIZE_T)
set(CMAKE_EXTRA_INCLUDE_FILES "time.h")
_check_sizeof("time_t" SIZEOF_TIME_T)
set(CMAKE_EXTRA_INCLUDE_FILES)

# Extra size probes.  off_t/ssize_t/pid_t/... live in sys/types.h; the intN_t
# types come from stdint.h (which check_type_size includes by default).
set(CMAKE_EXTRA_INCLUDE_FILES "sys/types.h")
_check_sizeof("off_t" SIZEOF_OFF_T)
_check_sizeof("ssize_t" SIZEOF_SSIZE_T)
_check_sizeof("pid_t" SIZEOF_PID_T)
_check_sizeof("uid_t" SIZEOF_UID_T)
_check_sizeof("gid_t" SIZEOF_GID_T)
_check_sizeof("dev_t" SIZEOF_DEV_T)
_check_sizeof("mode_t" SIZEOF_MODE_T)
set(CMAKE_EXTRA_INCLUDE_FILES "sys/resource.h")
_check_sizeof("rlim_t" SIZEOF_RLIM_T)
set(CMAKE_EXTRA_INCLUDE_FILES)
_check_sizeof("ptrdiff_t" SIZEOF_PTRDIFF_T)
_check_sizeof("intptr_t" SIZEOF_INTPTR_T)
_check_sizeof("uintptr_t" SIZEOF_UINTPTR_T)
_check_sizeof("int8_t" SIZEOF_INT8_T)
_check_sizeof("uint8_t" SIZEOF_UINT8_T)
_check_sizeof("int16_t" SIZEOF_INT16_T)
_check_sizeof("uint16_t" SIZEOF_UINT16_T)
_check_sizeof("int32_t" SIZEOF_INT32_T)
_check_sizeof("uint32_t" SIZEOF_UINT32_T)
_check_sizeof("int64_t" SIZEOF_INT64_T)
_check_sizeof("uint64_t" SIZEOF_UINT64_T)
if(SIZEOF_LONG_LONG GREATER 0)
  _emit("#define HAVE_LONG_LONG 1")
endif()
if(SIZEOF_OFF_T GREATER 0)
  _emit("#define HAVE_OFF_T 1")
endif()
_emit_have_if_sizeof(SIZEOF_INT8_T HAVE_INT8_T)
_emit_have_if_sizeof(SIZEOF_UINT8_T HAVE_UINT8_T)
_emit_have_if_sizeof(SIZEOF_INT16_T HAVE_INT16_T)
_emit_have_if_sizeof(SIZEOF_UINT16_T HAVE_UINT16_T)
_emit_have_if_sizeof(SIZEOF_INT32_T HAVE_INT32_T)
_emit_have_if_sizeof(SIZEOF_UINT32_T HAVE_UINT32_T)
_emit_have_if_sizeof(SIZEOF_INT64_T HAVE_INT64_T)
_emit_have_if_sizeof(SIZEOF_UINT64_T HAVE_UINT64_T)
_emit_have_if_sizeof(SIZEOF_INTPTR_T HAVE_INTPTR_T)
_emit_have_if_sizeof(SIZEOF_UINTPTR_T HAVE_UINTPTR_T)
if(SIZEOF_SSIZE_T GREATER 0)
  _emit("#define HAVE_SSIZE_T 1")
endif()

# ---------------------------------------------------------------------------
# Library functions.  Each is probed in the headers that would declare it; a
# negative result simply means the fallback in include/ruby/missing.h (and the
# matching missing/*.c) is used.  The HAVE_<name> variables also drive the
# missing/ source selection in CMakeLists.txt.
# ---------------------------------------------------------------------------
_emit("// Library functions (auto-detected)")
set(_ruby_funcs
  # string / stdlib
  "strchr|string.h|HAVE_STRCHR"
  "strstr|string.h|HAVE_STRSTR"
  "strerror|string.h|HAVE_STRERROR"
  "strlcpy|string.h|HAVE_STRLCPY"
  "strlcat|string.h|HAVE_STRLCAT"
  "memcmp|string.h|HAVE_MEMCMP"
  "memmove|string.h|HAVE_MEMMOVE"
  "strtod|stdlib.h|HAVE_STRTOD"
  "strtol|stdlib.h|HAVE_STRTOL"
  "strtoul|stdlib.h|HAVE_STRTOUL"
  "strftime|time.h|HAVE_STRFTIME"
  "setenv|stdlib.h|HAVE_SETENV"
  "unsetenv|stdlib.h|HAVE_UNSETENV"
  # math
  "acosh|math.h|HAVE_ACOSH"
  "asinh|math.h|HAVE_ASINH"
  "atanh|math.h|HAVE_ATANH"
  "cbrt|math.h|HAVE_CBRT"
  "cosh|math.h|HAVE_COSH"
  "sinh|math.h|HAVE_SINH"
  "tanh|math.h|HAVE_TANH"
  "erf|math.h|HAVE_ERF"
  "erfc|math.h|HAVE_ERFC"
  "finite|math.h|HAVE_FINITE"
  "fmod|math.h|HAVE_FMOD"
  "frexp|math.h|HAVE_FREXP"
  "hypot|math.h|HAVE_HYPOT"
  "isinf|math.h|HAVE_ISINF"
  "isnan|math.h|HAVE_ISNAN"
  "lgamma_r|math.h|HAVE_LGAMMA_R"
  "log2|math.h|HAVE_LOG2"
  "modf|math.h|HAVE_MODF"
  "round|math.h|HAVE_ROUND"
  "signbit|math.h|HAVE_SIGNBIT"
  "tgamma|math.h|HAVE_TGAMMA"
  # unistd / process / filesystem
  "alloca|alloca.h+stdlib.h|HAVE_ALLOCA"
  "close|unistd.h|HAVE_CLOSE"
  "dup2|unistd.h|HAVE_DUP2"
  "getcwd|unistd.h|HAVE_GETCWD"
  "chroot|unistd.h|HAVE_CHROOT"
  "eaccess|unistd.h+sys/stat.h|HAVE_EACCESS"
  "truncate|unistd.h|HAVE_TRUNCATE"
  "ftruncate|unistd.h|HAVE_FTRUNCATE"
  "fseeko|stdio.h+sys/types.h|HAVE_FSEEKO"
  "ftello|stdio.h+sys/types.h|HAVE_FTELLO"
  "truncate64|unistd.h|HAVE_TRUNCATE64"
  "ftruncate64|unistd.h|HAVE_FTRUNCATE64"
  "fseeko64|stdio.h+sys/types.h|HAVE_FSEEKO64"
  "ftello64|stdio.h+sys/types.h|HAVE_FTELLO64"
  "link|unistd.h|HAVE_LINK"
  "symlink|unistd.h|HAVE_SYMLINK"
  "readlink|unistd.h|HAVE_READLINK"
  "readdir_r|dirent.h|HAVE_READDIR_R"
  "telldir|dirent.h|HAVE_TELLDIR"
  "seekdir|dirent.h|HAVE_SEEKDIR"
  "fsync|unistd.h|HAVE_FSYNC"
  "fdatasync|unistd.h|HAVE_FDATASYNC"
  "fchown|unistd.h+sys/stat.h|HAVE_FCHOWN"
  "fchmod|unistd.h+sys/stat.h|HAVE_FCHMOD"
  "lchown|unistd.h|HAVE_LCHOWN"
  "lchmod|unistd.h+sys/stat.h|HAVE_LCHMOD"
  "posix_fadvise|fcntl.h|HAVE_POSIX_FADVISE"
  "fcntl|fcntl.h+unistd.h|HAVE_FCNTL"
  "lockf|unistd.h|HAVE_LOCKF"
  "lstat|sys/stat.h+unistd.h|HAVE_LSTAT"
  "flock|sys/file.h|HAVE_FLOCK"
  "ffs|strings.h+string.h|HAVE_FFS"
  "setproctitle|unistd.h+sys/types.h|HAVE_SETPROCTITLE"
  "syscall|unistd.h+sys/syscall.h|HAVE_SYSCALL"
  "setitimer|sys/time.h|HAVE_SETITIMER"
  "getpriority|sys/resource.h|HAVE_GETPRIORITY"
  "getrlimit|sys/resource.h|HAVE_GETRLIMIT"
  "setrlimit|sys/resource.h|HAVE_SETRLIMIT"
  "getpgrp|unistd.h|HAVE_GETPGRP"
  "setpgrp|unistd.h|HAVE_SETPGRP"
  "getpgid|unistd.h|HAVE_GETPGID"
  "setpgid|unistd.h|HAVE_SETPGID"
  "seteuid|unistd.h|HAVE_SETEUID"
  "setreuid|unistd.h|HAVE_SETREUID"
  "setresuid|unistd.h|HAVE_SETRESUID"
  "setegid|unistd.h|HAVE_SETEGID"
  "setregid|unistd.h|HAVE_SETREGID"
  "setresgid|unistd.h|HAVE_SETRESGID"
  "setuid|unistd.h|HAVE_SETUID"
  "setgid|unistd.h|HAVE_SETGID"
  "initgroups|unistd.h+grp.h|HAVE_INITGROUPS"
  "getgroups|unistd.h|HAVE_GETGROUPS"
  "setgroups|unistd.h+grp.h|HAVE_SETGROUPS"
  "getgrnam_r|grp.h|HAVE_GETGRNAM_R"
  "sysconf|unistd.h|HAVE_SYSCONF"
  "pause|unistd.h|HAVE_PAUSE"
  "fork|unistd.h|HAVE_FORK"
  "killpg|unistd.h+signal.h|HAVE_KILLPG"
  "wait4|sys/wait.h|HAVE_WAIT4"
  "waitpid|sys/wait.h|HAVE_WAITPID"
  "setsid|unistd.h|HAVE_SETSID"
  "socketpair|sys/socket.h|HAVE_SOCKETPAIR"
  "shutdown|sys/socket.h|HAVE_SHUTDOWN"
  "sendfile|sys/sendfile.h+sys/socket.h|HAVE_SENDFILE"
  "pread|unistd.h|HAVE_PREAD"
  "poll|poll.h|HAVE_POLL"
  "ppoll|poll.h+time.h|HAVE_PPOLL"
  # signals
  "sigprocmask|signal.h|HAVE_SIGPROCMASK"
  "sigaction|signal.h|HAVE_SIGACTION"
  "sigaltstack|signal.h|HAVE_SIGALTSTACK"
  # time
  "mktime|time.h|HAVE_MKTIME"
  "timegm|time.h|HAVE_TIMEGM"
  "gmtime_r|time.h|HAVE_GMTIME_R"
  "clock_gettime|time.h|HAVE_CLOCK_GETTIME"
  "gettimeofday|sys/time.h|HAVE_GETTIMEOFDAY"
  "utimes|sys/time.h|HAVE_UTIMES"
  "utimensat|sys/stat.h+fcntl.h|HAVE_UTIMENSAT"
  "times|sys/times.h|HAVE_TIMES"
  # dynamic loading / backtrace
  "dlopen|dlfcn.h|HAVE_DLOPEN"
  "dl_iterate_phdr|link.h+dlfcn.h|HAVE_DL_ITERATE_PHDR"
  "backtrace|execinfo.h|HAVE_BACKTRACE"
  # threading
  "sched_yield|sched.h|HAVE_SCHED_YIELD"
  "pthread_attr_setinheritsched|pthread.h|HAVE_PTHREAD_ATTR_SETINHERITSCHED"
  "pthread_getattr_np|pthread.h|HAVE_PTHREAD_GETATTR_NP"
  "pthread_attr_getstack|pthread.h|HAVE_PTHREAD_ATTR_GETSTACK"
  "pthread_condattr_setclock|pthread.h|HAVE_PTHREAD_CONDATTR_SETCLOCK"
  "pthread_sigmask|signal.h+pthread.h|HAVE_PTHREAD_SIGMASK"
  "getcontext|ucontext.h|HAVE_GETCONTEXT"
  "setcontext|ucontext.h|HAVE_SETCONTEXT"
)
foreach(_e IN LISTS _ruby_funcs)
  string(REPLACE "|" ";" _parts "${_e}")
  list(GET _parts 0 _sym)
  list(GET _parts 1 _hdrs)
  list(GET _parts 2 _mac)
  # multiple candidate headers are joined with '+' (a ';' would split the list)
  string(REPLACE "+" ";" _hdrs "${_hdrs}")
  _detect_func(${_sym} "${_hdrs}" ${_mac})
endforeach()

# setjmp: prefer _setjmp/_longjmp (they do not save/restore the signal mask).
_detect_func("_setjmp" "setjmp.h" HAVE__SETJMP)
_detect_func("_longjmp" "setjmp.h" HAVE__LONGJMP)
if(HAVE__SETJMP)
  _emit("#define RUBY_SETJMP(env) _setjmp(env)")
else()
  _emit("#define RUBY_SETJMP(env) setjmp(env)")
endif()
if(HAVE__LONGJMP)
  _emit("#define RUBY_LONGJMP(env, val) _longjmp((env), (val))")
else()
  _emit("#define RUBY_LONGJMP(env, val) longjmp((env), (val))")
endif()
_emit("#define RUBY_JMP_BUF jmp_buf")

# crypt: modern glibc keeps it in libcrypt rather than libc, so decide by an
# actual library lookup, then tell CMakeLists whether missing/crypt.c is needed.
set(RUBY_CRYPT_OK 0)
check_library_exists("" crypt "" RUBY_CRYPT_IN_LIBC)
if(RUBY_CRYPT_IN_LIBC)
  set(RUBY_CRYPT_OK 1)
else()
  check_library_exists(crypt crypt "" RUBY_HAVE_LIBCRYPT)
  if(RUBY_HAVE_LIBCRYPT)
    set(RUBY_CRYPT_OK 1)
  endif()
endif()
if(RUBY_CRYPT_OK)
  _emit("#define HAVE_CRYPT 1")
else()
  set(RUBY_NEEDS_CRYPT 1)
endif()

# Optional shared libraries (dlopen/clock_gettime live in libc on modern
# systems; the stubs are harmless when present).
check_library_exists(dl dlopen "" RUBY_HAVE_LIBDL)
if(RUBY_HAVE_LIBDL)
  _emit("#define HAVE_LIBDL 1")
endif()
check_library_exists(rt clock_gettime "" RUBY_HAVE_LIBRT)
if(RUBY_HAVE_LIBRT)
  _emit("#define HAVE_LIBRT 1")
endif()

# ---------------------------------------------------------------------------
# struct / type layout checks
# ---------------------------------------------------------------------------
_detect_compile("
#include <time.h>
#include <sys/time.h>
int main(void){ struct timeval tv; tv.tv_sec = 0; tv.tv_usec = 0; return (int)tv.tv_sec; }
" HAVE_STRUCT_TIMEVAL)
_detect_compile("
#include <time.h>
int main(void){ struct timespec ts; ts.tv_sec = 0; ts.tv_nsec = 0; return (int)ts.tv_sec; }
" HAVE_STRUCT_TIMESPEC)
_detect_compile("
#include <sys/time.h>
#include <time.h>
int main(void){ struct timezone tz; tz.tz_minuteswest = 0; return tz.tz_minuteswest; }
" HAVE_STRUCT_TIMEZONE)
_detect_compile("
#include <time.h>
int main(void){ struct tm t; return (int)(t.tm_zone != 0); }
" HAVE_STRUCT_TM_TM_ZONE)
_detect_compile("
#include <time.h>
int main(void){ struct tm t; return (int)t.tm_gmtoff; }
" HAVE_STRUCT_TM_TM_GMTOFF)
_detect_compile("
#include <time.h>
typedef char __ruby_clk[(sizeof(clockid_t) > 0) ? 1 : -1];
int main(void){ return 0; }
" HAVE_CLOCKID_T)
_detect_compile("
#include <sys/stat.h>
int main(void){ struct stat s; return (int)(s.st_blksize); }
" HAVE_STRUCT_STAT_ST_BLKSIZE)
_detect_compile("
#include <sys/stat.h>
int main(void){ struct stat s; return (int)(s.st_blocks); }
" HAVE_STRUCT_STAT_ST_BLOCKS)
_detect_compile("
#include <sys/stat.h>
int main(void){ struct stat s; return (int)(s.st_rdev); }
" HAVE_STRUCT_STAT_ST_RDEV)
_detect_compile("
#include <sys/stat.h>
int main(void){ struct stat s; s.st_atim.tv_nsec = 0; return (int)s.st_atim.tv_nsec; }
" HAVE_STRUCT_STAT_ST_ATIM)
_detect_compile("
#include <sys/stat.h>
int main(void){ struct stat s; s.st_mtim.tv_nsec = 0; return (int)s.st_mtim.tv_nsec; }
" HAVE_STRUCT_STAT_ST_MTIM)
_detect_compile("
#include <sys/stat.h>
int main(void){ struct stat s; s.st_ctim.tv_nsec = 0; return (int)s.st_ctim.tv_nsec; }
" HAVE_STRUCT_STAT_ST_CTIM)

# short-hand HAVE_ macros derived from the layout checks above
if(HAVE_STRUCT_STAT_ST_BLKSIZE)
  _emit("#define HAVE_ST_BLKSIZE 1")
endif()
if(HAVE_STRUCT_STAT_ST_BLOCKS)
  _emit("#define HAVE_ST_BLOCKS 1")
endif()
if(HAVE_STRUCT_STAT_ST_RDEV)
  _emit("#define HAVE_ST_RDEV 1")
endif()
if(HAVE_STRUCT_TM_TM_ZONE)
  _emit("#define HAVE_TM_ZONE 1")
endif()

# Sizes of stat members used in #if comparisons (file.c).  A candidate is
# accepted when the member is exactly as wide as long/long long/int/short.
function(_check_stat_member_size NAME ACCESS)
  set(_found 0)
  set(_tokens LONG LONG_LONG INT SHORT)
  set(_ctypes "long" "long long" "int" "short")
  foreach(_tok _ctype IN ZIP_LISTS _tokens _ctypes)
    check_c_source_compiles("
#include <sys/stat.h>
typedef char __ruby_sz[ (sizeof(${ACCESS}) == sizeof(${_ctype})) ? 1 : -1 ];
int main(void){ return 0; }
" RUBY_STAT_MEMBER_${NAME}_${_tok})
    if(RUBY_STAT_MEMBER_${NAME}_${_tok})
      string(APPEND HEADER_CONTENT "#define SIZEOF_STRUCT_STAT_ST_${NAME} SIZEOF_${_tok}\n")
      set(_found 1)
      break()
    endif()
  endforeach()
  if(NOT _found)
    string(APPEND HEADER_CONTENT "#define SIZEOF_STRUCT_STAT_ST_${NAME} 0\n")
  endif()
  set(HEADER_CONTENT "${HEADER_CONTENT}" PARENT_SCOPE)
endfunction()
_check_stat_member_size(INO "((struct stat*)0)->st_ino")
_check_stat_member_size(BLOCKS "((struct stat*)0)->st_blocks")

# ---------------------------------------------------------------------------
# Compiler capabilities
# ---------------------------------------------------------------------------
_emit("#define HAVE_PROTOTYPES 1")
_emit("#define HAVE_STDARG_PROTOTYPES 1")
_emit("#define HAVE_VA_ARGS_MACRO 1")
_emit("#define TOKEN_PASTE(x, y) x##y")
_emit("#define STRINGIZE(expr) STRINGIZE0(expr)")
_detect_compile("__attribute__((noreturn)) void f(void); int main(void){return 0;}" RUBY_HAS_ATTRIBUTE)
_detect_compile("int main(void){ int x = 0; return __atomic_fetch_add(&x, 1, __ATOMIC_SEQ_CST); }" HAVE_GCC_ATOMIC_BUILTINS)
_detect_compile("int main(void){ int x = 0; return __sync_fetch_and_add(&x, 1); }" HAVE_GCC_SYNC_BUILTINS)
if(RUBY_HAS_ATTRIBUTE)
  _emit("#define NORETURN(x) __attribute__ ((noreturn)) x")
  _emit("#define DEPRECATED(x) __attribute__ ((deprecated)) x")
  _emit("#define NOINLINE(x) __attribute__ ((noinline)) x")
  _emit("#define RUBY_FUNC_EXPORTED __attribute__ ((visibility(\"default\"))) extern")
  _emit("#define RUBY_ALIAS_FUNCTION_TYPE(type, prot, name, args) type prot __attribute__((alias(#name)));")
  _emit("#define RUBY_ALIAS_FUNCTION_VOID(prot, name, args) RUBY_ALIAS_FUNCTION_TYPE(void, prot, name, args)")
else()
  _emit("#define NORETURN(x) x")
  _emit("#define DEPRECATED(x) x")
  _emit("#define NOINLINE(x) x")
endif()

# ---------------------------------------------------------------------------
# Runtime / ABI details
# ---------------------------------------------------------------------------
_emit("#define RETSIGTYPE void")
_emit("#define GETGROUPS_T gid_t")
_emit("#define RSHIFT(x, y) ((x) >> (int)(y))")
_emit("#define HAVE_RB_FD_INIT 1")
_emit("#define CANONICALIZATION_FOR_MATHN 1")
_emit("#define PRI_LL_PREFIX \"ll\"")
_emit("#define PRI_SIZE_PREFIX \"z\"")
_emit("#define PRI_PTRDIFF_PREFIX \"t\"")
_emit("#define SPT_TYPE SPT_REUSEARGV")
_emit("#define NEGATIVE_TIME_T 1")
if(HAVE_SIGACTION)
  _emit("#define POSIX_SIGNAL 1")
endif()

check_symbol_exists(tzname "time.h" RUBY_HAVE_TZNAME)
if(RUBY_HAVE_TZNAME)
  _emit("#define HAVE_TZNAME 1")
endif()
check_symbol_exists(daylight "time.h" RUBY_HAVE_DAYLIGHT)
if(RUBY_HAVE_DAYLIGHT)
  _emit("#define HAVE_DAYLIGHT 1")
endif()
check_symbol_exists(timezone "time.h" RUBY_HAVE_TIMEZONE)
if(RUBY_HAVE_TIMEZONE)
  _emit("#define HAVE_TIMEZONE 1")
  _emit("#define HAVE_VAR_TIMEZONE 1")
  _emit("#define TYPEOF_VAR_TIMEZONE long")
  _emit("#define TIMEZONE_VOID 1")
endif()
check_symbol_exists(_SC_CLK_TCK "unistd.h" RUBY_HAVE_SC_CLK_TCK)
if(RUBY_HAVE_SC_CLK_TCK)
  _emit("#define HAVE__SC_CLK_TCK 1")
endif()
check_symbol_exists(sys_nerr "stdio.h;errno.h" RUBY_HAVE_DECL_SYS_NERR)
if(RUBY_HAVE_DECL_SYS_NERR)
  _emit("#define HAVE_DECL_SYS_NERR 1")
else()
  _emit("#define HAVE_DECL_SYS_NERR 0")
endif()

# Stack direction: probed at configure time; -1 (down) is the safe default if
# the probe cannot run (e.g. cross compilation).
check_c_source_runs("
static int find_stack_direction(void) {
    static char *addr = 0;
    char dummy;
    if (addr == 0) { addr = &dummy; return find_stack_direction(); }
    return (&dummy > addr) ? 1 : -1;
}
int main(void){ return (find_stack_direction() == -1) ? 0 : 1; }
" RUBY_STACK_GROWS_DOWN)
if(RUBY_STACK_GROWS_DOWN)
  _emit("#define STACK_GROW_DIRECTION -1")
elseif("${RUBY_STACK_GROWS_DOWN}" STREQUAL "")
  _emit("#define STACK_GROW_DIRECTION -1")
else()
  _emit("#define STACK_GROW_DIRECTION 1")
endif()

# __libc_stack_end (glibc): thread_pthread.c declares it itself, this is just
# a link-time existence probe for the stack-end address.
check_c_source_compiles("
extern void *__libc_stack_end;
int main(void){ return __libc_stack_end ? 0 : 0; }
" RUBY_HAVE_LIBC_STACK_END)
if(RUBY_HAVE_LIBC_STACK_END)
  _emit("#define STACK_END_ADDRESS __libc_stack_end")
endif()

# FILE internals used for input-buffer bookkeeping (glibc layout).
check_c_source_compiles("
#include <stdio.h>
int main(void){ return (int)sizeof(((FILE*)0)->_IO_read_ptr); }
" RUBY_FILE_HAS_IO_READ_PTR)
check_c_source_compiles("
#include <stdio.h>
int main(void){ return (int)sizeof(((FILE*)0)->_IO_read_end); }
" RUBY_FILE_HAS_IO_READ_END)
if(RUBY_FILE_HAS_IO_READ_PTR)
  _emit("#define FILE_READPTR _IO_read_ptr")
endif()
if(RUBY_FILE_HAS_IO_READ_END)
  _emit("#define FILE_READEND _IO_read_end")
endif()

# ---------------------------------------------------------------------------
# Type aliases and numeric conversion helpers
# ---------------------------------------------------------------------------
_emit_alias(pid pid_t SIZEOF_PID_T 0 PIDT)
_emit_alias(uid uid_t SIZEOF_UID_T 1 UIDT)
_emit_alias(gid gid_t SIZEOF_GID_T 1 GIDT)
_emit_alias(time time_t SIZEOF_TIME_T 0 TIMET)
_emit_alias(dev dev_t SIZEOF_DEV_T 1 DEVT)
_emit_alias(mode mode_t SIZEOF_MODE_T 1 MODET)
_emit_alias(rlim rlim_t SIZEOF_RLIM_T 1 RLIM)

# ---- Windows branch ---------------------------------------------------------
# The POSIX probes above cannot run meaningfully against the MSVC CRT (no
# <unistd.h>/<sys/time.h>/pid_t/..., no libm), so on Windows we emit the same
# known-good configuration the official win32 build produces (see
# win32/Makefile.sub).  The HAVE_* variables are set here too, so CMakeLists.txt
# selects the same missing/*.c fallbacks (crypt, ffs, langinfo, lgamma_r,
# strlcat, strlcpy, setproctitle + win32/win32.c + win32/file.c) the nmake
# build uses.
else()
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(RUBY_WIN_PTR 8)
    set(RUBY_WIN_SSIZE "__int64")
  else()
    set(RUBY_WIN_PTR 4)
    set(RUBY_WIN_SSIZE "int")
  endif()

  _emit("// Header Check")
  _emit("#define STDC_HEADERS 1")
  foreach(_e IN ITEMS
      "sys/types.h|HAVE_SYS_TYPES_H"
      "sys/stat.h|HAVE_SYS_STAT_H"
      "stdlib.h|HAVE_STDLIB_H"
      "stddef.h|HAVE_STDDEF_H"
      "string.h|HAVE_STRING_H"
      "memory.h|HAVE_MEMORY_H"
      "stdint.h|HAVE_STDINT_H"
      "limits.h|HAVE_LIMITS_H"
      "fcntl.h|HAVE_FCNTL_H"
      "float.h|HAVE_FLOAT_H"
      "time.h|HAVE_TIME_H"
      "sys/utime.h|HAVE_SYS_UTIME_H")
    string(REPLACE "|" ";" _parts "${_e}")
    list(GET _parts 0 _hp)
    list(GET _parts 1 _hm)
    _emit("#define ${_hm} 1")
  endforeach()

  _emit("// Sizeof")
  set(SIZEOF_INT 4)
  set(SIZEOF_SHORT 2)
  set(SIZEOF_LONG 4)
  set(SIZEOF_LONG_LONG 8)
  set(SIZEOF___INT64 8)
  set(SIZEOF_VOIDP ${RUBY_WIN_PTR})
  set(SIZEOF_FLOAT 4)
  set(SIZEOF_DOUBLE 8)
  set(SIZEOF_TIME_T 8)
  set(SIZEOF_OFF_T 4)
  set(SIZEOF_SSIZE_T ${RUBY_WIN_PTR})
  set(SIZEOF_PID_T 0)
  set(SIZEOF_UID_T 0)
  set(SIZEOF_GID_T 0)
  set(SIZEOF_DEV_T 4)
  set(SIZEOF_MODE_T 0)
  set(SIZEOF_RLIM_T 0)
  set(SIZEOF_SIZE_T ${RUBY_WIN_PTR})
  set(SIZEOF_PTRDIFF_T ${RUBY_WIN_PTR})
  set(SIZEOF_INTPTR_T ${RUBY_WIN_PTR})
  set(SIZEOF_UINTPTR_T ${RUBY_WIN_PTR})
  set(SIZEOF_INT8_T 1)
  set(SIZEOF_UINT8_T 1)
  set(SIZEOF_INT16_T 2)
  set(SIZEOF_UINT16_T 2)
  set(SIZEOF_INT32_T 4)
  set(SIZEOF_UINT32_T 4)
  set(SIZEOF_INT64_T 8)
  set(SIZEOF_UINT64_T 8)
  foreach(_s IN ITEMS
      SIZEOF_INT SIZEOF_SHORT SIZEOF_LONG SIZEOF_LONG_LONG SIZEOF___INT64
      SIZEOF_VOIDP SIZEOF_FLOAT SIZEOF_DOUBLE SIZEOF_TIME_T SIZEOF_OFF_T
      SIZEOF_SSIZE_T SIZEOF_PID_T SIZEOF_UID_T SIZEOF_GID_T SIZEOF_DEV_T
      SIZEOF_MODE_T SIZEOF_RLIM_T SIZEOF_SIZE_T SIZEOF_PTRDIFF_T
      SIZEOF_INTPTR_T SIZEOF_UINTPTR_T SIZEOF_INT8_T SIZEOF_UINT8_T
      SIZEOF_INT16_T SIZEOF_UINT16_T SIZEOF_INT32_T SIZEOF_UINT32_T
      SIZEOF_INT64_T SIZEOF_UINT64_T)
    _emit("#define ${_s} ${${_s}}")
  endforeach()
  _emit("#define HAVE_LONG_LONG 1")
  _emit("#define HAVE_OFF_T 1")
  _emit("#define HAVE_SSIZE_T 1")
  if(MSVC)
    # MSVC has no ssize_t; MinGW provides it itself.
    _emit("#define ssize_t ${RUBY_WIN_SSIZE}")
  endif()
  _emit_have_if_sizeof(SIZEOF_INT8_T HAVE_INT8_T)
  _emit_have_if_sizeof(SIZEOF_UINT8_T HAVE_UINT8_T)
  _emit_have_if_sizeof(SIZEOF_INT16_T HAVE_INT16_T)
  _emit_have_if_sizeof(SIZEOF_UINT16_T HAVE_UINT16_T)
  _emit_have_if_sizeof(SIZEOF_INT32_T HAVE_INT32_T)
  _emit_have_if_sizeof(SIZEOF_UINT32_T HAVE_UINT32_T)
  _emit_have_if_sizeof(SIZEOF_INT64_T HAVE_INT64_T)
  _emit_have_if_sizeof(SIZEOF_UINT64_T HAVE_UINT64_T)
  _emit_have_if_sizeof(SIZEOF_INTPTR_T HAVE_INTPTR_T)
  _emit_have_if_sizeof(SIZEOF_UINTPTR_T HAVE_UINTPTR_T)

  # Library functions present in the MSVC CRT are enabled so the matching
  # missing/*.c fallbacks stay out of the build, exactly like the official
  # win32 build (its MISSING set is only crypt/ffs/langinfo/lgamma_r/strlcat/
  # strlcpy/setproctitle + win32/win32 + win32/file).
  _emit("// Library functions (MSVC CRT)")
  foreach(_f IN ITEMS
      HAVE_STRCHR HAVE_STRSTR HAVE_STRERROR HAVE_STRTOD HAVE_STRTOL HAVE_STRTOUL
      HAVE_STRFTIME HAVE_MEMCMP HAVE_MEMMOVE HAVE_MKDIR HAVE_STRCASECMP
      HAVE_STRNCASECMP HAVE_ACOSH HAVE_ASINH HAVE_ATANH HAVE_CBRT HAVE_LOG2
      HAVE_ERF HAVE_ERFC HAVE_ROUND HAVE_TGAMMA HAVE_COSH HAVE_SINH HAVE_TANH
      HAVE_FMOD HAVE_FREXP HAVE_MODF HAVE_HYPOT HAVE_SIGNBIT HAVE_ISNAN
      HAVE_FINITE HAVE_ISINF HAVE_ALLOCA HAVE_DUP2 HAVE_GETCWD HAVE_TRUNCATE
      HAVE_FTRUNCATE HAVE_FSEEKO HAVE_FTELLO HAVE_TIMES HAVE_FCNTL HAVE_LINK
      HAVE_FLOCK HAVE_FSYNC HAVE_WAITPID HAVE_MKTIME HAVE_TELLDIR HAVE_SEEKDIR)
    set(${_f} 1)
    _emit("#define ${_f} 1")
  endforeach()
  # strlcpy/strlcat/ffs/setproctitle/langinfo/lgamma_r/crypt are absent from
  # the MSVC CRT; leave their HAVE_* undefined so the missing/*.c fallbacks
  # (and missing/crypt.c) are compiled, as in the official win32 build.
  set(RUBY_NEEDS_CRYPT 1)

  _emit("#define RUBY_SETJMP(env) _setjmp(env)")
  _emit("#define RUBY_LONGJMP(env, val) longjmp((env), (val))")
  _emit("#define RUBY_JMP_BUF jmp_buf")

  _emit("// struct / type layout")
  _emit("#define HAVE_STRUCT_TIMEVAL 1")   # from winsock2.h via ruby/win32.h
  _emit("#define HAVE_STRUCT_TIMESPEC 1")  # from the UCRT time.h
  _emit("#define TYPEOF_TIMEVAL_TV_SEC long")
  _emit("#define HAVE_STRUCT_STAT_ST_RDEV 1")
  _emit("#define HAVE_ST_RDEV 1")
  _emit("#define SIZEOF_STRUCT_STAT_ST_INO SIZEOF_SHORT")
  _emit("#define SIZEOF_STRUCT_STAT_ST_BLOCKS 0")

  _emit("// Compiler capabilities")
  _emit("#define HAVE_PROTOTYPES 1")
  _emit("#define HAVE_STDARG_PROTOTYPES 1")
  _emit("#define HAVE_VA_ARGS_MACRO 1")
  _emit("#define TOKEN_PASTE(x, y) x##y")
  _emit("#define STRINGIZE(expr) STRINGIZE0(expr)")
  _emit("#define NORETURN(x) __declspec(noreturn) x")
  _emit("#define DEPRECATED(x) __declspec(deprecated) x")
  _emit("#define NOINLINE(x) __declspec(noinline) x")

  _emit("// Runtime / ABI details")
  _emit("#define RETSIGTYPE void")
  _emit("#define GETGROUPS_T int")
  _emit("#define RSHIFT(x, y) ((x) >> (int)(y))")
  _emit("#define HAVE_RB_FD_INIT 1")
  _emit("#define CANONICALIZATION_FOR_MATHN 1")
  _emit("#define PRI_LL_PREFIX \"I64\"")
  _emit("#define PRI_SIZE_PREFIX \"z\"")
  _emit("#define PRI_PTRDIFF_PREFIX \"t\"")
  _emit("#define SPT_TYPE SPT_NONE")
  _emit("#define NEGATIVE_TIME_T 1")
  _emit("#define SETPGRP_VOID 1")
  _emit("#define HAVE_DECL_SYS_NERR 1")
  _emit("#define STACK_GROW_DIRECTION -1")
  # EXECUTABLE_EXTS drives dln_find.c's executable-extension list (DOSISH).
  # tzname/daylight and the FILE internals (FILE_READPTR/FILE_COUNT) are NOT
  # defined: the modern UCRT exposes them differently (or not at all), and
  # time.c/io.c have clean fallbacks when the macros are absent.
  _emit("#define EXECUTABLE_EXTS \".exe\", \".com\", \".cmd\", \".bat\"")
  # RT_VER selects the modern CRT code paths in win32.c (RT_VER >= 80 turns on
  # the UCRT-compatible error/RTC/pioinfo handling; the VC6-era _ftol shim in
  # the RT_VER <= 60 branch is skipped).
  _emit("#define RT_VER 140")
  _emit("#define inline __inline")
  _emit("#define NEED_IO_SEEK_BETWEEN_RW 1")
  _emit("#define LOAD_RELATIVE 1")
  _emit("#define DEFAULT_KCODE KCODE_NONE")

  _emit("// Type aliases and numeric conversion helpers")
  _emit_alias(pid pid_t SIZEOF_PID_T 0 PIDT)
  _emit_alias(uid uid_t SIZEOF_UID_T 1 UIDT)
  _emit_alias(gid gid_t SIZEOF_GID_T 1 GIDT)
  _emit_alias(time time_t SIZEOF_TIME_T 0 TIMET)
  _emit_alias(dev dev_t SIZEOF_DEV_T 1 DEVT)
  _emit_alias(mode mode_t SIZEOF_MODE_T 1 MODET)
  _emit_alias(rlim rlim_t SIZEOF_RLIM_T 1 RLIM)
endif()

# ---------------------------------------------------------------------------
# Platform / installation constants
# ---------------------------------------------------------------------------
if(NOT CMAKE_SYSTEM_PROCESSOR)
  set(CMAKE_SYSTEM_PROCESSOR "unknown")
endif()
string(TOLOWER "${CMAKE_SYSTEM_NAME}" _os_name)
if(NOT _os_name)
  set(_os_name "unknown")
endif()
_emit("#define RUBY_PLATFORM \"${CMAKE_SYSTEM_PROCESSOR}-${_os_name}\"")
_emit("#define RUBY_LIB_VERSION_STYLE 3")
_emit("#define RUBY_EXEC_PREFIX \"/usr/local\"")
_emit("#define RUBY_LIB_PREFIX RUBY_EXEC_PREFIX\"/lib/ruby\"")
_emit("#define RUBY_SITE_LIB RUBY_LIB_PREFIX\"/site_ruby\"")
_emit("#define RUBY_VENDOR_LIB RUBY_LIB_PREFIX\"/vendor_ruby\"")
if(WIN32)
  _emit("#define DLEXT \".dll\"")
  _emit("#define DLEXT_MAXLEN 4")
else()
  _emit("#define DLEXT \".so\"")
  _emit("#define DLEXT_MAXLEN 3")
endif()

# ELF targets (Linux, *BSD, ...) get USE_ELF plus the addr2line.c backtrace
# support; detected via the __ELF__ macro predefined by the compiler driver.
if(NOT WIN32)
check_c_source_compiles("
#ifndef __ELF__
#error not an ELF target
#endif
int main(void){ return 0; }
" RUBY_IS_ELF)
if(RUBY_IS_ELF)
  _emit("#define USE_ELF 1")
endif()
endif()

_emit("")
_emit("#endif /* INCLUDE_RUBY_CONFIG_H */")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/include/ruby/)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/include/ruby/config.h "${HEADER_CONTENT}")

