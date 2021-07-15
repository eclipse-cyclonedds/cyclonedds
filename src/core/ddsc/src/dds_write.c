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
#include "dds/ddsi/q_addrset.h"
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

    // TODO: use a proper timeout to control the time it is allowed to take to obtain a chunk more accurately
    // but for now only try a limited number of times (hence non-blocking).
    // Otherwise we could block here forever and this also leads to problems with thread progress monitoring.

    int32_t number_of_trys = 10; //try 10 times over at least 10ms, considering the wait time below

    while (true)
    {
      enum iox_AllocationResult alloc_result = iox_pub_loan_chunk(wr->m_iox_pub, (void **) &ice_hdr, chunk_size);
      if (AllocationResult_SUCCESS == alloc_result)
        break;

      if(--number_of_trys <= 0) {
        return NULL;
      }

      dds_sleepfor (DDS_MSECS (1)); // TODO: how long should we wait?
    }
    ice_hdr->data_size = sample_size;
    sample = SHIFT_PAST_ICEORYX_HEADER(ice_hdr);
    return sample;
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

    if(*sample) {
      register_pub_loan(wr, *sample);
    } else {
      ret = DDS_RETCODE_ERROR; // could not obtain a sample
    }
    
  } else {
    ret = DDS_RETCODE_UNSUPPORTED;
  }
  dds_writer_unlock (wr);
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

#if DDS_HAS_SHM
static bool deliver_data_via_iceoryx(dds_writer *wr, struct ddsi_serdata *d) {
    if (wr->m_iox_pub != NULL && d->iox_chunk != NULL)
    {
      iceoryx_header_t * ice_hdr = d->iox_chunk;
      // Local readers go through Iceoryx as well (because the Iceoryx support code doesn't exclude
      // that), which means we should suppress the internal path
      ice_hdr->guid = wr->m_wr->e.guid;
      ice_hdr->tstamp = d->timestamp.v;
      ice_hdr->statusinfo = d->statusinfo;
      ice_hdr->data_kind = (unsigned char)d->kind;
      ddsi_serdata_get_keyhash (d, &ice_hdr->keyhash, false);
      // iox_pub_publish_chunk takes ownership, storing a null pointer here doesn't
      // preclude the existence of race conditions on this, but it certainly improves
      // the chances of detecting them
      d->iox_chunk = NULL;
      iox_pub_publish_chunk (wr->m_iox_pub, ice_hdr);
      return true; //we published the chunk
    }
    return false; // we did not publish the chunk
}
#endif

static struct ddsi_serdata *convert_serdata(struct writer *ddsi_wr, struct ddsi_serdata *din) {
  struct ddsi_serdata *dout;
  if (ddsi_wr->type == din->type)
  {
    dout = din;
    // dout refc: must consume 1
    // din refc: must consume 0 (it is an alias of dact)
  }
  else if (din->type->ops->version == ddsi_sertype_v0)
  {
    // deliberately allowing mismatches between d->type and ddsi_wr->type:
    // that way we can allow transferring data from one domain to another
    dout = ddsi_serdata_ref_as_type (ddsi_wr->type, din);
    // dout refc: must consume 1
    // din refc: must consume 1 (independent of dact: types are distinct)
  }
  else
  {
    // hope for the best (the type checks/conversions were missing in the
    // sertopic days anyway, so this is simply bug-for-bug compatibility
    dout = ddsi_sertopic_wrap_serdata (ddsi_wr->type, din->kind, din);
    // dout refc: must consume 1
    // din refc: must consume 1
  }
  return dout;
}

