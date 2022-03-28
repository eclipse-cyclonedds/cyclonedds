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
#include "dds/features.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_serdata_default.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_plist_generic.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_xt_impl.h"
#include "dds/ddsi/ddsi_xt_typelookup.h"
#include "dds/ddsi/ddsi_typelookup.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/ddsi/q_gc.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_protocol.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/q_transmit.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/q_misc.h"

static struct writer *get_typelookup_writer (const struct ddsi_domaingv *gv, uint32_t wr_eid)
{
  struct participant *pp;
  struct writer *wr = NULL;
  struct entidx_enum_participant est;
  thread_state_awake (lookup_thread_state (), gv);
  entidx_enum_participant_init (&est, gv->entity_index);
  while (wr == NULL && (pp = entidx_enum_participant_next (&est)) != NULL)
  {
    if (participant_builtin_writers_ready (pp))
      wr = get_builtin_writer (pp, wr_eid);
  }
  entidx_enum_participant_fini (&est);
  thread_state_asleep (lookup_thread_state ());
  return wr;
}

bool ddsi_tl_request_type (struct ddsi_domaingv * const gv, const ddsi_typeid_t *type_id, const ddsi_typeid_t ** dependent_type_ids, uint32_t dependent_type_id_count)
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
  else if (!dependent_type_id_count && (type->state == DDSI_TYPE_REQUESTED || type->xt.has_obj))
  {
    // type lookup is pending or the type is already resolved, so we'll return true
    // to indicate that the type request is done (or not required)
    GVTRACE ("state not-new for %s\n", ddsi_make_typeid_str (&tidstr, type_id));
    ddsrt_mutex_unlock (&gv->typelib_lock);
    return true;
  }

  struct writer *wr = get_typelookup_writer (gv, NN_ENTITYID_TL_SVC_BUILTIN_REQUEST_WRITER);
  if (wr == NULL)
  {
    GVTRACE ("no pp found with tl request writer");
    ddsrt_mutex_unlock (&gv->typelib_lock);
    return false;
  }

  DDS_Builtin_TypeLookup_Request request;
  memset (&request, 0, sizeof (request));
  memcpy (&request.header.requestId.writer_guid.guidPrefix, &wr->e.guid.prefix, sizeof (request.header.requestId.writer_guid.guidPrefix));
  memcpy (&request.header.requestId.writer_guid.entityId, &wr->e.guid.entityid, sizeof (request.header.requestId.writer_guid.entityId));
  type->request_seqno++;
  request.header.requestId.sequence_number.high = (int32_t) (type->request_seqno >> 32);
  request.header.requestId.sequence_number.low = (uint32_t) type->request_seqno;
  (void) snprintf (request.header.instanceName, sizeof (request.header.instanceName), "dds.builtin.TOS.%08"PRIx32 "%08"PRIx32 "%08"PRIx32 "%08"PRIx32,
    wr->c.pp->e.guid.prefix.u[0], wr->c.pp->e.guid.prefix.u[1], wr->c.pp->e.guid.prefix.u[2], wr->c.pp->e.guid.entityid.u);
  request.data._d = DDS_Builtin_TypeLookup_getTypes_HashId;
  request.data._u.getTypes.type_ids._length = 1 + dependent_type_id_count;
  request.data._u.getTypes.type_ids._buffer = ddsrt_malloc ((dependent_type_id_count + 1) * sizeof (*request.data._u.getTypes.type_ids._buffer));
  ddsi_typeid_copy_impl (&request.data._u.getTypes.type_ids._buffer[0], &type_id->x);
  for (uint32_t n = 0; n < dependent_type_id_count; n++)
    ddsi_typeid_copy_impl (&request.data._u.getTypes.type_ids._buffer[n + 1], &dependent_type_ids[n]->x);

  struct ddsi_serdata *serdata = ddsi_serdata_from_sample (gv->tl_svc_request_type, SDK_DATA, &request);
  ddsrt_free (request.data._u.getTypes.type_ids._buffer);
  if (!serdata)
  {
    GVTRACE (" from_sample failed\n");
    ddsrt_mutex_unlock (&gv->typelib_lock);
    return false;
  }
  serdata->timestamp = ddsrt_time_wallclock ();
  type->state = DDSI_TYPE_REQUESTED;
  ddsrt_mutex_unlock (&gv->typelib_lock);

  thread_state_awake (lookup_thread_state (), gv);
  GVTRACE ("wr "PGUIDFMT" typeid %s\n", PGUID (wr->e.guid), ddsi_make_typeid_str (&tidstr, type_id));
  struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, serdata);
  write_sample_gc (lookup_thread_state (), NULL, wr, serdata, tk);
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
  thread_state_asleep (lookup_thread_state ());

  return true;
}

