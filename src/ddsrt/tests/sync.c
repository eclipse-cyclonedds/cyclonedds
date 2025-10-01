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
  ddsrt_cond_wctime_t cond;
  ddsrt_wctime_t abstime;
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
  CU_ASSERT_EQ (ret, DDS_RETCODE_OK);
  while (ddsrt_atomic_ld32(&arg.cnt) == 0)
    /* Wait for thread to be scheduled. */ ;
  ddsrt_atomic_inc32(&arg.cnt);
  ddsrt_mutex_unlock(&arg.lock);
  ret = ddsrt_thread_join(thr, &res);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT_EQ (res, 1);
  CU_ASSERT_EQ (ddsrt_atomic_ld32(&arg.cnt), 4UL);
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
  CU_ASSERT_EQ (locked, true);
  locked = ddsrt_mutex_trylock (&lock);
  /* NOTE: On VxWorks RTP mutexes seemingly can be locked recursively. Still,
           behavior should be consistent across targets. If this fails, fix
           the implementation instead. */
  CU_ASSERT_EQ (locked, false);
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
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = ddsrt_thread_join(thr, &res);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT_EQ (res, 1);
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
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ret = ddsrt_thread_join(thr, &res);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  ddsrt_rwlock_unlock(&arg.rwlock);
  CU_ASSERT_EQ (res, exp);
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
    CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  }

  ddsrt_atomic_st32(&once_count, 1);

  for (int i = 0; i < ONCE_THREADS; i++) {
    res = 0;
    ret = ddsrt_thread_join(thrs[i], &res);
    CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
    CU_ASSERT_EQ (res, 2);
  }

  ddsrt_once(&once_control, &do_once);
  CU_ASSERT_EQ (ddsrt_atomic_ld32(&once_count), 2);
}

#define WAITUNTIL_ARG(tb_) \
  struct waituntil_##tb_##_arg { \
    ddsrt_atomic_uint32_t flag; \
    ddsrt_mutex_t lock; \
    ddsrt_cond_t sync_cond; \
    ddsrt_cond_##tb_##_t cond; \
    ddsrt_##tb_##_t abstimeout; \
  }

WAITUNTIL_ARG(wctime);
WAITUNTIL_ARG(mtime);
WAITUNTIL_ARG(etime);

static uint32_t waituntil_check (const char *tname, int64_t after, int64_t abstimeout, uint32_t cnt)
{
  fprintf (stderr, "%s: expected to wait until %"PRId64" ns\n", tname, abstimeout);
  fprintf (stderr, "%s: waited until %"PRId64" ns = exp + %"PRId64" ns\n", tname, after, after - abstimeout);
  fprintf (stderr, "%s: woke up %"PRIu32" times\n", tname, cnt);
  uint32_t res = 0;
  /* CI persistently gives some grief, so add a safety margin */
  if (after > abstimeout - DDS_MSECS (10))
  {
    /* An arbitrary number to ensure the implementation did not just spin, aka is completely broken. */
    res = cnt < 3;
  }
  return res;
}

#define WAITUNTIL_THREAD(tb_, tname_) \
  static uint32_t waituntil_##tb_##_routine (void *ptr) \
  { \
    struct waituntil_##tb_##_arg * const arg = ptr; \
    uint32_t cnt = 0; \
    ddsrt_mutex_lock (&arg->lock); \
    ddsrt_atomic_st32 (&arg->flag, 1); \
    ddsrt_cond_signal (&arg->sync_cond); \
    while (ddsrt_cond_##tb_##_waituntil (&arg->cond, &arg->lock, arg->abstimeout)) \
      cnt++; \
    ddsrt_##tb_##_t after = ddsrt_time_##tname_ (); \
    ddsrt_mutex_unlock (&arg->lock); \
    return waituntil_check (#tname_, after.v, arg->abstimeout.v, cnt); \
  }

WAITUNTIL_THREAD(wctime, wallclock)
WAITUNTIL_THREAD(mtime, monotonic)
WAITUNTIL_THREAD(etime, elapsed)

#define COND_WAITUNTIL_DELAY DDS_MSECS(100)

#define WAITUNTIL_TEST(tb_, tname_) \
  static void do_test_cond_waituntil_##tb_ (void) \
  { \
    ddsrt_thread_t thr; \
    ddsrt_threadattr_t tattr; \
    ddsrt_threadattr_init (&tattr); \
    struct waituntil_##tb_##_arg arg = { .flag = DDSRT_ATOMIC_UINT32_INIT (0) }; \
    ddsrt_mutex_init (&arg.lock); \
    ddsrt_cond_init (&arg.sync_cond); \
    ddsrt_cond_##tb_##_init (&arg.cond); \
    arg.abstimeout = ddsrt_##tb_##_add_duration (ddsrt_time_##tname_ (), COND_WAITUNTIL_DELAY); \
    ddsrt_mutex_lock (&arg.lock); \
    dds_return_t rc = ddsrt_thread_create (&thr, "cond_waituntil", &tattr, &waituntil_##tb_##_routine, &arg); \
    CU_ASSERT_EQ_FATAL (rc, DDS_RETCODE_OK); \
    while (ddsrt_atomic_ld32 (&arg.flag) == 0) \
      ddsrt_cond_wait (&arg.sync_cond, &arg.lock); \
    ddsrt_mutex_unlock (&arg.lock); \
    dds_sleepfor (2 * COND_WAITUNTIL_DELAY); \
    ddsrt_cond_##tb_##_signal (&arg.cond); \
    uint32_t res = 0; \
    rc = ddsrt_thread_join (thr, &res); \
    CU_ASSERT_EQ_FATAL (rc, DDS_RETCODE_OK); \
    CU_ASSERT_EQ (res, 1); \
  }

WAITUNTIL_TEST(wctime, wallclock)
WAITUNTIL_TEST(mtime, monotonic)
WAITUNTIL_TEST(etime, elapsed)

CU_Test(ddsrt_sync, cond_waituntil_wctime)
{
  do_test_cond_waituntil_wctime ();
}

CU_Test(ddsrt_sync, cond_waituntil_mtime)
{
  do_test_cond_waituntil_mtime ();
}

CU_Test(ddsrt_sync, cond_waituntil_etime)
{
  do_test_cond_waituntil_etime ();
}
