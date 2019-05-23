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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/threads_priv.h"

typedef struct {
  char *name;
  ddsrt_thread_routine_t routine;
  void *arg;
} thread_context_t;

static uint32_t
os_startRoutineWrapper(
  void *threadContext)
{
  thread_context_t *context = threadContext;
  uint32_t resultValue = 0;

  ddsrt_thread_setname(context->name);

  /* Call the user routine */
  resultValue = context->routine(context->arg);

  /* Free the thread context resources, arguments is responsibility */
  /* for the caller of os_threadCreate                                */
  ddsrt_free(context->name);
  ddsrt_free(context);

  /* return the result of the user routine */
  return resultValue;
}

dds_retcode_t
ddsrt_thread_create(
  ddsrt_thread_t *thrptr,
  const char *name,
  const ddsrt_threadattr_t *attr,
  ddsrt_thread_routine_t start_routine,
  void *arg)
{
  ddsrt_thread_t thr;
  thread_context_t *ctx;
  int32_t prio;

  assert(thrptr != NULL);
  assert(name != NULL);
  assert(attr != NULL);
  assert(start_routine != NULL);

  if ((ctx = ddsrt_malloc(sizeof(*ctx))) == NULL ||
      (ctx->name = ddsrt_strdup(name)) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  ctx->routine = start_routine;
  ctx->arg = arg;
  thr.handle = CreateThread(NULL,
    (SIZE_T)attr->stackSize,
    (LPTHREAD_START_ROUTINE)os_startRoutineWrapper,
    (LPVOID)ctx,
    (DWORD)0,
    &thr.tid);

  if (thr.handle == NULL)
    return DDS_RETCODE_ERROR;

  *thrptr = thr;

  /* Windows thread priorities are in the range below:
      -15 : THREAD_PRIORITY_IDLE
       -2 : THREAD_PRIORITY_LOWEST
       -1 : THREAD_PRIORITY_BELOW_NORMAL
        0 : THREAD_PRIORITY_NORMAL
        1 : THREAD_PRIORITY_ABOVE_NORMAL
        2 : THREAD_PRIORITY_HIGHEST
       15 : THREAD_PRIORITY_TIME_CRITICAL
    For realtime threads additional values are allowed: */

  /* PROCESS_QUERY_INFORMATION rights required to call GetPriorityClass
     Ensure that priorities are effectively in the allowed range depending
     on GetPriorityClass result. */
  /* FIXME: The logic here might be incorrect.
     https://docs.microsoft.com/en-us/windows/desktop/api/processthreadsapi/nf-processthreadsapi-setthreadpriority */
  prio = attr->schedPriority;
  if (GetPriorityClass(GetCurrentProcess()) == REALTIME_PRIORITY_CLASS) {
    if (attr->schedPriority < -7)
      prio = THREAD_PRIORITY_IDLE;
    else if (attr->schedPriority > 6)
      prio = THREAD_PRIORITY_TIME_CRITICAL;
  } else {
    if (attr->schedPriority < THREAD_PRIORITY_LOWEST)
      prio = THREAD_PRIORITY_IDLE;
    else if (attr->schedPriority > THREAD_PRIORITY_HIGHEST)
      prio = THREAD_PRIORITY_TIME_CRITICAL;
  }

  if (SetThreadPriority(thr.handle, prio) == 0) {
    DDS_WARNING("SetThreadPriority failed with %i\n", GetLastError());
  }

  return DDS_RETCODE_OK;
}

ddsrt_tid_t
ddsrt_gettid(void)
{
  return GetCurrentThreadId();
}

ddsrt_thread_t
ddsrt_thread_self(
    void)
{
  ddsrt_thread_t thr;
  thr.tid = GetCurrentThreadId();
  thr.handle = GetCurrentThread(); /* pseudo HANDLE, no need to close it */

  return thr;
}

bool ddsrt_thread_equal(ddsrt_thread_t a, ddsrt_thread_t b)
{
  return a.tid == b.tid;
}

/* ES: dds2086: Close handle should not be performed here. Instead the handle
 * should not be closed until the os_threadWaitExit(...) call is called.
 * CloseHandle (threadHandle);
 */
dds_retcode_t
ddsrt_thread_join(
  ddsrt_thread_t thread,
  uint32_t *thread_result)
{
    DWORD tr;
    DWORD waitres;
    BOOL status;

    if (thread.handle == NULL) {
        return DDS_RETCODE_BAD_PARAMETER;
    }

    waitres = WaitForSingleObject(thread.handle, INFINITE);
    if (waitres != WAIT_OBJECT_0) {
        //err = GetLastError();
        //OS_DEBUG_1("os_threadWaitExit", "WaitForSingleObject Failed %d", err);
        return DDS_RETCODE_ERROR;
    }

    status = GetExitCodeThread(thread.handle, &tr);
    if (!status) {
       //err = GetLastError();
       //OS_DEBUG_1("os_threadWaitExit", "GetExitCodeThread Failed %d", err);
       return DDS_RETCODE_ERROR;
    }

    assert(tr != STILL_ACTIVE);
    if (thread_result) {
        *thread_result = tr;
    }
    CloseHandle(thread.handle);

    return DDS_RETCODE_OK;
}

/* Thread names on Linux are limited to 16 bytes, no reason to provide
   more storage than that as internal threads must adhere to that limit. */
static ddsrt_thread_local char thread_name[16] = "";

size_t
ddsrt_thread_getname(
  char *__restrict str,
  size_t size)
{
  size_t cnt;

  assert(str != NULL);

  if ((cnt = ddsrt_strlcpy(str, thread_name, size)) == 0) {
    ddsrt_tid_t tid = ddsrt_gettid();
    cnt = (size_t)snprintf(str, size, "%"PRIdTID, tid);
    assert(cnt >= 0);
  }

  return cnt;
}

static const DWORD MS_VC_EXCEPTION=0x406D1388;

#pragma pack(push,8)
typedef struct tagTHREADNAME_INFO
{
   DWORD dwType; /** Must be 0x1000. */
   LPCSTR szName; /** Pointer to name (in user addr space). */
   DWORD dwThreadID; /** Thread ID (-1=caller thread). */
   DWORD dwFlags; /**  Reserved for future use, must be zero. */
} THREADNAME_INFO;
#pragma pack(pop)

/** \brief Wrap thread start routine
 *
 * \b os_startRoutineWrapper wraps a threads starting routine.
 * before calling the user routine. It tries to set a thread name
 * that will be visible if the process is running under the MS
 * debugger.
 */
void
ddsrt_thread_setname(
  const char *__restrict name)
{
    assert(name != NULL);

    THREADNAME_INFO info;
    info.dwType = 0x1000;
    info.szName = name;
    info.dwThreadID = -1;
    info.dwFlags = 0;

    /* Empty try/except that catches everything on purpose to set the thread
       name. See: http://msdn.microsoft.com/en-us/library/xcb2z8hs.aspx. */
#pragma warning(push)
#pragma warning(disable: 6320 6322)
    __try
    {
        RaiseException(MS_VC_EXCEPTION, 0, sizeof(info)/sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        /* Suppress warnings. */
    }
#pragma warning(pop)

    ddsrt_strlcpy(thread_name, name, sizeof(thread_name));
}


static ddsrt_thread_local thread_cleanup_t *thread_cleanup = NULL;

dds_retcode_t ddsrt_thread_cleanup_push(void (*routine)(void *), void *arg)
{
  thread_cleanup_t *tail;

  assert(routine != NULL);

  if ((tail = ddsrt_malloc(sizeof(thread_cleanup_t))) != NULL) {
    tail->prev = thread_cleanup;
    thread_cleanup = tail;
    assert(tail != NULL);
    tail->routine = routine;
    tail->arg = arg;
    return DDS_RETCODE_OK;
  }
  return DDS_RETCODE_OUT_OF_RESOURCES;
}

dds_retcode_t ddsrt_thread_cleanup_pop(int execute)
{
  thread_cleanup_t *tail;

  if ((tail = thread_cleanup) != NULL) {
    thread_cleanup = tail->prev;
    if (execute) {
      tail->routine(tail->arg);
    }
    ddsrt_free(tail);
  }
  return DDS_RETCODE_OK;
}

static void
thread_cleanup_fini(void)
{
  thread_cleanup_t *tail, *prev;

  tail = thread_cleanup;
  while (tail != NULL) {
    prev = tail->prev;
    assert(tail->routine != NULL);
    tail->routine(tail->arg);
    ddsrt_free(tail);
    tail = prev;
  }

  thread_cleanup = NULL;
}

void
ddsrt_thread_init(void)
{
  return;
}

void
ddsrt_thread_fini(void)
{
  thread_cleanup_fini();
}
