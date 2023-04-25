// Copyright(c) 2006 to 2022 ZettaScale Technology and others
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
#include <string.h>

#include "threads_priv.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/retcode.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sync.h"

typedef enum {
  THREAD_STARTING = 0,
  THREAD_RUNNING,
  THREAD_EXITING /* Indicates the thread has returned from the specified
                    start_routine, but FreeRTOS may not report it as deleted
                    yet. */
} thread_state_t;

typedef struct {
    ddsrt_thread_routine_t func;
    void *arg;
    TaskHandle_t task; /* Thread identifier for looking up thread context from
                          another thread. Read-only, read by other threads. */
    thread_state_t stat;
    thread_cleanup_t *dtors; /* Cleanup routines. Private. */
    uint32_t ret; /* Return value. NULL if thread has not terminated, maybe
                     NULL if thread has terminated, read by other thread(s)
                     after termination. */
    TaskHandle_t blkd; /* Thread blocked until thread terminates. may or may
                          not be empty when thread terminates. Written by other
                          thread, protected by registry mutex. */
} thread_context_t;

/* Thread registry (in combination with thread context) is required to properly
   implement thread join functionality. */

/* Threads have their own context. The context is automatically allocated and
   initialized, either when the thread is created (local threads) or when the
   API is first used. */

/* FIXME: The same mechanism more-or-less exists in DDSI, perhaps more of the
          logic in DDSI can be moved down at some point? */
typedef struct {
  ddsrt_mutex_t mutex;
  /* The number of available spaces in the thread context array does not have
     to equal the number of used spaces. e.g. when a memory allocation for a
     new thread context array fails when destroying a context it is better to
     leave one space unused. */
  thread_context_t **ctxs;
  size_t cnt;
  size_t len;
} thread_registry_t;

static ddsrt_thread_local thread_context_t *thread_context = NULL;

static thread_registry_t thread_registry;

static ddsrt_once_t thread_registry_once = DDSRT_ONCE_INIT;

static uint32_t non_local_thread(void *arg) { (void)arg; return 0;}

/* FreeRTOS documentation states vTaskGetInfo is intended for debugging because
   its use results in the scheduler remaining suspended for an extended period,
   but the scheduler is only suspended if eTaskState is not eInvalid. */
ddsrt_tid_t
ddsrt_gettid(void)
{
  TaskStatus_t status;

  vTaskGetInfo(xTaskGetCurrentTaskHandle(), &status, pdFALSE, eInvalid);

  return status.xTaskNumber;
}

DDS_EXPORT ddsrt_tid_t
ddsrt_gettid_for_thread( ddsrt_thread_t thread)
{
  return (ddsrt_tid_t)thread.task;
}


ddsrt_thread_t
ddsrt_thread_self(void)
{
  ddsrt_thread_t thr = { .task = xTaskGetCurrentTaskHandle() };

  return thr;
}

bool ddsrt_thread_equal(ddsrt_thread_t a, ddsrt_thread_t b)
{
  return (a.task == b.task);
}

size_t
ddsrt_thread_getname(char *__restrict name, size_t size)
{
  char *ptr;

  assert(name != NULL);
  assert(size >= 1);

  if ((ptr = pcTaskGetName(NULL)) == NULL) {
    ptr = "";
  }

  return ddsrt_strlcpy(name, ptr, size);
}

static void
thread_registry_init(void)
{
  /* One time initialization guaranteed by ddsrt_once. */
  (void)memset(&thread_registry, 0, sizeof(thread_registry));
  ddsrt_mutex_init(&thread_registry.mutex);
}

static thread_context_t *
thread_context_find(TaskHandle_t task)
{
  thread_context_t *ctx = NULL;

  for (size_t i = 0; i < thread_registry.cnt && ctx == NULL; i++) {
    if (thread_registry.ctxs[i] != NULL &&
        thread_registry.ctxs[i]->task == task)
    {
      ctx = thread_registry.ctxs[i];
    }
  }

  return ctx;
}

