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
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>
#include <sys/time.h>

#include "os/os.h"

#if HAVE_LKST
#include "lkst.h"
#include "mach/mach_time.h"

extern int ospl_lkst_enabled;
#endif

static bool ospl_mtx_prio_inherit = false;
#if HAVE_LKST
int ospl_lkst_enabled;
#endif

#if defined __GLIBC_PREREQ
#if __GLIBC_PREREQ(2,5)
#define OSPL_PRIO_INHERIT_SUPPORTED
#endif
#endif

#if HAVE_LKST
static void random_delay(unsigned long long t)
{
  unsigned z = (unsigned)((t * 16292676669999574021ull) >> 32);
  struct timespec ts;
  /* delay ~ 10% of the lock operations */
  if ((z & 1023) < 900)
    return;
  /* delay by up to < 16ms */
  ts.tv_sec = 0;
  ts.tv_nsec = (z >> 10) & 16383;
  (void) nanosleep (&ts, NULL);
}
#endif

void os_syncModuleInit(void)
{
#if HAVE_LKST
  ospl_lkst_enabled = lkst_init (1);
#endif
  ospl_mtx_prio_inherit = 0;
}

void os_syncModuleExit(void)
{
#if HAVE_LKST
  if (ospl_lkst_enabled)
    lkst_fini ();
#endif
}

os_result os_mutexSetPriorityInheritanceMode(bool enabled)
{
  ospl_mtx_prio_inherit = enabled;
  return os_resultSuccess;
}

void os_mutexInit (os_mutex *mutex)
{
  int shared;
  assert (mutex != NULL);

  pthread_mutex_init (&mutex->mutex, NULL);
#if HAVE_LKST
  if (ospl_lkst_enabled)
    lkst_track_init (mutex, shared ? LKST_MF_SHARED : 0);
#else
  (void)shared;
#endif
}

void os_mutexDestroy (os_mutex *mutex)
{
  assert (mutex != NULL);
#if HAVE_LKST
  if (ospl_lkst_enabled)
    lkst_track_destroy (mutex);
#endif

  if (pthread_mutex_destroy (&mutex->mutex) != 0)
    abort();
}

void os_mutexLock (os_mutex *mutex)
{
  assert (mutex != NULL);

#if HAVE_LKST
  if (!ospl_lkst_enabled)
#endif
  {
    if (pthread_mutex_lock (&mutex->mutex) != 0)
      abort();
  }
#if HAVE_LKST
  else
  {
    unsigned long long t = mach_absolute_time (), dt;
    if (ospl_lkst_enabled > 1)
      random_delay (t);
    if (pthread_mutex_trylock (&mutex->mutex) == 0)
      dt = 0;
    else
    {
      if (pthread_mutex_lock (&mutex->mutex) != 0)
        abort();
      dt = 1 | (mach_absolute_time () - t);
    }
    lkst_track_op (mutex, LKST_LOCK, t, dt);
  }
#endif
}

os_result os_mutexLock_s (os_mutex *mutex)
{
  int result;
  assert (mutex != NULL);

#if HAVE_LKST
  if (!ospl_lkst_enabled)
#endif
  {
    result = pthread_mutex_lock (&mutex->mutex);
  }
#if HAVE_LKST
  else
  {
    unsigned long long t = mach_absolute_time (), dt;
    if (ospl_lkst_enabled > 1)
      random_delay (t);
    if ((result = pthread_mutex_trylock (&mutex->mutex)) == 0)
      dt = 0;
    else
    {
      result = pthread_mutex_lock (&mutex->mutex);
      dt = 1 | (mach_absolute_time () - t);
    }
    lkst_track_op (mutex, LKST_LOCK, t, dt);
  }
#endif
  return (result == 0) ? os_resultSuccess : os_resultFail;
}

os_result os_mutexTryLock (os_mutex *mutex)
{
  int result;
  assert (mutex != NULL);

  result = pthread_mutex_trylock (&mutex->mutex);
  if (result != 0 && result != EBUSY)
    abort();
#if HAVE_LKST
  if (result == 0 && ospl_lkst_enabled)
    lkst_track_op (mutex, LKST_LOCK, mach_absolute_time (), 0);
#endif
  return (result == 0) ? os_resultSuccess : os_resultBusy;
}

void os_mutexUnlock (os_mutex *mutex)
{
  assert (mutex != NULL);

#if HAVE_LKST
  if (ospl_lkst_enabled)
    lkst_track_op (mutex, LKST_UNLOCK, mach_absolute_time (), 0);
#endif
  if (pthread_mutex_unlock (&mutex->mutex) != 0)
    abort();
}

void os_condInit (os_cond *cond, os_mutex *dummymtx __attribute__ ((unused)))
{
  assert (cond != NULL);

  pthread_cond_init (&cond->cond, NULL);
}

