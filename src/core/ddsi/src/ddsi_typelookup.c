/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <string.h>
#include <stdlib.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/ddsi_serdata_pserop.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_plist_generic.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_typelookup.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_typeid.h"
#include "dds/ddsi/q_gc.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_protocol.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_xmsg.h"

const enum pserop typelookup_service_request_ops[] = { XG, Xl, XQ, XK, XSTOP, XSTOP };
size_t typelookup_service_request_nops = sizeof (typelookup_service_request_ops) / sizeof (typelookup_service_request_ops[0]);

const enum pserop typelookup_service_reply_ops[] = { XG, Xl, XQ, XK, XO, XSTOP, XSTOP };
size_t typelookup_service_reply_nops = sizeof (typelookup_service_reply_ops) / sizeof (typelookup_service_reply_ops[0]);

typedef struct tlm_endpoints_iter {
  const struct tl_meta *tlm;
  uint32_t idx;
} tlm_endpoints_iter_t;


/* FIXME: the current data structure for storing endpoints in struct tl_meta does not
   scale very well, it needs to be replaced by a better algorithm */

static bool tlm_endpoint_exists (struct tl_meta *tlm, const ddsi_guid_t *proxy_ep_guid)
{
  /* FIXME: see comment above, this data structure needs to be replaced */
  for (uint32_t n = 0; n < tlm->proxy_endpoints.count; n++)
  {
    if (!memcmp (&tlm->proxy_endpoints.eps[n], proxy_ep_guid, sizeof (*proxy_ep_guid)))
      return true;
  }
  return false;
}

static void tlm_endpoint_add (struct ddsi_domaingv *gv, struct tl_meta *tlm, const ddsi_guid_t *proxy_ep_guid)
{
  if (!tlm_endpoint_exists (tlm, proxy_ep_guid))
  {
    /* FIXME: see comment above, this data structure needs to be replaced */
    tlm->proxy_endpoints.count++;
    tlm->proxy_endpoints.eps = ddsrt_realloc (tlm->proxy_endpoints.eps, tlm->proxy_endpoints.count * sizeof (*(tlm->proxy_endpoints.eps)));
    memcpy (&tlm->proxy_endpoints.eps[tlm->proxy_endpoints.count - 1], proxy_ep_guid, sizeof (*proxy_ep_guid));
    GVTRACE (" add ep "PGUIDFMT, PGUID (*proxy_ep_guid));
  }
}

static void tlm_endpoint_remove (struct ddsi_domaingv *gv, struct tl_meta *tlm, const ddsi_guid_t *proxy_ep_guid)
{
  /* FIXME: see comment above, this data structure needs to be replaced */
  uint32_t n = 0;
  while (n < tlm->proxy_endpoints.count)
  {
    if (memcmp (&tlm->proxy_endpoints.eps[n], proxy_ep_guid, sizeof (*proxy_ep_guid)) != 0)
      n++;
    else
    {
      GVTRACE (" remove ep "PGUIDFMT, PGUID (*proxy_ep_guid));
      if (--tlm->proxy_endpoints.count > 0)
      {
        memmove (&tlm->proxy_endpoints.eps[n], &tlm->proxy_endpoints.eps[n + 1], (tlm->proxy_endpoints.count - n) * sizeof (*(tlm->proxy_endpoints.eps)));
        tlm->proxy_endpoints.eps = ddsrt_realloc (tlm->proxy_endpoints.eps, tlm->proxy_endpoints.count * sizeof (*(tlm->proxy_endpoints.eps)));
      }
      else
      {
        ddsrt_free (tlm->proxy_endpoints.eps);
        tlm->proxy_endpoints.eps = NULL;
      }
      break;
    }
  }
}

static bool tlm_endpoints_empty (const struct tl_meta *tlm)
{
  return tlm->proxy_endpoints.count == 0;
}

static size_t tlm_endpoints_count (const struct tl_meta *tlm)
{
  return tlm->proxy_endpoints.count;
}

static ddsi_guid_t *tlm_endpoints_iter_first (const struct tl_meta *tlm, struct tlm_endpoints_iter *iter)
{
  assert (tlm != NULL);
  assert (iter != NULL);
  iter->tlm = tlm;
  iter->idx = 0;
  return (tlm->proxy_endpoints.count > 0) ? &tlm->proxy_endpoints.eps[0] : NULL;
}

