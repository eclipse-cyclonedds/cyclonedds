// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsrt/sync.h"

#include "CUnit/Theory.h"

/* FreeRTOS specific! */

static void fill(ddsrt_tasklist_t *list)
{
  CU_ASSERT_PTR_NOT_NULL_FATAL(list);
  CU_ASSERT_EQUAL_FATAL(list->len, DDSRT_TASKLIST_INITIAL);

  for (size_t i = 1; i <= DDSRT_TASKLIST_INITIAL; i++) {
    ddsrt_tasklist_push(list, (TaskHandle_t)i);
    CU_ASSERT_EQUAL_FATAL(list->cnt, i);
    CU_ASSERT_EQUAL_FATAL(list->off, 0);
    CU_ASSERT_EQUAL_FATAL(list->end, i - 1);
  }

  CU_ASSERT_EQUAL_FATAL(list->len, DDSRT_TASKLIST_INITIAL);
  CU_ASSERT_EQUAL_FATAL(list->cnt, DDSRT_TASKLIST_INITIAL);
  CU_ASSERT_EQUAL_FATAL(list->off, 0);
  CU_ASSERT_EQUAL_FATAL(list->end, DDSRT_TASKLIST_INITIAL - 1);
}

static void fill_wrapped(ddsrt_tasklist_t *list)
{
  size_t i;

  fill(list);

  for (i = 1; i <= DDSRT_TASKLIST_CHUNK; i++) {
    ddsrt_tasklist_pop(list, NULL);
    CU_ASSERT_EQUAL_FATAL(list->cnt, DDSRT_TASKLIST_INITIAL - i);
    CU_ASSERT_EQUAL_FATAL(list->off, i);
    CU_ASSERT_EQUAL_FATAL(list->end, DDSRT_TASKLIST_INITIAL - 1);
  }

  for (i = (DDSRT_TASKLIST_INITIAL+1); i <= (DDSRT_TASKLIST_INITIAL+DDSRT_TASKLIST_CHUNK); i++) {
    ddsrt_tasklist_push(list, (TaskHandle_t)i);
    CU_ASSERT_EQUAL_FATAL(list->cnt, i - DDSRT_TASKLIST_CHUNK);
    CU_ASSERT_EQUAL_FATAL(list->off, DDSRT_TASKLIST_CHUNK);
    CU_ASSERT_EQUAL_FATAL(list->end, (i - 1) - DDSRT_TASKLIST_INITIAL);
  }

  CU_ASSERT_EQUAL_FATAL(list->len, DDSRT_TASKLIST_INITIAL);
  CU_ASSERT_EQUAL_FATAL(list->cnt, DDSRT_TASKLIST_INITIAL);
  CU_ASSERT_EQUAL_FATAL(list->off, DDSRT_TASKLIST_CHUNK);
  CU_ASSERT_EQUAL_FATAL(list->end, DDSRT_TASKLIST_CHUNK - 1);
}

typedef void(*fill_t)(ddsrt_tasklist_t *);

CU_TheoryDataPoints(ddsrt_sync, tasklist_pop_all) = {
  CU_DataPoints(fill_t, &fill, &fill_wrapped),
  CU_DataPoints(size_t, 1, DDSRT_TASKLIST_CHUNK + 1),
  CU_DataPoints(size_t, DDSRT_TASKLIST_INITIAL, DDSRT_TASKLIST_INITIAL + DDSRT_TASKLIST_CHUNK)
};

/* Most basic test to verify behavior is correct for simple use case. */
CU_Theory((fill_t func, size_t first, size_t last), ddsrt_sync, tasklist_pop_all)
{
  TaskHandle_t task;
  ddsrt_tasklist_t list;

  ddsrt_tasklist_init(&list);
  func(&list);

  task = ddsrt_tasklist_pop(&list, NULL);
  CU_ASSERT_PTR_EQUAL(task, (TaskHandle_t)first);

  for (size_t i = first + 1; i < last; i++) {
    task = ddsrt_tasklist_pop(&list, NULL);
    CU_ASSERT_PTR_EQUAL(task, (TaskHandle_t)i);
  }

  CU_ASSERT_EQUAL(list.cnt, 1);
  CU_ASSERT_EQUAL(list.off, ((DDSRT_TASKLIST_INITIAL*2) - last) - 1);
  CU_ASSERT_EQUAL(list.end, ((DDSRT_TASKLIST_INITIAL*2) - last) - 1);
  task = ddsrt_tasklist_pop(&list, NULL);
  CU_ASSERT_PTR_EQUAL(task, (TaskHandle_t)last);
  task = ddsrt_tasklist_pop(&list, NULL);
  CU_ASSERT_PTR_NULL(task);
  CU_ASSERT_EQUAL(list.cnt, 0);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, 0);

  ddsrt_tasklist_fini(&list);
}

