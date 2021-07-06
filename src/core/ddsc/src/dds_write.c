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

#ifdef DDS_HAS_SHM
#include "dds/ddsi/shm_sync.h"
#endif

#ifdef DDS_HAS_SHM
struct AlignIceOryxChunk_t {
  iceoryx_header_t header;
  uint64_t worst_case_member;
};

static void register_pub_loan(dds_writer *wr, void *pub_loan)
{
  for (uint32_t i = 0; i < MAX_PUB_LOANS; ++i)
  {
    if (!wr->m_iox_pub_loans[i])
    {
      wr->m_iox_pub_loans[i] = pub_loan;
      return;
    }
  }
  /* The loan pool should be big enough to store the maximum number of open IceOryx loans.
   * So if IceOryx grants the loan, we should be able to store it.
   */
  assert(false);
}

static bool deregister_pub_loan(dds_writer *wr, const void *pub_loan)
{
    for (uint32_t i = 0; i < MAX_PUB_LOANS; ++i)
    {
      if (wr->m_iox_pub_loans[i] == pub_loan)
      {
        wr->m_iox_pub_loans[i] = NULL;
        return true;
      }
    }
    return false;
}

static void *create_iox_chunk(dds_writer *wr)
{
    iceoryx_header_t *ice_hdr;
    void *sample;
    uint32_t sample_size = wr->m_topic->m_stype->iox_size;
    uint32_t chunk_size = DETERMINE_ICEORYX_CHUNK_SIZE(sample_size);
    while (1)
    {
      enum iox_AllocationResult alloc_result = iox_pub_loan_chunk(wr->m_iox_pub, (void **) &ice_hdr, chunk_size);
      if (AllocationResult_SUCCESS == alloc_result)
        break;
      // SHM_TODO: Maybe there is a better way to do while unable to allocate.
      //           BTW, how long I should sleep is also another problem.
      dds_sleepfor (DDS_MSECS (1));
    }
    ice_hdr->data_size = sample_size;
    sample = SHIFT_PAST_ICEORYX_HEADER(ice_hdr);
    return sample;
}

static void release_iox_chunk(dds_writer *wr, void *sample)
{
  iceoryx_header_t *ice_hdr = SHIFT_BACK_TO_ICEORYX_HEADER(sample);
  iox_pub_release_chunk(wr->m_iox_pub, ice_hdr);
}
#endif

dds_return_t dds_loan_sample(dds_entity_t writer, void** sample)
{
#ifndef DDS_HAS_SHM
  (void) writer;
  (void) sample;
  return DDS_RETCODE_UNSUPPORTED;
#else
  dds_return_t ret;
  dds_writer *wr;

  if (!sample)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;

  if (wr->m_iox_pub)
  {
    *sample = create_iox_chunk(wr);
    register_pub_loan(wr, *sample);
  } else {
    ret = DDS_RETCODE_UNSUPPORTED;
  }
  dds_writer_unlock (wr);
  return ret;
#endif
}

dds_return_t dds_return_writer_loan(dds_writer *writer, void *sample)
{
#ifndef DDS_HAS_SHM
  (void) writer;
  (void) sample;
  return DDS_RETCODE_UNSUPPORTED;
#else
  dds_return_t ret;

  if (!sample)
    return DDS_RETCODE_BAD_PARAMETER;

  ddsrt_mutex_lock (&writer->m_entity.m_mutex);

  if (writer->m_iox_pub)
  {
    if (deregister_pub_loan(writer, sample)) {
      release_iox_chunk(writer, sample);
      ret = DDS_RETCODE_OK;
    } else {
      ret = DDS_RETCODE_PRECONDITION_NOT_MET;
    }
  } else {
    ret = DDS_RETCODE_UNSUPPORTED;
  }
  ddsrt_mutex_unlock (&writer->m_entity.m_mutex);
  return ret;
#endif
}

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
  if (wr->m_topic->m_filter.mode != DDS_TOPIC_FILTER_NONE)
  {
    dds_writer_unlock (wr);
    return DDS_RETCODE_ERROR;
  }
  serdata->statusinfo = 0;
  serdata->timestamp.v = dds_time ();
  ret = dds_writecdr_impl (wr, wr->m_xp, serdata, !wr->whc_batch);
  dds_writer_unlock (wr);
  return ret;
}

