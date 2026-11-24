//===----------------------------------------------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//
//  This file implements the storage for the "Caught Exception Stack"
//  https://itanium-cxx-abi.github.io/cxx-abi/abi-eh.html#cxx-exc-stack
//
//===----------------------------------------------------------------------===//

#include "cxa_exception.h"

#include <__thread/support.h>

#if defined(_LIBCXXABI_HAS_NO_THREADS) && defined(__I86__) && defined(__WINDOWS__)

#include "abort_message.h"

// 16-bit Windows: a DLL runs on the stack of each task that calls it, and
// one task may yield (in GetMessage, for example) inside a catch handler
// while another task throws in the same DLL. So each task, found by its
// stack segment, has the globals of its own. A slot with no exception in
// it is free for any task.
namespace __cxxabiv1 {
namespace {
    const int kTasks = 8;
    struct TaskGlobals {
        unsigned short ss;
        __cxa_eh_globals globals;
    };
    TaskGlobals task_globals[kTasks];

    /// Returns true if \p g holds no exception.
    bool isIdle(const __cxa_eh_globals &g) {
        return g.caughtExceptions == nullptr && g.uncaughtExceptions == 0;
    }

    /// Returns the globals of the current task, from the slot of its stack
    /// segment, or from a free slot.
    __cxa_eh_globals *taskGlobals() {
        unsigned short ss;
        __asm__("movw %%ss, %0" : "=r"(ss));
        for (TaskGlobals &t : task_globals)
            if (t.ss == ss && !isIdle(t.globals))
                return &t.globals;
        for (TaskGlobals &t : task_globals)
            if (isIdle(t.globals)) {
                t.ss = ss;
                return &t.globals;
            }
        __abort_message("%s", "too many tasks are handling C++ exceptions");
    }
} // namespace

extern "C" {
    __cxa_eh_globals *__cxa_get_globals() { return taskGlobals(); }
    __cxa_eh_globals *__cxa_get_globals_fast() { return taskGlobals(); }
} // extern "C"
} // namespace __cxxabiv1

#elif defined(_LIBCXXABI_HAS_NO_THREADS)

namespace __cxxabiv1 {
extern "C" {
    static __cxa_eh_globals eh_globals;
    __cxa_eh_globals *__cxa_get_globals() { return &eh_globals; }
    __cxa_eh_globals *__cxa_get_globals_fast() { return &eh_globals; }
} // extern "C"
} // namespace __cxxabiv1

#elif __has_feature(cxx_thread_local)

namespace __cxxabiv1 {
namespace {
    __cxa_eh_globals *__globals() {
        static thread_local __cxa_eh_globals eh_globals;
        return &eh_globals;
    }
} // namespace

extern "C" {
    __cxa_eh_globals *__cxa_get_globals() { return __globals(); }
    __cxa_eh_globals *__cxa_get_globals_fast() { return __globals(); }
} // extern "C"
} // namespace __cxxabiv1

#else

#include "abort_message.h"
#include "fallback_malloc.h"

#if defined(__ELF__) && defined(_LIBCXXABI_LINK_PTHREAD_LIB)
#pragma comment(lib, "pthread")
#endif

//  In general, we treat all threading errors as fatal.
//  We cannot call std::terminate() because that will in turn
//  call __cxa_get_globals() and cause infinite recursion.

namespace __cxxabiv1 {
namespace {
    std::__libcpp_tls_key key_;
    constinit std::__libcpp_exec_once_flag flag_ = _LIBCPP_EXEC_ONCE_INITIALIZER;

    void _LIBCPP_TLS_DESTRUCTOR_CC destruct_(void *p) {
        __free_with_fallback(p);
        if (0 != std::__libcpp_tls_set(key_, NULL))
            __abort_message("cannot zero out thread value for __cxa_get_globals()");
    }

    void construct_() {
        if (0 != std::__libcpp_tls_create(&key_, destruct_))
            __abort_message("cannot create thread specific key for __cxa_get_globals()");
    }
} // namespace

extern "C" {
    __cxa_eh_globals *__cxa_get_globals() {
        // Try to get the globals for this thread
        __cxa_eh_globals *retVal = __cxa_get_globals_fast();

        // If this is the first time we've been asked for these globals, create them
        if (NULL == retVal) {
            retVal = static_cast<__cxa_eh_globals*>(
                __calloc_with_fallback(1, sizeof(__cxa_eh_globals)));
            if (NULL == retVal)
                __abort_message("cannot allocate __cxa_eh_globals");
            if (0 != std::__libcpp_tls_set(key_, retVal))
               __abort_message("std::__libcpp_tls_set failure in __cxa_get_globals()");
        }
        return retVal;
    }

    // Note that this implementation will reliably return NULL if not
    // preceded by a call to __cxa_get_globals().  This is an extension
    // to the Itanium ABI and is taken advantage of in several places in
    // libc++abi.
    __cxa_eh_globals *__cxa_get_globals_fast() {
        // First time through, create the key.
        if (0 != std::__libcpp_execute_once(&flag_, construct_))
            __abort_message("execute once failure in __cxa_get_globals_fast()");
        return static_cast<__cxa_eh_globals*>(std::__libcpp_tls_get(key_));
    }
} // extern "C"
} // namespace __cxxabiv1

#endif