CU_TheoryDataPoints(ddsrt_sync, tasklist_pop_n_push) = {
  CU_DataPoints(fill_t,
    &fill, &fill, &fill, &fill,
    &fill_wrapped, &fill_wrapped, &fill_wrapped, &fill_wrapped, &fill_wrapped),
  CU_DataPoints(TaskHandle_t, /* Task to pop. */
    (TaskHandle_t)NULL,
    (TaskHandle_t)1,
    (TaskHandle_t)DDSRT_TASKLIST_CHUNK,
    (TaskHandle_t)DDSRT_TASKLIST_INITIAL,
    (TaskHandle_t)NULL,
    (TaskHandle_t)(DDSRT_TASKLIST_CHUNK + 1),
    (TaskHandle_t)DDSRT_TASKLIST_INITIAL,
    (TaskHandle_t)(DDSRT_TASKLIST_INITIAL + 1),
    (TaskHandle_t)(DDSRT_TASKLIST_INITIAL + DDSRT_TASKLIST_CHUNK)),
  CU_DataPoints(size_t, /* Expected position to clear. */
    0, 0, DDSRT_TASKLIST_CHUNK - 1, DDSRT_TASKLIST_INITIAL - 1,
    DDSRT_TASKLIST_CHUNK, DDSRT_TASKLIST_CHUNK, DDSRT_TASKLIST_INITIAL - 1, 0, DDSRT_TASKLIST_CHUNK - 1),
  CU_DataPoints(size_t, /* Expected position of pushed task. */
    0, 0, DDSRT_TASKLIST_INITIAL - 1, DDSRT_TASKLIST_INITIAL - 1,
    DDSRT_TASKLIST_CHUNK, DDSRT_TASKLIST_CHUNK, DDSRT_TASKLIST_CHUNK, DDSRT_TASKLIST_CHUNK - 1, DDSRT_TASKLIST_CHUNK - 1)
};

/* Test to verify tasklist is correctly updated (trimmed and packed) when the
   tasklist is sparse. */
CU_Theory((fill_t func, TaskHandle_t task, size_t pos, size_t end), ddsrt_sync, tasklist_pop_n_push)
{
  ddsrt_tasklist_t list;

  ddsrt_tasklist_init(&list);
  func(&list);

  if (task == NULL) {
    ddsrt_tasklist_pop(&list, NULL);
  } else {
    CU_ASSERT_PTR_EQUAL(ddsrt_tasklist_pop(&list, task), task);
    CU_ASSERT_PTR_NULL(ddsrt_tasklist_pop(&list, task));
  }
  CU_ASSERT_PTR_EQUAL(list.tasks[pos], NULL);
  task = (TaskHandle_t)(DDSRT_TASKLIST_INITIAL*2);
  CU_ASSERT_NOT_EQUAL_FATAL(ddsrt_tasklist_push(&list, task), -1);
  CU_ASSERT_PTR_EQUAL(list.tasks[end], task);
  CU_ASSERT_EQUAL(list.len, DDSRT_TASKLIST_INITIAL);
  CU_ASSERT_EQUAL(list.cnt, DDSRT_TASKLIST_INITIAL);

  ddsrt_tasklist_fini(&list);
}

CU_Test(ddsrt_sync, tasklist_ltrim)
{
  ddsrt_tasklist_t list;

  ddsrt_tasklist_init(&list);
  fill(&list);

  ddsrt_tasklist_pop(&list, (TaskHandle_t)2);
  ddsrt_tasklist_pop(&list, (TaskHandle_t)3);
  CU_ASSERT_EQUAL(list.cnt, DDSRT_TASKLIST_INITIAL - 2);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, 9);
  ddsrt_tasklist_pop(&list, (TaskHandle_t)1);
  CU_ASSERT_EQUAL(list.cnt, DDSRT_TASKLIST_INITIAL - 3);
  CU_ASSERT_EQUAL(list.off, 3);
  CU_ASSERT_EQUAL(list.end, 9);

  ddsrt_tasklist_fini(&list);
}

CU_Test(ddsrt_sync, tasklist_rtrim)
{
  ddsrt_tasklist_t list;

  ddsrt_tasklist_init(&list);
  fill(&list);

  ddsrt_tasklist_pop(&list, (TaskHandle_t)(DDSRT_TASKLIST_INITIAL - 1));
  ddsrt_tasklist_pop(&list, (TaskHandle_t)(DDSRT_TASKLIST_INITIAL - 2));
  CU_ASSERT_EQUAL(list.cnt, DDSRT_TASKLIST_INITIAL - 2);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_INITIAL - 1);
  ddsrt_tasklist_pop(&list, (TaskHandle_t)DDSRT_TASKLIST_INITIAL);
  CU_ASSERT_EQUAL(list.cnt, DDSRT_TASKLIST_INITIAL - 3);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_INITIAL - 4);

  ddsrt_tasklist_fini(&list);
}

