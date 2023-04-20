// Copyright(c) 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <limits.h>

#include "dds/dds.h"
#include "CUnit/Theory.h"
#include "RoundTrip.h"

#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/time.h"


#define N_THREADS (10)

static const dds_duration_t TEST_DURATION = DDS_SECS(3);

static ddsrt_atomic_uint32_t terminate;


static uint32_t create_participants_thread (void *varg)
{
  (void) varg;
  while (!ddsrt_atomic_ld32 (&terminate))
  {
    dds_entity_t par = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
    if (par < 0)
    {
      fprintf (stderr, "dds_create_participant failed: %s\n", dds_strretcode (par));
      ddsrt_atomic_st32 (&terminate, 1);
      return 1;
    }

    dds_return_t ret = dds_delete(par);
    if (ret != DDS_RETCODE_OK)
    {
      fprintf (stderr, "dds_delete failed: %s\n", dds_strretcode (ret));
      ddsrt_atomic_st32 (&terminate, 1);
      return 1;
    }
  }
  return 0;
}


static void participant_creation_torture(void)
{
  dds_return_t rc;
  ddsrt_thread_t tids[N_THREADS];
  ddsrt_threadattr_t tattr;
  ddsrt_threadattr_init (&tattr);

  /* Start threads. */
  for (size_t i = 0; i < sizeof(tids) / sizeof(*tids); i++)
  {
    rc = ddsrt_thread_create (&tids[i], "domain_torture_explicit", &tattr, create_participants_thread, 0);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  }

  /* Let the threads do the torturing for a while. */
  dds_sleepfor(TEST_DURATION);

  /* Stop and check threads results. */
  ddsrt_atomic_st32 (&terminate, 1);
  for (size_t i = 0; i < sizeof (tids) / sizeof (tids[0]); i++)
  {
    uint32_t retval;
    rc = ddsrt_thread_join (tids[i], &retval);
    CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
    CU_ASSERT (retval == 0);
  }
}


/*
 * There are some issues when completely init/deinit the
 * library in a torturing way. We really just want to
 * check the domain creation/deletion. So, disable this
 * test for now.
 */
CU_Test (ddsc_domain, torture_implicit, .disabled=true)
{
  /* No explicit domain creation, just start creating and
   * deleting participants (that'll create and delete the
   * domain implicitly) in a torturing manner. */
  participant_creation_torture();
}


CU_Test (ddsc_domain, torture_explicit)
{
  dds_return_t rc;
  dds_entity_t domain;

  /* Create domain explicitly. */
  domain = dds_create_domain(1, "");
  CU_ASSERT_FATAL (domain > 0);

  /* Start creating and deleting participants on the
   * explicit domain in a torturing manner. */
  participant_creation_torture();

  /* Delete domain. */
  rc = dds_delete(domain);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  rc = dds_delete(domain);
  CU_ASSERT_FATAL (rc != DDS_RETCODE_OK);
}
