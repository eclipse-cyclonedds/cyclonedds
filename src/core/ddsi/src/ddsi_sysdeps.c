// Copyright(c) 2006 to 2021 ZettaScale Technology and others
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

#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/misc.h"

#include "ddsi__log.h"
#include "ddsi__sysdeps.h"

#if (defined __APPLE__ || (defined __linux && (defined __GLIBC__ || defined __UCLIBC__))) || (__GNUC__ > 0 && (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) < 40100) && ! DDSRT_WITH_FREERTOS
#include <errno.h>
#include <execinfo.h>
#include <signal.h>

static ddsrt_atomic_uint32_t log_stacktrace_flag = DDSRT_ATOMIC_UINT32_INIT(0);
static struct {
  int depth;
  void *stk[64];
} log_stacktrace_stk;


static void log_stacktrace_sigh (int sig __attribute__ ((unused)))
{
  int e = errno;
  log_stacktrace_stk.depth = backtrace (log_stacktrace_stk.stk, (int) (sizeof (log_stacktrace_stk.stk) / sizeof (*log_stacktrace_stk.stk)));
  ddsrt_atomic_inc32 (&log_stacktrace_flag);
  errno = e;
}

void ddsi_log_stacktrace (const struct ddsrt_log_cfg *logcfg, const char *name, ddsrt_thread_t tid)
{
  const dds_time_t d = 1000000;
  struct sigaction act, oact;
  char **strs;
  int i;
  DDS_CLOG (~DDS_LC_FATAL, logcfg, "-- stack trace of %s requested --\n", name);
  act.sa_handler = log_stacktrace_sigh;
  act.sa_flags = 0;
  sigfillset (&act.sa_mask);
  while (!ddsrt_atomic_cas32 (&log_stacktrace_flag, 0, 1))
    dds_sleepfor (d);
  sigaction (SIGXCPU, &act, &oact);
  pthread_kill (tid.v, SIGXCPU);
  while (!ddsrt_atomic_cas32 (&log_stacktrace_flag, 2, 3) && pthread_kill (tid.v, 0) == 0)
    dds_sleepfor (d);
  sigaction (SIGXCPU, &oact, NULL);
  if (pthread_kill (tid.v, 0) != 0)
    DDS_CLOG (~DDS_LC_FATAL, logcfg, "-- thread exited --\n");
  else
  {
    DDS_CLOG (~DDS_LC_FATAL, logcfg, "-- stack trace follows --\n");
    strs = backtrace_symbols (log_stacktrace_stk.stk, log_stacktrace_stk.depth);
    for (i = 0; i < log_stacktrace_stk.depth; i++)
      DDS_CLOG (~DDS_LC_FATAL, logcfg, "%s\n", strs[i]);
    free (strs);
    DDS_CLOG (~DDS_LC_FATAL, logcfg, "-- end of stack trace --\n");
  }
  ddsrt_atomic_st32 (&log_stacktrace_flag, 0);
}
#elif defined _MSC_VER
#include <dbghelp.h>

static ddsrt_atomic_uint32_t log_stacktrace_init = DDSRT_ATOMIC_UINT32_INIT(0);

static bool really_do_stacktrace (void)
{
  uint32_t init = ddsrt_atomic_ld32 (&log_stacktrace_init);
  switch (init)
  {
    case 0:
      if (!ddsrt_atomic_cas32 (&log_stacktrace_init, 0, 1))
        return false;
      if (!SymInitialize (GetCurrentProcess (), NULL, TRUE))
        return false; // leave "init" at 1 -> no further time wasted
      ddsrt_atomic_st32 (&log_stacktrace_init, 2);
      return true;
    case 1:
      return false;
    default:
      return true;
  }
}

