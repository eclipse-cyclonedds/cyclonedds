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
#include "dds/ddsrt/sync.h"

#include "CUnit/Theory.h"

/* FreeRTOS specific! */

static void fill(tasklist_t *list)
{
  CU_ASSERT_PTR_NOT_NULL_FATAL(list);
  CU_ASSERT_EQUAL_FATAL(list->len, TASKLIST_INITIAL);

  for (size_t i = 1; i <= TASKLIST_INITIAL; i++) {
    tasklist_push(list, (TaskHandle_t)i);
    CU_ASSERT_EQUAL_FATAL(list->cnt, i);
    CU_ASSERT_EQUAL_FATAL(list->off, 0);
    CU_ASSERT_EQUAL_FATAL(list->end, i - 1);
  }

  CU_ASSERT_EQUAL_FATAL(list->len, TASKLIST_INITIAL);
  CU_ASSERT_EQUAL_FATAL(list->cnt, TASKLIST_INITIAL);
  CU_ASSERT_EQUAL_FATAL(list->off, 0);
  CU_ASSERT_EQUAL_FATAL(list->end, TASKLIST_INITIAL - 1);
}

static void fill_wrapped(tasklist_t *list)
{
  size_t i;

  fill(list);

  for (i = 1; i <= TASKLIST_CHUNK; i++) {
    tasklist_pop(list, NULL);
    CU_ASSERT_EQUAL_FATAL(list->cnt, TASKLIST_INITIAL - i);
    CU_ASSERT_EQUAL_FATAL(list->off, i);
    CU_ASSERT_EQUAL_FATAL(list->end, TASKLIST_INITIAL - 1);
  }

  for (i = (TASKLIST_INITIAL+1); i <= (TASKLIST_INITIAL+TASKLIST_CHUNK); i++) {
    tasklist_push(list, (TaskHandle_t)i);
    CU_ASSERT_EQUAL_FATAL(list->cnt, i - TASKLIST_CHUNK);
    CU_ASSERT_EQUAL_FATAL(list->off, TASKLIST_CHUNK);
    CU_ASSERT_EQUAL_FATAL(list->end, (i - 1) - TASKLIST_INITIAL);
  }

  CU_ASSERT_EQUAL_FATAL(list->len, TASKLIST_INITIAL);
  CU_ASSERT_EQUAL_FATAL(list->cnt, TASKLIST_INITIAL);
  CU_ASSERT_EQUAL_FATAL(list->off, TASKLIST_CHUNK);
  CU_ASSERT_EQUAL_FATAL(list->end, TASKLIST_CHUNK - 1);
}

typedef void(*fill_t)(tasklist_t *);

CU_TheoryDataPoints(ddsrt_sync, tasklist_pop_all) = {
  CU_DataPoints(fill_t, &fill, &fill_wrapped),
  CU_DataPoints(size_t, 1, TASKLIST_CHUNK + 1),
  CU_DataPoints(size_t, TASKLIST_INITIAL, TASKLIST_INITIAL + TASKLIST_CHUNK)
};

/* Most basic test to verify behavior is correct for simple use case. */
CU_Theory((fill_t func, size_t first, size_t last), ddsrt_sync, tasklist_pop_all)
{
  TaskHandle_t task;
  tasklist_t list;

  tasklist_init(&list);
  func(&list);

  task = tasklist_pop(&list, NULL);
  CU_ASSERT_PTR_EQUAL(task, (TaskHandle_t)first);

  for (size_t i = first + 1; i < last; i++) {
    task = tasklist_pop(&list, NULL);
    CU_ASSERT_PTR_EQUAL(task, (TaskHandle_t)i);
  }

  CU_ASSERT_EQUAL(list.cnt, 1);
  CU_ASSERT_EQUAL(list.off, ((TASKLIST_INITIAL*2) - last) - 1);
  CU_ASSERT_EQUAL(list.end, ((TASKLIST_INITIAL*2) - last) - 1);
  task = tasklist_pop(&list, NULL);
  CU_ASSERT_PTR_EQUAL(task, (TaskHandle_t)last);
  task = tasklist_pop(&list, NULL);
  CU_ASSERT_PTR_NULL(task);
  CU_ASSERT_EQUAL(list.cnt, 0);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, 0);

  tasklist_fini(&list);
}

