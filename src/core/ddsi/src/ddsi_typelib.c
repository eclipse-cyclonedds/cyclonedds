// Copyright(c) 2006 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/features.h"

#ifdef DDS_HAS_TYPE_DISCOVERY

#include <string.h>
#include <stdlib.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_xt_typemap.h"
#include "ddsi__entity.h"
#include "ddsi__endpoint_match.h"
#include "ddsi__misc.h"
#include "ddsi__thread.h"
#include "ddsi__entity_index.h"
#include "ddsi__xt_impl.h"
#include "ddsi__typelookup.h"
#include "ddsi__serdata_cdr.h"
#include "ddsi__list_tmpl.h"
#include "ddsi__topic.h"
#include "ddsi__typelib.h"
#include "ddsi__typewrap.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds/ddsc/dds_public_impl.h"

DDSI_LIST_DECLS_TMPL(static, ddsi_type_proxy_guid_list, ddsi_guid_t, ddsrt_attribute_unused)
DDSI_LIST_CODE_TMPL(static, ddsi_type_proxy_guid_list, ddsi_guid_t, ddsi_nullguid, ddsrt_malloc, ddsrt_free)

static int ddsi_type_compare_wrap (const void *type_a, const void *type_b);
const ddsrt_avl_treedef_t ddsi_typelib_treedef = DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_type, avl_node), 0, ddsi_type_compare_wrap, 0);

static int ddsi_typeid_compare_src_dep (const void *typedep_a, const void *typedep_b);
static int ddsi_typeid_compare_dep_src (const void *typedep_a, const void *typedep_b);
const ddsrt_avl_treedef_t ddsi_typedeps_treedef = DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_type_dep, src_avl_node), 0, ddsi_typeid_compare_src_dep, 0);
const ddsrt_avl_treedef_t ddsi_typedeps_reverse_treedef = DDSRT_AVL_TREEDEF_INITIALIZER (offsetof (struct ddsi_type_dep, dep_avl_node), 0, ddsi_typeid_compare_dep_src, 0);

bool ddsi_typeinfo_equal (const ddsi_typeinfo_t *a, const ddsi_typeinfo_t *b, ddsi_type_include_deps_t deps)
{
  if (a == NULL || b == NULL)
    return a == b;
  return ddsi_type_id_with_deps_equal (&a->x.minimal, &b->x.minimal, deps) && ddsi_type_id_with_deps_equal (&a->x.complete, &b->x.complete, deps);
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
    {
      ddsi_typeid_copy_impl (&dst->x.minimal.dependent_typeids._buffer[n].type_id, &src->x.minimal.dependent_typeids._buffer[n].type_id);
      dst->x.minimal.dependent_typeids._buffer[n].typeobject_serialized_size = src->x.minimal.dependent_typeids._buffer[n].typeobject_serialized_size;
    }
  }

  ddsi_typeid_copy_impl (&dst->x.complete.typeid_with_size.type_id, &src->x.complete.typeid_with_size.type_id);
  dst->x.complete.dependent_typeid_count = src->x.complete.dependent_typeid_count;
  dst->x.complete.dependent_typeids._length = dst->x.complete.dependent_typeids._maximum = src->x.complete.dependent_typeids._length;
  if (dst->x.complete.dependent_typeids._length > 0)
  {
    dst->x.complete.dependent_typeids._release = true;
    dst->x.complete.dependent_typeids._buffer = ddsrt_calloc (dst->x.complete.dependent_typeids._length, sizeof (*dst->x.complete.dependent_typeids._buffer));
    for (uint32_t n = 0; n < dst->x.complete.dependent_typeids._length; n++)
    {
      ddsi_typeid_copy_impl (&dst->x.complete.dependent_typeids._buffer[n].type_id, &src->x.complete.dependent_typeids._buffer[n].type_id);
      dst->x.complete.dependent_typeids._buffer[n].typeobject_serialized_size = src->x.complete.dependent_typeids._buffer[n].typeobject_serialized_size;
    }
  }

  return dst;
}

ddsi_typeinfo_t *ddsi_typeinfo_deser (const unsigned char *data, uint32_t sz)
{
  unsigned char *data_ne;
  uint32_t srcoff = 0;

  if (sz == 0 || data == NULL)
    return false;

  /* Type objects are stored as a LE serialized CDR blob in the topic descriptor */
  DDSRT_WARNING_MSVC_OFF(6326)
  bool bswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  DDSRT_WARNING_MSVC_ON(6326)
  if (bswap)
    data_ne = ddsrt_memdup (data, sz);
  else
    data_ne = (unsigned char *) data;
  if (!dds_stream_normalize_data ((char *) data_ne, &srcoff, sz, bswap, DDSI_RTPS_CDR_ENC_VERSION_2, DDS_XTypes_TypeInformation_desc.m_ops))
  {
    if (bswap)
      ddsrt_free (data_ne);
    return NULL;
  }

  dds_istream_t is = { .m_buffer = data_ne, .m_index = 0, .m_size = sz, .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 };
  ddsi_typeinfo_t *typeinfo = ddsrt_calloc (1, sizeof (*typeinfo));
  dds_stream_read (&is, (void *) typeinfo, &dds_cdrstream_default_allocator, DDS_XTypes_TypeInformation_desc.m_ops);
  if (bswap)
    ddsrt_free (data_ne);
  return typeinfo;
}

ddsi_typeid_t *ddsi_typeinfo_typeid (const ddsi_typeinfo_t *type_info, ddsi_typeid_kind_t kind)
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
  dds_stream_free_sample (typeinfo, &dds_cdrstream_default_allocator, DDS_XTypes_TypeInformation_desc.m_ops);
}

void ddsi_typeinfo_free (ddsi_typeinfo_t *typeinfo)
{
  ddsi_typeinfo_fini (typeinfo);
  ddsrt_free (typeinfo);
}

const ddsi_typeid_t *ddsi_typeinfo_minimal_typeid (const ddsi_typeinfo_t *typeinfo)
{
  if (typeinfo == NULL)
    return NULL;
  return (const ddsi_typeid_t *) &typeinfo->x.minimal.typeid_with_size.type_id;
}