static const ddsi_guid_t *tlm_endpoints_iter_next (struct tlm_endpoints_iter *iter)
{
  assert (iter != NULL);
  return (iter->tlm->proxy_endpoints.count > ++iter->idx) ? &iter->tlm->proxy_endpoints.eps[iter->idx] : NULL;
}

bool ddsi_tl_meta_equal (const struct tl_meta *a, const struct tl_meta *b)
{
  return ddsi_typeid_equal (&a->type_id, &b->type_id);
}

uint32_t ddsi_tl_meta_hash (const struct tl_meta *tl_meta)
{
  // As the type id is the key in the hash table and the type id currently only
  // consists of a hash value, we'll use the first 32 bits of that hash for now
  return *(uint32_t *) tl_meta->type_id.hash;
}

static void tlm_fini (struct tl_meta *tlm)
{
  if (tlm->sertype != NULL)
    ddsi_sertype_unref ((struct ddsi_sertype *) tlm->sertype);
  bool ep_empty = tlm_endpoints_empty (tlm);
  assert (ep_empty);
  (void) ep_empty;
  ddsrt_free (tlm);
}

struct tl_meta * ddsi_tl_meta_lookup_locked (struct ddsi_domaingv *gv, const type_identifier_t *type_id)
{
  struct tl_meta templ;
  memset (&templ, 0, sizeof (templ));
  templ.type_id = *type_id;
  struct tl_meta *tlm = ddsrt_hh_lookup (gv->tl_admin, &templ);
  return tlm;
}

struct tl_meta * ddsi_tl_meta_lookup (struct ddsi_domaingv *gv, const type_identifier_t *type_id)
{
  struct tl_meta *tlm;
  ddsrt_mutex_lock (&gv->tl_admin_lock);
  tlm = ddsi_tl_meta_lookup_locked (gv, type_id);
  ddsrt_mutex_unlock (&gv->tl_admin_lock);
  return tlm;
}

static void tlm_ref_impl (struct ddsi_domaingv *gv, const type_identifier_t *type_id, const struct ddsi_sertype *type, const ddsi_guid_t *proxy_ep_guid)
{
  bool resolved = false;
  assert (type_id || type);
  GVTRACE (" ref tl_meta");
  type_identifier_t *tid;
  if (type_id != NULL)
    tid = (type_identifier_t *) type_id;
  else
  {
    GVTRACE (" sertype %p", type);
    tid = ddsi_typeid_from_sertype (type);
    if (tid == NULL)
      return;
  }
  GVTRACE (" tid "PTYPEIDFMT, PTYPEID (*tid));

  ddsrt_mutex_lock (&gv->tl_admin_lock);
  struct tl_meta *tlm = ddsi_tl_meta_lookup_locked (gv, tid);
  if (tlm == NULL)
  {
    tlm = ddsrt_malloc (sizeof (*tlm));
    memset (tlm, 0, sizeof (*tlm));
    tlm->state = TL_META_NEW;
    tlm->type_id = *tid;
    ddsrt_hh_add (gv->tl_admin, tlm);
    GVTRACE (" new %p", tlm);
  }
  if (proxy_ep_guid != NULL)
    tlm_endpoint_add (gv, tlm, proxy_ep_guid);
  if (tlm->sertype == NULL && type != NULL)
  {
    tlm->sertype = ddsi_sertype_ref (type);
    tlm->state = TL_META_RESOLVED;
    GVTRACE (" resolved");
    resolved = true;
  }
  tlm->refc++;
  GVTRACE (" state %d refc %u\n", tlm->state, tlm->refc);

  if (resolved)
    ddsrt_cond_broadcast (&gv->tl_resolved_cond);

  ddsrt_mutex_unlock (&gv->tl_admin_lock);

  if (type_id == NULL)
    ddsrt_free (tid);
}

void ddsi_tl_meta_local_ref (struct ddsi_domaingv *gv, const type_identifier_t *type_id, const struct ddsi_sertype *type)
{
  if (type_id == NULL || !ddsi_typeid_none (type_id))
    tlm_ref_impl (gv, type_id, type, NULL);
}

void ddsi_tl_meta_proxy_ref (struct ddsi_domaingv *gv, const type_identifier_t *type_id, const ddsi_guid_t *proxy_ep_guid)
{
  if (type_id == NULL || !ddsi_typeid_none (type_id))
    tlm_ref_impl (gv, type_id, NULL, proxy_ep_guid);
}

