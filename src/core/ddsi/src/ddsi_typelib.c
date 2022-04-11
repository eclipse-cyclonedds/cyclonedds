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
#include "dds/ddsi/ddsi_xt_impl.h"
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
  return ddsi_type_id_with_deps_equal (&a->x.minimal, &b->x.minimal) && ddsi_type_id_with_deps_equal (&a->x.complete, &b->x.complete);
}

ddsi_typeinfo_t * ddsi_typeinfo_dup (const ddsi_typeinfo_t *src)
{
  ddsi_typeinfo_t *dst = ddsrt_calloc (1, sizeof (*dst));
  ddsi_typeid_copy_impl (&dst->x.minimal.typeid_with_size.type_id, &src->x.minimal.typeid_with_size.type_id);
  dst->x.minimal.dependent_typeid_count = src->x.minimal.dependent_typeid_count;
  dst->x.minimal.dependent_typeids._length = dst->x.minimal.dependent_typeids._maximum = src->x.minimal.dependent_typeids._length;
  if (dst->x.minimal.dependent_typeids._length > 0)
  {
    dst->x.minimal.dependent_typeids._release = true;
    dst->x.minimal.dependent_typeids._buffer = ddsrt_calloc (dst->x.minimal.dependent_typeids._length, sizeof (*dst->x.minimal.dependent_typeids._buffer));
    for (uint32_t n = 0; n < dst->x.minimal.dependent_typeids._length; n++)
      ddsi_typeid_copy_impl (&dst->x.minimal.dependent_typeids._buffer[n].type_id, &src->x.minimal.dependent_typeids._buffer[n].type_id);
  }

  ddsi_typeid_copy_impl (&dst->x.complete.typeid_with_size.type_id, &src->x.complete.typeid_with_size.type_id);
  dst->x.complete.dependent_typeid_count = src->x.complete.dependent_typeid_count;
  dst->x.complete.dependent_typeids._length = dst->x.complete.dependent_typeids._maximum = src->x.complete.dependent_typeids._length;
  if (dst->x.complete.dependent_typeids._length > 0)
  {
    dst->x.complete.dependent_typeids._release = true;
    dst->x.complete.dependent_typeids._buffer = ddsrt_calloc (dst->x.complete.dependent_typeids._length, sizeof (*dst->x.complete.dependent_typeids._buffer));
    for (uint32_t n = 0; n < dst->x.complete.dependent_typeids._length; n++)
      ddsi_typeid_copy_impl (&dst->x.complete.dependent_typeids._buffer[n].type_id, &src->x.complete.dependent_typeids._buffer[n].type_id);
  }

  return dst;
}

ddsi_typeinfo_t *ddsi_typeinfo_deser (const struct ddsi_sertype_cdr_data *ser)
{
  unsigned char *data;
  uint32_t srcoff = 0;

  if (ser->sz == 0 || ser->data == NULL)
    return false;

  /* Type objects are stored as a LE serialized CDR blob in the topic descriptor */
  DDSRT_WARNING_MSVC_OFF(6326)
  bool bswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  DDSRT_WARNING_MSVC_ON(6326)
  if (bswap)
    data = ddsrt_memdup (ser->data, ser->sz);
  else
    data = ser->data;
  if (!dds_stream_normalize_data ((char *) data, &srcoff, ser->sz, bswap, CDR_ENC_VERSION_2, DDS_XTypes_TypeInformation_desc.m_ops))
  {
    if (bswap)
      ddsrt_free (data);
    return NULL;
  }

  dds_istream_t is = { .m_buffer = data, .m_index = 0, .m_size = ser->sz, .m_xcdr_version = CDR_ENC_VERSION_2 };
  ddsi_typeinfo_t *typeinfo = ddsrt_calloc (1, sizeof (*typeinfo));
  dds_stream_read (&is, (void *) typeinfo, DDS_XTypes_TypeInformation_desc.m_ops);
  if (bswap)
    ddsrt_free (data);
  return typeinfo;
}

