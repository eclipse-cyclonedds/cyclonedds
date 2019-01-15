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
#include <assert.h>
#include <string.h>
#include "dds__writer.h"
#include "dds__write.h"
#include "ddsi/ddsi_tkmap.h"
#include "ddsi/q_error.h"
#include "ddsi/q_thread.h"
#include "ddsi/q_xmsg.h"
#include "ddsi/ddsi_serdata.h"
#include "dds__stream.h"
#include "dds__err.h"
#include "ddsi/q_transmit.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_config.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_radmin.h"

dds_return_t dds_write (dds_entity_t writer, const void *data)
{
  dds_return_t ret;
  dds__retcode_t rc;
  dds_writer *wr;

  if (data == NULL)
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);

  if ((rc = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);
  ret = dds_write_impl (wr, data, dds_time (), 0);
  dds_writer_unlock (wr);
  return ret;
}

dds_return_t dds_writecdr (dds_entity_t writer, struct ddsi_serdata *serdata)
{
  dds_return_t ret;
  dds__retcode_t rc;
  dds_writer *wr;

  if (serdata == NULL)
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);

  if ((rc = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);
  ret = dds_writecdr_impl (wr, serdata, dds_time (), 0);
  dds_writer_unlock (wr);
  return ret;
}

dds_return_t dds_write_ts (dds_entity_t writer, const void *data, dds_time_t timestamp)
{
  dds_return_t ret;
  dds__retcode_t rc;
  dds_writer *wr;

  if (data == NULL || timestamp < 0)
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);

  if ((rc = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return DDS_ERRNO (rc);
  ret = dds_write_impl (wr, data, timestamp, 0);
  dds_writer_unlock (wr);
  return ret;
}

static dds_return_t try_store (struct rhc *rhc, const struct proxy_writer_info *pwr_info, struct ddsi_serdata *payload, struct ddsi_tkmap_instance *tk, dds_duration_t *max_block_ms)
{
  while (!(ddsi_plugin.rhc_plugin.rhc_store_fn) (rhc, pwr_info, payload, tk))
  {
    if (*max_block_ms > 0)
    {
      dds_sleepfor (DDS_HEADBANG_TIMEOUT);
      *max_block_ms -= DDS_HEADBANG_TIMEOUT;
    }
    else
    {
      DDS_ERROR ("The writer could not deliver data on time, probably due to a local reader resources being full\n");
      return DDS_ERRNO (DDS_RETCODE_TIMEOUT);
    }
  }
  return DDS_RETCODE_OK;
}

static dds_return_t deliver_locally (struct writer *wr, struct ddsi_serdata *payload, struct ddsi_tkmap_instance *tk)
{
  dds_return_t ret = DDS_RETCODE_OK;
  os_mutexLock (&wr->rdary.rdary_lock);
  if (wr->rdary.fastpath_ok)
  {
    struct reader ** const rdary = wr->rdary.rdary;
    if (rdary[0])
    {
      dds_duration_t max_block_ms = nn_from_ddsi_duration (wr->xqos->reliability.max_blocking_time);
      struct proxy_writer_info pwr_info;
      unsigned i;
      make_proxy_writer_info (&pwr_info, &wr->e, wr->xqos);
      for (i = 0; rdary[i]; i++) {
        DDS_TRACE ("reader %x:%x:%x:%x\n", PGUID (rdary[i]->e.guid));
        if ((ret = try_store (rdary[i]->rhc, &pwr_info, payload, tk, &max_block_ms)) != DDS_RETCODE_OK)
          break;
      }
    }
    os_mutexUnlock (&wr->rdary.rdary_lock);
  }
  else
  {
    /* When deleting, pwr is no longer accessible via the hash
     tables, and consequently, a reader may be deleted without
     it being possible to remove it from rdary. The primary
     reason rdary exists is to avoid locking the proxy writer
     but this is less of an issue when we are deleting it, so
     we fall back to using the GUIDs so that we can deliver all
     samples we received from it. As writer being deleted any
     reliable samples that are rejected are simply discarded. */
    ut_avlIter_t it;
    struct pwr_rd_match *m;
    struct proxy_writer_info pwr_info;
    dds_duration_t max_block_ms = nn_from_ddsi_duration (wr->xqos->reliability.max_blocking_time);
    os_mutexUnlock (&wr->rdary.rdary_lock);
    make_proxy_writer_info (&pwr_info, &wr->e, wr->xqos);
    os_mutexLock (&wr->e.lock);
    for (m = ut_avlIterFirst (&wr_local_readers_treedef, &wr->local_readers, &it); m != NULL; m = ut_avlIterNext (&it))
    {
      struct reader *rd;
      if ((rd = ephash_lookup_reader_guid (&m->rd_guid)) != NULL)
      {
        DDS_TRACE("reader-via-guid %x:%x:%x:%x\n", PGUID (rd->e.guid));
        /* Copied the return value ignore from DDSI deliver_user_data() function. */
        if ((ret = try_store (rd->rhc, &pwr_info, payload, tk, &max_block_ms)) != DDS_RETCODE_OK)
          break;
      }
    }
    os_mutexUnlock (&wr->e.lock);
  }
  return ret;
}

dds_return_t dds_write_impl (dds_writer *wr, const void * data, dds_time_t tstamp, dds_write_action action)
{
  struct thread_state1 * const thr = lookup_thread_state ();
  const bool asleep = !vtime_awake_p (thr->vtime);
  const bool writekey = action & DDS_WR_KEY_BIT;
  struct writer *ddsi_wr = wr->m_wr;
  struct ddsi_tkmap_instance *tk;
  struct ddsi_serdata *d;
  dds_return_t ret = DDS_RETCODE_OK;
  int w_rc;

  if (data == NULL)
  {
    DDS_ERROR("No data buffer provided\n");
    return DDS_ERRNO (DDS_RETCODE_BAD_PARAMETER);
  }

  /* Check for topic filter */
  if (wr->m_topic->filter_fn && !writekey)
    if (!(wr->m_topic->filter_fn) (data, wr->m_topic->filter_ctx))
      return DDS_RETCODE_OK;

  if (asleep)
    thread_state_awake (thr);

  /* Serialize and write data or key */
  d = ddsi_serdata_from_sample (ddsi_wr->topic, writekey ? SDK_KEY : SDK_DATA, data);
  d->statusinfo = ((action & DDS_WR_DISPOSE_BIT) ? NN_STATUSINFO_DISPOSE : 0) | ((action & DDS_WR_UNREGISTER_BIT) ? NN_STATUSINFO_UNREGISTER : 0);
  d->timestamp.v = tstamp;
  ddsi_serdata_ref (d);
  tk = ddsi_tkmap_lookup_instance_ref (d);
  w_rc = write_sample_gc (wr->m_xp, ddsi_wr, d, tk);

  if (w_rc >= 0)
  {
    /* Flush out write unless configured to batch */
    if (!config.whc_batch)
      nn_xpack_send (wr->m_xp, false);
    ret = DDS_RETCODE_OK;
  } else if (w_rc == ERR_TIMEOUT) {
    DDS_ERROR ("The writer could not deliver data on time, probably due to a reader resources being full\n");
    ret = DDS_ERRNO (DDS_RETCODE_TIMEOUT);
  } else if (w_rc == ERR_INVALID_DATA) {
    DDS_ERROR ("Invalid data provided\n");
    ret = DDS_ERRNO (DDS_RETCODE_ERROR);
  } else {
    DDS_ERROR ("Internal error\n");
    ret = DDS_ERRNO (DDS_RETCODE_ERROR);
  }
  if (ret == DDS_RETCODE_OK)
    ret = deliver_locally (ddsi_wr, d, tk);
  ddsi_serdata_unref (d);
  ddsi_tkmap_instance_unref (tk);

  if (asleep)
    thread_state_asleep (thr);
  return ret;
}

dds_return_t dds_writecdr_impl_lowlevel (struct writer *ddsi_wr, struct nn_xpack *xp, struct ddsi_serdata *d)
{
  struct thread_state1 * const thr = lookup_thread_state ();
  const bool asleep = !vtime_awake_p (thr->vtime);
  struct ddsi_tkmap_instance * tk;
  int ret = DDS_RETCODE_OK;
  int w_rc;

  if (asleep)
    thread_state_awake (thr);

  ddsi_serdata_ref (d);
  tk = ddsi_tkmap_lookup_instance_ref (d);
  w_rc = write_sample_gc (xp, ddsi_wr, d, tk);
  if (w_rc >= 0) {
    /* Flush out write unless configured to batch */
    if (!config.whc_batch && xp != NULL)
      nn_xpack_send (xp, false);
    ret = DDS_RETCODE_OK;
  } else if (w_rc == ERR_TIMEOUT) {
    DDS_ERROR ("The writer could not deliver data on time, probably due to a reader resources being full\n");
    ret = DDS_ERRNO(DDS_RETCODE_TIMEOUT);
  } else if (w_rc == ERR_INVALID_DATA) {
    DDS_ERROR ("Invalid data provided\n");
    ret = DDS_ERRNO (DDS_RETCODE_ERROR);
  } else {
    DDS_ERROR ("Internal error\n");
    ret = DDS_ERRNO (DDS_RETCODE_ERROR);
  }

  if (ret == DDS_RETCODE_OK)
    ret = deliver_locally (ddsi_wr, d, tk);
  ddsi_serdata_unref (d);
  ddsi_tkmap_instance_unref (tk);

  if (asleep)
    thread_state_asleep (thr);

  return ret;
}

dds_return_t dds_writecdr_impl (dds_writer *wr, struct ddsi_serdata *d, dds_time_t tstamp, dds_write_action action)
{
  if (wr->m_topic->filter_fn)
    abort ();
  /* Set if disposing or unregistering */
  d->statusinfo = ((action & DDS_WR_DISPOSE_BIT) ? NN_STATUSINFO_DISPOSE : 0) | ((action & DDS_WR_UNREGISTER_BIT) ? NN_STATUSINFO_UNREGISTER : 0);
  d->timestamp.v = tstamp;
  return dds_writecdr_impl_lowlevel (wr->m_wr, wr->m_xp, d);
}

void dds_write_set_batch (bool enable)
{
  config.whc_batch = enable ? 1 : 0;
}

void dds_write_flush (dds_entity_t writer)
{
  struct thread_state1 * const thr = lookup_thread_state ();
  const bool asleep = !vtime_awake_p (thr->vtime);
  dds_writer *wr;
  dds__retcode_t rc;

  if (asleep)
    thread_state_awake (thr);
  if ((rc = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    DDS_ERROR ("Error occurred on locking writer\n");
  else
  {
    nn_xpack_send (wr->m_xp, true);
    dds_writer_unlock (wr);
  }

  if (asleep)
    thread_state_asleep (thr);
  return;
}
