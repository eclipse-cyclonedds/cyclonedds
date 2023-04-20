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
#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef __APPLE__
#include <pthread.h>
#include <AvailabilityMacros.h>
#endif /* __APPLE__ */

#include "CUnit/Test.h"
#include "CUnit/Theory.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/time.h"

/* On macOS, fmemopen was introduced in version 10.13.  The hassle of providing
   an alternative implementation of it just for running a few sanity checks on an
   old version of macOS isn't worth the bother.

   The CUnit.cmake boiler-plate generator doesn't recognize #ifdef'ing tests away
   because it runs on the source rather than on the output of the C preprocessor
   (a reasonable decision in itself).  Therefore, just skip the body of each test. */

#if __APPLE__ && !(defined MAC_OS_X_VERSION_10_13 && MAC_OS_X_VERSION_MIN_REQUIRED >= MAC_OS_X_VERSION_10_13)
#define HAVE_FMEMOPEN 0
#else
#define HAVE_FMEMOPEN 1
#endif

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>

/* Windows does not support opening a stream to a buffer like fmemopen on
 * Linux does. A temporary file that will never be flushed to disk is created
 * instead. See the link below for more detail.
 *
 * https://blogs.msdn.microsoft.com/larryosterman/2004/04/19/its-only-temporary/
 */

static FILE *fmemopen(void *buf, size_t size, const char *mode)
{
  DWORD err = 0;
  int fd = -1;
  DWORD ret;
  FILE *fh = NULL;
  HANDLE hdl = INVALID_HANDLE_VALUE;
  /* GetTempFileName will fail if the directory is be longer than MAX_PATH-14
     characters */
  char tmpdir[(MAX_PATH + 1) - 14];
  char tmpfile[MAX_PATH + 1];
  static const char pfx[] = "cyclone"; /* Up to first three are used. */

  (void)buf;
  (void)size;

  ret = GetTempPath(sizeof(tmpdir), tmpdir);
  if (ret == 0) {
    err = GetLastError();
  } else if (ret > sizeof(tmpdir)) {
    err = ENOMEM;
  }

  if (GetTempFileName(tmpdir, pfx, 0, tmpfile) == 0) {
    err = GetLastError();
    assert(err != ERROR_BUFFER_OVERFLOW);
  } else {
     /* The combination of FILE_ATTRIBUTE_TEMPORARY and
        FILE_FLAG_DELETE_ON_CLOSE hints to the filesystem that the file should
        never be flushed to disk. */
    hdl = CreateFile(
      tmpfile,
      GENERIC_READ | GENERIC_WRITE,
      0,
      NULL,
      CREATE_ALWAYS,
      FILE_FLAG_DELETE_ON_CLOSE | FILE_ATTRIBUTE_TEMPORARY,
      NULL);
    if (hdl == INVALID_HANDLE_VALUE) {
      err = GetLastError();
    }
  }

  if (err) {
    errno = (int)err;
  } else {
    DDSRT_WARNING_MSVC_OFF(4996);
    if ((fd = _open_osfhandle((intptr_t)hdl, _O_APPEND)) == -1) {
      /* errno set by _open_osfhandle. */
      CloseHandle(hdl);
    } else if ((fh = fdopen(fd, mode)) == NULL) {
      /* errno set by fdopen. */
      _close(fd); /* Automatically closes underlying handle. */
    } else {
      return fh;
    }
    DDSRT_WARNING_MSVC_ON(4996);
  }

  return NULL;
}
#endif /* _WIN32 */

#if HAVE_FMEMOPEN
static FILE *fh = NULL;

static void count(void *ptr, const dds_log_data_t *data)
{
  (void)data;
  *(int *)ptr += 1;
}

static void copy(void *ptr, const dds_log_data_t *data)
{
  *(char **)ptr = ddsrt_strdup(data->message);
}
#endif

static void reset(void)
{
  /* Reset log internals to default. */
  dds_set_log_mask(DDS_LC_ERROR | DDS_LC_WARNING);
  dds_set_trace_sink(NULL, NULL);
  dds_set_log_sink(NULL, NULL);
}

static void setup(void)
{
#if HAVE_FMEMOPEN
  fh = fmemopen(NULL, 1024, "wb+");
  CU_ASSERT_PTR_NOT_NULL_FATAL(fh);
#endif
}

static void teardown(void)
{
  reset();
#if HAVE_FMEMOPEN
  (void)fclose(fh);
#endif
}