const ddsi_typeid_t *ddsi_typeinfo_complete_typeid (const ddsi_typeinfo_t *typeinfo)
{
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

bool ddsi_typeinfo_present (const ddsi_typeinfo_t *typeinfo)
{
  const ddsi_typeid_t *tid_min = ddsi_typeinfo_minimal_typeid (typeinfo), *tid_compl = ddsi_typeinfo_complete_typeid (typeinfo);
  return !ddsi_typeid_is_none (tid_min) || !ddsi_typeid_is_none (tid_compl);
}

bool ddsi_typeinfo_valid (const ddsi_typeinfo_t *typeinfo)
{
  const ddsi_typeid_t *tid_min = ddsi_typeinfo_minimal_typeid (typeinfo), *tid_compl = ddsi_typeinfo_complete_typeid (typeinfo);
  return !ddsi_typeid_is_none (tid_min) && !ddsi_typeid_is_none (tid_compl) &&
      !ddsi_typeid_is_fully_descriptive (tid_min) && !ddsi_typeid_is_fully_descriptive (tid_compl) &&
      typeinfo_dependent_typeids_valid (&typeinfo->x.minimal, DDSI_TYPEID_KIND_MINIMAL) &&
      typeinfo_dependent_typeids_valid (&typeinfo->x.complete, DDSI_TYPEID_KIND_COMPLETE);
}

const struct DDS_XTypes_TypeObject * ddsi_typemap_typeobj (const ddsi_typemap_t *tmap, const struct DDS_XTypes_TypeIdentifier *type_id)
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

ddsi_typemap_t *ddsi_typemap_deser (const unsigned char *data, uint32_t sz)
{
  unsigned char *data_ne;
  uint32_t srcoff = 0;

  if (sz == 0 || data == NULL)
    return NULL;

  DDSRT_WARNING_MSVC_OFF(6326)
  bool bswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  DDSRT_WARNING_MSVC_ON(6326)
  if (bswap)
    data_ne = ddsrt_memdup (data, sz);
  else
    data_ne = (unsigned char *) data;
  if (!dds_stream_normalize_data ((char *) data_ne, &srcoff, sz, bswap, DDSI_RTPS_CDR_ENC_VERSION_2, DDS_XTypes_TypeMapping_desc.m_ops))
  {
    if (bswap)
      ddsrt_free (data_ne);
    return NULL;
  }

  dds_istream_t is = { .m_buffer = data_ne, .m_index = 0, .m_size = sz, .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 };
  ddsi_typemap_t *typemap = ddsrt_calloc (1, sizeof (*typemap));
  dds_stream_read (&is, (void *) typemap, &dds_cdrstream_default_allocator, DDS_XTypes_TypeMapping_desc.m_ops);
  if (bswap)
    ddsrt_free (data_ne);
  return typemap;
}

void ddsi_typemap_fini (ddsi_typemap_t *typemap)
{
  dds_stream_free_sample (typemap, &dds_cdrstream_default_allocator, DDS_XTypes_TypeMapping_desc.m_ops);
}

static bool ti_to_pairs_equal (const dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *a, const dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *b)
{
  if (a->_length != b->_length)
    return false;

  bool equal = true;
  for (uint32_t n = 0; equal && n < a->_length; n++)
  {
    struct DDS_XTypes_TypeObject *to_b = NULL;
    for (uint32_t m = 0; !to_b && m < b->_length; m++)
    {
      if (!ddsi_typeid_compare_impl (&a->_buffer[n].type_identifier, &b->_buffer[m].type_identifier))
        to_b = &b->_buffer[m].type_object;
    }
    if (to_b == NULL)
      equal = false;
    else
    {
      dds_ostream_t to_a_ser = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 };
      dds_ostream_t to_b_ser = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 };
      dds_stream_write_sample (&to_a_ser, &dds_cdrstream_default_allocator, &a->_buffer[n].type_object, &DDS_XTypes_TypeObject_cdrstream_desc);
      dds_stream_write_sample (&to_b_ser, &dds_cdrstream_default_allocator, to_b, &DDS_XTypes_TypeObject_cdrstream_desc);
      equal = (to_a_ser.m_index == to_b_ser.m_index) && memcmp (to_a_ser.m_buffer, to_b_ser.m_buffer, to_a_ser.m_index) == 0;
      dds_ostream_fini (&to_a_ser, &dds_cdrstream_default_allocator);
      dds_ostream_fini (&to_b_ser, &dds_cdrstream_default_allocator);
    }
  }
  return equal;
}

static bool ti_pairs_equal (const dds_sequence_DDS_XTypes_TypeIdentifierPair *a, const dds_sequence_DDS_XTypes_TypeIdentifierPair *b)
{
  if (a->_length != b->_length)
    return false;
  for (uint32_t n = 0; n < a->_length; n++)
  {
    bool found = false;
    for (uint32_t m = 0; !found && m < b->_length; m++)
    {
      if (!ddsi_typeid_compare_impl (&a->_buffer[n].type_identifier1, &b->_buffer[m].type_identifier1))
      {
        if (ddsi_typeid_compare_impl (&a->_buffer[n].type_identifier2, &b->_buffer[m].type_identifier2))
          return false;
        found = true;
      }
    }
    if (!found)
      return false;
  }
  return true;
}

bool ddsi_typemap_equal (const ddsi_typemap_t *a, const ddsi_typemap_t *b)
{
  if (a == NULL || b == NULL)
    return a == b;
  return ti_to_pairs_equal (&a->x.identifier_object_pair_minimal, &b->x.identifier_object_pair_minimal)
      && ti_to_pairs_equal (&a->x.identifier_object_pair_complete, &b->x.identifier_object_pair_complete)
      && ti_pairs_equal (&a->x.identifier_complete_minimal, &b->x.identifier_complete_minimal);
}

static bool ddsi_type_proxy_guid_exists (struct ddsi_type *type, const ddsi_guid_t *proxy_guid)
{
  struct ddsi_type_proxy_guid_list_iter it;
  for (ddsi_guid_t guid = ddsi_type_proxy_guid_list_iter_first (&type->proxy_guids, &it); !ddsi_is_null_guid (&guid); guid = ddsi_type_proxy_guid_list_iter_next (&it))
  {
    if (ddsi_guid_eq (&guid, proxy_guid))
      return true;
  }
  return false;
}

static int ddsi_type_proxy_guids_eq (const struct ddsi_guid a, const struct ddsi_guid b)
{
  return ddsi_guid_eq (&a, &b);
}

int ddsi_type_compare (const struct ddsi_type *a, const struct ddsi_type *b)
{
  return ddsi_typeid_compare (&a->xt.id, &b->xt.id);
}

static int ddsi_type_compare_wrap (const void *type_a, const void *type_b)
{
  return ddsi_type_compare (type_a, type_b);
}

static int ddsi_typeid_compare_src_dep (const void *typedep_a, const void *typedep_b)
{
  const struct ddsi_type_dep *a = (const struct ddsi_type_dep *) typedep_a, *b = (const struct ddsi_type_dep *) typedep_b;
  int cmp;
  if ((cmp = ddsi_typeid_compare (&a->src_type_id, &b->src_type_id)))
    return cmp;
  return ddsi_typeid_compare (&a->dep_type_id, &b->dep_type_id);
}

static int ddsi_typeid_compare_dep_src (const void *typedep_a, const void *typedep_b)
{
  const struct ddsi_type_dep *a = (const struct ddsi_type_dep *) typedep_a, *b = (const struct ddsi_type_dep *) typedep_b;
  int cmp;
  if ((cmp = ddsi_typeid_compare (&a->dep_type_id, &b->dep_type_id)))
    return cmp;
  return ddsi_typeid_compare (&a->src_type_id, &b->src_type_id);
}

static void type_dep_trace (struct ddsi_domaingv *gv, const char *prefix, struct ddsi_type_dep *dep)
{
  struct ddsi_typeid_str tistr, tistrdep;
  GVTRACE ("%sdep <%s, %s>\n", prefix, ddsi_make_typeid_str (&tistr, &dep->src_type_id), ddsi_make_typeid_str (&tistrdep, &dep->dep_type_id));
}

