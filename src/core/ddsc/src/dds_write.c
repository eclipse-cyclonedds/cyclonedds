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
#include "dds__heap_loan.h"
#include "dds__writer.h"
#include "dds__write.h"
#include "dds__loaned_sample.h"
#include "dds__psmx.h"
#include "dds__guid.h"

extern inline bool dds_source_timestamp_is_valid_ddsi_time (dds_time_t timestamp, ddsi_protocol_version_t protover);

struct ddsi_serdata_plain { struct ddsi_serdata p; };
struct ddsi_serdata_any   { struct ddsi_serdata a; };

struct dds_write_impl_psmx_key {
  struct ddsi_serdata *ksd;
  struct ddsi_serdata *ksdref;
  ddsrt_iovec_t key_iov;
  // type, sample are only needed for writecdr
  const struct ddsi_sertype *type;
  void *sample;
};

static void dds_write_impl_psmx_key_init (struct dds_write_impl_psmx_key *k)
{
  k->ksd = NULL;
  k->ksdref = NULL;
  k->key_iov.iov_len = 0;
  k->key_iov.iov_base = NULL;
  k->type = NULL;
  k->sample = NULL;
}

static dds_return_t dds_write_impl_psmx_get_key (struct dds_write_impl_psmx_key *k, const struct ddsi_sertype *type, const void *data)
{
  if (k->ksd || !type->has_key)
    return DDS_RETCODE_OK;
  if ((k->ksd = ddsi_serdata_from_sample (type, SDK_KEY, data)) == NULL)
    return DDS_RETCODE_ERROR;
  k->ksdref = ddsi_serdata_to_ser_ref (k->ksd, 0, ddsi_serdata_size (k->ksd), &k->key_iov);
  return DDS_RETCODE_OK;
}

static dds_return_t dds_write_impl_psmx_get_key_untyped_serdata (struct dds_write_impl_psmx_key *k, const struct ddsi_sertype *type, const struct ddsi_serdata *serdata)
{
  if (k->ksd || !type->has_key)
    return DDS_RETCODE_OK;
  // ouch, but less painful than changing the interface to allow extracting the serialized key
  // optimizing writing a key is not worth the bother
  // we get here from writecdr, so we don't have a sample on input
  k->type = type;
  k->sample = dds_alloc (type->sizeof_type);
  if (!ddsi_serdata_untyped_to_sample (type, serdata, k->sample, NULL, NULL))
  {
    dds_free (k->sample);
    k->sample = NULL;
    return DDS_RETCODE_ERROR;
  }
  return dds_write_impl_psmx_get_key (k, type, k->sample);
}