/* By default only DDS_LC_FATAL and DDS_LC_ERROR are set. This means setting a
   trace sink should not have any effect, because no trace categories are
   enabled. The message should end up in the log file. */
CU_Test(dds_log, only_log_file, .init=setup, .fini=teardown)
{
#if HAVE_FMEMOPEN
  char buf[1024], *ptr;
  int cnt = 0;
  size_t nbytes;

  dds_set_log_file(fh);
  dds_set_trace_sink(&count, &cnt);
  DDS_ERROR("foo%s\n", "bar");
  (void)fseek(fh, 0L, SEEK_SET);
  nbytes = fread(buf, 1, sizeof(buf) - 1, fh);
  /* At least foobar should have been printed to the log file. */
  CU_ASSERT_FATAL(nbytes > 6);
  buf[nbytes] = '\0';
  ptr = strstr(buf, "foobar\n");
  CU_ASSERT_PTR_NOT_NULL(ptr);
  /* No trace categories are enabled by default, verify trace callback was
     not invoked. */
  CU_ASSERT_EQUAL(cnt, 0);
#endif
}

/* Messages must be printed to the trace file if at least one trace category
   is enabled. Messages must not be written twice if the trace file is the
   same as the log file. */
CU_Test(dds_log, same_file, .init=setup, .fini=teardown)
{
#if HAVE_FMEMOPEN
  char buf[1024], *ptr;
  size_t nbytes;

  dds_set_log_mask(DDS_LC_ALL);
  dds_set_log_file(fh);
  dds_set_trace_file(fh);
  DDS_ERROR("foo%s\n", "bar");
  (void)fseek(fh, 0L, SEEK_SET);
  nbytes = fread(buf, 1, sizeof(buf) - 1, fh);
  /* At least foobar should have been written to the trace file. */
  CU_ASSERT_FATAL(nbytes > 6);
  buf[nbytes] = '\0';
  ptr = strstr(buf, "foobar\n");
  CU_ASSERT_PTR_NOT_NULL_FATAL(ptr);
  /* The message should only have been printed once, verify foobar does not
     occur again. */
  assert(ptr);
  ptr = strstr(ptr + 1, "foobar\n");
  CU_ASSERT_PTR_NULL(ptr);
#endif
}

/* The sinks are considered to be the same only if the callback and userdata
   both are an exact match. If the userdata is different, the function should
   be called twice for log messages. */
CU_Test(dds_log, same_sink_function, .fini=reset)
{
#if HAVE_FMEMOPEN
  int log_cnt = 0, trace_cnt = 0;

  dds_set_log_mask(DDS_LC_ALL);
  dds_set_log_sink(&count, &log_cnt);
  dds_set_trace_sink(&count, &trace_cnt);
  DDS_ERROR("foo%s\n", "bar");
  CU_ASSERT_EQUAL(log_cnt, 1);
  CU_ASSERT_EQUAL(trace_cnt, 1);
#endif
}

CU_Test(dds_log, exact_same_sink, .fini=reset)
{
#if HAVE_FMEMOPEN
  int cnt = 0;

  dds_set_log_mask(DDS_LC_ALL);
  dds_set_log_sink(&count, &cnt);
  dds_set_trace_sink(&count, &cnt);
  DDS_ERROR("foo%s\n", "bar");
  CU_ASSERT_EQUAL(cnt, 1);
#endif
}

/* The log file must be restored if the sink is unregistered, verify the log
   file is not used while the sink is registered. Verify use of the log file is
   restored again when the sink is unregistered. */
