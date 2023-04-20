// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>
#include "dds__writer.h"
#include "dds__write.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsi/ddsi_xmsg.h"
#include "dds/ddsi/ddsi_rhc.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds/ddsi/ddsi_transmit.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_endpoint_match.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/ddsi/ddsi_radmin.h" /* sampleinfo */
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_deliver_locally.h"

#include "dds/ddsc/dds_loan_api.h"
#include "dds__loan.h"

#ifdef DDS_HAS_SHM
#include "dds/ddsi/ddsi_shm_transport.h"
#include "dds/ddsi/ddsi_addrset.h"
#endif

struct ddsi_serdata_plain { struct ddsi_serdata p; };
struct ddsi_serdata_iox   { struct ddsi_serdata x; };
struct ddsi_serdata_any   { struct ddsi_serdata a; };

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

static dds_return_t local_on_delivery_failure_fastpath (struct ddsi_entity_common *source_entity, bool source_entity_locked, struct ddsi_local_reader_ary *fastpath_rdary, void *vsourceinfo)
{
  (void) fastpath_rdary;
  (void) source_entity_locked;
  assert (source_entity->kind == DDSI_EK_WRITER);
  struct ddsi_writer *wr = (struct ddsi_writer *) source_entity;
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

static dds_return_t deliver_locally (struct ddsi_writer *wr, struct ddsi_serdata *payload, struct ddsi_tkmap_instance *tk)
{
  static const struct ddsi_deliver_locally_ops deliver_locally_ops = {
    .makesample = local_make_sample,
    .first_reader = ddsi_writer_first_in_sync_reader,
    .next_reader = ddsi_writer_next_in_sync_reader,
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
  rc = ddsi_deliver_locally_allinsync (wr->e.gv, &wr->e, false, &wr->rdary, &wrinfo, &deliver_locally_ops, &sourceinfo);
  if (rc == DDS_RETCODE_TIMEOUT)
    DDS_CERROR (&wr->e.gv->logconfig, "The writer could not deliver data on time, probably due to a local reader resources being full\n");
  return rc;
}

#if DDS_HAS_SHM
static void deliver_data_via_iceoryx (dds_writer *wr, struct ddsi_serdata_iox *d)
{
  iox_chunk_header_t *chunk_header =
  iox_chunk_header_from_user_payload(d->x.iox_chunk);
  iceoryx_header_t *ice_hdr = iox_chunk_header_to_user_header(chunk_header);

  // Local readers go through Iceoryx as well (because the Iceoryx support
  // code doesn't exclude that), which means we should suppress the internal
  // path
  ice_hdr->guid = wr->m_wr->e.guid;
  ice_hdr->tstamp = d->x.timestamp.v;
  ice_hdr->statusinfo = d->x.statusinfo;
  ice_hdr->data_kind = (unsigned char)d->x.kind;
  ddsi_serdata_get_keyhash(&d->x, &ice_hdr->keyhash, false);
  // iox_pub_publish_chunk takes ownership, storing a null pointer here
  // doesn't preclude the existence of race conditions on this, but it
  // certainly improves the chances of detecting them
  iox_pub_publish_chunk(wr->m_iox_pub, d->x.iox_chunk);
  d->x.iox_chunk = NULL;
}
#endif

static struct ddsi_serdata_any *convert_serdata(struct ddsi_writer *ddsi_wr, struct ddsi_serdata_any *din)
{
  struct ddsi_serdata_any *dout;
  if (ddsi_wr->type == din->a.type)
  {
    dout = din;
    // dout refc: must consume 1
    // din refc: must consume 0 (it is an alias of dact)
  }
  else
  {
    assert (din->a.type->ops->version == ddsi_sertype_v0);
    // deliberately allowing mismatches between d->type and ddsi_wr->type:
    // that way we can allow transferring data from one domain to another
    dout = (struct ddsi_serdata_any *) ddsi_serdata_ref_as_type (ddsi_wr->type, &din->a);
    // dout refc: must consume 1
    // din refc: must consume 1 (independent of dact: types are distinct)
  }
  return dout;
}

static dds_return_t deliver_data_network (struct ddsi_thread_state * const thrst, struct ddsi_writer *ddsi_wr, struct ddsi_serdata_any *d, struct ddsi_xpack *xp, bool flush, struct ddsi_tkmap_instance *tk)
{
  // ddsi_write_sample_gc always consumes 1 refc from d
  int ret = ddsi_write_sample_gc (thrst, xp, ddsi_wr, &d->a, tk);
  if (ret >= 0)
  {
    /* Flush out write unless configured to batch */
    if (flush && xp != NULL)
      ddsi_xpack_send (xp, false);
    return DDS_RETCODE_OK;
  }
  else
  {
    return (ret == DDS_RETCODE_TIMEOUT) ? ret : DDS_RETCODE_ERROR;
  }
}

static dds_return_t deliver_data_any (struct ddsi_thread_state * const thrst, struct ddsi_writer *ddsi_wr, dds_writer *wr, struct ddsi_serdata_any *d, struct ddsi_xpack *xp, bool flush)
{
  struct ddsi_tkmap_instance * const tk = ddsi_tkmap_lookup_instance_ref (ddsi_wr->e.gv->m_tkmap, &d->a);
  dds_return_t ret;
  if ((ret = deliver_data_network (thrst, ddsi_wr, d, xp, flush, tk)) != DDS_RETCODE_OK)
  {
    ddsi_tkmap_instance_unref (ddsi_wr->e.gv->m_tkmap, tk);
    return ret;
  }
#ifdef DDS_HAS_SHM
  if (d->a.iox_chunk != NULL)
  {
    // delivers to all iceoryx readers, including local ones
    deliver_data_via_iceoryx (wr, (struct ddsi_serdata_iox *) d);
  }
#else
  (void) wr;
#endif
  ret = deliver_locally (ddsi_wr, &d->a, tk);
  ddsi_tkmap_instance_unref (ddsi_wr->e.gv->m_tkmap, tk);
  return ret;
}

static dds_return_t dds_writecdr_impl_common (struct ddsi_writer *ddsi_wr, struct ddsi_xpack *xp, struct ddsi_serdata_any *din, bool flush, dds_writer *wr)
{
  // consumes 1 refc from din in all paths (weird, but ... history ...)
  // let refc(din) be r, so upon returning it must be r-1
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  int ret = DDS_RETCODE_OK;
  assert (wr != NULL);

  struct ddsi_serdata_any * const d = convert_serdata(ddsi_wr, din);
  if (d == NULL)
  {
    ddsi_serdata_unref(&din->a); // refc(din) = r - 1 as required
    return DDS_RETCODE_ERROR;
  }

  // d = din: refc(d) = r, otherwise refc(d) = 1

  ddsi_thread_state_awake (thrst, ddsi_wr->e.gv);
  ddsi_serdata_ref (&d->a); // d = din: refc(d) = r + 1, otherwise refc(d) = 2

#ifdef DDS_HAS_SHM
  // transfer ownership of an iceoryx chunk if it exists
  // din and d may alias each other
  // note: use those assignments instead of if-statement (jump) for efficiency
  void* iox_chunk = din->a.iox_chunk;
  din->a.iox_chunk = NULL;
  d->a.iox_chunk = iox_chunk;
  assert ((wr->m_iox_pub == NULL) == (d->a.iox_chunk == NULL));
#endif

  ret = deliver_data_any (thrst, ddsi_wr, wr, d, xp, flush);

  if(d != din)
    ddsi_serdata_unref(&din->a); // d != din: refc(din) = r - 1 as required, refc(d) unchanged
  ddsi_serdata_unref(&d->a); // d = din: refc(d) = r - 1, otherwise refc(din) = r-1 and refc(d) = 0

  ddsi_thread_state_asleep (thrst);
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

static void set_statusinfo_timestamp (struct ddsi_serdata_any *d, dds_time_t tstamp, dds_write_action action)
{
  d->a.statusinfo = (((action & DDS_WR_DISPOSE_BIT) ? DDSI_STATUSINFO_DISPOSE : 0) |
                     ((action & DDS_WR_UNREGISTER_BIT) ? DDSI_STATUSINFO_UNREGISTER : 0));
  d->a.timestamp.v = tstamp;
}

#ifdef DDS_HAS_SHM
static size_t get_required_buffer_size(struct dds_topic *topic, const void *sample)
{
  bool has_fixed_size_type = topic->m_stype->fixed_size;
  if (has_fixed_size_type) {
    return topic->m_stype->iox_size;
  }

  return ddsi_sertype_get_serialized_size(topic->m_stype, (void*) sample);
}

static bool fill_iox_chunk(dds_writer *wr, const void *sample, void *iox_chunk, size_t sample_size)
{
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

static dds_return_t create_and_fill_chunk (dds_writer *wr, const void *data, void **iox_chunk)
{
  const size_t required_size = get_required_buffer_size(wr->m_topic, data);
  if (required_size == SIZE_MAX)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  if ((*iox_chunk = shm_create_chunk (wr->m_iox_pub, required_size)) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  if (!fill_iox_chunk (wr, data, *iox_chunk, required_size))
    return DDS_RETCODE_BAD_PARAMETER; // serialization failed
  return DDS_RETCODE_OK;
}

static dds_return_t get_iox_chunk (dds_writer *wr, const void *data, void **iox_chunk)
{
  //note: whether the data was loaned cannot be determined in the non-iceoryx case currently
  if (!dds_deregister_pub_loan (wr, data))
    return create_and_fill_chunk (wr, data, iox_chunk);
  else
  {
    // The user already provided an iceoryx_chunk with data (by using the dds_loan API)
    // We assume if we got the data from a loan it contains raw data
    // (i.e. not serialized)
    // This requires the user to adhere to the contract, we cannot enforce this
    // with the given API.
    *iox_chunk = (void *) data;
    iceoryx_header_t *iox_header = iceoryx_header_from_chunk (*iox_chunk);
    iox_header->shm_data_state = IOX_CHUNK_CONTAINS_RAW_DATA;
    return DDS_RETCODE_OK;
  }
}

// Synchronizes the current number of fast path readers and returns it.
// Locking the mutex is needed to synchronize the value.
// This number may change concurrently any time we do not hold the lock,
// i.e. become outdated when we return from the function.
static uint32_t get_num_fast_path_readers(struct ddsi_writer *ddsi_wr) {
  ddsrt_mutex_lock(&ddsi_wr->rdary.rdary_lock);
  uint32_t n = ddsi_wr->rdary.n_readers;
  ddsrt_mutex_unlock(&ddsi_wr->rdary.rdary_lock);
  return n;
}

// has to support two cases:
// 1) data is in an external buffer allocated on the stack or dynamically
// 2) data is in an iceoryx buffer obtained by dds_loan_sample
static dds_return_t dds_write_impl_iox (dds_writer *wr, struct ddsi_writer *ddsi_wr, bool writekey, const void *data, dds_time_t tstamp, dds_write_action action)
{
  assert (ddsi_thread_is_awake ());
  assert (wr != NULL && wr->m_iox_pub != NULL);

  void *iox_chunk = NULL;
  dds_return_t ret;

  //note: whether the data was loaned cannot be determined in the non-iceoryx case currently
  if ((ret = get_iox_chunk (wr, data, &iox_chunk)) != 0)
    return ret;
  assert (iox_chunk != NULL);

  // The following holds from here
  // 1) data still points to the original data
  // 2) iox_chunk is NOT NULL
  //    a) was created by the write call or
  //    b) data is an iceoryx chunk and iox_chunk == data
  // in case 2 a) iox_chunk will contain serialized or raw data
  //
  // in case 2 a) and b) we must ensure we publish or release the chunk (failure)
  // in 2b) we could argue to not release the chunk but the user effectively passed ownership
  //        by calling write

  // ddsi_wr->as can be changed by the matching/unmatching of proxy readers if we don't hold the lock
  // it is rather unfortunate that this then means we have to lock here to check, then lock again to
  // actually distribute the data, so some further refactoring is needed.
  ddsrt_mutex_lock (&ddsi_wr->e.lock);
  const bool no_network_readers = ddsi_addrset_empty (ddsi_wr->as);
  ddsrt_mutex_unlock (&ddsi_wr->e.lock);

  // NB: local readers are not in L := ddsi_wr->rdary if they use iceoryx.
  // Furthermore, all readers that ignore local publishers will not use iceoryx.
  // We will never use only(!) iceoryx if there is any local reader in L.
  // We will serialize the data in this case and deliver it mixed, i.e.
  // partially with iceoryx as required by the QoS and type. The readers in L
  // will get the data via the local delivery mechanism (fast path or slow
  // path).

  const uint32_t num_fast_path_readers = get_num_fast_path_readers(ddsi_wr);

  // If use_only_iceoryx is true, there were no fast path readers at the moment
  // we checked.
  // If fast path readers arive later, they may not get data but this
  // is fine as we can consider their connections not fully established
  // and hence they are not considered for data transfer.
  // The alternative is to block new fast path connections entirely (by holding
  // the mutex) until data delivery is complete.
  const bool use_only_iceoryx =
      no_network_readers &&
      ddsi_wr->xqos->durability.kind == DDS_DURABILITY_VOLATILE &&
      num_fast_path_readers == 0;

  // 4. Prepare serdata
  // avoid serialization for volatile writers if there are no network readers

  struct ddsi_serdata_iox *d = NULL;
  if (use_only_iceoryx)
  {
    // note: If we could keep ownership of the loaned data after iox publish we could implement lazy
    // serialization (only serializing when sending from writer history cache, i.e. not when storing).
    // The benefit of this would be minor in most cases though, when we assume a static configuration
    // where we either have network readers (requiring serialization) or not.

    // do not serialize yet (may not need it if only using iceoryx or no readers)
    d = (struct ddsi_serdata_iox *) ddsi_serdata_from_loaned_sample (ddsi_wr->type, writekey ? SDK_KEY : SDK_DATA, iox_chunk);
  }
  else
  {
    // serialize for network since we will need to send via network anyway
    // we also need to serialize into an iceoryx chunk

    struct ddsi_serdata *dtmp = ddsi_serdata_from_sample (ddsi_wr->type, writekey ? SDK_KEY : SDK_DATA, data);
    if (dtmp != NULL)
    {
      // Needed for the mixed case where serdata d was created for the network path
      // but iceoryx can also be used.
      // In this case d was created by ddsi_serdata_from_sample and we need
      // to set the iceoryx chunk.
      dtmp->iox_chunk = iox_chunk;
      d = (struct ddsi_serdata_iox *) dtmp;
    }
  }
  if (d == NULL)
  {
    iox_pub_release_chunk (wr->m_iox_pub, iox_chunk);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  assert (d->x.iox_chunk != NULL);

  // refc(d) = 1 after successful construction
  set_statusinfo_timestamp ((struct ddsi_serdata_any *) d, tstamp, action);

  // 5. Deliver the data
  if(use_only_iceoryx) {
    // deliver via iceoryx only
    // TODO: can we avoid constructing d in this case?
    // There are no local non-iceoryx readers in this case.
    deliver_data_via_iceoryx (wr, d);
    ddsi_serdata_unref (&d->x); // refc(d) = 0
  } else {
    // this may convert the input data if needed (convert_serdata) and then deliver it using
    // network and/or iceoryx as required
    // d refc(d) = 1, call will reduce refcount by 1
    ret = dds_writecdr_impl_common (ddsi_wr, wr->m_xp, (struct ddsi_serdata_any *) d, !wr->whc_batch, wr);
    if (ret != DDS_RETCODE_OK)
      iox_pub_release_chunk (wr->m_iox_pub, d->x.iox_chunk);
  }
  return ret;
}
#endif

static dds_return_t dds_write_impl_plain (dds_writer *wr, struct ddsi_writer *ddsi_wr, bool writekey, const void *data, dds_time_t tstamp, dds_write_action action)
{
  assert (ddsi_thread_is_awake ());
#ifdef DDS_HAS_SHM
  assert (wr->m_iox_pub == NULL);
#endif

  struct ddsi_serdata_plain *d = NULL;
  d = (struct ddsi_serdata_plain *) ddsi_serdata_from_sample (ddsi_wr->type, writekey ? SDK_KEY : SDK_DATA, data);
  if (d == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  set_statusinfo_timestamp ((struct ddsi_serdata_any *) d, tstamp, action);
  return dds_writecdr_impl_common(ddsi_wr, wr->m_xp, (struct ddsi_serdata_any *) d, !wr->whc_batch, wr);
}

// has to support two cases:
// 1) data is in an external buffer allocated on the stack or dynamically
// 2) data is in an iceoryx buffer obtained by dds_loan_sample
dds_return_t dds_write_impl (dds_writer *wr, const void * data, dds_time_t tstamp, dds_write_action action)
{
  // 1. Input validation
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  const bool writekey = action & DDS_WR_KEY_BIT;
  struct ddsi_writer *ddsi_wr = wr->m_wr;
  int ret = DDS_RETCODE_OK;

  if (data == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  // 2. Topic filter
  if (!evalute_topic_filter (wr, data, writekey))
    return DDS_RETCODE_OK;

  ddsi_thread_state_awake (thrst, &wr->m_entity.m_domain->gv);
#ifdef DDS_HAS_SHM
  if (wr->m_iox_pub)
    ret = dds_write_impl_iox (wr, ddsi_wr, writekey, data, tstamp, action);
  else
    ret = dds_write_impl_plain (wr, ddsi_wr, writekey, data, tstamp, action);
#else
  ret = dds_write_impl_plain (wr, ddsi_wr, writekey, data, tstamp, action);
#endif
  ddsi_thread_state_asleep (thrst);
  return ret;
}

dds_return_t dds_writecdr_impl (dds_writer *wr, struct ddsi_xpack *xp, struct ddsi_serdata *dinp, bool flush)
{
  return dds_writecdr_impl_common (wr->m_wr, xp, (struct ddsi_serdata_any *) dinp, flush, wr);
}

void dds_write_flush (dds_entity_t writer)
{
  dds_writer *wr;
  if (dds_writer_lock (writer, &wr) == DDS_RETCODE_OK)
  {
    struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
    ddsi_thread_state_awake (thrst, &wr->m_entity.m_domain->gv);
    ddsi_xpack_send (wr->m_xp, true);
    ddsi_thread_state_asleep (thrst);
    dds_writer_unlock (wr);
  }
}

dds_return_t dds_writecdr_local_orphan_impl (struct ddsi_local_orphan_writer *lowr, struct ddsi_serdata *d)
{
  // this never sends on the network and xp is only relevant for the network
#ifdef DDS_HAS_SHM
  assert (d->iox_chunk == NULL);
#endif

  // consumes 1 refc from din in all paths (weird, but ... history ...)
  // let refc(din) be r, so upon returning it must be r-1
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  int ret = DDS_RETCODE_OK;
  assert (lowr->wr.type == d->type);

  // d = din: refc(d) = r, otherwise refc(d) = 1

  ddsi_thread_state_awake (thrst, lowr->wr.e.gv);
  struct ddsi_tkmap_instance * const tk = ddsi_tkmap_lookup_instance_ref (lowr->wr.e.gv->m_tkmap, d);
  deliver_locally (&lowr->wr, d, tk);
  ddsi_tkmap_instance_unref (lowr->wr.e.gv->m_tkmap, tk);
  ddsi_serdata_unref(d); // d = din: refc(d) = r - 1
  ddsi_thread_state_asleep (thrst);
  return ret;
}