static dds_return_t deliver_data (struct writer *ddsi_wr, dds_writer *wr, struct ddsi_serdata *d, struct nn_xpack *xp, bool flush) {
  struct thread_state1 * const ts1 = lookup_thread_state ();

  struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref (ddsi_wr->e.gv->m_tkmap, d);
  // write_sample_gc always consumes 1 refc from d
  int ret = write_sample_gc (ts1, xp, ddsi_wr, d, tk);
  if (ret >= 0)
  {
    /* Flush out write unless configured to batch */
    if (flush && xp != NULL)
      nn_xpack_send (xp, false);
    ret = DDS_RETCODE_OK;
  }
  else
  {
    if (ret != DDS_RETCODE_TIMEOUT)
      ret = DDS_RETCODE_ERROR;
  }

  bool suppress_local_delivery = false;
#ifdef DDS_HAS_SHM
  if (wr && ret == DDS_RETCODE_OK) {
    //suppress if we successfully sent it via iceoryx
    suppress_local_delivery = deliver_data_via_iceoryx(wr, d);
  }
#else
  (void) wr;
#endif

  if (ret == DDS_RETCODE_OK && !suppress_local_delivery)
    ret = deliver_locally (ddsi_wr, d, tk);

  ddsi_tkmap_instance_unref (ddsi_wr->e.gv->m_tkmap, tk);

  return ret;
}

static dds_return_t dds_writecdr_impl_common (struct writer *ddsi_wr, struct nn_xpack *xp, struct ddsi_serdata *din, bool flush, dds_writer *wr)
{
  // consumes 1 refc from din in all paths (weird, but ... history ...)
  // let refc(din) be r, so upon returning it must be r-1
  struct thread_state1 * const ts1 = lookup_thread_state ();
  int ret = DDS_RETCODE_OK;

  struct ddsi_serdata *d = convert_serdata(ddsi_wr, din);

  if (d == NULL)
  {  
    ddsi_serdata_unref(din); // refc(din) = r - 1 as required
    return DDS_RETCODE_ERROR;
  }

  // d = din: refc(d) = r, otherwise refc(d) = 1

  thread_state_awake (ts1, ddsi_wr->e.gv);
  ddsi_serdata_ref(d); // d = din: refc(d) = r + 1, otherwise refc(d) = 2

#ifdef DDS_HAS_SHM
  // transfer ownership of an iceoryx chunk if it exists
  d->iox_chunk = din->iox_chunk;
  din->iox_chunk = NULL;
#endif

  ret = deliver_data(ddsi_wr, wr, d, xp, flush); // d = din: refc(d) = r, otherwise refc(d) = 1

  if(d != din)
    ddsi_serdata_unref(din); // d != din: refc(din) = r - 1 as required, refc(d) unchanged
  ddsi_serdata_unref(d); // d = din: refc(d) = r - 1, otherwise refc(din) = r-1 and refc(d) = 0
  
  thread_state_asleep (ts1);
  return ret;
}

