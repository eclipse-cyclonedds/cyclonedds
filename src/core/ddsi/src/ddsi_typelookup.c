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
#include "dds/ddsrt/hopscotch.h"
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
      (void) ddsi_get_builtin_writer (pp, wr_eid, &wr);
  }
  ddsi_entidx_enum_participant_fini (&est);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  return wr;
}

static int32_t tl_request_get_deps (struct ddsi_domaingv * const gv, struct ddsrt_hh *deps, struct ddsrt_hh *visited, int32_t cnt, struct ddsi_type *type)
{
  if (ddsi_type_visit_seen (visited, type))
    return cnt;

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
      if (ddsrt_hh_add (deps, &dep_type->xt.id))
      {
        int32_t add = 1;
        if (dep_type->xt.id.x._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
          add = dep_type->xt.id.x._u.sc_component_id.scc_length;
        cnt = (add > 0 && add <= INT32_MAX - cnt) ? cnt + add : INT32_MAX;
      }
      dep_type->state = DDSI_TYPE_REQUESTED;
    }
    cnt = tl_request_get_deps (gv, deps, visited, cnt, dep_type);
  }
  ddsi_typeid_fini (&tmpl.src_type_id);
  return cnt;
}

static bool deps_typeid_equal (const void *type_id_a, const void *type_id_b)
{
  const ddsi_typeid_t *a = type_id_a;
  const ddsi_typeid_t *b = type_id_b;
  if (a->x._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT && b->x._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
  {
    const DDS_XTypes_StronglyConnectedComponentId *sa = &a->x._u.sc_component_id;
    const DDS_XTypes_StronglyConnectedComponentId *sb = &b->x._u.sc_component_id;
    return ddsi_type_scc_id_same_component_impl (sa, sb);
  }
  return ddsi_typeid_compare (type_id_a, type_id_b) == 0;
}

static uint32_t deps_typeid_hash (const void *type_id)
{
  const ddsi_typeid_t *id = type_id;
  assert (ddsi_typeid_is_hash (id));
  if (id->x._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
    return ddsi_type_scc_id_component_hash_impl (&id->x._u.sc_component_id);
  return ddsi_typeid_hash (id);
}

static int32_t tl_typeid_request_count (const ddsi_typeid_t *type_id)
{
  if (type_id->x._d != DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
    return 1;
  return type_id->x._u.sc_component_id.scc_length > 0 ? type_id->x._u.sc_component_id.scc_length : INT32_MAX;
}

struct tl_scc_import_cache_entry {
  struct DDS_XTypes_StronglyConnectedComponentId scc_id;
  dds_return_t ret;
  bool complete;
};

static bool tl_scc_import_cache_equal (const void *ventry_a, const void *ventry_b)
{
  const struct tl_scc_import_cache_entry *a = ventry_a;
  const struct tl_scc_import_cache_entry *b = ventry_b;
  return ddsi_type_scc_id_same_component_impl (&a->scc_id, &b->scc_id);
}

static uint32_t tl_scc_import_cache_hash (const void *ventry)
{
  const struct tl_scc_import_cache_entry *entry = ventry;
  return ddsi_type_scc_id_component_hash_impl (&entry->scc_id);
}

static void tl_scc_import_cache_free_entry (void *ventry, void *arg)
{
  (void) arg;
  ddsrt_free (ventry);
}

static struct tl_scc_import_cache_entry *tl_scc_import_cache_lookup (
    struct ddsrt_hh *cache,
    const struct DDS_XTypes_StronglyConnectedComponentId *scc_id)
{
  if (cache == NULL)
    return NULL;
  const struct tl_scc_import_cache_entry templ = {
    .scc_id = *scc_id
  };
  return ddsrt_hh_lookup (cache, &templ);
}

static void tl_scc_import_cache_store (
    struct ddsrt_hh *cache,
    const struct DDS_XTypes_StronglyConnectedComponentId *scc_id,
    dds_return_t ret,
    bool complete)
{
  struct tl_scc_import_cache_entry *entry = ddsrt_malloc_s (sizeof (*entry));
  if (entry == NULL)
    return;
  *entry = (struct tl_scc_import_cache_entry) {
    .scc_id = *scc_id,
    .ret = ret,
    .complete = complete
  };
  ddsrt_hh_add_absent (cache, entry);
}

static void tl_scc_import_cache_free (struct ddsrt_hh *cache)
{
  if (cache != NULL)
  {
    ddsrt_hh_enum (cache, tl_scc_import_cache_free_entry, NULL);
    ddsrt_hh_free (cache);
  }
}

static void tl_typeid_copy_request_ids (struct DDS_XTypes_TypeIdentifier *dst, uint32_t *index, const ddsi_typeid_t *type_id)
{
  if (type_id->x._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
  {
    const int32_t scc_length = type_id->x._u.sc_component_id.scc_length;
    assert (scc_length > 0);
    for (int32_t n = 1; n <= scc_length; n++)
    {
      ddsi_typeid_copy_impl (&dst[(*index)++], &type_id->x);
      dst[*index - 1]._u.sc_component_id.scc_index = n;
    }
  }
  else
  {
    ddsi_typeid_copy_impl (&dst[(*index)++], &type_id->x);
  }
}

static dds_return_t create_tl_request_msg (struct ddsi_domaingv * const gv, DDS_Builtin_TypeLookup_Request *request, const struct ddsi_writer *wr, const ddsi_guid_t *proxypp_guid, struct ddsi_type *type, enum ddsi_type_include_deps resolve_deps)
{
  int32_t cnt = 0;
  uint32_t index = 0;
  struct ddsrt_hh *deps = NULL;
  memset (request, 0, sizeof (*request));
  const ddsi_guid_t wr_guid_ext = {
    .prefix = ddsi_hton_guid_prefix (wr->e.guid.prefix),
    .entityid = ddsi_hton_entityid (wr->e.guid.entityid)
  };
  DDSRT_STATIC_ASSERT (sizeof (request->header.requestId.writer_guid) == sizeof (wr_guid_ext));
  memcpy (&request->header.requestId.writer_guid, &wr_guid_ext, sizeof (request->header.requestId.writer_guid));
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
  {
    cnt = tl_typeid_request_count (&type->xt.id);
    if (cnt == INT32_MAX)
      goto err;
  }
  if (resolve_deps == DDSI_TYPE_INCLUDE_DEPS)
  {
    deps = ddsrt_hh_new (1, deps_typeid_hash, deps_typeid_equal);
    struct ddsrt_hh *visited = ddsi_type_visit_new ();
    const int32_t subcnt = tl_request_get_deps (gv, deps, visited, 0, type);
    ddsi_type_visit_free (visited);
    if (subcnt == INT32_MAX || subcnt > INT32_MAX - cnt)
    {
      cnt = INT32_MAX;
      goto err;
    }
    cnt += subcnt;
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
      tl_typeid_copy_request_ids (request->data._u.getTypes.type_ids._buffer, &index, &type->xt.id);
      type->state = DDSI_TYPE_REQUESTED;
    }

    if (resolve_deps == DDSI_TYPE_INCLUDE_DEPS)
    {
      struct ddsrt_hh_iter iter;
      for (ddsi_typeid_t *tid = ddsrt_hh_iter_first (deps, &iter); tid; tid = ddsrt_hh_iter_next (&iter))
        tl_typeid_copy_request_ids (request->data._u.getTypes.type_ids._buffer, &index, tid);
    }
  }

err:
  if (deps)
    ddsrt_hh_free (deps);
  return (cnt == INT32_MAX) ? DDS_RETCODE_ERROR : cnt;
}

bool ddsi_tl_request_type (struct ddsi_domaingv * const gv, const ddsi_typeid_t *type_id, const ddsi_guid_t *proxypp_guid, enum ddsi_type_include_deps deps)
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

  struct ddsi_serdata *serdata;
  dds_return_t rc = ddsi_serdata_from_sample_err (&serdata, gv->tl_svc_request_type, SDK_DATA, &request);
  ddsrt_free (request.data._u.getTypes.type_ids._buffer);
  if (rc != DDS_RETCODE_OK)
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

static void create_tl_reply_msg (DDS_Builtin_TypeLookup_Reply *reply, const struct DDS_SampleIdentity *requestid, const struct DDS_XTypes_TypeIdentifierTypeObjectPairSeq *types)
{
  memset (reply, 0, sizeof (*reply));
  reply->header.relatedRequestId = *requestid;
  reply->header.remoteEx = DDS_RPC_REMOTE_EX_OK;
  reply->return_data._d = DDS_Builtin_TypeLookup_getTypes_HashId;
  reply->return_data._u.getType._d = DDS_RETCODE_OK;
  reply->return_data._u.getType._u.result.types._length = types->_length;
  reply->return_data._u.getType._u.result.types._buffer = types->_buffer;

}

static void write_typelookup_reply (struct ddsi_writer *wr, const struct DDS_SampleIdentity *requestid, const struct DDS_XTypes_TypeIdentifierTypeObjectPairSeq *types)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  DDS_Builtin_TypeLookup_Reply reply;
  create_tl_reply_msg (&reply, requestid, types);
  GVTRACE (" tl-reply ");
  struct ddsi_serdata *serdata;
  dds_return_t rc = ddsi_serdata_from_sample_err (&serdata, gv->tl_svc_reply_type, SDK_DATA, &reply);
  if (rc != DDS_RETCODE_OK)
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

static bool tl_reply_has_type (const struct DDS_XTypes_TypeIdentifierTypeObjectPairSeq *types, const struct DDS_XTypes_TypeIdentifier *type_id)
{
  for (uint32_t n = 0; n < types->_length; n++)
  {
    if (ddsi_typeid_compare_impl (&types->_buffer[n].type_identifier, type_id) == 0)
      return true;
  }
  return false;
}

static dds_return_t tl_reply_append_type (struct DDS_XTypes_TypeIdentifierTypeObjectPairSeq *types, const struct ddsi_type *type)
{
  if (tl_reply_has_type (types, &type->xt.id.x))
    return DDS_RETCODE_OK;

  DDS_XTypes_TypeIdentifierTypeObjectPair *buf =
    ddsrt_realloc_s (types->_buffer, (types->_length + 1) * sizeof (*types->_buffer));
  if (buf == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  types->_buffer = buf;
  ddsi_typeid_copy_impl (&types->_buffer[types->_length].type_identifier, &type->xt.id.x);
  ddsi_xt_get_typeobject_impl (&type->xt, &types->_buffer[types->_length].type_object);
  types->_length++;
  types->_maximum = types->_length;
  return DDS_RETCODE_OK;
}

static dds_return_t tl_reply_append_type_component (struct DDS_XTypes_TypeIdentifierTypeObjectPairSeq *types, const struct ddsi_type *type)
{
  if (type->xt.id.x._d != DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT || type->scc == NULL)
    return tl_reply_append_type (types, type);

  dds_return_t ret = DDS_RETCODE_OK;
  for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < type->scc->n_wire_types; n++)
  {
    const struct ddsi_type *member = type->scc->types[n];
    if (member == NULL || !ddsi_type_resolved_locked (type->gv, member, DDSI_TYPE_IGNORE_DEPS))
      return DDS_RETCODE_BAD_PARAMETER;
    ret = tl_reply_append_type (types, member);
  }
  return ret;
}

static ddsi_guid_t from_guid (const DDS_GUID_t *guid)
{
  ddsi_guid_t ddsi_guid;
  memcpy (&ddsi_guid.prefix, &guid->guidPrefix, sizeof (ddsi_guid.prefix));
  memcpy (&ddsi_guid.entityid, &guid->entityId, sizeof (ddsi_guid.entityid));
  return ddsi_ntoh_guid (ddsi_guid);
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
  if (!ddsi_serdata_to_sample (d, &req, NULL, NULL))
  {
    GVTRACE (" handle-tl-req deserialization failed");
    return;
  }

  if (req.data._d != DDS_Builtin_TypeLookup_getTypes_HashId)
  {
    GVTRACE (" handle-tl-req wr "PGUIDFMT " unknown req-type %"PRIi32, PGUID (from_guid (&req.header.requestId.writer_guid)), req.data._d);
    ddsi_sertype_free_sample (d->type, &req, DDS_FREE_CONTENTS);
    return;
  }

  GVTRACE (" handle-tl-req wr "PGUIDFMT " seqnr %"PRIu64" ntypeids %"PRIu32, PGUID (from_guid (&req.header.requestId.writer_guid)), from_seqno (&req.header.requestId.sequence_number), req.data._u.getTypes.type_ids._length);

  ddsrt_mutex_lock (&gv->typelib_lock);
  struct DDS_XTypes_TypeIdentifierTypeObjectPairSeq types = { 0, 0, NULL, false };
  dds_return_t ret = DDS_RETCODE_OK;
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
      if ((ret = tl_reply_append_type_component (&types, type)) != DDS_RETCODE_OK)
        break;
    }
  }
  ddsrt_mutex_unlock (&gv->typelib_lock);

  struct ddsi_writer *wr = get_typelookup_writer (gv, DDSI_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER);
  if (ret != DDS_RETCODE_OK)
    GVTRACE (" failed to construct tl-reply");
  else if (wr != NULL)
    write_typelookup_reply (wr, &req.header.requestId, &types);
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
  struct ddsrt_hh *scc_import_cache = NULL;
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

    if (r.type_identifier._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
    {
      dds_return_t ret;
      bool complete;
      struct tl_scc_import_cache_entry *cache_entry =
        tl_scc_import_cache_lookup (scc_import_cache, &r.type_identifier._u.sc_component_id);
      const dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *pairs =
        (const dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *) &reply->return_data._u.getType._u.result.types;
      if (cache_entry != NULL)
      {
        ret = cache_entry->ret;
        complete = cache_entry->complete;
        GVTRACE (" cached SCC import ret=%d complete=%d", ret, complete);
      }
      else
      {
        complete = false;
        ret = ddsi_type_add_scc_typeobjs_locked (gv, pairs, &r.type_identifier, false, &complete);
        /* This cache is only an optimization for one immutable reply while
           typelib_lock is held.  If allocating the table or entry fails, there
           is simply no cached result and a later slot falls back to retrying
           the import exactly as it did before this cache existed.  If a later
           allocation succeeds, the result it stores is still valid for the same
           reply/component pair; allocation failure never changes the reply or
           the component identity. */
        if (scc_import_cache == NULL)
          scc_import_cache = ddsrt_hh_new (1, tl_scc_import_cache_hash, tl_scc_import_cache_equal);
        if (scc_import_cache != NULL)
          tl_scc_import_cache_store (scc_import_cache, &r.type_identifier._u.sc_component_id, ret, complete);
      }
      if (ret == DDS_RETCODE_OK && complete)
      {
        GVTRACE (" resolved SCC type %s\n", ddsi_make_typeid_str_impl (&str, &r.type_identifier));
        ddsi_type_get_gpe_matches (gv, type, gpe_match_upd, n_match_upd);
        resolved = true;
      }
      else
      {
        GVTRACE (" incomplete or invalid SCC component\n");
      }
    }
    else if (ddsi_type_add_typeobj (gv, type, &r.type_object) == DDS_RETCODE_OK)
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
    ddsrt_cond_etime_broadcast (&gv->typelib_resolved_cond);
  ddsrt_mutex_unlock (&gv->typelib_lock);
  tl_scc_import_cache_free (scc_import_cache);
}

void ddsi_tl_handle_reply (struct ddsi_domaingv *gv, struct ddsi_serdata *d)
{
  struct ddsi_generic_proxy_endpoint **gpe_match_upd = NULL;
  uint32_t n_match_upd = 0;
  assert (!(d->statusinfo & (DDSI_STATUSINFO_DISPOSE | DDSI_STATUSINFO_UNREGISTER)));

  DDS_Builtin_TypeLookup_Reply reply;
  memset (&reply, 0, sizeof (reply));
  if (!ddsi_serdata_to_sample (d, &reply, NULL, NULL))
  {
    GVTRACE (" handle-tl-req deserialization failed");
    return;
  }
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