static void tlm_unref_impl_locked (struct ddsi_domaingv *gv, struct tl_meta *tlm, const ddsi_guid_t *proxy_ep_guid)
{
  assert (tlm->refc > 0);
  if (proxy_ep_guid != NULL)
    tlm_endpoint_remove (gv, tlm, proxy_ep_guid);
  if (--tlm->refc == 0)
  {
    GVTRACE (" remove tl_meta");
    ddsrt_hh_remove (gv->tl_admin, tlm);
    tlm_fini (tlm);
  }
}

static void tlm_unref_impl (struct ddsi_domaingv *gv, const type_identifier_t *type_id, const struct ddsi_sertype *type, const ddsi_guid_t *proxy_ep_guid)
{
  assert (type_id || type);
  GVTRACE ("unref tl_meta");
  type_identifier_t *tid;
  if (type_id != NULL)
    tid = (type_identifier_t *) type_id;
  else
  {
    GVTRACE (" sertype %p", type);
    tid = ddsi_typeid_from_sertype (type);
    if (tid == NULL)
      return;
  }
  GVTRACE (" tid "PTYPEIDFMT, PTYPEID (*tid));

  ddsrt_mutex_lock (&gv->tl_admin_lock);
  struct tl_meta *tlm = ddsi_tl_meta_lookup_locked (gv, tid);
  assert (tlm != NULL);
  tlm_unref_impl_locked (gv, tlm, proxy_ep_guid);
  ddsrt_mutex_unlock (&gv->tl_admin_lock);
  if (type_id == NULL)
    ddsrt_free (tid);
  GVTRACE ("\n");
}

void ddsi_tl_meta_proxy_unref (struct ddsi_domaingv *gv, const type_identifier_t *type_id, const ddsi_guid_t *proxy_ep_guid)
{
  if (type_id == NULL || !ddsi_typeid_none (type_id))
    tlm_unref_impl (gv, type_id, NULL, proxy_ep_guid);
}

void ddsi_tl_meta_local_unref (struct ddsi_domaingv *gv, const type_identifier_t *type_id, const struct ddsi_sertype *type)
{
  if (type_id == NULL || !ddsi_typeid_none (type_id))
    tlm_unref_impl (gv, type_id, type, NULL);
}

static struct writer *get_typelookup_writer (const struct ddsi_domaingv *gv, uint32_t wr_eid)
{
  struct participant *pp;
  struct writer *wr = NULL;
  struct entidx_enum_participant est;
  thread_state_awake (lookup_thread_state (), gv);
  entidx_enum_participant_init (&est, gv->entity_index);
  while (wr == NULL && (pp = entidx_enum_participant_next (&est)) != NULL)
    wr = get_builtin_writer (pp, wr_eid);
  entidx_enum_participant_fini (&est);
  thread_state_asleep (lookup_thread_state ());
  return wr;
}

bool ddsi_tl_request_type (struct ddsi_domaingv * const gv, const type_identifier_t *type_id)
{
  ddsrt_mutex_lock (&gv->tl_admin_lock);
  struct tl_meta *tlm = ddsi_tl_meta_lookup_locked (gv, type_id);
  GVTRACE ("tl-req ");
  if (tlm->state != TL_META_NEW)
  {
    // type lookup is pending or the type is already resolved, so we'll return true
    // to indicate that the type request is done (or not required)
    GVTRACE ("state not-new (%u) for "PTYPEIDFMT"\n", tlm->state, PTYPEID (*type_id));
    ddsrt_mutex_unlock (&gv->tl_admin_lock);
    return true;
  }

  struct writer *wr = get_typelookup_writer (gv, NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER);
  if (wr == NULL)
  {
    GVTRACE ("no pp found with tl request writer");
    ddsrt_mutex_unlock (&gv->tl_admin_lock);
    return false;
  }

  type_lookup_request_t request;
  memcpy (&request.writer_guid, &wr->e.guid, sizeof (wr->e.guid));
  request.sequence_number = ++tlm->request_seqno;
  request.type_ids.n = 1;
  request.type_ids.type_ids = &tlm->type_id;

  struct ddsi_serdata *serdata = ddsi_serdata_from_sample (gv->tl_svc_request_type, SDK_DATA, &request);
  serdata->timestamp = ddsrt_time_wallclock ();

  tlm->state = TL_META_REQUESTED;
  ddsrt_mutex_unlock (&gv->tl_admin_lock);

  thread_state_awake (lookup_thread_state (), gv);
  GVTRACE ("wr "PGUIDFMT" typeid "PTYPEIDFMT"\n", PGUID (wr->e.guid), PTYPEID(*type_id));
  struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, serdata);
  write_sample_gc (lookup_thread_state (), NULL, wr, serdata, tk);
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
  thread_state_asleep (lookup_thread_state ());

  return true;
}

