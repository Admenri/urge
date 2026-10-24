# ---------------------------------------------------------------------------
# RubyUnix.cmake -- platform configuration for POSIX-ish systems
#
# Everything in here is discovered with feature tests, never with a list of
# known operating systems.  A completely unknown OS that provides a C library
# and <unistd.h> will configure and build; missing pieces are taken from
# missing/ (the same fallbacks the autoconf build uses).
#
# Results are assembled into <build>/include/ruby/config.h.
# ---------------------------------------------------------------------------

include(CheckIncludeFile)
include(CheckCSourceCompiles)
include(CheckLibraryExists)
include(CheckIncludeFiles)
include(FindThreads)

# ---------------------------------------------------------------------------
# 1. Feature test macros
# ---------------------------------------------------------------------------
set(RUBY_FEATURE_TEST_MACROS
  _GNU_SOURCE
  _DARWIN_C_SOURCE
  _NETBSD_SOURCE
  _OPENBSD_SOURCE
  _ALL_SOURCE
  _TANDEM_SOURCE
  _HPUX_ALT_XOPEN_SOCKET_API
  _POSIX_PTHREAD_SEMANTICS
  __EXTENSIONS__
  __STDC_WANT_LIB_EXT2__
  __STDC_WANT_IEC_60559_TYPES_EXT__
  __STDC_WANT_IEC_60559_FUNCS_EXT__
  __STDC_WANT_IEC_60559_BFP_EXT__
  __STDC_WANT_IEC_60559_DFP_EXT__
  __STDC_WANT_IEC_60559_ATTRIBS_EXT__
  __STDC_WANT_MATH_SPEC_FUNCS__
)

set(RUBY_FEATURE_TEST_DEFINES "")
set(CMAKE_REQUIRED_DEFINITIONS "")
foreach(macro IN LISTS RUBY_FEATURE_TEST_MACROS)
  string(APPEND RUBY_FEATURE_TEST_DEFINES "#ifndef ${macro}\n# define ${macro} 1\n#endif\n")
  list(APPEND CMAKE_REQUIRED_DEFINITIONS "-D${macro}")
endforeach()

# ---------------------------------------------------------------------------
# 2. Headers
# ---------------------------------------------------------------------------
set(RUBY_HEADERS
  stdio.h stdlib.h string.h strings.h inttypes.h stdint.h stdbool.h stddef.h
  stdarg.h limits.h float.h wchar.h wctype.h math.h
  sys/types.h sys/stat.h sys/time.h sys/times.h sys/wait.h sys/param.h
  sys/resource.h sys/select.h sys/socket.h sys/uio.h sys/mman.h sys/ioctl.h
  sys/file.h sys/fcntl.h sys/random.h sys/syscall.h sys/sysmacros.h
  sys/prctl.h sys/sendfile.h sys/eventfd.h sys/epoll.h sys/user.h sys/utsname.h
  sys/auxv.h sys/pstat.h sys/statvfs.h sys/un.h netinet/in.h arpa/inet.h
  netdb.h syslog.h
  unistd.h fcntl.h dirent.h grp.h pwd.h signal.h setjmp.h errno.h locale.h
  langinfo.h malloc.h time.h utime.h poll.h dlfcn.h crypt.h alloca.h
  ucontext.h pthread.h sched.h semaphore.h termios.h
  stdalign.h stdatomic.h stdckdint.h
  x86intrin.h gmp.h elf.h elf_abi.h mach-o/loader.h crt_externs.h
)

set(RUBY_FEATURE_DEFINES "")
set(RUBY_CHECK_PREAMBLE "")
foreach(hdr IN LISTS RUBY_HEADERS)
  string(TOUPPER "${hdr}" _var)
  string(REGEX REPLACE "[^A-Z0-9]" "_" _var "${_var}")
  set(_var "HAVE_${_var}")
  check_include_file("${hdr}" ${_var})
  if(${_var})
    string(APPEND RUBY_FEATURE_DEFINES "#define ${_var} 1\n")
    string(APPEND RUBY_CHECK_PREAMBLE "#include <${hdr}>\n")
  endif()
endforeach()

# <sys/types.h> must be visible for the checks below.
set(RUBY_CHECK_PREAMBLE
  "${RUBY_FEATURE_TEST_DEFINES}#include <sys/types.h>\n${RUBY_CHECK_PREAMBLE}")

if(HAVE_STDIO_H AND HAVE_STDLIB_H AND HAVE_STRING_H)
  string(APPEND RUBY_FEATURE_DEFINES "#define STDC_HEADERS 1\n")
endif()

# Helper: compile a snippet with the platform preamble.
function(ruby_c_check code var)
  check_c_source_compiles("${RUBY_CHECK_PREAMBLE}\n${code}" ${var})
endfunction()

# ---------------------------------------------------------------------------
# 3. Libraries
# ---------------------------------------------------------------------------
set(RUBY_LINK_LIBS "")
set(CMAKE_REQUIRED_LIBRARIES "")

find_package(Threads QUIET)
if(Threads_FOUND)
  string(APPEND RUBY_FEATURE_DEFINES "#define HAVE_LIBPTHREAD 1\n")
  list(APPEND RUBY_LINK_LIBS Threads::Threads)
endif()

set(_lib_names crypt dl rt z m)
set(_lib_syms  crypt dlopen clock_gettime deflate cos)
set(_lib_defs  HAVE_LIBCRYPT HAVE_LIBDL HAVE_LIBRT HAVE_LIBZ HAVE_LIBM)
foreach(i RANGE 4)
  list(GET _lib_names ${i} _lib)
  list(GET _lib_syms  ${i} _sym)
  list(GET _lib_defs  ${i} _def)
  check_library_exists(${_lib} ${_sym} "" RUBY_CV_${_def})
  if(RUBY_CV_${_def})
    string(APPEND RUBY_FEATURE_DEFINES "#define ${_def} 1\n")
    list(APPEND RUBY_LINK_LIBS ${_lib})
  endif()
