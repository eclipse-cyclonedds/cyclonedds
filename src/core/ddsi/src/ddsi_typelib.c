/*
 * Copyright(c) 2006 to 2021 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include "dds/features.h"

#ifdef DDS_HAS_TYPE_DISCOVERY

#include <string.h>
#include <stdlib.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_xt_typemap.h"
#include "dds/ddsi/ddsi_typelookup.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/ddsc/dds_public_impl.h"

DDSI_LIST_DECLS_TMPL(static, ddsi_type_proxy_guid_list, ddsi_guid_t, ddsrt_attribute_unused)
DDSI_LIST_CODE_TMPL(static, ddsi_type_proxy_guid_list, ddsi_guid_t, nullguid, ddsrt_malloc, ddsrt_free)

static int ddsi_type_compare_wrap (const void *type_a, const void *type_b);
const ddsrt_avl_treedef_t ddsi_typelib_treedef = DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_type, avl_node), 0, ddsi_type_compare_wrap, 0);

bool ddsi_typeinfo_equal (const ddsi_typeinfo_t *a, const ddsi_typeinfo_t *b)
{
  if (a == NULL || b == NULL)
    return a == b;
  return ddsi_type_id_with_deps_equal (&a->minimal, &b->minimal) && ddsi_type_id_with_deps_equal (&a->complete, &b->complete);
}

ddsi_typeinfo_t * ddsi_typeinfo_dup (const ddsi_typeinfo_t *src)
{
  ddsi_typeinfo_t *dst = ddsrt_calloc (1, sizeof (*dst));
  ddsi_typeid_copy (&dst->minimal.typeid_with_size.type_id, &src->minimal.typeid_with_size.type_id);
  dst->minimal.dependent_typeid_count = src->minimal.dependent_typeid_count;
  dst->minimal.dependent_typeids._length = dst->minimal.dependent_typeids._maximum = src->minimal.dependent_typeids._length;
  if (dst->minimal.dependent_typeids._length > 0)
  {
    dst->minimal.dependent_typeids._release = true;
    dst->minimal.dependent_typeids._buffer = ddsrt_calloc (dst->minimal.dependent_typeids._length, sizeof (*dst->minimal.dependent_typeids._buffer));
    for (uint32_t n = 0; n < dst->minimal.dependent_typeids._length; n++)
      ddsi_typeid_copy (&dst->minimal.dependent_typeids._buffer[n].type_id, &src->minimal.dependent_typeids._buffer[n].type_id);
  }

  ddsi_typeid_copy (&dst->complete.typeid_with_size.type_id, &src->complete.typeid_with_size.type_id);
  dst->complete.dependent_typeid_count = src->complete.dependent_typeid_count;
  dst->complete.dependent_typeids._length = dst->complete.dependent_typeids._maximum = src->complete.dependent_typeids._length;
  if (dst->complete.dependent_typeids._length > 0)
  {
    dst->complete.dependent_typeids._release = true;
    dst->complete.dependent_typeids._buffer = ddsrt_calloc (dst->complete.dependent_typeids._length, sizeof (*dst->complete.dependent_typeids._buffer));
    for (uint32_t n = 0; n < dst->complete.dependent_typeids._length; n++)
      ddsi_typeid_copy (&dst->complete.dependent_typeids._buffer[n].type_id, &src->complete.dependent_typeids._buffer[n].type_id);
  }

  return dst;
}

void ddsi_typeinfo_deserLE (unsigned char *buf, uint32_t sz, ddsi_typeinfo_t **typeinfo)
{
  unsigned char *data;
  uint32_t srcoff = 0;

  /* Type objects are stored as a LE serialized CDR blob in the topic descriptor */
  DDSRT_WARNING_MSVC_OFF(6326)
  bool bswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  DDSRT_WARNING_MSVC_ON(6326)
  if (bswap)
    data = ddsrt_memdup (buf, sz);
  else
    data = buf;
  if (!dds_stream_normalize_data ((char *) data, &srcoff, sz, bswap, CDR_ENC_VERSION_2, DDS_XTypes_TypeInformation_desc.m_ops))
  {
    ddsrt_free (data);
    *typeinfo = NULL;
    return;
  }

  dds_istream_t is = { .m_buffer = data, .m_index = 0, .m_size = sz, .m_xcdr_version = CDR_ENC_VERSION_2 };
  *typeinfo = ddsrt_calloc (1, sizeof (**typeinfo));
  dds_stream_read (&is, (void *) *typeinfo, DDS_XTypes_TypeInformation_desc.m_ops);
  if (bswap)
    ddsrt_free (data);
}

