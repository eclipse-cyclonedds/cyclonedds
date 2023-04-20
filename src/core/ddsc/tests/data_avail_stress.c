// Copyright(c) 2020 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>

#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/random.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/environ.h"

#include "dds/dds.h"
#include "test_common.h"

struct writethread_arg {
  dds_entity_t wr;
  ddsrt_atomic_uint32_t stop;
};

static uint32_t writethread (void *varg)
{
  struct writethread_arg * const arg = varg;
  Space_Type1 data = { 0, 0, 0 };
  dds_return_t ret = 0;
  while (!ddsrt_atomic_ld32 (&arg->stop) && ret == 0)
  {
    data.long_3++;
    ret = dds_write (arg->wr, &data);
  }
  ddsrt_atomic_or32 (&arg->stop, (ret != 0) ? 2 : 0);
  printf ("nwrites: %d\n", (int) data.long_3);
  return 0;
}

struct listener_status {
  ddsrt_atomic_uint32_t triggered;
  ddsrt_atomic_uint32_t taken;
  ddsrt_atomic_uint32_t badparam;
  ddsrt_atomic_uint32_t error;
};

struct listener_arg {
  uint32_t mask;
  struct listener_status *status;
};

static void data_avail (dds_entity_t rd, void *varg)
{
  struct listener_arg * const __restrict arg = varg;
  dds_return_t rc;
  Space_Type1 sample;
  void *sampleptr = &sample;
  dds_sample_info_t si;
  ddsrt_atomic_or32 (&arg->status->triggered, arg->mask);
  rc = dds_take (rd, &sampleptr, &si, 1, 1);
  if (rc < 0)
  {
    // there's a race condition during reader creation and destruction
    // where the handle is inaccessible but the listener can trigger,
    // so treat "bad parameter" as an okay-ish case
    if (rc == DDS_RETCODE_BAD_PARAMETER)
      ddsrt_atomic_inc32 (&arg->status->badparam);
    else
    {
      printf ("data_avail: take failed rc %d\n", (int) rc);
      ddsrt_atomic_inc32 (&arg->status->error);
    }
  }
  ddsrt_atomic_add32 (&arg->status->taken, (rc > 0 ? (uint32_t) rc : 0));
}

static dds_entity_t pub_dom, sub_dom;
static dds_entity_t pub_pp, sub_pp;
static dds_entity_t pub_tp, sub_tp;
static dds_entity_t wr;
static ddsrt_thread_t wrtid;

static void setup (bool remote, struct writethread_arg *wrarg)
{
  // Creating/deleting readers while writing becomes interesting
  // only once the network is lossy.
  const char *config = "${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}\
    <Discovery>\
      <ExternalDomainId>0</ExternalDomainId>\
    </Discovery>\
    <Internal>\
      <Test>\
        <XmitLossiness>100</XmitLossiness>\
      </Test>\
    </Internal>";
  char *conf_pub = ddsrt_expand_envvars (config, 0);
  char *conf_sub = ddsrt_expand_envvars (config, 1);
  pub_dom = dds_create_domain (0, conf_pub);
  CU_ASSERT_FATAL (pub_dom > 0);
  sub_dom = remote ? dds_create_domain (1, conf_sub) : 0;
  CU_ASSERT_FATAL (!remote || sub_dom > 0);
  ddsrt_free (conf_pub);
  ddsrt_free (conf_sub);

  pub_pp = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (pub_pp > 0);
  sub_pp = dds_create_participant (remote ? 1 : 0, NULL, NULL);
  CU_ASSERT_FATAL (sub_pp > 0);

  char tpname[100];
  create_unique_topic_name ("ddsc_data_avail_stress_delete_reader", tpname, sizeof (tpname));

  dds_qos_t * const qos = dds_create_qos ();
  CU_ASSERT_FATAL (qos != NULL);
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_SECS (1));
  dds_qset_writer_data_lifecycle (qos, false);
  pub_tp = dds_create_topic (pub_pp, &Space_Type1_desc, tpname, qos, NULL);
  CU_ASSERT_FATAL (pub_tp > 0);
  sub_tp = dds_create_topic (sub_pp, &Space_Type1_desc, tpname, qos, NULL);
  CU_ASSERT_FATAL (sub_tp > 0);
  dds_delete_qos (qos);

  wr = dds_create_writer (pub_pp, pub_tp, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);

  wrarg->wr = wr;
  ddsrt_atomic_st32 (&wrarg->stop, 0);
  ddsrt_threadattr_t tattr;
  ddsrt_threadattr_init (&tattr);
  dds_return_t rc = ddsrt_thread_create (&wrtid, "writer", &tattr, writethread, wrarg);
  CU_ASSERT_FATAL (rc == 0);
}

