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
  pthread_mutex_init (&mutex->mutex, NULL);
}

void ddsrt_mutex_destroy (ddsrt_mutex_t *mutex)
{
  if (pthread_mutex_destroy (&mutex->mutex) != 0)
    abort ();
}

void ddsrt_mutex_lock (ddsrt_mutex_t *mutex)
{
  if (pthread_mutex_lock (&mutex->mutex) != 0)
    abort ();
}

bool ddsrt_mutex_trylock (ddsrt_mutex_t *mutex)
{
  int err;
  err = pthread_mutex_trylock (&mutex->mutex);
  if (err != 0 && err != EBUSY)
    abort ();
  return (err == 0);
}

void ddsrt_mutex_unlock (ddsrt_mutex_t *mutex)
{
  if (pthread_mutex_unlock (&mutex->mutex) != 0)
    abort ();
}

enum ddsrt_clockid {
  DDSRT_CLOCK_WALLCLOCK,
  DDSRT_CLOCK_MONOTONIC,
  DDSRT_CLOCK_ELAPSED
};

#ifdef DDSRT_HAVE_CONDATTR_SETCLOCK
static void ddsrt_condattr_setclock (pthread_condattr_t *attr, enum ddsrt_clockid clockid)
{
  // note: the mapping must work, but it is not a given that it is always this
  // (macOS would be different, if it had pthread_condattr_setclock)
  switch (clockid)
  {
    case DDSRT_CLOCK_WALLCLOCK:
      if (pthread_condattr_setclock (attr, CLOCK_REALTIME) != 0)
        abort ();
      break;
    case DDSRT_CLOCK_MONOTONIC:
    case DDSRT_CLOCK_ELAPSED:
      if (pthread_condattr_setclock (attr, CLOCK_MONOTONIC) != 0)
        abort ();
      break;
  }
}
#endif

static void ddsrt_cond_init_impl (pthread_cond_t *cond, enum ddsrt_clockid clockid)
{
  pthread_condattr_t condattr;
  pthread_condattr_init (&condattr);
#ifdef DDSRT_HAVE_CONDATTR_SETCLOCK
  ddsrt_condattr_setclock (&condattr, clockid);
#else
  (void) clockid;
#endif
  if (pthread_cond_init (cond, &condattr) != 0)
    abort ();
  pthread_condattr_destroy (&condattr);
}

void ddsrt_cond_init (ddsrt_cond_t *cond)
{
  pthread_cond_init (&cond->cond, NULL);
}

void ddsrt_cond_wctime_init (ddsrt_cond_wctime_t *cond)
{
  ddsrt_cond_init_impl (&cond->cond, DDSRT_CLOCK_WALLCLOCK);
}

void ddsrt_cond_mtime_init (ddsrt_cond_mtime_t *cond)
{
  ddsrt_cond_init_impl (&cond->cond, DDSRT_CLOCK_MONOTONIC);
}

void ddsrt_cond_etime_init (ddsrt_cond_etime_t *cond)
{
  ddsrt_cond_init_impl (&cond->cond, DDSRT_CLOCK_ELAPSED);
}

void ddsrt_cond_destroy (ddsrt_cond_t *cond)
{
  if (pthread_cond_destroy (&cond->cond) != 0)
    abort ();
}

void ddsrt_cond_wctime_destroy (ddsrt_cond_wctime_t *cond)
{
  if (pthread_cond_destroy (&cond->cond) != 0)
    abort ();
}

void ddsrt_cond_mtime_destroy (ddsrt_cond_mtime_t *cond)
{
  if (pthread_cond_destroy (&cond->cond) != 0)
    abort ();
}

void ddsrt_cond_etime_destroy (ddsrt_cond_etime_t *cond)
{
  if (pthread_cond_destroy (&cond->cond) != 0)
    abort ();
}

void ddsrt_cond_wait (ddsrt_cond_t *cond, ddsrt_mutex_t *mutex)
{
  if (pthread_cond_wait (&cond->cond, &mutex->mutex) != 0)
    abort ();
}