void ddsi_typeinfo_fini (ddsi_typeinfo_t *typeinfo)
{
  dds_stream_free_sample (typeinfo, DDS_XTypes_TypeInformation_desc.m_ops);
}

static const ddsi_typeobj_t * ddsi_typemap_typeobj (const ddsi_typemap_t *tmap, const ddsi_typeid_t *type_id)
{
  assert (type_id);
  assert (tmap);
  if (!ddsi_typeid_is_hash (type_id))
    return NULL;
  const dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *list = ddsi_typeid_is_minimal (type_id) ?
    &tmap->identifier_object_pair_minimal : &tmap->identifier_object_pair_complete;
  for (uint32_t i = 0; i < list->_length; i++)
  {
    DDS_XTypes_TypeIdentifierTypeObjectPair *pair = &list->_buffer[i];
    if (!ddsi_typeid_compare (type_id, &pair->type_identifier))
      return &pair->type_object;
  }
  return NULL;
}

void ddsi_typemap_deser (unsigned char *buf, uint32_t sz, ddsi_typemap_t **typemap)
{
  unsigned char *data;
  uint32_t srcoff = 0;
  DDSRT_WARNING_MSVC_OFF(6326)
  bool bswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  DDSRT_WARNING_MSVC_ON(6326)
  if (bswap)
  {
    data = ddsrt_memdup (buf, sz);
    if (!dds_stream_normalize_data ((char *) data, &srcoff, sz, bswap, CDR_ENC_VERSION_2, DDS_XTypes_TypeMapping_desc.m_ops))
    {
      ddsrt_free (data);
      *typemap = NULL;
      return;
    }
  }
  else
    data = buf;

  dds_istream_t is = { .m_buffer = data, .m_index = 0, .m_size = sz, .m_xcdr_version = CDR_ENC_VERSION_2 };
  *typemap = ddsrt_calloc (1, sizeof (**typemap));
  dds_stream_read (&is, (void *) *typemap, DDS_XTypes_TypeMapping_desc.m_ops);
  if (bswap)
    ddsrt_free (data);
}

static void ddsi_typemap_fini (ddsi_typemap_t *typemap)
{
  dds_stream_free_sample (typemap, DDS_XTypes_TypeMapping_desc.m_ops);
}

static bool ddsi_type_proxy_guid_exists (struct ddsi_type *type, const ddsi_guid_t *proxy_guid)
{
  struct ddsi_type_proxy_guid_list_iter it;
  for (ddsi_guid_t guid = ddsi_type_proxy_guid_list_iter_first (&type->proxy_guids, &it); !is_null_guid (&guid); guid = ddsi_type_proxy_guid_list_iter_next (&it))
  {
    if (guid_eq (&guid, proxy_guid))
      return true;
  }
  return false;
}

static int ddsi_type_proxy_guids_eq (const struct ddsi_guid a, const struct ddsi_guid b)
{
  return guid_eq (&a, &b);
}

int ddsi_type_compare (const struct ddsi_type *a, const struct ddsi_type *b)
{
  return ddsi_typeid_compare (&a->xt.id, &b->xt.id);
}

static int ddsi_type_compare_wrap (const void *type_a, const void *type_b)
{
  return ddsi_type_compare (type_a, type_b);
}

