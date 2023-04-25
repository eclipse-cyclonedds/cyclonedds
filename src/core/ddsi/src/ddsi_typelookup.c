// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include <stdlib.h>
#include "dds/features.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_xt_typelookup.h"
#include "dds/ddsi/ddsi_typebuilder.h"
#include "dds/ddsi/ddsi_gc.h"
#include "ddsi__plist_generic.h"
#include "ddsi__entity_index.h"
#include "ddsi__typelookup.h"
#include "ddsi__xt_impl.h"
#include "ddsi__entity.h"
#include "ddsi__endpoint_match.h"
#include "ddsi__participant.h"
#include "ddsi__protocol.h"
#include "ddsi__radmin.h"
#include "ddsi__transmit.h"
#include "ddsi__xmsg.h"
#include "ddsi__misc.h"
#include "ddsi__typelib.h"
#include "dds/cdr/dds_cdrstream.h"

static bool participant_builtin_writers_ready (struct ddsi_participant *pp)
{
  // lock is needed to read the state, we're fine even if the state flips
  // from operational to deleting, this exists to protect against the gap
  // between making the participant discoverable through the entity index
  // and checking pp->bes
  ddsrt_mutex_lock (&pp->refc_lock);
  const bool x = pp->state >= DDSI_PARTICIPANT_STATE_OPERATIONAL;
  ddsrt_mutex_unlock (&pp->refc_lock);
  return x;
}

static struct ddsi_writer *get_typelookup_writer (const struct ddsi_domaingv *gv, uint32_t wr_eid)
{
  struct ddsi_participant *pp;
  struct ddsi_writer *wr = NULL;
  struct ddsi_entity_enum_participant est;
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
  ddsi_entidx_enum_participant_init (&est, gv->entity_index);
  while (wr == NULL && (pp = ddsi_entidx_enum_participant_next (&est)) != NULL)
  {
    if (participant_builtin_writers_ready (pp))
      wr = ddsi_get_builtin_writer (pp, wr_eid);
  }
  ddsi_entidx_enum_participant_fini (&est);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  return wr;
}

static int32_t tl_request_get_deps (struct ddsi_domaingv * const gv, struct ddsrt_hh *deps, int32_t cnt, struct ddsi_type *type)
{
  struct ddsi_type_dep tmpl, *dep = &tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.src_type_id, &type->xt.id);

  ddsrt_avl_iter_t it;
  for (dep = ddsrt_avl_iter_succ (&ddsi_typedeps_treedef, &gv->typedeps, &it, dep); dep && !ddsi_typeid_compare (&type->xt.id, &dep->src_type_id) && cnt < INT32_MAX; dep = ddsrt_avl_iter_next (&it))
  {
    struct ddsi_type *dep_type = ddsi_type_lookup_locked (gv, &dep->dep_type_id);
    assert (dep_type);
    if (!ddsi_type_resolved_locked (gv, dep_type, DDSI_TYPE_IGNORE_DEPS))
    {
      assert (ddsi_typeid_is_hash (&dep_type->xt.id));
      ddsrt_hh_add (deps, &dep_type->xt.id);
      cnt++;
      dep_type->state = DDSI_TYPE_REQUESTED;
    }
    cnt = tl_request_get_deps (gv, deps, cnt, dep_type);
  }
  ddsi_typeid_fini (&tmpl.src_type_id);
  return cnt;
}

static int deps_typeid_equal (const void *type_id_a, const void *type_id_b)
{
  return !ddsi_typeid_compare (type_id_a, type_id_b);
}

static uint32_t deps_typeid_hash (const void *type_id)
{
  uint32_t hash32;
  DDS_XTypes_EquivalenceHash hash;
  assert (ddsi_typeid_is_hash (type_id));
  ddsi_typeid_get_equivalence_hash (type_id, &hash);
  memcpy (&hash32, hash, sizeof (hash32));
  return hash32;
}