static void write_typelookup_reply (struct writer *wr, seqno_t seqno, type_identifier_type_object_pair_seq_t *types)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  type_lookup_reply_t reply;

  GVTRACE (" tl-reply ");
  memcpy (&reply.writer_guid, &wr->e.guid, sizeof (wr->e.guid));
  reply.sequence_number = seqno;
  reply.types.n = types->n;
  reply.types.types = types->types;
  struct ddsi_serdata *serdata = ddsi_serdata_from_sample (gv->tl_svc_reply_type, SDK_DATA, &reply);
  serdata->timestamp = ddsrt_time_wallclock ();

  GVTRACE ("wr "PGUIDFMT"\n", PGUID (wr->e.guid));
  struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, serdata);
  write_sample_gc (lookup_thread_state (), NULL, wr, serdata, tk);
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
}

void ddsi_tl_handle_request (struct ddsi_domaingv *gv, struct ddsi_serdata *sample_common)
{
  assert (!(sample_common->statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER)));
  const struct ddsi_serdata_pserop *sample = (const struct ddsi_serdata_pserop *) sample_common;
  const type_lookup_request_t *req = sample->sample;
  GVTRACE (" handle-tl-req wr "PGUIDFMT " seqnr %"PRIi64" ntypeids %"PRIu32, PGUID (req->writer_guid), req->sequence_number, req->type_ids.n);

  ddsrt_mutex_lock (&gv->tl_admin_lock);
  type_identifier_type_object_pair_seq_t types = { 0, NULL };
  for (uint32_t n = 0; n < req->type_ids.n; n++)
  {
    type_identifier_t *tid = &req->type_ids.type_ids[n];
    GVTRACE (" type "PTYPEIDFMT, PTYPEID (*tid));
    struct tl_meta *tlm = ddsi_tl_meta_lookup_locked (gv, tid);
    if (tlm != NULL && tlm->state == TL_META_RESOLVED)
    {
      size_t sz;
      types.types = ddsrt_realloc (types.types, (types.n + 1) * sizeof (*types.types));
      types.types[types.n].type_identifier = tlm->type_id;
      if (!ddsi_sertype_serialize (tlm->sertype, &sz, &types.types[types.n].type_object.value))
      {
        GVTRACE (" serialize failed");
      }
      else
      {
        assert (sz <= UINT32_MAX);
        types.n++;
        types.types[types.n - 1].type_object.length = (uint32_t) sz;
        GVTRACE (" found");
      }
    }
  }
  ddsrt_mutex_unlock (&gv->tl_admin_lock);

  struct writer *wr = get_typelookup_writer (gv, NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER);
  if (wr != NULL)
    write_typelookup_reply (wr, req->sequence_number, &types);
  else
    GVTRACE (" no tl-reply writer");

  for (uint32_t n = 0; n < types.n; n++)
    ddsrt_free (types.types[n].type_object.value);
  ddsrt_free (types.types);
}

static void tlm_register_with_proxy_endpoints_locked (struct ddsi_domaingv *gv, struct tl_meta *tlm)
{
  thread_state_awake (lookup_thread_state (), gv);

  tlm_endpoints_iter_t it;
  for (const ddsi_guid_t *ep = tlm_endpoints_iter_first (tlm, &it); ep != NULL; ep = tlm_endpoints_iter_next (&it))
  {
    struct entity_common *ec;
    if ((ec = entidx_lookup_guid_untyped (gv->entity_index, ep)) != NULL)
    {
      assert (ec->kind == EK_PROXY_READER || ec->kind == EK_PROXY_WRITER);
      struct generic_proxy_endpoint *gpe = (struct generic_proxy_endpoint *) ec;
      ddsrt_mutex_lock (&gpe->e.lock);
      if (gpe->c.type == NULL)
        gpe->c.type = ddsi_sertype_ref (tlm->sertype);
      ddsrt_mutex_unlock (&gpe->e.lock);
    }
  }
  thread_state_asleep (lookup_thread_state ());
}