static void ddsi_type_fini (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  ddsi_xt_type_fini (gv, &type->xt);
  if (type->sertype)
    ddsi_sertype_unref ((struct ddsi_sertype *) type->sertype);
  struct ddsi_type_dep *dep1, *dep = type->deps;
  while (dep)
  {
    ddsi_type_unref_locked (gv, dep->type);
    dep1 = dep->prev;
    ddsrt_free (dep);
    dep = dep1;
  }
#ifndef NDEBUG
  assert (!ddsi_type_proxy_guid_list_count (&type->proxy_guids));
#endif
  ddsrt_free (type);
}

struct ddsi_type * ddsi_type_lookup_locked (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id)
{
  assert (type_id);
  /* The type identifier field is at offset 0 in struct ddsi_type and the compare
     function only uses this field, so we can safely cast to a ddsi_type here. */
  return ddsrt_avl_lookup (&ddsi_typelib_treedef, &gv->typelib, (struct ddsi_type *) type_id);
}

static struct ddsi_type * ddsi_type_new (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id, const ddsi_typeobj_t *type_obj)
{
  assert (!ddsi_typeid_is_none (type_id));
  assert (!ddsi_type_lookup_locked (gv, type_id));
  struct ddsi_type *type = ddsrt_calloc (1, sizeof (*type));
  GVTRACE (" new %p", type);
  if (ddsi_xt_type_init (gv, &type->xt, type_id, type_obj) < 0)
  {
    ddsrt_free (type);
    return NULL;
  }
  ddsrt_avl_insert (&ddsi_typelib_treedef, &gv->typelib, type);
  return type;
}

static bool type_has_dep (const struct ddsi_type *type, const ddsi_typeid_t *dep_type_id)
{
  struct ddsi_type_dep *dep = type->deps;
  while (dep)
  {
    if (!ddsi_typeid_compare (&dep->type->xt.id, dep_type_id))
      return true;
    dep = dep->prev;
  }
  return false;
}

static void type_add_dep (struct ddsi_domaingv *gv, struct ddsi_type *type, const ddsi_typeid_t *dep_type_id, const ddsi_typeobj_t *dep_type_obj, uint32_t *n_match_upd, struct generic_proxy_endpoint ***gpe_match_upd)
{
  GVTRACE ("type "PTYPEIDFMT" add dep "PTYPEIDFMT"\n", PTYPEID (type->xt.id), PTYPEID (*dep_type_id));
  struct ddsi_type *dep_type = ddsi_type_ref_id_locked (gv, dep_type_id);
  assert (dep_type);
  if (dep_type_obj)
  {
    assert (n_match_upd);
    assert (gpe_match_upd);
    ddsi_xt_type_add_typeobj (gv, &dep_type->xt, dep_type_obj);
    dep_type->state = DDSI_TYPE_RESOLVED;
    (void) ddsi_type_get_gpe_matches (gv, type, gpe_match_upd, n_match_upd);
  }
  /* FIXME: This should be using more efficient storage structure, but as the number
     of dependent types is typically rather small, the performance hit is limited */
  struct ddsi_type_dep *tmp = type->deps;
  type->deps = ddsrt_malloc (sizeof (*type->deps));
  type->deps->prev = tmp;
  type->deps->type = dep_type;
}

static void type_add_deps (struct ddsi_domaingv *gv, struct ddsi_type *type, const ddsi_typeinfo_t *type_info, const ddsi_typemap_t *type_map, ddsi_typeid_kind_t kind, uint32_t *n_match_upd, struct generic_proxy_endpoint ***gpe_match_upd)
{
  assert (type_info);
  assert (kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_COMPLETE);
  if ((kind == DDSI_TYPEID_KIND_MINIMAL && type_info->minimal.dependent_typeid_count > 0)
    || (kind == DDSI_TYPEID_KIND_COMPLETE && type_info->complete.dependent_typeid_count > 0))
  {
    const dds_sequence_DDS_XTypes_TypeIdentifierWithSize *dep_ids;
    if (kind == DDSI_TYPEID_KIND_COMPLETE)
      dep_ids = &type_info->complete.dependent_typeids;
    else
      dep_ids = &type_info->minimal.dependent_typeids;

    for (uint32_t n = 0; dep_ids && n < dep_ids->_length; n++)
    {
      const ddsi_typeid_t *dep_type_id = &dep_ids->_buffer[n].type_id;
      if (ddsi_typeid_compare (&type->xt.id, dep_type_id) && !type_has_dep (type, dep_type_id))
      {
        const ddsi_typeobj_t *dep_type_obj = type_map ? ddsi_typemap_typeobj (type_map, dep_type_id) : NULL;
        type_add_dep (gv, type, dep_type_id, dep_type_obj, n_match_upd, gpe_match_upd);
      }
    }
  }
}