static dds_return_t
thread_context_create(thread_context_t **ctxptr)
{
  dds_return_t rc = DDS_RETCODE_OK;
  size_t len;
  thread_context_t *ctx = NULL, **ctxs = NULL;

  assert(ctxptr != NULL);

  ctx = ddsrt_calloc(1, sizeof(*ctx));
  if (ctx == NULL) {
    rc = DDS_RETCODE_OUT_OF_RESOURCES;
  } else {
    if (thread_registry.cnt < thread_registry.len) {
      len = thread_registry.len;
      ctxs = thread_registry.ctxs;
    } else {
      assert(thread_registry.cnt == thread_registry.len);
      len = thread_registry.len + 1;
      ctxs = ddsrt_realloc(thread_registry.ctxs, len * sizeof(ctx));
    }

    if (ctxs == NULL) {
      ddsrt_free(ctx);
      rc = DDS_RETCODE_OUT_OF_RESOURCES;
    } else {
      ctxs[thread_registry.cnt++] = *ctxptr = ctx;
      thread_registry.len = len;
      thread_registry.ctxs = ctxs;
    }
  }

  return rc;
}

#define thread_context_require() thread_context_acquire(NULL)

static dds_return_t
thread_context_acquire(thread_context_t **ctxptr)
{
  dds_return_t rc = DDS_RETCODE_OK;
  thread_context_t *ctx = thread_context;

  if (ctx == NULL) {
    /* Dynamically initialize global thread registry (exactly once). */
    ddsrt_once(&thread_registry_once, &thread_registry_init);

    ddsrt_mutex_lock(&thread_registry.mutex);
    if ((rc = thread_context_create(&ctx)) == 0) {
      /* This situation only arises for non-native (not created in our code)
         threads. Some members must therefore still be initialized to ensure
         proper operation. */
      ctx->func = &non_local_thread;
      ctx->stat = THREAD_RUNNING;
      ctx->task = xTaskGetCurrentTaskHandle();
    }
    ddsrt_mutex_unlock(&thread_registry.mutex);
    thread_context = ctx;
  } else {
    assert(ctx->func != NULL);
    assert(ctx->stat == THREAD_RUNNING);
    assert(ctx->task == xTaskGetCurrentTaskHandle());
  }

  if (rc == DDS_RETCODE_OK && ctxptr != NULL) {
    assert(ctx != NULL);
    *ctxptr = ctx;
  }

  return rc;
}

static void
thread_context_destroy(thread_context_t *ctx)
{
  size_t i = 0;
  thread_context_t **arr;

  if (ctx != NULL) {
    while (i < thread_registry.cnt && thread_registry.ctxs[i] != ctx) {
      i++;
    }

    if (i < thread_registry.cnt) {
      thread_registry.ctxs[i] = NULL;
      if (i < (thread_registry.cnt - 1)) {
        (void)memmove(
            thread_registry.ctxs + (i),
            thread_registry.ctxs + (i+1),
            (thread_registry.cnt - (i+1)) * sizeof(*thread_registry.ctxs));
      }
      thread_registry.cnt--;

      /* Free contexts when count reaches zero. */
      if (thread_registry.cnt == 0) {
        ddsrt_free(thread_registry.ctxs);
        thread_registry.ctxs = NULL;
        thread_registry.len = 0;
      } else {
        arr = ddsrt_realloc(
          thread_registry.ctxs,
          thread_registry.cnt * sizeof(*thread_registry.ctxs));
        /* Ignore allocation failure, save free spot. */
        if (arr != NULL) {
          thread_registry.ctxs = arr;
          thread_registry.len = thread_registry.cnt;
        }
      }
    }

    ddsrt_free(ctx);
  }
}

static void
thread_fini(thread_context_t *ctx, uint32_t ret)
{
  thread_cleanup_t *tail;

  assert(ctx != NULL);

  /* Acquire registry lock to publish task result and state. */
  ddsrt_mutex_lock(&thread_registry.mutex);

  /* Pop all cleanup handlers from the thread's cleanup stack. */
  while ((tail = ctx->dtors) != NULL) {
    ctx->dtors = tail->prev;
    if (tail->routine != 0) {
      tail->routine(tail->arg);
    }
    ddsrt_free(tail);
  }

  /* FreeRTOS can report task state, but doesn't register the result or
     notifies a thread that wants to join. */
  ctx->ret = ret;
  ctx->stat = THREAD_EXITING;

  /* Thread resources will be leaked (especially for non-local threads)
     if not reclaimed by a thread join. Local threads (threads created
     within the DDS stack) are required to be joined. Thread resource
     leakage for local threads must be considered a bug. Non-local
     threads, however, are not aware that there are resources that must
     be reclaimed and local threads might not be aware that there are
     non-local threads that must be joined. Therefore, if a non-local thread
     exits, it's resources are reclaimed if no thread is waiting to join. */
  if (ctx->blkd != NULL) {
    /* Task join functionality is based on notifications, as it is
       significantly faster than using a queue, semaphore or event group to
       perform an equivalent operation.

       When a task receives a notification, it's notification state is set to
       pending. When it reads it's notification state, the notification state
       is set to not-pending. A task can wait, with an optional time out, for
       it's notification state to become pending. */

    /* Ignore result, there's nothing that can be done on failure and it always
       returns pdPASS. */
    (void)xTaskNotifyGive(ctx->blkd);
  } else if (ctx->func == &non_local_thread) {
    assert(ret == 0);
    thread_context_destroy(ctx);
  }

  ddsrt_mutex_unlock(&thread_registry.mutex);
}

