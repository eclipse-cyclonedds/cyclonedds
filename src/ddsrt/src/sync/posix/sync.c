// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/time.h"

void ddsrt_mutex_init (ddsrt_mutex_t *mutex)
{
  assert (mutex != NULL);
  pthread_mutex_init (&mutex->mutex, NULL);
}

void ddsrt_mutex_destroy (ddsrt_mutex_t *mutex)
{
  assert (mutex != NULL);

  if (pthread_mutex_destroy (&mutex->mutex) != 0)
    abort();
}

void ddsrt_mutex_lock (ddsrt_mutex_t *mutex)
{
  assert (mutex != NULL);

  if (pthread_mutex_lock (&mutex->mutex) != 0)
    abort();
}

bool
ddsrt_mutex_trylock (ddsrt_mutex_t *mutex)
{
  int err;
  assert (mutex != NULL);

  err = pthread_mutex_trylock (&mutex->mutex);
  if (err != 0 && err != EBUSY)
    abort();
  return (err == 0);
}

void
ddsrt_mutex_unlock (ddsrt_mutex_t *mutex)
{
  assert (mutex != NULL);

  if (pthread_mutex_unlock (&mutex->mutex) != 0)
    abort();
}

void
ddsrt_cond_init (ddsrt_cond_t *cond)
{
  assert (cond != NULL);

  pthread_cond_init (&cond->cond, NULL);
}

void
ddsrt_cond_destroy (ddsrt_cond_t *cond)
{
  assert (cond != NULL);

  if (pthread_cond_destroy (&cond->cond) != 0)
    abort();
}

void
ddsrt_cond_wait (ddsrt_cond_t *cond, ddsrt_mutex_t *mutex)
{
  assert (cond != NULL);
  assert (mutex != NULL);

  if (pthread_cond_wait (&cond->cond, &mutex->mutex) != 0)
    abort();
}

bool
ddsrt_cond_waituntil(
  ddsrt_cond_t *cond,
  ddsrt_mutex_t *mutex,
  dds_time_t abstime)
{
  struct timespec ts = { .tv_sec = 0, .tv_nsec = 0 };

  assert(cond != NULL);
  assert(mutex != NULL);

  if (abstime == DDS_NEVER) {
    ddsrt_cond_wait(cond, mutex);
    return true;
  }
  if (abstime > 0) {
    ts.tv_sec = (time_t) (abstime / DDS_NSECS_IN_SEC);
    ts.tv_nsec = (suseconds_t) (abstime % DDS_NSECS_IN_SEC);
  }

  switch (pthread_cond_timedwait(&cond->cond, &mutex->mutex, &ts)) {
    case 0:
      return true;
    case ETIMEDOUT:
      return false;
    default:
      break;
  }

  abort();
}

bool
ddsrt_cond_waitfor(
  ddsrt_cond_t *cond,
  ddsrt_mutex_t *mutex,
  dds_duration_t reltime)
{
  assert(cond != NULL);
  assert(mutex != NULL);

  return ddsrt_cond_waituntil(
    cond, mutex, ddsrt_time_add_duration(dds_time(), reltime));
}

void
ddsrt_cond_signal (ddsrt_cond_t *cond)
{
  assert (cond != NULL);

  if (pthread_cond_signal (&cond->cond) != 0)
    abort();
}

void
ddsrt_cond_broadcast (ddsrt_cond_t *cond)
{
  assert (cond != NULL);

  if (pthread_cond_broadcast (&cond->cond) != 0)
    abort();
}

void
ddsrt_rwlock_init (ddsrt_rwlock_t *rwlock)
{
  assert(rwlock != NULL);

#if __SunOS_5_6
  if (pthread_mutex_init(&rwlock->rwlock, NULL) != 0)
    abort();
#else
  /* process-shared attribute is set to PTHREAD_PROCESS_PRIVATE by default */
  if (pthread_rwlock_init(&rwlock->rwlock, NULL) != 0)
    abort();
#endif
}

void
ddsrt_rwlock_destroy (ddsrt_rwlock_t *rwlock)
{
  assert(rwlock != NULL);
#if __SunOS_5_6
  if (pthread_mutex_destroy(&rwlock->rwlock) != 0)
    abort();
#else
  if (pthread_rwlock_destroy(&rwlock->rwlock) != 0)
    abort();
#endif
}

void ddsrt_rwlock_read (ddsrt_rwlock_t *rwlock)
{
  int err;

  assert(rwlock != NULL);
#if __SunOS_5_6
  err = pthread_mutex_lock(&rwlock->rwlock);
#else
  err = pthread_rwlock_rdlock(&rwlock->rwlock);
#endif
  assert(err == 0);
  (void)err;
}

void ddsrt_rwlock_write (ddsrt_rwlock_t *rwlock)
{
  int err;

  assert(rwlock != NULL);
#if __SunOS_5_6
  err = pthread_mutex_lock(&rwlock->rwlock);
#else
  err = pthread_rwlock_wrlock(&rwlock->rwlock);
#endif
  assert(err == 0);
  (void)err;
}

bool ddsrt_rwlock_tryread (ddsrt_rwlock_t *rwlock)
{
  int err;

  assert(rwlock != NULL);
#if __SunOS_5_6
  err = pthread_mutex_trylock(&rwlock->rwlock);
#else
  err = pthread_rwlock_tryrdlock(&rwlock->rwlock);
#endif
  assert(err == 0 || err == EBUSY);
  return err == 0;
}

bool ddsrt_rwlock_trywrite (ddsrt_rwlock_t *rwlock)
{
  int err;

  assert(rwlock != NULL);
#if __SunOS_5_6
  err = pthread_mutex_trylock(&rwlock->rwlock);
#else
  err = pthread_rwlock_trywrlock(&rwlock->rwlock);
#endif
  assert(err == 0 || err == EBUSY);

  return err == 0;
}

void ddsrt_rwlock_unlock (ddsrt_rwlock_t *rwlock)
{
  int err;

  assert(rwlock != NULL);
#if __SunOS_5_6
  err = pthread_mutex_unlock(&rwlock->rwlock);
#else
  err = pthread_rwlock_unlock(&rwlock->rwlock);
#endif
  assert(err == 0);
  (void)err;
}

void ddsrt_once (ddsrt_once_t *control, ddsrt_once_fn init_fn)
{
  /* There are no defined errors that can be returned by pthread_once */
  (void)pthread_once(control, init_fn);
}
