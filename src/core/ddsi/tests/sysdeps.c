/*
 * Copyright(c) 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include "dds/ddsrt/log.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsi/sysdeps.h"
#include "CUnit/Theory.h"

enum loggerstate {
  STL_INIT, STL_FOLLOWS, STL_FRAMES, STL_TEST_PASSED, STL_TEST_FAILED
};
static const char *loggerstatestr[] = {
  "INIT", "FOLLOWS", "FRAMES", "PASSED", "FAILED"
};

static ddsrt_log_cfg_t logconfig;
static enum loggerstate loggerstate;
static unsigned framecount;

static bool ishexdigit (unsigned char c)
{
  return (isdigit (c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'));
}

static bool looks_like_stackframe (const char *msg)
{
  // we're already happy if there are some letters and digits
  enum state { WS_OR_PUNCT, WS_ZERO, WS_WORD, WS_HEX, WS_WORD_OR_HEX, WS_MESSY } state = WS_OR_PUNCT;
  unsigned char c;
  int nnum = 0, nword = 0;
  printf ("%-8s", "");
  while ((c = (unsigned char) *msg++) != 0)
  {
    printf ("%d", (int) state);
    switch (state)
    {
      case WS_OR_PUNCT:
        if (c == '0') state = WS_ZERO;
        else if (isdigit (c)) state = WS_HEX;
        else if (ishexdigit (c)) state = WS_WORD_OR_HEX;
        else if (isalpha (c)) state = WS_WORD;
        break;
      case WS_ZERO:
        if (c == 'x' || c == 'X') state = WS_HEX;
        else if (isalpha (c)) state = WS_MESSY;
        else if (!isdigit (c)) { nnum++; state = WS_OR_PUNCT; }
        else state = WS_HEX; // hex is ok, also without 0x prefix
        break;
      case WS_HEX:
        if (ishexdigit (c)) { }
        else if (isalpha (c)) state = WS_MESSY;
        else { nnum++; state = WS_OR_PUNCT; }
        break;
      case WS_WORD_OR_HEX:
        if (ishexdigit (c)) { }
        else if (isalpha (c)) state = WS_WORD;
        else { nnum++; state = WS_OR_PUNCT; }
        break;
      case WS_WORD:
        if (!isalnum (c)) { nword++; state = WS_OR_PUNCT; }
        break;
      case WS_MESSY:
        break;
    }
  }
  printf("\n");
  // want at least a name and an offset or just an address in a not-too-messy string
  return (state != WS_MESSY) && ((nword + nnum) >= 2 || (nword == 0 && nnum == 1));
}

static void logger (void *ptr, const dds_log_data_t *data)
{
  (void) ptr;
  printf ("%-7s %s", loggerstatestr[loggerstate], data->message);
  switch (loggerstate)
  {
    case STL_INIT:
      if (strstr (data->message, "stack trace") && strstr (data->message, "requested"))
        loggerstate = STL_FOLLOWS;
      break;
    case STL_FOLLOWS:
      if (strstr (data->message, "stack trace follows"))
      {
        framecount = 0;
        loggerstate = STL_FRAMES;
      }
      /* fall through */
    case STL_FRAMES:
      if (strstr (data->message, "end of stack trace"))
      {
        loggerstate = (framecount > 2) ? STL_TEST_PASSED : STL_TEST_FAILED;
      }
      else if (looks_like_stackframe (data->message))
      {
        framecount++;
      }
      break;
    case STL_TEST_PASSED:
    case STL_TEST_FAILED:
      break;
  }
}

static void setup (void)
{
  ddsrt_init ();
  dds_set_log_mask (DDS_LC_ERROR);
  dds_set_log_sink (&logger, 0);
  dds_set_trace_sink (&logger, 0);
  dds_log_cfg_init (&logconfig, 0, DDS_LC_TRACE, 0, 0);
  loggerstate = STL_INIT;
  framecount = 0;
}

static void teardown (void)
{
  dds_set_log_sink (0, 0);
  dds_set_trace_sink (0, 0);
  ddsrt_fini ();
}

CU_Test (ddsi_sysdeps, log_stacktrace_self, .init = setup, .fini = teardown)
{
  log_stacktrace (&logconfig, "self", ddsrt_thread_self ());
  CU_ASSERT_FATAL (loggerstate == STL_TEST_PASSED || loggerstate == STL_INIT);
}

struct log_stacktrace_thread_arg {
  int stop;
  ddsrt_cond_t cond;
  ddsrt_mutex_t lock;
};

static uint32_t log_stacktrace_thread (void *varg)
{
  struct log_stacktrace_thread_arg * const arg = varg;
  ddsrt_mutex_lock (&arg->lock);
  while (!arg->stop)
    ddsrt_cond_wait (&arg->cond, &arg->lock);
  ddsrt_mutex_unlock (&arg->lock);
  return 0;
}

CU_Test (ddsi_sysdeps, log_stacktrace_other, .init = setup, .fini = teardown)
{
  struct log_stacktrace_thread_arg arg;

  arg.stop = 0;
  ddsrt_mutex_init (&arg.lock);
  ddsrt_cond_init (&arg.cond);

  ddsrt_threadattr_t tattr;
  ddsrt_thread_t tid;
  dds_return_t rc;
  ddsrt_threadattr_init (&tattr);
  rc = ddsrt_thread_create (&tid, "log_stacktrace_thread", &tattr, log_stacktrace_thread, &arg);
  CU_ASSERT_FATAL (rc == 0);

  log_stacktrace (&logconfig, "other", tid);

  ddsrt_mutex_lock (&arg.lock);
  arg.stop = 1;
  ddsrt_cond_signal (&arg.cond);
  ddsrt_mutex_unlock (&arg.lock);
  rc = ddsrt_thread_join (tid, NULL);
  CU_ASSERT_FATAL (rc == 0);
  CU_ASSERT_FATAL (loggerstate == STL_TEST_PASSED || loggerstate == STL_INIT);
}
