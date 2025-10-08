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

#include "Space.h"

#include "dds/dds.h"
#include "test_common.h"

CU_Test (ddsc_data_on_readers, basic)
{
#define NRDS 2
  const dds_entity_t dp = dds_create_participant (0, NULL, NULL);
  char tpname[100];
  create_unique_topic_name ("ddsc_data_on_readers_basic", tpname, sizeof (tpname));
  const dds_entity_t tp = dds_create_topic (dp, &Space_Type1_desc, tpname, NULL, NULL);
  const dds_entity_t sub = dds_create_subscriber (dp, NULL, NULL);
  dds_entity_t rds[NRDS];
  for (int i = 0; i < NRDS; i++)
    rds[i] = dds_create_reader (sub, tp, NULL, NULL);
  const dds_entity_t wr = dds_create_writer (dp, tp, NULL, NULL);

  dds_entity_t ws[2];
  ws[0] = dds_create_waitset (dp);
  ws[1] = dds_create_waitset (dp);

  dds_return_t rc;
  uint32_t status;

  // run with 0 .. 2 .. 0 attached waitsets
  for (int x = 0; x <= 4; x++)
  {
    bool materialized = (x > 0 && x < 4);
    if (x > 0 && x <= 2)
    {
      rc = dds_waitset_attach (ws[x - 1], sub, 0);
      CU_ASSERT_EQ_FATAL (rc, 0);
    }
    else if (x > 2)
    {
      rc = dds_waitset_detach (ws[4 - x], sub);
      CU_ASSERT_EQ_FATAL (rc, 0);
    }

    // initially no DoR or DA
    rc = dds_take_status (sub, &status, DDS_DATA_ON_READERS_STATUS);
    CU_ASSERT_FATAL (rc == 0 && status == 0);
    for (int i = 0; i < NRDS; i++)
    {
      rc = dds_read_status (rds[i], &status, DDS_DATA_AVAILABLE_STATUS);
      CU_ASSERT_FATAL (rc == 0 && status == 0);
    }

    // after write, DoR and DA on all
    rc = dds_write (wr, &(Space_Type1){1,1,1});
    CU_ASSERT_EQ_FATAL (rc, 0);
    rc = dds_read_status (sub, &status, DDS_DATA_ON_READERS_STATUS);
    CU_ASSERT_FATAL (rc == 0 && status != 0);
    for (int i = 0; i < NRDS; i++)
    {
      rc = dds_read_status (rds[i], &status, DDS_DATA_AVAILABLE_STATUS);
      CU_ASSERT_FATAL (rc == 0 && status != 0);
    }

    // attempting to take non-materialized status has no effect, does reset flag if
    // materialized -- make sure we hit that case at least once but not always if
    // materialized
    if (!materialized || x == 2)
    {
      rc = dds_take_status (sub, &status, DDS_DATA_ON_READERS_STATUS);
      CU_ASSERT_FATAL (rc == 0 && status != 0);
      rc = dds_read_status (sub, &status, DDS_DATA_ON_READERS_STATUS);
      CU_ASSERT_FATAL (rc == 0 && ((materialized && status == 0) || (!materialized && status != 0)));
    }

    // read/take resets DA on reader, also resets DoR iff DoR is materialized
    Space_Type1 samp;
    void *raw = &samp;
    dds_sample_info_t info;
    for (int k = 0; k < NRDS; k++)
    {
      rc = dds_read (rds[k], &raw, &info, 1, 1);
      CU_ASSERT_EQ_FATAL (rc, 1);
      rc = dds_read_status (sub, &status, DDS_DATA_ON_READERS_STATUS);
      CU_ASSERT_EQ_FATAL (rc, 0);
      CU_ASSERT_NEQ_FATAL ((materialized && status == 0) ||
                           (!materialized && k < NRDS-1 && status != 0) ||
                           (!materialized && k == NRDS-1 && status == 0), false);
      for (int i = 0; i < NRDS; i++)
      {
        rc = dds_read_status (rds[i], &status, DDS_DATA_AVAILABLE_STATUS);
        CU_ASSERT_EQ_FATAL (rc, 0);
        CU_ASSERT_FATAL ((i <= k && status == 0) || (i > k && status != 0));
      }
    }
  }
#undef NRDS

  rc = dds_delete (dp);
  CU_ASSERT_EQ_FATAL (rc, 0);
}

CU_Test (ddsc_data_on_readers, while_da)
{
#define NRDS 2
  const dds_entity_t dp = dds_create_participant (0, NULL, NULL);
  char tpname[100];
  create_unique_topic_name ("ddsc_data_on_readers_basic", tpname, sizeof (tpname));
  const dds_entity_t tp = dds_create_topic (dp, &Space_Type1_desc, tpname, NULL, NULL);
  const dds_entity_t sub = dds_create_subscriber (dp, NULL, NULL);
  dds_entity_t rds[NRDS];
  for (int i = 0; i < NRDS; i++)
    rds[i] = dds_create_reader (sub, tp, NULL, NULL);
  const dds_entity_t wr = dds_create_writer (dp, tp, NULL, NULL);
  const dds_entity_t ws = dds_create_waitset (dp);

  dds_return_t rc;
  uint32_t status;

  // initially no DoR or DA
  rc = dds_take_status (sub, &status, DDS_DATA_ON_READERS_STATUS);
  CU_ASSERT_FATAL (rc == 0 && status == 0);
  for (int i = 0; i < NRDS; i++)
  {
    rc = dds_read_status (rds[i], &status, DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_FATAL (rc == 0 && status == 0);
  }

  // after write, DoR and DA on all
  rc = dds_write (wr, &(Space_Type1){1,1,1});
  CU_ASSERT_EQ_FATAL (rc, 0);
  rc = dds_read_status (sub, &status, DDS_DATA_ON_READERS_STATUS);
  CU_ASSERT_FATAL (rc == 0 && status != 0);
  for (int i = 0; i < NRDS; i++)
  {
    rc = dds_read_status (rds[i], &status, DDS_DATA_AVAILABLE_STATUS);
    CU_ASSERT_FATAL (rc == 0 && status != 0);
  }

  // attach waitset - switchover to materialized
  // will remain set
  rc = dds_waitset_attach (ws, sub, 0);
  CU_ASSERT_EQ_FATAL (rc, 0);
  rc = dds_read_status (sub, &status, DDS_DATA_ON_READERS_STATUS);
  CU_ASSERT_FATAL (rc == 0 && status != 0);
#undef NRDS

  rc = dds_delete (dp);
  CU_ASSERT_EQ_FATAL (rc, 0);
}