static void stress_data_avail_delete_reader (bool remote, int duration)
{
#define NRDS 10
  dds_return_t rc;
  struct writethread_arg wrarg;
  setup (remote, &wrarg);

  struct listener_status lstatus = {
    .triggered = DDSRT_ATOMIC_UINT32_INIT (0),
    .taken = DDSRT_ATOMIC_UINT32_INIT (0),
    .badparam = DDSRT_ATOMIC_UINT32_INIT (0),
    .error = DDSRT_ATOMIC_UINT32_INIT (0)
  };
  struct listener_arg larg[NRDS];
  dds_listener_t *list[NRDS];
  for (uint32_t i = 0; i < NRDS; i++)
  {
    larg[i] = (struct listener_arg) { .mask = 1u << i, .status = &lstatus };
    list[i] = dds_create_listener (&larg[i]);
    CU_ASSERT_FATAL (list[i] != NULL);
    dds_lset_data_available (list[i], data_avail);
  }
  const dds_time_t tend = dds_time () + DDS_SECS (duration);
  uint32_t nreaders = 0;
  dds_entity_t rds[NRDS] = { 0 };
  while (!ddsrt_atomic_ld32 (&wrarg.stop) && !ddsrt_atomic_ld32 (&lstatus.error) && dds_time () < tend)
  {
    const uint32_t rdidx = ddsrt_random () % NRDS;
    if (rds[rdidx])
    {
      rc = dds_delete (rds[rdidx]);
      CU_ASSERT_FATAL (rc == 0);
      rds[rdidx] = 0;
      ddsrt_atomic_and32 (&lstatus.triggered, ~larg[rdidx].mask);
    }

    if ((nreaders % (30 * NRDS)) == 0)
    {
      // Trigger code path where the last reader disappears, which must
      // do "something" to prevent old data, received out-of-order, from
      // being received by a new reader as a consequence of receiving
      // the first heartbeat from the proxy writer after creating that
      // new reader.
      for (int i = 0; i < NRDS; i++)
      {
        if (rds[i] != 0)
        {
          rc = dds_delete (rds[i]);
          CU_ASSERT_FATAL (rc == 0);
          rds[i] = 0;
          ddsrt_atomic_and32 (&lstatus.triggered, ~larg[rdidx].mask);
        }
      }
      dds_sleepfor (DDS_MSECS (1));
    }

    rds[rdidx] = dds_create_reader (sub_pp, sub_tp, NULL, list[rdidx]);
    CU_ASSERT_FATAL (rds[rdidx] > 0);
    while (!ddsrt_atomic_ld32 (&wrarg.stop) && !ddsrt_atomic_ld32 (&lstatus.error) && dds_time () < tend &&
           !(ddsrt_atomic_ld32 (&lstatus.triggered) & larg[rdidx].mask))
    {
      dds_sleepfor (DDS_MSECS (1));
    }

    nreaders++;
  }
  ddsrt_atomic_or32 (&wrarg.stop, 1);
  ddsrt_thread_join (wrtid, NULL);

  printf ("nreaders %"PRIu32"\n", nreaders);
  printf ("triggered %"PRIx32"\n", ddsrt_atomic_ld32 (&lstatus.triggered));
  printf ("error %"PRIu32"\n", ddsrt_atomic_ld32 (&lstatus.error));
  printf ("taken %"PRIu32"\n", ddsrt_atomic_ld32 (&lstatus.taken));
  printf ("badparam %"PRIu32"\n", ddsrt_atomic_ld32 (&lstatus.badparam));
  printf ("stop %"PRIu32"\n", ddsrt_atomic_ld32 (&wrarg.stop));

  CU_ASSERT_FATAL (nreaders > 10); // sanity check
  CU_ASSERT_FATAL (!ddsrt_atomic_ld32 (&lstatus.error));
  CU_ASSERT_FATAL (ddsrt_atomic_ld32 (&lstatus.taken) > 100);
  CU_ASSERT_FATAL (!(ddsrt_atomic_ld32 (&wrarg.stop) & 2));

  for (uint32_t i = 0; i < NRDS; i++)
    dds_delete_listener (list[i]);
  rc = dds_delete (sub_dom);
  CU_ASSERT_FATAL (sub_dom == 0 || rc == 0);
  rc = dds_delete (pub_dom);
  CU_ASSERT_FATAL (pub_dom == 0 || rc == 0);
#undef NRDS
}

CU_Test(ddsc_data_avail_stress, local)
{
  stress_data_avail_delete_reader (false, 3);
}

CU_Test(ddsc_data_avail_stress, remote, .timeout = 15)
{
  stress_data_avail_delete_reader (true, 9);
}
