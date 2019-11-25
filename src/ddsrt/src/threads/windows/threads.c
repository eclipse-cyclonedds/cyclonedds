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

/* tlhelp32 for ddsrt_thread_list */
#include <stdio.h>
#include <stdlib.h>
#include <tlhelp32.h>

/* {Get,Set}ThreadDescription is the Windows 10 interface for dealing with thread names, but it at
   least in some setups the linker can't find the symbols in kernel32.lib, even though kernel32.dll
   exports them.  (Perhaps it is just a broken installation, who knows ...)  Looking them up
   dynamically works fine.  */
typedef HRESULT (WINAPI *SetThreadDescription_t) (HANDLE hThread, PCWSTR lpThreadDescription);
typedef HRESULT (WINAPI *GetThreadDescription_t) (HANDLE hThread, PWSTR *ppszThreadDescription);
static volatile SetThreadDescription_t SetThreadDescription_ptr = 0;
static volatile GetThreadDescription_t GetThreadDescription_ptr = 0;

static HRESULT WINAPI SetThreadDescription_dummy (HANDLE hThread, PCWSTR lpThreadDescription)
{
  (void) hThread;
  (void) lpThreadDescription;
  return E_FAIL;
}

static HRESULT WINAPI GetThreadDescription_dummy (HANDLE hThread, PWSTR *ppszThreadDescription)
{
  (void) hThread;
  return E_FAIL;
}

static void getset_threaddescription_addresses (void)
{
  /* Rely on MSVC's interpretation of the meaning of volatile
     to order checking & setting the pointers */
  if (GetThreadDescription_ptr == 0)
  {
    HMODULE mod;
    FARPROC p;
    if (!GetModuleHandleExA (GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, "kernel32.dll", &mod))
    {
      SetThreadDescription_ptr = SetThreadDescription_dummy;
      GetThreadDescription_ptr = GetThreadDescription_dummy;
    }
    else
    {
      if ((p = GetProcAddress (mod, "SetThreadDescription")) != 0)
        SetThreadDescription_ptr = (SetThreadDescription_t) p;
      else
        SetThreadDescription_ptr = SetThreadDescription_dummy;
      if ((p = GetProcAddress (mod, "GetThreadDescription")) != 0)
        GetThreadDescription_ptr = (GetThreadDescription_t) p;
      else
        GetThreadDescription_ptr = GetThreadDescription_dummy;
    }
  }
}

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

dds_return_t
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
dds_return_t
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
   more storage than that as internal threads must adhere to that limit.
   Use the thread-local variable instead of relying on GetThreadDescription
   to avoid the dynamic memory allocation, as the thread name is used by
   the logging code and the overhead there matters. */
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

/** \brief Set thread name for debugging and system monitoring
 *
 * Windows 10 introduced the SetThreadDescription function, which is
 * obviously the sane interface.  For reasons unknown to me, the
 * linker claims to have no knowledge of the function, even though
 * they appear present, and so it seems to sensible to retain the
 * old exception-based trick as a fall-back mechanism.  At least
 * until the reason for {Get,Set}Description's absence from the
 * regular libraries.
 */
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

void
ddsrt_thread_setname(
  const char *__restrict name)
{
  assert (name != NULL);
  getset_threaddescription_addresses ();
  if (SetThreadDescription_ptr != SetThreadDescription_dummy)
  {
    size_t size = strlen (name) + 1;
    wchar_t *wname = malloc (size * sizeof (*wname));
    size_t cnt = 0;
    mbstowcs_s (&cnt, wname, size, name, _TRUNCATE);
    SetThreadDescription_ptr (GetCurrentThread (), wname);
    free (wname);
  }
  else
  {
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
  }
  (void)ddsrt_strlcpy (thread_name, name, sizeof (thread_name));
}

dds_return_t
ddsrt_thread_list (
  ddsrt_thread_list_id_t * __restrict tids,
  size_t size)
{
  HANDLE hThreadSnap;
  THREADENTRY32 te32;
  const DWORD pid = GetCurrentProcessId ();
  int32_t n = 0;

  if ((hThreadSnap = CreateToolhelp32Snapshot (TH32CS_SNAPTHREAD, 0)) == INVALID_HANDLE_VALUE)
    return 0;

  memset (&te32, 0, sizeof (te32));
  te32.dwSize = sizeof (THREADENTRY32);
  if (!Thread32First (hThreadSnap, &te32))
  {
    CloseHandle (hThreadSnap);
    return 0;
  }

  do {
    if (te32.th32OwnerProcessID != pid)
      continue;
    if ((size_t) n < size)
    {
      /* get a handle to the thread, not counting the thread the thread if no such
         handle is obtainable */
      if ((tids[n] = OpenThread (THREAD_QUERY_INFORMATION, FALSE, te32.th32ThreadID)) == NULL)
        continue;
    }
    n++;
  } while (Thread32Next (hThreadSnap, &te32));
  CloseHandle (hThreadSnap);
  return n;
}

dds_return_t
ddsrt_thread_getname_anythread (
  ddsrt_thread_list_id_t tid,
  char * __restrict name,
  size_t size)
{
  getset_threaddescription_addresses ();
  if (size > 0)
  {
    PWSTR data;
    HRESULT hr = GetThreadDescription_ptr (tid, &data);
    if (! SUCCEEDED (hr))
      name[0] = 0;
    else
    {
      size_t cnt;
      wcstombs_s (&cnt, name, size, data, _TRUNCATE);
      LocalFree (data);
    }
    if (name[0] == 0)
    {
      snprintf (name, sizeof (name), "%"PRIdTID, GetThreadId (tid));
    }
  }
  return DDS_RETCODE_OK;
}

static ddsrt_thread_local thread_cleanup_t *thread_cleanup = NULL;

dds_return_t ddsrt_thread_cleanup_push(void (*routine)(void *), void *arg)
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

dds_return_t ddsrt_thread_cleanup_pop(int execute)
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
