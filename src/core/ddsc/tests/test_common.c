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
#include "dds/dds.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "test_common.h"

static void sync_reader_writer_impl (dds_entity_t participant_rd, dds_entity_t reader, dds_entity_t participant_wr, dds_entity_t writer, bool expect_sync, dds_duration_t timeout)
{
  dds_attach_t triggered;
  dds_return_t ret;
  dds_entity_t waitset_rd = dds_create_waitset (participant_rd);
  CU_ASSERT_FATAL (waitset_rd > 0);
  dds_entity_t waitset_wr = dds_create_waitset (participant_wr);
  CU_ASSERT_FATAL (waitset_wr > 0);

  /* Sync reader to writer. */
  ret = dds_set_status_mask (reader, DDS_SUBSCRIPTION_MATCHED_STATUS);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_attach (waitset_rd, reader, reader);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_wait (waitset_rd, &triggered, 1, timeout);
  if (expect_sync)
  {
    CU_ASSERT_EQUAL_FATAL (ret, 1);
    CU_ASSERT_EQUAL_FATAL (reader, (dds_entity_t)(intptr_t) triggered);
  }
  else
    CU_ASSERT_EQUAL_FATAL (ret, 0);
  ret = dds_waitset_detach (waitset_rd, reader);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  dds_delete (waitset_rd);

  /* Sync writer to reader. */
  ret = dds_set_status_mask (writer, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_attach (waitset_wr, writer, writer);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_wait (waitset_wr, &triggered, 1, timeout);
  if (expect_sync)
  {
    CU_ASSERT_EQUAL_FATAL (ret, 1);
    CU_ASSERT_EQUAL_FATAL (writer, (dds_entity_t)(intptr_t) triggered);
  }
  else
    CU_ASSERT_EQUAL_FATAL (ret, 0);
  ret = dds_waitset_detach (waitset_wr, writer);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  dds_delete (waitset_wr);
}

void sync_reader_writer (dds_entity_t participant_rd, dds_entity_t reader, dds_entity_t participant_wr, dds_entity_t writer)
{
  // Timing out after 1s would seem to be reasonable, but in reality seems to result in CI flakiness
  // for some tests (e.g., CUnit_ddsc_xtypes_basic at the time of this comment).  A hypothesis is
  // that some of the tests that happen to run in parallel cause so much load and network traffic
  // that there is the occasional bit of packet loss, and if that affects discovery packets, it could
  // plausibly make it take longer than 1s.
  //
  // Given that some of the tests running in parallel are ones that intentionally generate massive
  // amounts of discovery traffic for topics and types, I think this hypothesis is plausible.
  //
  // Increasing the timeout by quite a bit if "expect_sync" is set (which it is here) should not
  // introduce any new problems as a timeout here results in immediate test failure.
  sync_reader_writer_impl (participant_rd, reader, participant_wr, writer, true, DDS_SECS (5));
}

void no_sync_reader_writer (dds_entity_t participant_rd, dds_entity_t reader, dds_entity_t participant_wr, dds_entity_t writer, dds_duration_t timeout)
{
  sync_reader_writer_impl (participant_rd, reader, participant_wr, writer, false, timeout);
}
