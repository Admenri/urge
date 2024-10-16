/*
** Fiber - Header only cross platform wrapper of fiber API
**
** For the latest info, see https://github.com/paladin-t/fiber/
**
** Copyright (C) 2017 - 2018 Wang Renxin
**
** Just #include "fiber.h" before using this library.
**
** Permission is hereby granted, free of charge, to any person obtaining a copy of
** this software and associated documentation files (the "Software"), to deal in
** the Software without restriction, including without limitation the rights to
** use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
** the Software, and to permit persons to whom the Software is furnished to do so,
** subject to the following conditions:
**
** The above copyright notice and this permission notice shall be included in all
** copies or substantial portions of the Software.
**
** THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
** IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
** FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
** COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
** IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
** CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __FIBER_H__
#define __FIBER_H__

#if defined __EMSCRIPTEN__
#	define FB_OS_EMSCRIPTEN
#elif defined _WIN64
#	define FB_OS_WIN
#	define FB_OS_WIN64
#elif defined _WIN32
#	define FB_OS_WIN
#	define FB_OS_WIN32
#elif defined __APPLE__
#	include <TargetConditionals.h>
#	define FB_OS_APPLE
#	if defined TARGET_OS_IPHONE && TARGET_OS_IPHONE == 1
#		define FB_OS_IOS
#	elif defined TARGET_IPHONE_SIMULATOR && TARGET_IPHONE_SIMULATOR == 1
#		define FB_OS_IOS_SIM
#	elif defined TARGET_OS_MAC && TARGET_OS_MAC == 1
#		define FB_OS_MAC
#	endif
#elif defined __ANDROID__
#	define FB_OS_ANDROID
#elif defined __linux__
#	define FB_OS_LINUX
#elif defined __unix__
#	define FB_OS_UNIX
#else
#	define FB_OS_UNKNOWN
#endif

#include <memory.h>
#include <stdint.h>
#include <stdlib.h>
#if defined FB_OS_EMSCRIPTEN
#	include <emscripten.h>
#elif defined FB_OS_WIN
#	include <Windows.h>
#elif defined FB_OS_APPLE
	/* The `context` API was deprecated on Apple platforms, so we need an alternative. */
#	ifdef __cplusplus
	extern "C" {
#	include "taskimpl.h"
	}
