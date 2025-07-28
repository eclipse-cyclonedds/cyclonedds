// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdlib.h>

#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/static_assert.h"

void ddsrt_mutex_init (ddsrt_mutex_t *mutex)
{
  InitializeSRWLock (&mutex->lock);
}

void ddsrt_mutex_destroy (ddsrt_mutex_t *mutex)
{
  (void) mutex;
}

void ddsrt_mutex_lock (ddsrt_mutex_t *mutex)
{
  AcquireSRWLockExclusive (&mutex->lock);
}

bool ddsrt_mutex_trylock (ddsrt_mutex_t *mutex)
{
  return TryAcquireSRWLockExclusive (&mutex->lock);
}

void ddsrt_mutex_unlock (ddsrt_mutex_t *mutex)
{
  ReleaseSRWLockExclusive (&mutex->lock);
}

void ddsrt_cond_init (ddsrt_cond_t *cond)
{
  InitializeConditionVariable (&cond->cond);
}

void ddsrt_cond_wctime_init (ddsrt_cond_wctime_t *cond)
{
  InitializeConditionVariable (&cond->cond);
}

void ddsrt_cond_mtime_init (ddsrt_cond_mtime_t *cond)
{
  InitializeConditionVariable (&cond->cond);
}

void ddsrt_cond_etime_init (ddsrt_cond_etime_t *cond)
{
  InitializeConditionVariable (&cond->cond);
}

void ddsrt_cond_destroy (ddsrt_cond_t *cond)
{
  (void) cond;
}

void ddsrt_cond_wctime_destroy (ddsrt_cond_wctime_t *cond)
{
  (void) cond;
}

void ddsrt_cond_mtime_destroy (ddsrt_cond_mtime_t *cond)
{
  (void) cond;
}

void ddsrt_cond_etime_destroy (ddsrt_cond_etime_t *cond)
{
  (void) cond;
}

static void ddsrt_cond_wait_impl (CONDITION_VARIABLE *cond, SRWLOCK *mutex)
{
  if (!SleepConditionVariableSRW(cond, mutex, INFINITE, 0)) {
    abort();
  }
}

void ddsrt_cond_wait (ddsrt_cond_t *cond, ddsrt_mutex_t *mutex)
{
  ddsrt_cond_wait_impl (&cond->cond, &mutex->lock);
}

void ddsrt_cond_wctime_wait (ddsrt_cond_wctime_t *cond, ddsrt_mutex_t *mutex)
{
  ddsrt_cond_wait_impl (&cond->cond, &mutex->lock);
}

void ddsrt_cond_mtime_wait (ddsrt_cond_mtime_t *cond, ddsrt_mutex_t *mutex)
{
  ddsrt_cond_wait_impl (&cond->cond, &mutex->lock);
}

void ddsrt_cond_etime_wait (ddsrt_cond_etime_t *cond, ddsrt_mutex_t *mutex)
{
  ddsrt_cond_wait_impl (&cond->cond, &mutex->lock);
}

static bool ddsrt_cond_waituntil_impl (CONDITION_VARIABLE *cond, SRWLOCK *mutex, int64_t tnow, int64_t abstimeout)
{
  if (abstimeout == DDS_NEVER)
  {
    if (!SleepConditionVariableSRW (cond, mutex, INFINITE, 0))
      abort ();
    return true;
  }
  else if (abstimeout < tnow)
  {
    return false;
  }
  else
  {
    DDSRT_STATIC_ASSERT (INFINITE < (DDS_NEVER / DDS_NSECS_IN_MSEC));
    const int64_t timeout = abstimeout - tnow;
    DWORD msecs;
    bool truncated;
    if (timeout < (INFINITE - 1) * DDS_NSECS_IN_MSEC - (DDS_NSECS_IN_MSEC - 1))
    {
      msecs = (DWORD) ((timeout + DDS_NSECS_IN_MSEC - 1) / DDS_NSECS_IN_MSEC);
      truncated = false;
    }
    else
    {
      msecs = INFINITE - 1;
      truncated = true;
    }
    if (SleepConditionVariableSRW (cond, mutex, msecs, 0))
      return true;
    if (GetLastError () != ERROR_TIMEOUT)
      abort ();
    return truncated;
  }
}