void ddsi_type_free (struct ddsi_type *type)
{
  struct ddsi_domaingv *gv = type->gv;
  struct ddsi_type_dep key;
  memset (&key, 0, sizeof (key));
  ddsi_typeid_copy (&key.src_type_id, &type->xt.id);
  ddsi_xt_type_fini (gv, &type->xt, true);

  struct ddsi_type_dep *dep;
  while ((dep = ddsrt_avl_lookup_succ_eq (&ddsi_typedeps_treedef, &gv->typedeps, &key)) != NULL && !ddsi_typeid_compare (&dep->src_type_id, &key.src_type_id))
  {
    type_dep_trace (gv, "ddsi_type_free ", dep);
    ddsrt_avl_delete (&ddsi_typedeps_treedef, &gv->typedeps, dep);
    ddsrt_avl_delete (&ddsi_typedeps_reverse_treedef, &gv->typedeps_reverse, dep);
    if (dep->from_type_info)
    {
      /* This dependency record was added based on dependencies from a type-info object,
         and the dep-type was ref-counted when creating the dependency. Therefore, an
         unref is required at this point when the from_type_info flag is set. */
      struct ddsi_type *dep_type = ddsi_type_lookup_locked (gv, &dep->dep_type_id);
      ddsi_type_unref_locked (gv, dep_type);
    }
    ddsi_typeid_fini (&dep->src_type_id);
    ddsi_typeid_fini (&dep->dep_type_id);
    ddsrt_free (dep);
  }
#ifndef NDEBUG
  assert (!ddsi_type_proxy_guid_list_count (&type->proxy_guids));
#endif
  ddsi_typeid_fini (&key.src_type_id);
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

struct ddsi_type * ddsi_type_lookup (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id)
{
  ddsrt_mutex_lock (&gv->typelib_lock);
  struct ddsi_type *type = ddsi_type_lookup_locked_impl (gv, &type_id->x);
  ddsrt_mutex_unlock (&gv->typelib_lock);
  return type;
}

static dds_return_t ddsi_type_new (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct DDS_XTypes_TypeIdentifier *type_id, const struct DDS_XTypes_TypeObject *type_obj)
{
  dds_return_t ret;
  struct ddsi_typeid_str tistr;
  assert (type);
  assert (!ddsi_typeid_is_none_impl (type_id));
  assert (!ddsi_type_lookup_locked_impl (gv, type_id));

  ddsi_typeid_t type_obj_id;
  if (type_obj && ((ret = ddsi_typeobj_get_hash_id (type_obj, &type_obj_id))
      || (ret = (ddsi_typeid_compare_impl (&type_obj_id.x, type_id) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK))))
  {
    GVWARNING ("non-matching type identifier (%s) and type object (%s)\n", ddsi_make_typeid_str_impl (&tistr, type_id), ddsi_make_typeid_str (&tistr, &type_obj_id));
    *type = NULL;
    return ret;
  }

  if ((*type = ddsrt_calloc (1, sizeof (**type))) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  (*type)->gv = gv;

  GVTRACE (" new %p", *type);
  if ((ret = ddsi_xt_type_init_impl (gv, &(*type)->xt, type_id, type_obj)) != DDS_RETCODE_OK)
  {
    ddsi_type_free (*type);
    *type = NULL;
    return ret;
  }
  if (!ddsi_typeid_is_hash (&(*type)->xt.id))
    (*type)->state = DDSI_TYPE_RESOLVED;
  /* inserted with refc 0 (set by calloc), refc is increased in
     ddsi_type_ref_* functions */
  ddsrt_avl_insert (&ddsi_typelib_treedef, &gv->typelib, *type);
  return DDS_RETCODE_OK;
}

static void set_type_invalid (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  type->state = DDSI_TYPE_INVALID;

  struct ddsi_type_dep tmpl, *reverse_dep = &tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.dep_type_id, &type->xt.id);
  while ((reverse_dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_reverse_treedef, &gv->typedeps_reverse, reverse_dep)) && !ddsi_typeid_compare (&type->xt.id, &reverse_dep->dep_type_id))
  {
    struct ddsi_type *dep_src_type = ddsi_type_lookup_locked (gv, &reverse_dep->src_type_id);
    set_type_invalid (gv, dep_src_type);
  }
}

dds_return_t ddsi_type_add_typeobj (struct ddsi_domaingv *gv, struct ddsi_type *type, const struct DDS_XTypes_TypeObject *type_obj)
{
  dds_return_t ret = DDS_RETCODE_OK;
  if (type->state != DDSI_TYPE_RESOLVED)
  {
    ddsi_typeid_t type_id;
    int cmp = -1;
    if ((ret = ddsi_typeobj_get_hash_id (type_obj, &type_id))
        || (ret = (cmp = ddsi_typeid_compare (&type->xt.id, &type_id)) ? DDS_RETCODE_BAD_PARAMETER : DDS_RETCODE_OK)
        || (ret = ddsi_xt_type_add_typeobj (gv, &type->xt, type_obj))
    ) {
      if (cmp == 0)
      {
        /* Mark this type and all types that (indirectly) depend on this type
           invalid, because at this point we know that the type object that matches
           the type id for this type is invalid (except in case of a hash collision
           and a different valid type object exists with the same id) */
        set_type_invalid (gv, type);
      }
      else
      {
        /* In case the object does not match the type id, reset the type's state to
           unresolved so that it can de resolved in case the correct type object
           is received */
        type->state = DDSI_TYPE_UNRESOLVED;
      }
    }
    else
      type->state = DDSI_TYPE_RESOLVED;
  }
  return ret;
}

static dds_return_t ddsi_type_register_dep_impl (struct ddsi_domaingv *gv, const ddsi_typeid_t *src_type_id, struct ddsi_type **dst_dep_type, const struct DDS_XTypes_TypeIdentifier *dep_tid, bool from_type_info)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct ddsi_typeid dep_type_id;

  if (ddsi_typeid_is_none_impl (dep_tid))
    return DDS_RETCODE_BAD_PARAMETER;

  dep_type_id.x = *dep_tid;
  struct ddsi_type_dep *dep = ddsrt_calloc (1, sizeof (*dep));
  ddsi_typeid_copy (&dep->src_type_id, src_type_id);
  ddsi_typeid_copy (&dep->dep_type_id, &dep_type_id);
  bool existing = ddsrt_avl_lookup (&ddsi_typedeps_treedef, &gv->typedeps, dep) != NULL;
  type_dep_trace (gv, existing ? "has " : "add ", dep);
  if (!existing)
  {
    dep->from_type_info = from_type_info;
    ddsrt_avl_insert (&ddsi_typedeps_treedef, &gv->typedeps, dep);
    ddsrt_avl_insert (&ddsi_typedeps_reverse_treedef, &gv->typedeps_reverse, dep);
    if ((ret = ddsi_type_ref_id_locked (gv, dst_dep_type, &dep_type_id)) != DDS_RETCODE_OK)
    {
      ddsrt_avl_delete (&ddsi_typedeps_treedef, &gv->typedeps, dep);
      ddsrt_avl_delete (&ddsi_typedeps_reverse_treedef, &gv->typedeps_reverse, dep);
      ddsi_typeid_fini (&dep->src_type_id);
      ddsi_typeid_fini (&dep->dep_type_id);
      ddsrt_free (dep);
    }
  }
  else
  {
    ddsi_typeid_fini (&dep->src_type_id);
    ddsi_typeid_fini (&dep->dep_type_id);
    ddsrt_free (dep);
    if (!from_type_info)
      ret = ddsi_type_ref_id_locked (gv, dst_dep_type, &dep_type_id);
    else
    {
      *dst_dep_type = ddsi_type_lookup_locked (gv, &dep_type_id);
      if (*dst_dep_type == NULL)
        ret = DDS_RETCODE_ERROR;
    }
  }
  return ret;
}

dds_return_t ddsi_type_register_dep (struct ddsi_domaingv *gv, const ddsi_typeid_t *src_type_id, struct ddsi_type **dst_dep_type, const struct DDS_XTypes_TypeIdentifier *dep_tid)
{
  return ddsi_type_register_dep_impl (gv, src_type_id, dst_dep_type, dep_tid, false);
}

