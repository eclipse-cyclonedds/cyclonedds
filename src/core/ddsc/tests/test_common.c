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
#include "dds__entity.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/ddsi_entity_index.h"
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
  ret = dds_waitset_wait (waitset_rd, &triggered, 1, DDS_SECS (5));
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
  ret = dds_waitset_wait (waitset_wr, &triggered, 1, DDS_SECS (5));
  CU_ASSERT_EQUAL_FATAL (ret, 1);
  CU_ASSERT_EQUAL_FATAL (writer, (dds_entity_t)(intptr_t) triggered);
  ret = dds_waitset_detach (waitset_wr, writer);
  CU_ASSERT_EQUAL_FATAL (ret, DDS_RETCODE_OK);
  dds_delete (waitset_wr);
}

void waitfor_or_reset_fastpath (dds_entity_t rdhandle, bool fastpath, size_t nwr)
{
  dds_return_t rc;
  struct dds_entity *x;

  rc = dds_entity_pin (rdhandle, &x);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  CU_ASSERT_FATAL (dds_entity_kind (x) == DDS_KIND_READER);

  struct reader * const rd = ((struct dds_reader *) x)->m_rd;
  struct rd_pwr_match *m;
  ddsi_guid_t cursor;
  size_t wrcount = 0;
  thread_state_awake (lookup_thread_state (), rd->e.gv);
  ddsrt_mutex_lock (&rd->e.lock);

  memset (&cursor, 0, sizeof (cursor));
  while ((m = ddsrt_avl_lookup_succ (&rd_writers_treedef, &rd->writers, &cursor)) != NULL)
  {
    cursor = m->pwr_guid;
    ddsrt_mutex_unlock (&rd->e.lock);
    struct proxy_writer * const pwr = entidx_lookup_proxy_writer_guid (rd->e.gv->entity_index, &cursor);
    ddsrt_mutex_lock (&pwr->rdary.rdary_lock);
    if (!fastpath)
      pwr->rdary.fastpath_ok = false;
    else
    {
      while (!pwr->rdary.fastpath_ok)
      {
        ddsrt_mutex_unlock (&pwr->rdary.rdary_lock);
        dds_sleepfor (DDS_MSECS (10));
        ddsrt_mutex_lock (&pwr->rdary.rdary_lock);
      }
    }
    wrcount++;
    ddsrt_mutex_unlock (&pwr->rdary.rdary_lock);
    ddsrt_mutex_lock (&rd->e.lock);
  }

  memset (&cursor, 0, sizeof (cursor));
  while ((m = ddsrt_avl_lookup_succ (&rd_local_writers_treedef, &rd->local_writers, &cursor)) != NULL)
  {
    cursor = m->pwr_guid;
    ddsrt_mutex_unlock (&rd->e.lock);
    struct writer * const wr = entidx_lookup_writer_guid (rd->e.gv->entity_index, &cursor);
    ddsrt_mutex_lock (&wr->rdary.rdary_lock);
    if (!fastpath)
      wr->rdary.fastpath_ok = fastpath;
    else
    {
      while (!wr->rdary.fastpath_ok)
      {
        ddsrt_mutex_unlock (&wr->rdary.rdary_lock);
        dds_sleepfor (DDS_MSECS (10));
        ddsrt_mutex_lock (&wr->rdary.rdary_lock);
      }
    }
    wrcount++;
    ddsrt_mutex_unlock (&wr->rdary.rdary_lock);
    ddsrt_mutex_lock (&rd->e.lock);
  }
  ddsrt_mutex_unlock (&rd->e.lock);
  thread_state_asleep (lookup_thread_state ());
  dds_entity_unpin (x);

  CU_ASSERT_FATAL (wrcount == nwr);
}
