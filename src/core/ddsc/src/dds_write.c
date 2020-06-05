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
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_deliver_locally.h"

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

static struct reader *writer_first_in_sync_reader (struct entity_index *entity_index, struct entity_common *wrcmn, ddsrt_avl_iter_t *it)
{
  assert (wrcmn->kind == EK_WRITER);
  struct writer *wr = (struct writer *) wrcmn;
  struct wr_rd_match *m = ddsrt_avl_iter_first (&wr_local_readers_treedef, &wr->local_readers, it);
  return m ? entidx_lookup_reader_guid (entity_index, &m->rd_guid) : NULL;
}

static struct reader *writer_next_in_sync_reader (struct entity_index *entity_index, ddsrt_avl_iter_t *it)
{
  struct wr_rd_match *m = ddsrt_avl_iter_next (it);
  return m ? entidx_lookup_reader_guid (entity_index, &m->rd_guid) : NULL;
}

struct local_sourceinfo {
  const struct ddsi_sertopic *src_topic;
  struct ddsi_serdata *src_payload;
  struct ddsi_tkmap_instance *src_tk;
  ddsrt_mtime_t timeout;
};

static struct ddsi_serdata *local_make_sample (struct ddsi_tkmap_instance **tk, struct ddsi_domaingv *gv, struct ddsi_sertopic const * const topic, void *vsourceinfo)
{
  struct local_sourceinfo *si = vsourceinfo;
  struct ddsi_serdata *d = ddsi_serdata_ref_as_topic (topic, si->src_payload);
  if (d == NULL)
  {
    DDS_CWARNING (&gv->logconfig, "local: deserialization %s/%s failed in topic type conversion\n", topic->name, topic->type_name);
    return NULL;
  }
  if (topic != si->src_topic)
    *tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, d);
  else
  {
    // if the topic is the same, we can avoid the lookup
    ddsi_tkmap_instance_ref (si->src_tk);
    *tk = si->src_tk;
  }
  return d;
}

static dds_return_t local_on_delivery_failure_fastpath (struct entity_common *source_entity, bool source_entity_locked, struct local_reader_ary *fastpath_rdary, void *vsourceinfo)
{
  (void) fastpath_rdary;
  (void) source_entity_locked;
  assert (source_entity->kind == EK_WRITER);
  struct writer *wr = (struct writer *) source_entity;
  struct local_sourceinfo *si = vsourceinfo;
  ddsrt_mtime_t tnow = ddsrt_time_monotonic ();
  if (si->timeout.v == 0)
    si->timeout = ddsrt_mtime_add_duration (tnow, wr->xqos->reliability.max_blocking_time);
  if (tnow.v >= si->timeout.v)
    return DDS_RETCODE_TIMEOUT;
  else
  {
    dds_sleepfor (DDS_HEADBANG_TIMEOUT);
    return DDS_RETCODE_OK;
  }
}

static dds_return_t deliver_locally (struct writer *wr, struct ddsi_serdata *payload, struct ddsi_tkmap_instance *tk)
{
  static const struct deliver_locally_ops deliver_locally_ops = {
    .makesample = local_make_sample,
    .first_reader = writer_first_in_sync_reader,
    .next_reader = writer_next_in_sync_reader,
    .on_failure_fastpath = local_on_delivery_failure_fastpath
  };
  struct local_sourceinfo sourceinfo = {
    .src_topic = wr->topic,
    .src_payload = payload,
    .src_tk = tk,
    .timeout = { 0 },
  };
  dds_return_t rc;
  struct ddsi_writer_info wrinfo;
  ddsi_make_writer_info (&wrinfo, &wr->e, wr->xqos, payload->statusinfo);
  rc = deliver_locally_allinsync (wr->e.gv, &wr->e, false, &wr->rdary, &wrinfo, &deliver_locally_ops, &sourceinfo);
  if (rc == DDS_RETCODE_TIMEOUT)
    DDS_CERROR (&wr->e.gv->logconfig, "The writer could not deliver data on time, probably due to a local reader resources being full\n");
  return rc;
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
  if (dds_writer_lock (writer, &wr) == DDS_RETCODE_OK)
  {
    thread_state_awake (ts1, &wr->m_entity.m_domain->gv);
    nn_xpack_send (wr->m_xp, true);
    thread_state_asleep (ts1);
    dds_writer_unlock (wr);
  }
}
