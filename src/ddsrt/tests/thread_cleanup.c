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
#include <stdint.h>

#include "CUnit/Test.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"

CU_Init(ddsrt_thread_cleanup)
{
  ddsrt_init();
  return 0;
}

CU_Clean(ddsrt_thread_cleanup)
{
  ddsrt_fini();
  return 0;
}

#define THREAD_RESET_1 (1<<0)
#define THREAD_RESET_2 (1<<1)
#define THREAD_RUN_OFFSET (4)
#define THREAD_RUN_1 (1<<(THREAD_RUN_OFFSET))
#define THREAD_RUN_2 (1<<(THREAD_RUN_OFFSET + 1))

struct thread_argument {
  int flags;
  int pop;
  int one;
  int two;
  int executed;
  int cancelled;
  int block;
  ddsrt_mutex_t *mutex;
  ddsrt_thread_t thread;
};

static struct thread_argument *
make_thread_argument(
  int flags, int pop, int one, int two)
{
  struct thread_argument *targ = ddsrt_malloc(sizeof(*targ));
  memset(targ, 0, sizeof(*targ));
  targ->flags = flags;
  targ->pop = pop;
  targ->one = one;
  targ->two = two;

  return targ;
}

static void
reset_one(
  void *arg)
{
  struct thread_argument *targ = (struct thread_argument *)arg;
  targ->one = 0;
  targ->executed++;
}

static void
reset_two(
    void *arg)
{
  struct thread_argument *targ = (struct thread_argument *)arg;
  targ->two = 0;
  targ->executed++;
}

static uint32_t
thread_main(
  void *arg)
{
  int pushed = 0;
  int popped = 0;
  int execute = 0;
  struct thread_argument *targ = (struct thread_argument *)arg;

  if (targ->flags & THREAD_RESET_1) {
    ddsrt_thread_cleanup_push(&reset_one, arg);
    pushed++;
  }
  if (targ->flags & THREAD_RESET_2) {
    ddsrt_thread_cleanup_push(&reset_two, arg);
    pushed++;
  }

  assert(targ->pop <= pushed);

  if (targ->block) {
    ddsrt_mutex_lock(targ->mutex);
  }

  while (popped < targ->pop) {
    execute = 1 << (THREAD_RUN_OFFSET + (targ->pop - (popped + 1)));
    ddsrt_thread_cleanup_pop(targ->flags & execute);
    targ->cancelled++;
    popped++;
  }

  if (targ->block) {
    ddsrt_mutex_unlock(targ->mutex);
  }

  return 0;
}

static void
setup(
  struct thread_argument *arg)
{
  dds_return_t rc;
  ddsrt_thread_t thr;
  ddsrt_threadattr_t attr;

  uint32_t tres = 0;

  ddsrt_threadattr_init(&attr);
  rc = ddsrt_thread_create(&thr, "", &attr, &thread_main, (void *)arg);
  CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);

  arg->thread = thr;
  if (!arg->block) {
    rc = ddsrt_thread_join(thr, &tres);
    CU_ASSERT_EQUAL_FATAL(rc, DDS_RETCODE_OK);
  }
}

/* verify the cleanup routine is called */
CU_Test(ddsrt_thread_cleanup, push_one)
{
  int flags = THREAD_RESET_1;
  struct thread_argument *targ = make_thread_argument(flags, 0, 1, 2);
  setup(targ);

  CU_ASSERT_EQUAL(targ->one, 0);
  CU_ASSERT_EQUAL(targ->two, 2);
  CU_ASSERT_EQUAL(targ->executed, 1);
  CU_ASSERT_EQUAL(targ->cancelled, 0);

  ddsrt_free(targ);
}

/* verify all cleanup routines are called if multiple are registered */
CU_Test(ddsrt_thread_cleanup, push_two)
{
  int flags = THREAD_RESET_1 | THREAD_RESET_2;
  struct thread_argument *targ = make_thread_argument(flags, 0, 1, 2);
  setup(targ);

  CU_ASSERT_EQUAL(targ->one, 0);
  CU_ASSERT_EQUAL(targ->two, 0);
  CU_ASSERT_EQUAL(targ->executed, 2);
  CU_ASSERT_EQUAL(targ->cancelled, 0);

  ddsrt_free(targ);
}

/* verify the first cleanup routine is still called if second got popped */
CU_Test(ddsrt_thread_cleanup, push_two_pop_one_no_exec)
{
  int flags = THREAD_RESET_1 | THREAD_RESET_2;
  struct thread_argument *targ = make_thread_argument(flags, 1, 1, 2);
  setup(targ);

  CU_ASSERT_EQUAL(targ->one, 0);
  CU_ASSERT_EQUAL(targ->two, 2);
  CU_ASSERT_EQUAL(targ->executed, 1);
  CU_ASSERT_EQUAL(targ->cancelled, 1);

  ddsrt_free(targ);
}