#ifdef DDS_HAS_SHM
dds_return_t dds_write_impl (dds_writer *wr, const void * data, dds_time_t tstamp, dds_write_action action)
{
  // 1. Input validation
  struct thread_state1 * const ts1 = lookup_thread_state ();
  const bool writekey = action & DDS_WR_KEY_BIT;
  struct writer *ddsi_wr = wr->m_wr;
  int ret = DDS_RETCODE_OK;

  if (data == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  // 2. Topic filter
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

  // 3. Check availability of iceoryx and reader status

  bool iceoryx_available = wr->m_iox_pub != NULL;

  if(iceoryx_available) {
    //note: whether the data was loaned cannot be determined in the non-iceoryx case currently
    if(!deregister_pub_loan(wr, data))
    {
      void* chunk_data = create_iox_chunk(wr);
      if(chunk_data) {
        memcpy (chunk_data, data, wr->m_topic->m_stype->iox_size);
        data = chunk_data; // note that this points to the data in the chunk which is preceded by the iceoryx header
      } else {
        // we failed to obtain a chunk, iceoryx transport is thus not available
        // we will use the network path instead
        iceoryx_available = false;
      } 
    } 
  }

  bool no_network_readers = addrset_empty (ddsi_wr->as);
  bool use_only_iceoryx = iceoryx_available && no_network_readers;

  // iceoryx_available implies volatile 
  // otherwise we need to add the check in the use_only_iceoryx expression
  assert(!iceoryx_available || ddsi_wr->xqos->durability.kind == DDS_DURABILITY_VOLATILE);

  // 4. Prepare serdata
  // avoid serialization for volatile writers if there are no network readers

  struct ddsi_serdata *d = NULL;

  if(use_only_iceoryx) {
    // note: If we could keep ownership of the loaned data after iox publish we could implement lazy
    // serialization (only serializing when sending from writer history cache, i.e. not when storing).
    // The benefit of this would be minor in most cases though, when we assume a static configuration
    // where we either have network readers (requiring serialization) or not.

    // do not serialize yet (may not need it if only using iceoryx or no readers)
    d = ddsi_serdata_from_loaned_sample (ddsi_wr->type, writekey ? SDK_KEY : SDK_DATA, data);
    //d = ddsi_serdata_from_sample (ddsi_wr->type, writekey ? SDK_KEY : SDK_DATA, data);
  } else {
    // serialize since we will need to send via network anyway
    d = ddsi_serdata_from_sample (ddsi_wr->type, writekey ? SDK_KEY : SDK_DATA, data);
  }

  if(d == NULL) {
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto finalize_write;
  }

  // refc(d) = 1 after successful construction

  // should ideally be done in the serdata construction but we explicitly set it here for now
  if(iceoryx_available) {
    d->iox_chunk = SHIFT_BACK_TO_ICEORYX_HEADER(data); // maybe serialization was performed
  } else {
    d->iox_chunk = NULL; // also indicates that serialization has been performed
  }

  d->statusinfo = (((action & DDS_WR_DISPOSE_BIT) ? NN_STATUSINFO_DISPOSE : 0) |
                  ((action & DDS_WR_UNREGISTER_BIT) ? NN_STATUSINFO_UNREGISTER : 0));
  d->timestamp.v = tstamp;

  // 5. Deliver the data

  if(use_only_iceoryx) {
    // deliver via iceoryx only
    if(deliver_data_via_iceoryx(wr, d)) {
      ret = DDS_RETCODE_OK;
    } else {
      ret = DDS_RETCODE_ERROR;
    }
    ddsi_serdata_unref(d); // refc(d) = 0
  } else {
    // this may convert the input data if needed (convert_serdata) and then deliver it using
    // network and/or iceoryx as required
    // d refc(d) = 1, call will reduce refcount by 1
    ret = dds_writecdr_impl_common(ddsi_wr, wr->m_xp, d, !wr->whc_batch, wr);
  }

finalize_write:
  thread_state_asleep (ts1);
  return ret;
}

#else

// implementation if no shared memory (iceoryx) is available
dds_return_t dds_write_impl (dds_writer *wr, const void * data, dds_time_t tstamp, dds_write_action action)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();
  const bool writekey = action & DDS_WR_KEY_BIT;
  struct writer *ddsi_wr = wr->m_wr;
  struct ddsi_serdata *d;
  dds_return_t ret = DDS_RETCODE_OK;

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

    tk = ddsi_tkmap_lookup_instance_ref (wr->m_entity.m_domain->gv.m_tkmap, d);
    ret = write_sample_gc (ts1, wr->m_xp, ddsi_wr, d, tk);

    if (ret >= 0) {
      /* Flush out write unless configured to batch */
      if (!wr->whc_batch)
        nn_xpack_send (wr->m_xp, false);
      ret = DDS_RETCODE_OK;
    } else if (ret != DDS_RETCODE_TIMEOUT) {
      ret = DDS_RETCODE_ERROR;
    } 

    if (ret == DDS_RETCODE_OK)
      ret = deliver_locally (ddsi_wr, d, tk);
    ddsi_serdata_unref (d);
    ddsi_tkmap_instance_unref (wr->m_entity.m_domain->gv.m_tkmap, tk);
  }
  thread_state_asleep (ts1);
  return ret;
}
#endif

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