static DWORD init_stackframe (STACKFRAME64 *frame, const CONTEXT *context)
{
  memset (frame, 0, sizeof (*frame));
#ifdef _M_IX86
  frame->AddrPC.Offset = context->Eip;
  frame->AddrPC.Mode = AddrModeFlat;
  frame->AddrFrame.Offset = context->Ebp;
  frame->AddrFrame.Mode = AddrModeFlat;
  frame->AddrStack.Offset = context->Esp;
  frame->AddrStack.Mode = AddrModeFlat;
  return IMAGE_FILE_MACHINE_I386;
#elif _M_X64
  frame->AddrPC.Offset = context->Rip;
  frame->AddrPC.Mode = AddrModeFlat;
  frame->AddrFrame.Offset = context->Rsp;
  frame->AddrFrame.Mode = AddrModeFlat;
  frame->AddrStack.Offset = context->Rsp;
  frame->AddrStack.Mode = AddrModeFlat;
  return IMAGE_FILE_MACHINE_AMD64;
#elif _M_IA64
  frame->AddrPC.Offset = context->StIIP;
  frame->AddrPC.Mode = AddrModeFlat;
  frame->AddrFrame.Offset = context->IntSp;
  frame->AddrFrame.Mode = AddrModeFlat;
  frame->AddrBStore.Offset = context->RsBSP;
  frame->AddrBStore.Mode = AddrModeFlat;
  frame->AddrStack.Offset = context->IntSp;
  frame->AddrStack.Mode = AddrModeFlat;
  return IMAGE_FILE_MACHINE_IA64;
#else
#error "Unsupported machine type for windows stacktrace support"
#endif
}

static int copy_thread_context (CONTEXT *dst, struct _EXCEPTION_POINTERS *ep)
{
  // Used to get a thread context off the current thread
  *dst = *ep->ContextRecord;
  return EXCEPTION_EXECUTE_HANDLER;
}