struct ddsi_type * ddsi_type_ref_locked (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  assert (type);
  type->refc++;
  GVTRACE ("ref ddsi_type %p refc %"PRIu32"\n", type, type->refc);
  return type;
}

struct ddsi_type * ddsi_type_ref_id_locked (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id)
{
  assert (!ddsi_typeid_is_none (type_id));
  GVTRACE ("ref ddsi_type type-id " PTYPEIDFMT, PTYPEID(*type_id));
  struct ddsi_type *type = ddsi_type_lookup_locked (gv, type_id);
  if (!type)
    type = ddsi_type_new (gv, type_id, NULL);
  type->refc++;
  GVTRACE (" refc %"PRIu32"\n", type->refc);
  return type;
}

struct ddsi_type * ddsi_type_ref_local (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype, ddsi_typeid_kind_t kind)
{
  struct generic_proxy_endpoint **gpe_match_upd = NULL;
  uint32_t n_match_upd = 0;

  assert (sertype != NULL);
  assert (kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_COMPLETE);
  ddsi_typeinfo_t *type_info = ddsi_sertype_typeinfo (sertype);
  if (!type_info)
    return NULL;
  ddsi_typemap_t *type_map = ddsi_sertype_typemap (sertype);
  const ddsi_typeid_t *type_id = (kind == DDSI_TYPEID_KIND_MINIMAL) ? &type_info->minimal.typeid_with_size.type_id : &type_info->complete.typeid_with_size.type_id;
  const ddsi_typeobj_t *type_obj = ddsi_typemap_typeobj (type_map, type_id);
  bool resolved = false;

  GVTRACE ("ref ddsi_type local sertype %p id " PTYPEIDFMT, sertype, PTYPEID(*type_id));

  ddsrt_mutex_lock (&gv->typelib_lock);

  struct ddsi_type *type = ddsi_type_lookup_locked (gv, type_id);
  if (!type)
    type = ddsi_type_new (gv, type_id, type_obj);
  else if (type_obj)
    ddsi_xt_type_add_typeobj (gv, &type->xt, type_obj);
  type->refc++;
  GVTRACE (" refc %"PRIu32"\n", type->refc);

  type_add_deps (gv, type, type_info, type_map, kind, &n_match_upd, &gpe_match_upd);

  if (type->sertype == NULL)
  {
    type->sertype = ddsi_sertype_ref (sertype);
    GVTRACE ("type "PTYPEIDFMT" resolved\n", PTYPEID(*type_id));
    resolved = true;
  }
  if (resolved)
    ddsrt_cond_broadcast (&gv->typelib_resolved_cond);
  ddsrt_mutex_unlock (&gv->typelib_lock);

  if (gpe_match_upd != NULL)
  {
    for (uint32_t e = 0; e < n_match_upd; e++)
    {
      GVTRACE ("type " PTYPEIDFMT " trigger matching "PGUIDFMT"\n", PTYPEID (*type_id), PGUID(gpe_match_upd[e]->e.guid));
      update_proxy_endpoint_matching (gv, gpe_match_upd[e]);
    }
    ddsrt_free (gpe_match_upd);
  }

  ddsi_typemap_fini (type_map);
  ddsrt_free (type_map);
  ddsi_typeinfo_fini (type_info);
  ddsrt_free (type_info);
  return type;
}

