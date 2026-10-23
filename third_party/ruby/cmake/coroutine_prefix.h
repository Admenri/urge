/*
 * coroutine_prefix.h -- injected (with -include) before the coroutine
 * assembly sources.
 *
 * template/Makefile.in passes
 *     -DPREFIXED_SYMBOL(name)=name
 * (or _name on Mach-O) on the command line.  A header is used instead so the
 * definition does not have to survive shell/CMake quoting.
 */
#ifndef RUBY_COROUTINE_PREFIX_H
#define RUBY_COROUTINE_PREFIX_H

#if defined(__APPLE__)
# define PREFIXED_SYMBOL(name) _##name
#else
# define PREFIXED_SYMBOL(name) name
#endif

#endif /* RUBY_COROUTINE_PREFIX_H */