static void
thread_start_routine(void *arg)
{
  thread_context_t *ctx = (thread_context_t *)arg;
  uint32_t ret;

  ddsrt_mutex_lock(&thread_registry.mutex);
  /* Context for the current task is always correctly initialized and
     registered at this stage. It's not strictly required to update task
     state, but the synchronization itself is. */
  ctx->stat = THREAD_RUNNING;
  ddsrt_mutex_unlock(&thread_registry.mutex);

  /* Thread-local storage is initialized by the function that creates the
     thread because a reference to the thread's context is stored and
     synchronization is considerably easier if it's handled there. */

  thread_context = ctx;
  ret = ctx->func(ctx->arg);

  thread_fini(ctx, ret); /* DO NOT DEREFERENCE THREAD CONTEXT ANYMORE! */

  /* Delete current task. */
  vTaskDelete(NULL);
}

/* xTaskCreate takes the stack depth in the number of words (NOT bytes). In
   practice this simply means it multiplies the given number with the size
   of StackType_t in bytes (see tasks.c). FreeRTOSConfig.h must define
   configMINIMAL_STACK_SIZE, which is the stack size in words allocated for
   the idle task. */
#define WORD_SIZE (sizeof(StackType_t))
/* configMINIMAL_STACK_SIZE is applied as the default stack size. Whether or
   not this is considered a sane default depends on the target. The default can
   be adjusted in FreeRTOSConfig.h Of course the configuration file also allows
   the user to change it on a per-thread basis at runtime. */
#define MIN_STACK_SIZE ((uint16_t)(configMINIMAL_STACK_SIZE * WORD_SIZE))

dds_return_t
ddsrt_thread_create(
  ddsrt_thread_t *thread,
  const char *name,
  const ddsrt_threadattr_t *attr,
  ddsrt_thread_routine_t start_routine,
  void *arg)
{
  dds_return_t rc;
  TaskHandle_t task;
  UBaseType_t prio;
  uint16_t size = MIN_STACK_SIZE;
  thread_context_t *ctx = NULL;

  assert(thread != NULL);
  assert(name != NULL);
  assert(attr != NULL);
  assert(start_routine != 0);

  if ((rc = thread_context_require()) != DDS_RETCODE_OK) {
    return rc;
  }

  /* Non-realtime scheduling does not exist in FreeRTOS. */
  if (attr->schedClass != DDSRT_SCHED_DEFAULT &&
      attr->schedClass != DDSRT_SCHED_REALTIME)
  {
    return DDS_RETCODE_BAD_PARAMETER;
  } else if (attr->schedPriority < 0 ||
             attr->schedPriority > (configMAX_PRIORITIES - 1))
  {
    return DDS_RETCODE_BAD_PARAMETER;
  }

  /* Stack size is quietly increased to match at least the minimum. */
  if (attr->stackSize > size) {
    size = (uint16_t)(attr->stackSize / WORD_SIZE);
    if (attr->stackSize % WORD_SIZE) {
      size++;
    }
  }

  /* Assume that when the default priority of zero (0) is specified, the user
     wants the thread to inherit the priority of the calling thread. */
  assert(0 == tskIDLE_PRIORITY);
  if (attr->schedPriority == 0) {
    prio = uxTaskPriorityGet(NULL);
  } else {
    prio = (UBaseType_t)attr->schedPriority;
  }

  ddsrt_mutex_lock(&thread_registry.mutex);

  /* Thread context is allocated here so that it can be handled when no more
     memory is available. Simply storing the entire context in thread-local
     storage would have been possible, but would require the implementation to
     define and allocate a separate struct in order to support thread joins. */
  if ((rc = thread_context_create(&ctx)) == DDS_RETCODE_OK) {
    ctx->func = start_routine;
    ctx->arg = arg;

    if (pdPASS != xTaskCreate(
          &thread_start_routine, name, size, ctx, prio, &task))
    {
      thread_context_destroy(ctx);
      rc = DDS_RETCODE_OUT_OF_RESOURCES;
    } else {
      thread->task = ctx->task = task;
    }
  }

  ddsrt_mutex_unlock(&thread_registry.mutex);

  return rc;
}

