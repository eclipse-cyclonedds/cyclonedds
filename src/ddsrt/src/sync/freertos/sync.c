// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <FreeRTOS.h>
#include <task.h>
#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/time.h"

void ddsrt_mutex_init(ddsrt_mutex_t *mutex)
{
  SemaphoreHandle_t sem;

  assert(mutex != NULL);

  if ((sem = xSemaphoreCreateMutex()) == NULL) {
    abort();
  }

  (void)memset(mutex, 0, sizeof(*mutex));
  mutex->sem = sem;
}

void ddsrt_mutex_destroy(ddsrt_mutex_t *mutex)
{
  assert(mutex != NULL);

  vSemaphoreDelete(mutex->sem);
  (void)memset(mutex, 0, sizeof(*mutex));
}

static bool
mutex_lock(ddsrt_mutex_t *mutex, int blk)
{
  assert(mutex != NULL);

  if (xSemaphoreTake(mutex->sem, (blk == 1 ? portMAX_DELAY : 0)) != pdPASS) {
    /* xSemaphoreTake will only return pdFAIL on timeout. The wait will be
       indefinite if INCLUDE_vTaskSuspend is set to 1 in FreeRTOSConfig.h
       and portMAX_DELAY was passed. */
    assert(blk == 0);
    return false;
  }

  return true;
}

void ddsrt_mutex_lock(ddsrt_mutex_t *mutex)
{
  if (!mutex_lock(mutex, 1)) {
    DDS_FATAL("Failed to lock 0x%p", mutex);
  }
}

bool ddsrt_mutex_trylock(ddsrt_mutex_t *mutex)
{
  return mutex_lock(mutex, 0);
}

void ddsrt_mutex_unlock(ddsrt_mutex_t *mutex)
{
  assert(mutex != NULL);

  if (xSemaphoreGive(mutex->sem) != pdPASS) {
    DDS_FATAL("Failed to unlock 0x%p", mutex->sem);
  }
}

static dds_return_t
cond_timedwait(
  ddsrt_cond_t *cond,
  ddsrt_mutex_t *mutex,
  dds_duration_t reltime)
{
  dds_return_t rc = DDS_RETCODE_OK;
  dds_time_t abstime;
  TaskHandle_t task;
  TickType_t ticks = 0;

  assert(cond != NULL);
  assert(mutex != NULL);

  abstime = ddsrt_time_add_duration(dds_time(), reltime);
  ticks = ddsrt_duration_to_ticks_ceil(reltime);

  xSemaphoreTake(cond->sem, portMAX_DELAY);
  ddsrt_mutex_unlock(mutex);

  task = xTaskGetCurrentTaskHandle();
  /* Register current task with condition. */
  ddsrt_tasklist_push(&cond->tasks, task);
  /* Discard pending notifications. */
  ulTaskNotifyTake(1, 0);

  xSemaphoreGive(cond->sem);
  /* Wait to be notified. */
  switch (ulTaskNotifyTake(1, ticks)) {
    case 0:
      xSemaphoreTake(cond->sem, ticks);
      ddsrt_tasklist_pop(&cond->tasks, task);
      xSemaphoreGive(cond->sem);
      break;
    default:
      /* Task already removed from condition. */
      break;
  }

  /* Timeout must only be returned if the time has actually passed. */
  if (dds_time() >= abstime) {
    rc = DDS_RETCODE_TIMEOUT;
  }

  ddsrt_mutex_lock(mutex);

  return rc;
}

void ddsrt_cond_init(ddsrt_cond_t *cond)
{
  SemaphoreHandle_t sem;
  ddsrt_tasklist_t tasks;

  assert(cond != NULL);

  if (ddsrt_tasklist_init(&tasks) == -1) {
    abort();
  }
  if ((sem = xSemaphoreCreateMutex()) == NULL) {
    ddsrt_tasklist_fini(&tasks);
    abort();
  }

  (void)memset(cond, 0, sizeof(*cond));
  cond->sem = sem;
  cond->tasks = tasks;
}

void ddsrt_cond_destroy(ddsrt_cond_t *cond)
{
  assert(cond != NULL);

  vSemaphoreDelete(cond->sem);
  ddsrt_tasklist_fini(&cond->tasks);
  (void)memset(cond, 0, sizeof(*cond));
}

void ddsrt_cond_wait(ddsrt_cond_t *cond, ddsrt_mutex_t *mutex)
{
  assert(cond != NULL);
  assert(mutex != NULL);

  (void)cond_timedwait(cond, mutex, DDS_INFINITY);
}