CU_Test(dds_log, no_sink, .init=setup, .fini=teardown)
{
#if HAVE_FMEMOPEN
  int ret;
  char buf[1024], *ptr = NULL;
  size_t cnt[2] = {0, 0};

  /* Set the destination log file and verify the message is written. */
  dds_set_log_file(fh);
  DDS_ERROR("foobar\n");
  ret = fseek(fh, 0L, SEEK_SET);
  CU_ASSERT_EQUAL_FATAL(ret, 0);
  buf[0] = '\0';
  cnt[0] = fread(buf, 1, sizeof(buf) - 1, fh);
  buf[cnt[0]] = '\0';
  ptr = strstr(buf, "foobar\n");
  CU_ASSERT_PTR_NOT_NULL_FATAL(ptr);

  /* Register a custom sink and verify it receives the message. */
  ptr = NULL;
  dds_set_log_sink(&copy, &ptr);
  DDS_ERROR("foobaz\n");
  CU_ASSERT_PTR_NOT_NULL_FATAL(ptr);
  CU_ASSERT(strcmp(ptr, "foobaz\n") == 0);
  ddsrt_free(ptr);
  ptr = NULL;
  /* Verify it has not been written to the stream. */
  ret = fseek(fh, 0L, SEEK_SET);
  CU_ASSERT_EQUAL_FATAL(ret, 0);
  buf[0] = '\0';
  cnt[1] = fread(buf, 1, sizeof(buf) - 1, fh);
  buf[cnt[1]] = '\0';
  ptr = strstr(buf, "foobaz\n");
  CU_ASSERT_PTR_NULL_FATAL(ptr);

  /* Unregister the custom sink and verify the default is restored. */
  dds_set_log_sink(0, NULL);
  ret = fseek(fh, 0, SEEK_SET);
  CU_ASSERT_EQUAL_FATAL(ret, 0);
  ptr = NULL;
  DDS_ERROR("foobaz\n");
  ret = fseek(fh, 0, SEEK_SET);
  CU_ASSERT_EQUAL_FATAL(ret, 0);
  CU_ASSERT_PTR_NULL(ptr);
  buf[0]= '\0';
  cnt[1] = fread(buf, 1, sizeof(buf) - 1, fh);
#ifdef _WIN32
  /* Write on Windows appends. */
  CU_ASSERT_EQUAL(cnt[1], cnt[0] * 2);
#else
  CU_ASSERT_EQUAL(cnt[1], cnt[0]);
#endif
  buf[cnt[1]] = '\0';
  ptr = strstr(buf, "foobaz\n");
  CU_ASSERT_PTR_NOT_NULL_FATAL(ptr);
#endif
}

/* A newline terminates the message. Until that a newline is encountered, the
   messages must be concatenated in the buffer. The newline is replaced by a
   NULL byte if it is flushed to a sink. */
CU_Test(dds_log, newline_terminates, .fini=reset)
{
#if HAVE_FMEMOPEN
  char *msg = NULL;

  dds_set_log_sink(&copy, &msg);
  DDS_ERROR("foo");
  CU_ASSERT_PTR_NULL_FATAL(msg);
  DDS_ERROR("bar");
  CU_ASSERT_PTR_NULL_FATAL(msg);
  DDS_ERROR("baz\n");
  CU_ASSERT_PTR_NOT_NULL_FATAL(msg);
  CU_ASSERT(strcmp(msg, "foobarbaz\n") == 0);
  ddsrt_free(msg);
#endif
}

/* Nothing must be written unless a category is enabled. */
CU_Test(dds_log, disabled_categories_discarded, .fini=reset)
{
#if HAVE_FMEMOPEN
  char *msg = NULL;
  dds_set_log_sink(&copy, &msg);
  DDS_INFO("foobar\n");
  CU_ASSERT_PTR_NULL_FATAL(msg);
  dds_set_log_mask(DDS_LC_FATAL | DDS_LC_ERROR | DDS_LC_INFO);
  DDS_INFO("foobar\n");
  CU_ASSERT_PTR_NOT_NULL_FATAL(msg);
  CU_ASSERT(strcmp(msg, "foobar\n") == 0);
  ddsrt_free(msg);
#endif
}

#if HAVE_FMEMOPEN
static ddsrt_cond_t cond;
static ddsrt_mutex_t mutex;

struct arg {
  ddsrt_cond_t *cond;
  ddsrt_mutex_t *mutex;
  dds_time_t before;
  dds_time_t after;
};

static void dummy(void *ptr, const dds_log_data_t *data)
{
  (void)ptr;
  (void)data;
}

static void block(void *ptr, const dds_log_data_t *data)
{
  (void)data;
  struct arg *arg = (struct arg *)ptr;
  ddsrt_mutex_lock(arg->mutex);
  arg->before = dds_time();
  ddsrt_cond_broadcast(arg->cond);
  ddsrt_mutex_unlock(arg->mutex);
  arg->after = dds_time();
}

static uint32_t run(void *ptr)
{
  (void)ptr;

  DDS_ERROR("foobar\n");

  return 0;
}
#endif

/* Log and trace sinks can be changed at runtime. However, the operation must
   be synchronous! Verify the dds_set_log_sink blocks while other threads
   reside in the log or trace sinks. */