dds_return_t dds_forwardcdr (dds_entity_t writer, struct ddsi_serdata *serdata)
{
  dds_return_t ret;
  dds_writer *wr;

  if (serdata == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  if ((ret = dds_writer_lock (writer, &wr)) != DDS_RETCODE_OK)
    return ret;
  if (wr->m_topic->m_filter.mode != DDS_TOPIC_FILTER_NONE)
  {
    dds_writer_unlock (wr);
    return DDS_RETCODE_ERROR;
  }
  ret = dds_writecdr_impl (wr, wr->m_xp, serdata, !wr->whc_batch);
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
  const struct ddsi_sertype *src_type;
  struct ddsi_serdata *src_payload;
  struct ddsi_tkmap_instance *src_tk;
  ddsrt_mtime_t timeout;
};

static struct ddsi_serdata *local_make_sample (struct ddsi_tkmap_instance **tk, struct ddsi_domaingv *gv, struct ddsi_sertype const * const type, void *vsourceinfo)
{
  struct local_sourceinfo *si = vsourceinfo;
  struct ddsi_serdata *d = ddsi_serdata_ref_as_type (type, si->src_payload);
  if (d == NULL)
  {
    DDS_CWARNING (&gv->logconfig, "local: deserialization %s failed in type conversion\n", type->type_name);
    return NULL;
  }
  if (type != si->src_type)
    *tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, d);
  else
  {
    // if the type is the same, we can avoid the lookup
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
    .src_type = wr->type,
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
  struct ddsi_serdata *d;
  dds_return_t ret = DDS_RETCODE_OK;
  int w_rc;

  if (data == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  /* Check for topic filter */
  if (!writekey && wr->m_topic->m_filter.mode != DDS_TOPIC_FILTER_NONE)
  {
    const struct dds_topic_filter *f = &wr->m_topic->m_filter;
    switch (f->mode)
    {
      case DDS_TOPIC_FILTER_NONE:
      case DDS_TOPIC_FILTER_SAMPLEINFO_ARG:
        break;
      case DDS_TOPIC_FILTER_SAMPLE:
        if (!f->f.sample (data))
          return DDS_RETCODE_OK;
        break;
      case DDS_TOPIC_FILTER_SAMPLE_ARG:
        if (!f->f.sample_arg (data, f->arg))
          return DDS_RETCODE_OK;
        break;
      case DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG: {
        struct dds_sample_info si;
        memset (&si, 0, sizeof (si));
        if (!f->f.sample_sampleinfo_arg (data, &si, f->arg))
          return DDS_RETCODE_OK;
        break;
      }
    }
  }

  thread_state_awake (ts1, &wr->m_entity.m_domain->gv);

  /* Serialize and write data or key */
  if ((d = ddsi_serdata_from_sample (ddsi_wr->type, writekey ? SDK_KEY : SDK_DATA, data)) == NULL)
    ret = DDS_RETCODE_BAD_PARAMETER;
  else
  {
    struct ddsi_tkmap_instance *tk;
    d->statusinfo = (((action & DDS_WR_DISPOSE_BIT) ? NN_STATUSINFO_DISPOSE : 0) |
                     ((action & DDS_WR_UNREGISTER_BIT) ? NN_STATUSINFO_UNREGISTER : 0));
    d->timestamp.v = tstamp;
    ddsi_serdata_ref (d);

    bool suppress_local_delivery = false;
#ifdef DDS_HAS_SHM
    if (wr->m_iox_pub)
    {
      iceoryx_header_t *ice_hdr;

      if (!deregister_pub_loan(wr, data))
      {
        void *pub_loan;

        pub_loan = create_iox_chunk(wr);
        memcpy (pub_loan, data, wr->m_topic->m_stype->iox_size);
        data = pub_loan;
      }
      ice_hdr = SHIFT_BACK_TO_ICEORYX_HEADER(data);
      ice_hdr->guid = ddsi_wr->e.guid;
      ice_hdr->tstamp = tstamp;
      ice_hdr->statusinfo = d->statusinfo;
      ice_hdr->data_kind = writekey ? SDK_KEY : SDK_DATA;
      ddsi_serdata_get_keyhash(d, &ice_hdr->keyhash, false);
      iox_pub_publish_chunk (wr->m_iox_pub, ice_hdr);

      // Iceoryx will do delivery to local subscriptions, so we suppress it here
      suppress_local_delivery = true;
    }
    else
    {

    }
#endif

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
    if (ret == DDS_RETCODE_OK && !suppress_local_delivery)
      ret = deliver_locally (ddsi_wr, d, tk);
    ddsi_serdata_unref (d);
    ddsi_tkmap_instance_unref (wr->m_entity.m_domain->gv.m_tkmap, tk);
  }
  thread_state_asleep (ts1);
  return ret;
}

static dds_return_t dds_writecdr_impl_common (struct writer *ddsi_wr, struct nn_xpack *xp, struct ddsi_serdata *dinp, bool flush, dds_writer *wr)
{
  // consumes 1 refc from dinp in all paths (weird, but ... history ...)
  struct thread_state1 * const ts1 = lookup_thread_state ();
  struct ddsi_tkmap_instance *tk;
  struct ddsi_serdata *dact;
  int ret = DDS_RETCODE_OK;
  int w_rc;

  if (ddsi_wr->type == dinp->type)
  {
    dact = dinp;
    // dact refc: must consume 1
    // dinp refc: must consume 0 (it is an alias of dact)
  }
  else if (dinp->type->ops->version == ddsi_sertype_v0)
  {
    // deliberately allowing mismatches between d->type and ddsi_wr->type:
    // that way we can allow transferring data from one domain to another
    dact = ddsi_serdata_ref_as_type (ddsi_wr->type, dinp);
    // dact refc: must consume 1
    // dinp refc: must consume 1 (independent of dact: types are distinct)
  }
  else
  {
    // hope for the best (the type checks/conversions were missing in the
    // sertopic days anyway, so this is simply bug-for-bug compatibility
    dact = ddsi_sertopic_wrap_serdata (ddsi_wr->type, dinp->kind, dinp);
    // dact refc: must consume 1
    // dinp refc: must consume 1
  }

  if (dact == NULL)
  {
    // dinp may not be NULL, so this means something bad happened
    // still must drop a dinp reference
    ddsi_serdata_unref (dinp);
    return DDS_RETCODE_ERROR;
  }

  thread_state_awake (ts1, ddsi_wr->e.gv);

  // retain dact until after write_sample_gc so we can still pass it
  // to deliver_locally
  ddsi_serdata_ref (dact);

  tk = ddsi_tkmap_lookup_instance_ref (ddsi_wr->e.gv->m_tkmap, dact);
  // write_sample_gc always consumes 1 refc from dact
  w_rc = write_sample_gc (ts1, xp, ddsi_wr, dact, tk);
  if (w_rc >= 0)
  {
    /* Flush out write unless configured to batch */
    if (flush && xp != NULL)
      nn_xpack_send (xp, false);
    ret = DDS_RETCODE_OK;
  }
  else
  {
    if (w_rc == DDS_RETCODE_TIMEOUT)
      ret = DDS_RETCODE_TIMEOUT;
    else if (w_rc == DDS_RETCODE_BAD_PARAMETER)
      ret = DDS_RETCODE_ERROR;
    else
      ret = DDS_RETCODE_ERROR;
  }

  bool suppress_local_delivery = false;
#ifdef DDS_HAS_SHM
  if (wr && ret == DDS_RETCODE_OK) {
    // Currently, Iceoryx is enabled only for volatile data, so data gets stored in the WHC only
    // if remote subscribers exist, and in that case, at the moment that forces serialization of
    // the data.  So dropping iox_chunk is survivable.
    if (wr->m_iox_pub != NULL && dinp->iox_chunk != NULL)
    {
      iceoryx_header_t * ice_hdr = dinp->iox_chunk;
      // Local readers go through Iceoryx as well (because the Iceoryx support code doesn't exclude
      // that), which means we should suppress the internal path
      suppress_local_delivery = true;
      ice_hdr->guid = ddsi_wr->e.guid;
      ice_hdr->tstamp = dinp->timestamp.v;
      ice_hdr->statusinfo = dinp->statusinfo;
      ice_hdr->data_kind = (unsigned char)dinp->kind;
      ddsi_serdata_get_keyhash (dinp, &ice_hdr->keyhash, false);
      // iox_pub_publish_chunk takes ownership, storing a null pointer here doesn't
      // preclude the existence of race conditions on this, but it certainly improves
      // the chances of detecting them
      dinp->iox_chunk = NULL;
      iox_pub_publish_chunk (wr->m_iox_pub, ice_hdr);
    }
  }
#else
  (void) wr;
#endif

  if (ret == DDS_RETCODE_OK && !suppress_local_delivery)
    ret = deliver_locally (ddsi_wr, dact, tk);

  // refc management at input is such that we must still consume a dinp
  // reference if it isn't identical to dact; doing it prior to dropping
  // a reference to dact means we're not testing pointers to freed memory
  // (which is undefined behaviour IIRC).
  if (dact != dinp)
    ddsi_serdata_unref (dinp);

  // balance ddsi_serdata_ref
  ddsi_serdata_unref (dact);

  ddsi_tkmap_instance_unref (ddsi_wr->e.gv->m_tkmap, tk);
  thread_state_asleep (ts1);
  return ret;
}

dds_return_t dds_writecdr_impl (dds_writer *wr, struct nn_xpack *xp, struct ddsi_serdata *dinp, bool flush)
{
  return dds_writecdr_impl_common (wr->m_wr, xp, dinp, flush, wr);
}

dds_return_t dds_writecdr_local_orphan_impl (struct local_orphan_writer *lowr, struct nn_xpack *xp, struct ddsi_serdata *dinp)
{
  return dds_writecdr_impl_common (&lowr->wr, xp, dinp, true, NULL);
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
