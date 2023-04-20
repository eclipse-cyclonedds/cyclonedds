// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdint.h>

#include "CUnit/Theory.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/time.h"

CU_Init(ddsrt_sync)
{
  ddsrt_init();
  return 0;
}

CU_Clean(ddsrt_sync)
{
  ddsrt_fini();
  return 0;
}

typedef struct {
  ddsrt_atomic_uint32_t cnt;
  ddsrt_mutex_t lock;
  ddsrt_rwlock_t rwlock;
  ddsrt_cond_t cond;
  dds_time_t abstime;
  dds_time_t reltime;
} thread_arg_t;

static uint32_t mutex_lock_routine(void *ptr)
{
  int res;
  thread_arg_t *arg = (thread_arg_t *)ptr;

  ddsrt_atomic_inc32(&arg->cnt);
  ddsrt_mutex_lock(&arg->lock);
  res = ddsrt_atomic_cas32(&arg->cnt, 2UL, 4UL);
  ddsrt_mutex_unlock(&arg->lock);
  return (uint32_t)res;
}

/* This test is merely best-effort, the scheduler might schedule the main
   main thread before a lock operation is attempted by the second thread. */
CU_Test(ddsrt_sync, mutex_lock_conc)
{
  dds_return_t ret;
  ddsrt_thread_t thr;
  ddsrt_threadattr_t attr;
  thread_arg_t arg = { .cnt = DDSRT_ATOMIC_UINT32_INIT(0) };
  uint32_t res = 0;

  ddsrt_mutex_init(&arg.lock);
  ddsrt_mutex_lock(&arg.lock);
  ddsrt_threadattr_init(&attr);
  ret = ddsrt_thread_create(&thr, "mutex_lock_conc", &attr, &mutex_lock_routine, &arg);
  CU_ASSERT_EQUAL(ret, DDS_RETCODE_OK);
  while (ddsrt_atomic_ld32(&arg.cnt) == 0)
    /* Wait for thread to be scheduled. */ ;
  ddsrt_atomic_inc32(&arg.cnt);
  ddsrt_mutex_unlock(&arg.lock);
  ret = ddsrt_thread_join(thr, &res);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  CU_ASSERT_EQUAL(res, 1);
  CU_ASSERT_EQUAL(ddsrt_atomic_ld32(&arg.cnt), 4UL);
  ddsrt_mutex_destroy(&arg.lock);
}

static uint32_t mutex_trylock_routine(void *ptr)
{
  thread_arg_t *arg = (thread_arg_t *)ptr;

  if (ddsrt_mutex_trylock(&arg->lock)) {
    ddsrt_atomic_inc32(&arg->cnt);
  }

  return ddsrt_atomic_ld32(&arg->cnt);
}

CU_Test(ddsrt_sync, mutex_trylock)
{
  bool locked;
  ddsrt_mutex_t lock;

  ddsrt_mutex_init(&lock);
  locked = ddsrt_mutex_trylock(&lock);
  CU_ASSERT(locked == true);
  locked = ddsrt_mutex_trylock (&lock);
  /* NOTE: On VxWorks RTP mutexes seemingly can be locked recursively. Still,
           behavior should be consistent across targets. If this fails, fix
           the implementation instead. */
  CU_ASSERT(locked == false);
  ddsrt_mutex_unlock(&lock);
  ddsrt_mutex_destroy(&lock);
}

static uint32_t rwlock_tryread_routine(void *ptr)
{
  thread_arg_t *arg = (thread_arg_t *)ptr;

  if (ddsrt_rwlock_tryread(&arg->rwlock)) {
    ddsrt_atomic_inc32(&arg->cnt);
    ddsrt_rwlock_unlock(&arg->rwlock);
  }

  return ddsrt_atomic_ld32(&arg->cnt);
}

static uint32_t rwlock_trywrite_routine(void *ptr)
{
  thread_arg_t *arg = (thread_arg_t *)ptr;

  /* This operation should never succeed in the test, but if it does the
     result must reflect it. */
  if (ddsrt_rwlock_trywrite(&arg->rwlock)) {
    ddsrt_atomic_inc32(&arg->cnt);
    ddsrt_rwlock_unlock(&arg->rwlock);
  }

  return ddsrt_atomic_ld32(&arg->cnt);
}