struct ddsi_type * ddsi_type_ref_proxy (struct ddsi_domaingv *gv, const ddsi_typeinfo_t *type_info, ddsi_typeid_kind_t kind, const ddsi_guid_t *proxy_guid)
{
  assert (type_info);
  assert (kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_COMPLETE);
  const ddsi_typeid_t *type_id = (kind == DDSI_TYPEID_KIND_MINIMAL) ? &type_info->minimal.typeid_with_size.type_id : &type_info->complete.typeid_with_size.type_id;

  ddsrt_mutex_lock (&gv->typelib_lock);

  GVTRACE ("ref ddsi_type proxy id " PTYPEIDFMT, PTYPEID(*type_id));
  struct ddsi_type *type = ddsi_type_lookup_locked (gv, type_id);
  if (!type)
    type = ddsi_type_new (gv, type_id, NULL);
  type->refc++;
  GVTRACE(" refc %"PRIu32"\n", type->refc);

  type_add_deps (gv, type, type_info, NULL, kind, NULL, NULL);

  if (proxy_guid != NULL && !ddsi_type_proxy_guid_exists (type, proxy_guid))
  {
    ddsi_type_proxy_guid_list_insert (&type->proxy_guids, *proxy_guid);
    GVTRACE ("type "PTYPEIDFMT" add ep "PGUIDFMT"\n", PTYPEID (*type_id), PGUID (*proxy_guid));
  }

  ddsrt_mutex_unlock (&gv->typelib_lock);
  return type;
}

static void ddsi_type_unref_impl_locked (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  assert (type->refc > 0);
  if (--type->refc == 0)
  {
    GVTRACE (" refc 0 remove type ");
    ddsrt_avl_delete (&ddsi_typelib_treedef, &gv->typelib, type);
    ddsi_type_fini (gv, type);
  }
  else
    GVTRACE (" refc %" PRIu32 " ", type->refc);
}

void ddsi_type_unreg_proxy (struct ddsi_domaingv *gv, struct ddsi_type *type, const ddsi_guid_t *proxy_guid)
{
  assert (proxy_guid);
  if (!type)
    return;
  ddsrt_mutex_lock (&gv->typelib_lock);
  GVTRACE ("unreg proxy guid " PGUIDFMT " ddsi_type id " PTYPEIDFMT "\n", PGUID (*proxy_guid), PTYPEID (type->xt.id));
  ddsi_type_proxy_guid_list_remove (&type->proxy_guids, *proxy_guid, ddsi_type_proxy_guids_eq);
  ddsrt_mutex_unlock (&gv->typelib_lock);
}

void ddsi_type_unref (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  if (!type)
    return;
  ddsrt_mutex_lock (&gv->typelib_lock);
  GVTRACE ("unref ddsi_type id " PTYPEIDFMT, PTYPEID (type->xt.id));
  ddsi_type_unref_impl_locked (gv, type);
  ddsrt_mutex_unlock (&gv->typelib_lock);
  GVTRACE ("\n");
}

void ddsi_type_unref_sertype (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype)
{
  assert (sertype);
  ddsrt_mutex_lock (&gv->typelib_lock);

  ddsi_typeid_kind_t kinds[2] = { DDSI_TYPEID_KIND_MINIMAL, DDSI_TYPEID_KIND_COMPLETE };
  for (uint32_t n = 0; n < sizeof (kinds) / sizeof (kinds[0]); n++)
  {
    struct ddsi_type *type;
    ddsi_typeid_t *type_id = ddsi_sertype_typeid (sertype, kinds[n]);
    if (!ddsi_typeid_is_none (type_id) && ((type = ddsi_type_lookup_locked (gv, type_id))))
    {
      GVTRACE ("unref ddsi_type id " PTYPEIDFMT, PTYPEID (type->xt.id));
      ddsi_type_unref_impl_locked (gv, type);
    }
    if (type_id)
    {
      ddsi_typeid_fini (type_id);
      ddsrt_free (type_id);
    }
  }

  ddsrt_mutex_unlock (&gv->typelib_lock);
}

void ddsi_type_unref_locked (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  if (type)
  {
    GVTRACE ("unref ddsi_type id " PTYPEIDFMT, PTYPEID (type->xt.id));
    ddsi_type_unref_impl_locked (gv, type);
  }
}

