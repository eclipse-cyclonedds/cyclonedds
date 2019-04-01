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
#ifndef DDSRT_SYNC_FREERTOS_H
#define DDSRT_SYNC_FREERTOS_H

#include <FreeRTOS.h>
#include <semphr.h>
#include <task.h>
#include <stddef.h>

#include "dds/ddsrt/atomics.h"

#if (INCLUDE_vTaskSuspend != 1)
/* INCLUDE_vTaskSuspend must be set to 1 to make xSemaphoreTake wait
   indefinitely when passed portMAX_DELAY. See reference manual. */
#error "INCLUDE_vTaskSuspend != 1 in FreeRTOSConfig.h"
#endif

#if defined (__cplusplus)
extern "C" {
#endif

typedef struct {
  SemaphoreHandle_t sem;
} ddsrt_mutex_t;

typedef struct {
  size_t len;
  size_t cnt;
  size_t off;
  size_t end;
  TaskHandle_t *tasks;
} tasklist_t;

typedef struct {
  SemaphoreHandle_t sem;
  tasklist_t tasks;
} ddsrt_cond_t;

/* This readers-writer lock implementation does not prefer writers over readers
   or vice versa. Multiple readers are allowed to hold the lock simultaneously
   and can acquire it directly if no writers are queued. However, if a writer
   is queued, new readers and writers are queued behind it in order. Any reader
   that acquires the lock after a writer frees it, notifies the next task. If
   that task tries to acquire a write lock it waits until the reader frees the
   lock. However, if the task tries to acquire a read lock it will succeed, and
   notify the next task, etc. */
typedef struct {
  SemaphoreHandle_t sem;
  tasklist_t tasks;
  int32_t state;
  uint32_t cnt;
  uint32_t rdcnt;
  uint32_t wrcnt;
} ddsrt_rwlock_t;

typedef ddsrt_atomic_uint32_t ddsrt_once_t;
#define DDSRT_ONCE_INIT { .v = (1<<0) /* ONCE_NOT_STARTED */ }


/* The declarations below are here for tests and must be considered private. */

/* Number of buckets to grow buffer by. */
#define TASKLIST_CHUNK (5)
/* Number of buckets to allocate initially. */
#define TASKLIST_INITIAL (TASKLIST_CHUNK * 2)

int tasklist_init(tasklist_t *list);
void tasklist_fini(tasklist_t *list);
void tasklist_ltrim(tasklist_t *list);
void tasklist_rtrim(tasklist_t *list);
void tasklist_pack(tasklist_t *list);
int tasklist_shrink(tasklist_t *list);
int tasklist_grow(tasklist_t *list);
ssize_t tasklist_find(tasklist_t *list, TaskHandle_t task);
TaskHandle_t tasklist_peek(tasklist_t *list, TaskHandle_t task);
TaskHandle_t tasklist_pop(tasklist_t *list, TaskHandle_t task);
int tasklist_push(tasklist_t *list, TaskHandle_t task);


#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_SYNC_FREERTOS_H */
