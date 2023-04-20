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

void ddsrt_mutex_init(ddsrt_mutex_t *mutex)
{
  assert(mutex != NULL);
  InitializeSRWLock(&mutex->lock);
}

void ddsrt_mutex_destroy(ddsrt_mutex_t *mutex)
{
  assert(mutex != NULL);
}

void ddsrt_mutex_lock(ddsrt_mutex_t *mutex)
{
  assert(mutex != NULL);
  AcquireSRWLockExclusive(&mutex->lock);
}

bool ddsrt_mutex_trylock(ddsrt_mutex_t *mutex)
{
  assert(mutex != NULL);
  return TryAcquireSRWLockExclusive(&mutex->lock);
}

void ddsrt_mutex_unlock(ddsrt_mutex_t *mutex)
{
  assert(mutex != NULL);
  ReleaseSRWLockExclusive(&mutex->lock);
}

void ddsrt_cond_init (ddsrt_cond_t *cond)
{
  assert(cond != NULL);
  InitializeConditionVariable(&cond->cond);
}

void ddsrt_cond_destroy(ddsrt_cond_t *cond)
{
  assert(cond != NULL);
}

void
ddsrt_cond_wait(ddsrt_cond_t *cond, ddsrt_mutex_t *mutex)
{
  assert(cond != NULL);
  assert(mutex != NULL);

  if (!SleepConditionVariableSRW(&cond->cond, &mutex->lock, INFINITE, 0)) {
    abort();
  }
}

bool
ddsrt_cond_waituntil(
  ddsrt_cond_t *cond,
  ddsrt_mutex_t *mutex,
  dds_time_t abstime)
{
  dds_duration_t reltime;

  assert(cond != NULL);
  assert(mutex != NULL);

  if (abstime == DDS_NEVER) {
    reltime = DDS_INFINITY;
  } else {
    dds_time_t time = dds_time();
    reltime = (abstime > time ? abstime - time : 0);
  }

  return ddsrt_cond_waitfor(cond, mutex, reltime);
}

bool
ddsrt_cond_waitfor(
  ddsrt_cond_t *cond,
  ddsrt_mutex_t *mutex,
  dds_duration_t reltime)
{
  dds_time_t abstime;
  DWORD msecs;

  assert(cond != NULL);
  assert(mutex != NULL);

  if (reltime == DDS_INFINITY) {
    ddsrt_cond_wait(cond, mutex);
    return true;
  }

  abstime = ddsrt_time_add_duration(dds_time(), reltime);
  msecs = ddsrt_duration_to_msecs_ceil(reltime);
  if (SleepConditionVariableSRW(&cond->cond, &mutex->lock, msecs, 0)) {
    return true;
  } else if (GetLastError() != ERROR_TIMEOUT) {
    abort();
  }

  return (dds_time() >= abstime) ? false : true;
}

void ddsrt_cond_signal (ddsrt_cond_t *cond)
{
  assert(cond != NULL);
  WakeConditionVariable(&cond->cond);
}

void ddsrt_cond_broadcast (ddsrt_cond_t *cond)
{
  assert(cond != NULL);
  WakeAllConditionVariable(&cond->cond);
}

void ddsrt_rwlock_init (ddsrt_rwlock_t *rwlock)
{
  assert(rwlock);
  InitializeSRWLock(&rwlock->lock);
  rwlock->state = 0;
}

void ddsrt_rwlock_destroy (ddsrt_rwlock_t *rwlock)
{
  assert(rwlock);
}

void ddsrt_rwlock_read (ddsrt_rwlock_t *rwlock)
{
  assert(rwlock);
  AcquireSRWLockShared(&rwlock->lock);
  rwlock->state = 1;
}

void ddsrt_rwlock_write(ddsrt_rwlock_t *rwlock)
{
  assert(rwlock);
  AcquireSRWLockExclusive(&rwlock->lock);
  rwlock->state = -1;
}

bool ddsrt_rwlock_tryread (ddsrt_rwlock_t *rwlock)
{
  assert(rwlock);
  if (TryAcquireSRWLockShared(&rwlock->lock)) {
    rwlock->state = 1;
    return true;
  }
  return false;
}

bool ddsrt_rwlock_trywrite (ddsrt_rwlock_t *rwlock)
{
  assert(rwlock);
  if (TryAcquireSRWLockExclusive(&rwlock->lock)) {
    rwlock->state = -1;
    return true;
  }
  return false;
}

void ddsrt_rwlock_unlock (ddsrt_rwlock_t *rwlock)
{
  assert(rwlock);
  assert(rwlock->state != 0);
  if (rwlock->state > 0) {
    ReleaseSRWLockShared(&rwlock->lock);
  } else {
    ReleaseSRWLockExclusive(&rwlock->lock);
  }
}

typedef struct {
  ddsrt_once_fn init_fn;
} once_arg_t;

static BOOL WINAPI
once_wrapper(
  PINIT_ONCE InitOnce,
  PVOID Parameter,
  PVOID *Context)
{
  once_arg_t *wrap = (once_arg_t *)Parameter;

  (void)InitOnce;
  assert(Parameter != NULL);
  assert(Context == NULL);

  wrap->init_fn();

  return TRUE;
}

void
ddsrt_once(
  ddsrt_once_t *control,
  ddsrt_once_fn init_fn)
{
  once_arg_t wrap = { .init_fn = init_fn };
  (void) InitOnceExecuteOnce(control, &once_wrapper, &wrap, NULL);
}
