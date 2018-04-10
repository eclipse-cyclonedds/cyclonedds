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
#ifdef OSPL_STRICT_MEM
        assert(mutex->signature != OS_MUTEX_MAGIC_SIG);
#endif
        InitializeSRWLock(&mutex->lock);
#ifdef OSPL_STRICT_MEM
        mutex->signature = OS_MUTEX_MAGIC_SIG;
#endif
}

void os_mutexDestroy(
        _Inout_ _Post_invalid_ os_mutex *mutex)
{
        assert(mutex != NULL);
#ifdef OSPL_STRICT_MEM
        assert(mutex->signature == OS_MUTEX_MAGIC_SIG);
        mutex->signature = 0;
#endif
}

_Acquires_nonreentrant_lock_(&mutex->lock)
void os_mutexLock(
        _Inout_ os_mutex *mutex)
{
        assert(mutex != NULL);
#ifdef OSPL_STRICT_MEM
        assert(mutex->signature == OS_MUTEX_MAGIC_SIG);
#endif
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
#ifdef OSPL_STRICT_MEM
        assert(mutex->signature == OS_MUTEX_MAGIC_SIG);
#endif
        return TryAcquireSRWLockExclusive(&mutex->lock) ? os_resultSuccess : os_resultBusy;
}

_Releases_nonreentrant_lock_(&mutex->lock)
void os_mutexUnlock(
        _Inout_ os_mutex *mutex)
{
        assert(mutex != NULL);
#ifdef OSPL_STRICT_MEM
        assert(mutex->signature == OS_MUTEX_MAGIC_SIG);
#endif
        ReleaseSRWLockExclusive(&mutex->lock);
}

void os_condInit(
        _Out_ os_cond *cond,
        _In_ os_mutex *dummymtx)
{
        assert(cond != NULL);
        assert(dummymtx != NULL);
#ifdef OSPL_STRICT_MEM
        assert(cond->signature != OS_COND_MAGIC_SIG);
#endif
        (void)dummymtx;
        InitializeConditionVariable(&cond->cond);
#ifdef OSPL_STRICT_MEM
        cond->signature = OS_COND_MAGIC_SIG;
#endif
}

void os_condDestroy(
        _Inout_ _Post_invalid_ os_cond *cond)
{
        assert(cond != NULL);
#ifdef OSPL_STRICT_MEM
        assert(cond->signature == OS_COND_MAGIC_SIG);
        cond->signature = 0;
#endif
}

void os_condWait(os_cond *cond, os_mutex *mutex)
{
        assert(cond != NULL);
        assert(mutex != NULL);
#ifdef OSPL_STRICT_MEM
        assert(cond->signature == OS_COND_MAGIC_SIG);
        assert(mutex->signature == OS_MUTEX_MAGIC_SIG);
#endif
        if (!SleepConditionVariableSRW(&cond->cond, &mutex->lock, INFINITE, 0)) {
                abort();
        }
}

os_result os_condTimedWait(os_cond *cond, os_mutex *mutex, const os_time *time)
{
        DWORD timems;
        assert(cond != NULL);
        assert(mutex != NULL);
#ifdef OSPL_STRICT_MEM
        assert(cond->signature == OS_COND_MAGIC_SIG);
        assert(mutex->signature == OS_MUTEX_MAGIC_SIG);
#endif
        timems = time->tv_sec * 1000 + (time->tv_nsec + 999999999) / 1000000;
        if (SleepConditionVariableSRW(&cond->cond, &mutex->lock, timems, 0))
                return os_resultSuccess;
        else if (GetLastError() != ERROR_TIMEOUT)
                abort();
        else if (timems != INFINITE)
                return os_resultTimeout;
        else
                return os_resultSuccess;
}

void os_condSignal(os_cond *cond)
{
        assert(cond != NULL);
#ifdef OSPL_STRICT_MEM
        assert(cond->signature == OS_COND_MAGIC_SIG);
#endif
        WakeConditionVariable(&cond->cond);
}

void os_condBroadcast(os_cond *cond)
{
        assert(cond != NULL);
#ifdef OSPL_STRICT_MEM
        assert(cond->signature == OS_COND_MAGIC_SIG);
#endif
        WakeAllConditionVariable(&cond->cond);
}

void os_rwlockInit(_Out_ os_rwlock *rwlock)
{
        assert(rwlock);
#ifdef OSPL_STRICT_MEM
        assert(rwlock->signature != OS_RWLOCK_MAGIC_SIG);
#endif
        InitializeSRWLock(&rwlock->lock);
        rwlock->state = 0;
#ifdef OSPL_STRICT_MEM
        rwlock->signature = OS_RWLOCK_MAGIC_SIG;
#endif
}

void os_rwlockDestroy(_Inout_ _Post_invalid_ os_rwlock *rwlock)
{
        assert(rwlock);
#ifdef OSPL_STRICT_MEM
        assert(rwlock->signature != OS_RWLOCK_MAGIC_SIG);
        rwlock->signature = 0;
#endif
}

void os_rwlockRead(os_rwlock *rwlock)
{
        assert(rwlock);
#ifdef OSPL_STRICT_MEM
        assert(rwlock->signature != OS_RWLOCK_MAGIC_SIG);
#endif
        AcquireSRWLockShared(&rwlock->lock);
        rwlock->state = 1;
}

void os_rwlockWrite(os_rwlock *rwlock)
{
        assert(rwlock);
#ifdef OSPL_STRICT_MEM
        assert(rwlock->signature != OS_RWLOCK_MAGIC_SIG);
#endif
        AcquireSRWLockExclusive(&rwlock->lock);
        rwlock->state = -1;
}

os_result os_rwlockTryRead(os_rwlock *rwlock)
{
        assert(rwlock);
#ifdef OSPL_STRICT_MEM
        assert(rwlock->signature != OS_RWLOCK_MAGIC_SIG);
#endif
        if (TryAcquireSRWLockShared(&rwlock->lock)) {
                rwlock->state = 1;
                return os_resultSuccess;
        }
        else {
                return os_resultBusy;
        }
}

os_result os_rwlockTryWrite(os_rwlock *rwlock)
{
        assert(rwlock);
#ifdef OSPL_STRICT_MEM
        assert(rwlock->signature != OS_RWLOCK_MAGIC_SIG);
#endif
        if (TryAcquireSRWLockExclusive(&rwlock->lock)) {
                rwlock->state = -1;
                return os_resultSuccess;
        }
        else {
                return os_resultBusy;
        }
}

void os_rwlockUnlock(os_rwlock *rwlock)
{
        assert(rwlock);
#ifdef OSPL_STRICT_MEM
        assert(rwlock->signature != OS_RWLOCK_MAGIC_SIG);
#endif
        assert(rwlock->state != 0);
        if (rwlock->state > 0) {
                ReleaseSRWLockShared(&rwlock->lock);
        }
        else {
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
