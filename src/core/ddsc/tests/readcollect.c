// Copyright(c) 2023 ZettaScale Technology and others
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
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsi/ddsi_serdata.h"

#include "dds__entity.h"
#include "test_common.h"

/* Regular read/take is built on the same interface, so we don't need to check
   here that it works in the regular cases.  What *is* interesting to test is
   error handling. */

typedef dds_return_t (*read_op) (dds_entity_t rd_or_cnd, uint32_t maxs, dds_instance_handle_t handle, uint32_t mask, dds_read_with_collector_fn_t collect_sample, void *collect_sample_arg);

static Space_Type1 getdata (const dds_sample_info_t *si, const struct ddsi_sertype *st, struct ddsi_serdata *sd)
{
  Space_Type1 s;
  bool ok;
  if (si->valid_data)
    ok = ddsi_serdata_to_sample (sd, &s, NULL, NULL);
  else
    ok = ddsi_serdata_untyped_to_sample (st, sd, &s, NULL, NULL);
  CU_ASSERT_FATAL (ok);
  return s;
}

static dds_return_t coll_fail_always (void *varg, const dds_sample_info_t *si, const struct ddsi_sertype *st, struct ddsi_serdata *sd)
{
  (void)varg;
  CU_ASSERT_FATAL (!si->valid_data || st == sd->type);
  Space_Type1 s = getdata (si, st, sd);
  printf ("coll_fail_always: %d, %d\n", s.long_1, s.long_2);
  return INT32_MIN; // easily recognized negative number that is not a return code from Cyclone DDS
}

struct coll_fail_after_1_arg {
  int count;
  int32_t k;
};

static dds_return_t coll_fail_after_1 (void *varg, const dds_sample_info_t *si, const struct ddsi_sertype *st, struct ddsi_serdata *sd)
{
  CU_ASSERT_FATAL (!si->valid_data || st == sd->type);
  struct coll_fail_after_1_arg * const arg = varg;
  Space_Type1 s = getdata (si, st, sd);
  printf ("coll_fail_after_1: %d, %d\n", s.long_1, s.long_2);
  arg->k = s.long_1;
  return (arg->count++ == 0) ? 0 : INT32_MIN;
}

