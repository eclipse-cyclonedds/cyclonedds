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
/****************************************************************
 * Interface definition for thread management of SPLICE-DDS     *
 ****************************************************************/

#ifndef OS_THREAD_H
#define OS_THREAD_H

/** \file os_thread.h
 *  \brief Thread management - create threads
 */

#include "os/os_defs.h"

#if defined (__cplusplus)
extern "C" {
#endif

    /* Number of slots in Thread Private Memory */

    typedef enum os_threadMemoryIndex {
        OS_THREAD_CLOCK_OFFSET,
        OS_THREAD_UT_TRACE,
        OS_THREAD_JVM,
        OS_THREAD_PROTECT,
        OS_THREAD_API_INFO,
        OS_THREAD_WARNING,
        OS_THREAD_ALLOCATOR_STATE,
        OS_THREAD_NAME,
        OS_THREAD_PROCESS_INFO,
        OS_THREAD_STATE, /* Used for monitoring thread progress */
        OS_THREAD_STR_ERROR,
        OS_THREAD_MEM_ARRAY_SIZE /* Number of slots in Thread Private Memory */
    } os_threadMemoryIndex;

    /** \brief Definition for a thread routine invoked on thread create. */
    typedef uint32_t (*os_threadRoutine)(void*);

    /** \brief Definition of the scheduling class */
    typedef enum os_schedClass {
        /** Schedule processes and threads according a platform default.
         *  OS_SCHED_REALTIME for timesharing platforms and
         *  OS_SCHED_TIMESHARE for realtime platforms
         */
        OS_SCHED_DEFAULT,
        /** Schedule processes and threads on realtime basis,
         *  on most platforms implying:
         *  - Fixed Priority
         *  - Preemption
         *  - Either "First In First Out" or "Round Robin"
         */
        OS_SCHED_REALTIME,
        /** Schedule processes and threads on timesharing basis,
         *  on most platforms implying:
         *  - Dynamic Priority to guarantee fair share
         *  - Preemption
         */
        OS_SCHED_TIMESHARE
    } os_schedClass;

    /** \brief Definition of the thread attributes
     */
    typedef struct os_threadAttr {
        /** Specifies the scheduling class */
        os_schedClass      schedClass;
        /** Specifies the thread priority */
        int32_t            schedPriority;
        /** Specifies the thread stack size */
        uint32_t           stackSize;
    } os_threadAttr;

    /** \brief Internal structure used to store cleanup handlers (private) */
    typedef struct {
        void (*func)(void *);
        void *data;
    } os_threadCleanup;

    /** \brief Create a new thread
     *
     * Creates a new thread of control that executes concurrently with
     * the calling thread. The new thread applies the function start_routine
     * passing it arg as first argument.
     *
     * The new thread terminates by returning from the start_routine function.
     * The created thread is identified by the returned threadId.
     *
     * Possible Results:
     * - assertion failure: threadId = NULL || name = NULL ||
     *     threadAttr = NULL || start_routine = NULL
     * - returns os_resultSuccess if
     *     the thread is successfuly created
     * - returns os_resultFail if
     *     the thread is not created because of an failure
     */
    OSAPI_EXPORT os_result
    os_threadCreate(
            _Out_ os_threadId *threadId,
            _In_z_ const char *name,
            _In_ const os_threadAttr *threadAttr,
            _In_ os_threadRoutine start_routine,
            _In_opt_ void *arg);

    /** \brief Return the integer representation of the given thread ID
     *
     * Possible Results:
     * - returns the integer representation of the given thread ID
     */
    OSAPI_EXPORT uintmax_t
    os_threadIdToInteger(
            os_threadId id);

    /** \brief Return the thread ID of the calling thread
     *
     * Possible Results:
     * - returns the tread ID of the calling thread
     */
    OSAPI_EXPORT os_threadId
    os_threadIdSelf(void);

    int os_threadEqual (os_threadId a, os_threadId b);

    /** \brief Wait for the termination of the identified thread
     *
     * If the identified thread is still running, wait for its termination
     * else return immediately. In thread_result it returns the exit status
     * of the thread. If thread_result is passed as a NULL pointer,
     * no exit status is returned, but os_threadWaitExit still waits for the
     * thread to terminate.
     *
     * Possible Results:
     * - assertion failure: threadId = 0
     * - returns os_resultSuccess when
     *     the identified thread is not running
     * - returns os_threadFail if
     *     the services is aborted because of a failure
     */
    OSAPI_EXPORT os_result
    os_threadWaitExit(
            _In_ os_threadId threadId,
            _Out_opt_ uint32_t *thread_result);

    /** \brief Get name of current thread
     *
     * Postcondition:
     * - \b name is ""
     *     the thread name could not be determined
     * - \b name is "<thread name>"
     *     the thread name could be determined
     *
     * \b name will not be filled beyond the specified \b length
     */
    OSAPI_EXPORT int32_t
    os_threadGetThreadName(
            char *name,
            uint32_t length);

    /** \brief Set the default thread attributes
     *
     * Postcondition:
     * - thread scheduling class is OS_SCHED_DEFAULT
     * - thread priority is platform dependent
     *
     * Possible Results:
     * - assertion failure: threadAttr = NULL
     */
    OSAPI_EXPORT void
    os_threadAttrInit(
            os_threadAttr *threadAttr)
        __nonnull_all__;

    /** \brief Allocate thread private memory
     *
     * Allocate heap memory of the specified \b size and
     * relate it to the thread by storing the memory
     * reference in an thread specific reference array
     * indexed by \b index. If the indexed thread reference
     * array location already contains a reference, no
     * memory will be allocated and NULL is returned.
     *
     * Possible Results:
     * - returns NULL if
     *     index < 0 || index >= OS_THREAD_MEM_ARRAY_SIZE
     * - returns NULL if
     *     no sufficient memory is available on heap
     * - returns NULL if
     *     os_threadMemGet (index) returns != NULL
     * - returns reference to allocated heap memory
     *     of the requested size if
     *     memory is successfully allocated
     */
    OSAPI_EXPORT void *
    os_threadMemMalloc(
            int32_t index,
            size_t size);

    /** \brief Free thread private memory
     *
     * Free the memory referenced by the thread reference
     * array indexed location. If this reference is NULL,
     * or index is invalid, no action is taken.
     * The reference is set to NULL after freeing the
     * heap memory.
     *
     * Postcondition:
     * - os_threadMemGet (index) = NULL and allocated
     *   heap memory is freed
     */
    OSAPI_EXPORT void
    os_threadMemFree(
            int32_t index);

    /** \brief Get thread private memory
     *
     * Possible Results:
     * - returns NULL if
     *     index < 0 || index >= OS_THREAD_MEM_ARRAY_SIZE
     * - returns NULL if
     *     No heap memory is related to the thread for
     *     the specified index
     * - returns a reference to the allocated memory
     */
    OSAPI_EXPORT void *
    os_threadMemGet(
            int32_t index);

    /** \brief Pop cleanup handler from the top of thread's cleanup stack
     *
     * Remove routine at the top of the calling thread's cleanup stack and
     * optionally invoke it (if execute is non-zero).
     */
    OSAPI_EXPORT void
    os_threadCleanupPop(
        int32_t execute);

    /** \brief Push cleanup handler onto thread's cleanup stack
     *
     * Push a cleanup handler onto the top of the calling thread's cleanup
     * stack. The cleanup handler will be popped of the thread's cleanup stack
     * and invoked with the specified argument when the thread exits.
     */
    OSAPI_EXPORT void
    os_threadCleanupPush(
        void (*routine)(void*),
        void *arg);

#if defined (__cplusplus)
}
#endif

#endif /* OS_THREAD_H */