static dds_return_t type_add_deps (struct ddsi_domaingv *gv, struct ddsi_type *type, const ddsi_typeinfo_t *type_info, const ddsi_typemap_t *type_map, ddsi_typeid_kind_t kind, uint32_t *n_match_upd, struct ddsi_generic_proxy_endpoint ***gpe_match_upd)
{
  assert (type_info);
  assert (kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_COMPLETE);
  dds_return_t ret = DDS_RETCODE_OK;
  if ((kind == DDSI_TYPEID_KIND_MINIMAL && !type_info->x.minimal.dependent_typeid_count)
    || (kind == DDSI_TYPEID_KIND_COMPLETE && !type_info->x.complete.dependent_typeid_count))
    return ret;

  const dds_sequence_DDS_XTypes_TypeIdentifierWithSize *dep_ids =
    (kind == DDSI_TYPEID_KIND_COMPLETE) ? &type_info->x.complete.dependent_typeids : &type_info->x.minimal.dependent_typeids;

  for (uint32_t n = 0; dep_ids && n < dep_ids->_length && ret == DDS_RETCODE_OK; n++)
  {
    const struct DDS_XTypes_TypeIdentifier *dep_type_id = &dep_ids->_buffer[n].type_id;
    if (!ddsi_typeid_compare_impl (&type->xt.id.x, dep_type_id))
      continue;

    struct ddsi_type *dst_dep_type = NULL;
    if ((ret = ddsi_type_register_dep_impl (gv, &type->xt.id, &dst_dep_type, dep_type_id, true)) != DDS_RETCODE_OK)
    {
      /* If an error occurs, stop registering deps. The type will be finalized later, that
         will clean up the dependencies that are already added at this point */
      type->state = DDSI_TYPE_INVALID;
      return ret;
    }
    assert (dst_dep_type);
    if (!type_map || ddsi_type_resolved_locked (gv, dst_dep_type, DDSI_TYPE_IGNORE_DEPS))
      continue;

    const struct DDS_XTypes_TypeObject *dep_type_obj = ddsi_typemap_typeobj (type_map, dep_type_id);
    if (dep_type_obj)
    {
      assert (n_match_upd && gpe_match_upd);
      if ((ret = ddsi_type_add_typeobj (gv, dst_dep_type, dep_type_obj)) == DDS_RETCODE_OK)
        ddsi_type_get_gpe_matches (gv, type, gpe_match_upd, n_match_upd);
    }
  }
  return ret;
}

void ddsi_type_ref_locked (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct ddsi_type *src)
{
  assert (src);
  struct ddsi_type *t = (struct ddsi_type *) src;
  t->refc++;
  GVTRACE ("ref ddsi_type %p refc %"PRIu32"\n", t, t->refc);
  if (type)
    *type = t;
}

void ddsi_type_ref (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct ddsi_type *src)
{
  ddsrt_mutex_lock (&gv->typelib_lock);
  ddsi_type_ref_locked (gv, type, src);
  ddsrt_mutex_unlock (&gv->typelib_lock);
}

dds_return_t ddsi_type_ref_id_locked_impl (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct DDS_XTypes_TypeIdentifier *type_id)
{
  struct ddsi_typeid_str tistr;
  dds_return_t ret = DDS_RETCODE_OK;
  if (ddsi_typeid_is_none_impl (type_id))
    return DDS_RETCODE_BAD_PARAMETER;

  GVTRACE ("ref ddsi_type type-id %s", ddsi_make_typeid_str_impl (&tistr, type_id));
  struct ddsi_type *t = ddsi_type_lookup_locked_impl (gv, type_id);
  if (!t && (ret = ddsi_type_new (gv, &t, type_id, NULL)) != DDS_RETCODE_OK)
  {
    if (type)
      *type = NULL;
    return ret;
  }
  t->refc++;
  GVTRACE (" refc %"PRIu32"\n", t->refc);
  if (type)
    *type = t;
  return ret;
}

dds_return_t ddsi_type_ref_id_locked (struct ddsi_domaingv *gv, struct ddsi_type **type, const ddsi_typeid_t *type_id)
{
  return ddsi_type_ref_id_locked_impl (gv, type, &type_id->x);
}

static bool valid_top_level_type (const struct ddsi_type *type)
{
  if (type->state == DDSI_TYPE_INVALID)
    return false;
  if (type->xt.kind != DDSI_TYPEID_KIND_COMPLETE && type->xt.kind != DDSI_TYPEID_KIND_MINIMAL)
    return false;
  if (ddsi_xt_is_resolved (&type->xt) && type->xt._d != DDS_XTypes_TK_STRUCTURE && type->xt._d != DDS_XTypes_TK_UNION)
    return false;
  return true;
}

dds_return_t ddsi_type_ref_local (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct ddsi_sertype *sertype, ddsi_typeid_kind_t kind)
{
  struct ddsi_generic_proxy_endpoint **gpe_match_upd = NULL;
  uint32_t n_match_upd = 0;
  struct ddsi_typeid_str tistr;
  dds_return_t ret = DDS_RETCODE_OK;
  bool resolved = false;

  assert (sertype);
  assert (kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_COMPLETE);
  ddsi_typeinfo_t *type_info = ddsi_sertype_typeinfo (sertype);
  if (!type_info)
  {
    if (type)
      *type = NULL;
    return DDS_RETCODE_OK;
  }

  ddsi_typemap_t *type_map = ddsi_sertype_typemap (sertype);
  const struct DDS_XTypes_TypeIdentifier *type_id = (kind == DDSI_TYPEID_KIND_MINIMAL) ? &type_info->x.minimal.typeid_with_size.type_id : &type_info->x.complete.typeid_with_size.type_id;
  const struct DDS_XTypes_TypeObject *type_obj = ddsi_typemap_typeobj (type_map, type_id);

  GVTRACE ("ref ddsi_type local sertype %p id %s", sertype, ddsi_make_typeid_str_impl (&tistr, type_id));

  ddsrt_mutex_lock (&gv->typelib_lock);
  struct ddsi_type *t = ddsi_type_lookup_locked_impl (gv, type_id);
  if (!t)
  {
    ret = ddsi_type_new (gv, &t, type_id, type_obj);
    resolved = true;
  }
  else if (type_obj)
  {
    enum ddsi_type_state s = t->state;
    ret = ddsi_type_add_typeobj (gv, t, type_obj);
    resolved = (t->state == DDSI_TYPE_RESOLVED && t->state != s);
  }
  if (ret != DDS_RETCODE_OK)
  {
    ddsrt_mutex_unlock (&gv->typelib_lock);
    goto err;
  }

  t->refc++;
  GVTRACE (" refc %"PRIu32"\n", t->refc);

  if ((ret = valid_top_level_type (t) ? DDS_RETCODE_OK : DDS_RETCODE_BAD_PARAMETER)
      || (ret = type_add_deps (gv, t, type_info, type_map, kind, &n_match_upd, &gpe_match_upd))
      || (ret = ddsi_xt_validate (gv, &t->xt)))
  {
    GVWARNING ("local sertype with invalid top-level type %s\n", ddsi_make_typeid_str (&tistr, &t->xt.id));
    ddsi_type_unref_locked (gv, t);
    ddsrt_mutex_unlock (&gv->typelib_lock);
    goto err;
  }

  if (resolved)
  {
    GVTRACE ("type %s resolved\n", ddsi_make_typeid_str_impl (&tistr, type_id));
    ddsrt_cond_broadcast (&gv->typelib_resolved_cond);
  }
  ddsrt_mutex_unlock (&gv->typelib_lock);

  if (gpe_match_upd != NULL)
  {
    for (uint32_t e = 0; e < n_match_upd; e++)
    {
      GVTRACE ("type %s trigger matching "PGUIDFMT"\n", ddsi_make_typeid_str_impl (&tistr, type_id), PGUID(gpe_match_upd[e]->e.guid));
      ddsi_update_proxy_endpoint_matching (gv, gpe_match_upd[e]);
    }
    ddsrt_free (gpe_match_upd);
  }
  if (type)
    *type = t;

err:
  ddsi_typemap_fini (type_map);
  ddsrt_free (type_map);
  ddsi_typeinfo_fini (type_info);
  ddsrt_free (type_info);
  return ret;
}