void
ddsrt_thread_init(uint32_t reason)
{
  (void)reason;
  if (thread_context_require() != DDS_RETCODE_OK) {
    assert(0);
  }
}

void
ddsrt_thread_fini(uint32_t reason)
{
  thread_context_t *ctx;

  (void)reason;
  /* NO-OP if no context exists since thread-local storage and cleanup
     handler references are both stored in the thread context. */
  if ((ctx = thread_context) != NULL) {
    assert(ctx->func != &non_local_thread);
    thread_fini(ctx, 0);
  }
}

dds_return_t
ddsrt_thread_join(ddsrt_thread_t thread, uint32_t *thread_result)
{
  dds_return_t rc;
  thread_context_t *ctx;
  eTaskState status;

  if ((rc = thread_context_require()) != DDS_RETCODE_OK) {
    return rc;
  }

  ddsrt_mutex_lock(&thread_registry.mutex);
  ctx = thread_context_find(thread.task);
  if (ctx != NULL) {
    /* Task should never be joined by multiple tasks simultaneously */
    assert(ctx->blkd == NULL);
    rc = DDS_RETCODE_TRY_AGAIN;

    do {
      (void)memset(&status, 0, sizeof(status));
      status = eTaskGetState(thread.task);
      if (status == eDeleted) {
        /* FreeRTOS reports the task is deleted. Require the context to exist,
           fetch the result and free the context afterwards. */
        assert(ctx != NULL);
        rc = DDS_RETCODE_OK;
      } else if (status != eInvalid) {
        assert(ctx != NULL);
        /* FreeRTOS reports the task is still active. That does not mean the
           task has not yet returned from start_routine. */
        if (ctx->stat == THREAD_EXITING) {
          /* Thread context will not be accessed by the thread itself anymore
             and it should be safe to free it. */
          rc = DDS_RETCODE_OK;
        } else {
          ctx->blkd = xTaskGetCurrentTaskHandle();

          /* Reset notify state and counter. */
          ulTaskNotifyTake(pdTRUE, 0);

          ddsrt_mutex_unlock(&thread_registry.mutex);

          /* Wait to be notified. */
          ulTaskNotifyTake(pdTRUE, portMAX_DELAY);

          ddsrt_mutex_lock(&thread_registry.mutex);
        }
      } else {
        rc = DDS_RETCODE_BAD_PARAMETER;
      }
    } while (rc == DDS_RETCODE_TRY_AGAIN);

    if (rc == DDS_RETCODE_OK) {
      if (thread_result != NULL) {
        *thread_result = ctx->ret;
      }
      thread_context_destroy(ctx);
    }
  }

  ddsrt_mutex_unlock(&thread_registry.mutex);

  return rc;
}

dds_return_t
ddsrt_thread_cleanup_push(void (*routine)(void *), void *arg)
{
  dds_return_t rc = DDS_RETCODE_OK;
  thread_cleanup_t *tail = NULL;
  thread_context_t *ctx;

  assert(routine != NULL);

  if (thread_context_acquire(&ctx) == 0) {
    if ((tail = ddsrt_malloc(sizeof(*tail))) == NULL) {
      rc = DDS_RETCODE_OUT_OF_RESOURCES;
    } else {
      tail->prev = ctx->dtors;
      tail->routine = routine;
      tail->arg = arg;
      ctx->dtors = tail;
    }
  }

  return rc;
}

dds_return_t
ddsrt_thread_cleanup_pop(int execute)
{
  thread_cleanup_t *tail;
  thread_context_t *ctx;

  if (thread_context_acquire(&ctx) == 0) {
    if ((tail = ctx->dtors) != NULL) {
      ctx->dtors = tail->prev;
      if (execute) {
        tail->routine(tail->arg);
      }
      ddsrt_free(tail);
    }
  }

  return DDS_RETCODE_OK;
}
