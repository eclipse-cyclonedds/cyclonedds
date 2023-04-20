// Copyright(c) 2006 to 2019 ZettaScale Technology and others
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
#if DDSRT_WITH_FREERTOS
# include <FreeRTOS.h>
# include <task.h>
#elif !defined(_WIN32)
# include <sched.h>
# include <unistd.h>
#endif

#include "CUnit/Theory.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"

static int32_t min_fifo_prio = 250;
static int32_t max_fifo_prio = 250;
static int32_t max_other_prio = 60;
static int32_t min_other_prio = 250;

CU_Init(ddsrt_thread)
{
  ddsrt_init();
#if DDSRT_WITH_FREERTOS
  max_other_prio = max_fifo_prio = configMAX_PRIORITIES - 1;
  min_other_prio = min_fifo_prio = tskIDLE_PRIORITY + 1;
#elif defined(WIN32)
  max_fifo_prio = THREAD_PRIORITY_HIGHEST;
  min_fifo_prio = THREAD_PRIORITY_LOWEST;
  max_other_prio = THREAD_PRIORITY_HIGHEST;
  min_other_prio = THREAD_PRIORITY_LOWEST;
#else
  min_fifo_prio = sched_get_priority_min(SCHED_FIFO);
  max_fifo_prio = sched_get_priority_max(SCHED_FIFO);
# if !defined(_WRS_KERNEL)
  max_other_prio = sched_get_priority_max(SCHED_OTHER);
  min_other_prio = sched_get_priority_min(SCHED_OTHER);
# endif
#endif

  return 0;
}

CU_Clean(ddsrt_thread)
{
  ddsrt_fini();
  return 0;
}

typedef struct {
  int res;
  int ret;
  ddsrt_threadattr_t *attr;
} thread_arg_t;

static uint32_t thread_main(void *ptr)
{
  thread_arg_t *arg = (thread_arg_t *)ptr;
  ddsrt_threadattr_t *attr;

  assert(arg != NULL);

  attr = arg->attr;

#if DDSRT_WITH_FREERTOS
  int prio = (int)uxTaskPriorityGet(NULL);
  if (prio == attr->schedPriority) {
    arg->res = 1;
  }
#elif _WIN32
  int prio = GetThreadPriority(GetCurrentThread());
  if (prio == THREAD_PRIORITY_ERROR_RETURN)
    abort();
  if (prio == attr->schedPriority) {
    arg->res = 1;
  }
#else
  int err;
  int policy;
  struct sched_param sched;

  err = pthread_getschedparam(pthread_self(), &policy, &sched);
  if (err != 0) {
    abort();
  }
  if (((policy == SCHED_OTHER && attr->schedClass == DDSRT_SCHED_TIMESHARE) ||
       (policy == SCHED_FIFO && attr->schedClass == DDSRT_SCHED_REALTIME))
    && (sched.sched_priority == attr->schedPriority))
  {
    arg->res = 1;
  }
#endif

  return (uint32_t)arg->ret;
}

CU_TheoryDataPoints(ddsrt_thread, create_and_join) = {
  CU_DataPoints(ddsrt_sched_t, DDSRT_SCHED_TIMESHARE, DDSRT_SCHED_TIMESHARE,
                               DDSRT_SCHED_REALTIME,  DDSRT_SCHED_REALTIME),
  CU_DataPoints(int32_t *,     &min_other_prio,       &max_other_prio,
                               &min_fifo_prio,        &max_fifo_prio),
  CU_DataPoints(uint32_t,      10101,                 20202,
                               30303,                 40404)
};