static dds_return_t create_tl_request_msg (struct ddsi_domaingv * const gv, DDS_Builtin_TypeLookup_Request *request, const struct ddsi_writer *wr, const ddsi_guid_t *proxypp_guid, struct ddsi_type *type, ddsi_type_include_deps_t resolve_deps)
{
  int32_t cnt = 0;
  uint32_t index = 0;
  struct ddsrt_hh *deps = NULL;
  memset (request, 0, sizeof (*request));
  memcpy (&request->header.requestId.writer_guid.guidPrefix, &wr->e.guid.prefix, sizeof (request->header.requestId.writer_guid.guidPrefix));
  memcpy (&request->header.requestId.writer_guid.entityId, &wr->e.guid.entityid, sizeof (request->header.requestId.writer_guid.entityId));
  /* For the (DDS-RPC) sample identity, we'll use the sequence number of the top-level
     type that requires a lookup, even if the top-level type itself is resolved and only
     one or more of its dependencies need to be resolved. When handling the reply, there
     is (currently) no need to correlate the reply message to a specific request. */
  request->header.requestId.sequence_number.high = (int32_t) (type->request_seqno >> 32);
  request->header.requestId.sequence_number.low = (uint32_t) type->request_seqno;
  const ddsi_guid_t *instance_name_guid = proxypp_guid ? proxypp_guid : &ddsi_nullguid;
  (void) snprintf (request->header.instanceName, sizeof (request->header.instanceName), "dds.builtin.TOS.%08"PRIx32 "%08"PRIx32 "%08"PRIx32 "%08"PRIx32,
    instance_name_guid->prefix.u[0], instance_name_guid->prefix.u[1], instance_name_guid->prefix.u[2], instance_name_guid->entityid.u);
  request->data._d = DDS_Builtin_TypeLookup_getTypes_HashId;

  if (!ddsi_type_resolved_locked (gv, type, DDSI_TYPE_IGNORE_DEPS))
    cnt++;
  if (resolve_deps == DDSI_TYPE_INCLUDE_DEPS)
  {
    deps = ddsrt_hh_new (1, deps_typeid_hash, deps_typeid_equal);
    cnt += tl_request_get_deps (gv, deps, 0, type);
  }
  request->data._u.getTypes.type_ids._length = (uint32_t) cnt;
  if (cnt > 0)
  {
    if ((request->data._u.getTypes.type_ids._buffer = ddsrt_malloc ((uint32_t) cnt * sizeof (*request->data._u.getTypes.type_ids._buffer))) == NULL)
    {
      cnt = DDS_RETCODE_OUT_OF_RESOURCES;
      goto err;
    }

    if (!ddsi_type_resolved_locked (gv, type, DDSI_TYPE_IGNORE_DEPS))
    {
      ddsi_typeid_copy_impl (&request->data._u.getTypes.type_ids._buffer[index++], &type->xt.id.x);
      type->state = DDSI_TYPE_REQUESTED;
    }

    if (resolve_deps == DDSI_TYPE_INCLUDE_DEPS)
    {
      struct ddsrt_hh_iter iter;
      for (ddsi_typeid_t *tid = ddsrt_hh_iter_first (deps, &iter); tid; tid = ddsrt_hh_iter_next (&iter))
        ddsi_typeid_copy_impl (&request->data._u.getTypes.type_ids._buffer[index++], &tid->x);
    }
  }

err:
  if (resolve_deps == DDSI_TYPE_INCLUDE_DEPS)
    ddsrt_hh_free (deps);
  return (dds_return_t) cnt;
}