bool
ddsrt_cond_waitfor(
  ddsrt_cond_t *cond,
  ddsrt_mutex_t *mutex,
  dds_duration_t reltime)
{
  dds_return_t rc;

  assert(cond != NULL);
  assert(mutex != NULL);

  switch ((rc = cond_timedwait(cond, mutex, reltime))) {
    case DDS_RETCODE_OUT_OF_RESOURCES:
      abort();
    case DDS_RETCODE_TIMEOUT:
      return false;
    default:
      assert(rc == DDS_RETCODE_OK);
      break;
  }

  return true;
}

bool
ddsrt_cond_waituntil(
  ddsrt_cond_t *cond,
  ddsrt_mutex_t *mutex,
  dds_time_t abstime)
{
  dds_return_t rc;
  dds_time_t time;
  dds_duration_t reltime;

  assert(cond != NULL);
  assert(mutex != NULL);

  time = dds_time();
  reltime = (abstime > time ? abstime - time : 0);

  switch ((rc = cond_timedwait(cond, mutex, reltime))) {
    case DDS_RETCODE_OUT_OF_RESOURCES:
      abort();
    case DDS_RETCODE_TIMEOUT:
      return false;
    default:
      assert(rc == DDS_RETCODE_OK);
      break;
  }

  return true;
}

void ddsrt_cond_signal(ddsrt_cond_t *cond)
{
  TaskHandle_t task;

  assert(cond != NULL);

  xSemaphoreTake(cond->sem, portMAX_DELAY);
  if ((task = ddsrt_tasklist_pop(&cond->tasks, NULL)) != NULL) {
    xTaskNotifyGive(task);
  }
  xSemaphoreGive(cond->sem);
}

void ddsrt_cond_broadcast(ddsrt_cond_t *cond)
{
  TaskHandle_t task;

  assert(cond != NULL);

  xSemaphoreTake(cond->sem, portMAX_DELAY);
  while ((task = ddsrt_tasklist_pop(&cond->tasks, NULL)) != NULL) {
    xTaskNotifyGive(task);
  }
  xSemaphoreGive(cond->sem);
}

#define WRITE_LOCKED (-1)
#define UNLOCKED (0)
#define READ_LOCKED (1)

void ddsrt_rwlock_init(ddsrt_rwlock_t *rwlock)
{
  SemaphoreHandle_t sem;
  ddsrt_tasklist_t tasks;

  assert(rwlock != NULL);

  if (ddsrt_tasklist_init(&tasks) == -1) {
    abort();
  }
  if ((sem = xSemaphoreCreateMutex()) == NULL) {
    ddsrt_tasklist_fini(&tasks);
    abort();
  }

  memset(rwlock, 0, sizeof(*rwlock));
  rwlock->sem = sem;
  rwlock->tasks = tasks;
  rwlock->state = UNLOCKED;
}

void ddsrt_rwlock_destroy(ddsrt_rwlock_t *rwlock)
{
  assert(rwlock != NULL);

  vSemaphoreDelete(rwlock->sem);
  ddsrt_tasklist_fini(&rwlock->tasks);
  memset(rwlock, 0, sizeof(*rwlock));
}

void ddsrt_rwlock_read(ddsrt_rwlock_t *rwlock)
{
  TaskHandle_t task = xTaskGetCurrentTaskHandle();

  assert(rwlock != NULL);

  xSemaphoreTake(rwlock->sem, portMAX_DELAY);
  rwlock->rdcnt++;
  if (rwlock->wrcnt != 0) {
    ddsrt_tasklist_push(&rwlock->tasks, task);
    /* Discard pending notifications. */
    ulTaskNotifyTake(1, 0);
    xSemaphoreGive(rwlock->sem);
    /* Wait to be notified. */
    ulTaskNotifyTake(1, portMAX_DELAY);
    xSemaphoreTake(rwlock->sem, portMAX_DELAY);
    ddsrt_tasklist_pop(&rwlock->tasks, task);
  }
  assert(rwlock->state == UNLOCKED ||
         rwlock->state == READ_LOCKED);
  rwlock->cnt++;
  rwlock->state = READ_LOCKED;
  /* Notify next task, if any. */
  if ((task = ddsrt_tasklist_peek(&rwlock->tasks, NULL)) != NULL) {
    xTaskNotifyGive(task);
  }
  xSemaphoreGive(rwlock->sem);
}

void ddsrt_rwlock_write(ddsrt_rwlock_t *rwlock)
{
  TaskHandle_t task = xTaskGetCurrentTaskHandle();

  assert(rwlock != NULL);

  xSemaphoreTake(rwlock->sem, portMAX_DELAY);
  rwlock->wrcnt++;
  if (rwlock->rdcnt != 0 || rwlock->wrcnt != 1) {
    ddsrt_tasklist_push(&rwlock->tasks, task);
    do {
      /* Discard pending notifications. */
      ulTaskNotifyTake(1, 0);
      xSemaphoreGive(rwlock->sem);
      /* Wait to be notified. */
      ulTaskNotifyTake(1, portMAX_DELAY);
      xSemaphoreTake(rwlock->sem, portMAX_DELAY);
    } while (rwlock->state != UNLOCKED);
    ddsrt_tasklist_pop(&rwlock->tasks, task);
  }
  assert(rwlock->cnt == 0);
  assert(rwlock->state == UNLOCKED);
  rwlock->cnt++;
  rwlock->state = WRITE_LOCKED;
  xSemaphoreGive(rwlock->sem);
}