static void dds_write_impl_psmx_key_fini (struct dds_write_impl_psmx_key *k)
{
  if (k->ksd)
  {
    ddsi_serdata_to_ser_unref (k->ksdref, &k->key_iov);
    ddsi_serdata_unref (k->ksd);
  }
  if (k->sample)
  {
    ddsi_sertype_free_sample (k->type, k->sample, DDS_FREE_CONTENTS);
    dds_free (k->sample);
  }
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

  if (data == NULL)
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

static struct ddsi_serdata_any *convert_serdata (struct ddsi_writer *ddsi_wr, struct ddsi_serdata_any *din, bool require_copy)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

static struct ddsi_serdata_any *convert_serdata (struct ddsi_writer *ddsi_wr, struct ddsi_serdata_any *din, bool require_copy)
{
  struct ddsi_serdata_any *dout;
  if (require_copy)
  {
    dout = (struct ddsi_serdata_any *) ddsi_serdata_copy_as_type (ddsi_wr->type, &din->a);
    // dout refc: must consume 1
    // din refc: must consume 1 (independent of dact: types are distinct)
  }
  else if (ddsi_wr->type == din->a.type)
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

static dds_return_t deliver_data_any (struct ddsi_thread_state * const thrst, struct dds_writer *wr, struct ddsi_writer *ddsi_wr, struct ddsi_serdata_any *d, struct ddsi_xpack *xp, bool flush)
  ddsrt_nonnull ((1, 2, 3)) ddsrt_attribute_warn_unused_result;

static dds_return_t deliver_data_any (struct ddsi_thread_state * const thrst, struct dds_writer *wr, struct ddsi_writer *ddsi_wr, struct ddsi_serdata_any *d, struct ddsi_xpack *xp, bool flush)
{
  struct ddsi_tkmap_instance * const tk = ddsi_tkmap_lookup_instance_ref (ddsi_wr->e.gv->m_tkmap, &d->a);
  dds_return_t ret;
  ddsi_serdata_ref (&d->a); // d = din: refc(d) = r + 1, otherwise refc(d) = 2
  if ((ret = deliver_data_network (thrst, ddsi_wr, d, xp, flush, tk)) != DDS_RETCODE_OK)
    goto done;
  if ((ret = deliver_locally (ddsi_wr, &d->a, tk)) != DDS_RETCODE_OK)
    goto done;
  if (d->a.loan)
  {
    // Loan metadata and origin assumed valid by now
    struct dds_loaned_sample * const loan = d->a.loan;
#ifndef NDEBUG
    const ddsi_guid_t loan_metadata_ddsi_guid = dds_guid_to_ddsi_guid (loan->metadata->guid);
    assert (loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_PSMX &&
            loan->metadata->timestamp == d->a.timestamp.v &&
            loan->metadata->statusinfo == d->a.statusinfo &&
            memcmp (&loan_metadata_ddsi_guid, &ddsi_wr->e.guid, sizeof (loan_metadata_ddsi_guid)) == 0);
#endif

    // generate serialized key blob only when really needed
    struct dds_write_impl_psmx_key key;
    dds_write_impl_psmx_key_init (&key);

    const struct dds_psmx_endpoint_int *loan_source_ep = NULL;
    for (uint32_t l = 0; l < wr->m_endpoint.psmx_endpoints.length; l++)
    {
      struct dds_psmx_endpoint_int const * const ep = wr->m_endpoint.psmx_endpoints.endpoints[l];
      if (loan->loan_origin.psmx_endpoint == ep->ext)
      {
        // we don't have to copy to ourselves, but we need the internal endpoint for delivering the data
        loan_source_ep = ep;
        continue;
      }

      if (ep->wants_key)
      {
        dds_return_t ret2 = dds_write_impl_psmx_get_key_untyped_serdata (&key, wr->m_wr->type, tk->m_sample);
        if (ret2 != DDS_RETCODE_OK && ret == DDS_RETCODE_OK)
          ret = ret2;
        continue;
      }

      struct dds_loaned_sample *loan_copy = ep->ops.request_loan (ep, loan->metadata->sample_size);
      if (loan_copy == NULL)
      {
        if (ret == DDS_RETCODE_OK)
          ret = DDS_RETCODE_OUT_OF_RESOURCES;
        continue;
      }
      dds_loaned_sample_copy (loan_copy, loan);
      dds_return_t ret2 = ep->ops.write_with_key (ep, loan_copy, (uint32_t) key.key_iov.iov_len, key.key_iov.iov_base);
      if (ret2 != DDS_RETCODE_OK && ret == DDS_RETCODE_OK)
        ret = ret2;
    }

    assert (loan_source_ep != NULL);
    bool do_psmx_write = true;
    if (loan_source_ep->wants_key)
    {
      dds_return_t ret2 = dds_write_impl_psmx_get_key_untyped_serdata (&key, wr->m_wr->type, tk->m_sample);
      if (ret2 != DDS_RETCODE_OK)
      {
        do_psmx_write = false;
        if (ret == DDS_RETCODE_OK)
          ret = ret2;
      }
    }
    if (do_psmx_write)
    {
      dds_return_t ret2 = loan_source_ep->ops.write_with_key (loan_source_ep, loan, (uint32_t) key.key_iov.iov_len, key.key_iov.iov_base);
      if (ret2 != DDS_RETCODE_OK && ret == DDS_RETCODE_OK)
        ret = ret2;
    }
    dds_write_impl_psmx_key_fini (&key);
  }
done:
  ddsi_tkmap_instance_unref (ddsi_wr->e.gv->m_tkmap, tk);
  ddsi_serdata_unref (&d->a);
  return ret;
}

static dds_return_t dds_writecdr_impl_validate_loan (const struct dds_writer *wr, const struct ddsi_serdata_any *din)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

static dds_return_t dds_writecdr_impl_validate_loan (const struct dds_writer *wr, const struct ddsi_serdata_any *din)
{
  assert (din->a.loan != NULL);
  // Cases 4 and 8: bad parameter because a loan is passed in where none is expected
  if (wr->m_endpoint.psmx_endpoints.length == 0)
    return DDS_RETCODE_BAD_PARAMETER;
  else
  {
    // Case 5 with mismatch in loan
    dds_loaned_sample_t const * const loan = din->a.loan;
    struct dds_psmx_metadata const * const md = loan->metadata;
    bool loan_originates_here = false;
    for (uint32_t e = 0; e < wr->m_endpoint.psmx_endpoints.length && !loan_originates_here; e++) {
      if (loan->loan_origin.psmx_endpoint == wr->m_endpoint.psmx_endpoints.endpoints[e]->ext)
        loan_originates_here = true;
    }
    const ddsi_guid_t loan_metadata_ddsi_guid = dds_guid_to_ddsi_guid (md->guid);
    if (loan->loan_origin.origin_kind != DDS_LOAN_ORIGIN_KIND_PSMX ||
        !loan_originates_here ||
        md->timestamp != din->a.timestamp.v ||
        md->statusinfo != din->a.statusinfo ||
        memcmp (&loan_metadata_ddsi_guid, &wr->m_entity.m_guid, sizeof (loan_metadata_ddsi_guid)) != 0)
    {
      return DDS_RETCODE_BAD_PARAMETER;
    }
  }
  return DDS_RETCODE_OK;
}

static dds_return_t dds_writecdr_impl_ensureloan (struct dds_writer *wr, struct ddsi_serdata_any *din)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

static dds_return_t dds_writecdr_impl_ensureloan (struct dds_writer *wr, struct ddsi_serdata_any *din)
{
  if (din->a.loan != NULL)
    return DDS_RETCODE_OK;
  assert (ddsrt_atomic_ld32 (&din->a.refc) == 1);

  const uint32_t sersize = ddsi_serdata_size (&din->a);
  dds_loaned_sample_t * const loan = dds_writer_request_psmx_loan (wr, sersize - 4);
  if (loan == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  if (sersize > 4)
    ddsi_serdata_to_ser (&din->a, 4, sersize - 4, loan->sample_ptr);
  struct dds_psmx_metadata * const md = loan->metadata;
  md->sample_state = (din->a.kind == SDK_KEY) ? DDS_LOANED_SAMPLE_STATE_SERIALIZED_KEY : DDS_LOANED_SAMPLE_STATE_SERIALIZED_DATA;
  md->guid = dds_guid_from_ddsi_guid (wr->m_entity.m_guid);
  md->timestamp = din->a.timestamp.v;
  md->statusinfo = din->a.statusinfo;
  unsigned char cdr_header[4];
  ddsi_serdata_to_ser (&din->a, 0, 4, cdr_header);
  memcpy (&md->cdr_identifier, cdr_header, sizeof (md->cdr_identifier));
  memcpy (&md->cdr_options, cdr_header + 2, sizeof (md->cdr_options));
  din->a.loan = loan;
  return DDS_RETCODE_OK;
}

static dds_return_t dds_writecdr_impl_common (struct dds_writer *wr, struct ddsi_writer *ddsi_wr, struct ddsi_xpack *xp, struct ddsi_serdata_any *din, bool flush)
  ddsrt_nonnull((1, 2, 4)) ddsrt_attribute_warn_unused_result;

static dds_return_t dds_writecdr_impl_common (struct dds_writer *wr, struct ddsi_writer *ddsi_wr, struct ddsi_xpack *xp, struct ddsi_serdata_any *din, bool flush)
{
  // consumes 1 refc from din in all paths (weird, but ... history ...)
  // let refc(din) be r, so upon returning it must be r-1
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  dds_return_t ret;

  if (!dds_source_timestamp_is_valid_ddsi_time (din->a.timestamp.v, wr->protocol_version))
  {
    ddsi_serdata_unref (&din->a);
    return DDS_RETCODE_BAD_PARAMETER;
  }

  // Cases:
  //    din    correct      psmx?   loan?
  //    refc   sertype?
  // 1. >=1    yes          no      no        no changes needed: send din
  // 2. >1     yes          yes     no        add loan to copy, send copy
  // 3. 1      yes          yes     no        add loan to din, send din
  // 4. >=1    yes          no      yes       invalid, return BAD_PARAMETER
  // 5. >=1    yes          yes     yes       send din if loan ok (5a) else return BAD_PARAMETER (5b)
  // 6. >=1    no           no      no        convert + case 1
  // 7. >=1    no           yes     no        convert + case 3
  // 8. >=1    no           no      yes       invalid, return BAD_PARAMETER
  // 9. >=1    no           yes     yes       convert + case 3
  //
  // Case 9 assumes the loan is not included in the conversion.  Cases 6-9 are weird
  // ones anyway are almost never used, so no point in trying to be smart.  Only case
  // 3 is worth the optimization of not making a copy if a unique pointer is passed in.

  // Cases 4 and 8 and case 5a (mismatch in loan)
  if (din->a.loan != NULL && (ret = dds_writecdr_impl_validate_loan (wr, din)) != DDS_RETCODE_OK)
  {
    ddsi_serdata_unref (&din->a);
    return ret;
  }

  // Do we need a copy because we have to patch in a loan?
  // This reduces cases 1, 2, 3, 5b, 6, 7, 9 to a simple process:
  // - if uses_psmx and no loan present yet in `d`: add one
  // - send d
  // - if d != din, both need to be unref'd else only one
  const bool uses_psmx = (wr->m_endpoint.psmx_endpoints.length > 0);
  const bool require_copy = uses_psmx && din->a.loan == NULL && ddsrt_atomic_ld32 (&din->a.refc) > 1;
  struct ddsi_serdata_any * const d = convert_serdata (ddsi_wr, din, require_copy);
  if (d != din)
  {
    // Don't need din anymore, drop the reference.  (And if it was an alias, there'd be
    // no additional refcount, in which case this unref call must not be done.)
    ddsi_serdata_unref (&din->a);
    if (d == NULL)
      return DDS_RETCODE_ERROR;
  }

  // If we need a loan to be present because of PSMX, but none is, make one.  It could be
  // that din didn't have one, or it could be that we copied it because of the refcount,
  // or that we converted it.  Whichever the case may be, we need to make one.
  if (uses_psmx && (ret = dds_writecdr_impl_ensureloan (wr, d)) != DDS_RETCODE_OK)
  {
    ddsi_serdata_unref (&d->a);
    return ret;
  }

  // d = din: refc(d) = r, otherwise refc(d) = 1
  ddsi_thread_state_awake (thrst, ddsi_wr->e.gv);
  ret = deliver_data_any (thrst, wr, ddsi_wr, d, xp, flush);
  ddsi_thread_state_asleep (thrst);
  return ret;
}

static bool evaluate_topic_filter (const dds_writer *wr, const void *data, enum ddsi_serdata_kind sdkind)
{
  // false if data rejected by filter
  if (wr->m_topic->m_filter.mode == DDS_TOPIC_FILTER_NONE || sdkind == SDK_KEY)
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

ddsrt_nonnull_all
static dds_return_t dds_write_impl_deliver_via_ddsi (struct ddsi_thread_state * const ts, dds_writer *wr, struct ddsi_serdata *d)
{
  struct ddsi_writer *ddsi_wr = wr->m_wr;
  dds_return_t ret = DDS_RETCODE_OK;

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

ddsrt_nonnull_all
static bool dds_write_impl_use_only_psmx (dds_writer *wr)
{
  // Return false if PSMX is not involved
  if (wr->m_endpoint.psmx_endpoints.length == 0)
    return false;

  struct ddsi_writer * const ddsi_wr = wr->m_wr;
  // ddsi_wr->as can be changed by the matching/unmatching of proxy readers if we don't hold the lock
  // it is rather unfortunate that this then means we have to lock here to check, then lock again to
  // actually distribute the data, so some further refactoring is needed.
  ddsrt_mutex_lock (&ddsi_wr->e.lock);
  const bool no_network_readers = ddsi_addrset_empty (ddsi_wr->as);
  // NB: local readers are not in L := ddsi_wr->rdary if they use PSMX.
  // Furthermore, all readers that ignore local publishers will not use PSMX.
  // We will never use only(!) PSMX if there is any local reader in L.
  // We will serialize the data in this case and deliver it mixed, i.e.
  // partially with PSMX as required by the QoS and type. The readers in L
  // will get the data via the local delivery mechanism (fast path or slow
  // path).
  // Modifications only happen when ddsi_wr->e.lock and ...rdary.lock held,
  // so only holding the outer lock is fine.  It is possible a reader gets
  // added or removed once we unlock.
  const bool no_fast_path_readers = (ddsi_wr->rdary.n_readers == 0);
  ddsrt_mutex_unlock (&ddsi_wr->e.lock);

  // If use_only_psmx is true, there were no fast path readers at the moment
  // we checked.
  // If fast path readers arive later, they may not get data but this
  // is fine as we can consider their connections not fully established
  // and hence they are not considered for data transfer.
  // The alternative is to block new fast path connections entirely (by holding
  // the mutex) until data delivery is complete.
  return ddsi_wr->xqos->durability.kind == DDS_DURABILITY_VOLATILE && no_network_readers && no_fast_path_readers;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static dds_return_t dds_write_impl_deliver_via_psmx (const struct dds_psmx_endpoint_int *psmx_ep, struct dds_loaned_sample *loan, const ddsrt_iovec_t *key_iov)
{
  assert (loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_PSMX);
  assert (loan->metadata->sample_state != DDS_LOANED_SAMPLE_STATE_UNITIALIZED);
  assert (psmx_ep->ext == loan->loan_origin.psmx_endpoint);
  dds_return_t ret = psmx_ep->ops.write_with_key (psmx_ep, loan, (uint32_t) key_iov->iov_len, key_iov->iov_base);
  dds_loaned_sample_unref (loan);
  return ret;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static struct dds_loaned_sample *dds_write_impl_serialize_into_loan (const struct dds_writer *wr, const struct ddsi_sertype *sertype, enum ddsi_serdata_kind sdkind, const void *data, dds_time_t timestamp, uint32_t statusinfo)
{
  size_t loan_size_unpadded;
  uint16_t enc_identifier;
  if (ddsi_sertype_get_serialized_size (sertype, sdkind, data, &loan_size_unpadded, &enc_identifier) != 0)
    return NULL;
  const uint32_t pad_mask = 3u;
  const uint32_t loan_size_padded = ((uint32_t) loan_size_unpadded + pad_mask) & ~pad_mask;
  struct dds_loaned_sample * const loan = dds_writer_request_psmx_loan (wr, loan_size_padded);
  if (loan == NULL)
    return NULL;
  struct dds_psmx_metadata * const md = loan->metadata;
  md->sample_state = (sdkind == SDK_KEY) ? DDS_LOANED_SAMPLE_STATE_SERIALIZED_KEY : DDS_LOANED_SAMPLE_STATE_SERIALIZED_DATA;
  md->cdr_identifier = enc_identifier;
  md->cdr_options = ddsrt_toBE2u ((uint16_t) (loan_size_padded - loan_size_unpadded));
  ddsi_sertype_serialize_into (sertype, sdkind, data, loan->sample_ptr, loan_size_unpadded);
  dds_psmx_set_loan_writeinfo (loan, &wr->m_entity.m_guid, timestamp, statusinfo);
  return loan;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull ((1, 3))
static struct ddsi_serdata *dds_write_impl_make_serdata (const struct ddsi_sertype *sertype, enum ddsi_serdata_kind sdkind, const void *data, struct dds_loaned_sample *heap_loan, dds_time_t timestamp, uint32_t statusinfo)
{
  assert (heap_loan == NULL || heap_loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_HEAP);
  struct ddsi_serdata *serdata;
  if (heap_loan == NULL)
    serdata = ddsi_serdata_from_sample (sertype, sdkind, data);
  else // claim "cdr required" to keep things simple
    serdata = ddsi_serdata_from_loaned_sample (sertype, sdkind, data, heap_loan, true);
  if (serdata == NULL)
    return NULL;
  serdata->statusinfo = statusinfo;
  serdata->timestamp.v = timestamp;
  return serdata;
}

ddsrt_attribute_warn_unused_result ddsrt_nonnull_all
static dds_return_t dds_write_impl_psmxloan_serdata (struct dds_writer *wr, const void *data, enum ddsi_serdata_kind sdkind, dds_time_t timestamp, uint32_t statusinfo, struct dds_loaned_sample **psmx_loan, struct ddsi_serdata **serdata, struct dds_loaned_sample **loan_to_be_freed)
{
  const bool use_only_psmx = dds_write_impl_use_only_psmx (wr);
  struct ddsi_sertype const * const sertype = wr->m_wr->type;
  struct dds_loaned_sample *loan = dds_loan_pool_find_and_remove_loan (wr->m_loans, data);
  *loan_to_be_freed = NULL;
  if (loan)
  {
    // If we have a loan:
    // - if there is only PSMX involved, we can short-circuit delivery and get out quickly
    // - if anything is around, construct a regular serdata and derive a PSMX copy of it afterward if needed
    assert (ddsrt_atomic_ld32 (&loan->refc) == 1);
    struct dds_psmx_metadata * const md = loan->metadata;
    assert (md->sample_state == DDS_LOANED_SAMPLE_STATE_UNITIALIZED);
    md->sample_state = (sdkind == SDK_KEY) ? DDS_LOANED_SAMPLE_STATE_RAW_KEY : DDS_LOANED_SAMPLE_STATE_RAW_DATA;
    md->cdr_identifier = DDSI_RTPS_SAMPLE_NATIVE;
    md->cdr_options = 0;
    dds_psmx_set_loan_writeinfo (loan, &wr->m_entity.m_guid, timestamp, statusinfo);
    switch (loan->loan_origin.origin_kind)
    {
      case DDS_LOAN_ORIGIN_KIND_PSMX:
        // we never do PSMX loans for complex types
        assert (sertype->is_memcpy_safe);
        *psmx_loan = loan;
        if (use_only_psmx)
        {
          // short-circuit possible without requiring a serdata
          *serdata = NULL;
        }
        else if ((*serdata = dds_write_impl_make_serdata (sertype, sdkind, data, NULL, timestamp, statusinfo)) == NULL)
        {
          // It is either no memory or invalid data, we've historically gambled on it being invalid
          // data because being out of memory is exceedingly unlikely on decent platforms
          dds_loaned_sample_unref (loan);
          return DDS_RETCODE_BAD_PARAMETER;
        }
        return DDS_RETCODE_OK;
      case DDS_LOAN_ORIGIN_KIND_HEAP:
        if (use_only_psmx && sertype->ops->get_serialized_size)
        {
          // short-circuit possible without requiring a serdata
          *serdata = NULL;
          *psmx_loan = dds_write_impl_serialize_into_loan (wr, sertype, sdkind, data, timestamp, statusinfo);
          if (*psmx_loan != NULL)
            *loan_to_be_freed = loan;
          else
          {
            // It is either no memory or invalid data, we've historically gambled on it being invalid data
            dds_loaned_sample_unref (loan);
            return DDS_RETCODE_BAD_PARAMETER;
          }
        }
        else if (wr->m_endpoint.psmx_endpoints.length == 0)
        {
          // no PSMX, so local readers and/or network; keeping the loan makes sense for local readers
          *psmx_loan = NULL;
          // claim "cdr required" - it may not be strictly required for volatile data if there are only
          // local readers, but let's not complicate it too much now
          if ((*serdata = dds_write_impl_make_serdata (sertype, sdkind, data, loan, timestamp, statusinfo)) == NULL)
          {
            // It is either no memory or invalid data, we've historically gambled on it being invalid
            // data because being out of memory is exceedingly unlikely on decent platforms
            dds_loaned_sample_unref (loan);
            return DDS_RETCODE_BAD_PARAMETER;
          }
        }
        else
        {
          // PSMX, so CDR in memory.  It seems not so likely that there will be enough local readers not using
          // PSMX to make it worth the retaining the loan
          assert (!sertype->is_memcpy_safe);
          // Make a "standard" serdata and then use that to make a PSMX loan
          *serdata = dds_write_impl_make_serdata (sertype, sdkind, data, NULL, timestamp, statusinfo);
          if (*serdata == NULL)
          {
            // It is either no memory or invalid data, we've historically gambled on it being invalid
            // data because being out of memory is exceedingly unlikely on decent platforms
            dds_loaned_sample_unref (loan);
            return DDS_RETCODE_BAD_PARAMETER;
          }
          *psmx_loan = dds_writer_psmx_loan_from_serdata (wr, *serdata);
          if (*psmx_loan == NULL)
          {
            ddsi_serdata_unref (*serdata);
            dds_loaned_sample_unref (loan);
            return DDS_RETCODE_OUT_OF_RESOURCES;
          }
          *loan_to_be_freed = loan;
        }
        return DDS_RETCODE_OK;
    }
    return DDS_RETCODE_ERROR;
  }
  else if (use_only_psmx && (sertype->is_memcpy_safe || sertype->ops->get_serialized_size))
  {
    *serdata = NULL;
    if (sertype->is_memcpy_safe)
      *psmx_loan = dds_writer_psmx_loan_raw (wr, data, sdkind, timestamp, statusinfo);
    else
      *psmx_loan = dds_write_impl_serialize_into_loan (wr, sertype, sdkind, data, timestamp, statusinfo);
    return (*psmx_loan != NULL) ? DDS_RETCODE_OK : DDS_RETCODE_OUT_OF_RESOURCES;
  }
  else
  {
    // not much room for optimization, so keep things simple: construct a serdata, then take it from
    // there
    if ((*serdata = dds_write_impl_make_serdata (sertype, sdkind, data, NULL, timestamp, statusinfo)) == NULL)
    {
      // It is either no memory or invalid data, we've historically gambled on it being invalid
      // data because being out of memory is exceedingly unlikely on decent platforms
      return DDS_RETCODE_BAD_PARAMETER;
    }

    // If PSMX and no loan, make one using the data/serdata we do have
    if (wr->m_endpoint.psmx_endpoints.length == 0)
      *psmx_loan = NULL;
    else
    {
      if (sertype->is_memcpy_safe)
        *psmx_loan = dds_writer_psmx_loan_raw (wr, data, sdkind, timestamp, statusinfo);
      else
        *psmx_loan = dds_writer_psmx_loan_from_serdata (wr, *serdata);
      if (*psmx_loan == NULL)
      {
        ddsi_serdata_unref (*serdata);
        return DDS_RETCODE_OUT_OF_RESOURCES;
      }
    }
    return DDS_RETCODE_OK;
  }
}

dds_return_t dds_write_impl (dds_writer *wr, const void *data, dds_time_t timestamp, dds_write_action action)
{
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  const enum ddsi_serdata_kind sdkind = (action & DDS_WR_KEY_BIT) ? SDK_KEY : SDK_DATA;
  const uint32_t statusinfo =
    (((action & DDS_WR_DISPOSE_BIT) ? DDSI_STATUSINFO_DISPOSE : 0) |
     ((action & DDS_WR_UNREGISTER_BIT) ? DDSI_STATUSINFO_UNREGISTER : 0));

  if (!dds_source_timestamp_is_valid_ddsi_time (timestamp, wr->protocol_version))
    return DDS_RETCODE_BAD_PARAMETER;

  if (!evaluate_topic_filter (wr, data, sdkind))
    return DDS_RETCODE_OK;

  // I. psmx loan => assert (psmx && is_memcpy_safe)
  //   a. psmx only
  //     - no need for a serdata, so skip everything and deliver loan via PSMX
  //   b. psmx && others
  //     - ddsi_serdata_from_sample because we need a "normal one" anyway
  //     - deliver loan via PSMX (has to wait for from_sample because publishing via PSMX invalidates the data)
  //     - deliver serdata
  //
  // II. heap loan => (note: not necessarily is_memcpy_safe)
  //   a. psmx only - assert (!is_memcpy_safe); as-if no loan
  //   b. psmx && others - assert (!is_memcpy_safe); as-if no loan: we typically have PSMX loopback and that makes the loan useless
  //   c. no psmx
  //     - ddsi_serdata_from_loaned_sample, deliver serdata
  //
  // III. not loan
  //   a. psmx only
  //     1. is_memcpy_safe
  //       - allocate PSMX loan, memcpy, deliver loan via PSMX
  //     2. not is_memcpy_safe
  //       - get_serialized_size (key/sample), allocate PSMX loan, serialize_into
  //       - deliver loan via PSMX
  //       note: if get_serialized_size is 0, treat as case III.b instead
  //   b. psmx && others
  //     1. is_memcpy_safe
  //       - allocate PSMX loan, memcpy, deliver loan via PSMX
  //       - ddsi_serdata_from_sample, deliver serdata
  //     2. not is_memcpy_safe
  //       - ddsi_serdata_from_sample
  //       - allocate PSMX loan based on size of CDR in serdata, memcpy, deliver loan via PSMX
  //       - deliver serdata
  //   c. no psmx
  //     - ddsi_serdata_from_sample, deliver serdata
  ddsi_thread_state_awake (thrst, &wr->m_entity.m_domain->gv);
  struct ddsi_serdata *serdata;
  struct dds_loaned_sample *psmx_loan;
  // If the input is a keyed topic and there's a PSMX endpoint that wants the key value, then we
  // need to retain access the sample until after the key for the PSMX writes has been generated.
  //
  // If the data is not a loan or a PSMX loan, then by construction it remains valid at least
  // until the last of the PSMX writes.  If it is a heap loan, it may have been copied into some
  // other form (serialized PSMX loan and/or serdata), but those don't allow for cheap extraction
  // of the key.  So it can't be freed by "dds_write_impl_psmxloan_serdata".
  struct dds_loaned_sample *loan_to_be_freed;
  dds_return_t ret = DDS_RETCODE_OK;
  if ((ret = dds_write_impl_psmxloan_serdata (wr, data, sdkind, timestamp, statusinfo, &psmx_loan, &serdata, &loan_to_be_freed)) == DDS_RETCODE_OK)
  {
    assert (psmx_loan != NULL || serdata != NULL);
    assert ((psmx_loan == NULL) == (wr->m_endpoint.psmx_endpoints.length == 0));
    assert (psmx_loan == NULL || psmx_loan->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_PSMX);
    assert (loan_to_be_freed == NULL || loan_to_be_freed->loan_origin.origin_kind == DDS_LOAN_ORIGIN_KIND_HEAP);
    if (psmx_loan != NULL)
    {
      // generate serialized key blob only when really needed
      struct dds_write_impl_psmx_key key;
      dds_write_impl_psmx_key_init (&key);

      const struct dds_psmx_endpoint_int *loan_source_ep = NULL;
      for (uint32_t l = 0; l < wr->m_endpoint.psmx_endpoints.length; l++)
      {
        struct dds_psmx_endpoint_int const * const ep = wr->m_endpoint.psmx_endpoints.endpoints[l];
        if (psmx_loan->loan_origin.psmx_endpoint == ep->ext)
        {
          // we don't have to copy to ourselves, but we need the internal endpoint for delivering the data
          loan_source_ep = ep;
          continue;
        }

        if (ep->wants_key)
        {
          dds_return_t ret2 = dds_write_impl_psmx_get_key (&key, wr->m_wr->type, data);
          if (ret2 != DDS_RETCODE_OK && ret == DDS_RETCODE_OK)
            ret = ret2;
          continue;
        }

        struct dds_loaned_sample *loan_copy = ep->ops.request_loan (ep, psmx_loan->metadata->sample_size);
        if (loan_copy == NULL)
        {
          if (ret == DDS_RETCODE_OK)
            ret = DDS_RETCODE_OUT_OF_RESOURCES;
          continue;
        }
        dds_loaned_sample_copy (loan_copy, psmx_loan);
        dds_return_t ret2 = dds_write_impl_deliver_via_psmx (ep, loan_copy, &key.key_iov);
        if (ret2 != DDS_RETCODE_OK && ret == DDS_RETCODE_OK)
          ret = ret2;
      }

      assert (loan_source_ep != NULL);
      bool do_psmx_write = true;
      if (loan_source_ep->wants_key)
      {
        dds_return_t ret2 = dds_write_impl_psmx_get_key (&key, wr->m_wr->type, data);
        if (ret2 != DDS_RETCODE_OK)
        {
          dds_loaned_sample_unref (psmx_loan);
          do_psmx_write = false;
          if (ret == DDS_RETCODE_OK)
            ret = ret2;
        }
      }
      if (do_psmx_write)
      {
        dds_return_t ret2 = dds_write_impl_deliver_via_psmx (loan_source_ep, psmx_loan, &key.key_iov); // "consumes" the loan
        if (ret2 != DDS_RETCODE_OK && ret == DDS_RETCODE_OK)
          ret = ret2;
      }
      dds_write_impl_psmx_key_fini (&key);
    }

    if (serdata != NULL)
    {
      if (ret == DDS_RETCODE_OK)
        ret = dds_write_impl_deliver_via_ddsi (thrst, wr, serdata);
      ddsi_serdata_unref (serdata);
    }

    if (loan_to_be_freed)
      dds_loaned_sample_unref (loan_to_be_freed);
  }
  ddsi_thread_state_asleep (thrst);
  return ret;
}

dds_return_t dds_writecdr_impl (dds_writer *wr, struct ddsi_xpack *xp, struct ddsi_serdata *d, bool flush)
{
  dds_return_t ret = dds_writecdr_impl_common (wr, wr->m_wr, xp, (struct ddsi_serdata_any *) d, flush);
  return ret;
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