bool ddsi_tl_request_type (struct ddsi_domaingv * const gv, const ddsi_typeid_t *type_id, const ddsi_guid_t *proxypp_guid, ddsi_type_include_deps_t deps)
{
  struct ddsi_typeid_str tidstr;
  assert (ddsi_typeid_is_hash (type_id));
  ddsrt_mutex_lock (&gv->typelib_lock);
  struct ddsi_type *type = ddsi_type_lookup_locked (gv, type_id);
  GVTRACE ("tl-req ");
  if (!type)
  {
    GVTRACE ("cannot find %s\n", ddsi_make_typeid_str (&tidstr, type_id));
    ddsrt_mutex_unlock (&gv->typelib_lock);
    return false;
  }

  if (deps != DDSI_TYPE_INCLUDE_DEPS && (type->state == DDSI_TYPE_REQUESTED || ddsi_type_resolved_locked (gv, type, DDSI_TYPE_IGNORE_DEPS)))
  {
    // type lookup is pending or the type is already resolved, so we'll return true
    // to indicate that the type request is done (or not required)
    GVTRACE ("%s is %s\n", ddsi_make_typeid_str (&tidstr, type_id), type->state == DDSI_TYPE_REQUESTED ? "requested" : "resolved");
    ddsrt_mutex_unlock (&gv->typelib_lock);
    return true;
  }

  struct ddsi_writer *wr = get_typelookup_writer (gv, DDSI_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER);
  if (wr == NULL)
  {
    GVTRACE ("no pp found with tl request writer");
    ddsrt_mutex_unlock (&gv->typelib_lock);
    return false;
  }

  DDS_Builtin_TypeLookup_Request request;
  type->request_seqno++;
  dds_return_t n = create_tl_request_msg (gv, &request, wr, proxypp_guid, type, deps);
  if (n <= 0)
  {
    GVTRACE (n == 0 ? "no resolvable types" : "out of memory");
    ddsrt_mutex_unlock (&gv->typelib_lock);
    return false;
  }

  struct ddsi_serdata *serdata = ddsi_serdata_from_sample (gv->tl_svc_request_type, SDK_DATA, &request);
  ddsrt_free (request.data._u.getTypes.type_ids._buffer);
  if (!serdata)
  {
    GVTRACE (" from_sample failed\n");
    ddsrt_mutex_unlock (&gv->typelib_lock);
    return false;
  }
  serdata->timestamp = ddsrt_time_wallclock ();
  ddsrt_mutex_unlock (&gv->typelib_lock);

  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
  GVTRACE ("wr "PGUIDFMT" typeid %s\n", PGUID (wr->e.guid), ddsi_make_typeid_str (&tidstr, type_id));
  struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, serdata);
  ddsi_write_sample_gc (ddsi_lookup_thread_state (), NULL, wr, serdata, tk);
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());

  return true;
}

static void create_tl_reply_msg (DDS_Builtin_TypeLookup_Reply *reply, const struct ddsi_writer *wr, ddsi_seqno_t seqno, const struct DDS_XTypes_TypeIdentifierTypeObjectPairSeq *types)
{
  memset (reply, 0, sizeof (*reply));
  memcpy (&reply->header.relatedRequestId.writer_guid.guidPrefix, &wr->e.guid.prefix, sizeof (reply->header.relatedRequestId.writer_guid.guidPrefix));
  memcpy (&reply->header.relatedRequestId.writer_guid.entityId, &wr->e.guid.entityid, sizeof (reply->header.relatedRequestId.writer_guid.entityId));
  reply->header.relatedRequestId.sequence_number.high = (int32_t) (seqno >> 32);
  reply->header.relatedRequestId.sequence_number.low = (uint32_t) seqno;
  reply->header.remoteEx = DDS_RPC_REMOTE_EX_OK;
  reply->return_data._d = DDS_Builtin_TypeLookup_getTypes_HashId;
  reply->return_data._u.getType._d = DDS_RETCODE_OK;
  reply->return_data._u.getType._u.result.types._length = types->_length;
  reply->return_data._u.getType._u.result.types._buffer = types->_buffer;

}

static void write_typelookup_reply (struct ddsi_writer *wr, ddsi_seqno_t seqno, const struct DDS_XTypes_TypeIdentifierTypeObjectPairSeq *types)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  DDS_Builtin_TypeLookup_Reply reply;
  create_tl_reply_msg (&reply, wr, seqno, types);
  GVTRACE (" tl-reply ");
  struct ddsi_serdata *serdata = ddsi_serdata_from_sample (gv->tl_svc_reply_type, SDK_DATA, &reply);
  if (!serdata)
  {
    GVTRACE (" from_sample failed\n");
    return;
  }
  serdata->timestamp = ddsrt_time_wallclock ();

  GVTRACE ("wr "PGUIDFMT"\n", PGUID (wr->e.guid));
  struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, serdata);
  ddsi_write_sample_gc (ddsi_lookup_thread_state (), NULL, wr, serdata, tk);
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
}