void ddsi_log_stacktrace (const struct ddsrt_log_cfg *logcfg, const char *name, ddsrt_thread_t tid)
{
  const char *errmsg = "unknown";
  HANDLE handle = 0;
  bool close_required = false;

  if (!really_do_stacktrace ())
    return;

  DDS_CLOG (~DDS_LC_FATAL, logcfg, "-- stack trace of %s requested --\n", name);

  // GetCurrentThread() returns a pseudo handle which is really a magic constant (-2).
  // This is what we get for threads not created by Cyclone DDS itself.  If we get this,
  // we'd better open the thread using the thread id to get a handle.
  //
  // If it is this thread, we have to avoid suspending it.  The only way of knowing
  // whether it really is this thread is by inspecting the "tid" field because of how
  // the tid/handle pair (ab)used for threads not created by Cyclone itself.  If it is
  // this thread, it really doesn't matter whether "handle" is the pseudo handle or a
  // real handle, either will work.
  const bool self = (tid.tid == GetCurrentThreadId ());
  if (self || tid.handle != GetCurrentThread ())
    handle = tid.handle;
  else if ((handle = OpenThread (THREAD_ALL_ACCESS, 0, tid.tid)) != 0)
    close_required = true;
  else
  {
    errmsg = "OpenThread";
    goto fail;
  }

  // Once the thread is suspended we can get the context and walk the stack
  CONTEXT context;
  memset (&context, 0, sizeof (context));
  if (!self)
  {
    if (SuspendThread (handle) == (DWORD) -1)
    {
      errmsg = "SuspendThread";
      goto fail;
    }

    // Walking the stack requires the current thread state as a starting point.  Getting the
    // context requires ContextFlags to be initialized, but it doesn't say what to put in.
    // Some kind soul on the Internet figured out that CONTEXT_ALL does the trick.
    //
    // Per Windows docs: GetThreadContext is only supported on a suspended thread, therefore
    // not on the current thread.
    context.ContextFlags = CONTEXT_ALL;
    if (!GetThreadContext (handle, &context))
    {
      (void) ResumeThread (handle);
      errmsg = "GetThreadContext";
      goto fail;
    }
  }
  else
  {
    // If GetThreadContext doesn't work on the current thread, exceptions do and give access
    // to a thread context in the exception filter:
    //
    //   [...] you can use GetExceptionInformation only within the exception
    //   filter expression. The information it points to is generally on the stack and is no
    //   longer available when control gets transferred to the exception handler.
    //
    // Here the exception occurs in the same function, so presumably the thread context
    // remains valid and it is ok to copy it out of the exception information and use it for
    // walking the stack.
    __try {
      *((volatile int *)0) = 0; // intentional access violation
    } __except (copy_thread_context (&context, GetExceptionInformation ())) {
      // nothing to be done
    }
  }

  // Loop over the stack frames, storing the addresses
  STACKFRAME64 stackframe;
  const DWORD image_file_machine = init_stackframe (&stackframe, &context);
  DWORD64 stk[64];
  size_t nstk = 0;
  const HANDLE process = GetCurrentProcess ();
  while (nstk < sizeof (stk) / sizeof (stk[0]))
  {
    if (!StackWalk64 (image_file_machine, process, handle, &stackframe, &context, 0, SymFunctionTableAccess64, SymGetModuleBase64, 0))
      break;
    stk[nstk++] = (uintptr_t) stackframe.AddrPC.Offset;
  }

  // Symbol lookup is presumably relatively expensive, so once the address have been
  // captured, resume the thread.  There is the possibility of it unloading/loading
  // libraries and invalidating the addresses, but that is highly unlikely and the Cyclone
  // DLL itself (which we care about most) won't be unloaded.
  if (!self)
    (void) ResumeThread (handle);
  if (nstk == 0)
  {
    errmsg = "StackWalk64";
    goto fail;
  }

  // No longer need the thread handle, and we don't want to leak if we Open'd it
  if (close_required)
    (void) CloseHandle (handle);

  // Convert to module!function + offset
  DWORD64 disp;
  union {
    char buffer[sizeof (SYMBOL_INFO) + MAX_SYM_NAME * sizeof (TCHAR)];
    SYMBOL_INFO sym;
  } symbol;
  symbol.sym.SizeOfStruct = sizeof (SYMBOL_INFO);
  symbol.sym.MaxNameLen = MAX_SYM_NAME;
  for (size_t i = 0; i < nstk; i++)
  {
    const DWORD64 modulebase = SymGetModuleBase64 (process, stk[i]);
    char modulename[MAX_PATH];
    if (!(modulebase && GetModuleFileNameA ((HINSTANCE) modulebase, modulename, MAX_PATH)))
      strcpy_s (modulename, sizeof (modulename), "unknown");
    if (SymFromAddr (process, stk[i], &disp, &symbol.sym))
      DDS_CLOG (~DDS_LC_FATAL, logcfg, "%s@0x%"PRIx64"!0x%"PRIx64" %s+0x%"PRIx64"\n",
                modulename, (uint64_t) modulebase, (uint64_t) stk[i] - modulebase,
                symbol.sym.Name, (uint64_t) disp);
    else
      DDS_CLOG (~DDS_LC_FATAL, logcfg, "%s@0x%"PRIx64"!0x%"PRIx64"\n",
                modulename, (uint64_t) modulebase, (uint64_t) stk[i] - modulebase);
  }
  DDS_CLOG (~DDS_LC_FATAL, logcfg, "-- end of stack trace --\n");
  return;

 fail:
  if (close_required)
    (void) CloseHandle (handle);
  DDS_CLOG (~DDS_LC_FATAL, logcfg, "-- no stack trace: %s failure --\n", errmsg);
}
#else
void ddsi_log_stacktrace (const struct ddsrt_log_cfg *logcfg, const char *name, ddsrt_thread_t tid)
{
  DDSRT_UNUSED_ARG (logcfg);
  DDSRT_UNUSED_ARG (name);
  DDSRT_UNUSED_ARG (tid);
}
#endif