CU_Test(ddsrt_sync, mutex_trylock_conc)
{
  dds_return_t ret;
  ddsrt_thread_t thr;
  ddsrt_threadattr_t attr;
  thread_arg_t arg = { .cnt = DDSRT_ATOMIC_UINT32_INIT(1) };
  uint32_t res = 0;

  ddsrt_mutex_init(&arg.lock);
  ddsrt_mutex_lock(&arg.lock);
  ddsrt_threadattr_init(&attr);
  ret = ddsrt_thread_create(&thr, "mutex_trylock_conc", &attr, &mutex_trylock_routine, &arg);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = ddsrt_thread_join(thr, &res);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  CU_ASSERT_EQUAL(res, 1);
  ddsrt_mutex_unlock(&arg.lock);
  ddsrt_mutex_destroy(&arg.lock);
}

#define READ (1)
#define TRYREAD (2)
#define WRITE (3)
#define TRYWRITE (4)

CU_TheoryDataPoints(ddsrt_sync, rwlock_trylock_conc) = {
  CU_DataPoints(uint32_t, READ,    READ,     WRITE,   WRITE),
  CU_DataPoints(uint32_t, TRYREAD, TRYWRITE, TRYREAD, TRYWRITE),
  CU_DataPoints(uint32_t, 2,       1,        1,       1)
};

CU_Theory((uint32_t lock, uint32_t trylock, uint32_t exp), ddsrt_sync, rwlock_trylock_conc)
{
  dds_return_t ret;
  ddsrt_thread_t thr;
  ddsrt_threadattr_t attr;
  ddsrt_thread_routine_t func;
  thread_arg_t arg = { .cnt = DDSRT_ATOMIC_UINT32_INIT(1) };
  uint32_t res = 0;

  ddsrt_rwlock_init(&arg.rwlock);
  if (lock == READ) {
    ddsrt_rwlock_read(&arg.rwlock);
  } else {
    ddsrt_rwlock_write(&arg.rwlock);
  }

  if (trylock == TRYREAD) {
    func = &rwlock_tryread_routine;
  } else {
    func = &rwlock_trywrite_routine;
  }

  ddsrt_threadattr_init(&attr);
  ret = ddsrt_thread_create(&thr, "rwlock_trylock_conc", &attr, func, &arg);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = ddsrt_thread_join(thr, &res);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ddsrt_rwlock_unlock(&arg.rwlock);
  CU_ASSERT_EQUAL(res, exp);
  ddsrt_rwlock_destroy(&arg.rwlock);
}

/* An atomic read is used for synchronization because it is important that the
   threads try to access the once control concurrently as much as possible. Of
   course, this is only best-effort as there is no guarantee that
   initialization is actually tried concurrently. */
static ddsrt_atomic_uint32_t once_count = DDSRT_ATOMIC_UINT32_INIT(0);
static ddsrt_once_t once_control = DDSRT_ONCE_INIT;

#define ONCE_THREADS (8)

static void do_once(void)
{
  ddsrt_atomic_inc32(&once_count);
}

static uint32_t once_routine(void *ptr)
{
  (void)ptr;
  while (ddsrt_atomic_ld32(&once_count) == 0)
    /* Wait for the go-ahead. */ ;
  ddsrt_once(&once_control, &do_once);
  return ddsrt_atomic_ld32(&once_count);
}

CU_Test(ddsrt_sync, once_conc)
{
  dds_return_t ret;
  ddsrt_thread_t thrs[ONCE_THREADS];
  ddsrt_threadattr_t attr;
  uint32_t res;
  char buf[32];

  ddsrt_threadattr_init(&attr);
  for (int i = 0; i < ONCE_THREADS; i++) {
    (void)snprintf(buf, sizeof(buf), "once_conc%d", i + 1);
    ret = ddsrt_thread_create(&thrs[i], buf, &attr, &once_routine, NULL);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  }

  ddsrt_atomic_st32(&once_count, 1);

  for (int i = 0; i < ONCE_THREADS; i++) {
    res = 0;
    ret = ddsrt_thread_join(thrs[i], &res);
    CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
    CU_ASSERT_EQUAL(res, 2);
  }

  ddsrt_once(&once_control, &do_once);
  CU_ASSERT_EQUAL(ddsrt_atomic_ld32(&once_count), 2);
}

