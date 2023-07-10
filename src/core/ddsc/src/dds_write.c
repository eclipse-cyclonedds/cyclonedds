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
#include "dds/ddsi/ddsi_addrset.h"
#include "dds/ddsc/dds_loan.h"
#include "dds__heap_loan.h"
#include "dds__writer.h"
#include "dds__write.h"
#include "dds__loan.h"
#include "dds__psmx.h"

struct ddsi_serdata_plain { struct ddsi_serdata p; };
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
  struct ddsi_serdata * const din = si->src_payload;

  struct ddsi_serdata *d;
  // Mustn't store *PSMX writer* loans in RHCs because the PSMX write operation is
  // assumed to consume the reference (the write path zeros some pointers in the
  // loan structure to give us a fighting chance of catching a mistake).
  //
  // Loans from *PSMX reader* are ok, those are meant to hang around until the
  // application uses that data and releases it. But those don't go through here.
  //
  // And for completeness: data arriving over the network never goes through here
  // either.
  if (din->loan != NULL && din->loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_PSMX)
    d = ddsi_serdata_copy_as_type (type, din);
  else
    d = ddsi_serdata_ref_as_type (type, din);
  if (d == NULL)
  {
    DDS_CWARNING (&gv->logconfig, "local: deserialization %s failed in type conversion\n", type->type_name);
    return NULL;
  }
  // Mustn't store *PSMX writer* loans in RHCs because the PSMX write operation is
  // assumed to consume the reference (the write path zeros some pointers in the
  // loan structure to give us a fighting chance of catching a mistake).
  //
  // Loans from *PSMX reader* are ok, those are meant to hang around until the
  // application uses that data and releases it. But those don't go through here.
  //
  // And for completeness: data arriving over the network never goes through here
  // either.
  assert (d->loan == NULL || d->loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_HEAP);
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
    .timeout = { 0 }
  };
  dds_return_t rc;
  struct ddsi_writer_info wrinfo;
  ddsi_make_writer_info (&wrinfo, &wr->e, wr->xqos, payload->statusinfo);
  rc = ddsi_deliver_locally_allinsync (wr->e.gv, &wr->e, false, &wr->rdary, &wrinfo, &deliver_locally_ops, &sourceinfo);
  if (rc == DDS_RETCODE_TIMEOUT)
    DDS_CERROR (&wr->e.gv->logconfig, "The writer could not deliver data on time, probably due to a local reader resources being full\n");
  return rc;
}

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

static dds_return_t deliver_data_any (struct ddsi_thread_state * const thrst, struct ddsi_writer *ddsi_wr, struct ddsi_serdata_any *d, struct ddsi_xpack *xp, bool flush)
{
  struct ddsi_tkmap_instance * const tk = ddsi_tkmap_lookup_instance_ref (ddsi_wr->e.gv->m_tkmap, &d->a);
  dds_return_t ret;
  if ((ret = deliver_data_network (thrst, ddsi_wr, d, xp, flush, tk)) != DDS_RETCODE_OK)
  {
    ddsi_tkmap_instance_unref (ddsi_wr->e.gv->m_tkmap, tk);
    return ret;
  }
  ret = deliver_locally (ddsi_wr, &d->a, tk);
  ddsi_tkmap_instance_unref (ddsi_wr->e.gv->m_tkmap, tk);
  return ret;
}

