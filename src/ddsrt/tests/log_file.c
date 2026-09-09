// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include "CUnit/Theory.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/filesystem.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/threads.h"

#if DDSRT_HAVE_FILESYSTEM && defined _WIN32
#include <direct.h>
#endif

static char filename[128];
static unsigned serial;

static void setup (void)
{
  ddsrt_init ();
  snprintf (filename, sizeof (filename), "ddsrt-log-file-%"PRIdPID"-%u.log", ddsrt_getpid (), serial++);
  dds_set_trace_sink (NULL, NULL);
}

static void teardown (void)
{
  remove (filename);
  ddsrt_fini ();
}

static void seed_file (void)
{
  FILE *fp = fopen (filename, "w");
  CU_ASSERT_NEQ_FATAL (fp, NULL);
  CU_ASSERT_GEQ (fputs ("previous run\n", fp), 0);
  CU_ASSERT_EQ_FATAL (fclose (fp), 0);
}

static void check_file (const char *expected)
{
  char buf[256];
  FILE *fp = fopen (filename, "r");
  CU_ASSERT_NEQ_FATAL (fp, NULL);
  const size_t n = fread (buf, 1, sizeof (buf) - 1, fp);
  buf[n] = 0;
  CU_ASSERT_EQ (ferror (fp), 0);
  CU_ASSERT_EQ (fclose (fp), 0);
  CU_ASSERT_STREQ (buf, expected);
}

CU_TheoryDataPoints (ddsrt_log_file, append) = {
  CU_DataPoints (bool, false, false, true, true),
  CU_DataPoints (bool, false, true, false, true)
};

CU_Theory ((bool first_append, bool second_append), ddsrt_log_file, append)
{
  setup ();
  seed_file ();
  FILE *first, *second;
  CU_ASSERT_EQ_FATAL (ddsrt_log_file_open (filename, first_append, &first), DDS_RETCODE_OK);
  CU_ASSERT_GEQ (fputs ("first\n", first), 0);
  CU_ASSERT_EQ (fflush (first), 0);

  char *name;
#if DDSRT_HAVE_FILESYSTEM
  CU_ASSERT_EQ_FATAL (ddsrt_file_abspath (filename, &name), DDS_RETCODE_OK);
#else
  name = ddsrt_malloc (strlen (filename) + 1);
  strcpy (name, filename);
#endif
  CU_ASSERT_EQ_FATAL (ddsrt_log_file_open (name, second_append, &second), DDS_RETCODE_OK);
  ddsrt_free (name);
  CU_ASSERT_EQ (first, second);
  CU_ASSERT_EQ (ddsrt_log_file_close (first), DDS_RETCODE_OK);
  CU_ASSERT_GEQ (fputs ("second\n", second), 0);
  CU_ASSERT_EQ (ddsrt_log_file_close (second), DDS_RETCODE_OK);

  /* Zero references must close the stream without forgetting prior use. */
  CU_ASSERT_EQ_FATAL (ddsrt_log_file_open (filename, false, &first), DDS_RETCODE_OK);
  CU_ASSERT_GEQ (fputs ("reopened\n", first), 0);
  CU_ASSERT_EQ (ddsrt_log_file_close (first), DDS_RETCODE_OK);
  check_file (first_append ? "previous run\nfirst\nsecond\nreopened\n" : "first\nsecond\nreopened\n");
  teardown ();
}