CU_TheoryDataPoints(ddsrt_sync, tasklist_pop_n_push) = {
  CU_DataPoints(fill_t,
    &fill, &fill, &fill, &fill,
    &fill_wrapped, &fill_wrapped, &fill_wrapped, &fill_wrapped, &fill_wrapped),
  CU_DataPoints(TaskHandle_t, /* Task to pop. */
    (TaskHandle_t)NULL,
    (TaskHandle_t)1,
    (TaskHandle_t)TASKLIST_CHUNK,
    (TaskHandle_t)TASKLIST_INITIAL,
    (TaskHandle_t)NULL,
    (TaskHandle_t)(TASKLIST_CHUNK + 1),
    (TaskHandle_t)TASKLIST_INITIAL,
    (TaskHandle_t)(TASKLIST_INITIAL + 1),
    (TaskHandle_t)(TASKLIST_INITIAL + TASKLIST_CHUNK)),
  CU_DataPoints(size_t, /* Expected position to clear. */
    0, 0, TASKLIST_CHUNK - 1, TASKLIST_INITIAL - 1,
    TASKLIST_CHUNK, TASKLIST_CHUNK, TASKLIST_INITIAL - 1, 0, TASKLIST_CHUNK - 1),
  CU_DataPoints(size_t, /* Expected position of pushed task. */
    0, 0, TASKLIST_INITIAL - 1, TASKLIST_INITIAL - 1,
    TASKLIST_CHUNK, TASKLIST_CHUNK, TASKLIST_CHUNK, TASKLIST_CHUNK - 1, TASKLIST_CHUNK - 1)
};

/* Test to verify tasklist is correctly updated (trimmed and packed) when the
   tasklist is sparse. */
CU_Theory((fill_t func, TaskHandle_t task, size_t pos, size_t end), ddsrt_sync, tasklist_pop_n_push)
{
  tasklist_t list;

  tasklist_init(&list);
  func(&list);

  if (task == NULL) {
    tasklist_pop(&list, NULL);
  } else {
    CU_ASSERT_PTR_EQUAL(tasklist_pop(&list, task), task);
    CU_ASSERT_PTR_NULL(tasklist_pop(&list, task));
  }
  CU_ASSERT_PTR_EQUAL(list.tasks[pos], NULL);
  task = (TaskHandle_t)(TASKLIST_INITIAL*2);
  CU_ASSERT_NOT_EQUAL_FATAL(tasklist_push(&list, task), -1);
  CU_ASSERT_PTR_EQUAL(list.tasks[end], task);
  CU_ASSERT_EQUAL(list.len, TASKLIST_INITIAL);
  CU_ASSERT_EQUAL(list.cnt, TASKLIST_INITIAL);

  tasklist_fini(&list);
}

CU_Test(ddsrt_sync, tasklist_ltrim)
{
  tasklist_t list;

  tasklist_init(&list);
  fill(&list);

  tasklist_pop(&list, (TaskHandle_t)2);
  tasklist_pop(&list, (TaskHandle_t)3);
  CU_ASSERT_EQUAL(list.cnt, TASKLIST_INITIAL - 2);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, 9);
  tasklist_pop(&list, (TaskHandle_t)1);
  CU_ASSERT_EQUAL(list.cnt, TASKLIST_INITIAL - 3);
  CU_ASSERT_EQUAL(list.off, 3);
  CU_ASSERT_EQUAL(list.end, 9);

  tasklist_fini(&list);
}

CU_Test(ddsrt_sync, tasklist_rtrim)
{
  tasklist_t list;

  tasklist_init(&list);
  fill(&list);

  tasklist_pop(&list, (TaskHandle_t)(TASKLIST_INITIAL - 1));
  tasklist_pop(&list, (TaskHandle_t)(TASKLIST_INITIAL - 2));
  CU_ASSERT_EQUAL(list.cnt, TASKLIST_INITIAL - 2);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, TASKLIST_INITIAL - 1);
  tasklist_pop(&list, (TaskHandle_t)TASKLIST_INITIAL);
  CU_ASSERT_EQUAL(list.cnt, TASKLIST_INITIAL - 3);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, TASKLIST_INITIAL - 4);

  tasklist_fini(&list);
}

