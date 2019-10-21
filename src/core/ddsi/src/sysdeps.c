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

#include "cyclonedds/ddsrt/atomics.h"
#include "cyclonedds/ddsrt/misc.h"

#include "cyclonedds/ddsi/q_log.h"
#include "cyclonedds/ddsi/sysdeps.h"

#if DDSRT_WITH_FREERTOS || !(defined __APPLE__ || defined __linux) || (__GNUC__ > 0 && (__GNUC__ * 10000 + __GNUC_MINOR__ * 100 + __GNUC_PATCHLEVEL__) < 40100)
void log_stacktrace (const struct ddsrt_log_cfg *logcfg, const char *name, ddsrt_thread_t tid)
{
  DDSRT_UNUSED_ARG (name);
  DDSRT_UNUSED_ARG (tid);
}
#else
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

void log_stacktrace (const struct ddsrt_log_cfg *logcfg, const char *name, ddsrt_thread_t tid)
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
#endif