dds_return_t ddsi_type_ref_proxy (struct ddsi_domaingv *gv, struct ddsi_type **type, const ddsi_typeinfo_t *type_info, ddsi_typeid_kind_t kind, const ddsi_guid_t *proxy_guid)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct ddsi_typeid_str tistr;
  assert (type_info);
  assert (kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_COMPLETE);
  const struct DDS_XTypes_TypeIdentifier *type_id = (kind == DDSI_TYPEID_KIND_MINIMAL) ? &type_info->x.minimal.typeid_with_size.type_id : &type_info->x.complete.typeid_with_size.type_id;

  ddsrt_mutex_lock (&gv->typelib_lock);

  GVTRACE ("ref ddsi_type proxy id %s", ddsi_make_typeid_str_impl (&tistr, type_id));
  struct ddsi_type *t = ddsi_type_lookup_locked_impl (gv, type_id);
  if (!t && (ret = ddsi_type_new (gv, &t, type_id, NULL)) != DDS_RETCODE_OK)
    goto err;
  t->refc++;
  GVTRACE(" refc %"PRIu32"\n", t->refc);
  if (!valid_top_level_type (t))
  {
    ret = DDS_RETCODE_BAD_PARAMETER;
    ddsi_type_unref_locked (gv, t);
    GVTRACE (" invalid top-level type\n");
    goto err;
  }

  if ((ret = type_add_deps (gv, t, type_info, NULL, kind, NULL, NULL))
      || (ret = ddsi_xt_validate (gv, &t->xt)))
  {
    ddsi_type_unref_locked (gv, t);
    goto err;
  }

  if (proxy_guid != NULL && !ddsi_type_proxy_guid_exists (t, proxy_guid))
  {
    ddsi_type_proxy_guid_list_insert (&t->proxy_guids, *proxy_guid);
    GVTRACE ("type %s add ep "PGUIDFMT"\n", ddsi_make_typeid_str_impl (&tistr, type_id), PGUID (*proxy_guid));
  }
  if (type)
    *type = t;
err:
  ddsrt_mutex_unlock (&gv->typelib_lock);
  return ret;
}

static dds_return_t xcdr2_ser (const void *obj, const struct dds_cdrstream_desc *desc, dds_ostream_t *os)
{
  os->m_buffer = NULL;
  os->m_index = 0;
  os->m_size = 0;
  os->m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2;
  dds_return_t ret = dds_stream_write_sampleLE ((dds_ostreamLE_t *) os, &dds_cdrstream_default_allocator, obj, desc) ? DDS_RETCODE_OK : DDS_RETCODE_BAD_PARAMETER;
  return ret;
}

static dds_return_t get_typeid_with_size (DDS_XTypes_TypeIdentifierWithSize *typeid_with_size, const DDS_XTypes_TypeIdentifier *ti, const DDS_XTypes_TypeObject *to)
{
  dds_return_t ret;
  dds_ostream_t os;
  ddsi_typeid_copy_impl (&typeid_with_size->type_id, ti);
  if ((ret = xcdr2_ser (to, &DDS_XTypes_TypeObject_cdrstream_desc, &os)) < 0)
    return ret;
  typeid_with_size->typeobject_serialized_size = os.m_index;
  dds_ostream_fini (&os, &dds_cdrstream_default_allocator);
  return DDS_RETCODE_OK;
}