void ddsi_tl_meta_register_with_proxy_endpoints (struct ddsi_domaingv *gv, const struct ddsi_sertype *type)
{
  type_identifier_t *type_id = ddsi_typeid_from_sertype (type);
  if (type_id)
  {
    ddsrt_mutex_lock (&gv->tl_admin_lock);
    struct tl_meta *tlm = ddsi_tl_meta_lookup_locked (gv, type_id);
    tlm_register_with_proxy_endpoints_locked (gv, tlm);
    ddsrt_mutex_unlock (&gv->tl_admin_lock);
    ddsrt_free (type_id);
  }
}

void ddsi_tl_handle_reply (struct ddsi_domaingv *gv, struct ddsi_serdata *sample_common)
{
  struct generic_proxy_endpoint **gpe_match_upd = NULL;
  uint32_t n = 0, n_match_upd = 0;
  assert (!(sample_common->statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER)));
  const struct ddsi_serdata_pserop *sample = (const struct ddsi_serdata_pserop *) sample_common;
  const type_lookup_reply_t *reply = sample->sample;
  struct ddsi_sertype_default *st = NULL;
  bool resolved = false;

  ddsrt_mutex_lock (&gv->tl_admin_lock);
  GVTRACE ("handle-tl-reply wr "PGUIDFMT " seqnr %"PRIi64" ntypeids %"PRIu32" ", PGUID (reply->writer_guid), reply->sequence_number, reply->types.n);
  while (n < reply->types.n)
  {
    type_identifier_type_object_pair_t r = reply->types.types[n];
    struct tl_meta *tlm = ddsi_tl_meta_lookup_locked (gv, &r.type_identifier);
    if (tlm != NULL && tlm->state == TL_META_REQUESTED && !tlm_endpoints_empty (tlm))
    {
      bool sertype_new = false;
      GVTRACE (" type "PTYPEIDFMT, PTYPEID (r.type_identifier));
      st = ddsrt_malloc (sizeof (*st));
      // assume ddsi_sertype_ops_default at this point, as it should be serialized with the sertype_default serializer
      if (!ddsi_sertype_deserialize (gv, &st->c, &ddsi_sertype_ops_default, r.type_object.length, r.type_object.value))
      {
        GVTRACE (" deser failed\n");
        ddsrt_free (st);
        n++;
        continue;
      }
      ddsrt_mutex_lock (&gv->sertypes_lock);
      struct ddsi_sertype *existing_sertype = ddsi_sertype_lookup_locked (gv, &st->c);
      if (existing_sertype == NULL)
      {
        ddsi_sertype_register_locked (gv, &st->c);
        sertype_new = true;
      }
      ddsi_sertype_unref_locked (gv, &st->c); // unref because both init_from_ser and sertype_lookup/register refcounts the type
      ddsrt_mutex_unlock (&gv->sertypes_lock);

      tlm->state = TL_META_RESOLVED;
      tlm->sertype = &st->c; // refcounted by sertype_register/lookup
      if (sertype_new)
      {
        gpe_match_upd = ddsrt_realloc (gpe_match_upd, (n_match_upd + tlm_endpoints_count (tlm)) * sizeof (*gpe_match_upd));
        tlm_endpoints_iter_t it;
        for (const ddsi_guid_t *ep = tlm_endpoints_iter_first (tlm, &it); ep != NULL; ep = tlm_endpoints_iter_next (&it))
          gpe_match_upd[n_match_upd++] = (struct generic_proxy_endpoint *) entidx_lookup_guid_untyped (gv->entity_index, ep);

        tlm_register_with_proxy_endpoints_locked (gv, tlm);
      }
      resolved = true;
    }
    n++;
  }
  GVTRACE ("\n");
  if (resolved)
    ddsrt_cond_broadcast (&gv->tl_resolved_cond);
  ddsrt_mutex_unlock (&gv->tl_admin_lock);

  if (gpe_match_upd != NULL)
  {
    for (uint32_t e = 0; e < n_match_upd; e++)
    {
      if (gpe_match_upd[e] != NULL)
        update_proxy_endpoint_matching (gv, gpe_match_upd[e]);
    }
    ddsrt_free (gpe_match_upd);
  }
}
