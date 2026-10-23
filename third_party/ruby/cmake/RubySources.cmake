# ---------------------------------------------------------------------------
# RubySources.cmake -- source file lists
#
# Mirrors COMMONOBJS in common.mk.  Files that the original build compiles by
# #include-ing them from another translation unit (eval_error.c, vm_eval.c,
# thread_pthread.c, ...) must NOT be listed here.
# ---------------------------------------------------------------------------

set(RUBY_CORE_SOURCES
  # --- core -------------------------------------------------------------
  array.c
  ast.c
  bignum.c
  box.c
  class.c
  compar.c
  compile.c
  complex.c
  concurrent_set.c
  cont.c
  debug.c
  debug_counter.c
  dir.c
  dln_find.c
  encoding.c
  enum.c
  enumerator.c
  error.c
  eval.c
  file.c
  gc.c
  hash.c
  imemo.c
  inits.c
  io.c
  io_buffer.c
  iseq.c
  load.c
  marshal.c
  math.c
  memory_view.c
  node.c
  node_dump.c
  numeric.c
  object.c
  pack.c
  pathname.c
  parse.c
  parser_st.c
  proc.c
  process.c
  ractor.c
  random.c
  range.c
  rational.c
  re.c
  regcomp.c
  regenc.c
  regerror.c
  regexec.c
  regparse.c
  regsyntax.c
  ruby.c
  ruby_parser.c
  scheduler.c
  set.c
  shape.c
  signal.c
  sprintf.c
  st.c
  strftime.c
  string.c
  struct.c
  symbol.c
  thread.c
  time.c
  transcode.c
  util.c
  variable.c
  version.c
  vm.c
  vm_backtrace.c
  vm_dump.c
  vm_sync.c
  vm_trace.c
  weakmap.c
)

# --- Prism parser (PRISM_FILES in common.mk) -------------------------------
set(RUBY_PRISM_SOURCES
  prism/api_node.c
  prism/api_pack.c
  prism/diagnostic.c
  prism/encoding.c
  prism/extension.c
  prism/node.c
  prism/options.c
  prism/pack.c
  prism/prettyprint.c
  prism/regexp.c
  prism/serialize.c
  prism/static_literals.c
  prism/token_type.c
  prism/util/pm_buffer.c
  prism/util/pm_char.c
  prism/util/pm_constant_pool.c
  prism/util/pm_integer.c
  prism/util/pm_list.c
  prism/util/pm_memchr.c
  prism/util/pm_newline_list.c
  prism/util/pm_string.c
  prism/util/pm_strncasecmp.c
  prism/util/pm_strpbrk.c
  prism/prism.c
  prism_init.c
)

# --- built-in encodings (BUILTIN_ENCOBJS in enc/Makefile.in) ---------------
# Everything else in enc/ is a dynamically loadable encoding in the original
# build; this build is core-only, so only the built-ins are compiled in.
set(RUBY_ENC_SOURCES
  enc/ascii.c
  enc/us_ascii.c
  enc/unicode.c
  enc/utf_8.c
  enc/encdb.c
)

# --- built-in transcoders (BUILTIN_TRANSES in enc/Makefile.in) -------------
set(RUBY_TRANS_SOURCES
  enc/trans/newline.c
)

list(APPEND RUBY_CORE_SOURCES
  ${RUBY_PRISM_SOURCES}
  ${RUBY_ENC_SOURCES}
  ${RUBY_TRANS_SOURCES})

list(TRANSFORM RUBY_CORE_SOURCES PREPEND "${RUBY_SOURCE_DIR}/")
list(TRANSFORM RUBY_ENC_SOURCES PREPEND "${RUBY_SOURCE_DIR}/")

# Files provided by the platform module (coroutine, thread backend, missing/).
set(RUBY_PLATFORM_SOURCES "")

# Ruby scripts that are compiled into the interpreter ("builtin" features).
# The list comes from the original build: the core .rb files next to the .c
# files they belong to.
set(RUBY_BUILTIN_RB_SRCS
  "${RUBY_SOURCE_DIR}/ast.rb"
  "${RUBY_SOURCE_DIR}/dir.rb"
  "${RUBY_SOURCE_DIR}/gc.rb"
  "${RUBY_SOURCE_DIR}/numeric.rb"
  "${RUBY_SOURCE_DIR}/io.rb"
  "${RUBY_SOURCE_DIR}/marshal.rb"
  "${RUBY_SOURCE_DIR}/pack.rb"
  "${RUBY_SOURCE_DIR}/trace_point.rb"
  "${RUBY_SOURCE_DIR}/warning.rb"
  "${RUBY_SOURCE_DIR}/array.rb"
  "${RUBY_SOURCE_DIR}/hash.rb"
  "${RUBY_SOURCE_DIR}/kernel.rb"
  "${RUBY_SOURCE_DIR}/pathname_builtin.rb"
  "${RUBY_SOURCE_DIR}/ractor.rb"
  "${RUBY_SOURCE_DIR}/symbol.rb"
  "${RUBY_SOURCE_DIR}/timev.rb"
  "${RUBY_SOURCE_DIR}/thread_sync.rb"
  "${RUBY_SOURCE_DIR}/nilclass.rb"
  "${RUBY_SOURCE_DIR}/prelude.rb"
  "${RUBY_SOURCE_DIR}/gem_prelude.rb"
  "${RUBY_SOURCE_DIR}/jit_hook.rb"
  "${RUBY_SOURCE_DIR}/jit_undef.rb"
  "${RUBY_SOURCE_DIR}/yjit.rb"
  "${RUBY_SOURCE_DIR}/zjit.rb"
)
