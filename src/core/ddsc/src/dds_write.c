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
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/ddsi_rhc.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds__stream.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/q_globals.h"

dds_return_t dds_write (dds_entity_t writer, const void *data)
{
  dds_return_t ret;
  dds_writer *wr;

  if (data == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;
  ret = dds_write_impl (wr, data, dds_time (), 0);
  dds_writer_unlock (wr);
  return ret;
}

dds_return_t dds_writecdr (dds_entity_t writer, struct ddsi_serdata *serdata)
{
  dds_return_t ret;
  dds_writer *wr;

  if (serdata == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;
  ret = dds_writecdr_impl (wr, serdata, dds_time (), 0);
  dds_writer_unlock (wr);
  return ret;
}

dds_return_t dds_write_ts (dds_entity_t writer, const void *data, dds_time_t timestamp)
{
  dds_return_t ret;
  dds_writer *wr;

  if (data == NULL || timestamp < 0)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;
  ret = dds_write_impl (wr, data, timestamp, 0);
  dds_writer_unlock (wr);
  return ret;
}

static dds_return_t try_store (struct ddsi_rhc *rhc, const struct ddsi_writer_info *pwr_info, struct ddsi_serdata *payload, struct ddsi_tkmap_instance *tk, dds_duration_t *max_block_ms)
{
  while (! ddsi_rhc_store (rhc, pwr_info, payload, tk))
  {
    if (*max_block_ms > 0)
    {
      dds_sleepfor (DDS_HEADBANG_TIMEOUT);
      *max_block_ms -= DDS_HEADBANG_TIMEOUT;
    }
    else
    {
      return DDS_RETCODE_TIMEOUT;
    }
  }
  return DDS_RETCODE_OK;
}

static dds_return_t deliver_locally (struct writer *wr, struct ddsi_serdata *payload, struct ddsi_tkmap_instance *tk)
{
  dds_return_t ret = DDS_RETCODE_OK;
  ddsrt_mutex_lock (&wr->rdary.rdary_lock);
  if (wr->rdary.fastpath_ok)
  {
    struct reader ** const rdary = wr->rdary.rdary;
    if (rdary[0])
    {
      dds_duration_t max_block_ms = wr->xqos->reliability.max_blocking_time;
      struct ddsi_writer_info pwr_info;
      ddsi_make_writer_info (&pwr_info, &wr->e, wr->xqos, payload->statusinfo);
      for (uint32_t i = 0; rdary[i]; i++) {
        DDS_CTRACE (&wr->e.gv->logconfig, "reader "PGUIDFMT"\n", PGUID (rdary[i]->e.guid));
        if ((ret = try_store (rdary[i]->rhc, &pwr_info, payload, tk, &max_block_ms)) != DDS_RETCODE_OK)
          break;
      }
    }
    ddsrt_mutex_unlock (&wr->rdary.rdary_lock);
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
    ddsrt_avl_iter_t it;
    struct pwr_rd_match *m;
    struct ddsi_writer_info wrinfo;
    const struct entity_index *gh = wr->e.gv->entity_index;
    dds_duration_t max_block_ms = wr->xqos->reliability.max_blocking_time;
    ddsrt_mutex_unlock (&wr->rdary.rdary_lock);
    ddsi_make_writer_info (&wrinfo, &wr->e, wr->xqos, payload->statusinfo);
    ddsrt_mutex_lock (&wr->e.lock);
    for (m = ddsrt_avl_iter_first (&wr_local_readers_treedef, &wr->local_readers, &it); m != NULL; m = ddsrt_avl_iter_next (&it))
    {
      struct reader *rd;
      if ((rd = entidx_lookup_reader_guid (gh, &m->rd_guid)) != NULL)
      {
        DDS_CTRACE (&wr->e.gv->logconfig, "reader-via-guid "PGUIDFMT"\n", PGUID (rd->e.guid));
        /* Copied the return value ignore from DDSI deliver_user_data () function. */
        if ((ret = try_store (rd->rhc, &wrinfo, payload, tk, &max_block_ms)) != DDS_RETCODE_OK)
          break;
      }
    }
    ddsrt_mutex_unlock (&wr->e.lock);
  }

  if (ret == DDS_RETCODE_TIMEOUT)
  {
    DDS_CERROR (&wr->e.gv->logconfig, "The writer could not deliver data on time, probably due to a local reader resources being full\n");
  }
  return ret;
}

dds_return_t dds_write_impl (dds_writer *wr, const void * data, dds_time_t tstamp, dds_write_action action)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  const bool writekey = action & DDS_WR_KEY_BIT;
  struct writer *ddsi_wr = wr->m_wr;
  struct ddsi_tkmap_instance *tk;
  struct ddsi_serdata *d;
  dds_return_t ret = DDS_RETCODE_OK;
  int w_rc;

  if (data == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  /* Check for topic filter */
  if (wr->m_topic->filter_fn && !writekey)
    if (! wr->m_topic->filter_fn (data, wr->m_topic->filter_ctx))
      return DDS_RETCODE_OK;

  thread_state_awake (ts1, &wr->m_entity.m_domain->gv);

  /* Serialize and write data or key */
  d = ddsi_serdata_from_sample (ddsi_wr->topic, writekey ? SDK_KEY : SDK_DATA, data);
  d->statusinfo = (((action & DDS_WR_DISPOSE_BIT) ? NN_STATUSINFO_DISPOSE : 0) |
                   ((action & DDS_WR_UNREGISTER_BIT) ? NN_STATUSINFO_UNREGISTER : 0));
  d->timestamp.v = tstamp;
  ddsi_serdata_ref (d);
  tk = ddsi_tkmap_lookup_instance_ref (wr->m_entity.m_domain->gv.m_tkmap, d);
  w_rc = write_sample_gc (ts1, wr->m_xp, ddsi_wr, d, tk);

  if (w_rc >= 0) {
    /* Flush out write unless configured to batch */
    if (!wr->whc_batch)
      nn_xpack_send (wr->m_xp, false);
    ret = DDS_RETCODE_OK;
  } else if (w_rc == DDS_RETCODE_TIMEOUT) {
    ret = DDS_RETCODE_TIMEOUT;
  } else if (w_rc == DDS_RETCODE_BAD_PARAMETER) {
    ret = DDS_RETCODE_ERROR;
  } else {
    ret = DDS_RETCODE_ERROR;
  }
  if (ret == DDS_RETCODE_OK)
    ret = deliver_locally (ddsi_wr, d, tk);
  ddsi_serdata_unref (d);
  ddsi_tkmap_instance_unref (wr->m_entity.m_domain->gv.m_tkmap, tk);
  thread_state_asleep (ts1);
  return ret;
}

dds_return_t dds_writecdr_impl_lowlevel (struct writer *ddsi_wr, struct nn_xpack *xp, struct ddsi_serdata *d, bool flush)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  struct ddsi_tkmap_instance * tk;
  int ret = DDS_RETCODE_OK;
  int w_rc;

  thread_state_awake (ts1, ddsi_wr->e.gv);
  ddsi_serdata_ref (d);
  tk = ddsi_tkmap_lookup_instance_ref (ddsi_wr->e.gv->m_tkmap, d);
  w_rc = write_sample_gc (ts1, xp, ddsi_wr, d, tk);
  if (w_rc >= 0) {
    /* Flush out write unless configured to batch */
    if (flush && xp != NULL)
      nn_xpack_send (xp, false);
    ret = DDS_RETCODE_OK;
  } else if (w_rc == DDS_RETCODE_TIMEOUT) {
    ret = DDS_RETCODE_TIMEOUT;
  } else if (w_rc == DDS_RETCODE_BAD_PARAMETER) {
    ret = DDS_RETCODE_ERROR;
  } else {
    ret = DDS_RETCODE_ERROR;
  }

  if (ret == DDS_RETCODE_OK)
    ret = deliver_locally (ddsi_wr, d, tk);
  ddsi_serdata_unref (d);
  ddsi_tkmap_instance_unref (ddsi_wr->e.gv->m_tkmap, tk);
  thread_state_asleep (ts1);
  return ret;
}

dds_return_t dds_writecdr_impl (dds_writer *wr, struct ddsi_serdata *d, dds_time_t tstamp, dds_write_action action)
{
  if (wr->m_topic->filter_fn)
    abort ();
  /* Set if disposing or unregistering */
  d->statusinfo = (((action & DDS_WR_DISPOSE_BIT) ? NN_STATUSINFO_DISPOSE : 0) |
                   ((action & DDS_WR_UNREGISTER_BIT) ? NN_STATUSINFO_UNREGISTER : 0));
  d->timestamp.v = tstamp;
  return dds_writecdr_impl_lowlevel (wr->m_wr, wr->m_xp, d, !wr->whc_batch);
}

void dds_write_flush (dds_entity_t writer)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  dds_writer *wr;
  dds_return_t rc;
  if ((rc = dds_writer_lock (writer, &wr)) == DDS_RETCODE_OK)
  {
    thread_state_awake (ts1, &wr->m_entity.m_domain->gv);
    nn_xpack_send (wr->m_xp, true);
    thread_state_asleep (ts1);
    dds_writer_unlock (wr);
  }
}