ddsi_typeid_t *ddsi_typeinfo_typeid (ddsi_typeinfo_t *type_info, ddsi_typeid_kind_t kind)
{
  ddsi_typeid_t *type_id = NULL;
  if (kind == DDSI_TYPEID_KIND_MINIMAL && !ddsi_typeid_is_none (ddsi_typeinfo_minimal_typeid (type_info)))
    type_id = ddsi_typeid_dup (ddsi_typeinfo_minimal_typeid (type_info));
  else if (!ddsi_typeid_is_none (ddsi_typeinfo_complete_typeid (type_info)))
    type_id = ddsi_typeid_dup (ddsi_typeinfo_complete_typeid (type_info));
  return type_id;
}

void ddsi_typeinfo_fini (ddsi_typeinfo_t *typeinfo)
{
  dds_stream_free_sample (typeinfo, DDS_XTypes_TypeInformation_desc.m_ops);
}

const ddsi_typeid_t *ddsi_typeinfo_minimal_typeid (const ddsi_typeinfo_t *typeinfo)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_typeid, x) == 0);
  if (typeinfo == NULL)
    return NULL;
  return (const ddsi_typeid_t *) &typeinfo->x.minimal.typeid_with_size.type_id;
}

const ddsi_typeid_t *ddsi_typeinfo_complete_typeid (const ddsi_typeinfo_t *typeinfo)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_typeid, x) == 0);
  if (typeinfo == NULL)
    return NULL;
  return (const ddsi_typeid_t *) &typeinfo->x.complete.typeid_with_size.type_id;
}

static bool typeinfo_dependent_typeids_valid (const struct DDS_XTypes_TypeIdentifierWithDependencies *t, ddsi_typeid_kind_t kind)
{
  if (t->dependent_typeid_count == -1)
  {
    if (t->dependent_typeids._length > 0)
      return false;
  }
  else
  {
    if (t->dependent_typeids._length > INT32_MAX ||
        t->dependent_typeid_count < (int32_t) t->dependent_typeids._length ||
        (t->dependent_typeids._length > 0 && t->dependent_typeids._buffer == NULL))
      return false;
    for (uint32_t n = 0; n < t->dependent_typeids._length; n++)
    {
      if ((kind == DDSI_TYPEID_KIND_MINIMAL && !ddsi_typeid_is_minimal_impl (&t->dependent_typeids._buffer[n].type_id))
          || (kind == DDSI_TYPEID_KIND_COMPLETE && !ddsi_typeid_is_complete_impl (&t->dependent_typeids._buffer[n].type_id))
          || t->dependent_typeids._buffer[n].typeobject_serialized_size == 0)
        return false;
    }
  }
  return true;
}

bool ddsi_typeinfo_valid (const ddsi_typeinfo_t *typeinfo)
{
  const ddsi_typeid_t *tid_min = ddsi_typeinfo_minimal_typeid (typeinfo);
  const ddsi_typeid_t *tid_compl = ddsi_typeinfo_complete_typeid (typeinfo);
  if (ddsi_typeid_is_none (tid_min) || !ddsi_typeid_is_hash (tid_min) ||
      ddsi_typeid_is_none (tid_compl) || !ddsi_typeid_is_hash (tid_compl))
    return false;
  if (!typeinfo_dependent_typeids_valid (&typeinfo->x.minimal, DDSI_TYPEID_KIND_MINIMAL) ||
      !typeinfo_dependent_typeids_valid (&typeinfo->x.complete, DDSI_TYPEID_KIND_COMPLETE))
    return false;
  return true;
}

static const struct DDS_XTypes_TypeObject * ddsi_typemap_typeobj (const ddsi_typemap_t *tmap, const struct DDS_XTypes_TypeIdentifier *type_id)
{
  assert (type_id);
  assert (tmap);
  if (!ddsi_typeid_is_hash_impl (type_id))
    return NULL;
  const dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *list = ddsi_typeid_is_minimal_impl (type_id) ?
    &tmap->x.identifier_object_pair_minimal : &tmap->x.identifier_object_pair_complete;
  for (uint32_t i = 0; i < list->_length; i++)
  {
    DDS_XTypes_TypeIdentifierTypeObjectPair *pair = &list->_buffer[i];
    if (!ddsi_typeid_compare_impl (type_id, &pair->type_identifier))
      return &pair->type_object;
  }
  return NULL;
}