static dds_return_t dds_writecdr_impl_common (struct ddsi_writer *ddsi_wr, struct ddsi_xpack *xp, struct ddsi_serdata_any *din, bool flush)
{
  // consumes 1 refc from din in all paths (weird, but ... history ...)
  // let refc(din) be r, so upon returning it must be r-1
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  int ret = DDS_RETCODE_OK;

  struct ddsi_serdata_any * const d = convert_serdata(ddsi_wr, din);
  if (d == NULL)
  {
    ddsi_serdata_unref(&din->a); // refc(din) = r - 1 as required
    return DDS_RETCODE_ERROR;
  }

  // d = din: refc(d) = r, otherwise refc(d) = 1

  ddsi_thread_state_awake (thrst, ddsi_wr->e.gv);
  ddsi_serdata_ref (&d->a); // d = din: refc(d) = r + 1, otherwise refc(d) = 2

  ret = deliver_data_any (thrst, ddsi_wr, d, xp, flush);

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

static bool requires_serialization(struct dds_topic *topic)
{
  return !topic->m_stype->fixed_size;
}

static bool allows_serialization_into_buffer(struct dds_topic *topic)
{
  return topic->m_stype->ops->serialize_into != NULL &&
      topic->m_stype->ops->get_serialized_size != NULL;
}

static bool get_required_buffer_size(struct dds_topic *topic, const void *sample, uint32_t *sz32)
{
  size_t sz;
  assert (topic && sz32 && sample);

  if (!requires_serialization(topic))
    sz = topic->m_stype->zerocopy_size;
  else if (allows_serialization_into_buffer(topic))
    sz = ddsi_sertype_get_serialized_size(topic->m_stype, (void*) sample);
  else
    return false;

  if (sz == SIZE_MAX || sz > UINT32_MAX)
    return false; // SIZE_MAX: error value (FIXME) or oversize
  *sz32 = (uint32_t) sz;
  return true;
}

static dds_return_t dds_write_basic_impl (struct ddsi_thread_state * const ts, dds_writer *wr, struct ddsi_serdata *d)
{
  struct ddsi_writer *ddsi_wr = wr->m_wr;
  dds_return_t ret = DDS_RETCODE_OK;

  if (d == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref (wr->m_entity.m_domain->gv.m_tkmap, d);

  (void) ddsi_serdata_ref(d);
  ret = ddsi_write_sample_gc (ts, wr->m_xp, ddsi_wr, d, tk);
  if (ret >= 0) {
    /* Flush out write unless configured to batch */
    if (!wr->whc_batch)
      ddsi_xpack_send (wr->m_xp, false);
    ret = DDS_RETCODE_OK;
  } else if (ret != DDS_RETCODE_TIMEOUT) {
    ret = DDS_RETCODE_ERROR;
  }

  if (ret == DDS_RETCODE_OK)
    ret = deliver_locally (ddsi_wr, d, tk);

  ddsi_tkmap_instance_unref (wr->m_entity.m_domain->gv.m_tkmap, tk);

  return ret;
}

dds_return_t dds_request_writer_loan(dds_writer *wr, void **samples_ptr, int32_t n_samples)
{
  if (n_samples < 0 || !samples_ptr)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_return_t ret = DDS_RETCODE_OK;
  int32_t index = 0;

  ddsrt_mutex_lock (&wr->m_entity.m_mutex);
  dds_loaned_sample_t **loans_ptr = dds_alloc (sizeof(dds_loaned_sample_t*)*(size_t)n_samples);
  if (!loans_ptr)
  {
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto fail_alloc;
  }

  // attempt to request loans from an PSMX instance
  if (wr->m_topic->m_stype->fixed_size)
  {
    for (uint32_t i = 0; i < wr->m_endpoint.psmx_endpoints.length; i++)
    {
      for (; index < n_samples; index++)
      {
        dds_loaned_sample_t *loan = dds_psmx_endpoint_request_loan (wr->m_endpoint.psmx_endpoints.endpoints[i], wr->m_topic->m_stype->zerocopy_size);
        if (!loan)
        {
          ret = DDS_RETCODE_ERROR;
          goto fail;
        }
        loans_ptr[index] = loan;
      }
    }
  }

  //attempt to request loans from heap based interface
  if (index == 0)
  {
    for (; index < n_samples; index++)
    {
      dds_loaned_sample_t *loan;
      if ((ret = dds_heap_loan (wr->m_topic->m_stype, &loan)) != DDS_RETCODE_OK)
        goto fail;
      loans_ptr[index] = loan;
    }
  }

  assert (index == n_samples);
  for (int32_t i = 0; i < n_samples; i++)
  {
    dds_loan_pool_add_loan (wr->m_loans, loans_ptr[i]); // takes over ref
    samples_ptr[i] = loans_ptr[i]->sample_ptr;
  }

fail:
  if (index != n_samples && loans_ptr != NULL)  //we couldnt get the number of loans requested
  {
    for (int32_t i = 0; i < index; i++)
      dds_loaned_sample_free (loans_ptr[i]);
  }
  dds_free (loans_ptr);

fail_alloc:
  ddsrt_mutex_unlock (&wr->m_entity.m_mutex);
  return ret < 0 ? ret : index;
}

dds_return_t dds_return_writer_loan (dds_writer *wr, void **samples_ptr, int32_t n_samples)
{
  if (n_samples < 0)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_return_t ret = DDS_RETCODE_OK;
  ddsrt_mutex_lock (&wr->m_entity.m_mutex);
  for (int32_t i = 0; i < n_samples && ret == DDS_RETCODE_OK; i++)
  {
    void *sample = samples_ptr[i];
    if (!sample)
      continue;

    dds_loaned_sample_t * loan = dds_loan_pool_find_loan(wr->m_loans, sample);
    if (loan)
    {
      (void) dds_loan_pool_remove_loan (loan);
      (void) dds_loaned_sample_unref (loan);
    }
    else
    {
      ret = DDS_RETCODE_BAD_PARAMETER;
    }
  }
  ddsrt_mutex_unlock (&wr->m_entity.m_mutex);
  return ret;
}

// Synchronizes the current number of fast path readers and returns it.
// Locking the mutex is needed to synchronize the value.
// This number may change concurrently any time we do not hold the lock,
// i.e. become outdated when we return from the function.
static uint32_t get_num_fast_path_readers (struct ddsi_writer *ddsi_wr)
{
  ddsrt_mutex_lock (&ddsi_wr->rdary.rdary_lock);
  uint32_t n = ddsi_wr->rdary.n_readers;
  ddsrt_mutex_unlock (&ddsi_wr->rdary.rdary_lock);
  return n;
}

static dds_loaned_sample_t *get_loan_to_use (dds_writer *wr, const void *data, dds_loaned_sample_t **loan_to_free)
{
  // 3. Check whether data is loaned
  dds_loaned_sample_t *supplied_loan = dds_loan_pool_find_loan (wr->m_loans, data);
  assert (supplied_loan == NULL || ddsrt_atomic_ld32 (&supplied_loan->refc) == 1);
  if (supplied_loan)
    dds_loan_pool_remove_loan (supplied_loan);

  assert ((supplied_loan == NULL) ||
          (supplied_loan != NULL && ddsrt_atomic_ld32 (&supplied_loan->refc) == 1 && supplied_loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_HEAP && supplied_loan->loan_pool == NULL) ||
          (supplied_loan != NULL && ddsrt_atomic_ld32 (&supplied_loan->refc) == 1 && supplied_loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_PSMX && supplied_loan->loan_pool == NULL));
  if (supplied_loan && supplied_loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_PSMX)
  {
    // a PSMX loan, use it
    *loan_to_free = NULL;
    return supplied_loan;
  }

  dds_loaned_sample_t *loan = NULL;

  // 4. If it is a heap loan, attempt to get a PSMX loan
  // FIXME: the condition is actually: if not a PSMX loan, try to get one
  // FIXME: should this not be required to succeed? We're assuming the PSMX bit will work later on
  // FIXME: what about: supplied_loan is a heap loan and no PSMX involved? why not use it?
  uint32_t required_size = 0;
  if (get_required_buffer_size (wr->m_topic, data, &required_size) && required_size)
  {
    // attempt to get a loan from a PSMX
    for (uint32_t i = 0; i < wr->m_endpoint.psmx_endpoints.length && !loan; i++)
      loan = dds_psmx_endpoint_request_loan (wr->m_endpoint.psmx_endpoints.endpoints[i], required_size);
  }

  // too many cases ...
  assert ((supplied_loan == NULL && loan == NULL) ||
          (supplied_loan == NULL && loan != NULL && ddsrt_atomic_ld32 (&loan->refc) == 1 && loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_PSMX && loan->loan_pool == NULL) ||
          (supplied_loan != NULL && loan == NULL && ddsrt_atomic_ld32 (&supplied_loan->refc) == 1 && supplied_loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_HEAP && supplied_loan->loan_pool == NULL) ||
          (supplied_loan != NULL && loan == supplied_loan && ddsrt_atomic_ld32 (&loan->refc) == 1 && ddsrt_atomic_ld32 (&loan->refc) == 1 && loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_PSMX && loan->loan_pool == NULL) ||
          (supplied_loan != NULL && loan != supplied_loan
              && ddsrt_atomic_ld32 (&supplied_loan->refc) == 1 && supplied_loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_PSMX && supplied_loan->loan_pool == NULL
              && ddsrt_atomic_ld32 (&loan->refc) == 1 && ddsrt_atomic_ld32 (&loan->refc) == 1 && loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_PSMX && loan->loan_pool == NULL));

  // by definition different from loan
  // not to be freed yet: freeing it invalidates data
  *loan_to_free = supplied_loan;
  return loan;
}

static struct ddsi_serdata *make_serdata (struct ddsi_writer * const ddsi_wr, const void *data, dds_loaned_sample_t *loan, bool writekey, bool use_only_psmx)
{
  struct ddsi_serdata *d;
  if (loan == NULL)
    d = ddsi_serdata_from_sample (ddsi_wr->type, writekey ? SDK_KEY : SDK_DATA, data);
  else
  {
    assert (ddsrt_atomic_ld32 (&loan->refc) == 1);
    assert (loan->loan_pool == NULL);
    d = ddsi_serdata_from_loaned_sample (ddsi_wr->type, writekey ? SDK_KEY : SDK_DATA, data, loan, !use_only_psmx);
  }
  if (d == NULL)
  {
    if (loan != NULL)
      dds_loaned_sample_unref (loan);
  }
  return d;
}

// has to support two cases:
// 1) data is in an external buffer allocated on the stack or dynamically
// 2) data is in an zerocopy buffer obtained by dds_loan_sample
dds_return_t dds_write_impl (dds_writer *wr, const void *data, dds_time_t tstamp, dds_write_action action)
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

  // 3. Check whether data is loaned
  dds_loaned_sample_t *loan_to_free;
  dds_loaned_sample_t * const loan = get_loan_to_use (wr, data, &loan_to_free);
  assert (loan == NULL || loan != loan_to_free);

  // ddsi_wr->as can be changed by the matching/unmatching of proxy readers if we don't hold the lock
  // it is rather unfortunate that this then means we have to lock here to check, then lock again to
  // actually distribute the data, so some further refactoring is needed.
  ddsrt_mutex_lock (&ddsi_wr->e.lock);
  const bool no_network_readers = ddsi_addrset_empty (ddsi_wr->as);
  ddsrt_mutex_unlock (&ddsi_wr->e.lock);

  // NB: local readers are not in L := ddsi_wr->rdary if they use PSMX.
  // Furthermore, all readers that ignore local publishers will not use PSMX.
  // We will never use only(!) PSMX if there is any local reader in L.
  // We will serialize the data in this case and deliver it mixed, i.e.
  // partially with PSMX as required by the QoS and type. The readers in L
  // will get the data via the local delivery mechanism (fast path or slow
  // path).
  const uint32_t num_fast_path_readers = get_num_fast_path_readers (ddsi_wr);

  // If use_only_psmx is true, there were no fast path readers at the moment
  // we checked.
  // If fast path readers arive later, they may not get data but this
  // is fine as we can consider their connections not fully established
  // and hence they are not considered for data transfer.
  // The alternative is to block new fast path connections entirely (by holding
  // the mutex) until data delivery is complete.
  const bool use_only_psmx =
      no_network_readers &&
      ddsi_wr->xqos->durability.kind == DDS_DURABILITY_VOLATILE &&
      num_fast_path_readers == 0;

  // create a correct serdata
  struct ddsi_serdata * const d = make_serdata (ddsi_wr, data, loan, writekey, use_only_psmx);

  // data, loan_to_free no longer needed (all paths)
  if (loan_to_free)
    dds_loaned_sample_unref (loan_to_free);

  // bail out if serdata creation failed
  if (d == NULL)
  {
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto fail_serdata;
  }

  // refc(d) = 1 after successful construction
  d->statusinfo = (((action & DDS_WR_DISPOSE_BIT) ? DDSI_STATUSINFO_DISPOSE : 0) |
                  ((action & DDS_WR_UNREGISTER_BIT) ? DDSI_STATUSINFO_UNREGISTER : 0));
  d->timestamp.v = tstamp;

  // 6. Deliver the data
  // 6.a ... via network
  if ((ret = dds_write_basic_impl (thrst, wr, d)) != DDS_RETCODE_OK)
    goto unref_serdata;

  // 6.b ... through PSMX
  if (loan)
  {
    assert (loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_PSMX);
    struct dds_psmx_endpoint *endpoint = loan->loan_origin.psmx_endpoint;

    // populate metadata fields
    struct dds_psmx_metadata *md = loan->metadata;
    memcpy (&md->guid, &ddsi_wr->e.guid, sizeof (md->guid));
    md->timestamp = d->timestamp.v;
    md->statusinfo = d->statusinfo;
    ret = endpoint->ops.write (endpoint, loan);
  }

unref_serdata:
  ddsi_serdata_unref (d); // refc(d) = 0
fail_serdata:
  ddsi_thread_state_asleep (thrst);
  return ret;
}

dds_return_t dds_writecdr_impl (dds_writer *wr, struct ddsi_xpack *xp, struct ddsi_serdata *d, bool flush)
{
  return dds_writecdr_impl_common (wr->m_wr, xp, (struct ddsi_serdata_any *) d, flush);
}

void dds_write_flush_impl (dds_writer *wr)
{
  ddsrt_mutex_lock (&wr->m_entity.m_mutex);
  ddsi_xpack_send (wr->m_xp, true);
  ddsrt_mutex_unlock (&wr->m_entity.m_mutex);
}

dds_return_t dds_writecdr_local_orphan_impl (struct ddsi_local_orphan_writer *lowr, struct ddsi_serdata *d)
{
  // this never sends on the network and xp is only relevant for the network
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