static void write_typelookup_reply (struct writer *wr, seqno_t seqno, struct DDS_XTypes_TypeIdentifierTypeObjectPairSeq *types)
{
  struct ddsi_domaingv * const gv = wr->e.gv;
  DDS_Builtin_TypeLookup_Reply reply;
  memset (&reply, 0, sizeof (reply));

  GVTRACE (" tl-reply ");
  memcpy (&reply.header.requestId.writer_guid.guidPrefix, &wr->e.guid.prefix, sizeof (reply.header.requestId.writer_guid.guidPrefix));
  memcpy (&reply.header.requestId.writer_guid.entityId, &wr->e.guid.entityid, sizeof (reply.header.requestId.writer_guid.entityId));
  reply.header.requestId.sequence_number.high = (int32_t) (seqno >> 32);
  reply.header.requestId.sequence_number.low = (uint32_t) seqno;
  (void) snprintf (reply.header.instanceName, sizeof (reply.header.instanceName), "dds.builtin.TOS.%08"PRIx32 "%08"PRIx32 "%08"PRIx32 "%08"PRIx32,
    wr->c.pp->e.guid.prefix.u[0], wr->c.pp->e.guid.prefix.u[1], wr->c.pp->e.guid.prefix.u[2], wr->c.pp->e.guid.entityid.u);
  reply.return_data._d = DDS_Builtin_TypeLookup_getTypes_HashId;
  reply.return_data._u.getType._d = DDS_RETCODE_OK;
  reply.return_data._u.getType._u.result.types._length = types->_length;
  reply.return_data._u.getType._u.result.types._buffer = types->_buffer;
  struct ddsi_serdata *serdata = ddsi_serdata_from_sample (gv->tl_svc_reply_type, SDK_DATA, &reply);
  if (!serdata)
  {
    GVTRACE (" from_sample failed\n");
    return;
  }
  serdata->timestamp = ddsrt_time_wallclock ();

  GVTRACE ("wr "PGUIDFMT"\n", PGUID (wr->e.guid));
  struct ddsi_tkmap_instance *tk = ddsi_tkmap_lookup_instance_ref (gv->m_tkmap, serdata);
  write_sample_gc (lookup_thread_state (), NULL, wr, serdata, tk);
  ddsi_tkmap_instance_unref (gv->m_tkmap, tk);
}

static ddsi_guid_t from_guid (const DDS_GUID_t *guid)
{
  ddsi_guid_t ddsi_guid;
  memcpy (&ddsi_guid.prefix, &guid->guidPrefix, sizeof (ddsi_guid.prefix));
  memcpy (&ddsi_guid.entityid, &guid->entityId, sizeof (ddsi_guid.entityid));
  return ddsi_guid;
}

static seqno_t from_seqno (const DDS_SequenceNumber *seqno)
{
  return fromSN((nn_sequence_number_t){ .high = seqno->high, .low = seqno->low });
}

void ddsi_tl_handle_request (struct ddsi_domaingv *gv, struct ddsi_serdata *d)
{
  assert (!(d->statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER)));

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
    if (type && type->xt.has_obj)
    {
      types._buffer = ddsrt_realloc (types._buffer, (types._length + 1) * sizeof (*types._buffer));
      ddsi_typeid_copy_impl (&types._buffer[types._length].type_identifier, type_id);
      ddsi_xt_get_typeobject_impl (&type->xt, &types._buffer[types._length].type_object);
      types._length++;
    }
  }
  ddsrt_mutex_unlock (&gv->typelib_lock);

  struct writer *wr = get_typelookup_writer (gv, NN_ENTITYID_TL_SVC_BUILTIN_REPLY_WRITER);
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