uint32_t ddsi_typeinfo_get_dependent_typeids (const ddsi_typeinfo_t *type_info, const ddsi_typeid_t *** dep_ids, ddsi_typeid_kind_t kind)
{
  assert (dep_ids);
  assert (kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_COMPLETE);
  int32_t cnt = (kind == DDSI_TYPEID_KIND_MINIMAL) ? type_info->x.minimal.dependent_typeid_count : type_info->x.complete.dependent_typeid_count;
  if (cnt <= 0)
  {
    *dep_ids = NULL;
    return 0;
  }

  *dep_ids = ddsrt_malloc ((uint32_t) cnt * sizeof (**dep_ids));
  if (kind == DDSI_TYPEID_KIND_MINIMAL)
  {
    // FIXME: remove cast
    for (int32_t n = 0; n < type_info->x.minimal.dependent_typeid_count; n++)
      (*dep_ids)[n] = (ddsi_typeid_t *) &type_info->x.minimal.dependent_typeids._buffer[n].type_id;
  }
  else
  {
    // FIXME: remove cast
    for (int32_t n = 0; n < type_info->x.complete.dependent_typeid_count; n++)
      (*dep_ids)[n] = (ddsi_typeid_t *) &type_info->x.complete.dependent_typeids._buffer[n].type_id;
  }
  return (uint32_t) cnt;
}


