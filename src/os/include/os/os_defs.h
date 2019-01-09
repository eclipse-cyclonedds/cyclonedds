/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef OS_DEFS_H
#define OS_DEFS_H

#define OS_LITTLE_ENDIAN 1
#define OS_BIG_ENDIAN 2

#if OS_ENDIANNESS != OS_LITTLE_ENDIAN && OS_ENDIANNESS != OS_BIG_ENDIAN
#error "OS_ENDIANNESS not set correctly"
#endif

#include "os/os_decl_attributes.h"

#if defined (__cplusplus)
extern "C" {
#endif

    /* \brief OS_FUNCTION provides undecorated function name of current function
     *
     * Behavior of OS_FUNCTION outside a function is undefined. Note that
     * implementations differ across compilers and compiler versions. It might be
     * implemented as either a string literal or a constant variable.
     */
#if defined(__STDC_VERSION__) && (__STDC_VERSION__ >= 199901)
#   define OS_FUNCTION __func__
#elif defined(__cplusplus) && (__cplusplus >= 201103)
#   define OS_FUNCTION __func__
#elif defined(__GNUC__)
#   define OS_FUNCTION __FUNCTION__
#elif defined(__clang__)
#   define OS_FUNCTION __FUNCTION__
#elif defined(__ghs__)
#   define OS_FUNCTION __FUNCTION__
#elif (defined(__SUNPRO_C) || defined(__SUNPRO_CC))
    /* Solaris Studio had support for __func__ before it supported __FUNCTION__.
       Compiler flag -features=extensions is required on older versions. */
#   define OS_FUNCTION __func__
#elif defined(__FUNCTION__)
    /* Visual Studio */
#   define OS_FUNCTION __FUNCTION__
#elif defined(__vxworks)
    /* At least versions 2.9.6 and 3.3.4 of the GNU C Preprocessor only define
       __GNUC__ if the entire GNU C compiler is in use. VxWorks 5.5 targets invoke
       the preprocessor separately resulting in __GNUC__ not being defined. */
#   define OS_FUNCTION __FUNCTION__
#else
#   warning "OS_FUNCTION is not supported"
#endif

    /* \brief OS_PRETTY_FUNCTION provides function signature of current function
     *
     * See comments on OS_FUNCTION for details.
     */
#if defined(__GNUC__)
#   define OS_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(__clang__)
#   define OS_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(__ghs__)
#   define OS_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif (defined(__SUNPRO_C) && __SUNPRO_C >= 0x5100)
    /* Solaris Studio supports __PRETTY_FUNCTION__ in C since version 12.1 */
#   define OS_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif (defined(__SUNPRO_CC) && __SUNPRO_CC >= 0x5120)
    /* Solaris Studio supports __PRETTY_FUNCTION__ in C++ since version 12.3 */
#   define OS_PRETTY_FUNCTION __PRETTY_FUNCTION__
#elif defined(__FUNCSIG__)
    /* Visual Studio */
#   define OS_PRETTY_FUNCTION __FUNCSIG__
#elif defined(__vxworks)
    /* See comments on __vxworks macro above. */
#   define OS_PRETTY_FUNCTION __PRETTY_FUNCTION__
#else
    /* Do not warn user about OS_PRETTY_FUNCTION falling back to OS_FUNCTION.
       #   warning "OS_PRETTY_FUNCTION is not supported, using OS_FUNCTION"
    */
#   define OS_PRETTY_FUNCTION OS_FUNCTION
#endif

#if defined(__GNUC__)
#if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 402
#define OSPL_GCC_DIAG_STR(s) #s
#define OSPL_GCC_DIAG_JOINSTR(x,y) OSPL_GCC_DIAG_STR(x ## y)
#define OSPL_GCC_DIAG_DO_PRAGMA(x) _Pragma (#x)
#define OSPL_GCC_DIAG_PRAGMA(x) OSPL_GCC_DIAG_DO_PRAGMA(GCC diagnostic x)
#if ((__GNUC__ * 100) + __GNUC_MINOR__) >= 406
#define OS_WARNING_GNUC_OFF(x) OSPL_GCC_DIAG_PRAGMA(push) OSPL_GCC_DIAG_PRAGMA(ignored OSPL_GCC_DIAG_JOINSTR(-W,x))
#define OS_WARNING_GNUC_ON(x) OSPL_GCC_DIAG_PRAGMA(pop)
#else
#define OS_WARNING_GNUC_OFF(x) OSPL_GCC_DIAG_PRAGMA(ignored OSPL_GCC_DIAG_JOINSTR(-W,x))
#define OS_WARNING_GNUC_ON(x)  OSPL_GCC_DIAG_PRAGMA(warning OSPL_GCC_DIAG_JOINSTR(-W,x))
#endif
#else
#define OS_WARNING_GNUC_OFF(x)
#define OS_WARNING_GNUC_ON(x)
#endif
#else
#define OS_WARNING_GNUC_OFF(x)
#define OS_WARNING_GNUC_ON(x)
#endif

#ifdef _MSC_VER

#define OS_WARNING_MSVC_OFF(x)               \
__pragma (warning(push))             \
__pragma (warning(disable: ## x))

#define OS_WARNING_MSVC_ON(x)               \
__pragma (warning(pop))
#else
#define OS_WARNING_MSVC_OFF(x)
#define OS_WARNING_MSVC_ON(x)
#endif

/**
 * \brief Calculate maximum value of an integer type
 *
 * A somewhat complex, but efficient way to calculate the maximum value of an
 * integer type at compile time.
 *
 * For unsigned numerical types the first part up to XOR is enough. The second
 * part is to make up for signed numerical types.
 */
#define OS_MAX_INTEGER(T) \
    ((T)(((T)~0) ^ ((T)!((T)~0 > 0) << (CHAR_BIT * sizeof(T) - 1))))
/**
 * \brief Calculate minimum value of an integer type
 */
#define OS_MIN_INTEGER(T) \
    ((-OS_MAX_INTEGER(T)) - 1)

#if !defined (OS_UNUSED_ARG)
#define OS_UNUSED_ARG(a) (void) (a)
#endif

    /** \brief Time structure definition
     */
    typedef struct os_time {
        /** Seconds since the Unix epoch; 1-jan-1970 00:00:00 (UTC) */
        os_timeSec tv_sec;
        /** Number of nanoseconds since the Unix epoch, modulo 10^9. */
        int32_t tv_nsec;
        /** os_time can be used for a duration type with the following
            semantics for negative durations: tv_sec specifies the
            sign of the duration, tv_nsec is always positive and added
            to the real value (thus real value is tv_sec+tv_nsec/10^9,
            for example { -1, 500000000 } is -0.5 seconds) */
    } os_time;

    /** \brief Types on which we define atomic operations.  The 64-bit
     *  types are always defined, even if we don't really support atomic
     *   operations on them.
     */
    typedef struct { uint32_t v; } os_atomic_uint32_t;
    typedef struct { uint64_t v; } os_atomic_uint64_t;
    typedef struct { uintptr_t v; } os_atomic_uintptr_t;
    typedef os_atomic_uintptr_t os_atomic_voidp_t;

    /** \brief Initializers for the types on which atomic operations are
        defined.
    */
#define OS_ATOMIC_UINT32_INIT(v) { (v) }
#define OS_ATOMIC_UINT64_INIT(v) { (v) }
#define OS_ATOMIC_UINTPTR_INIT(v) { (v) }
#define OS_ATOMIC_VOIDP_INIT(v) { (uintptr_t) (v) }

    /** \brief Definition of the service return values */
    typedef _Return_type_success_(return == os_resultSuccess) enum os_result {
        /** The service is successfully completed */
        os_resultSuccess,
        /** A resource was not found */
        os_resultUnavailable,
        /** The service is timed out */
        os_resultTimeout,
        /** The requested resource is busy */
        os_resultBusy,
        /** An invalid argument is passed */
        os_resultInvalid,
        /** The operating system returned a failure */
        os_resultFail
    } os_result;

#if defined(_MSC_VER)
    /* Thread-local storage using __declspec(thread) on Windows versions before
       Vista and Server 2008 works in DLLs if they are bound to the executable,
       it does not work if the library is loaded using LoadLibrary. */
#define os_threadLocal __declspec(thread)
#elif defined(__GNUC__) || (defined(__clang__) && __clang_major__ >= 2)
    /* GCC supports Thread-local storage for x86 since version 3.3. Clang
       supports Thread-local storage since version 2.0. */
    /* VxWorks 7 supports __thread for both GCC and DIAB, older versions may
       support it as well, but that is not verified. */
#define os_threadLocal __thread
#elif defined(__SUNPRO_C) || defined(__SUNPRO_CC)
#define os_threadLocal __thread
#else
#error "os_threadLocal is not supported"
#endif

#if defined (__cplusplus)
}
#endif

#endif