CU_Test(ddsrt_thread_cleanup, push_two_pop_one_exec)
{
  int flags = THREAD_RESET_1 | THREAD_RESET_2 | THREAD_RUN_1;
  struct thread_argument *targ = make_thread_argument(flags, 1, 1, 2);
  setup(targ);

  CU_ASSERT_EQUAL(targ->one, 0);
  CU_ASSERT_EQUAL(targ->two, 0);
  CU_ASSERT_EQUAL(targ->executed, 2);
  CU_ASSERT_EQUAL(targ->cancelled, 1);

  ddsrt_free(targ);
}

/* verify no cleanup routines are called if all got popped */
CU_Test(ddsrt_thread_cleanup, push_two_pop_two_no_exec)
{
  int flags = THREAD_RESET_1 | THREAD_RESET_2;
  struct thread_argument *targ = make_thread_argument(flags, 2, 1, 2);
  setup(targ);

  CU_ASSERT_EQUAL(targ->one, 1);
  CU_ASSERT_EQUAL(targ->two, 2);
  CU_ASSERT_EQUAL(targ->executed, 0);
  CU_ASSERT_EQUAL(targ->cancelled, 2);

  ddsrt_free(targ);
}

CU_Test(ddsrt_thread_cleanup, push_two_pop_two_exec_one)
{
  int flags = THREAD_RESET_1 | THREAD_RESET_2 | THREAD_RUN_1;
  struct thread_argument *targ = make_thread_argument(flags, 2, 1, 2);
  setup(targ);

  CU_ASSERT_EQUAL(targ->one, 0);
  CU_ASSERT_EQUAL(targ->two, 2);
  CU_ASSERT_EQUAL(targ->executed, 1);
  CU_ASSERT_EQUAL(targ->cancelled, 2);

  ddsrt_free(targ);
}

CU_Test(ddsrt_thread_cleanup, push_two_pop_two_exec_both)
{
  int flags = THREAD_RESET_1 | THREAD_RESET_2 | THREAD_RUN_1 | THREAD_RUN_2;
  struct thread_argument *targ = make_thread_argument(flags, 2, 1, 2);
  setup(targ);

  CU_ASSERT_EQUAL(targ->one, 0);
  CU_ASSERT_EQUAL(targ->two, 0);
  CU_ASSERT_EQUAL(targ->executed, 2);
  CU_ASSERT_EQUAL(targ->cancelled, 2);

  ddsrt_free(targ);
}

CU_Test(ddsrt_thread_cleanup, no_interference)
{
  int flags = THREAD_RESET_1 | THREAD_RESET_2;
  struct thread_argument *targ1 = make_thread_argument(flags, 0, 1, 2);
  struct thread_argument *targ2 = make_thread_argument(flags, 2, 1, 2);
  ddsrt_mutex_t mutex1, mutex2;

  ddsrt_mutex_init(&mutex1);
  ddsrt_mutex_init(&mutex2);
  ddsrt_mutex_lock(&mutex1);
  ddsrt_mutex_lock(&mutex2);

  targ1->mutex = &mutex1;
  targ1->block = 1;
  targ2->mutex = &mutex2;
  targ2->block = 1;

  setup(targ1);
  setup(targ2);

  /* ensure thread 2 pops it's cleanup routines while thread 1 blocks */
  ddsrt_mutex_unlock(&mutex2);
  ddsrt_thread_join(targ2->thread, NULL);

  CU_ASSERT_EQUAL(targ2->one, 1);
  CU_ASSERT_EQUAL(targ2->two, 2);
  CU_ASSERT_EQUAL(targ2->executed, 0);
  CU_ASSERT_EQUAL(targ2->cancelled, 2);

  /* instruct thread 1 to continue */
  ddsrt_mutex_unlock(&mutex1);
  ddsrt_thread_join(targ1->thread, NULL);

  CU_ASSERT_EQUAL(targ1->one, 0);
  CU_ASSERT_EQUAL(targ1->two, 0);
  CU_ASSERT_EQUAL(targ1->executed, 2);
  CU_ASSERT_EQUAL(targ1->cancelled, 0);

  ddsrt_mutex_destroy(&mutex1);
  ddsrt_mutex_destroy(&mutex2);
  ddsrt_free(targ1);
  ddsrt_free(targ2);
}