static dds_return_t DDS_XTypes_TypeIdentifierWithDependencies_deps_init (DDS_XTypes_TypeIdentifierWithDependencies *x, uint32_t n_deps)
{
  x->dependent_typeid_count = 0;
  x->dependent_typeids._release = true;
  x->dependent_typeids._length = 0;
  x->dependent_typeids._maximum = n_deps;
  if (n_deps == 0)
    x->dependent_typeids._buffer = NULL;
  else
  {
    if ((x->dependent_typeids._buffer = ddsrt_calloc (n_deps, sizeof (*x->dependent_typeids._buffer))) == NULL)
      return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  return DDS_RETCODE_OK;
}

static dds_return_t DDS_XTypes_TypeIdentifierWithDependencies_deps_append (DDS_XTypes_TypeIdentifierWithDependencies *x, const DDS_XTypes_TypeIdentifier *ti, const DDS_XTypes_TypeObject *to)
{
  assert (x->dependent_typeid_count >= 0  && (uint32_t) x->dependent_typeid_count == x->dependent_typeids._length);
  assert (x->dependent_typeids._length < x->dependent_typeids._maximum);
  dds_return_t ret;
  if ((ret = get_typeid_with_size (&x->dependent_typeids._buffer[x->dependent_typeids._length], ti, to)))
    return ret;
  // if identical to one already present, ignore it (needed for minimal typeobjects)
  for (uint32_t i = 0; i < x->dependent_typeids._length; i++)
  {
    if (ddsi_typeid_compare_impl (&x->dependent_typeids._buffer[i].type_id, &x->dependent_typeids._buffer[x->dependent_typeids._length].type_id) == 0)
    {
      assert (x->dependent_typeids._buffer[i].typeobject_serialized_size == x->dependent_typeids._buffer[x->dependent_typeids._length].typeobject_serialized_size);
      return DDS_RETCODE_OK;
    }
  }
  x->dependent_typeid_count++;
  x->dependent_typeids._length++;
  return DDS_RETCODE_OK;
}

static void DDS_XTypes_TypeIdentifierWithDependencies_deps_fini (DDS_XTypes_TypeIdentifierWithDependencies *x)
{
  for (uint32_t i = 0; i < x->dependent_typeids._length; i++)
    ddsi_typeid_fini_impl (&x->dependent_typeids._buffer[i].type_id);
  ddsrt_free (x->dependent_typeids._buffer);
}

static dds_return_t ddsi_typeinfo_deps_init (struct ddsi_typeinfo *type_info, uint32_t n_deps)
{
  dds_return_t ret;
  if ((ret = DDS_XTypes_TypeIdentifierWithDependencies_deps_init (&type_info->x.minimal, n_deps)) != 0)
    return ret;
  if ((ret = DDS_XTypes_TypeIdentifierWithDependencies_deps_init (&type_info->x.complete, n_deps)) != 0)
    DDS_XTypes_TypeIdentifierWithDependencies_deps_fini (&type_info->x.minimal);
  return ret;
}

static dds_return_t ddsi_typeinfo_deps_append (struct ddsi_domaingv *gv, struct ddsi_typeinfo *type_info, const struct ddsi_type_dep *dep)
{
  struct ddsi_type const * const dep_type = ddsi_type_lookup_locked (gv, &dep->dep_type_id);
  DDS_XTypes_TypeObject to_dep_m;
  ddsi_typeid_t ti_dep_m;
  dds_return_t ret;

  ddsi_xt_get_typeobject_kind_impl (&dep_type->xt, &to_dep_m, DDSI_TYPEID_KIND_MINIMAL);
  if ((ret = ddsi_typeobj_get_hash_id (&to_dep_m, &ti_dep_m)))
  {
    ddsi_typeobj_fini_impl (&to_dep_m);
    return ret;
  }

  DDS_XTypes_TypeObject to_dep_c;
  ddsi_xt_get_typeobject_kind_impl (&dep_type->xt, &to_dep_c, DDSI_TYPEID_KIND_COMPLETE);

  // append dedups because two different complete type ids can map to the same minimal type id
  // (it does so inefficiently, but ... well ... good enough for now)
  if ((ret = DDS_XTypes_TypeIdentifierWithDependencies_deps_append (&type_info->x.minimal, &ti_dep_m.x, &to_dep_m)) == 0)
    ret = DDS_XTypes_TypeIdentifierWithDependencies_deps_append (&type_info->x.complete, &dep_type->xt.id.x, &to_dep_c);

  ddsi_typeobj_fini_impl (&to_dep_c);
  ddsi_typeobj_fini_impl (&to_dep_m);
  ddsi_typeid_fini (&ti_dep_m);
  return ret;
}

static void ddsi_typeinfo_deps_fini (struct ddsi_typeinfo *type_info)
{
  DDS_XTypes_TypeIdentifierWithDependencies_deps_fini (&type_info->x.minimal);
  DDS_XTypes_TypeIdentifierWithDependencies_deps_fini (&type_info->x.complete);
}

static dds_return_t ddsi_type_get_typeinfo_toplevel (struct ddsi_domaingv *gv, const struct ddsi_type *type, struct ddsi_typeinfo *type_info)
{
  DDS_XTypes_TypeObject to_c, to_m;
  ddsi_typeid_t ti_m;
  dds_return_t ret;
  ddsi_xt_get_typeobject_kind_impl (&type->xt, &to_c, DDSI_TYPEID_KIND_COMPLETE);
  ddsi_xt_get_typeobject_kind_impl (&type->xt, &to_m, DDSI_TYPEID_KIND_MINIMAL);
  if ((ret = ddsi_typeobj_get_hash_id (&to_m, &ti_m)))
    goto err_typeid;

#ifndef NDEBUG
  {
    ddsi_typeid_t ti_c;
    if (ddsi_typeobj_get_hash_id (&to_c, &ti_c) != 0)
      abort ();
    assert (ddsi_typeid_compare(&type->xt.id, &ti_c) == 0);
    ddsi_typeid_fini (&ti_c);
  }
  struct ddsi_type *tmp_type = ddsi_type_lookup_locked (gv, &type->xt.id);
  assert (tmp_type);
#else
  (void) gv;
#endif

  if ((ret = get_typeid_with_size (&type_info->x.minimal.typeid_with_size, &ti_m.x, &to_m)) == 0)
    ret = get_typeid_with_size (&type_info->x.complete.typeid_with_size, &type->xt.id.x, &to_c);

  ddsi_typeid_fini (&ti_m);
err_typeid:
  ddsi_typeobj_fini_impl (&to_c);
  ddsi_typeobj_fini_impl (&to_m);
  return ret;
}

dds_return_t ddsi_type_get_typeinfo (struct ddsi_domaingv *gv, const struct ddsi_type *type, struct ddsi_typeinfo *type_info)
{
  dds_return_t ret;

  assert (ddsi_typeid_is_complete (&type->xt.id));
  if ((ret = ddsi_type_get_typeinfo_toplevel (gv, type, type_info)))
    return ret;

  struct ddsi_type_dep tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.src_type_id, &type->xt.id);

  uint32_t n_deps = 0;
  struct ddsi_type_dep *dep = &tmpl;
  while ((dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_treedef, &gv->typedeps, dep)) && !ddsi_typeid_compare (&type->xt.id, &dep->src_type_id))
    n_deps += ddsi_typeid_is_hash (&dep->dep_type_id) ? 1 : 0;
  assert (n_deps <= INT32_MAX);

  if ((ret = ddsi_typeinfo_deps_init (type_info, n_deps)))
  {
    ddsi_typeid_fini (&tmpl.src_type_id);
    return ret;
  }

  dep = &tmpl;
  while ((dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_treedef, &gv->typedeps, dep)) && !ddsi_typeid_compare (&type->xt.id, &dep->src_type_id))
  {
    /* XTypes spec 7.6.3.2.1: The TypeIdentifiers included in the TypeInformation shall include only direct HASH TypeIdentifiers,
     so we'll skip both fully descriptive and indirect hash identifiers (kind DDSI_TYPEID_KIND_PLAIN_COLLECTION_MINIMAL and
     DDSI_TYPEID_KIND_PLAIN_COLLECTION_COMPLETE) */
    if (!ddsi_typeid_is_hash (&dep->dep_type_id))
      continue;
    if ((ret = ddsi_typeinfo_deps_append (gv, type_info, dep)) != 0)
    {
      ddsi_typeinfo_deps_fini (type_info);
      ddsi_typeid_fini (&tmpl.src_type_id);
      return ret;
    }
  }

  ddsi_typeid_fini (&tmpl.src_type_id);
  return ret;
}

dds_return_t ddsi_type_get_typeinfo_ser (struct ddsi_domaingv *gv, const struct ddsi_type *type, unsigned char **data, uint32_t *sz)
{
  dds_return_t ret;
  dds_ostream_t os = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 };
  struct ddsi_typeinfo type_info;
  if ((ret = ddsi_type_get_typeinfo (gv, type, &type_info)))
    goto err_typeinfo;
  if ((ret = xcdr2_ser (&type_info.x, &DDS_XTypes_TypeInformation_cdrstream_desc, &os)) != DDS_RETCODE_OK)
    goto err_ser;
  ddsi_typeinfo_fini (&type_info);
  *data = os.m_buffer;
  *sz = os.m_index;
err_ser:
  ddsi_typeinfo_fini (&type_info);
err_typeinfo:
  return ret;
}

static void typemap_add_type (struct ddsi_typemap *type_map, const struct ddsi_type *type)
{
  uint32_t n = type_map->x.identifier_complete_minimal._length;
  type_map->x.identifier_complete_minimal._length++;
  type_map->x.identifier_complete_minimal._maximum++;

  type_map->x.identifier_object_pair_minimal._length++;
  type_map->x.identifier_object_pair_minimal._maximum++;
  ddsi_xt_get_typeobject_kind_impl (&type->xt, &type_map->x.identifier_object_pair_minimal._buffer[n].type_object, DDSI_TYPEID_KIND_MINIMAL);
  ddsi_typeobj_get_hash_id_impl (&type_map->x.identifier_object_pair_minimal._buffer[n].type_object, &type_map->x.identifier_object_pair_minimal._buffer[n].type_identifier);
  ddsi_typeid_copy_impl (&type_map->x.identifier_complete_minimal._buffer[n].type_identifier2, &type_map->x.identifier_object_pair_minimal._buffer[n].type_identifier);

  type_map->x.identifier_object_pair_complete._length++;
  type_map->x.identifier_object_pair_complete._maximum++;
  ddsi_xt_get_typeobject_kind_impl (&type->xt, &type_map->x.identifier_object_pair_complete._buffer[n].type_object, DDSI_TYPEID_KIND_COMPLETE);
  ddsi_typeobj_get_hash_id_impl (&type_map->x.identifier_object_pair_complete._buffer[n].type_object, &type_map->x.identifier_object_pair_complete._buffer[n].type_identifier);
  ddsi_typeid_copy_impl (&type_map->x.identifier_complete_minimal._buffer[n].type_identifier1, &type_map->x.identifier_object_pair_complete._buffer[n].type_identifier);
}