CU_Theory((ddsrt_sched_t sched, int32_t *prio, uint32_t exp), ddsrt_thread, create_and_join, .timeout=60)
{
  int skip = 0;
  uint32_t res = 50505;
  dds_return_t ret;
  ddsrt_thread_t thr;
  ddsrt_threadattr_t attr;
  thread_arg_t arg;

#if DDSRT_WITH_FREERTOS
  if (sched == DDSRT_SCHED_TIMESHARE) {
    skip = 1;
    CU_PASS("FreeRTOS only support SCHED_FIFO");
  }
#elif defined(__VXWORKS__)
# if defined(_WRS_KERNEL)
  if (sched == DDSRT_SCHED_TIMESHARE) {
    skip = 1;
    CU_PASS("VxWorks DKM only supports SCHED_FIFO");
  }
# endif
#elif !defined(_WIN32)
  if (sched == DDSRT_SCHED_REALTIME && (getuid() != 0 && geteuid() != 0)) {
    skip = 1;
    CU_PASS("SCHED_FIFO requires root privileges");
  }
#endif
  if (!skip) {
    ddsrt_threadattr_init(&attr);
    attr.schedClass = sched;
    attr.schedPriority = *prio;
    memset(&arg, 0, sizeof(arg));
    arg.ret = (int32_t)exp;
    arg.attr = &attr;
    ret = ddsrt_thread_create(&thr, "thread", &attr, &thread_main, &arg);
    CU_ASSERT_EQUAL(ret, DDS_RETCODE_OK);
    if (ret == DDS_RETCODE_OK) {
      ret = ddsrt_thread_join (thr, &res);
      CU_ASSERT_EQUAL(ret, DDS_RETCODE_OK);
      CU_ASSERT_EQUAL(res, exp);
      if (ret == DDS_RETCODE_OK) {
        CU_ASSERT_EQUAL(arg.res, 1);
      }
    }
  }
}

CU_Test(ddsrt_thread, thread_id)
{
  int eq = 0;
  ddsrt_thread_t thr;
#if DDSRT_WITH_FREERTOS
  TaskHandle_t task;
#elif defined(_WIN32)
  DWORD _tid;
#else
  pthread_t _thr;
#endif

  thr = ddsrt_thread_self();

#if DDSRT_WITH_FREERTOS
  task = xTaskGetCurrentTaskHandle();
  eq = (thr.task == task);
#elif defined(_WIN32)
  _tid = GetCurrentThreadId();
  eq = (thr.tid == _tid);
#else
  _thr = pthread_self();
  eq = pthread_equal(thr.v, _thr);
#endif

  CU_ASSERT_NOT_EQUAL(eq, 0);
}


static ddsrt_mutex_t locks[2];

static uint32_t thread_main_waitforme(void *ptr)
{
  uint32_t ret = 0;
  (void)ptr;
  ddsrt_mutex_lock(&locks[0]);
  ret = 10101;
  ddsrt_mutex_unlock(&locks[0]);
  return ret;
}

static uint32_t thread_main_waitforit(void *ptr)
{
  uint32_t res = 0;
  ddsrt_thread_t *thr = (ddsrt_thread_t *)ptr;
  ddsrt_mutex_lock(&locks[1]);
  (void)ddsrt_thread_join(*thr, &res);
  ddsrt_mutex_unlock(&locks[1]);
  return res + 20202;
}

CU_Test(ddsrt_thread, stacked_join)
{
  dds_return_t ret;
  ddsrt_thread_t thrs[2];
  ddsrt_threadattr_t attr;
  uint32_t res = 0;

  ddsrt_mutex_init(&locks[0]);
  ddsrt_mutex_init(&locks[1]);
  ddsrt_mutex_lock(&locks[0]);
  ddsrt_mutex_lock(&locks[1]);
  ddsrt_threadattr_init(&attr);
  ret = ddsrt_thread_create(&thrs[0], "", &attr, &thread_main_waitforme, NULL);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ret = ddsrt_thread_create(&thrs[1], "", &attr, &thread_main_waitforit, &thrs[0]);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

  ddsrt_mutex_unlock(&locks[1]);
  dds_sleepfor(DDS_MSECS(100)); /* 100ms */
  ddsrt_mutex_unlock(&locks[0]);

  ddsrt_thread_join(thrs[1], &res);

  CU_ASSERT_EQUAL(res, 30303);

  ddsrt_mutex_destroy(&locks[0]);
  ddsrt_mutex_destroy(&locks[1]);
}

CU_Test(ddsrt_thread, attribute)
{
  ddsrt_threadattr_t attr;

  ddsrt_threadattr_init(&attr);
  CU_ASSERT_EQUAL(attr.schedClass, DDSRT_SCHED_DEFAULT);
  CU_ASSERT_EQUAL(attr.schedPriority, 0);
  CU_ASSERT_EQUAL(attr.stackSize, 0);
}
