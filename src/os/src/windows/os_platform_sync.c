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
#include <assert.h>
#include "os/os.h"

void os_mutexInit(
    _Out_ os_mutex *mutex)
{
    assert(mutex != NULL);

    InitializeSRWLock(&mutex->lock);
}

void os_mutexDestroy(
    _Inout_ _Post_invalid_ os_mutex *mutex)
{
    assert(mutex != NULL);
}

_Acquires_nonreentrant_lock_(&mutex->lock)
void os_mutexLock(
    _Inout_ os_mutex *mutex)
{
    assert(mutex != NULL);

    AcquireSRWLockExclusive(&mutex->lock);
}

_Check_return_
_When_(return == os_resultSuccess, _Acquires_nonreentrant_lock_(&mutex->lock))
os_result os_mutexLock_s(
    _Inout_ os_mutex *mutex)
{
    os_mutexLock(mutex);
    return os_resultSuccess;
}

_Check_return_
_When_(return == os_resultSuccess, _Acquires_nonreentrant_lock_(&mutex->lock))
os_result
os_mutexTryLock(
    _Inout_ os_mutex *mutex)
{
    assert(mutex != NULL);

    return TryAcquireSRWLockExclusive(&mutex->lock) ? os_resultSuccess : os_resultBusy;
}

_Releases_nonreentrant_lock_(&mutex->lock)
void os_mutexUnlock(
    _Inout_ os_mutex *mutex)
{
    assert(mutex != NULL);

    ReleaseSRWLockExclusive(&mutex->lock);
}

void os_condInit(
        _Out_ os_cond *cond,
        _In_ os_mutex *dummymtx)
{
    assert(cond != NULL);
    assert(dummymtx != NULL);

    (void)dummymtx;
    InitializeConditionVariable(&cond->cond);
}

void os_condDestroy(
        _Inout_ _Post_invalid_ os_cond *cond)
{
    assert(cond != NULL);
}

void os_condWait(os_cond *cond, os_mutex *mutex)
{
    assert(cond != NULL);
    assert(mutex != NULL);

    if (!SleepConditionVariableSRW(&cond->cond, &mutex->lock, INFINITE, 0)) {
        abort();
    }
}

os_result os_condTimedWait(os_cond *cond, os_mutex *mutex, const os_time *time)
{
    DWORD timems;
    assert(cond != NULL);
    assert(mutex != NULL);

    timems = time->tv_sec * 1000 + (time->tv_nsec + 999999999) / 1000000;
    if (SleepConditionVariableSRW(&cond->cond, &mutex->lock, timems, 0)) {
        return os_resultSuccess;
    } else if (GetLastError() != ERROR_TIMEOUT) {
        abort();
    } else if (timems != INFINITE) {
        return os_resultTimeout;
    } else {
        return os_resultSuccess;
    }
}

void os_condSignal(os_cond *cond)
{
    assert(cond != NULL);

    WakeConditionVariable(&cond->cond);
}

void os_condBroadcast(os_cond *cond)
{
    assert(cond != NULL);

    WakeAllConditionVariable(&cond->cond);
}

void os_rwlockInit(_Out_ os_rwlock *rwlock)
{
    assert(rwlock);

    InitializeSRWLock(&rwlock->lock);
    rwlock->state = 0;
}

void os_rwlockDestroy(_Inout_ _Post_invalid_ os_rwlock *rwlock)
{
    assert(rwlock);
}

void os_rwlockRead(os_rwlock *rwlock)
{
    assert(rwlock);

    AcquireSRWLockShared(&rwlock->lock);
    rwlock->state = 1;
}

void os_rwlockWrite(os_rwlock *rwlock)
{
    assert(rwlock);

    AcquireSRWLockExclusive(&rwlock->lock);
    rwlock->state = -1;
}

os_result os_rwlockTryRead(os_rwlock *rwlock)
{
    assert(rwlock);

    if (TryAcquireSRWLockShared(&rwlock->lock)) {
        rwlock->state = 1;
        return os_resultSuccess;
    }

    return os_resultBusy;
}

os_result os_rwlockTryWrite(os_rwlock *rwlock)
{
    assert(rwlock);

    if (TryAcquireSRWLockExclusive(&rwlock->lock)) {
        rwlock->state = -1;
        return os_resultSuccess;
    }

    return os_resultBusy;
}

void os_rwlockUnlock(os_rwlock *rwlock)
{
    assert(rwlock);

    assert(rwlock->state != 0);
    if (rwlock->state > 0) {
        ReleaseSRWLockShared(&rwlock->lock);
    } else {
        ReleaseSRWLockExclusive(&rwlock->lock);
    }
}

struct os__onceWrapper {
    os_once_fn init_fn;
};

static BOOL WINAPI
os__onceWrapper(
    _Inout_ PINIT_ONCE InitOnce,
    _Inout_opt_ PVOID Parameter,
    _Outptr_opt_result_maybenull_ PVOID *Context)
{
    struct os__onceWrapper *wrap = (struct os__onceWrapper *) Parameter;

    /* Only to be invoked from os_once, so assume inputs to be as
     * expected instead of implementing checks officially needed to
     * fulfill SAL. */
    _Analysis_assume_(wrap);
    _Analysis_assume_(Context == NULL);

    wrap->init_fn();

    return TRUE;
}

void
os_once(
    _Inout_ os_once_t *control,
    _In_ os_once_fn init_fn)
{
    struct os__onceWrapper wrap = { .init_fn = init_fn };
    (void) InitOnceExecuteOnce(control, &os__onceWrapper, &wrap, NULL);
}