CU_Test(ddsrt_sync, tasklist_wrapped_ltrim)
{
  tasklist_t list;

  tasklist_init(&list);
  fill_wrapped(&list);

  for (size_t i = TASKLIST_CHUNK+2; i < TASKLIST_INITIAL; i++) {
    tasklist_pop(&list, (TaskHandle_t)i);
  }
  CU_ASSERT_EQUAL(list.cnt, TASKLIST_INITIAL - (TASKLIST_CHUNK - 2));
  CU_ASSERT_EQUAL(list.off, TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.end, TASKLIST_CHUNK - 1);
  tasklist_pop(&list, (TaskHandle_t)(TASKLIST_CHUNK+1));
  CU_ASSERT_EQUAL(list.cnt, TASKLIST_INITIAL - (TASKLIST_CHUNK - 1));
  CU_ASSERT_EQUAL(list.off, TASKLIST_INITIAL - 1);
  CU_ASSERT_EQUAL(list.end, TASKLIST_CHUNK - 1);
  tasklist_pop(&list, (TaskHandle_t)(TASKLIST_INITIAL+1));
  tasklist_pop(&list, (TaskHandle_t)TASKLIST_INITIAL);
  CU_ASSERT_EQUAL(list.cnt, TASKLIST_INITIAL - (TASKLIST_CHUNK + 1));
  CU_ASSERT_EQUAL(list.off, 1);
  CU_ASSERT_EQUAL(list.end, TASKLIST_CHUNK - 1);

  tasklist_fini(&list);
}

CU_Test(ddsrt_sync, tasklist_wrapped_rtrim)
{
  tasklist_t list;
  size_t last = TASKLIST_INITIAL + TASKLIST_CHUNK;

  tasklist_init(&list);
  fill_wrapped(&list);

  for (size_t i = last - 1; i > TASKLIST_INITIAL + 1; i--) {
    tasklist_pop(&list, (TaskHandle_t)i);
  }
  CU_ASSERT_EQUAL(list.cnt, (TASKLIST_INITIAL - TASKLIST_CHUNK) + 2);
  CU_ASSERT_EQUAL(list.off, TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.end, TASKLIST_CHUNK - 1);
  tasklist_pop(&list, (TaskHandle_t)(TASKLIST_INITIAL + TASKLIST_CHUNK));
  CU_ASSERT_EQUAL(list.cnt, (TASKLIST_INITIAL - TASKLIST_CHUNK) + 1);
  CU_ASSERT_EQUAL(list.off, TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.end, 0);
  tasklist_pop(&list, (TaskHandle_t)(TASKLIST_INITIAL - 1));
  tasklist_pop(&list, (TaskHandle_t)(TASKLIST_INITIAL - 2));
  tasklist_pop(&list, (TaskHandle_t)(TASKLIST_INITIAL + 1));
  CU_ASSERT_EQUAL(list.cnt, (TASKLIST_INITIAL - TASKLIST_CHUNK) - 2);
  CU_ASSERT_EQUAL(list.off, TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.end, TASKLIST_INITIAL - 1);
  tasklist_pop(&list, (TaskHandle_t)TASKLIST_INITIAL);
  CU_ASSERT_EQUAL(list.cnt, (TASKLIST_INITIAL - TASKLIST_CHUNK) - 3);
  CU_ASSERT_EQUAL(list.off, TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.end, TASKLIST_INITIAL - 4);

  tasklist_fini(&list);
}