CU_Test(ddsrt_sync, tasklist_wrapped_ltrim)
{
  ddsrt_tasklist_t list;

  ddsrt_tasklist_init(&list);
  fill_wrapped(&list);

  for (size_t i = DDSRT_TASKLIST_CHUNK+2; i < DDSRT_TASKLIST_INITIAL; i++) {
    ddsrt_tasklist_pop(&list, (TaskHandle_t)i);
  }
  CU_ASSERT_EQUAL(list.cnt, DDSRT_TASKLIST_INITIAL - (DDSRT_TASKLIST_CHUNK - 2));
  CU_ASSERT_EQUAL(list.off, DDSRT_TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_CHUNK - 1);
  ddsrt_tasklist_pop(&list, (TaskHandle_t)(DDSRT_TASKLIST_CHUNK+1));
  CU_ASSERT_EQUAL(list.cnt, DDSRT_TASKLIST_INITIAL - (DDSRT_TASKLIST_CHUNK - 1));
  CU_ASSERT_EQUAL(list.off, DDSRT_TASKLIST_INITIAL - 1);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_CHUNK - 1);
  ddsrt_tasklist_pop(&list, (TaskHandle_t)(DDSRT_TASKLIST_INITIAL+1));
  ddsrt_tasklist_pop(&list, (TaskHandle_t)DDSRT_TASKLIST_INITIAL);
  CU_ASSERT_EQUAL(list.cnt, DDSRT_TASKLIST_INITIAL - (DDSRT_TASKLIST_CHUNK + 1));
  CU_ASSERT_EQUAL(list.off, 1);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_CHUNK - 1);

  ddsrt_tasklist_fini(&list);
}

CU_Test(ddsrt_sync, tasklist_wrapped_rtrim)
{
  ddsrt_tasklist_t list;
  size_t last = DDSRT_TASKLIST_INITIAL + DDSRT_TASKLIST_CHUNK;

  ddsrt_tasklist_init(&list);
  fill_wrapped(&list);

  for (size_t i = last - 1; i > DDSRT_TASKLIST_INITIAL + 1; i--) {
    ddsrt_tasklist_pop(&list, (TaskHandle_t)i);
  }
  CU_ASSERT_EQUAL(list.cnt, (DDSRT_TASKLIST_INITIAL - DDSRT_TASKLIST_CHUNK) + 2);
  CU_ASSERT_EQUAL(list.off, DDSRT_TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_CHUNK - 1);
  ddsrt_tasklist_pop(&list, (TaskHandle_t)(DDSRT_TASKLIST_INITIAL + DDSRT_TASKLIST_CHUNK));
  CU_ASSERT_EQUAL(list.cnt, (DDSRT_TASKLIST_INITIAL - DDSRT_TASKLIST_CHUNK) + 1);
  CU_ASSERT_EQUAL(list.off, DDSRT_TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.end, 0);
  ddsrt_tasklist_pop(&list, (TaskHandle_t)(DDSRT_TASKLIST_INITIAL - 1));
  ddsrt_tasklist_pop(&list, (TaskHandle_t)(DDSRT_TASKLIST_INITIAL - 2));
  ddsrt_tasklist_pop(&list, (TaskHandle_t)(DDSRT_TASKLIST_INITIAL + 1));
  CU_ASSERT_EQUAL(list.cnt, (DDSRT_TASKLIST_INITIAL - DDSRT_TASKLIST_CHUNK) - 2);
  CU_ASSERT_EQUAL(list.off, DDSRT_TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_INITIAL - 1);
  ddsrt_tasklist_pop(&list, (TaskHandle_t)DDSRT_TASKLIST_INITIAL);
  CU_ASSERT_EQUAL(list.cnt, (DDSRT_TASKLIST_INITIAL - DDSRT_TASKLIST_CHUNK) - 3);
  CU_ASSERT_EQUAL(list.off, DDSRT_TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_INITIAL - 4);

  ddsrt_tasklist_fini(&list);
}