#	else /* __cplusplus */
#	include "taskimpl.h"
#	endif /* __cplusplus */
#else /* Backend macro. */
#	include <ucontext.h>
#endif /* Backend macro. */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/*
** {========================================================
** Declaration
*/

#if INTPTR_MAX == INT32_MAX
#	define FB32BITENV
#elif INTPTR_MAX == INT64_MAX
#	define FB64BITENV
#else
#	error "Environment not 32 or 64-bit."
#endif

#if defined FB_OS_APPLE && defined FB64BITENV
#	define FBSEPCMP
#endif /* FB_OS_APPLE && FB64BITENV */

#ifndef FBAPI
#	define FBAPI
#endif /* FBAPI */

#ifndef FBIMPL
#	define FBIMPL
#endif /* FBIMPL */

#ifndef bool_t
#	define bool_t unsigned char
#endif /* bool_t */
#ifndef __cplusplus
#	ifndef true
#		define true (1)
#	endif /* true */
#	ifndef false
#		define false (0)
#	endif /* false */
#endif /* __cplusplus */

/* Redefine `fballoc` and `fbfree` to use other allocator. */
#ifndef fballoc
#	define fballoc malloc
#endif /* fballoc */
#ifndef fbfree
#	define fbfree free
#endif /* fbfree */

/* The stack size defaults to 1MB. */
#ifndef FIBER_STACK_SIZE
#	define FIBER_STACK_SIZE (1024 * 1024)
#endif /* FIBER_STACK_SIZE */

struct fiber_t;

/**
 * @brief Fiber processor, will callback to this.
 *
 * @param[in] fb - On which fiber is calling.
  */
typedef void (* fiber_proc)(struct fiber_t* /*fb*/);

/**
 * @brief Fiber structure.
 */
typedef struct fiber_t {
	/**< Current fiber, all fibers within the same thread should share the same "current" pointer. */
	struct fiber_t** current;
	/**< How much memory is reserved for this fiber. */
	size_t stack_size;
	/**< Raw context of this fiber. */
	void* context;
	/**< Processer of this fiber, as callback. */
	fiber_proc proc;
	/**< Fiber carries some extra data with `userdata`. */
	void* userdata;
} fiber_t;

/**
 * @brief Checks whether a fiber is the current active one.
 *
 * @param[in] fb - Specific fiber to check with.
 * @return - True if `fb` is the current active fiber.
 */
FBAPI static bool_t fiber_is_current(const fiber_t* const fb);
/**
 * @brief Creates a fiber. Should create the primary fiber before creating minor fibers.
 *
 * @param[in] primary - The original primary fiber, pass `NULL` if it's the primary fiber to be created.
 * @param[in] stack - Reserved stack size for this fiber, pass 0 if it's the primary fiber to be created,
 *                    or uses `FIBER_STACK_SIZE` for minor fibers.
 * @param[in] run - Fiber processor.
 * @param[in] userdata - Userdata of fiber.
 * @return - Created fiber.
 */
FBAPI static fiber_t* fiber_create(fiber_t* primary, size_t stack, fiber_proc run, void* userdata);
/**
 * @brief Deletes a fiber. Should delete minor fibers before deleting the primary fiber.
 *
 * @param[in] fb - The fiber to be deleted.
 * @return - True for succeed.
 */
FBAPI static bool_t fiber_delete(fiber_t* fb);
/**
 * @brief Switches to a specific fiber. The `fiber_proc proc` processer will be called.
 *
 * @param[in] fb - The target fiber to switch to.
 * @return - True for succeed. This function returns when some fiber switches back here.
 */
FBAPI static bool_t fiber_switch(fiber_t* fb);

/* ========================================================} */

/*
** {========================================================
** Implementation
*/

#if defined FB_OS_EMSCRIPTEN

typedef struct emscripten_context_t {
	fiber_t* primary;
	union {
		emscripten_coroutine emco;
		fiber_t* next;
	};
} emscripten_context_t;

FBIMPL static void fiber_proc_impl(fiber_t* fb) {
	if (!fb) return;

	fb->proc(fb);
}

FBAPI static bool_t fiber_is_current(const fiber_t* const fb) {
	if (!fb) return false;

	return *fb->current == fb;
}

FBAPI static fiber_t* fiber_create(fiber_t* primary, size_t stack, fiber_proc run, void* userdata) {
	fiber_t* ret = 0;
	emscripten_context_t* ctx = 0;
	if ((!primary && run) || (primary && !run)) return ret;
	ret = (fiber_t*)fballoc(sizeof(fiber_t));
	ret->proc = run;
	ret->userdata = userdata;
	if (primary && run) {
		if (!stack) stack = FIBER_STACK_SIZE;
		ret->current = primary->current;
		ret->stack_size = stack;
		ctx = (emscripten_context_t*)fballoc(sizeof(emscripten_context_t));
		memset(ctx, 0, sizeof(emscripten_context_t));
		ctx->primary = primary;
		ctx->emco = emscripten_coroutine_create((em_arg_callback_func)fiber_proc_impl, ret, stack);
		ret->context = ctx;
	} else {
		ret->current = (fiber_t**)fballoc(sizeof(fiber_t*));
		*ret->current = ret;
		ret->stack_size = 0;
		ctx = (emscripten_context_t*)fballoc(sizeof(emscripten_context_t));
		memset(ctx, 0, sizeof(emscripten_context_t));
		ctx->primary = ret;
		ret->context = ctx;
	}

	return ret;
}

FBAPI static bool_t fiber_delete(fiber_t* fb) {
	emscripten_context_t* ctx = 0;
	if (!fb) return false;
	ctx = (emscripten_context_t*)fb->context;
	if (fb->proc) {
		// Using `free` to clean up the emscripten coroutine, this is an undocumented usage,
		// see https://github.com/kripken/emscripten/issues/6540 for discussion.
		free(ctx->emco);
		fbfree(ctx);
	} else {
		fbfree(ctx);
		fbfree(fb->current);
	}
	fbfree(fb);

	return true;
}

FBAPI static bool_t fiber_switch(fiber_t* fb) {
	fiber_t* prev = 0;
	fiber_t* primary = 0;
	emscripten_context_t* ctx = 0;
	if (!fb) return false;
	prev = *fb->current;
	*fb->current = fb;
	primary = (fiber_t*)((emscripten_context_t*)fb->context)->primary;
	ctx = (emscripten_context_t*)primary->context;
	ctx->next = fb;
	if (prev->proc) {
		emscripten_yield();
	} else {
		while (ctx->next && ctx->next != primary) {
			emscripten_context_t* to = (emscripten_context_t*)ctx->next->context;
			*fb->current = ctx->next;
			emscripten_coroutine_next(to->emco);
		}
	}

	return true;
}

#elif defined FB_OS_WIN

typedef LPFIBER_START_ROUTINE FiberRoutine;

#ifndef WINAPI
#	define WINAPI
#endif /* WINAPI */

#ifndef FIBERAPI
#	define FIBERAPI WINAPI
#endif /* FIBERAPI */

FBIMPL static void FIBERAPI fiber_proc_impl(fiber_t* fb) {
	if (!fb) return;

	fb->proc(fb);
}

FBAPI static bool_t fiber_is_current(const fiber_t* const fb) {
	if (!fb) return false;

	return *fb->current == fb;
}

FBAPI static fiber_t* fiber_create(fiber_t* primary, size_t stack, fiber_proc run, void* userdata) {
	fiber_t* ret = 0;
	if ((!primary && run) || (primary && !run)) return ret;
	ret = (fiber_t*)fballoc(sizeof(fiber_t));
	ret->proc = run;
	ret->userdata = userdata;
	if (primary && run) {
		if (!stack) stack = FIBER_STACK_SIZE;
		ret->current = primary->current;
		ret->stack_size = stack;
		ret->context = CreateFiber(stack, (FiberRoutine)fiber_proc_impl, ret);
	} else {
		ret->current = (fiber_t**)fballoc(sizeof(fiber_t*));
		*ret->current = ret;
		ret->stack_size = 0;
		ret->context = ConvertThreadToFiber(0);
	}

	return ret;
}

FBAPI static bool_t fiber_delete(fiber_t* fb) {
	if (!fb) return false;
	if (fb->proc)
		DeleteFiber(fb->context);
	else
		fbfree(fb->current);
	fbfree(fb);

	return true;
}

FBAPI static bool_t fiber_switch(fiber_t* fb) {
	if (!fb) return false;
	*fb->current = fb;
	SwitchToFiber(fb->context);

	return true;
}

#else /* Backend macro. */

#if defined FBSEPCMP
FBIMPL static void fiber_proc_impl(unsigned fst, unsigned scd) {
	uintptr_t ptr = ((uintptr_t)fst) | ((uintptr_t)scd << 32);
	fiber_t* fb = (fiber_t*)ptr;
	if (!fb) return;

	fb->proc(fb);
}
#else /* FBSEPCMP */
FBIMPL static void fiber_proc_impl(fiber_t* fb) {
	if (!fb) return;

	fb->proc(fb);
}
#endif /* FBSEPCMP */

FBAPI static bool_t fiber_is_current(const fiber_t* const fb) {
	if (!fb) return false;

	return *fb->current == fb;
}

FBAPI static fiber_t* fiber_create(fiber_t* primary, size_t stack, fiber_proc run, void* userdata) {
	fiber_t* ret = 0;
	ucontext_t* ctx = 0;
	if ((!primary && run) || (primary && !run)) return ret;
	ret = (fiber_t*)fballoc(sizeof(fiber_t));
	ret->proc = run;
	ret->userdata = userdata;
	if (primary && run) {
		if (!stack) stack = FIBER_STACK_SIZE;
		ctx = (ucontext_t*)fballoc(sizeof(ucontext_t));
		memset(ctx, 0, sizeof(ucontext_t));
		getcontext(ctx);
		ctx->uc_stack.ss_sp = fballoc(stack);
		ctx->uc_stack.ss_size = stack;
		ctx->uc_stack.ss_flags = 0;
#if defined FBSEPCMP
		do {
			uintptr_t fst = (uintptr_t)ret;
			uintptr_t scd = (uintptr_t)ret;
			fst &= 0x00000000ffffffff;
			scd >>= 32;
			makecontext(ctx, (void(*)())fiber_proc_impl, 2, (unsigned)fst, (unsigned)scd);
		} while (0);
#else /* FBSEPCMP */
		makecontext(ctx, (void(*)())fiber_proc_impl, 1, ret);
#endif /* FBSEPCMP */
		ret->current = primary->current;
		ret->stack_size = stack;
		ret->context = ctx;
	} else {
		ret->current = (fiber_t**)fballoc(sizeof(fiber_t*));
		ctx = (ucontext_t*)fballoc(sizeof(ucontext_t));
		memset(ctx, 0, sizeof(ucontext_t));
		*ret->current = ret;
		ret->stack_size = 0;
		ret->context = ctx;
	}

	return ret;
}

FBAPI static bool_t fiber_delete(fiber_t* fb) {
	if (!fb) return false;
	if (fb->proc) {
		ucontext_t* ctx = (ucontext_t*)fb->context;
		fbfree(ctx->uc_stack.ss_sp);
	} else {
		fbfree(fb->current);
	}
	fbfree(fb->context);
	fbfree(fb);

	return true;
}

FBAPI static bool_t fiber_switch(fiber_t* fb) {
	fiber_t* curr = 0;
	if (!fb) return false;
	curr = *fb->current; 
	*fb->current = fb;
	swapcontext((ucontext_t*)curr->context, (ucontext_t*)fb->context);

	return true;
}

#endif /* Backend macro. */

/* ========================================================} */

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* __FIBER_H__ */