void ddsi_tl_handle_reply (struct ddsi_domaingv *gv, struct ddsi_serdata *d)
{
  struct generic_proxy_endpoint **gpe_match_upd = NULL;
  uint32_t n_match_upd = 0;
  assert (!(d->statusinfo & (NN_STATUSINFO_DISPOSE | NN_STATUSINFO_UNREGISTER)));

  DDS_Builtin_TypeLookup_Reply reply;
  memset (&reply, 0, sizeof (reply));
  ddsi_serdata_to_sample (d, &reply, NULL, NULL);
  if (reply.return_data._d != DDS_Builtin_TypeLookup_getTypes_HashId)
  {
    GVTRACE (" handle-tl-reply wr "PGUIDFMT " unknown reply-type %"PRIi32, PGUID (from_guid (&reply.header.requestId.writer_guid)), reply.return_data._d);
    ddsi_sertype_free_sample (d->type, &reply, DDS_FREE_CONTENTS);
    return;
  }

  bool resolved = false;
  ddsrt_mutex_lock (&gv->typelib_lock);
  GVTRACE ("handle-tl-reply wr "PGUIDFMT " seqnr %"PRIu64" ntypeids %"PRIu32"\n", PGUID (from_guid (&reply.header.requestId.writer_guid)), from_seqno (&reply.header.requestId.sequence_number), reply.return_data._u.getType._u.result.types._length);
  for (uint32_t n = 0; n < reply.return_data._u.getType._u.result.types._length; n++)
  {
    struct ddsi_typeid_str str;
    DDS_XTypes_TypeIdentifierTypeObjectPair r = reply.return_data._u.getType._u.result.types._buffer[n];
    GVTRACE (" type %s", ddsi_make_typeid_str_impl (&str, &r.type_identifier));
    struct ddsi_type *type = ddsi_type_lookup_locked_impl (gv, &r.type_identifier);
    if (!type)
    {
      /* received a typelookup reply for a type we don't know, so the type
         object should not be stored as there is no endpoint using this type */
      continue;
    }
    if (type->xt.has_obj)
    {
      GVTRACE (" already resolved\n");
      continue;
    }

    if (ddsi_xt_type_add_typeobj (gv, &type->xt, &r.type_object) == 0)
    {
      type->state = DDSI_TYPE_RESOLVED;
      if (ddsi_typeid_is_minimal_impl (&r.type_identifier))
      {
        GVTRACE (" resolved minimal type %p\n", type);
        if (ddsi_type_get_gpe_matches (gv, type, &gpe_match_upd, &n_match_upd))
          resolved = true;
      }
      else
      {
        GVTRACE (" resolved complete type %p\n", type);

        // FIXME: create sertype from received (complete) type object, check if it exists and register if not
        // bool sertype_new = false;
        // struct ddsi_sertype *st = ...
        // ddsrt_mutex_lock (&gv->sertypes_lock);
        // struct ddsi_sertype *existing_sertype = ddsi_sertype_lookup_locked (gv, &st->c);
        // if (existing_sertype == NULL)
        // {
        //   ddsi_sertype_register_locked (gv, &st->c);
        //   sertype_new = true;
        // }
        // ddsi_sertype_unref_locked (gv, &st->c); // unref because both init_from_ser and sertype_lookup/register refcounts the type
        // ddsrt_mutex_unlock (&gv->sertypes_lock);
        // type->sertype = &st->c; // refcounted by sertype_register/lookup

        if (ddsi_type_get_gpe_matches (gv, type, &gpe_match_upd, &n_match_upd))
          resolved = true;
      }
    }
  }
  if (resolved)
    ddsrt_cond_broadcast (&gv->typelib_resolved_cond);
  ddsrt_mutex_unlock (&gv->typelib_lock);

  ddsi_sertype_free_sample (d->type, &reply, DDS_FREE_CONTENTS);

  if (gpe_match_upd != NULL)
  {
    for (uint32_t e = 0; e < n_match_upd; e++)
    {
      GVTRACE (" trigger matching "PGUIDFMT"\n", PGUID(gpe_match_upd[e]->e.guid));
      update_proxy_endpoint_matching (gv, gpe_match_upd[e]);
    }
    ddsrt_free (gpe_match_upd);
  }
}