static dds_return_t ddsi_type_get_typemap (struct ddsi_domaingv *gv, const struct ddsi_type *type, struct ddsi_typemap *type_map)
{
  dds_return_t ret = DDS_RETCODE_OK;
  memset (type_map, 0, sizeof (*type_map));

  struct ddsi_type_dep tmpl, *dep = &tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.src_type_id, &type->xt.id);

  uint32_t n_deps = 0;
  while ((dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_treedef, &gv->typedeps, dep)) && !ddsi_typeid_compare (&type->xt.id, &dep->src_type_id))
    n_deps += ddsi_typeid_is_hash (&dep->dep_type_id) ? 1 : 0;

  if (!(type_map->x.identifier_complete_minimal._buffer = ddsrt_calloc (1 + n_deps, sizeof (*type_map->x.identifier_complete_minimal._buffer)))
      || !(type_map->x.identifier_object_pair_minimal._buffer = ddsrt_calloc (1 + n_deps, sizeof (*type_map->x.identifier_object_pair_minimal._buffer)))
      || !(type_map->x.identifier_object_pair_complete._buffer = ddsrt_calloc (1 + n_deps, sizeof (*type_map->x.identifier_object_pair_complete._buffer))))
  {
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto err;
  }

  type_map->x.identifier_complete_minimal._release = true;
  type_map->x.identifier_object_pair_minimal._release = true;
  type_map->x.identifier_object_pair_complete._release = true;

  // add top-level type to typemap
  typemap_add_type (type_map, type);

  // add dependent types
  struct ddsi_type *dep_type;
  if (n_deps > 0)
  {
    dep = &tmpl;
    while ((dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_treedef, &gv->typedeps, dep)) && !ddsi_typeid_compare (&type->xt.id, &dep->src_type_id))
    {
      if (!ddsi_typeid_is_hash (&dep->dep_type_id))
        continue;
      if (!(dep_type = ddsi_type_lookup_locked (gv, &dep->dep_type_id)))
      {
        ret = DDS_RETCODE_ERROR;
        goto err;
      }
      typemap_add_type (type_map, dep_type);
    }
  }

err:
  if (ret != DDS_RETCODE_OK)
  {
    if (type_map->x.identifier_complete_minimal._buffer)
      ddsrt_free (type_map->x.identifier_complete_minimal._buffer);
    if (type_map->x.identifier_object_pair_minimal._buffer)
      ddsrt_free (type_map->x.identifier_object_pair_minimal._buffer);
    if (type_map->x.identifier_object_pair_complete._buffer)
      ddsrt_free (type_map->x.identifier_object_pair_complete._buffer);
  }
  ddsi_typeid_fini (&tmpl.src_type_id);
  return ret;
}

dds_return_t ddsi_type_get_typemap_ser (struct ddsi_domaingv *gv, const struct ddsi_type *type, unsigned char **data, uint32_t *sz)
{
  dds_return_t ret;
  dds_ostream_t os = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 };
  struct ddsi_typemap type_map;
  if ((ret = ddsi_type_get_typemap (gv, type, &type_map)))
    goto err_typemap;
  if ((ret = xcdr2_ser (&type_map.x, &DDS_XTypes_TypeMapping_cdrstream_desc, &os)) != DDS_RETCODE_OK)
    goto err_ser;
  ddsi_typemap_fini (&type_map);
  *data = os.m_buffer;
  *sz = os.m_index;
err_ser:
  ddsi_typemap_fini (&type_map);
err_typemap:
  return ret;
}

struct ddsi_typeobj *ddsi_type_get_typeobj (struct ddsi_domaingv *gv, const struct ddsi_type *type)
{
  if (!ddsi_type_resolved_locked (gv, type, DDSI_TYPE_IGNORE_DEPS))
    return NULL;

  ddsi_typeobj_t *to = ddsrt_malloc (sizeof (*to));
  ddsi_xt_get_typeobject (&type->xt, to);
  return to;
}

struct ddsi_domaingv *ddsi_type_get_gv (const struct ddsi_type *type)
{
  return type->gv;
}

DDS_XTypes_TypeKind ddsi_type_get_kind (const struct ddsi_type *type)
{
  return type->xt._d;
}

static void ddsi_type_unref_impl_locked (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  assert (type->refc > 0);
  if (--type->refc == 0)
  {
    GVTRACE (" refc 0 remove type ");
    if (type->state != DDSI_TYPE_CONSTRUCTING)
      ddsrt_avl_delete (&ddsi_typelib_treedef, &gv->typelib, type);
    ddsi_type_free (type);
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

void ddsi_type_unref_locked (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  if (type == NULL)
    return;
  struct ddsi_typeid_str tistr;
  GVTRACE ("unref ddsi_type id %s", ddsi_make_typeid_str (&tistr, &type->xt.id));
  ddsi_type_unref_impl_locked (gv, type);
  GVTRACE ("\n");
}

void ddsi_type_unref (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  ddsrt_mutex_lock (&gv->typelib_lock);
  ddsi_type_unref_locked (gv, type);
  ddsrt_mutex_unlock (&gv->typelib_lock);
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

static void ddsi_type_get_gpe_matches_impl (struct ddsi_domaingv *gv, const struct ddsi_type *type, struct ddsi_generic_proxy_endpoint ***gpe_match_upd, uint32_t *n_match_upd)
{
  if (!ddsi_type_proxy_guid_list_count (&type->proxy_guids))
    return;

  uint32_t n = 0;
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gv);
  *gpe_match_upd = ddsrt_realloc (*gpe_match_upd, (*n_match_upd + ddsi_type_proxy_guid_list_count (&type->proxy_guids)) * sizeof (**gpe_match_upd));
  struct ddsi_type_proxy_guid_list_iter it;
  for (ddsi_guid_t guid = ddsi_type_proxy_guid_list_iter_first (&type->proxy_guids, &it); !ddsi_is_null_guid (&guid); guid = ddsi_type_proxy_guid_list_iter_next (&it))
  {
    if (!ddsi_is_topic_entityid (guid.entityid))
    {
      struct ddsi_entity_common *ec = ddsi_entidx_lookup_guid_untyped (gv->entity_index, &guid);
      if (ec != NULL)
      {
        assert (ec->kind == DDSI_EK_PROXY_READER || ec->kind == DDSI_EK_PROXY_WRITER);
        (*gpe_match_upd)[*n_match_upd + n++] = (struct ddsi_generic_proxy_endpoint *) ec;
      }
    }
  }
  *n_match_upd += n;
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
}

void ddsi_type_get_gpe_matches (struct ddsi_domaingv *gv, const struct ddsi_type *type, struct ddsi_generic_proxy_endpoint ***gpe_match_upd, uint32_t *n_match_upd)
{
  /* No check for resolved state of dependencies for this type at this point: matching for
     endpoints using this type as top-level type (or dependent type, via reverse
     dependencies) will be re-tried, and missing dependencies will be requested from there.
     This should be changed so that dependencies are requested from this point, which will
     also fix type resolvind triggered by find_topic, in case an incomplete type lookup
     response is received and additional types need to be requested. */
  ddsi_type_get_gpe_matches_impl (gv, type, gpe_match_upd, n_match_upd);
  struct ddsi_type_dep tmpl, *reverse_dep = &tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.dep_type_id, &type->xt.id);
  while ((reverse_dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_reverse_treedef, &gv->typedeps_reverse, reverse_dep)) && !ddsi_typeid_compare (&type->xt.id, &reverse_dep->dep_type_id))
  {
    struct ddsi_type *dep_src_type = ddsi_type_lookup_locked (gv, &reverse_dep->src_type_id);
    ddsi_type_get_gpe_matches (gv, dep_src_type, gpe_match_upd, n_match_upd);
  }
  ddsi_typeid_fini (&tmpl.dep_type_id);
}