void ddsrt_cond_wctime_wait (ddsrt_cond_wctime_t *cond, ddsrt_mutex_t *mutex)
{
  if (pthread_cond_wait (&cond->cond, &mutex->mutex) != 0)
    abort ();
}

void ddsrt_cond_mtime_wait (ddsrt_cond_mtime_t *cond, ddsrt_mutex_t *mutex)
{
  if (pthread_cond_wait (&cond->cond, &mutex->mutex) != 0)
    abort ();
}

void ddsrt_cond_etime_wait (ddsrt_cond_etime_t *cond, ddsrt_mutex_t *mutex)
{
  if (pthread_cond_wait (&cond->cond, &mutex->mutex) != 0)
    abort ();
}

static bool ddsrt_cond_waituntil_impl (pthread_cond_t *cond, pthread_mutex_t *mutex, int64_t abstime)
{
  struct timespec ts = { .tv_sec = 0, .tv_nsec = 0 };
  if (abstime == DDS_NEVER) {
    pthread_cond_wait(cond, mutex);
    return true;
  }
  if (abstime > 0) {
    ts.tv_sec = (time_t) (abstime / DDS_NSECS_IN_SEC);
    ts.tv_nsec = (suseconds_t) (abstime % DDS_NSECS_IN_SEC);
  }
  switch (pthread_cond_timedwait (cond, mutex, &ts)) {
    case 0:
      return true;
    case ETIMEDOUT:
      return false;
    default:
      break;
  }
  abort();
  return false;
}

bool ddsrt_cond_wctime_waituntil (ddsrt_cond_wctime_t *cond, ddsrt_mutex_t *mutex, ddsrt_wctime_t abstime)
{
  return ddsrt_cond_waituntil_impl(&cond->cond, &mutex->mutex, abstime.v);
}

#ifdef DDSRT_HAVE_CONDATTR_SETCLOCK
bool ddsrt_cond_mtime_waituntil (ddsrt_cond_mtime_t *cond, ddsrt_mutex_t *mutex, ddsrt_mtime_t abstime)
{
  return ddsrt_cond_waituntil_impl(&cond->cond, &mutex->mutex, abstime.v);
}

bool ddsrt_cond_etime_waituntil (ddsrt_cond_etime_t *cond, ddsrt_mutex_t *mutex, ddsrt_etime_t abstime)
{
  return ddsrt_cond_waituntil_impl(&cond->cond, &mutex->mutex, abstime.v);
}
#else
static bool ddsrt_cond_waituntil_clockconv_impl (pthread_cond_t *cond, pthread_mutex_t *mutex, int64_t abstime, int64_t tnow_wc, int64_t tnow_src)
{
  if (abstime == DDS_NEVER)
    return ddsrt_cond_waituntil_impl (cond, mutex, abstime);
  else if (abstime < tnow_src)
    return false;
  else
    return ddsrt_cond_waituntil_impl (cond, mutex, ddsrt_time_add_duration (tnow_wc, abstime - tnow_src));
}

bool ddsrt_cond_mtime_waituntil (ddsrt_cond_mtime_t *cond, ddsrt_mutex_t *mutex, ddsrt_mtime_t abstime)
{
  const ddsrt_mtime_t tnow_m = ddsrt_time_monotonic ();
  const ddsrt_wctime_t tnow_wc = ddsrt_time_wallclock ();
  return ddsrt_cond_waituntil_clockconv_impl (&cond->cond, &mutex->mutex, abstime.v, tnow_wc.v, tnow_m.v);
}

bool ddsrt_cond_etime_waituntil (ddsrt_cond_etime_t *cond, ddsrt_mutex_t *mutex, ddsrt_etime_t abstime)
{
  const ddsrt_etime_t tnow_e = ddsrt_time_elapsed ();
  const ddsrt_wctime_t tnow_wc = ddsrt_time_wallclock ();
  return ddsrt_cond_waituntil_clockconv_impl (&cond->cond, &mutex->mutex, abstime.v, tnow_wc.v, tnow_e.v);
}
#endif

void ddsrt_cond_signal (ddsrt_cond_t *cond)
{
  if (pthread_cond_signal (&cond->cond) != 0)
    abort ();
}

