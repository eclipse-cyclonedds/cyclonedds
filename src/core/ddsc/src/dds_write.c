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
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_deliver_locally.h"

#include "dds/ddsc/dds_loan_api.h"
#include "dds__loan.h"

#ifdef DDS_HAS_SHM
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds/ddsi/ddsi_shm_transport.h"
#include "dds/ddsi/q_addrset.h"
#endif

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
      iox_chunk_header_t* chunk_header = iox_chunk_header_from_user_payload(d->iox_chunk);
      iceoryx_header_t *ice_hdr = iox_chunk_header_to_user_header(chunk_header);

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

      iox_pub_publish_chunk (wr->m_iox_pub, d->iox_chunk);
      d->iox_chunk = NULL;
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
  // din and d may alias each other
  // note: use those assignments instead of if-statement (jump) for efficiency
  void* iox_chunk = din->iox_chunk;
  din->iox_chunk = NULL;  
  d->iox_chunk = iox_chunk;    
#endif

  ret = deliver_data(ddsi_wr, wr, d, xp, flush); // d = din: refc(d) = r, otherwise refc(d) = 1

  if(d != din)
    ddsi_serdata_unref(din); // d != din: refc(din) = r - 1 as required, refc(d) unchanged
  ddsi_serdata_unref(d); // d = din: refc(d) = r - 1, otherwise refc(din) = r-1 and refc(d) = 0

  thread_state_asleep (ts1);
  return ret;
}

static bool evalute_topic_filter (const dds_writer *wr, const void *data, bool writekey)
{
  // false if data rejected by filter
  if (wr->m_topic->m_filter.mode == DDS_TOPIC_FILTER_NONE || writekey)
    return true;

  const struct dds_topic_filter *f = &wr->m_topic->m_filter;
  switch (f->mode)
  {
    case DDS_TOPIC_FILTER_NONE:
    case DDS_TOPIC_FILTER_SAMPLEINFO_ARG:
      break;
    case DDS_TOPIC_FILTER_SAMPLE:
      if (!f->f.sample (data))
        return false;
      break;
    case DDS_TOPIC_FILTER_SAMPLE_ARG:
      if (!f->f.sample_arg (data, f->arg))
        return false;
      break;
    case DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG: {
      struct dds_sample_info si;
      memset (&si, 0, sizeof (si));
      if (!f->f.sample_sampleinfo_arg (data, &si, f->arg))
        return false;
      break;
    }
  }
  return true;
}

#ifdef DDS_HAS_SHM
static size_t get_required_buffer_size(struct dds_topic *topic, const void *sample) {
  bool has_fixed_size_type = topic->m_stype->fixed_size;
  if (has_fixed_size_type) {
    return topic->m_stype->iox_size;
  }
  
  return ddsi_sertype_get_serialized_size(topic->m_stype, (void*) sample);
}

static bool fill_iox_chunk(dds_writer *wr, const void *sample, void *iox_chunk,
                           size_t sample_size) {
  bool has_fixed_size_type = wr->m_topic->m_stype->fixed_size;
  bool ret = true;
  iceoryx_header_t *iox_header = iceoryx_header_from_chunk(iox_chunk);
  if (has_fixed_size_type) {
    memcpy(iox_chunk, sample, sample_size);
    iox_header->shm_data_state = IOX_CHUNK_CONTAINS_RAW_DATA;
  } else {
    size_t size = iox_header->data_size;
    ret = ddsi_sertype_serialize_into(wr->m_wr->type, sample, iox_chunk, size);
    if(ret) {
      iox_header->shm_data_state = IOX_CHUNK_CONTAINS_SERIALIZED_DATA;
    } else {
       // data is in invalid state
       iox_header->shm_data_state = IOX_CHUNK_UNINITIALIZED;
    }
  }
  return ret;
}

