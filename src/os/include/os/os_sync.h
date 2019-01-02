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
#ifndef OS_SYNC_H
#define OS_SYNC_H

#if defined (__cplusplus)
extern "C" {
#endif

    /** \brief Sets the priority inheritance mode for mutexes
     *   that are created after this call. (only effective on
     *   platforms that support priority inheritance)
     *
     * Possible Results:
     * - returns os_resultSuccess if
     *     mutex is successfuly initialized
     * - returns os_resultSuccess
     */
    OSAPI_EXPORT os_result
    os_mutexSetPriorityInheritanceMode(
            bool enabled);

    /** \brief Initialize the mutex
     *
     * Possible Results:
     * - assertion failure: mutex = NULL
     * - returns os_resultSuccess if
     *     mutex is successfuly initialized
     * - returns os_resultFail if
     *     mutex is not initialized because of a failure
     */
    OSAPI_EXPORT void
    os_mutexInit(
            _Out_ os_mutex *mutex)
        __nonnull((1));

    /** \brief Destroy the mutex
     *
     * Never returns on failure
     */
    OSAPI_EXPORT void
    os_mutexDestroy(
            _Inout_ _Post_invalid_ os_mutex *mutex)
        __nonnull_all__;

    /** \brief Acquire the mutex.
     *
     * If you need to detect an error, use os_mutexLock_s instead.
     *
     * @see os_mutexLock_s
     */
    _Acquires_nonreentrant_lock_(&mutex->lock)
    OSAPI_EXPORT void
    os_mutexLock(
            _Inout_ os_mutex *mutex)
    __nonnull_all__;

    /**
     * \brief Acquire the mutex. Returns whether the call succeeeded or an error
     * occurred.
     *
     * Precondition:
     * - mutex is not yet acquired by the calling thread
     *
     * Possible Results:
     * - assertion failure: mutex = NULL
     * - returns os_resultSuccess if
     *     mutex is acquired
     * - returns os_resultFail if
     *     mutex is not acquired because of a failure
     */
    _Check_return_
    _When_(return == os_resultSuccess, _Acquires_nonreentrant_lock_(&mutex->lock))
        OSAPI_EXPORT os_result
        os_mutexLock_s(
                _Inout_ os_mutex *mutex)
        __nonnull_all__
        __attribute_warn_unused_result__;

    /** \brief Try to acquire the mutex, immediately return if the mutex
     *         is already acquired by another thread
     *
     * Precondition:
     * - mutex is not yet acquired by the calling thread
     *
     * Possible Results:
     * - assertion failure: mutex = NULL
     * - returns os_resultSuccess if
     *      mutex is acquired
     * - returns os_resultBusy if
     *      mutex is not acquired because it is already acquired
     *      by another thread
     * - returns os_resultFail if
     *      mutex is not acquired because of a failure
     */
    _Check_return_
    _When_(return == os_resultSuccess, _Acquires_nonreentrant_lock_(&mutex->lock))
        OSAPI_EXPORT os_result
        os_mutexTryLock (
                _Inout_ os_mutex *mutex)
        __nonnull_all__
        __attribute_warn_unused_result__;

    /** \brief Release the acquired mutex
     */
    _Releases_nonreentrant_lock_(&mutex->lock)
    OSAPI_EXPORT void
    os_mutexUnlock (
            _Inout_ os_mutex *mutex)
    __nonnull_all__;

    /** \brief Initialize the condition variable
     *
     * Possible Results:
     * - returns os_resultSuccess if
     *     cond is successfuly initialized
     * - returns os_resultFail if
     *     cond is not initialized and can not be used
     */
     OSAPI_EXPORT void
     os_condInit(
                _Out_ os_cond *cond,
                _In_ os_mutex *mutex)
        __nonnull_all__;

    /** \brief Destroy the condition variable
     */
    OSAPI_EXPORT void
    os_condDestroy(
            _Inout_ _Post_invalid_ os_cond *cond)
        __nonnull_all__;

    /** \brief Wait for the condition
     *
     * Precondition:
     * - mutex is acquired by the calling thread before calling
     *   os_condWait
     *
     * Postcondition:
     * - mutex is still acquired by the calling thread and should
     *   be released by it
     */
    OSAPI_EXPORT void
    os_condWait(
            os_cond *cond,
            os_mutex *mutex)
        __nonnull_all__;

    /** \brief Wait for the condition but return when the specified
     *         time has expired before the condition is triggered
     *
     * Precondition:
     * - mutex is acquired by the calling thread before calling
     *   os_condTimedWait
     *
     * Postcondition:
     * - mutex is still acquired by the calling thread and should
     *   be released by it
     *
     * Possible Results:
     * - assertion failure: cond = NULL || mutex = NULL ||
     *     time = NULL
     * - returns os_resultSuccess if
     *     cond is triggered
     * - returns os_resultTimeout if
     *     cond is timed out
     * - returns os_resultFail if
     *     cond is not triggered nor is timed out but
     *     os_condTimedWait has returned because of a failure
     */
    OSAPI_EXPORT os_result
    os_condTimedWait(
            os_cond *cond,
            os_mutex *mutex,
            const os_time *time)
        __nonnull_all__;

    /** \brief Signal the condition and wakeup one thread waiting
     *         for the condition
     *
     * Precondition:
     * - the mutex used with the condition in general should be
     *   acquired by the calling thread before setting the
     *   condition state and signalling
     */
    OSAPI_EXPORT void
    os_condSignal(
            os_cond *cond)
        __nonnull_all__;

    /** \brief Signal the condition and wakeup all thread waiting
     *         for the condition
     *
     * Precondition:
     * - the mutex used with the condition in general should be
     *   acquired by the calling thread before setting the
     *   condition state and signalling
     */
    OSAPI_EXPORT void
    os_condBroadcast(
            os_cond *cond)
        __nonnull_all__;

    /** \brief Initialize the rwloc
     *
     * Possible Results:
     * - assertion failure: rwlock = NULL
     * - returns os_resultSuccess if
     *     rwlock is successfuly initialized
     * - returns os_resultFail
     *     rwlock is not initialized and can not be used
     */
    OSAPI_EXPORT void
    os_rwlockInit(
            _Out_ os_rwlock *rwlock);

    /** \brief Destroy the rwlock
     *
     * Possible Results:
     * - assertion failure: rwlock = NULL
     * - returns os_resultSuccess if
     *     rwlock is successfuly destroyed
     * - returns os_resultBusy if
     *     rwlock is not destroyed because it is still claimed or referenced by a thread
     * - returns os_resultFail if
     *     rwlock is not destroyed
     */
    OSAPI_EXPORT void
    os_rwlockDestroy(
            _Inout_ _Post_invalid_ os_rwlock *rwlock);

    /** \brief Acquire the rwlock while intending to read only
     *
     * Precondition:
     * - rwlock is not yet acquired by the calling thread
     *
     * Postcondition:
     * - The data related to the critical section is not changed
     *   by the calling thread
     *
     * Possible Results:
     * - assertion failure: rwlock = NULL
     * - returns os_resultSuccess if
     *      rwlock is acquired
     * - returns os_resultFail if
     *      rwlock is not acquired because of a failure
     */
    OSAPI_EXPORT void
    os_rwlockRead(
            os_rwlock *rwlock);

    /** \brief Acquire the rwlock while intending to write
     *
     * Precondition:
     * - rwlock is not yet acquired by the calling thread
     *
     * Possible Results:
     * - assertion failure: rwlock = NULL
     * - returns os_resultSuccess if
     *      rwlock is acquired
     * - returns os_resultFail if
     *      rwlock is not acquired because of a failure
     */
    OSAPI_EXPORT void
    os_rwlockWrite(
            os_rwlock *rwlock);

    /** \brief Try to acquire the rwlock while intending to read only
     *
     * Try to acquire the rwlock while intending to read only,
     * immediately return if the mutex is acquired by
     * another thread with the intention to write
     *
     * Precondition:
     * - rwlock is not yet acquired by the calling thread
     *
     * Postcondition:
     * - The data related to the critical section is not changed
     *   by the calling thread
     *
     * Possible Results:
     * - assertion failure: rwlock = NULL
     * - returns os_resultSuccess if
     *      rwlock is acquired
     * - returns os_resultBusy if
     *      rwlock is not acquired because it is already
     *      acquired by another thread with the intention to write
     * - returns os_resultFail if
     *      rwlock is not acquired because of a failure
     */
    OSAPI_EXPORT os_result
    os_rwlockTryRead(
            os_rwlock *rwlock);

    /** \brief Try to acquire the rwlock while intending to write
     *
     * Try to acquire the rwlock while intending to write,
     * immediately return if the mutex is acquired by
     * another thread, either for read or for write
     *
     * Precondition:
     * - rwlock is not yet acquired by the calling thread
     *
     * Possible Results:
     * - assertion failure: rwlock = NULL
     * - returns os_resultSuccess if
     *      rwlock is acquired
     * - returns os_resultBusy if
     *      rwlock is not acquired because it is already
     *      acquired by another thread
     * - returns os_resultFail if
     *      rwlock is not acquired because of a failure
     */
    OSAPI_EXPORT os_result
    os_rwlockTryWrite(
            os_rwlock *rwlock);

    /** \brief Release the acquired rwlock
     *
     * Precondition:
     * - rwlock is already acquired by the calling thread
     *
     * Possible Results:
     * - assertion failure: rwlock = NULL
     * - returns os_resultSuccess if
     *     rwlock is released
     * - returns os_resultFail if
     *     rwlock is not released because of a failure
     */
    OSAPI_EXPORT void
    os_rwlockUnlock(
            os_rwlock *rwlock);

    /* Initialization callback used by os_once */
    typedef void (*os_once_fn)(void);

    /** \brief Invoke init_fn exactly once for a given control
     *
     * The first thread calling os_once(...) with a given control will invoke the
     * init_fn() with no arguments. Following calls with the same control will not
     * invoke init_fn().
     *
     * Precondition:
     * - The control parameter is properly initialized with OS_ONCE_T_STATIC_INIT
     *
     * Postcondition:
     * - On return init_fn() has completed exactly once for the given control.
    */
    OSAPI_EXPORT void
    os_once(
        _Inout_ os_once_t *control,
        _In_ os_once_fn init_fn);

    OSAPI_EXPORT os_mutex *
    os_getSingletonMutex(
        void);

#if defined (__cplusplus)
}
#endif

#endif