void ddsrt_cond_wctime_signal (ddsrt_cond_wctime_t *cond)
{
  if (pthread_cond_signal (&cond->cond) != 0)
    abort ();
}

void ddsrt_cond_mtime_signal (ddsrt_cond_mtime_t *cond)
{
  if (pthread_cond_signal (&cond->cond) != 0)
    abort ();
}

void ddsrt_cond_etime_signal (ddsrt_cond_etime_t *cond)
{
  if (pthread_cond_signal (&cond->cond) != 0)
    abort ();
}

void ddsrt_cond_broadcast (ddsrt_cond_t *cond)
{
  if (pthread_cond_broadcast (&cond->cond) != 0)
    abort ();
}

void ddsrt_cond_wctime_broadcast (ddsrt_cond_wctime_t *cond)
{
  if (pthread_cond_broadcast (&cond->cond) != 0)
    abort ();
}

void ddsrt_cond_mtime_broadcast (ddsrt_cond_mtime_t *cond)
{
  if (pthread_cond_broadcast (&cond->cond) != 0)
    abort ();
}

void ddsrt_cond_etime_broadcast (ddsrt_cond_etime_t *cond)
{
  if (pthread_cond_broadcast (&cond->cond) != 0)
    abort ();
}

void ddsrt_rwlock_init (ddsrt_rwlock_t *rwlock)
{
#if __SunOS_5_6
  if (pthread_mutex_init (&rwlock->rwlock, NULL) != 0)
    abort ();
#else
  /* process-shared attribute is set to PTHREAD_PROCESS_PRIVATE by default */
  if (pthread_rwlock_init (&rwlock->rwlock, NULL) != 0)
    abort ();
#endif
}

void ddsrt_rwlock_destroy (ddsrt_rwlock_t *rwlock)
{
#if __SunOS_5_6
  if (pthread_mutex_destroy (&rwlock->rwlock) != 0)
    abort ();
#else
  if (pthread_rwlock_destroy (&rwlock->rwlock) != 0)
    abort ();
#endif
}

void ddsrt_rwlock_read (ddsrt_rwlock_t *rwlock)
{
  int err;
#if __SunOS_5_6
  err = pthread_mutex_lock (&rwlock->rwlock);
#else
  err = pthread_rwlock_rdlock (&rwlock->rwlock);
#endif
  assert (err == 0);
  (void)err;
}

void ddsrt_rwlock_write (ddsrt_rwlock_t *rwlock)
{
  int err;
#if __SunOS_5_6
  err = pthread_mutex_lock (&rwlock->rwlock);
#else
  err = pthread_rwlock_wrlock (&rwlock->rwlock);
#endif
  assert (err == 0);
  (void)err;
}

bool ddsrt_rwlock_tryread (ddsrt_rwlock_t *rwlock)
{
  int err;
#if __SunOS_5_6
  err = pthread_mutex_trylock (&rwlock->rwlock);
#else
  err = pthread_rwlock_tryrdlock (&rwlock->rwlock);
#endif
  assert (err == 0 || err == EBUSY);
  return err == 0;
}

bool ddsrt_rwlock_trywrite (ddsrt_rwlock_t *rwlock)
{
  int err;
#if __SunOS_5_6
  err = pthread_mutex_trylock (&rwlock->rwlock);
#else
  err = pthread_rwlock_trywrlock (&rwlock->rwlock);
#endif
  assert (err == 0 || err == EBUSY);
  return err == 0;
}

void ddsrt_rwlock_unlock (ddsrt_rwlock_t *rwlock)
{
  int err;
#if __SunOS_5_6
  err = pthread_mutex_unlock (&rwlock->rwlock);
#else
  err = pthread_rwlock_unlock (&rwlock->rwlock);
#endif
  assert (err == 0);
  (void)err;
}

void ddsrt_once (ddsrt_once_t *control, ddsrt_once_fn init_fn)
{
  /* There are no defined errors that can be returned by pthread_once */
  (void)pthread_once (control, init_fn);
}