ddsi_typemap_t *ddsi_typemap_deser (const struct ddsi_sertype_cdr_data *ser)
{
  unsigned char *data;
  uint32_t srcoff = 0;

  if (ser->sz == 0 || ser->data == NULL)
    return NULL;

  DDSRT_WARNING_MSVC_OFF(6326)
  bool bswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  DDSRT_WARNING_MSVC_ON(6326)
  if (bswap)
    data = ddsrt_memdup (ser->data, ser->sz);
  else
    data = ser->data;
  if (!dds_stream_normalize_data ((char *) data, &srcoff, ser->sz, bswap, CDR_ENC_VERSION_2, DDS_XTypes_TypeMapping_desc.m_ops))
  {
    if (bswap)
      ddsrt_free (data);
    return NULL;
  }

  dds_istream_t is = { .m_buffer = data, .m_index = 0, .m_size = ser->sz, .m_xcdr_version = CDR_ENC_VERSION_2 };
  ddsi_typemap_t *typemap = ddsrt_calloc (1, sizeof (*typemap));
  dds_stream_read (&is, (void *) typemap, DDS_XTypes_TypeMapping_desc.m_ops);
  if (bswap)
    ddsrt_free (data);
  return typemap;
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

struct ddsi_type * ddsi_type_lookup_locked_impl (struct ddsi_domaingv *gv, const struct DDS_XTypes_TypeIdentifier *type_id)
{
  assert (type_id);
  /* The type identifier field is at offset 0 in struct ddsi_type and the compare
     function only uses this field, so we can safely cast to a ddsi_type here. */
  return ddsrt_avl_lookup (&ddsi_typelib_treedef, &gv->typelib, type_id);
}

struct ddsi_type * ddsi_type_lookup_locked (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id)
{
  return ddsi_type_lookup_locked_impl (gv, &type_id->x);
}

static struct ddsi_type * ddsi_type_new (struct ddsi_domaingv *gv, const struct DDS_XTypes_TypeIdentifier *type_id, const struct DDS_XTypes_TypeObject *type_obj)
{
  assert (!ddsi_typeid_is_none_impl (type_id));
  assert (!ddsi_type_lookup_locked_impl (gv, type_id));
  struct ddsi_type *type = ddsrt_calloc (1, sizeof (*type));
  GVTRACE (" new %p", type);
  if (ddsi_xt_type_init_impl (gv, &type->xt, type_id, type_obj) < 0)
  {
    ddsrt_free (type);
    return NULL;
  }
  ddsrt_avl_insert (&ddsi_typelib_treedef, &gv->typelib, type);
  return type;
}

static void type_add_dep (struct ddsi_domaingv *gv, struct ddsi_type *type, const struct DDS_XTypes_TypeIdentifier *dep_type_id, const struct DDS_XTypes_TypeObject *dep_type_obj, uint32_t *n_match_upd, struct generic_proxy_endpoint ***gpe_match_upd)
{
  struct ddsi_typeid_str tistr, tistrdep;
  GVTRACE ("type %s add dep %s\n", ddsi_make_typeid_str (&tistr, &type->xt.id), ddsi_make_typeid_str_impl (&tistrdep, dep_type_id));
  struct ddsi_type *dep_type = ddsi_type_ref_id_locked_impl (gv, dep_type_id);
  assert (dep_type);
  if (dep_type_obj)
  {
    assert (n_match_upd);
    assert (gpe_match_upd);
    if (ddsi_xt_type_add_typeobj (gv, &dep_type->xt, dep_type_obj) == 0)
    {
      dep_type->state = DDSI_TYPE_RESOLVED;
      (void) ddsi_type_get_gpe_matches (gv, type, gpe_match_upd, n_match_upd);
    }
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
  if ((kind == DDSI_TYPEID_KIND_MINIMAL && type_info->x.minimal.dependent_typeid_count > 0)
    || (kind == DDSI_TYPEID_KIND_COMPLETE && type_info->x.complete.dependent_typeid_count > 0))
  {
    const dds_sequence_DDS_XTypes_TypeIdentifierWithSize *dep_ids;
    if (kind == DDSI_TYPEID_KIND_COMPLETE)
      dep_ids = &type_info->x.complete.dependent_typeids;
    else
      dep_ids = &type_info->x.minimal.dependent_typeids;

    // FIXME: find a better approach to keep track of type dependencies
    for (uint32_t n = 0; dep_ids && n < dep_ids->_length; n++)
    {
      const struct DDS_XTypes_TypeIdentifier *dep_type_id = &dep_ids->_buffer[n].type_id;
      if (!ddsi_typeid_compare_impl (&type->xt.id.x, dep_type_id))
        continue;
      const struct DDS_XTypes_TypeObject *dep_type_obj = type_map ? ddsi_typemap_typeobj (type_map, dep_type_id) : NULL;
      struct ddsi_type_dep *dep = type->deps;
      while (dep)
      {
        if (!ddsi_typeid_compare_impl (&dep->type->xt.id.x, dep_type_id) && dep_type_obj != NULL && ddsi_xt_type_add_typeobj (gv, &dep->type->xt, dep_type_obj) == DDS_RETCODE_OK)
        {
          dep->type->state = DDSI_TYPE_RESOLVED;
          (void) ddsi_type_get_gpe_matches (gv, dep->type, gpe_match_upd, n_match_upd);
          break;
        }
        dep = dep->prev;
      }
      if (!dep)
        type_add_dep (gv, type, dep_type_id, dep_type_obj, n_match_upd, gpe_match_upd);
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

struct ddsi_type * ddsi_type_ref_id_locked_impl (struct ddsi_domaingv *gv, const struct DDS_XTypes_TypeIdentifier *type_id)
{
  struct ddsi_typeid_str tistr;
  assert (!ddsi_typeid_is_none_impl (type_id));
  GVTRACE ("ref ddsi_type type-id %s", ddsi_make_typeid_str_impl (&tistr, type_id));
  struct ddsi_type *type = ddsi_type_lookup_locked_impl (gv, type_id);
  if (!type)
    type = ddsi_type_new (gv, type_id, NULL);
  if (!type)
    return NULL;
  type->refc++;
  GVTRACE (" refc %"PRIu32"\n", type->refc);
  return type;
}

struct ddsi_type * ddsi_type_ref_id_locked (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id)
{
  return ddsi_type_ref_id_locked_impl (gv, &type_id->x);
}

struct ddsi_type * ddsi_type_ref_local (struct ddsi_domaingv *gv, const struct ddsi_sertype *sertype, ddsi_typeid_kind_t kind)
{
  struct generic_proxy_endpoint **gpe_match_upd = NULL;
  uint32_t n_match_upd = 0;
  struct ddsi_typeid_str tistr;

  assert (sertype != NULL);
  assert (kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_COMPLETE);
  ddsi_typeinfo_t *type_info = ddsi_sertype_typeinfo (sertype);
  if (!type_info)
    return NULL;
  ddsi_typemap_t *type_map = ddsi_sertype_typemap (sertype);
  const struct DDS_XTypes_TypeIdentifier *type_id = (kind == DDSI_TYPEID_KIND_MINIMAL) ? &type_info->x.minimal.typeid_with_size.type_id : &type_info->x.complete.typeid_with_size.type_id;
  const struct DDS_XTypes_TypeObject *type_obj = ddsi_typemap_typeobj (type_map, type_id);
  bool resolved = false;

  GVTRACE ("ref ddsi_type local sertype %p id %s", sertype, ddsi_make_typeid_str_impl (&tistr, type_id));

  ddsrt_mutex_lock (&gv->typelib_lock);

  struct ddsi_type *type = ddsi_type_lookup_locked_impl (gv, type_id);
  if (!type)
    type = ddsi_type_new (gv, type_id, type_obj);
  else if (type_obj)
    (void) ddsi_xt_type_add_typeobj (gv, &type->xt, type_obj);
  type->refc++;
  GVTRACE (" refc %"PRIu32"\n", type->refc);

  type_add_deps (gv, type, type_info, type_map, kind, &n_match_upd, &gpe_match_upd);

  if (type->sertype == NULL)
  {
    type->sertype = ddsi_sertype_ref (sertype);
    GVTRACE ("type %s resolved\n", ddsi_make_typeid_str_impl (&tistr, type_id));
    resolved = true;
  }
  if (resolved)
    ddsrt_cond_broadcast (&gv->typelib_resolved_cond);
  ddsrt_mutex_unlock (&gv->typelib_lock);

  if (gpe_match_upd != NULL)
  {
    for (uint32_t e = 0; e < n_match_upd; e++)
    {
      GVTRACE ("type %s trigger matching "PGUIDFMT"\n", ddsi_make_typeid_str_impl (&tistr, type_id), PGUID(gpe_match_upd[e]->e.guid));
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
  struct ddsi_typeid_str tistr;
  assert (type_info);
  assert (kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_COMPLETE);
  const struct DDS_XTypes_TypeIdentifier *type_id = (kind == DDSI_TYPEID_KIND_MINIMAL) ? &type_info->x.minimal.typeid_with_size.type_id : &type_info->x.complete.typeid_with_size.type_id;

  ddsrt_mutex_lock (&gv->typelib_lock);

  GVTRACE ("ref ddsi_type proxy id %s", ddsi_make_typeid_str_impl (&tistr, type_id));
  struct ddsi_type *type = ddsi_type_lookup_locked_impl (gv, type_id);
  if (!type)
    type = ddsi_type_new (gv, type_id, NULL);
  type->refc++;
  GVTRACE(" refc %"PRIu32"\n", type->refc);

  type_add_deps (gv, type, type_info, NULL, kind, NULL, NULL);

  if (proxy_guid != NULL && !ddsi_type_proxy_guid_exists (type, proxy_guid))
  {
    ddsi_type_proxy_guid_list_insert (&type->proxy_guids, *proxy_guid);
    GVTRACE ("type %s add ep "PGUIDFMT"\n", ddsi_make_typeid_str_impl (&tistr, type_id), PGUID (*proxy_guid));
  }

  ddsrt_mutex_unlock (&gv->typelib_lock);
  return type;
}

const struct ddsi_sertype *ddsi_type_sertype (const struct ddsi_type *type)
{
  if (type == NULL)
    return NULL;
  return type->sertype;
}

bool ddsi_type_has_typeobj (const struct ddsi_type *type)
{
  if (type == NULL)
    return false;
  return type->xt.has_obj;
}

struct ddsi_typeobj *ddsi_type_get_typeobj (const struct ddsi_type *type)
{
  if (!ddsi_type_has_typeobj (type))
    return NULL;

  ddsi_typeobj_t *to = ddsrt_malloc (sizeof (*to));
  ddsi_xt_get_typeobject (&type->xt, to);
  return to;
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
  struct ddsi_typeid_str tistr;
  assert (proxy_guid);
  if (!type)
    return;
  ddsrt_mutex_lock (&gv->typelib_lock);
  GVTRACE ("unreg proxy guid " PGUIDFMT " ddsi_type id %s\n", PGUID (*proxy_guid), ddsi_make_typeid_str (&tistr, &type->xt.id));
  ddsi_type_proxy_guid_list_remove (&type->proxy_guids, *proxy_guid, ddsi_type_proxy_guids_eq);
  ddsrt_mutex_unlock (&gv->typelib_lock);
}

void ddsi_type_unref (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  struct ddsi_typeid_str tistr;
  if (!type)
    return;
  ddsrt_mutex_lock (&gv->typelib_lock);
  GVTRACE ("unref ddsi_type id %s", ddsi_make_typeid_str (&tistr, &type->xt.id));
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
      struct ddsi_typeid_str tistr;
      GVTRACE ("unref ddsi_type id %s", ddsi_make_typeid_str (&tistr, &type->xt.id));
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
    struct ddsi_typeid_str tistr;
    GVTRACE ("unref ddsi_type id %s", ddsi_make_typeid_str (&tistr, &type->xt.id));
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
  thread_state_awake (lookup_thread_state (), gv);
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
  thread_state_asleep (lookup_thread_state ());
  return n;
}

bool ddsi_is_assignable_from (struct ddsi_domaingv *gv, const struct ddsi_type_pair *rd_type_pair, const struct ddsi_type_pair *wr_type_pair, const dds_type_consistency_enforcement_qospolicy_t *tce)
{
  if (!rd_type_pair || !wr_type_pair)
    return false;
  ddsrt_mutex_lock (&gv->typelib_lock);
  const struct xt_type
    *rd_xt = rd_type_pair->minimal ? &rd_type_pair->minimal->xt : &rd_type_pair->complete->xt,
    *wr_xt = wr_type_pair->minimal ? &wr_type_pair->minimal->xt : &wr_type_pair->complete->xt;
  bool assignable = ddsi_xt_is_assignable_from (gv, rd_xt, wr_xt, tce);
  ddsrt_mutex_unlock (&gv->typelib_lock);
  return assignable;
}

char *ddsi_make_typeid_str_impl (struct ddsi_typeid_str *buf, const DDS_XTypes_TypeIdentifier *type_id)
{
  snprintf (buf->str, sizeof (buf->str), PTYPEIDFMT, PTYPEID (*type_id));
  return buf->str;
}

char *ddsi_make_typeid_str (struct ddsi_typeid_str *buf, const ddsi_typeid_t *type_id)
{
  return ddsi_make_typeid_str_impl (buf, &type_id->x);
}

const ddsi_typeid_t *ddsi_type_pair_minimal_id (const struct ddsi_type_pair *type_pair)
{
  if (type_pair == NULL || type_pair->minimal == NULL)
    return NULL;
  return &type_pair->minimal->xt.id;
}

const ddsi_typeid_t *ddsi_type_pair_complete_id (const struct ddsi_type_pair *type_pair)
{
  if (type_pair == NULL || type_pair->complete == NULL)
    return NULL;
  return &type_pair->complete->xt.id;
}

const struct ddsi_sertype *ddsi_type_pair_complete_sertype (const struct ddsi_type_pair *type_pair)
{
  if (type_pair == NULL || type_pair->complete == NULL)
    return NULL;
  return type_pair->complete->sertype;
}

struct ddsi_type_pair *ddsi_type_pair_init (const ddsi_typeid_t *type_id_minimal, const ddsi_typeid_t *type_id_complete)
{
  struct ddsi_type_pair *type_pair = ddsrt_calloc (1, sizeof (*type_pair));
  if (type_id_minimal != NULL)
  {
    type_pair->minimal = ddsrt_malloc (sizeof (*type_pair->minimal));
    ddsi_typeid_copy (&type_pair->minimal->xt.id, type_id_minimal);
  }
  if (type_id_complete != NULL)
  {
    type_pair->complete = ddsrt_malloc (sizeof (*type_pair->complete));
    ddsi_typeid_copy (&type_pair->complete->xt.id, type_id_complete);
  }
  return type_pair;
}

void ddsi_type_pair_free (struct ddsi_type_pair *type_pair)
{
  if (type_pair == NULL)
    return;
  if (type_pair->minimal != NULL)
  {
    ddsi_typeid_fini (&type_pair->minimal->xt.id);
    ddsrt_free (type_pair->minimal);
  }
  if (type_pair->complete != NULL)
  {
    ddsi_typeid_fini (&type_pair->complete->xt.id);
    ddsrt_free (type_pair->complete);
  }
  ddsrt_free (type_pair);
}

bool ddsi_type_pair_has_minimal_obj (const struct ddsi_type_pair *type_pair)
{
  if (type_pair == NULL || type_pair->minimal == NULL)
    return false;
  return type_pair->minimal->xt.has_obj;
}

bool ddsi_type_pair_has_complete_obj (const struct ddsi_type_pair *type_pair)
{
  if (type_pair == NULL || type_pair->complete == NULL)
    return false;
  return type_pair->complete->xt.has_obj;
}

#endif /* DDS_HAS_TYPE_DISCOVERY */
