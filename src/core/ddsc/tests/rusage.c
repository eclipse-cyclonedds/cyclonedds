// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/dds.h"
#include "dds/ddsrt/rusage.h"

#include "test_common.h"

#if DDSRT_HAVE_RUSAGE
static void set_usage_sentinel (ddsrt_rusage_t *usage)
{
  usage->utime = (dds_time_t) -1;
  usage->stime = (dds_time_t) -1;
  usage->maxrss = SIZE_MAX;
  usage->idrss = SIZE_MAX;
  usage->nvcsw = SIZE_MAX;
  usage->nivcsw = SIZE_MAX;
}

static void assert_usage_written (const ddsrt_rusage_t *usage)
{
  CU_ASSERT_NEQ (usage->utime, (dds_time_t) -1);
  CU_ASSERT_NEQ (usage->stime, (dds_time_t) -1);
  CU_ASSERT_NEQ (usage->maxrss, SIZE_MAX);
  CU_ASSERT_NEQ (usage->idrss, SIZE_MAX);
  CU_ASSERT_NEQ (usage->nvcsw, SIZE_MAX);
  CU_ASSERT_NEQ (usage->nivcsw, SIZE_MAX);
}
#endif

CU_Test(ddsc_rusage, get)
{
#if DDSRT_HAVE_RUSAGE
  ddsrt_rusage_t usage;

  set_usage_sentinel (&usage);
  CU_ASSERT_EQ_FATAL (ddsrt_getrusage (DDSRT_RUSAGE_SELF, &usage), DDS_RETCODE_OK);
  assert_usage_written (&usage);

  set_usage_sentinel (&usage);
  CU_ASSERT_EQ_FATAL (ddsrt_getrusage (DDSRT_RUSAGE_THREAD, &usage), DDS_RETCODE_OK);
  assert_usage_written (&usage);

#if DDSRT_HAVE_THREAD_LIST
  dds_entity_t participant;
  ddsrt_thread_list_id_t tids[100];
  dds_return_t nthreads;

  participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (participant, 0);

  nthreads = ddsrt_thread_list (tids, sizeof (tids) / sizeof (tids[0]));
  CU_ASSERT_GT_FATAL (nthreads, 0);
  CU_ASSERT_FATAL ((size_t) nthreads <= sizeof (tids) / sizeof (tids[0]));

  for (dds_return_t i = 0; i < nthreads; i++)
  {
    set_usage_sentinel (&usage);
    CU_ASSERT_EQ (ddsrt_getrusage_anythread (tids[i], &usage), DDS_RETCODE_OK);
    assert_usage_written (&usage);
  }

  CU_ASSERT_EQ (dds_delete (participant), DDS_RETCODE_OK);
#else
  CU_PASS ("thread listing is not supported");
#endif
#else
  CU_PASS ("rusage is not supported");
#endif
}