bool ddsrt_cond_wctime_waituntil (ddsrt_cond_wctime_t *cond, ddsrt_mutex_t *mutex, ddsrt_wctime_t abstimeout)
{
  return ddsrt_cond_waituntil_impl (&cond->cond, &mutex->lock, ddsrt_time_wallclock().v, abstimeout.v);
}

bool ddsrt_cond_mtime_waituntil (ddsrt_cond_mtime_t *cond, ddsrt_mutex_t *mutex, ddsrt_mtime_t abstimeout)
{
  return ddsrt_cond_waituntil_impl (&cond->cond, &mutex->lock, ddsrt_time_monotonic().v, abstimeout.v);
}

bool ddsrt_cond_etime_waituntil (ddsrt_cond_etime_t *cond, ddsrt_mutex_t *mutex, ddsrt_etime_t abstimeout)
{
  return ddsrt_cond_waituntil_impl (&cond->cond, &mutex->lock, ddsrt_time_elapsed().v, abstimeout.v);
}

void ddsrt_cond_signal (ddsrt_cond_t *cond)
{
  WakeConditionVariable (&cond->cond);
}

void ddsrt_cond_wctime_signal (ddsrt_cond_wctime_t *cond)
{
  WakeConditionVariable (&cond->cond);
}

void ddsrt_cond_mtime_signal (ddsrt_cond_mtime_t *cond)
{
  WakeConditionVariable (&cond->cond);
}

void ddsrt_cond_etime_signal (ddsrt_cond_etime_t *cond)
{
  WakeConditionVariable (&cond->cond);
}

void ddsrt_cond_broadcast (ddsrt_cond_t *cond)
{
  WakeAllConditionVariable (&cond->cond);
}

void ddsrt_cond_wctime_broadcast (ddsrt_cond_wctime_t *cond)
{
  WakeAllConditionVariable (&cond->cond);
}

void ddsrt_cond_mtime_broadcast (ddsrt_cond_mtime_t *cond)
{
  WakeAllConditionVariable (&cond->cond);
}

void ddsrt_cond_etime_broadcast (ddsrt_cond_etime_t *cond)
{
  WakeAllConditionVariable (&cond->cond);
}

void ddsrt_rwlock_init (ddsrt_rwlock_t *rwlock)
{
  InitializeSRWLock (&rwlock->lock);
  rwlock->state = 0;
}

void ddsrt_rwlock_destroy (ddsrt_rwlock_t *rwlock)
{
  (void) rwlock;
}

void ddsrt_rwlock_read (ddsrt_rwlock_t *rwlock)
{
  AcquireSRWLockShared (&rwlock->lock);
  rwlock->state = 1;
}

void ddsrt_rwlock_write (ddsrt_rwlock_t *rwlock)
{
  AcquireSRWLockExclusive (&rwlock->lock);
  rwlock->state = -1;
}

bool ddsrt_rwlock_tryread (ddsrt_rwlock_t *rwlock)
{
  if (!TryAcquireSRWLockShared (&rwlock->lock))
    return false;
  rwlock->state = 1;
  return true;
}

bool ddsrt_rwlock_trywrite (ddsrt_rwlock_t *rwlock)
{
  if (!TryAcquireSRWLockExclusive(&rwlock->lock))
    return false;
  rwlock->state = -1;
  return true;
}

void ddsrt_rwlock_unlock (ddsrt_rwlock_t *rwlock)
{
  assert (rwlock->state != 0);
  if (rwlock->state > 0)
    ReleaseSRWLockShared (&rwlock->lock);
  else
    ReleaseSRWLockExclusive (&rwlock->lock);
}

typedef struct {
  ddsrt_once_fn init_fn;
} once_arg_t;

static BOOL WINAPI once_wrapper (PINIT_ONCE InitOnce, PVOID Parameter, PVOID *Context)
{
  once_arg_t *wrap = (once_arg_t *) Parameter;
  (void) InitOnce;
  assert (Parameter != NULL);
  assert (Context == NULL);
  wrap->init_fn ();
  return TRUE;
}

void ddsrt_once (ddsrt_once_t *control, ddsrt_once_fn init_fn)
{
  once_arg_t wrap = { .init_fn = init_fn };
  (void) InitOnceExecuteOnce (control, &once_wrapper, &wrap, NULL);
}