static uint32_t waitfor_routine(void *ptr)
{
  dds_time_t before, after;
  dds_duration_t reltime;
  thread_arg_t *arg = (thread_arg_t *)ptr;
  uint32_t cnt = 0, res = 0;

  while (ddsrt_atomic_ld32(&arg->cnt) == 0)
    /* Wait for go-ahead. */ ;
  ddsrt_mutex_lock(&arg->lock);
  before = dds_time();
  reltime = arg->reltime;
  while (ddsrt_cond_waitfor(&arg->cond, &arg->lock, reltime)) {
    after = dds_time();
    if ((after - before) < arg->reltime && (after - before) > 0) {
      reltime = arg->reltime - (after - before);
    } else {
      reltime = 0;
    }
    cnt++;
  }
  after = dds_time();
  reltime = after - before;
  fprintf(stderr, "waited for %"PRId64" (nanoseconds)\n", reltime);
  fprintf(stderr, "expected to wait %"PRId64" (nanoseconds)\n", arg->reltime);
  fprintf(stderr, "woke up %"PRIu32" times\n", cnt);
  ddsrt_mutex_unlock(&arg->lock);
  if (reltime >= arg->reltime) {
    /* Ensure that the condition variable at least waited for the amount of
       time so that time calculation is not (too) broken.*/
    res = cnt < 3; /* An arbitrary number to ensure the implementation
                      did not just spin, aka is completely broken. */
  }

  return res;
}

CU_Test(ddsrt_sync, cond_waitfor)
{
  dds_return_t rc;
  ddsrt_thread_t thr;
  ddsrt_threadattr_t attr;
  thread_arg_t arg = { .cnt = DDSRT_ATOMIC_UINT32_INIT(0), .reltime = DDS_MSECS(100) };
  uint32_t res = 0;

  ddsrt_mutex_init(&arg.lock);
  ddsrt_cond_init(&arg.cond);
  ddsrt_mutex_lock(&arg.lock);
  ddsrt_threadattr_init(&attr);
  rc = ddsrt_thread_create(&thr, "cond_waitfor", &attr, &waitfor_routine, &arg);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  ddsrt_mutex_unlock(&arg.lock);
  /* Give go-ahead. */
  ddsrt_atomic_inc32(&arg.cnt);
  /* Wait a little longer than the waiting thread. */
  dds_sleepfor(arg.reltime * 2);
  /* Send a signal too avoid blocking indefinitely. */
  ddsrt_cond_signal(&arg.cond);
  rc = ddsrt_thread_join(thr, &res);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  CU_ASSERT_EQUAL(res, 1);
}

static uint32_t waituntil_routine(void *ptr)
{
  dds_time_t after;
  thread_arg_t *arg = (thread_arg_t *)ptr;
  uint32_t cnt = 0, res = 0;

  ddsrt_mutex_lock(&arg->lock);
  while(ddsrt_cond_waituntil(&arg->cond, &arg->lock, arg->abstime)) {
    cnt++;
  }
  after = dds_time();
  ddsrt_mutex_unlock(&arg->lock);
  fprintf(stderr, "waited until %"PRId64" (nanoseconds)\n", after);
  fprintf(stderr, "expected to wait until %"PRId64" (nanoseconds)\n", arg->abstime);
  fprintf(stderr, "woke up %"PRIu32" times\n", cnt);
  if (after > arg->abstime) {
    res = cnt < 3; /* An arbitrary number to ensure the implementation
                      did not just spin, aka is completely broken. */
  }

  return res;
}

CU_Test(ddsrt_sync, cond_waituntil)
{
  dds_return_t rc;
  dds_duration_t delay = DDS_MSECS(100);
  ddsrt_thread_t thr;
  ddsrt_threadattr_t attr;
  thread_arg_t arg = { .cnt = DDSRT_ATOMIC_UINT32_INIT(0) };
  uint32_t res = 0;

  arg.abstime = dds_time() + delay;
  ddsrt_mutex_init(&arg.lock);
  ddsrt_cond_init(&arg.cond);
  ddsrt_mutex_lock(&arg.lock);
  ddsrt_threadattr_init(&attr);
  rc = ddsrt_thread_create(&thr, "cond_waituntil", &attr, &waituntil_routine, &arg);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  ddsrt_mutex_unlock(&arg.lock);
  dds_sleepfor(delay * 2);
  /* Send a signal too avoid blocking indefinitely. */
  ddsrt_cond_signal(&arg.cond);
  rc = ddsrt_thread_join(thr, &res);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  CU_ASSERT_EQUAL(res, 1);
}