CU_Test (ddsrt_log_file, failed_open, .init = setup, .fini = teardown)
{
  FILE *fp = (FILE *) 1;
  CU_ASSERT_EQ (ddsrt_log_file_open ("", false, &fp), DDS_RETCODE_BAD_PARAMETER);
  CU_ASSERT_EQ (fp, NULL);
#if DDSRT_HAVE_FILESYSTEM
#ifdef _WIN32
  CU_ASSERT_EQ_FATAL (_mkdir (filename), 0);
#else
  CU_ASSERT_EQ_FATAL (mkdir (filename, 0700), 0);
#endif
  CU_ASSERT_EQ (ddsrt_log_file_open (filename, false, &fp), DDS_RETCODE_ERROR);
  CU_ASSERT_EQ (fp, NULL);
#ifdef _WIN32
  CU_ASSERT_EQ_FATAL (_rmdir (filename), 0);
#else
  CU_ASSERT_EQ_FATAL (rmdir (filename), 0);
#endif
  seed_file ();
  CU_ASSERT_EQ_FATAL (ddsrt_log_file_open (filename, false, &fp), DDS_RETCODE_OK);
  CU_ASSERT_EQ (ddsrt_log_file_close (fp), DDS_RETCODE_OK);
  check_file ("");
#endif
}

#define NWRITERS 8
#define NRECORDS 64
static ddsrt_mutex_t start_lock;
static ddsrt_cond_t start_cond;
static bool start_writers;

static uint32_t write_records (void *arg)
{
  const unsigned writer = *(unsigned *) arg;
  ddsrt_mutex_lock (&start_lock);
  while (!start_writers)
    ddsrt_cond_wait (&start_cond, &start_lock);
  ddsrt_mutex_unlock (&start_lock);

  FILE *fp;
  if (ddsrt_log_file_open (filename, false, &fp) != DDS_RETCODE_OK)
    return 1;
  struct ddsrt_log_cfg cfg;
  dds_log_cfg_init (&cfg, writer, DDS_LC_USER1, stderr, fp);
  for (unsigned i = 0; i < NRECORDS; i++)
    DDS_CLOG (DDS_LC_USER1, &cfg, "record:%u:%u\n", writer, i);
  return ddsrt_log_file_close (fp) == DDS_RETCODE_OK ? 0 : 1;
}

CU_Test (ddsrt_log_file, concurrent, .init = setup, .fini = teardown)
{
  seed_file ();
  ddsrt_mutex_init (&start_lock);
  ddsrt_cond_init (&start_cond);
  start_writers = false;
  ddsrt_thread_t threads[NWRITERS];
  unsigned ids[NWRITERS];
  ddsrt_threadattr_t attr;
  ddsrt_threadattr_init (&attr);
  for (unsigned i = 0; i < NWRITERS; i++)
  {
    ids[i] = i;
    CU_ASSERT_EQ_FATAL (ddsrt_thread_create (&threads[i], "log-writer", &attr, write_records, &ids[i]), DDS_RETCODE_OK);
  }
  ddsrt_mutex_lock (&start_lock);
  start_writers = true;
  ddsrt_cond_broadcast (&start_cond);
  ddsrt_mutex_unlock (&start_lock);
  for (unsigned i = 0; i < NWRITERS; i++)
  {
    uint32_t result;
    CU_ASSERT_EQ (ddsrt_thread_join (threads[i], &result), DDS_RETCODE_OK);
    CU_ASSERT_EQ (result, 0);
  }
  ddsrt_cond_destroy (&start_cond);
  ddsrt_mutex_destroy (&start_lock);

  bool seen[NWRITERS][NRECORDS] = {{ false }};
  unsigned count = 0;
  char buf[256];
  FILE *fp = fopen (filename, "r");
  CU_ASSERT_NEQ_FATAL (fp, NULL);
  while (fgets (buf, sizeof (buf), fp))
  {
    unsigned writer, record;
    const char *msg = strstr (buf, "record:");
    CU_ASSERT_NEQ_FATAL (msg, NULL);
    CU_ASSERT_EQ_FATAL (sscanf (msg, "record:%u:%u", &writer, &record), 2);
    CU_ASSERT_LT_FATAL (writer, NWRITERS);
    CU_ASSERT_LT_FATAL (record, NRECORDS);
    CU_ASSERT (!seen[writer][record]);
    seen[writer][record] = true;
    count++;
  }
  CU_ASSERT_EQ (ferror (fp), 0);
  CU_ASSERT_EQ (fclose (fp), 0);
  CU_ASSERT_EQ (count, NWRITERS * NRECORDS);
}