// has to support two cases:
// 1) data is in an external buffer allocated on the stack or dynamically
// 2) data is in an iceoryx buffer obtained by dds_loan_sample
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
  if (!evalute_topic_filter (wr, data, writekey))
    return DDS_RETCODE_OK;

  thread_state_awake (ts1, &wr->m_entity.m_domain->gv);

  // 3. Check availability of iceoryx and reader status
  bool iceoryx_available = wr->m_iox_pub != NULL;  
  void *iox_chunk = NULL;

  if(iceoryx_available) {
    //note: whether the data was loaned cannot be determined in the non-iceoryx case currently
    if(!deregister_pub_loan(wr, data))
    {
      size_t required_size = get_required_buffer_size(wr->m_topic, data);
      if(required_size == SIZE_MAX) {
        ret = DDS_RETCODE_OUT_OF_RESOURCES;
        goto finalize_write;
      }
      iox_chunk = shm_create_chunk(wr->m_iox_pub, required_size);

      if(iox_chunk) {
        if(!fill_iox_chunk(wr, data, iox_chunk, required_size)) {
          // serialization failed
          ret = DDS_RETCODE_BAD_PARAMETER;
          goto release_chunk;
        }
      } else {
        // we failed to obtain a chunk, iceoryx transport is thus not available
        // we cannot use the network path is now since due to the locators
        // readers on the same machine would not get the data
        // TODO: proper fallback logic to network path?

        // iceoryx_available = false;
        ret = DDS_RETCODE_OUT_OF_RESOURCES;
        goto finalize_write;      
      } 
    } else {
      // The user already provided an iceoryx_chunk with data (by using the dds_loan API)
      // We assume if we got the data from a loan it contains raw data
      // (i.e. not serialized)
      // This requires the user to adhere to the contract, we cannot enforce this
      // with the given API.
      iox_chunk = (void*) data;
      iceoryx_header_t *iox_header = iceoryx_header_from_chunk(iox_chunk);
      iox_header->shm_data_state = IOX_CHUNK_CONTAINS_RAW_DATA;      
    }
  }

  // The following holds from here
  // 1) data still points to the original data
  // 2) iox_chunk is NULL or
  // 3) iox_chunk is NOT NULL
  //    a) was created by the write call or
  //    b) data is an iceoryx chunk and iox_chunk == data
  // in case 3 a) iox_chunk will contain serialized or raw data
  //
  // in case 3 a) and b) we must ensure we publish or release the chunk (failure)
  // in 3b) we could argue to not release the chunk but the user effectively passed ownership
  //        by calling write

  // ddsi_wr->as can be changed by the matching/unmatching of proxy readers if we don't hold the lock
  // it is rather unfortunate that this then means we have to lock here to check, then lock again to
  // actually distribute the data, so some further refactoring is needed.
  ddsrt_mutex_lock (&ddsi_wr->e.lock);
  bool no_network_readers = addrset_empty (ddsi_wr->as);
  ddsrt_mutex_unlock (&ddsi_wr->e.lock);
  bool use_only_iceoryx =
      iceoryx_available && no_network_readers &&
      ddsi_wr->xqos->durability.kind == DDS_DURABILITY_VOLATILE;

  // 4. Prepare serdata
  // avoid serialization for volatile writers if there are no network readers

  struct ddsi_serdata *d = NULL;

  if(use_only_iceoryx) {
    // note: If we could keep ownership of the loaned data after iox publish we could implement lazy
    // serialization (only serializing when sending from writer history cache, i.e. not when storing).
    // The benefit of this would be minor in most cases though, when we assume a static configuration
    // where we either have network readers (requiring serialization) or not.

    // do not serialize yet (may not need it if only using iceoryx or no readers)
    d = ddsi_serdata_from_loaned_sample (ddsi_wr->type, writekey ? SDK_KEY : SDK_DATA, iox_chunk);
    if(d == NULL) {
      ret = DDS_RETCODE_BAD_PARAMETER;
      goto release_chunk;
    }   
  } else {
    // serialize for network since we will need to send via network anyway
    // we also need to serialize into an iceoryx chunk
 
    d = ddsi_serdata_from_sample (ddsi_wr->type, writekey ? SDK_KEY : SDK_DATA, data);
    if(d == NULL) {
      ret = DDS_RETCODE_BAD_PARAMETER;
      goto release_chunk;
    }

    // Needed for the mixed case where serdata d was created for the network path
    // but iceoryx can also be used.
    // In this case d was created by ddsi_serdata_from_sample and we need
    // to set the iceoryx chunk.
    if(iceoryx_available) {
      d->iox_chunk = iox_chunk;
    } else {
      d->iox_chunk = NULL;
    }  
  }

  // refc(d) = 1 after successful construction

  d->statusinfo = (((action & DDS_WR_DISPOSE_BIT) ? NN_STATUSINFO_DISPOSE : 0) |
                  ((action & DDS_WR_UNREGISTER_BIT) ? NN_STATUSINFO_UNREGISTER : 0));
  d->timestamp.v = tstamp;

  // 5. Deliver the data

  if(use_only_iceoryx) {
    // deliver via iceoryx only
    // TODO: can we avoid constructing d in this case?
    if(deliver_data_via_iceoryx(wr, d)) {
      ret = DDS_RETCODE_OK;
    } else {
      // Did not publish iox_chunk. We have to return the chunk (if any).      
      iox_pub_release_chunk(wr->m_iox_pub, d->iox_chunk);
      d->iox_chunk = NULL;
      ret = DDS_RETCODE_ERROR;
    }
    ddsi_serdata_unref(d); // refc(d) = 0
  } else {
    // this may convert the input data if needed (convert_serdata) and then deliver it using
    // network and/or iceoryx as required
    // d refc(d) = 1, call will reduce refcount by 1
    ret = dds_writecdr_impl_common(ddsi_wr, wr->m_xp, d, !wr->whc_batch, wr);

    if(ret != DDS_RETCODE_OK && d->iox_chunk) {                      
        iox_pub_release_chunk(wr->m_iox_pub, d->iox_chunk);
    }
  }

finalize_write:
  thread_state_asleep (ts1);
  return ret;

release_chunk:  
  if(iox_chunk) {    
    iox_pub_release_chunk(wr->m_iox_pub, iox_chunk);
  }
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
  if (!evalute_topic_filter(wr, data, writekey))
    return DDS_RETCODE_OK;

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