endforeach()

if(RUBY_USE_GMP)
  check_library_exists(gmp "__gmpz_init" "" RUBY_HAVE_LIB_GMP)
  if(RUBY_HAVE_LIB_GMP AND HAVE_GMP_H)
    string(APPEND RUBY_FEATURE_DEFINES "#define HAVE_LIBGMP 1\n")
    list(APPEND RUBY_LINK_LIBS gmp)
    set(RUBY_USE_GMP_FINAL ON)
  endif()
endif()

# Function checks need the same libraries the final link uses.
set(CMAKE_REQUIRED_LIBRARIES ${RUBY_LINK_LIBS})

# ---------------------------------------------------------------------------
# 4. Functions
# ---------------------------------------------------------------------------
set(RUBY_FUNCS
  dup dup2 acosh cbrt crypt erf explicit_bzero ffs flock hypot lgamma_r
  memcmp memmove nan nextafter strchr strerror strlcat strlcpy strstr tgamma
  fork vfork arc4random_buf atan2l atan2f
  chmod chown chroot clock_gettime clock_getres copy_file_range cosh crypt_r
  dirfd dl_iterate_phdr dlopen dladdr dup3 eaccess endgrent eventfd
  execl execle execv execve fchdir fchmod fchown fcntl fdatasync fdopendir
  fmod fstatat fsync ftruncate ftruncate64 getcwd getegid getentropy geteuid
  getgid getgrnam getgrnam_r getgroups getlogin getlogin_r getpgid getpgrp
  getppid getpriority getpwnam getpwnam_r getpwuid getpwuid_r getrandom
  getresgid getresuid getrlimit getsid gettimeofday getuid gmtime_r initgroups
  ioctl kill killpg lchmod lchown link llabs lockf log2 lstat lutimes
  malloc_usable_size malloc_trim mblen memalign writev memrchr memmem mkfifo
  mknod mktime mmap mremap openat pclose pipe pipe2 poll popen posix_fadvise
  posix_madvise posix_memalign ppoll pread pwrite qsort_r readlink realpath
  round sched_getaffinity sched_yield seekdir sendfile setegid setenv seteuid
  setgid setgroups setpgid setpgrp setregid setresgid setresuid setreuid
  setrlimit setsid setuid shutdown sigaction sigaltstack sigprocmask sinh
  snprintf symlink syscall sysconf system tanh telldir timegm times truncate
  truncate64 tzset umask unsetenv utimensat utimes wait4 waitpid statx
  getcontext swapcontext makecontext setcontext backtrace setproctitle
  _setjmp _longjmp _setjmpex
  pthread_attr_setinheritsched pthread_attr_getstack pthread_attr_getguardsize
  pthread_condattr_setclock pthread_setname_np pthread_sigmask
  pthread_getattr_np timer_create timer_settime
)

foreach(fn IN LISTS RUBY_FUNCS)
  string(TOUPPER "${fn}" _up)
  set(_var "HAVE_${_up}")
  ruby_c_check("int main(void) { (void)${fn}; return 0; }" ${_var})
  if(${_var})
    string(APPEND RUBY_FEATURE_DEFINES "#define ${_var} 1\n")
  endif()
endforeach()

# Functions that are macros in <math.h> can only be tested by calling them.
function(ruby_check_call var code)
  ruby_c_check("${code}" ${var})
  if(${var})
    string(APPEND RUBY_FEATURE_DEFINES "#define ${var} 1\n")
    set(RUBY_FEATURE_DEFINES "${RUBY_FEATURE_DEFINES}" PARENT_SCOPE)
  endif()
endfunction()

ruby_check_call(HAVE_ISFINITE "int main(void){return isfinite(1.0)?0:1;}")
ruby_check_call(HAVE_ISINF "int main(void){return isinf(1.0)?0:1;}")
ruby_check_call(HAVE_ISNAN "int main(void){return isnan(1.0)?0:1;}")
ruby_check_call(HAVE_SIGNBIT "int main(void){return signbit(1.0)?0:1;}")
ruby_check_call(HAVE_FINITE "int main(void){return finite(1.0)?0:1;}")

# --- compiler builtins -----------------------------------------------------
function(ruby_check_builtin var code)
  ruby_c_check("${code}" ${var})
  if(${var})
    string(APPEND RUBY_FEATURE_DEFINES "#define ${var} 1\n")
    set(RUBY_FEATURE_DEFINES "${RUBY_FEATURE_DEFINES}" PARENT_SCOPE)
  endif()
endfunction()