static ddsi_guid_t from_guid (const DDS_GUID_t *guid)
{
  ddsi_guid_t ddsi_guid;
  memcpy (&ddsi_guid.prefix, &guid->guidPrefix, sizeof (ddsi_guid.prefix));
  memcpy (&ddsi_guid.entityid, &guid->entityId, sizeof (ddsi_guid.entityid));
  return ddsi_guid;
}

static ddsi_seqno_t from_seqno (const DDS_SequenceNumber *seqno)
{
  return ddsi_from_seqno((ddsi_sequence_number_t){ .high = seqno->high, .low = seqno->low });
}

void ddsi_tl_handle_request (struct ddsi_domaingv *gv, struct ddsi_serdata *d)
{
  assert (!(d->statusinfo & (DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER)));

  DDS_Builtin_TypeLookup_Request req;
  memset (&req, 0, sizeof (req));
  ddsi_serdata_to_sample (d, &req, NULL, NULL);
  if (req.data._d != DDS_Builtin_TypeLookup_getTypes_HashId)
  {
    GVTRACE (" handle-tl-req wr "PGUIDFMT " unknown req-type %"PRIi32, PGUID (from_guid (&req.header.requestId.writer_guid)), req.data._d);
    ddsi_sertype_free_sample (d->type, &req, DDS_FREE_CONTENTS);
    return;
  }

  GVTRACE (" handle-tl-req wr "PGUIDFMT " seqnr %"PRIu64" ntypeids %"PRIu32, PGUID (from_guid (&req.header.requestId.writer_guid)), from_seqno (&req.header.requestId.sequence_number), req.data._u.getTypes.type_ids._length);

  ddsrt_mutex_lock (&gv->typelib_lock);
  struct DDS_XTypes_TypeIdentifierTypeObjectPairSeq types = { 0, 0, NULL, false };
  for (uint32_t n = 0; n < req.data._u.getTypes.type_ids._length; n++)
  {
    struct ddsi_typeid_str tidstr;
    struct DDS_XTypes_TypeIdentifier *type_id = &req.data._u.getTypes.type_ids._buffer[n];
    if (!ddsi_typeid_is_hash_impl (type_id))
    {
      GVTRACE (" non-hash id %s", ddsi_make_typeid_str_impl (&tidstr, type_id));
      continue;
    }
    GVTRACE (" id %s", ddsi_make_typeid_str_impl (&tidstr, type_id));
    const struct ddsi_type *type = ddsi_type_lookup_locked_impl (gv, type_id);
    if (type && ddsi_type_resolved_locked (gv, type, DDSI_TYPE_IGNORE_DEPS))
    {
      types._buffer = ddsrt_realloc (types._buffer, (types._length + 1) * sizeof (*types._buffer));
      ddsi_typeid_copy_impl (&types._buffer[types._length].type_identifier, type_id);
      ddsi_xt_get_typeobject_impl (&type->xt, &types._buffer[types._length].type_object);
      types._length++;
    }
  }
  ddsrt_mutex_unlock (&gv->typelib_lock);

  struct ddsi_writer *wr = get_typelookup_writer (gv, DDSI_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER);
  if (wr != NULL)
    write_typelookup_reply (wr, from_seqno (&req.header.requestId.sequence_number), &types);
  else
    GVTRACE (" no tl-reply writer");

  ddsi_sertype_free_sample (d->type, &req, DDS_FREE_CONTENTS);
  for (uint32_t n = 0; n < types._length; n++)
  {
    ddsi_typeid_fini_impl (&types._buffer[n].type_identifier);
    ddsi_typeobj_fini_impl (&types._buffer[n].type_object);
  }
  ddsrt_free (types._buffer);
}