CU_Test(ddsrt_sync, tasklist_resize)
{
  tasklist_t list;
  int ret;

  tasklist_init(&list);
  fill(&list);

  /* Grow one past initial. Buffer should increase by chunk. */
  ret = tasklist_push(&list, (TaskHandle_t)(TASKLIST_INITIAL + 1));
  CU_ASSERT_EQUAL_FATAL(ret, 0);
  CU_ASSERT_EQUAL(list.len, TASKLIST_INITIAL + TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, TASKLIST_INITIAL);
  /* Grow one past initial+chunk. Buffer should increase by chunk again. */
  for (size_t i = 2; i <= TASKLIST_CHUNK + 1; i++) {
    ret = tasklist_push(&list, (TaskHandle_t)(TASKLIST_INITIAL + i));
    CU_ASSERT_EQUAL_FATAL(ret, 0);
  }
  CU_ASSERT_EQUAL(list.len, TASKLIST_INITIAL + (TASKLIST_CHUNK*2));
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, TASKLIST_INITIAL + TASKLIST_CHUNK);

  /* Shrink one past initial+chunk. Buffer should not decrease by chunk. */
  for (size_t i = 1; i <= TASKLIST_CHUNK; i++) {
    tasklist_pop(&list, (TaskHandle_t)i);
  }
  CU_ASSERT_EQUAL(list.len, TASKLIST_INITIAL + (TASKLIST_CHUNK*2));
  CU_ASSERT_EQUAL(list.off, TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.end, TASKLIST_INITIAL + TASKLIST_CHUNK);

  /* Shrink to initial. Buffer should decrease by chunk. */
  tasklist_pop(&list, (TaskHandle_t)(TASKLIST_CHUNK + 1));
  CU_ASSERT_EQUAL(list.len, TASKLIST_INITIAL + TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, TASKLIST_INITIAL - 1);

  /* Shrink to initial-chunk. Buffer should decrease by chunk. */
  for (size_t i = TASKLIST_CHUNK+1; i <= (TASKLIST_CHUNK*2)+1; i++) {
    tasklist_pop(&list, (TaskHandle_t)i);
    CU_ASSERT_EQUAL_FATAL(ret, 0);
  }
  CU_ASSERT_EQUAL(list.len, TASKLIST_INITIAL);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, (TASKLIST_INITIAL - TASKLIST_CHUNK) - 1);

  tasklist_fini(&list);
}

CU_Test(ddsrt_sync, tasklist_wrapped_resize)
{
  tasklist_t list;
  int ret;

  tasklist_init(&list);
  fill_wrapped(&list);

  /* Grow one past initial. Buffer should increase by chunk. */
  ret = tasklist_push(&list, (TaskHandle_t)(TASKLIST_INITIAL + TASKLIST_CHUNK + 1));
  CU_ASSERT_EQUAL_FATAL(ret, 0);
  CU_ASSERT_EQUAL(list.len, TASKLIST_INITIAL + TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.off, TASKLIST_INITIAL);
  CU_ASSERT_EQUAL(list.end, TASKLIST_CHUNK);
  /* Grow one past initial+chunk. Buffer should increase by chunk again. */
  for (size_t i = 2; i <= (TASKLIST_CHUNK + 1); i++) {
    ret = tasklist_push(&list, (TaskHandle_t)(TASKLIST_INITIAL + TASKLIST_CHUNK + i));
    CU_ASSERT_EQUAL_FATAL(ret, 0);
  }
  CU_ASSERT_EQUAL(list.len, TASKLIST_INITIAL + (TASKLIST_CHUNK*2));
  CU_ASSERT_EQUAL(list.off, TASKLIST_INITIAL + TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.end, TASKLIST_INITIAL);

  /* Shrink one past initial+chunk. Buffer should not decrease by chunk. */
  for (size_t i = 1; i <= TASKLIST_CHUNK; i++) {
    tasklist_pop(&list, (TaskHandle_t)(TASKLIST_CHUNK + i));
  }
  CU_ASSERT_EQUAL(list.len, TASKLIST_INITIAL + (TASKLIST_CHUNK*2));
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, TASKLIST_INITIAL);

  /* Shrink to initial. Buffer should decrease by chunk. */
  tasklist_pop(&list, (TaskHandle_t)((TASKLIST_CHUNK*2) + 1));
  CU_ASSERT_EQUAL(list.len, TASKLIST_INITIAL + TASKLIST_CHUNK);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, TASKLIST_INITIAL - 1);

  /* Shrink to initial-chunk. Buffer should decrease by chunk. */
  for (size_t i = 2; i <= TASKLIST_CHUNK + 1; i++) {
    tasklist_pop(&list, (TaskHandle_t)((TASKLIST_CHUNK*2) + i));
  }
  CU_ASSERT_EQUAL(list.len, TASKLIST_INITIAL);
  CU_ASSERT_EQUAL(list.off, 0);
  CU_ASSERT_EQUAL(list.end, (TASKLIST_INITIAL - TASKLIST_CHUNK) - 1);

  tasklist_fini(&list);
}
