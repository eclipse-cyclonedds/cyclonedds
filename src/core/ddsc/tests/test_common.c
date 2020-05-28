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

void sync_reader_writer (dds_entity_t participant_rd, dds_entity_t reader, dds_entity_t participant_wr, dds_entity_t writer)
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
  ret = dds_waitset_wait (waitset_rd, &triggered, 1, DDS_SECS (1));
  CU_ASSERT_EQUAL_FATAL (ret, 1);
  CU_ASSERT_EQUAL_FATAL (reader, (dds_entity_t)(intptr_t) triggered);
  ret = dds_waitset_detach (waitset_rd, reader);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  dds_delete (waitset_rd);

  /* Sync writer to reader. */
  ret = dds_set_status_mask (writer, DDS_PUBLICATION_MATCHED_STATUS);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_attach (waitset_wr, writer, writer);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  ret = dds_waitset_wait (waitset_wr, &triggered, 1, DDS_SECS (1));
  CU_ASSERT_EQUAL_FATAL (ret, 1);
  CU_ASSERT_EQUAL_FATAL (writer, (dds_entity_t)(intptr_t) triggered);
  ret = dds_waitset_detach (waitset_wr, writer);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  dds_delete (waitset_wr);
}