void ddsi_tl_add_types (struct ddsi_domaingv *gv, const DDS_Builtin_TypeLookup_Reply *reply, struct ddsi_generic_proxy_endpoint ***gpe_match_upd, uint32_t *n_match_upd)
{
  bool resolved = false;
  ddsrt_mutex_lock (&gv->typelib_lock);
  /* No need to correlate the sample identity of the incoming reply with the request
     that was sent, because the reply itself contains the type-id to type object mapping
     and we're not interested in what specific reply results in resolving a type */
  GVTRACE ("tl-reply-add-types wr "PGUIDFMT " seqnr %"PRIu64" ntypeids %"PRIu32"\n", PGUID (from_guid (&reply->header.relatedRequestId.writer_guid)),
      from_seqno (&reply->header.relatedRequestId.sequence_number), reply->return_data._u.getType._u.result.types._length);
  for (uint32_t n = 0; n < reply->return_data._u.getType._u.result.types._length; n++)
  {
    struct ddsi_typeid_str str;
    DDS_XTypes_TypeIdentifierTypeObjectPair r = reply->return_data._u.getType._u.result.types._buffer[n];
    GVTRACE (" type %s", ddsi_make_typeid_str_impl (&str, &r.type_identifier));
    struct ddsi_type *type = ddsi_type_lookup_locked_impl (gv, &r.type_identifier);
    if (!type)
    {
      /* received a typelookup reply for a type we don't know, so the type
         object should not be stored as there is no endpoint using this type */
      continue;
    }
    if (ddsi_type_resolved_locked (gv, type, DDSI_TYPE_IGNORE_DEPS))
    {
      GVTRACE (" already resolved\n");
      continue;
    }

    if (ddsi_type_add_typeobj (gv, type, &r.type_object) == DDS_RETCODE_OK)
    {
      if (ddsi_typeid_is_minimal_impl (&r.type_identifier))
      {
        GVTRACE (" resolved minimal type %s\n", ddsi_make_typeid_str_impl (&str, &r.type_identifier));
        ddsi_type_get_gpe_matches (gv, type, gpe_match_upd, n_match_upd);
        resolved = true;
      }
      else
      {
        GVTRACE (" resolved complete type %s\n", ddsi_make_typeid_str_impl (&str, &r.type_identifier));
        ddsi_type_get_gpe_matches (gv, type, gpe_match_upd, n_match_upd);
        resolved = true;
      }
    }
    else
    {
      GVTRACE (" failed to add typeobj\n");
    }
  }
  if (resolved)
    ddsrt_cond_broadcast (&gv->typelib_resolved_cond);
  ddsrt_mutex_unlock (&gv->typelib_lock);
}

void ddsi_tl_handle_reply (struct ddsi_domaingv *gv, struct ddsi_serdata *d)
{
  struct ddsi_generic_proxy_endpoint **gpe_match_upd = NULL;
  uint32_t n_match_upd = 0;
  assert (!(d->statusinfo & (DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER)));

  DDS_Builtin_TypeLookup_Reply reply;
  memset (&reply, 0, sizeof (reply));
  ddsi_serdata_to_sample (d, &reply, NULL, NULL);
  if (reply.return_data._d != DDS_Builtin_TypeLookup_getTypes_HashId)
  {
    GVTRACE (" handle-tl-reply wr "PGUIDFMT " unknown reply-type %"PRIi32, PGUID (from_guid (&reply.header.relatedRequestId.writer_guid)), reply.return_data._d);
    ddsi_sertype_free_sample (d->type, &reply, DDS_FREE_CONTENTS);
    return;
  }
  ddsi_tl_add_types (gv, &reply, &gpe_match_upd, &n_match_upd);
  ddsi_sertype_free_sample (d->type, &reply, DDS_FREE_CONTENTS);

  if (gpe_match_upd != NULL)
  {
    for (uint32_t e = 0; e < n_match_upd; e++)
    {
      GVTRACE (" trigger matching "PGUIDFMT"\n", PGUID(gpe_match_upd[e]->e.guid));
      ddsi_update_proxy_endpoint_matching (gv, gpe_match_upd[e]);
    }
    ddsrt_free (gpe_match_upd);
  }
}