CU_Test(dds_log, synchronous_sink_changes, .fini=reset)
{
#if HAVE_FMEMOPEN
  struct arg arg;
  ddsrt_thread_t tid;
  ddsrt_threadattr_t tattr;
  dds_return_t ret;

  ddsrt_mutex_init(&mutex);
  ddsrt_cond_init(&cond);
  (void)memset(&arg, 0, sizeof(arg));
  arg.mutex = &mutex;
  arg.cond = &cond;

  ddsrt_mutex_lock(&mutex);
  dds_set_log_sink(&block, &arg);
  ddsrt_threadattr_init(&tattr);
  ret = ddsrt_thread_create(&tid, "foobar", &tattr, &run, &arg);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);
  ddsrt_cond_wait(&cond, &mutex);
  dds_set_log_sink(dummy, NULL);

  CU_ASSERT(arg.before < arg.after);
  CU_ASSERT(arg.after < dds_time());
#endif
}

/* Sanity checks that FATAL calls abort() -- this is very much platform
   dependent code, so we only do it on Linux and macOS, assuming that
   the logging implementation doesn't make any distinction between different
   platforms and that abort() is correctly implemented by the C library.

   macOS: abort causes abnormal termination unless the handler doesn't return,
   hence the setjmp/longjmp. */
#if defined __linux || defined __APPLE__
#define TEST_DDS_LC_FATAL 1
#else
#define TEST_DDS_LC_FATAL 0
#endif

#if TEST_DDS_LC_FATAL
#include <signal.h>

static sigjmp_buf abort_jmpbuf;
static char abort_message[100];
static char abort_message_trace[100];
static ddsrt_log_cfg_t abort_logconfig;

static void abort_handler (int sig)
{
  (void) sig;
  siglongjmp (abort_jmpbuf, 1);
}

static void abort_log (void *arg, const dds_log_data_t *info)
{
  (void) arg;
  (void) ddsrt_strlcpy (abort_message, info->message, sizeof (abort_message));
}

static void abort_trace (void *arg, const dds_log_data_t *info)
{
  (void) arg;
  (void) ddsrt_strlcpy (abort_message_trace, info->message, sizeof (abort_message_trace));
}

CU_TheoryDataPoints(dds_log, fatal_aborts) = {
  CU_DataPoints(bool, false, false, false,  true,  true,  true), /* global/config */
  CU_DataPoints(int,      0,     1,     2,     0,     1,     2), /* mask init mode */
  CU_DataPoints(bool, false, false,  true, false, false,  true)  /* expect in trace? */
};
#else
CU_TheoryDataPoints(dds_log, fatal_aborts) = {
  CU_DataPoints(bool, false), /* global/config */
  CU_DataPoints(int,      0), /* mask init mode */
  CU_DataPoints(bool, false)  /* expect in trace? */
};
#endif

CU_Theory((bool local, int mode, bool expect_in_trace), dds_log, fatal_aborts)
{
#if TEST_DDS_LC_FATAL
  struct sigaction action, oldaction;
  action.sa_flags = 0;
  sigemptyset (&action.sa_mask);
  action.sa_handler = abort_handler;

  if (sigsetjmp (abort_jmpbuf, 0) != 0)
  {
    sigaction (SIGABRT, &oldaction, NULL);
    CU_ASSERT_STRING_EQUAL (abort_message, "oops\n");
    CU_ASSERT_STRING_EQUAL (abort_message_trace, expect_in_trace ? "oops\n" : "");
  }
  else
  {
    memset (abort_message, 0, sizeof (abort_message));
    memset (abort_message_trace, 0, sizeof (abort_message_trace));
    dds_set_log_sink (abort_log, NULL);
    dds_set_trace_sink (abort_trace, NULL);
    sigaction (SIGABRT, &action, &oldaction);
    if (local)
    {
      switch (mode)
      {
        case 0:
          /* FALL THROUGH */
        case 1: dds_log_cfg_init (&abort_logconfig, 0, 0, 0, 0); break;
        case 2: dds_log_cfg_init (&abort_logconfig, 0, DDS_LC_TRACE, 0, 0); break;
      }
      DDS_CLOG (DDS_LC_FATAL, &abort_logconfig, "oops\n");
    }
    else
    {
      switch (mode)
      {
        case 0: break;
        case 1: dds_set_log_mask (0); break;
        case 2: dds_set_log_mask (DDS_LC_TRACE); break;
      }
      DDS_FATAL ("oops\n");
    }
    sigaction (SIGABRT, &oldaction, NULL);
    CU_ASSERT (0);
  }
#else
  (void) local;
  (void) mode;
  (void) expect_in_trace;
#endif
}