bool ddsrt_rwlock_tryread(ddsrt_rwlock_t *rwlock)
{
  bool locked = false;
  TaskHandle_t task;

  assert(rwlock != NULL);

  xSemaphoreTake(rwlock->sem, portMAX_DELAY);
  if (rwlock->wrcnt == 0) {
    locked = true;
    rwlock->cnt++;
    rwlock->rdcnt++;
    rwlock->state = READ_LOCKED;
    /* Notify next task, if any. */
    if ((task = ddsrt_tasklist_peek(&rwlock->tasks, NULL)) != NULL) {
      xTaskNotifyGive(task);
    }
  }
  xSemaphoreGive(rwlock->sem);

  return locked;
}

bool ddsrt_rwlock_trywrite(ddsrt_rwlock_t *rwlock)
{
  bool locked = false;

  assert(rwlock != NULL);

  xSemaphoreTake(rwlock->sem, 0);
  if (rwlock->rdcnt == 0 && rwlock->wrcnt == 0) {
    locked = true;
    rwlock->cnt++;
    rwlock->wrcnt++;
    rwlock->state = WRITE_LOCKED;
  }
  xSemaphoreGive(rwlock->sem);

  return locked;
}

void ddsrt_rwlock_unlock(ddsrt_rwlock_t *rwlock)
{
  TaskHandle_t task;

  assert(rwlock != NULL);

  xSemaphoreTake(rwlock->sem, portMAX_DELAY);
  assert(rwlock->cnt != 0);
  rwlock->cnt--;
  if (rwlock->state == READ_LOCKED) {
    assert(rwlock->rdcnt != 0);
    rwlock->rdcnt--;
    if (rwlock->rdcnt == 0) {
      rwlock->state = UNLOCKED;
    }
  } else {
    assert(rwlock->state == WRITE_LOCKED);
    assert(rwlock->wrcnt != 0);
    assert(rwlock->cnt == 0);
    rwlock->wrcnt--;
    rwlock->state = UNLOCKED;
  }
  /* Notify next task, if any. */
  if ((rwlock->state == UNLOCKED) &&
      (task = ddsrt_tasklist_peek(&rwlock->tasks, NULL)) != NULL)
  {
    assert(rwlock->rdcnt != 0 ||
           rwlock->wrcnt != 0);
    xTaskNotifyGive(task);
  }
  xSemaphoreGive(rwlock->sem);
}

#define ONCE_NOT_STARTED (1<<0)
#define ONCE_IN_PROGRESS (1<<1)
#define ONCE_FINISHED (1<<2)

/* Wait one millisecond (tick) between polls. */
static const TickType_t once_delay = (configTICK_RATE_HZ / 1000);

void
ddsrt_once(
  ddsrt_once_t *control,
  ddsrt_once_fn init_fn)
{
  int ret, brk = 0;
  uint32_t stat;

  while (brk == 0) {
    stat = ddsrt_atomic_ld32(control);
    /* Verify once control was initialized properly. */
    assert(stat == ONCE_NOT_STARTED ||
           stat == ONCE_IN_PROGRESS ||
           stat == ONCE_FINISHED);

    if ((stat & ONCE_FINISHED) != 0) {
      /* The initialization function has been executed. No reason to block
         execution of this thread. Continue. */
      brk = 1;
    } else if ((stat & ONCE_IN_PROGRESS) != 0) {
      /* Another thread is executing the initialization function. Wait around
         for it to be finished. The polling loop is required because FreeRTOS
         does not offer futexes. */
      vTaskDelay(once_delay);
      /* Repeat. */
    } else {
      /* No thread was executing the initialization function (one might be
         executing it now) at the time of the load. If the atomic compare and
         swap operation is successful, this thread will run the initialization
         function. */
      if (ddsrt_atomic_cas32(
            control, ONCE_NOT_STARTED, ONCE_IN_PROGRESS) != 0)
      {
        /* Function must never block or yield, see reference manual. */
        init_fn();

        ret = (0 == ddsrt_atomic_cas32(
                 control, ONCE_IN_PROGRESS, ONCE_FINISHED));
        assert(ret == 0); (void)ret;

        brk = 1;
      } else {
        /* Another thread updated the state first. Repeat. */
      }
    }
  }

  return;
}