CU_Test(ddsrt_sync, tasklist_resize)
{
  ddsrt_tasklist_t list;
  int ret;

  ddsrt_tasklist_init(&list);
  fill(&list);

  /* Grow one past initial. Buffer should increase by chunk. */
  ret = ddsrt_tasklist_push(&list, (TaskHandle_t)(DDSRT_TASKLIST_INITIAL + 1));
  CU_ASSERT_EQUAL_FATAL(ret, 0);
  CU_ASSERT_EQUAL(list.len, DDSRT_TASKLIST_INITIAL + DDSRT_TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_INITIAL);
  /* Grow one past initial+chunk. Buffer should increase by chunk again. */
  for (size_t i = 2; i <= DDSRT_TASKLIST_CHUNK + 1; i++) {
    ret = ddsrt_tasklist_push(&list, (TaskHandle_t)(DDSRT_TASKLIST_INITIAL + i));
    CU_ASSERT_EQUAL_FATAL(ret, 0);
  }
  CU_ASSERT_EQUAL(list.len, DDSRT_TASKLIST_INITIAL + (DDSRT_TASKLIST_CHUNK*2));
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_INITIAL + DDSRT_TASKLIST_CHUNK);

  /* Shrink one past initial+chunk. Buffer should not decrease by chunk. */
  for (size_t i = 1; i <= DDSRT_TASKLIST_CHUNK; i++) {
    ddsrt_tasklist_pop(&list, (TaskHandle_t)i);
  }
  CU_ASSERT_EQUAL(list.len, DDSRT_TASKLIST_INITIAL + (DDSRT_TASKLIST_CHUNK*2));
  CU_ASSERT_EQUAL(list.off, DDSRT_TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_INITIAL + DDSRT_TASKLIST_CHUNK);

  /* Shrink to initial. Buffer should decrease by chunk. */
  ddsrt_tasklist_pop(&list, (TaskHandle_t)(DDSRT_TASKLIST_CHUNK + 1));
  CU_ASSERT_EQUAL(list.len, DDSRT_TASKLIST_INITIAL + DDSRT_TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_INITIAL - 1);

  /* Shrink to initial-chunk. Buffer should decrease by chunk. */
  for (size_t i = DDSRT_TASKLIST_CHUNK+1; i <= (DDSRT_TASKLIST_CHUNK*2)+1; i++) {
    ddsrt_tasklist_pop(&list, (TaskHandle_t)i);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
  }
  CU_ASSERT_EQUAL(list.len, DDSRT_TASKLIST_INITIAL);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, (DDSRT_TASKLIST_INITIAL - DDSRT_TASKLIST_CHUNK) - 1);

  ddsrt_tasklist_fini(&list);
}

CU_Test(ddsrt_sync, tasklist_wrapped_resize)
{
  ddsrt_tasklist_t list;
  int ret;

  ddsrt_tasklist_init(&list);
  fill_wrapped(&list);

  /* Grow one past initial. Buffer should increase by chunk. */
  ret = ddsrt_tasklist_push(&list, (TaskHandle_t)(DDSRT_TASKLIST_INITIAL + DDSRT_TASKLIST_CHUNK + 1));
  CU_ASSERT_EQUAL_FATAL(ret, 0);
  CU_ASSERT_EQUAL(list.len, DDSRT_TASKLIST_INITIAL + DDSRT_TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.off, DDSRT_TASKLIST_INITIAL);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_CHUNK);
  /* Grow one past initial+chunk. Buffer should increase by chunk again. */
  for (size_t i = 2; i <= (DDSRT_TASKLIST_CHUNK + 1); i++) {
    ret = ddsrt_tasklist_push(&list, (TaskHandle_t)(DDSRT_TASKLIST_INITIAL + DDSRT_TASKLIST_CHUNK + i));
    CU_ASSERT_EQUAL_FATAL(ret, 0);
  }
  CU_ASSERT_EQUAL(list.len, DDSRT_TASKLIST_INITIAL + (DDSRT_TASKLIST_CHUNK*2));
  CU_ASSERT_EQUAL(list.off, DDSRT_TASKLIST_INITIAL + DDSRT_TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_INITIAL);

  /* Shrink one past initial+chunk. Buffer should not decrease by chunk. */
  for (size_t i = 1; i <= DDSRT_TASKLIST_CHUNK; i++) {
    ddsrt_tasklist_pop(&list, (TaskHandle_t)(DDSRT_TASKLIST_CHUNK + i));
  }
  CU_ASSERT_EQUAL(list.len, DDSRT_TASKLIST_INITIAL + (DDSRT_TASKLIST_CHUNK*2));
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_INITIAL);

  /* Shrink to initial. Buffer should decrease by chunk. */
  ddsrt_tasklist_pop(&list, (TaskHandle_t)((DDSRT_TASKLIST_CHUNK*2) + 1));
  CU_ASSERT_EQUAL(list.len, DDSRT_TASKLIST_INITIAL + DDSRT_TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, DDSRT_TASKLIST_INITIAL - 1);

  /* Shrink to initial-chunk. Buffer should decrease by chunk. */
  for (size_t i = 2; i <= DDSRT_TASKLIST_CHUNK + 1; i++) {
    ddsrt_tasklist_pop(&list, (TaskHandle_t)((DDSRT_TASKLIST_CHUNK*2) + i));
  }
  CU_ASSERT_EQUAL(list.len, DDSRT_TASKLIST_INITIAL);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, (DDSRT_TASKLIST_INITIAL - DDSRT_TASKLIST_CHUNK) - 1);

  ddsrt_tasklist_fini(&list);
}