static void ddsi_type_register_with_proxy_endpoints_locked (struct ddsi_domaingv *gv, const struct ddsi_type *type)
{
  assert (type);
  thread_state_awake (lookup_thread_state (), gv);

  struct ddsi_type_proxy_guid_list_iter proxy_guid_it;
  for (ddsi_guid_t guid = ddsi_type_proxy_guid_list_iter_first (&type->proxy_guids, &proxy_guid_it); !is_null_guid (&guid); guid = ddsi_type_proxy_guid_list_iter_next (&proxy_guid_it))
  {
#ifdef DDS_HAS_TOPIC_DISCOVERY
    /* For proxy topics the type is not registered (in its topic definition),
       becauses (besides that it causes some locking-order trouble) it would
       only be used when searching for topics and at that point it can easily
       be retrieved using the type identifier via a lookup in the type_lookup
       administration. */
    assert (!is_topic_entityid (guid.entityid));
#endif
    struct entity_common *ec;
    if ((ec = entidx_lookup_guid_untyped (gv->entity_index, &guid)) != NULL)
    {
      assert (ec->kind == EK_PROXY_READER || ec->kind == EK_PROXY_WRITER);
      struct generic_proxy_endpoint *gpe = (struct generic_proxy_endpoint *) ec;
      ddsrt_mutex_lock (&gpe->e.lock);
      // FIXME: sertype from endpoint?
      if (gpe->c.type == NULL && type->sertype != NULL)
        gpe->c.type = ddsi_sertype_ref (type->sertype);
      ddsrt_mutex_unlock (&gpe->e.lock);
    }
  }
  thread_state_asleep (lookup_thread_state ());
}

void ddsi_type_register_with_proxy_endpoints (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype)
{
  ddsi_typeid_t *type_id = ddsi_sertype_typeid (sertype, DDSI_TYPEID_KIND_COMPLETE);
  if (ddsi_typeid_is_none (type_id))
    type_id = ddsi_sertype_typeid (sertype, DDSI_TYPEID_KIND_MINIMAL);
  if (!ddsi_typeid_is_none (type_id))
  {
    ddsrt_mutex_lock (&gv->typelib_lock);
    struct ddsi_type *type = ddsi_type_lookup_locked (gv, type_id);
    ddsi_type_register_with_proxy_endpoints_locked (gv, type);
    ddsrt_mutex_unlock (&gv->typelib_lock);
    ddsi_typeid_fini (type_id);
    ddsrt_free (type_id);
  }
}

uint32_t ddsi_type_get_gpe_matches (struct ddsi_domaingv *gv, const struct ddsi_type *type, struct generic_proxy_endpoint ***gpe_match_upd, uint32_t *n_match_upd)
{
  if (!ddsi_type_proxy_guid_list_count (&type->proxy_guids))
    return 0;

  uint32_t n = 0;
  *gpe_match_upd = ddsrt_realloc (*gpe_match_upd, (*n_match_upd + ddsi_type_proxy_guid_list_count (&type->proxy_guids)) * sizeof (**gpe_match_upd));
  struct ddsi_type_proxy_guid_list_iter it;
  for (ddsi_guid_t guid = ddsi_type_proxy_guid_list_iter_first (&type->proxy_guids, &it); !is_null_guid (&guid); guid = ddsi_type_proxy_guid_list_iter_next (&it))
  {
    if (!is_topic_entityid (guid.entityid))
    {
      struct entity_common *ec = entidx_lookup_guid_untyped (gv->entity_index, &guid);
      if (ec != NULL)
      {
        assert (ec->kind == EK_PROXY_READER || ec->kind == EK_PROXY_WRITER);
        (*gpe_match_upd)[*n_match_upd + n++] = (struct generic_proxy_endpoint *) ec;
      }
    }
  }
  *n_match_upd += n;
  ddsi_type_register_with_proxy_endpoints_locked (gv, type);
  return n;
}

#endif /* DDS_HAS_TYPE_DISCOVERY */