ruby_check_builtin(HAVE_BUILTIN___BUILTIN_BSWAP16 "int main(void){return (int)__builtin_bswap16(1);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_BSWAP32 "int main(void){return (int)__builtin_bswap32(1);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_BSWAP64 "int main(void){return (int)__builtin_bswap64(1);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_POPCOUNT "int main(void){return __builtin_popcount(1);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_POPCOUNTLL "int main(void){return __builtin_popcountll(1);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_CLZ "int main(void){return __builtin_clz(1);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_CLZL "int main(void){return __builtin_clzl(1);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_CLZLL "int main(void){return __builtin_clzll(1);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_CTZ "int main(void){return __builtin_ctz(1);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_CTZLL "int main(void){return __builtin_ctzll(1);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_CONSTANT_P "int main(void){return __builtin_constant_p(1);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_CHOOSE_EXPR "int main(void){return __builtin_choose_expr(1,1,2);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_TYPES_COMPATIBLE_P "int main(void){return __builtin_types_compatible_p(int,int);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_TRAP "int main(void){if(0)__builtin_trap();return 0;}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_EXPECT "int main(void){return __builtin_expect(1,1);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_UNREACHABLE "int main(void){if(0)__builtin_unreachable();return 0;}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_ASSUME_ALIGNED "int main(void){char*p=0;return __builtin_assume_aligned(p,8)!=0;}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_ALLOCA_WITH_ALIGN "int main(void){void*p=__builtin_alloca_with_align(8,64);return p!=0;}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_ADD_OVERFLOW "int main(void){int r;return __builtin_add_overflow(1,1,&r);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_ADD_OVERFLOW_P "int main(void){int r;return __builtin_add_overflow_p(1,1,(int)0);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_SUB_OVERFLOW "int main(void){int r;return __builtin_sub_overflow(1,1,&r);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_SUB_OVERFLOW_P "int main(void){int r;return __builtin_sub_overflow_p(1,1,(int)0);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_MUL_OVERFLOW "int main(void){int r;return __builtin_mul_overflow(1,1,&r);}")
ruby_check_builtin(HAVE_BUILTIN___BUILTIN_MUL_OVERFLOW_P "int main(void){int r;return __builtin_mul_overflow_p(1,1,(int)0);}")

# --- language / compiler features -----------------------------------------
ruby_c_check("int main(void){int x=({int y=1;y;});return x;}" HAVE_STMT_AND_DECL_IN_EXPR)
ruby_c_check("int main(void){__typeof__(1) x=1;return x;}" HAVE_TYPEOF)
ruby_c_check("int main(void){_Bool b=1;return b;}" HAVE__BOOL)
ruby_c_check("#define M(...) __VA_ARGS__\nint main(void){return M(0);}" HAVE_VA_ARGS_MACRO)
ruby_c_check("int main(void){int a[1];return (int)_Alignof(int)+a[0];}" HAVE__ALIGNOF)
ruby_c_check("int main(void){return (int)sizeof(long long);}" HAVE_LONG_LONG)
ruby_c_check("int main(void){static int x __attribute__((weak));return x;}" HAVE_FUNC_WEAK)
ruby_c_check("int main(void){void*p=nullptr;return p!=0;}" HAVE_NULLPTR)
ruby_c_check("int f(int) __attribute__((alias(\"g\"))); int g(int x){return x;} int main(void){return f(0);}" HAVE_ATTRIBUTE_FUNCTION_ALIAS)

foreach(_cv
    HAVE_STMT_AND_DECL_IN_EXPR HAVE_TYPEOF HAVE__BOOL HAVE_VA_ARGS_MACRO
    HAVE__ALIGNOF HAVE_LONG_LONG HAVE_FUNC_WEAK HAVE_NULLPTR
    HAVE_ATTRIBUTE_FUNCTION_ALIAS)
  if(${_cv})
    string(APPEND RUBY_FEATURE_DEFINES "#define ${_cv} 1\n")
  endif()
endforeach()

ruby_c_check("int main(void){int x=0;__atomic_add_fetch(&x,1,__ATOMIC_SEQ_CST);return x;}" HAVE_GCC_ATOMIC_BUILTINS)
ruby_c_check("int main(void){long long x=0;__atomic_add_fetch(&x,1,__ATOMIC_SEQ_CST);return (int)x;}" HAVE_GCC_ATOMIC_BUILTINS_64)
ruby_c_check("int main(void){int x=0;__sync_add_and_fetch(&x,1);return x;}" HAVE_GCC_SYNC_BUILTINS)
foreach(_cv HAVE_GCC_ATOMIC_BUILTINS HAVE_GCC_ATOMIC_BUILTINS_64 HAVE_GCC_SYNC_BUILTINS)
  if(${_cv})
    string(APPEND RUBY_FEATURE_DEFINES "#define ${_cv} 1\n")
  endif()
endforeach()

# ---------------------------------------------------------------------------
# 5. Types, sizes and integer conversions
# ---------------------------------------------------------------------------
set(RUBY_SIZEOF_DEFINES "")
set(RUBY_TYPEDEF_DEFINES "")

# Determine sizeof(type) by compiling a negative-size array probe.
function(ruby_check_sizeof type var)
  foreach(sz 1 2 4 8 16)
    set(_probe "RUBY_SIZEOF_${var}_${sz}")
    ruby_c_check(
      "int main(void){char a[(sizeof(${type})==${sz})?1:-1];(void)a;return 0;}"
      ${_probe})
    if(${_probe})
      set(${var} ${sz} PARENT_SCOPE)
      return()
    endif()
  endforeach()
  set(${var} 0 PARENT_SCOPE)
endfunction()

macro(ruby_sizeof_define type name)
  ruby_check_sizeof("${type}" RUBY_SIZEOF_${name})
  string(APPEND RUBY_SIZEOF_DEFINES "#define SIZEOF_${name} ${RUBY_SIZEOF_${name}}\n")
endmacro()

ruby_sizeof_define("int" INT)
ruby_sizeof_define("short" SHORT)
ruby_sizeof_define("long" LONG)
ruby_sizeof_define("long long" LONG_LONG)
ruby_sizeof_define("void *" VOIDP)
ruby_sizeof_define("float" FLOAT)
ruby_sizeof_define("double" DOUBLE)
ruby_sizeof_define("size_t" SIZE_T)
ruby_sizeof_define("ptrdiff_t" PTRDIFF_T)
ruby_sizeof_define("off_t" OFF_T)
ruby_sizeof_define("time_t" TIME_T)
ruby_sizeof_define("clock_t" CLOCK_T)
ruby_sizeof_define("dev_t" DEV_T)

# int64_t / __int128 / __int64
foreach(t int8_t uint8_t int16_t uint16_t int32_t uint32_t int64_t uint64_t
          intptr_t uintptr_t ssize_t)
  string(TOUPPER "${t}" _up)
  ruby_c_check("int main(void){${t} v=(${t})0;(void)v;return 0;}" HAVE_${_up})
  if(HAVE_${_up})
    string(APPEND RUBY_FEATURE_DEFINES "#define HAVE_${_up} 1\n")
    ruby_check_sizeof("${t}" RUBY_SIZEOF_${_up})
    string(APPEND RUBY_SIZEOF_DEFINES "#define SIZEOF_${_up} ${RUBY_SIZEOF_${_up}}\n")
  endif()
endforeach()

ruby_c_check("int main(void){__int128 v=0;return (int)v;}" HAVE_INT128_T)
if(HAVE_INT128_T)
  ruby_check_sizeof("__int128" RUBY_SIZEOF___INT128)
  string(APPEND RUBY_SIZEOF_DEFINES "#define SIZEOF___INT128 ${RUBY_SIZEOF___INT128}\n")
  string(APPEND RUBY_FEATURE_DEFINES "#define HAVE_INT128_T 1\n#define int128_t __int128\n")
  string(APPEND RUBY_FEATURE_DEFINES "#define HAVE_UINT128_T 1\n#define uint128_t unsigned __int128\n")
  string(APPEND RUBY_SIZEOF_DEFINES "#define SIZEOF_INT128_T SIZEOF___INT128\n#define SIZEOF_UINT128_T SIZEOF___INT128\n")
else()
  string(APPEND RUBY_SIZEOF_DEFINES "#define SIZEOF___INT128 0\n")
endif()
string(APPEND RUBY_SIZEOF_DEFINES "#define SIZEOF___INT64 0\n")

# --- typedef conversion macros ---------------------------------------------
# For a platform type T (e.g. pid_t) Ruby needs:
#   rb_T, SIGNEDNESS_OF_T, T2NUM/NUM2T and a PRI_T_PREFIX.
# The numeric conversion is chosen from the actual size and signedness, so
# this works on 32 bit, 64 bit and LP64/LLP64 systems alike.
function(ruby_typedef_conversions type upper rbname)
  string(TOUPPER "${upper}" _U)
  ruby_c_check("int main(void){${type} v=(${type})0;(void)v;return 0;}" RUBY_HAVE_${_U})
  if(NOT RUBY_HAVE_${_U})
    return()
  endif()

  string(APPEND RUBY_FEATURE_DEFINES "#define HAVE_${_U} 1\n")

  ruby_check_sizeof("${type}" RUBY_SIZEOF_${_U})
  ruby_c_check("int main(void){return (((${type})-1) < ((${type})0)) ? 1 : 0;}" RUBY_SIGNED_${_U})

  set(_prefix "${_U}")
  string(REPLACE "_" "" _prefix "${_prefix}")

  set(_lines "")
  string(APPEND _lines "#define ${rbname} ${type}\n")
  if(RUBY_SIGNED_${_U})
    string(APPEND _lines "#define SIGNEDNESS_OF_${_U} -1\n")
  else()
    string(APPEND _lines "#define SIGNEDNESS_OF_${_U} +1\n")
  endif()

  set(_sz ${RUBY_SIZEOF_${_U}})
  if(_sz EQUAL 8 AND RUBY_SIZEOF_LONG EQUAL 8)
    set(_long 1)
  else()
    set(_long 0)
  endif()

  if(_sz LESS 8)
    if(RUBY_SIGNED_${_U})
      string(APPEND _lines "#define ${_prefix}2NUM(v) INT2NUM(v)\n#define NUM2${_prefix}(v) NUM2INT(v)\n")
    else()
      string(APPEND _lines "#define ${_prefix}2NUM(v) UINT2NUM(v)\n#define NUM2${_prefix}(v) NUM2UINT(v)\n")
    endif()
    string(APPEND _lines "#define PRI_${_prefix}_PREFIX PRI_INT_PREFIX\n")
  elseif(_long)
    if(RUBY_SIGNED_${_U})
      string(APPEND _lines "#define ${_prefix}2NUM(v) LONG2NUM(v)\n#define NUM2${_prefix}(v) NUM2LONG(v)\n")
    else()
      string(APPEND _lines "#define ${_prefix}2NUM(v) ULONG2NUM(v)\n#define NUM2${_prefix}(v) NUM2ULONG(v)\n")
    endif()
    string(APPEND _lines "#define PRI_${_prefix}_PREFIX PRI_LONG_PREFIX\n")
  else()
    if(RUBY_SIGNED_${_U})
      string(APPEND _lines "#define ${_prefix}2NUM(v) LL2NUM(v)\n#define NUM2${_prefix}(v) NUM2LL(v)\n")
    else()
      string(APPEND _lines "#define ${_prefix}2NUM(v) ULL2NUM(v)\n#define NUM2${_prefix}(v) NUM2ULL(v)\n")
    endif()
    string(APPEND _lines "#define PRI_${_prefix}_PREFIX PRI_LL_PREFIX\n")
  endif()

  set_property(GLOBAL APPEND PROPERTY RUBY_TYPEDEF_LINES "${_lines}")
endfunction()

ruby_typedef_conversions("pid_t" "PID_T" "rb_pid_t")
ruby_typedef_conversions("uid_t" "UID_T" "rb_uid_t")
ruby_typedef_conversions("gid_t" "GID_T" "rb_gid_t")
ruby_typedef_conversions("time_t" "TIME_T" "rb_time_t")
ruby_typedef_conversions("dev_t" "DEV_T" "rb_dev_t")
ruby_typedef_conversions("mode_t" "MODE_T" "rb_mode_t")
ruby_typedef_conversions("rlim_t" "RLIM_T" "rb_rlim_t")
ruby_typedef_conversions("off_t" "OFF_T" "rb_off_t")
ruby_typedef_conversions("clockid_t" "CLOCKID_T" "rb_clockid_t")

get_property(_typedef_lines GLOBAL PROPERTY RUBY_TYPEDEF_LINES)
# The property is a CMake list; its items contain no semicolons, so joining
# them back into one block of C is a simple replace.
string(REPLACE ";" "" _typedef_lines "${_typedef_lines}")
string(APPEND RUBY_TYPEDEF_DEFINES "${_typedef_lines}")

if(HAVE_GETGROUPS AND RUBY_HAVE_GID_T)
  string(APPEND RUBY_TYPEDEF_DEFINES "#define GETGROUPS_T gid_t\n")
endif()

# ---------------------------------------------------------------------------
# 6. struct members
# ---------------------------------------------------------------------------
ruby_c_check("int main(void){struct stat s;(void)s.st_blksize;return 0;}" HAVE_STRUCT_STAT_ST_BLKSIZE)
ruby_c_check("int main(void){struct stat s;(void)s.st_blocks;return 0;}" HAVE_STRUCT_STAT_ST_BLOCKS)
ruby_c_check("int main(void){struct stat s;(void)s.st_rdev;return 0;}" HAVE_STRUCT_STAT_ST_RDEV)
ruby_c_check("int main(void){struct stat s;(void)s.st_atim;return 0;}" HAVE_STRUCT_STAT_ST_ATIM)
ruby_c_check("int main(void){struct stat s;(void)s.st_mtim;return 0;}" HAVE_STRUCT_STAT_ST_MTIM)
ruby_c_check("int main(void){struct stat s;(void)s.st_ctim;return 0;}" HAVE_STRUCT_STAT_ST_CTIM)
ruby_c_check("int main(void){struct timeval t;(void)t.tv_sec;return 0;}" HAVE_STRUCT_TIMEVAL)
ruby_c_check("int main(void){struct timespec t;(void)t.tv_sec;return 0;}" HAVE_STRUCT_TIMESPEC)
ruby_c_check("int main(void){struct timezone t;(void)t;return 0;}" HAVE_STRUCT_TIMEZONE)
ruby_c_check("int main(void){struct tm t;(void)t.tm_zone;return 0;}" HAVE_STRUCT_TM_TM_ZONE)
ruby_c_check("int main(void){struct tm t;(void)t.tm_gmtoff;return 0;}" HAVE_STRUCT_TM_TM_GMTOFF)
ruby_c_check("int main(void){extern long timezone;extern int daylight;(void)timezone;(void)daylight;return 0;}" HAVE_VAR_TIMEZONE)
ruby_c_check("int main(void){return (int)sizeof(((struct stat*)0)->st_size);}" RUBY_CV_STAT_SIZE)
ruby_c_check("int main(void){struct statx s;(void)s.stx_btime;return 0;}" HAVE_STRUCT_STATX_STX_BTIME)

foreach(_cv
    HAVE_STRUCT_STAT_ST_BLKSIZE HAVE_STRUCT_STAT_ST_BLOCKS
    HAVE_STRUCT_STAT_ST_RDEV HAVE_STRUCT_STAT_ST_ATIM
    HAVE_STRUCT_STAT_ST_MTIM HAVE_STRUCT_STAT_ST_CTIM
    HAVE_STRUCT_TIMEVAL HAVE_STRUCT_TIMESPEC HAVE_STRUCT_TIMEZONE
    HAVE_STRUCT_TM_TM_ZONE HAVE_STRUCT_TM_TM_GMTOFF
    HAVE_STRUCT_STATX_STX_BTIME)
  if(${_cv})
    string(APPEND RUBY_FEATURE_DEFINES "#define ${_cv} 1\n")
  endif()
endforeach()

if(HAVE_VAR_TIMEZONE)
  string(APPEND RUBY_FEATURE_DEFINES "#define HAVE_TZNAME 1\n#define HAVE_DAYLIGHT 1\n")
endif()

# Sizes of struct stat members, as used by file.c.
function(ruby_member_size expr outvar fallback)
  string(MAKE_C_IDENTIFIER "${expr}" _id)
  ruby_check_sizeof("${expr}" RUBY_SIZEOF_MEMBER_${_id})
  set(_sz ${RUBY_SIZEOF_MEMBER_${_id}})
  if(_sz EQUAL 0)
    set(_sz ${fallback})
  endif()
  set(${outvar} ${_sz} PARENT_SCOPE)
endfunction()

if(HAVE_STRUCT_STAT_ST_BLOCKS)
  ruby_member_size("((struct stat*)0)->st_size"   _sz_st_size   ${RUBY_SIZEOF_OFF_T})
  ruby_member_size("((struct stat*)0)->st_blocks" _sz_st_blocks ${RUBY_SIZEOF_OFF_T})
  ruby_member_size("((struct stat*)0)->st_ino"    _sz_st_ino    ${RUBY_SIZEOF_LONG})
  ruby_member_size("((struct stat*)0)->st_dev"    _sz_st_dev    ${RUBY_SIZEOF_DEV_T})
  ruby_member_size("((struct stat*)0)->st_rdev"   _sz_st_rdev   ${RUBY_SIZEOF_DEV_T})
  string(APPEND RUBY_SIZEOF_DEFINES
"#define SIZEOF_STRUCT_STAT_ST_SIZE ${_sz_st_size}
#define SIZEOF_STRUCT_STAT_ST_BLOCKS ${_sz_st_blocks}
#define SIZEOF_STRUCT_STAT_ST_INO ${_sz_st_ino}
#define SIZEOF_STRUCT_STAT_ST_DEV ${_sz_st_dev}
#define SIZEOF_STRUCT_STAT_ST_RDEV ${_sz_st_rdev}
")
endif()
if(HAVE_STRUCT_TIMEVAL)
  ruby_member_size("((struct timeval*)0)->tv_sec" _sz_tv_sec ${RUBY_SIZEOF_TIME_T})
  string(APPEND RUBY_SIZEOF_DEFINES "#define SIZEOF_STRUCT_TIMEVAL_TV_SEC ${_sz_tv_sec}\n")
endif()

# ---------------------------------------------------------------------------
# 7. Compiler attributes
# ---------------------------------------------------------------------------
ruby_c_check("void f(void) __attribute__((noreturn)); void f(void){__builtin_trap();} int main(void){return 0;}" RUBY_CV_GCC_ATTR)
ruby_c_check("int main(void){return (int)__builtin_offsetof(struct {int a;}, a);}" RUBY_CV_OFFSETOF)

set(RUBY_ATTRIBUTE_DEFINES "")
if(RUBY_CV_GCC_ATTR)
  string(APPEND RUBY_ATTRIBUTE_DEFINES
"#define RUBY_SYMBOL_EXPORT_BEGIN _Pragma(\"GCC visibility push(default)\")
#define RUBY_SYMBOL_EXPORT_END _Pragma(\"GCC visibility pop\")
#define CONSTFUNC(x) __attribute__ ((__const__)) x
#define PUREFUNC(x) __attribute__ ((__pure__)) x
#define NORETURN(x) __attribute__ ((__noreturn__)) x
#define DEPRECATED(x) __attribute__ ((__deprecated__)) x
#define DEPRECATED_BY(n,x) __attribute__ ((__deprecated__(\"by \"#n))) x
#define NOINLINE(x) __attribute__ ((__noinline__)) x
#define ALWAYS_INLINE(x) __attribute__ ((__always_inline__)) x
#define NO_SANITIZE(san, x) __attribute__ ((__no_sanitize__(san))) x
#define NO_SANITIZE_ADDRESS(x) __attribute__ ((__no_sanitize_address__)) x
#define NO_ADDRESS_SAFETY_ANALYSIS(x) __attribute__ ((__no_address_safety_analysis__)) x
#define WARN_UNUSED_RESULT(x) __attribute__ ((__warn_unused_result__)) x
#define MAYBE_UNUSED(x) __attribute__ ((__unused__)) x
#define ERRORFUNC(mesg,x) __attribute__ ((__error__ mesg)) x
#define WARNINGFUNC(mesg,x) __attribute__ ((__warning__ mesg)) x
#define WEAK(x) __attribute__ ((__weak__)) x
#define FUNC_UNOPTIMIZED(x) __attribute__ ((__optimize__(\"O0\"))) x
#define FUNC_MINIMIZED(x) __attribute__ ((__optimize__(\"-Os\",\"-fomit-frame-pointer\"))) x
#define RUBY_FUNC_EXPORTED __attribute__ ((__visibility__(\"default\"))) extern
#define RUBY_FUNC_NONNULL(n,x) __attribute__ ((__nonnull__(n))) x
#define RUBY_FUNCTION_NAME_STRING __func__
#define RBIMPL_ATTR_PACKED_STRUCT_BEGIN()
#define RBIMPL_ATTR_PACKED_STRUCT_END() __attribute__((packed))
")
  if(HAVE_ATTRIBUTE_FUNCTION_ALIAS)
    string(APPEND RUBY_ATTRIBUTE_DEFINES
"#define RUBY_ALIAS_FUNCTION_TYPE(type, prot, name, args) type prot __attribute__((alias(#name)));
#define RUBY_ALIAS_FUNCTION_VOID(prot, name, args) RUBY_ALIAS_FUNCTION_TYPE(void, prot, name, args)
")
  endif()
else()
  # Conservative fallbacks: still build on a compiler without GNU attributes.
  string(APPEND RUBY_ATTRIBUTE_DEFINES
"#define RUBY_SYMBOL_EXPORT_BEGIN
#define RUBY_SYMBOL_EXPORT_END
#define CONSTFUNC(x) x
#define PUREFUNC(x) x
#define NORETURN(x) x
#define DEPRECATED(x) x
#define DEPRECATED_BY(n,x) x
#define NOINLINE(x) x
#define ALWAYS_INLINE(x) x
#define NO_SANITIZE(san, x) x
#define NO_SANITIZE_ADDRESS(x) x
#define NO_ADDRESS_SAFETY_ANALYSIS(x) x
#define WARN_UNUSED_RESULT(x) x
#define MAYBE_UNUSED(x) x
#define ERRORFUNC(mesg,x) x
#define WARNINGFUNC(mesg,x) x
#define WEAK(x) x
#define FUNC_UNOPTIMIZED(x) x
#define FUNC_MINIMIZED(x) x
#define RUBY_FUNC_EXPORTED extern
#define RUBY_FUNC_NONNULL(n,x) x
#define RUBY_FUNCTION_NAME_STRING __func__
#define RBIMPL_ATTR_PACKED_STRUCT_BEGIN()
#define RBIMPL_ATTR_PACKED_STRUCT_END()
")
endif()

# ---------------------------------------------------------------------------
# 8. Thread implementation
# ---------------------------------------------------------------------------
if(RUBY_THREAD_MODEL)
  # Already decided by a platform module (e.g. Windows).
elseif(Threads_FOUND AND HAVE_PTHREAD_H)
  set(RUBY_THREAD_MODEL "pthread")
  set(RUBY_THREAD_IMPL_H "thread_pthread.h")
  set(RUBY_THREAD_IMPL_SRC "thread_pthread.c")
  string(APPEND RUBY_FEATURE_DEFINES "#define _REENTRANT 1\n#define _THREAD_SAFE 1\n")
else()
  set(RUBY_THREAD_MODEL "none")
  set(RUBY_THREAD_IMPL_H "thread_none.h")
  set(RUBY_THREAD_IMPL_SRC "thread_none.c")
endif()

# ---------------------------------------------------------------------------
# 9. Native coroutine implementation
# ---------------------------------------------------------------------------
set(_coro "")
if(EMSCRIPTEN)
  # WebAssembly builds use the emscripten coroutine backend (see wasm/).
  set(_coro emscripten)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(x86_64|amd64|AMD64)$")
  set(_coro amd64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(i[3-6]86|x86)$")
  set(_coro x86)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(aarch64|arm64|ARM64)$")
  set(_coro arm64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(riscv64|riscv)$")
  set(_coro riscv64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ppc64le|powerpc64le)$")
  set(_coro ppc64le)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ppc64|powerpc64)$")
  set(_coro ppc64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(ppc|powerpc)$")
  set(_coro ppc)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(loongarch64)$")
  set(_coro loongarch64)
elseif(CMAKE_SYSTEM_PROCESSOR MATCHES "^(arm|armv7.*)$")
  set(_coro arm32)
endif()

if(RUBY_COROUTINE_SRC)
  # Already decided by a platform module (e.g. Windows); it has also added the
  # corresponding source file to RUBY_PLATFORM_SOURCES.
elseif(_coro AND EXISTS "${RUBY_SOURCE_DIR}/coroutine/${_coro}/Context.h")
  set(RUBY_COROUTINE_DIR "${_coro}")
  set(RUBY_COROUTINE_H "coroutine/${_coro}/Context.h")
  if(EXISTS "${RUBY_SOURCE_DIR}/coroutine/${_coro}/Context.c")
    set(RUBY_COROUTINE_SRC "coroutine/${_coro}/Context.c")
    list(APPEND RUBY_PLATFORM_SOURCES "${RUBY_SOURCE_DIR}/coroutine/${_coro}/Context.c")
  else()
    set(RUBY_COROUTINE_SRC "coroutine/${_coro}/Context.S")
    enable_language(ASM)
    list(APPEND RUBY_PLATFORM_SOURCES "${RUBY_SOURCE_DIR}/coroutine/${_coro}/Context.S")
  endif()
elseif(HAVE_GETCONTEXT AND HAVE_SETCONTEXT)
  set(RUBY_COROUTINE_DIR "ucontext")
  set(RUBY_COROUTINE_H "coroutine/ucontext/Context.h")
  set(RUBY_COROUTINE_SRC "coroutine/ucontext/Context.c")
  list(APPEND RUBY_PLATFORM_SOURCES "${RUBY_SOURCE_DIR}/coroutine/ucontext/Context.c")
else()
  set(RUBY_COROUTINE_DIR "pthread")
  set(RUBY_COROUTINE_H "coroutine/pthread/Context.h")
  set(RUBY_COROUTINE_SRC "coroutine/pthread/Context.c")
  list(APPEND RUBY_PLATFORM_SOURCES "${RUBY_SOURCE_DIR}/coroutine/pthread/Context.c")
endif()
string(APPEND RUBY_FEATURE_DEFINES "#define COROUTINE_H \"${RUBY_COROUTINE_H}\"\n")

# ---------------------------------------------------------------------------
# 10. Missing (replacement) implementations
# ---------------------------------------------------------------------------
foreach(_fn
    acosh cbrt erf explicit_bzero ffs flock hypot lgamma_r memcmp memmove nan
    nextafter setproctitle strchr strerror strlcat strlcpy strstr tgamma)
  string(TOUPPER "${_fn}" _up)
  if(NOT HAVE_${_up} AND EXISTS "${RUBY_SOURCE_DIR}/missing/${_fn}.c")
    list(APPEND RUBY_PLATFORM_SOURCES "${RUBY_SOURCE_DIR}/missing/${_fn}.c")
    message(STATUS "using missing/${_fn}.c")
  endif()
endforeach()

if(NOT HAVE_ALLOCA AND EXISTS "${RUBY_SOURCE_DIR}/missing/alloca.c")
  list(APPEND RUBY_PLATFORM_SOURCES "${RUBY_SOURCE_DIR}/missing/alloca.c")
endif()

# addr2line is only meaningful for ELF and Mach-O binaries.
if((HAVE_ELF_H OR HAVE_ELF_ABI_H) AND EXISTS "${RUBY_SOURCE_DIR}/addr2line.c")
  list(APPEND RUBY_PLATFORM_SOURCES "${RUBY_SOURCE_DIR}/addr2line.c")
  string(APPEND RUBY_FEATURE_DEFINES "#define USE_ELF 1\n")
endif()

# --- WebAssembly (Emscripten) ---------------------------------------------
# The sources are the ones listed in wasm/GNUmakefile.in.
if(EMSCRIPTEN)
  foreach(_w missing fiber machine setjmp runtime)
    if(EXISTS "${RUBY_SOURCE_DIR}/wasm/${_w}.c")
      list(APPEND RUBY_PLATFORM_SOURCES "${RUBY_SOURCE_DIR}/wasm/${_w}.c")
    endif()
  endforeach()

  # `rb_wasm_get_stack_pointer` reads the `__stack_pointer` global, which no C
  # function can do: the compiler's own prologue moves the pointer it would be
  # reading.  wasm/machine_core.S is the only definition in the tree, written in
  # the WebAssembly dialect clang assembles, so it has to be built as ASM.
  if(EXISTS "${RUBY_SOURCE_DIR}/wasm/machine_core.S")
    if(NOT CMAKE_ASM_COMPILER)
      set(CMAKE_ASM_COMPILER "${CMAKE_C_COMPILER}")
    endif()
    enable_language(ASM)
    set_source_files_properties("${RUBY_SOURCE_DIR}/wasm/machine_core.S"
      PROPERTIES LANGUAGE ASM)
    list(APPEND RUBY_PLATFORM_SOURCES "${RUBY_SOURCE_DIR}/wasm/machine_core.S")
  endif()

  # Two of the function probes above believe the sysroot's headers instead of
  # the runtime: `copy_file_range`, `lutimes` and `initgroups` are declared and
  # have no implementation, and the probe only takes their address, so nothing
  # has to link.  Ruby has a portable fallback for the first two and drops
  # `Process.initgroups` for the third, which is all a browser can offer -- it
  # has no user database to ask.
  foreach(_absent COPY_FILE_RANGE LUTIMES INITGROUPS)
    string(REGEX REPLACE "#define HAVE_${_absent} 1\n" ""
      RUBY_FEATURE_DEFINES "${RUBY_FEATURE_DEFINES}")
  endforeach()

  set(RUBY_THREAD_MODEL "none")
  set(RUBY_THREAD_IMPL_H "thread_none.h")
  set(RUBY_THREAD_IMPL_SRC "thread_none.c")
endif()

# ---------------------------------------------------------------------------
# 11. Miscellaneous defines
# ---------------------------------------------------------------------------
set(RUBY_MISC_DEFINES "")

string(APPEND RUBY_MISC_DEFINES
"#define THREAD_IMPL_H \"${RUBY_THREAD_IMPL_H}\"
#define THREAD_IMPL_SRC \"${RUBY_THREAD_IMPL_SRC}\"
#define RUBY_PLATFORM \"${RUBY_ARCH}\"
#define PRI_LL_PREFIX \"ll\"
#define PRI_SIZE_PREFIX \"z\"
#define PRI_PTRDIFF_PREFIX \"t\"
#define HAVE_DECL_SYS_NERR 0
#define HAVE_DECL_GETENV 1
#define HAVE_RB_FD_INIT 1
#define DLEXT_MAXLEN 3
")

if(APPLE)
  string(APPEND RUBY_MISC_DEFINES "#define DLEXT \"bundle\"\n#define SOEXT \"dylib\"\n#define LOAD_RELATIVE 1\n")
else()
  string(APPEND RUBY_MISC_DEFINES "#define DLEXT \"so\"\n#define SOEXT \"so\"\n")
endif()

if(HAVE_SIGACTION)
  string(APPEND RUBY_MISC_DEFINES "#define POSIX_SIGNAL 1\n")
endif()

# --- setjmp flavour --------------------------------------------------------
# Same preference order as tool/m4/ruby_setjmp_type.m4: _setjmp() is cheaper
# than setjmp() where it exists because it does not save the signal mask.
if(HAVE__SETJMP AND HAVE__LONGJMP)
  string(APPEND RUBY_MISC_DEFINES
"#define RUBY_SETJMP(env) _setjmp(env)
#define RUBY_LONGJMP(env,val) _longjmp(env,val)
#define RUBY_JMP_BUF jmp_buf
")
else()
  string(APPEND RUBY_MISC_DEFINES
"#define RUBY_SETJMP(env) setjmp(env)
#define RUBY_LONGJMP(env,val) longjmp(env,val)
#define RUBY_JMP_BUF jmp_buf
")
endif()

if(HAVE_UCONTEXT_H)
  # signal.c uses this to reach the machine context of a ucontext_t.
  string(APPEND RUBY_MISC_DEFINES
    "#define DEFINE_MCONTEXT_PTR(mc, uc) mcontext_t *mc = &(uc)->uc_mcontext\n")
endif()

# --- right shift behaviour -------------------------------------------------
ruby_c_check("int main(void){char a[(-1 == (-1 >> 1)) ? 1 : -1];(void)a;return 0;}" RUBY_CV_RSHIFT_SIGN)
if(RUBY_CV_RSHIFT_SIGN)
  string(APPEND RUBY_MISC_DEFINES "#define RSHIFT(x,y) ((x)>>(int)(y))\n")
else()
  string(APPEND RUBY_MISC_DEFINES
    "#define RSHIFT(x,y) (((x)<0) ? ~((~(x))>>(int)(y)) : (x)>>(int)(y))\n")
endif()

if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|amd64|i[3-6]86|x86|aarch64|arm64|arm|riscv|ppc|loongarch|s390")
  string(APPEND RUBY_MISC_DEFINES "#define STACK_GROW_DIRECTION -1\n")
endif()

if(HAVE_LIBZ)
  string(APPEND RUBY_MISC_DEFINES "#define HAVE_ZLIB 1\n")
endif()

if(RUBY_DISABLE_RUBYGEMS)
  # RubyGems is not part of a core-only build; loading it would require
  # default gems that live in ext/.
  string(APPEND RUBY_MISC_DEFINES "#define DISABLE_RUBYGEMS 1\n")
endif()

if(RUBY_USE_GMP_FINAL)
  string(APPEND RUBY_MISC_DEFINES "#define USE_GMP 1\n")
endif()

# Ruby needs to know where its own executable lives for load path setup.
if(APPLE)
  string(APPEND RUBY_MISC_DEFINES "#define LOAD_RELATIVE 1\n")
endif()

# ---------------------------------------------------------------------------
# 12. Generate config.h
# ---------------------------------------------------------------------------
file(MAKE_DIRECTORY "${RUBY_BINARY_DIR}/include/ruby")
configure_file("${CMAKE_CURRENT_LIST_DIR}/config.h.in"
               "${RUBY_BINARY_DIR}/include/ruby/config.h"
               @ONLY)

# probes.h is normally produced by tool/gen_dummy_probes.rb (or dtrace).
# DTrace is not wired into this build, so a no-op header is used instead.
configure_file("${CMAKE_CURRENT_LIST_DIR}/dummy_probes.h"
               "${RUBY_BINARY_DIR}/probes.h"
               COPYONLY)

message(STATUS "config.h written to ${RUBY_BINARY_DIR}/include/ruby/config.h")