bool ddsi_type_resolved_locked (struct ddsi_domaingv *gv, const struct ddsi_type *type, ddsi_type_include_deps_t resolved_kind)
{
  bool resolved = type && ddsi_xt_is_resolved (&type->xt);
  assert (resolved_kind == DDSI_TYPE_IGNORE_DEPS || resolved_kind == DDSI_TYPE_INCLUDE_DEPS);
  if (resolved && resolved_kind == DDSI_TYPE_INCLUDE_DEPS)
  {
    struct ddsi_type_dep tmpl, *dep = &tmpl;
    memset (&tmpl, 0, sizeof (tmpl));
    ddsi_typeid_copy (&tmpl.src_type_id, &type->xt.id);
    while (resolved && (dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_treedef, &gv->typedeps, dep)) && !ddsi_typeid_compare (&type->xt.id, &dep->src_type_id))
    {
      struct ddsi_type *dep_type = ddsi_type_lookup_locked (gv, &dep->dep_type_id);
      if (dep_type && ddsi_xt_is_unresolved (&dep_type->xt))
        resolved = false;
      else
        resolved = ddsi_type_resolved_locked (gv, dep_type, resolved_kind);
    }
    ddsi_typeid_fini (&tmpl.src_type_id);
  }
  return resolved;
}

bool ddsi_type_resolved (struct ddsi_domaingv *gv, const struct ddsi_type *type, ddsi_type_include_deps_t resolved_kind)
{
  ddsrt_mutex_lock (&gv->typelib_lock);
  bool ret = ddsi_type_resolved_locked (gv, type, resolved_kind);
  ddsrt_mutex_unlock (&gv->typelib_lock);
  return ret;
}

bool ddsi_is_assignable_from (struct ddsi_domaingv *gv, const struct ddsi_type_pair *rd_type_pair, uint32_t rd_resolved, const struct ddsi_type_pair *wr_type_pair, uint32_t wr_resolved, const dds_type_consistency_enforcement_qospolicy_t *tce)
{
  if (!rd_type_pair || !wr_type_pair)
    return false;
  ddsrt_mutex_lock (&gv->typelib_lock);
  const struct xt_type
    *rd_xt = (rd_resolved == DDS_XTypes_EK_BOTH || rd_resolved == DDS_XTypes_EK_MINIMAL) ? &rd_type_pair->minimal->xt : &rd_type_pair->complete->xt,
    *wr_xt = (wr_resolved == DDS_XTypes_EK_BOTH || wr_resolved == DDS_XTypes_EK_MINIMAL) ? &wr_type_pair->minimal->xt : &wr_type_pair->complete->xt;
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

ddsi_typeinfo_t *ddsi_type_pair_minimal_info (struct ddsi_domaingv *gv, const struct ddsi_type_pair *type_pair)
{
  if (type_pair == NULL || type_pair->minimal == NULL)
    return NULL;
  ddsi_typeinfo_t *type_info;
  if (!(type_info = ddsrt_malloc (sizeof (*type_info))))
    return NULL;
  if (ddsi_type_get_typeinfo (gv, type_pair->minimal, type_info))
  {
    ddsrt_free (type_info);
    return NULL;
  }
  return type_info;
}

ddsi_typeinfo_t *ddsi_type_pair_complete_info (struct ddsi_domaingv *gv, const struct ddsi_type_pair *type_pair)
{
  if (type_pair == NULL || type_pair->complete == NULL)
    return NULL;
  ddsi_typeinfo_t *type_info;
  if (!(type_info = ddsrt_malloc (sizeof (*type_info))))
    return NULL;
  if (ddsi_type_get_typeinfo (gv, type_pair->complete, type_info))
  {
    ddsrt_free (type_info);
    return NULL;
  }
  return type_info;
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

static dds_return_t check_type_resolved_impl_locked (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id, dds_duration_t timeout, struct ddsi_type **type, ddsi_type_include_deps_t resolved_kind, bool *resolved)
{
  dds_return_t ret = DDS_RETCODE_OK;

  /* For a type to be resolved, we require it's top-level type identifier to be known
      and added to the type library as a result of a discovered endpoint or topic,
      or a topic created locally. */
  if ((*type = ddsi_type_lookup_locked (gv, type_id)) == NULL)
    ret = DDS_RETCODE_PRECONDITION_NOT_MET;
  else if (ddsi_type_resolved_locked (gv, *type, resolved_kind))
  {
    ddsi_type_ref_locked (gv, NULL, *type);
    *resolved = true;
  }
  else if (!timeout)
    ret = DDS_RETCODE_TIMEOUT;
  else
    *resolved = false;

  return ret;
}

static dds_return_t wait_for_type_resolved_impl_locked (struct ddsi_domaingv *gv, dds_duration_t timeout, const struct ddsi_type *type, ddsi_type_include_deps_t resolved_kind)
{
  const dds_time_t tnow = dds_time ();
  const dds_time_t abstimeout = (DDS_INFINITY - timeout <= tnow) ? DDS_NEVER : (tnow + timeout);
  while (!ddsi_type_resolved_locked (gv, type, resolved_kind))
  {
    if (!ddsrt_cond_waituntil (&gv->typelib_resolved_cond, &gv->typelib_lock, abstimeout))
      return DDS_RETCODE_TIMEOUT;
  }
  ddsi_type_ref_locked (gv, NULL, type);
  return DDS_RETCODE_OK;
}

dds_return_t ddsi_wait_for_type_resolved (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id, dds_duration_t timeout, struct ddsi_type **type, ddsi_type_include_deps_t resolved_kind, ddsi_type_request_t request)
{
  dds_return_t ret;
  bool resolved;

  assert (type);
  if (ddsi_typeid_is_none (type_id) || !ddsi_typeid_is_hash (type_id))
    return DDS_RETCODE_BAD_PARAMETER;

  ddsrt_mutex_lock (&gv->typelib_lock);
  ret = check_type_resolved_impl_locked (gv, type_id, timeout, type, resolved_kind, &resolved);
  ddsrt_mutex_unlock (&gv->typelib_lock);

  if (ret != DDS_RETCODE_OK || resolved)
    return ret;

  // TODO: provide proxy pp guid to ddsi_tl_request_type so that request can be sent to a specific node
  if (request == DDSI_TYPE_SEND_REQUEST && !ddsi_tl_request_type (gv, type_id, NULL, resolved_kind))
    return DDS_RETCODE_PRECONDITION_NOT_MET;

  ddsrt_mutex_lock (&gv->typelib_lock);
  ret = wait_for_type_resolved_impl_locked (gv, timeout, *type, resolved_kind);
  ddsrt_mutex_unlock (&gv->typelib_lock);

  return ret;
}

#endif /* DDS_HAS_TYPE_DISCOVERY */