static void dotest (read_op op)
{
  const dds_entity_t dp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (dp > 0);
  char topicname[100];
  create_unique_topic_name("ddsc_read_with_collector", topicname, sizeof (topicname));
  dds_qos_t *qos = dds_create_qos ();
  dds_qset_reliability(qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  dds_qset_history (qos, DDS_HISTORY_KEEP_ALL, 0);
  const dds_entity_t tp = dds_create_topic (dp, &Space_Type1_desc, topicname, qos, NULL);
  CU_ASSERT_FATAL (tp > 0);
  dds_delete_qos (qos);
  const dds_entity_t rd = dds_create_reader (dp, tp, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);
  const dds_entity_t wr = dds_create_writer (dp, tp, NULL, NULL);
  CU_ASSERT_FATAL (wr > 0);

  dds_return_t rc;
  for (int32_t k = 0; k < 3; k++)
  {
    for (int32_t v = 0; v < 3; v++)
    {
      rc = dds_write (wr, &(Space_Type1){ .long_1 = k, .long_2 = v, .long_3 = 0 });
      CU_ASSERT_FATAL (rc == 0);
    }
  }

  // failure on first call to collect: error is propagated
  rc = op (rd, INT32_MAX, 0, 0, coll_fail_always, NULL);
  CU_ASSERT_FATAL (rc == INT32_MIN);

  // failure on subsequent calls to collect: partial result returned
  struct coll_fail_after_1_arg arg1 = { .count = 0, .k = -1 };
  rc = op (rd, INT32_MAX, 0, 0, coll_fail_after_1, &arg1);
  CU_ASSERT_FATAL (rc == 1);
  CU_ASSERT_FATAL (arg1.k >= 0 && arg1.k <= 2);

  // same should be true if instance handle is provided, use a different instance just because we can
  dds_instance_handle_t ih;
  rc = dds_register_instance (wr, &ih, &(Space_Type1){ .long_1 = (1+arg1.k)%3, .long_2 = 0, .long_3 = 0 });
  CU_ASSERT_FATAL (rc == 0);
  rc = op (rd, INT32_MAX, ih, 0, coll_fail_always, NULL);
  CU_ASSERT_FATAL (rc == INT32_MIN);
  struct coll_fail_after_1_arg arg2 = { .count = 0, .k = -1 };
  rc = op (rd, INT32_MAX, ih, 0, coll_fail_after_1, &arg2);
  CU_ASSERT_FATAL (rc == 1);
  CU_ASSERT_FATAL (arg2.k == (1+arg1.k)%3);

  assert (op == dds_peek_with_collector || op == dds_read_with_collector || op == dds_take_with_collector);
  bool isread = (op == dds_read_with_collector);
  bool isnew = (op == dds_peek_with_collector);

  // check that the remainder is as we expect it
  Space_Type1 xs[10];
  dds_sample_info_t si[10];
  void *ptrs[10];
  for (uint32_t i = 0; i < 10; i++)
    ptrs[i] = &xs[i];
  rc = dds_take (rd, ptrs, si, (size_t) (2 + isread + isnew), (uint32_t) (2 + isread + isnew));
  for (int i = 0; i < rc; i++)
    printf ("take(1) %"PRId32", %"PRId32" %c%c\n", xs[i].long_1, xs[i].long_2,
            (si[i].sample_state == DDS_NOT_READ_SAMPLE_STATE) ? 'f' : 's',
            (si[i].view_state == DDS_NEW_VIEW_STATE) ? 'n' : 'o');
  CU_ASSERT_FATAL (rc == (int32_t) (2 + isread + isnew));
  for (int i = 0; i < rc; i++)
  {
    CU_ASSERT_FATAL (xs[i].long_1 == arg1.k);
    CU_ASSERT_FATAL (si[i].sample_state == (i == 0 && isread ? DDS_READ_SAMPLE_STATE : DDS_NOT_READ_SAMPLE_STATE));
    CU_ASSERT_FATAL (si[i].view_state == (isnew ? DDS_NEW_VIEW_STATE : DDS_NOT_NEW_VIEW_STATE));
  }
  rc = dds_take_instance (rd, ptrs, si, (size_t) (2 + isread + isnew), (uint32_t) (2 + isread + isnew), ih);
  for (int i = 0; i < rc; i++)
    printf ("take(2) %"PRId32", %"PRId32" %c%c\n", xs[i].long_1, xs[i].long_2,
            (si[i].sample_state == DDS_NOT_READ_SAMPLE_STATE) ? 'f' : 's',
            (si[i].view_state == DDS_NEW_VIEW_STATE) ? 'n' : 'o');
  CU_ASSERT_FATAL (rc == (int32_t) (2 + isread + isnew));
  for (int i = 0; i < rc; i++)
  {
    CU_ASSERT_FATAL (xs[i].long_1 == arg2.k);
    CU_ASSERT_FATAL (si[i].sample_state == (i == 0 && isread ? DDS_READ_SAMPLE_STATE : DDS_NOT_READ_SAMPLE_STATE));
    CU_ASSERT_FATAL (si[i].view_state == (isnew ? DDS_NEW_VIEW_STATE : DDS_NOT_NEW_VIEW_STATE));
  }
  rc = dds_take (rd, ptrs, si, 10, 10);
  for (int i = 0; i < rc; i++)
    printf ("take(3) %"PRId32", %"PRId32" %c%c\n", xs[i].long_1, xs[i].long_2,
            (si[i].sample_state == DDS_NOT_READ_SAMPLE_STATE) ? 'f' : 's',
            (si[i].view_state == DDS_NEW_VIEW_STATE) ? 'n' : 'o');
  CU_ASSERT_FATAL (rc == 3);
  for (int i = 0; i < rc; i++)
  {
    CU_ASSERT_FATAL (xs[i].long_1 == (arg1.k+2)%3);
    CU_ASSERT_FATAL (si[i].sample_state == DDS_NOT_READ_SAMPLE_STATE);
    CU_ASSERT_FATAL (si[i].view_state == DDS_NEW_VIEW_STATE);
  }

  rc = dds_delete (dp);
  CU_ASSERT_FATAL (rc == 0);
}

CU_Test(ddsc_read_with_collector, peek)
{
  dotest (dds_peek_with_collector);
}

CU_Test(ddsc_read_with_collector, read)
{
  dotest (dds_read_with_collector);
}

CU_Test(ddsc_read_with_collector, take)
{
  dotest (dds_take_with_collector);
}
