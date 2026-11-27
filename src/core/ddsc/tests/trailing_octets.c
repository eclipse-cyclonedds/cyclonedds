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
#include "dds/dds.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/time.h"

#include "test_util.h"
#include "TrailingOctets.h"

/* The reader and the writer use the same topic name and type name, but the
   writer's type has one member more than the reader's. With the type support
   generated without type information, that is a configurable combination: the
   reader is interested in a prefix of what the writer sends. The consequence is
   that received samples are longer than the extent of the reader's type.

   See issue #2453 for a report of such a sample being accepted. */

struct endpoints {
  dds_entity_t pp_rd;
  dds_entity_t pp_wr;
  dds_entity_t tp_rd;
  dds_entity_t tp_wr;
  dds_entity_t rd;
  dds_entity_t wr;
};

static void endpoints_init (struct endpoints *e, const char *topic_name, bool disallow_type_coercion)
{
  dds_topic_descriptor_t wide_desc = TrailingOctets_wide_desc;
  dds_qos_t *rqos = dds_create_qos ();
  dds_qos_t *wqos = dds_create_qos ();
  CU_ASSERT_NEQ_FATAL (rqos, NULL);
  CU_ASSERT_NEQ_FATAL (wqos, NULL);

  /* same type name as the reader's type, different layout */
  wide_desc.m_typename = TrailingOctets_narrow_desc.m_typename;

  e->pp_rd = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  e->pp_wr = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_GT_FATAL (e->pp_rd, 0);
  CU_ASSERT_GT_FATAL (e->pp_wr, 0);

  e->tp_rd = dds_create_topic (e->pp_rd, &TrailingOctets_narrow_desc, topic_name, NULL, NULL);
  e->tp_wr = dds_create_topic (e->pp_wr, &wide_desc, topic_name, NULL, NULL);
  CU_ASSERT_GT_FATAL (e->tp_rd, 0);
  CU_ASSERT_GT_FATAL (e->tp_wr, 0);

  dds_qset_reliability (rqos, DDS_RELIABILITY_RELIABLE, DDS_SECS (2));
  if (disallow_type_coercion)
    dds_qset_type_consistency (rqos, DDS_TYPE_CONSISTENCY_DISALLOW_TYPE_COERCION, false, false, false, true, false);
  e->rd = dds_create_reader (e->pp_rd, e->tp_rd, rqos, NULL);
  CU_ASSERT_GT_FATAL (e->rd, 0);

  dds_qset_reliability (wqos, DDS_RELIABILITY_RELIABLE, DDS_SECS (2));
  e->wr = dds_create_writer (e->pp_wr, e->tp_wr, wqos, NULL);
  CU_ASSERT_GT_FATAL (e->wr, 0);

  sync_reader_writer (e->pp_rd, e->rd, e->pp_wr, e->wr);
  dds_delete_qos (rqos);
  dds_delete_qos (wqos);
}

static void endpoints_fini (struct endpoints *e)
{
  dds_delete (e->rd);
  dds_delete (e->wr);
  dds_delete (e->tp_rd);
  dds_delete (e->tp_wr);
  dds_delete (e->pp_rd);
  dds_delete (e->pp_wr);
}

static void write_wide_sample (dds_entity_t wr)
{
  /* Stack-allocated sequence buffer: dds_write copies the sample, so there is
     no owned memory to release afterwards. */
  int32_t values[3] = { 63, 64, 65 };
  TrailingOctets_wide sample;
  memset (&sample, 0, sizeof (sample));
  ddsrt_strlcpy (sample.name, "ockgknze", sizeof (sample.name));
  sample.values._maximum = 3;
  sample.values._length = 3;
  sample.values._buffer = values;
  sample.values._release = false;
  sample.extra = 66;
  CU_ASSERT_EQ_FATAL (dds_write (wr, &sample), DDS_RETCODE_OK);
}

static uint32_t take_samples (dds_entity_t rd, dds_duration_t timeout)
{
  dds_entity_t ws = dds_create_waitset (DDS_CYCLONEDDS_HANDLE);
  CU_ASSERT_GT_FATAL (ws, 0);
  CU_ASSERT_EQ_FATAL (dds_waitset_attach (ws, rd, 0), DDS_RETCODE_OK);
  (void) dds_waitset_wait (ws, NULL, 0, timeout);
  CU_ASSERT_EQ_FATAL (dds_waitset_detach (ws, rd), DDS_RETCODE_OK);
  dds_delete (ws);

  void *samples[1] = { NULL };
  dds_sample_info_t si;
  const uint32_t n = (uint32_t) dds_take (rd, samples, &si, 1, 1);
  if (n == 1)
  {
    const TrailingOctets_narrow *sample = samples[0];
    /* the sample is as long as the writer's type and the reader's type covers a
       prefix of it; what the reader makes of it is what it always did */
    CU_ASSERT_EQ_FATAL (si.valid_data, true);
    CU_ASSERT_EQ_FATAL (sample->values._length, 3);
    CU_ASSERT_STREQ (sample->name, "ockgknze");
  }
  CU_ASSERT_EQ_FATAL (dds_return_loan (rd, samples, (int32_t) n), DDS_RETCODE_OK);
  return n;
}

CU_Test (ddsc_trailing_octets, accept_without_type_consistency)
{
  char topic_name[100];
  struct endpoints e;
  create_unique_topic_name ("ddsc_trailing_octets", topic_name, sizeof (topic_name));
  endpoints_init (&e, topic_name, false);
  write_wide_sample (e.wr);
  CU_ASSERT_EQ_FATAL (take_samples (e.rd, DDS_SECS (5)), 1);
  endpoints_fini (&e);
}

CU_Test (ddsc_trailing_octets, reject_with_disallow_type_coercion)
{
  char topic_name[100];
  struct endpoints e;
  create_unique_topic_name ("ddsc_trailing_octets", topic_name, sizeof (topic_name));
  endpoints_init (&e, topic_name, true);
  write_wide_sample (e.wr);
  CU_ASSERT_EQ_FATAL (take_samples (e.rd, DDS_SECS (5)), 0);
  endpoints_fini (&e);
}
