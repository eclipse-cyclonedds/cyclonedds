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
#include <stdlib.h>

#include "os/os.h"

#include "ddsi/q_error.h"
#include "ddsi/q_log.h"
#include "ddsi/q_config.h"
#include "ddsi/sysdeps.h"

#if !(defined __APPLE__ || defined __linux) || (__GNUC__ > 0 && (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) < 40100)
void log_stacktrace (const char *name, os_threadId tid)
{
  OS_UNUSED_ARG (name);
  OS_UNUSED_ARG (tid);
}
#else
#include <execinfo.h>
#include <signal.h>

static os_atomic_uint32_t log_stacktrace_flag = OS_ATOMIC_UINT32_INIT(0);
static struct {
  int depth;
  void *stk[64];
} log_stacktrace_stk;


static void log_stacktrace_sigh (int sig __attribute__ ((unused)))
{
  int e = os_getErrno();
  log_stacktrace_stk.depth = backtrace (log_stacktrace_stk.stk, (int) (sizeof (log_stacktrace_stk.stk) / sizeof (*log_stacktrace_stk.stk)));
  os_atomic_inc32 (&log_stacktrace_flag);
  os_setErrno(e);
}

void log_stacktrace (const char *name, os_threadId tid)
{
  if (dds_get_log_mask() == 0)
    ; /* no op if nothing logged */
  else if (!config.noprogress_log_stacktraces)
    DDS_LOG(~0u, "-- stack trace of %s requested, but traces disabled --\n", name);
  else
  {
    const os_time d = { 0, 1000000 };
    struct sigaction act, oact;
    char **strs;
    int i;
    DDS_LOG(~0u, "-- stack trace of %s requested --\n", name);
    act.sa_handler = log_stacktrace_sigh;
    act.sa_flags = 0;
    sigfillset (&act.sa_mask);
    while (!os_atomic_cas32 (&log_stacktrace_flag, 0, 1))
      os_nanoSleep (d);
    sigaction (SIGXCPU, &act, &oact);
    pthread_kill (tid.v, SIGXCPU);
    while (!os_atomic_cas32 (&log_stacktrace_flag, 2, 3) && pthread_kill (tid.v, 0) == 0)
      os_nanoSleep (d);
    sigaction (SIGXCPU, &oact, NULL);
    if (pthread_kill (tid.v, 0) != 0)
      DDS_LOG(~0u, "-- thread exited --\n");
    else
    {
      DDS_LOG(~0u, "-- stack trace follows --\n");
      strs = backtrace_symbols (log_stacktrace_stk.stk, log_stacktrace_stk.depth);
      for (i = 0; i < log_stacktrace_stk.depth; i++)
        DDS_LOG(~0u, "%s\n", strs[i]);
      free (strs);
      DDS_LOG(~0u, "-- end of stack trace --\n");
    }
    os_atomic_st32 (&log_stacktrace_flag, 0);
  }
}
#endif