void os_condDestroy (os_cond *cond)
{
  assert (cond != NULL);

  if (pthread_cond_destroy (&cond->cond) != 0)
    abort();
}

void os_condWait (os_cond *cond, os_mutex *mutex)
{
  assert (cond != NULL);
  assert (mutex != NULL);

#if HAVE_LKST
  if (ospl_lkst_enabled)
    lkst_track_op (mutex, LKST_UNLOCK, mach_absolute_time (), 0);
#endif
  if (pthread_cond_wait (&cond->cond, &mutex->mutex) != 0)
    abort();
#if HAVE_LKST
  /* Have no way of determining whether it was uncontended or not, and
   if not, how long the wait was. */
  if (ospl_lkst_enabled)
    lkst_track_op (mutex, LKST_LOCK, mach_absolute_time (), 0);
#endif
}

os_result os_condTimedWait (os_cond *cond, os_mutex *mutex, const os_time *time)
{
  struct timespec t;
  int result;
  os_time wakeup_time;
  struct timeval tv;
  os_time rt;

  assert (cond != NULL);
  assert (mutex != NULL);
  assert (time != NULL);

  (void) gettimeofday (&tv, NULL);

  rt.tv_sec = (os_timeSec) tv.tv_sec;
  rt.tv_nsec = (int32_t) tv.tv_usec * 1000;

  wakeup_time = os_timeAdd (rt, *time);
  t.tv_sec = wakeup_time.tv_sec;
  t.tv_nsec = wakeup_time.tv_nsec;

#if HAVE_LKST
  if (ospl_lkst_enabled)
    lkst_track_op (mutex, LKST_UNLOCK, mach_absolute_time (), 0);
#endif
  /* By default Darwin uses the realtime clock in pthread_cond_timedwait().
   * Unfortunately Darwin has not (yet) implemented
   * pthread_condattr_setclock(), so we cannot tell it to use the
   * the monotonic clock. */
  result = pthread_cond_timedwait (&cond->cond, &mutex->mutex, &t);
  if (result != 0 && result != ETIMEDOUT)
    abort();
#if HAVE_LKST
  /* Have no way of determining whether it was uncontended or not, and
   if not, how long the wait was. */
  if (ospl_lkst_enabled)
    lkst_track_op (mutex, LKST_LOCK, mach_absolute_time (), 0);
#endif
  return (result == ETIMEDOUT) ? os_resultTimeout : os_resultSuccess;
}

void os_condSignal (os_cond *cond)
{
  assert (cond != NULL);

  if (pthread_cond_signal (&cond->cond) != 0)
    abort();
}

void os_condBroadcast (os_cond *cond)
{
  assert (cond != NULL);

  if (pthread_cond_broadcast (&cond->cond) != 0)
    abort();
}

void os_rwlockInit(os_rwlock *rwlock)
{
    int err = 0;

    assert(rwlock != NULL);

    /* process-shared attribute is set to PTHREAD_PROCESS_PRIVATE by default */
    if ((err = pthread_rwlock_init(&rwlock->rwlock, NULL)) != 0) {
        abort();
    }
}

void os_rwlockDestroy(os_rwlock *rwlock)
{
    int err;

    assert(rwlock != NULL);

    if ((err = pthread_rwlock_destroy(&rwlock->rwlock)) != 0) {
        abort();
    }
}

void os_rwlockRead(os_rwlock *rwlock)
{
    int err;

    assert(rwlock != NULL);

    err = pthread_rwlock_rdlock(&rwlock->rwlock);
    assert(err == 0);
    (void)err;
}

void os_rwlockWrite(os_rwlock *rwlock)
{
    int err;

    assert(rwlock != NULL);

    err = pthread_rwlock_wrlock(&rwlock->rwlock);
    assert(err == 0);
    (void)err;
}

os_result os_rwlockTryRead(os_rwlock *rwlock)
{
    int err;

    assert(rwlock != NULL);

    err = pthread_rwlock_tryrdlock(&rwlock->rwlock);
    assert(err == 0 || err == EBUSY);

    return err == 0 ? os_resultSuccess : os_resultBusy;
}

os_result os_rwlockTryWrite(os_rwlock *rwlock)
{
    int err;

    assert(rwlock != NULL);

    err = pthread_rwlock_trywrlock(&rwlock->rwlock);
    assert(err == 0 || err == EBUSY);

    return err == 0 ? os_resultSuccess : os_resultBusy;
}

void os_rwlockUnlock(os_rwlock *rwlock)
{
    int err;

    assert(rwlock != NULL);

    err = pthread_rwlock_unlock(&rwlock->rwlock);
    assert(err == 0);
    (void)err;
}

void
os_once(
    _Inout_ os_once_t *control,
    _In_ os_once_fn init_fn)
{
    /* There are no defined errors that can be returned by pthread_once */
    (void)pthread_once(control, init_fn);
}
