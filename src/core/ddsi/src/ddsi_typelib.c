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

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_log.h"
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

#ifndef NDEBUG
void ddsi_typelib_dump_if_not_empty (struct ddsi_domaingv *gv)
{
  if (ddsrt_avl_is_empty (&gv->typelib))
    return;

  GVERROR ("typelib not empty at shutdown:\n");
  ddsrt_avl_iter_t it;
  for (struct ddsi_type *type = ddsrt_avl_iter_first (&ddsi_typelib_treedef, &gv->typelib, &it);
       type != NULL; type = ddsrt_avl_iter_next (&it))
  {
    if (type->scc)
      GVERROR ("  type=%p state=%d kind=%d refc=%"PRIu32" scc=%p scc_refc=%"PRIu32" active=%d freeing=%d nwire=%"PRIu32" ntypes=%"PRIu32" generated_minimal_ref=%p\n",
               (void *) type, type->state, type->xt.id.x._d, type->refc, (void *) type->scc,
               type->scc->refc, type->scc->active, type->scc->freeing,
               type->scc->n_wire_types, type->scc->n_types,
               (void *) type->scc->generated_minimal_ref);
    else
      GVERROR ("  type=%p state=%d kind=%d refc=%"PRIu32" scc=%p\n",
               (void *) type, type->state, type->xt.id.x._d, type->refc, (void *) type->scc);
  }
}
#endif

static const char *type_state_str (enum ddsi_type_state state)
{
  switch (state)
  {
    case DDSI_TYPE_UNRESOLVED: return "UNRESOLVED";
    case DDSI_TYPE_REQUESTED: return "REQUESTED";
    case DDSI_TYPE_PARTIAL_RESOLVED: return "PARTIAL_RESOLVED";
    case DDSI_TYPE_RESOLVED: return "RESOLVED";
    case DDSI_TYPE_INVALID: return "INVALID";
    case DDSI_TYPE_CONSTRUCTING: return "CONSTRUCTING";
    case DDSI_TYPE_COMPLETING: return "COMPLETING";
    case DDSI_TYPE_REPLACED: return "REPLACED";
  }
  return "UNKNOWN";
}

struct typelib_trace_typeid_str {
  char str[96];
};

static const char *typelib_trace_scc_kind_str (uint8_t kind)
{
  switch (kind)
  {
    case DDS_XTypes_EK_MINIMAL: return "MINIMAL";
    case DDS_XTypes_EK_COMPLETE: return "COMPLETE";
    default: return "INVALID";
  }
}

static char *typelib_trace_make_typeid_str (struct typelib_trace_typeid_str *buf, const struct DDS_XTypes_TypeIdentifier *type_id)
{
  if (type_id->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
  {
    const struct DDS_XTypes_StronglyConnectedComponentId *id = &type_id->_u.sc_component_id;
    const unsigned char *hash = id->sc_component_id._u.hash;
    snprintf (buf->str, sizeof (buf->str),
              "[SCC %s len=%d idx=%d %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x]",
              typelib_trace_scc_kind_str (id->sc_component_id._d),
              (int) id->scc_length, (int) id->scc_index,
              hash[0], hash[1], hash[2], hash[3], hash[4], hash[5], hash[6],
              hash[7], hash[8], hash[9], hash[10], hash[11], hash[12], hash[13]);
    return buf->str;
  }
  else
  {
    struct ddsi_typeid_str tistr;
    snprintf (buf->str, sizeof (buf->str), "%s", ddsi_make_typeid_str_impl (&tistr, type_id));
    return buf->str;
  }
}

#define TYPELIB_TRACE(...) do { \
    struct typelib_trace_typeid_str tistr; \
    (void) tistr; \
    DDS_CLOG (DDS_LC_TYPELIB, &gv->logconfig, __VA_ARGS__); \
  } while (0)
#define TYPELIB_IMPORT_TRACE(...) TYPELIB_TRACE ("import: " __VA_ARGS__)

struct ddsi_type_visit_node {
  const struct ddsi_type *type;
};

static uint32_t ddsi_type_visit_node_hash (const void *vnode)
{
  const struct ddsi_type_visit_node *node = vnode;
  const uintptr_t x = (uintptr_t) node->type;
  return ddsrt_mh3 (&x, sizeof (x), 0);
}

static bool ddsi_type_visit_node_equal (const void *vnode_a, const void *vnode_b)
{
  const struct ddsi_type_visit_node *a = vnode_a;
  const struct ddsi_type_visit_node *b = vnode_b;
  return a->type == b->type;
}

static void ddsi_type_visit_node_free (void *vnode, void *arg)
{
  (void) arg;
  ddsrt_free (vnode);
}

bool ddsi_type_visit_seen (struct ddsrt_hh *visited, const struct ddsi_type *type)
{
  const struct ddsi_type_visit_node templ = { .type = type };
  if (ddsrt_hh_lookup (visited, &templ) != NULL)
    return true;

  struct ddsi_type_visit_node *node = ddsrt_malloc (sizeof (*node));
  node->type = type;
  ddsrt_hh_add_absent (visited, node);
  return false;
}

struct ddsrt_hh *ddsi_type_visit_new (void)
{
  return ddsrt_hh_new (1, ddsi_type_visit_node_hash, ddsi_type_visit_node_equal);
}

void ddsi_type_visit_free (struct ddsrt_hh *visited)
{
  ddsrt_hh_enum (visited, ddsi_type_visit_node_free, NULL);
  ddsrt_hh_free (visited);
}

bool ddsi_typeinfo_equal (const ddsi_typeinfo_t *a, const ddsi_typeinfo_t *b, enum ddsi_type_include_deps deps)
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
  unsigned char *data_norm;
  uint32_t srcoff = 0;

  if (sz == 0 || data == NULL)
    return NULL;

  /* Type objects are stored as a LE serialized CDR blob in the topic descriptor */
  DDSRT_WARNING_MSVC_OFF(6326)
  bool bswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  DDSRT_WARNING_MSVC_ON(6326)
  data_norm = ddsrt_memdup (data, sz);
  if (dds_stream_normalize_xcdr2_data ((char *) data_norm, &srcoff, sz, bswap, DDS_XTypes_TypeInformation_desc.m_ops) != DDS_STREAM_NORMALIZE_SUCCESS)
  {
    ddsrt_free (data_norm);
    return NULL;
  }

  dds_istream_t is = { .m_buffer = data_norm, .m_index = 0, .m_size = sz, .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 };
  ddsi_typeinfo_t *typeinfo = ddsrt_calloc (1, sizeof (*typeinfo));
  dds_stream_read (&is, (void *) typeinfo, &dds_cdrstream_default_allocator, DDS_XTypes_TypeInformation_desc.m_ops);
  ddsrt_free (data_norm);
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
      if (ddsi_typeid_kind ((const ddsi_typeid_t *) &t->dependent_typeids._buffer[n].type_id) != kind
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
  const ddsi_typeid_t *tid_min = ddsi_typeinfo_minimal_typeid (typeinfo);
  const ddsi_typeid_t *tid_compl = ddsi_typeinfo_complete_typeid (typeinfo);
  // Based on DDS XTypes 1.3 7.6.3.2.1, one would think the minimal/complete part may contain
  // only MINIMAL/COMPLETE hash identifiers. However, it also says:
  //
  //   The TypeIdentifiers included in the TypeInformation shall include only direct HASH
  //   TypeIdentifiers (see Clause 7.3.4.6.3). In addition it shall not contain individual
  //   type identifiers for types belonging to Strongly Connected Component (i.e. those
  //   with discriminator TI_STRONG_COMPONENT), instead it shall include the identifier
  //   of the whole Strongly-Connected Component (SCCIdentifier, see Clause 7.3.4.9.3).
  //
  // which suggests we should also allow TI_STRONG_COMPONENT.  The equivalence kind in
  // the SCC identifier tells us whether it belongs in the minimal or complete half.
  return (ddsi_typeid_kind (tid_min) == DDSI_TYPEID_KIND_MINIMAL && ddsi_typeid_kind (tid_compl) == DDSI_TYPEID_KIND_COMPLETE &&
          typeinfo_dependent_typeids_valid (&typeinfo->x.minimal, DDSI_TYPEID_KIND_MINIMAL) &&
          typeinfo_dependent_typeids_valid (&typeinfo->x.complete, DDSI_TYPEID_KIND_COMPLETE));
}

static const dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *typemap_typeobj_pairseq (const ddsi_typemap_t *tmap, const struct DDS_XTypes_TypeIdentifier *type_id)
{
  assert (type_id);
  assert (tmap);
  if (!ddsi_typeid_is_hash_impl (type_id))
    return NULL;
  const bool is_minimal = ddsi_typeid_is_minimal_impl (type_id) ||
    (type_id->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT &&
     type_id->_u.sc_component_id.sc_component_id._d == DDS_XTypes_EK_MINIMAL);
  return is_minimal ?
    &tmap->x.identifier_object_pair_minimal : &tmap->x.identifier_object_pair_complete;
}

const struct DDS_XTypes_TypeObject * ddsi_typemap_typeobj (const ddsi_typemap_t *tmap, const struct DDS_XTypes_TypeIdentifier *type_id)
{
  const dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *list = typemap_typeobj_pairseq (tmap, type_id);
  if (list == NULL)
    return NULL;
  for (uint32_t i = 0; i < list->_length; i++)
  {
    DDS_XTypes_TypeIdentifierTypeObjectPair *pair = &list->_buffer[i];
    if (!ddsi_typeid_compare_impl (type_id, &pair->type_identifier))
      return &pair->type_object;
  }
  return NULL;
}

const char * ddsi_typemap_get_type_name (const ddsi_typemap_t *typemap, const ddsi_typeid_t *type_id)
{
  const struct DDS_XTypes_TypeObject *type_obj = ddsi_typemap_typeobj (typemap, &type_id->x);
  if (type_obj == NULL)
    return NULL;
  return ddsi_typeobj_get_type_name_impl (type_obj);
}

ddsi_typemap_t *ddsi_typemap_deser (const unsigned char *data, uint32_t sz)
{
  unsigned char *data_norm;
  uint32_t srcoff = 0;

  if (sz == 0 || data == NULL)
    return NULL;

  DDSRT_WARNING_MSVC_OFF(6326)
  bool bswap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
  DDSRT_WARNING_MSVC_ON(6326)
  data_norm = ddsrt_memdup (data, sz);
  if (dds_stream_normalize_xcdr2_data ((char *) data_norm, &srcoff, sz, bswap, DDS_XTypes_TypeMapping_desc.m_ops) != DDS_STREAM_NORMALIZE_SUCCESS)
  {
    ddsrt_free (data_norm);
    return NULL;
  }

  dds_istream_t is = { .m_buffer = data_norm, .m_index = 0, .m_size = sz, .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 };
  ddsi_typemap_t *typemap = ddsrt_calloc (1, sizeof (*typemap));
  dds_stream_read (&is, (void *) typemap, &dds_cdrstream_default_allocator, DDS_XTypes_TypeMapping_desc.m_ops);
  ddsrt_free (data_norm);
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
      if (dds_stream_write_sample (&to_a_ser, &dds_cdrstream_default_allocator, &a->_buffer[n].type_object, &DDS_XTypes_TypeObject_cdrstream_desc) &&
          dds_stream_write_sample (&to_b_ser, &dds_cdrstream_default_allocator, to_b, &DDS_XTypes_TypeObject_cdrstream_desc))
      {
        equal = (to_a_ser.m_index == to_b_ser.m_index) && memcmp (to_a_ser.m_buffer, to_b_ser.m_buffer, to_a_ser.m_index) == 0;
      }
      else
      {
        // type objects should always be valid, so serialization should succeed
        abort ();
      }
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
  struct typelib_trace_typeid_str tistrdep;
  TYPELIB_TRACE ("%sdep <%s, %s>\n", prefix,
                 typelib_trace_make_typeid_str (&tistr, &dep->src_type_id.x),
                 typelib_trace_make_typeid_str (&tistrdep, &dep->dep_type_id.x));
}

/* A received TypeMap carries TypeObjects with final SCC identifiers. The SCC
   hash, however, is computed over those TypeObjects while same-component SCC
   references still contain the placeholder zero hash used during generation. */
static void type_scc_normalize_typeid (struct DDS_XTypes_TypeIdentifier *type_id, const struct DDS_XTypes_StronglyConnectedComponentId *scc_id)
{
  if (type_id == NULL)
    return;

  switch (type_id->_d)
  {
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      type_scc_normalize_typeid (type_id->_u.seq_sdefn.element_identifier, scc_id);
      break;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      type_scc_normalize_typeid (type_id->_u.seq_ldefn.element_identifier, scc_id);
      break;
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
      type_scc_normalize_typeid (type_id->_u.array_sdefn.element_identifier, scc_id);
      break;
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      type_scc_normalize_typeid (type_id->_u.array_ldefn.element_identifier, scc_id);
      break;
    case DDS_XTypes_TI_PLAIN_MAP_SMALL:
      type_scc_normalize_typeid (type_id->_u.map_sdefn.element_identifier, scc_id);
      type_scc_normalize_typeid (type_id->_u.map_sdefn.key_identifier, scc_id);
      break;
    case DDS_XTypes_TI_PLAIN_MAP_LARGE:
      type_scc_normalize_typeid (type_id->_u.map_ldefn.element_identifier, scc_id);
      type_scc_normalize_typeid (type_id->_u.map_ldefn.key_identifier, scc_id);
      break;
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT:
      if (ddsi_type_scc_id_same_component_impl (&type_id->_u.sc_component_id, scc_id))
        memset (type_id->_u.sc_component_id.sc_component_id._u.hash, 0, sizeof (type_id->_u.sc_component_id.sc_component_id._u.hash));
      break;
    default:
      break;
  }
}

static void type_scc_normalize_complete_typeobj (struct DDS_XTypes_CompleteTypeObject *type_obj, const struct DDS_XTypes_StronglyConnectedComponentId *scc_id)
{
  switch (type_obj->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      type_scc_normalize_typeid (&type_obj->_u.alias_type.body.common.related_type, scc_id);
      break;
    case DDS_XTypes_TK_ANNOTATION:
      for (uint32_t n = 0; n < type_obj->_u.annotation_type.member_seq._length; n++)
        type_scc_normalize_typeid (&type_obj->_u.annotation_type.member_seq._buffer[n].common.member_type_id, scc_id);
      break;
    case DDS_XTypes_TK_STRUCTURE:
      type_scc_normalize_typeid (&type_obj->_u.struct_type.header.base_type, scc_id);
      for (uint32_t n = 0; n < type_obj->_u.struct_type.member_seq._length; n++)
        type_scc_normalize_typeid (&type_obj->_u.struct_type.member_seq._buffer[n].common.member_type_id, scc_id);
      break;
    case DDS_XTypes_TK_UNION:
      type_scc_normalize_typeid (&type_obj->_u.union_type.discriminator.common.type_id, scc_id);
      for (uint32_t n = 0; n < type_obj->_u.union_type.member_seq._length; n++)
        type_scc_normalize_typeid (&type_obj->_u.union_type.member_seq._buffer[n].common.type_id, scc_id);
      break;
    case DDS_XTypes_TK_SEQUENCE:
      type_scc_normalize_typeid (&type_obj->_u.sequence_type.element.common.type, scc_id);
      break;
    case DDS_XTypes_TK_ARRAY:
      type_scc_normalize_typeid (&type_obj->_u.array_type.element.common.type, scc_id);
      break;
    case DDS_XTypes_TK_MAP:
      type_scc_normalize_typeid (&type_obj->_u.map_type.key.common.type, scc_id);
      type_scc_normalize_typeid (&type_obj->_u.map_type.element.common.type, scc_id);
      break;
    default:
      break;
  }
}

static void type_scc_normalize_minimal_typeobj (struct DDS_XTypes_MinimalTypeObject *type_obj, const struct DDS_XTypes_StronglyConnectedComponentId *scc_id)
{
  switch (type_obj->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      type_scc_normalize_typeid (&type_obj->_u.alias_type.body.common.related_type, scc_id);
      break;
    case DDS_XTypes_TK_ANNOTATION:
      for (uint32_t n = 0; n < type_obj->_u.annotation_type.member_seq._length; n++)
        type_scc_normalize_typeid (&type_obj->_u.annotation_type.member_seq._buffer[n].common.member_type_id, scc_id);
      break;
    case DDS_XTypes_TK_STRUCTURE:
      type_scc_normalize_typeid (&type_obj->_u.struct_type.header.base_type, scc_id);
      for (uint32_t n = 0; n < type_obj->_u.struct_type.member_seq._length; n++)
        type_scc_normalize_typeid (&type_obj->_u.struct_type.member_seq._buffer[n].common.member_type_id, scc_id);
      break;
    case DDS_XTypes_TK_UNION:
      type_scc_normalize_typeid (&type_obj->_u.union_type.discriminator.common.type_id, scc_id);
      for (uint32_t n = 0; n < type_obj->_u.union_type.member_seq._length; n++)
        type_scc_normalize_typeid (&type_obj->_u.union_type.member_seq._buffer[n].common.type_id, scc_id);
      break;
    case DDS_XTypes_TK_SEQUENCE:
      type_scc_normalize_typeid (&type_obj->_u.sequence_type.element.common.type, scc_id);
      break;
    case DDS_XTypes_TK_ARRAY:
      type_scc_normalize_typeid (&type_obj->_u.array_type.element.common.type, scc_id);
      break;
    case DDS_XTypes_TK_MAP:
      type_scc_normalize_typeid (&type_obj->_u.map_type.key.common.type, scc_id);
      type_scc_normalize_typeid (&type_obj->_u.map_type.element.common.type, scc_id);
      break;
    default:
      break;
  }
}

static void type_scc_normalize_typeobj (struct DDS_XTypes_TypeObject *type_obj, const struct DDS_XTypes_StronglyConnectedComponentId *scc_id)
{
  if (type_obj->_d == DDS_XTypes_EK_COMPLETE)
    type_scc_normalize_complete_typeobj (&type_obj->_u.complete, scc_id);
  else if (type_obj->_d == DDS_XTypes_EK_MINIMAL)
    type_scc_normalize_minimal_typeobj (&type_obj->_u.minimal, scc_id);
}

static dds_return_t typeobj_deep_copy (struct DDS_XTypes_TypeObject *dst, const struct DDS_XTypes_TypeObject *src)
{
  dds_ostream_t os = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 };
  if (!dds_stream_write_sample (&os, &dds_cdrstream_default_allocator, src, &DDS_XTypes_TypeObject_cdrstream_desc))
  {
    dds_ostream_fini (&os, &dds_cdrstream_default_allocator);
    return DDS_RETCODE_BAD_PARAMETER;
  }
  dds_istream_t is = { .m_buffer = os.m_buffer, .m_index = 0, .m_size = os.m_index, .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 };
  dds_stream_read (&is, (char *) dst, &dds_cdrstream_default_allocator, DDS_XTypes_TypeObject_desc.m_ops);
  dds_ostream_fini (&os, &dds_cdrstream_default_allocator);
  return DDS_RETCODE_OK;
}

static dds_return_t type_scc_collect_component_pairs (
    const dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *pairs,
    const struct DDS_XTypes_StronglyConnectedComponentId *scc_id,
    const struct DDS_XTypes_TypeIdentifierTypeObjectPair ***slots,
    bool *complete)
{
  assert (pairs);
  assert (slots);
  assert (complete);
  *slots = NULL;
  *complete = false;
  if (!ddsi_type_scc_id_is_valid_impl (scc_id))
    return DDS_RETCODE_BAD_PARAMETER;

  const uint32_t nslots = (uint32_t) scc_id->scc_length;
  const struct DDS_XTypes_TypeIdentifierTypeObjectPair **xs = ddsrt_calloc_s (nslots, sizeof (*xs));
  if (xs == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  for (uint32_t n = 0; n < pairs->_length; n++)
  {
    const struct DDS_XTypes_TypeIdentifierTypeObjectPair *pair = &pairs->_buffer[n];
    const struct DDS_XTypes_TypeIdentifier *type_id = &pair->type_identifier;
    if (type_id->_d != DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT ||
        !ddsi_type_scc_id_same_component_impl (&type_id->_u.sc_component_id, scc_id))
      continue;
    if (!ddsi_type_scc_id_is_valid_impl (&type_id->_u.sc_component_id) || pair->type_object._d != scc_id->sc_component_id._d)
    {
      ddsrt_free (xs);
      return DDS_RETCODE_BAD_PARAMETER;
    }
    const uint32_t idx = (uint32_t) type_id->_u.sc_component_id.scc_index - 1;
    if (xs[idx] != NULL)
    {
      ddsrt_free (xs);
      return DDS_RETCODE_BAD_PARAMETER;
    }
    xs[idx] = pair;
  }

  for (uint32_t n = 0; n < nslots; n++)
  {
    if (xs[n] == NULL)
    {
      ddsrt_free (xs);
      return DDS_RETCODE_OK;
    }
  }

  *slots = xs;
  *complete = true;
  return DDS_RETCODE_OK;
}

static dds_return_t type_scc_verify_component_hash (
    const struct DDS_XTypes_StronglyConnectedComponentId *scc_id,
    const struct DDS_XTypes_TypeIdentifierTypeObjectPair * const *slots)
{
  const uint32_t nslots = (uint32_t) scc_id->scc_length;
  struct DDS_XTypes_TypeObject *type_objects = ddsrt_calloc_s (nslots, sizeof (*type_objects));
  if (type_objects == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  dds_return_t ret = DDS_RETCODE_OK;
  for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < nslots; n++)
  {
    if ((ret = typeobj_deep_copy (&type_objects[n], &slots[n]->type_object)) == DDS_RETCODE_OK)
      type_scc_normalize_typeobj (&type_objects[n], scc_id);
  }

  if (ret == DDS_RETCODE_OK)
  {
    DDS_XTypes_EquivalenceHash hash;
    if ((ret = ddsi_typeobj_get_scc_hash (hash, type_objects, nslots)) == DDS_RETCODE_OK)
    {
      struct DDS_XTypes_TypeObjectHashId hash_id = { ._d = scc_id->sc_component_id._d };
      memcpy (hash_id._u.hash, hash, sizeof (hash_id._u.hash));
      if (!ddsi_typeobject_hashid_equal_impl (&hash_id, &scc_id->sc_component_id))
        ret = DDS_RETCODE_BAD_PARAMETER;
    }
  }

  for (uint32_t n = 0; n < nslots; n++)
    ddsi_typeobj_fini_impl (&type_objects[n]);
  ddsrt_free (type_objects);
  return ret;
}

static dds_return_t type_scc_verify_component_pairs (
    const dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *pairs,
    const struct DDS_XTypes_StronglyConnectedComponentId *scc_id,
    const struct DDS_XTypes_TypeIdentifierTypeObjectPair ***slots,
    bool *complete)
{
  dds_return_t ret = type_scc_collect_component_pairs (pairs, scc_id, slots, complete);
  if (ret != DDS_RETCODE_OK || !*complete)
    return ret;
  if ((ret = type_scc_verify_component_hash (scc_id, *slots)) == DDS_RETCODE_OK)
    ret = ddsi_typeobj_scc_verify_strongly_connected (scc_id, *slots);
  if (ret != DDS_RETCODE_OK)
  {
    ddsrt_free ((void *) *slots);
    *slots = NULL;
    *complete = false;
  }
  return ret;
}

static struct ddsi_type_scc *ddsi_type_scc_lookup_locked (struct ddsi_domaingv *gv, const struct DDS_XTypes_StronglyConnectedComponentId *id)
{
  ddsrt_avl_iter_t it;
  for (struct ddsi_type *type = ddsrt_avl_iter_first (&ddsi_typelib_treedef, &gv->typelib, &it);
       type != NULL; type = ddsrt_avl_iter_next (&it))
  {
    struct ddsi_type_scc *scc = type->scc;
    if (scc != NULL && scc->n_wire_types == (uint32_t) id->scc_length && ddsi_typeobject_hashid_equal_impl (&scc->sc_component_id, &id->sc_component_id))
      return scc;
  }
  return NULL;
}

static dds_return_t ddsi_type_scc_find_or_create_locked (struct ddsi_domaingv *gv, const struct DDS_XTypes_StronglyConnectedComponentId *id, struct ddsi_type_scc **scc)
{
  if ((*scc = ddsi_type_scc_lookup_locked (gv, id)) != NULL)
    return DDS_RETCODE_OK;

  if ((*scc = ddsrt_calloc_s (1, sizeof (**scc))) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  (*scc)->types = ddsrt_calloc_s ((size_t) id->scc_length, sizeof ((*scc)->types[0]));
  if ((*scc)->types == NULL)
  {
    ddsrt_free (*scc);
    *scc = NULL;
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  (*scc)->gv = gv;
  (*scc)->sc_component_id = id->sc_component_id;
  (*scc)->n_wire_types = (uint32_t) id->scc_length;
  (*scc)->n_types = (*scc)->n_wire_types;
  return DDS_RETCODE_OK;
}

dds_return_t ddsi_type_scc_attach_locked (struct ddsi_domaingv *gv, struct ddsi_type *type, const struct DDS_XTypes_StronglyConnectedComponentId *id)
{
  assert (type->scc == NULL);
  if (!ddsi_type_scc_id_is_valid_impl (id))
    return DDS_RETCODE_BAD_PARAMETER;

  struct ddsi_type_scc *scc;
  dds_return_t ret = ddsi_type_scc_find_or_create_locked (gv, id, &scc);
  if (ret != DDS_RETCODE_OK)
    return ret;

  const uint32_t index = (uint32_t) id->scc_index - 1;
  if (scc->types[index] != NULL && scc->types[index] != type)
    return DDS_RETCODE_BAD_PARAMETER;
  scc->types[index] = type;
  type->scc = scc;
  return DDS_RETCODE_OK;
}

#ifndef NDEBUG
static bool ddsi_type_scc_has_type (const struct ddsi_type_scc *scc, const struct ddsi_type *type)
{
  for (uint32_t n = 0; n < scc->n_types; n++)
  {
    if (scc->types[n] == type)
      return true;
  }
  return false;
}
#endif

static bool ddsi_type_scc_can_include_suffix_type (const struct ddsi_type *type)
{
  switch (type->xt.kind)
  {
    case DDSI_TYPEID_KIND_PLAIN_COLLECTION_MINIMAL:
    case DDSI_TYPEID_KIND_PLAIN_COLLECTION_COMPLETE:
      return true;
    case DDSI_TYPEID_KIND_MINIMAL:
    case DDSI_TYPEID_KIND_COMPLETE:
      return type->xt._d == DDS_XTypes_TK_ALIAS;
    case DDSI_TYPEID_KIND_FULLY_DESCRIPTIVE:
    case DDSI_TYPEID_KIND_INVALID:
      return false;
  }
  return false;
}

static dds_return_t ddsi_type_scc_append_suffix_type (struct ddsi_type_scc *scc, struct ddsi_type *type)
{
  assert (type->scc == NULL);
  assert (ddsi_type_scc_can_include_suffix_type (type));
  assert (!ddsi_type_scc_has_type (scc, type));

  if (scc->active)
    return DDS_RETCODE_BAD_PARAMETER;
  if (scc->n_types == UINT32_MAX)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  struct ddsi_type **types = ddsrt_realloc_s (scc->types, ((size_t) scc->n_types + 1) * sizeof (*scc->types));
  if (types == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  scc->types = types;
  scc->types[scc->n_types++] = type;
  type->scc = scc;
  return DDS_RETCODE_OK;
}

static bool ddsi_type_scc_maybe_append_suffix_type (struct ddsi_type_scc *scc, struct ddsi_type *type, dds_return_t *ret)
{
  assert (*ret == DDS_RETCODE_OK);
  if (type->scc == scc)
    return true;
  if (type->scc != NULL || !ddsi_type_scc_can_include_suffix_type (type))
    return false;

  *ret = ddsi_type_scc_append_suffix_type (scc, type);
  return *ret == DDS_RETCODE_OK;
}

static bool ddsi_type_scc_attach_path_to_scc_locked (struct ddsi_domaingv *gv, struct ddsi_type_scc *scc, struct ddsi_type *type, struct ddsrt_hh *visited, dds_return_t *ret)
{
  assert (*ret == DDS_RETCODE_OK);
  if (type->scc == scc)
    return true;
  if (type->scc != NULL || ddsi_type_visit_seen (visited, type))
    return false;

  struct ddsi_type_dep tmpl, *dep = &tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.src_type_id, &type->xt.id);
  while ((dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_treedef, &gv->typedeps, dep)) != NULL && !ddsi_typeid_compare (&type->xt.id, &dep->src_type_id))
  {
    struct ddsi_type *dep_type = ddsi_type_lookup_locked (gv, &dep->dep_type_id);
    if (dep_type != NULL && ddsi_type_scc_attach_path_to_scc_locked (gv, scc, dep_type, visited, ret))
    {
      ddsi_typeid_fini (&tmpl.src_type_id);
      return ddsi_type_scc_maybe_append_suffix_type (scc, type, ret);
    }
    if (*ret != DDS_RETCODE_OK)
      break;
  }
  ddsi_typeid_fini (&tmpl.src_type_id);
  return false;
}

static bool ddsi_type_scc_attach_path_to_type_locked (struct ddsi_domaingv *gv, struct ddsi_type_scc *scc, struct ddsi_type *type, struct ddsi_type *target, struct ddsrt_hh *visited, dds_return_t *ret)
{
  assert (*ret == DDS_RETCODE_OK);
  if (type == target)
    return ddsi_type_scc_maybe_append_suffix_type (scc, type, ret);
  if (type->scc != NULL && type->scc != scc)
    return false;
  if (ddsrt_avl_is_empty (&gv->typedeps) || ddsi_type_visit_seen (visited, type))
    return false;

  struct ddsi_type_dep tmpl, *dep = &tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.src_type_id, &type->xt.id);
  while ((dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_treedef, &gv->typedeps, dep)) != NULL && !ddsi_typeid_compare (&type->xt.id, &dep->src_type_id))
  {
    struct ddsi_type *dep_type = ddsi_type_lookup_locked (gv, &dep->dep_type_id);
    if (dep_type != NULL && ddsi_type_scc_attach_path_to_type_locked (gv, scc, dep_type, target, visited, ret))
    {
      ddsi_typeid_fini (&tmpl.src_type_id);
      return type->scc == scc || ddsi_type_scc_maybe_append_suffix_type (scc, type, ret);
    }
    if (*ret != DDS_RETCODE_OK)
      break;
  }
  ddsi_typeid_fini (&tmpl.src_type_id);
  return false;
}

static dds_return_t ddsi_type_scc_update_for_dep_locked (struct ddsi_domaingv *gv, struct ddsi_type *src_type, struct ddsi_type *dep_type)
{
  dds_return_t ret = DDS_RETCODE_OK;
  if (src_type == NULL || dep_type == NULL || src_type == dep_type)
    return ret;

  if (src_type->scc != NULL && dep_type->scc == NULL)
  {
    struct ddsrt_hh *visited = ddsi_type_visit_new ();
    (void) ddsi_type_scc_attach_path_to_scc_locked (gv, src_type->scc, dep_type, visited, &ret);
    ddsi_type_visit_free (visited);
  }
  else if (dep_type->scc != NULL && src_type->scc == NULL)
  {
    for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < dep_type->scc->n_types && src_type->scc == NULL; n++)
    {
      if (dep_type->scc->types[n] == NULL)
        continue;
      struct ddsrt_hh *visited = ddsi_type_visit_new ();
      (void) ddsi_type_scc_attach_path_to_type_locked (gv, dep_type->scc, dep_type->scc->types[n], src_type, visited, &ret);
      ddsi_type_visit_free (visited);
    }
  }
  return ret;
}

static bool ddsi_type_scc_is_empty (const struct ddsi_type_scc *scc)
{
  for (uint32_t n = 0; n < scc->n_types; n++)
  {
    if (scc->types[n] != NULL)
      return false;
  }
  return true;
}

static bool ddsi_type_scc_wire_complete (const struct ddsi_type_scc *scc)
{
  for (uint32_t n = 0; n < scc->n_wire_types; n++)
  {
    if (scc->types[n] == NULL)
      return false;
  }
  return true;
}

static bool ddsi_type_scc_materialized (const struct ddsi_type *type)
{
  return type != NULL && type->scc != NULL && ddsi_type_scc_wire_complete (type->scc);
}

static bool type_state_has_valid_typeobj (enum ddsi_type_state state);

#ifndef NDEBUG
static void assert_scc_materialized (
    struct ddsi_domaingv *gv,
    const struct ddsi_type *type,
    const struct DDS_XTypes_TypeIdentifier *type_id,
    const char *where)
{
  if (type_id->_d != DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
    return;

  const bool ok = ddsi_type_scc_materialized (type);
  if (!ok)
  {
    struct typelib_trace_typeid_str tistr;
    GVWARNING ("SCC type %s should be materialized after %s\n",
               typelib_trace_make_typeid_str (&tistr, type_id), where);
    assert (ok);
  }
}
#else
#define assert_scc_materialized(gv, type, type_id, where) ((void) 0)
#endif

static void ddsi_type_scc_detach (struct ddsi_type *type)
{
  struct ddsi_type_scc *scc = type->scc;
  if (scc == NULL)
    return;

  for (uint32_t n = 0; n < scc->n_types; n++)
  {
    if (scc->types[n] == type)
      scc->types[n] = NULL;
  }
  type->scc = NULL;
  if (!scc->active && ddsi_type_scc_is_empty (scc))
  {
    ddsrt_free (scc->types);
    ddsrt_free (scc);
  }
}

static bool ddsi_type_dep_is_active_scc_internal (const struct ddsi_type *owner, const struct ddsi_type *type)
{
  return owner != NULL && type != NULL && owner->scc != NULL && owner->scc == type->scc && owner->scc->active;
}

static uint32_t ddsi_type_scc_count_dep_ref (const struct ddsi_type_scc *scc, const struct ddsi_type *type)
{
  return type != NULL && type->scc == scc ? 1u : 0u;
}

static uint32_t ddsi_type_scc_count_xt_refs (const struct ddsi_type_scc *scc, const struct xt_type *xt)
{
  uint32_t nrefs = 0;
  switch (xt->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      nrefs += ddsi_type_scc_count_dep_ref (scc, xt->_u.alias.related_type);
      break;
    case DDS_XTypes_TK_STRUCTURE:
      nrefs += ddsi_type_scc_count_dep_ref (scc, xt->_u.structure.base_type);
      for (uint32_t n = 0; n < xt->_u.structure.members.length; n++)
        nrefs += ddsi_type_scc_count_dep_ref (scc, xt->_u.structure.members.seq[n].type);
      break;
    case DDS_XTypes_TK_UNION:
      nrefs += ddsi_type_scc_count_dep_ref (scc, xt->_u.union_type.disc_type);
      for (uint32_t n = 0; n < xt->_u.union_type.members.length; n++)
        nrefs += ddsi_type_scc_count_dep_ref (scc, xt->_u.union_type.members.seq[n].type);
      break;
    case DDS_XTypes_TK_SEQUENCE:
      nrefs += ddsi_type_scc_count_dep_ref (scc, xt->_u.seq.c.element_type);
      break;
    case DDS_XTypes_TK_ARRAY:
      nrefs += ddsi_type_scc_count_dep_ref (scc, xt->_u.array.c.element_type);
      break;
    case DDS_XTypes_TK_MAP:
      nrefs += ddsi_type_scc_count_dep_ref (scc, xt->_u.map.c.element_type);
      nrefs += ddsi_type_scc_count_dep_ref (scc, xt->_u.map.key_type);
      break;
    default:
      break;
  }
  return nrefs;
}

static uint32_t ddsi_type_scc_count_typeinfo_refs_locked (struct ddsi_domaingv *gv, const struct ddsi_type_scc *scc, const struct ddsi_type *type)
{
  uint32_t nrefs = 0;
  struct ddsi_type_dep tmpl, *dep = &tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.src_type_id, &type->xt.id);
  while ((dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_treedef, &gv->typedeps, dep)) != NULL && !ddsi_typeid_compare (&type->xt.id, &dep->src_type_id))
  {
    if (dep->from_type_info)
    {
      struct ddsi_type *dep_type = ddsi_type_lookup_locked (gv, &dep->dep_type_id);
      nrefs += ddsi_type_scc_count_dep_ref (scc, dep_type);
    }
  }
  ddsi_typeid_fini (&tmpl.src_type_id);
  return nrefs;
}

static bool ddsi_type_scc_ready_to_activate (const struct ddsi_type_scc *scc)
{
  assert (!scc->active);
  /* Activation only changes ownership of references inside the component. It
     can run before dependencies outside the SCC have been resolved. */
  for (uint32_t n = 0; n < scc->n_types; n++)
  {
    if (scc->types[n] == NULL || !type_state_has_valid_typeobj (scc->types[n]->state))
      return false;
  }
  return true;
}

static void ddsi_type_scc_try_activate_locked (struct ddsi_domaingv *gv, struct ddsi_type_scc *scc)
{
  if (scc == NULL || scc->active || !ddsi_type_scc_ready_to_activate (scc))
    return;

  /* Before activation, dependency edges inside the SCC were ordinary type
     references.  Convert the accumulated per-type counts to one component
     count by subtracting those internal references. */
  uint32_t refc = 0, internal_refs = 0;
  for (uint32_t n = 0; n < scc->n_types; n++)
  {
    refc += scc->types[n]->refc;
    internal_refs += ddsi_type_scc_count_xt_refs (scc, &scc->types[n]->xt);
    internal_refs += ddsi_type_scc_count_typeinfo_refs_locked (gv, scc, scc->types[n]);
  }
  if (refc < internal_refs)
    return;

  scc->refc = refc - internal_refs;
  scc->active = true;
  for (uint32_t n = 0; n < scc->n_types; n++)
    scc->types[n]->refc = 0;
}

static void ddsi_type_fini (struct ddsi_type *type, bool detach_scc)
{
  struct ddsi_domaingv *gv = type->gv;
  struct ddsi_type_dep key;
  memset (&key, 0, sizeof (key));
  ddsi_typeid_copy (&key.src_type_id, &type->xt.id);
  ddsi_xt_type_fini_owned (gv, type, &type->xt, true);

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
      if (!ddsi_type_dep_is_active_scc_internal (type, dep_type))
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
  if (type->replacement != NULL)
  {
    struct ddsi_type *replacement = type->replacement;
    type->replacement = NULL;
    ddsi_type_unref_locked (gv, replacement);
  }
  if (detach_scc)
    ddsi_type_scc_detach (type);
}

void ddsi_type_free (struct ddsi_type *type)
{
  ddsi_type_fini (type, true);
  ddsrt_free (type);
}

void ddsi_type_replace_locked (struct ddsi_domaingv *gv, struct ddsi_type *type, const struct ddsi_type *replacement)
{
  assert (type);
  assert (replacement);
  assert (type != replacement);
  assert (type->gv == gv);
  assert (replacement->gv == gv);
  assert (type->replacement == NULL);
  assert (type->state == DDSI_TYPE_CONSTRUCTING || type->state == DDSI_TYPE_COMPLETING);
  assert (replacement->state != DDSI_TYPE_REPLACED);
  /* Dynamic construction can discover that an equivalent type already exists while
     other dynamic handles still reference the construction record. Keep that record
     alive as a forwarding node until those references are released or canonicalized. */
  ddsi_type_ref_locked (gv, &type->replacement, replacement);
  type->state = DDSI_TYPE_REPLACED;
}

void ddsi_type_canonicalize_locked (struct ddsi_domaingv *gv, struct ddsi_type **type)
{
  assert (type);
  while (*type != NULL && (*type)->state == DDSI_TYPE_REPLACED)
  {
    struct ddsi_type *old = *type;
    assert (old->gv == gv);
    assert (old->replacement != NULL);
    assert (old->replacement->gv == gv);
    ddsi_type_ref_locked (gv, type, old->replacement);
    ddsi_type_unref_locked (gv, old);
  }
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

static bool type_state_has_valid_typeobj (enum ddsi_type_state state)
{
  return state == DDSI_TYPE_PARTIAL_RESOLVED || state == DDSI_TYPE_RESOLVED;
}

static bool type_deps_resolved (struct ddsi_domaingv *gv, const struct ddsi_type *type, struct ddsrt_hh *visited)
{
  assert (ddsi_xt_has_definition (&type->xt));
  if (ddsi_type_visit_seen (visited, type))
    return true;

  struct ddsi_type_dep tmpl, *dep = &tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.src_type_id, &type->xt.id);
  while ((dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_treedef, &gv->typedeps, dep)) && !ddsi_typeid_compare (&type->xt.id, &dep->src_type_id))
  {
    const struct ddsi_type *dep_type = ddsi_type_lookup_locked (gv, &dep->dep_type_id);
    if (dep_type == NULL || dep_type->state == DDSI_TYPE_INVALID || dep_type->state == DDSI_TYPE_CONSTRUCTING ||
        dep_type->state == DDSI_TYPE_COMPLETING || dep_type->state == DDSI_TYPE_REPLACED || ddsi_xt_missing_definition (&dep_type->xt) ||
        (dep_type->state != DDSI_TYPE_RESOLVED && !type_deps_resolved (gv, dep_type, visited)))
    {
      ddsi_typeid_fini (&tmpl.src_type_id);
      return false;
    }
  }
  ddsi_typeid_fini (&tmpl.src_type_id);
  return true;
}

static enum ddsi_type_state type_resolved_state (struct ddsi_domaingv *gv, const struct ddsi_type *type)
{
  assert (ddsi_xt_has_definition (&type->xt));
  if (type->xt.id.x._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
  {
    if (type->scc == NULL)
    {
#ifndef NDEBUG
      struct typelib_trace_typeid_str tistr;
      GVWARNING ("SCC type %s has no runtime SCC\n", typelib_trace_make_typeid_str (&tistr, &type->xt.id.x));
      assert (type->scc != NULL);
#endif
      return DDSI_TYPE_PARTIAL_RESOLVED;
    }
  }
  if (type->scc != NULL && !ddsi_type_scc_wire_complete (type->scc))
    return DDSI_TYPE_PARTIAL_RESOLVED;

  struct ddsrt_hh *visited = ddsi_type_visit_new ();
  const bool resolved = type_deps_resolved (gv, type, visited);
  ddsi_type_visit_free (visited);
  return resolved ? DDSI_TYPE_RESOLVED : DDSI_TYPE_PARTIAL_RESOLVED;
}

ddsrt_nonnull_all
static void ddsi_type_update_state_locked_impl (struct ddsi_domaingv *gv, struct ddsi_type *type, struct ddsrt_hh *visited)
{
  if (ddsi_type_visit_seen (visited, type) || type->state == DDSI_TYPE_INVALID || type->state == DDSI_TYPE_CONSTRUCTING ||
      type->state == DDSI_TYPE_COMPLETING || type->state == DDSI_TYPE_REPLACED)
    return;

  enum ddsi_type_state state = type->state;
  if (ddsi_xt_missing_definition (&type->xt))
  {
    if (state != DDSI_TYPE_REQUESTED)
      state = DDSI_TYPE_UNRESOLVED;
  }
  else
    state = type_resolved_state (gv, type);
  if (state == type->state)
    return;
  type->state = state;

  /* PARTIAL_RESOLVED has a valid type object but may still be waiting for dependencies.
     Wake all waiters: those ignoring dependencies may proceed, stricter waiters will recheck. */
  if (type_state_has_valid_typeobj (state))
  {
    ddsi_type_scc_try_activate_locked (gv, type->scc);
    ddsrt_cond_etime_broadcast (&gv->typelib_resolved_cond);
  }

  struct ddsi_type_dep tmpl, *reverse_dep = &tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.dep_type_id, &type->xt.id);
  while ((reverse_dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_reverse_treedef, &gv->typedeps_reverse, reverse_dep)) && !ddsi_typeid_compare (&type->xt.id, &reverse_dep->dep_type_id))
  {
    struct ddsi_type *dep_src_type = ddsi_type_lookup_locked (gv, &reverse_dep->src_type_id);
    /* A dependency edge can be registered while its source type is still being
       constructed and has not yet been inserted in the typelib. The source is
       updated explicitly once construction completes. */
    if (dep_src_type != NULL)
      ddsi_type_update_state_locked_impl (gv, dep_src_type, visited);
  }
  ddsi_typeid_fini (&tmpl.dep_type_id);
}

ddsrt_nonnull_all
void ddsi_type_update_state_locked (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  struct ddsrt_hh *visited = ddsi_type_visit_new ();
  ddsi_type_update_state_locked_impl (gv, type, visited);
  ddsi_type_visit_free (visited);
}

static dds_return_t ddsi_type_new_impl (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct DDS_XTypes_TypeIdentifier *type_id, const struct DDS_XTypes_TypeObject *type_obj, bool scc_component_verified)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull((1,2,3));

static bool type_id_matches_type_obj_id (const struct DDS_XTypes_TypeIdentifier *type_id, const ddsi_typeid_t *type_obj_id)
{
  if (!ddsi_typeid_compare_impl (&type_obj_id->x, type_id))
    return true;

  if (type_id->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
  {
    /* An SCC TypeIdentifier identifies the component, not the individual
       TypeObject. The caller must verify the component hash before allowing this
       minimal/complete-kind-only match to install a TypeObject. */
    return type_id->_u.sc_component_id.sc_component_id._d == type_obj_id->x._d;
  }

  return false;
}

static dds_return_t ddsi_type_new_impl (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct DDS_XTypes_TypeIdentifier *type_id, const struct DDS_XTypes_TypeObject *type_obj, bool scc_component_verified)
{
  dds_return_t ret;
  assert (type);
  assert (!ddsi_typeid_is_none_impl (type_id));
  assert (!ddsi_type_lookup_locked_impl (gv, type_id));

  if (!gv->config.allow_recursive_types && ddsi_typeid_contains_scc_impl (type_id))
  {
    *type = NULL;
    return DDS_RETCODE_UNSUPPORTED;
  }

  if (type_obj != NULL && type_id->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT && !scc_component_verified)
  {
    TYPELIB_TRACE ("SCC type object for %s without component verification\n",
                   typelib_trace_make_typeid_str (&tistr, type_id));
    *type = NULL;
    return DDS_RETCODE_BAD_PARAMETER;
  }

  ddsi_typeid_t type_obj_id;
  memset (&type_obj_id, 0, sizeof (type_obj_id));
  if (type_obj && ((ret = ddsi_typeobj_get_hash_id (type_obj, &type_obj_id))
      || (ret = (type_id_matches_type_obj_id (type_id, &type_obj_id) ? DDS_RETCODE_OK : DDS_RETCODE_BAD_PARAMETER))))
  {
    struct typelib_trace_typeid_str type_id_str, type_obj_id_str;
    GVWARNING ("non-matching type identifier (%s) and type object (%s)\n",
               typelib_trace_make_typeid_str (&type_id_str, type_id),
               typelib_trace_make_typeid_str (&type_obj_id_str, &type_obj_id.x));
    *type = NULL;
    return ret;
  }

  if ((*type = ddsrt_calloc (1, sizeof (**type))) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  (*type)->gv = gv;
  (*type)->state = DDSI_TYPE_CONSTRUCTING;
  /* Recursive TypeObjects can refer back to the type currently being
     initialized. Insert it before loading dependencies so self-references find
     this object instead of creating an unresolved duplicate. The shallow id copy
     is only the AVL key until ddsi_xt_type_init_impl deep-copies it. */
  (*type)->xt.id.x = *type_id;
  if (type_id->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT &&
      (ret = ddsi_type_scc_attach_locked (gv, *type, &type_id->_u.sc_component_id)) != DDS_RETCODE_OK)
  {
    ddsi_type_free (*type);
    *type = NULL;
    return ret;
  }
  ddsrt_avl_insert (&ddsi_typelib_treedef, &gv->typelib, *type);

  TYPELIB_TRACE (" new %p", *type);
  if ((ret = ddsi_xt_type_init_impl (gv, *type, type_id, type_obj)) != DDS_RETCODE_OK)
  {
    ddsrt_avl_delete (&ddsi_typelib_treedef, &gv->typelib, *type);
    ddsi_type_free (*type);
    *type = NULL;
    return ret;
  }
  /* inserted with refc 0 (set by calloc), refc is increased in
     ddsi_type_ref_* functions */
  (*type)->state = DDSI_TYPE_UNRESOLVED;
  ddsi_type_update_state_locked (gv, *type);
  return DDS_RETCODE_OK;
}

static dds_return_t ddsi_type_new (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct DDS_XTypes_TypeIdentifier *type_id, const struct DDS_XTypes_TypeObject *type_obj)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull((1,2,3));

static dds_return_t ddsi_type_new (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct DDS_XTypes_TypeIdentifier *type_id, const struct DDS_XTypes_TypeObject *type_obj)
{
  return ddsi_type_new_impl (gv, type, type_id, type_obj, false);
}

static void set_type_invalid_impl (struct ddsi_domaingv *gv, struct ddsi_type *type, struct ddsrt_hh *visited)
{
  if (ddsi_type_visit_seen (visited, type))
    return;

  type->state = DDSI_TYPE_INVALID;

  struct ddsi_type_dep tmpl, *reverse_dep = &tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.dep_type_id, &type->xt.id);
  while ((reverse_dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_reverse_treedef, &gv->typedeps_reverse, reverse_dep)) && !ddsi_typeid_compare (&type->xt.id, &reverse_dep->dep_type_id))
  {
    struct ddsi_type *dep_src_type = ddsi_type_lookup_locked (gv, &reverse_dep->src_type_id);
    if (dep_src_type != NULL)
      set_type_invalid_impl (gv, dep_src_type, visited);
  }
  ddsi_typeid_fini (&tmpl.dep_type_id);
}

static void set_type_invalid (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  struct ddsrt_hh *visited = ddsi_type_visit_new ();
  set_type_invalid_impl (gv, type, visited);
  ddsi_type_visit_free (visited);
}

static dds_return_t validate_type (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  const dds_return_t ret = ddsi_xt_validate (gv, type);
  if (ret != DDS_RETCODE_OK)
    set_type_invalid (gv, type);
  return ret;
}

static void ddsi_type_remove_typeobj_deps_locked (struct ddsi_domaingv *gv, const struct ddsi_type *type);

static dds_return_t ddsi_type_add_typeobj_impl_common (
    struct ddsi_domaingv *gv,
    struct ddsi_type *type,
    const struct DDS_XTypes_TypeObject *type_obj,
    bool scc_component_verified,
    bool invalidate_on_failure)
{
  if (type_state_has_valid_typeobj (type->state))
    return DDS_RETCODE_OK;

  if (type->xt.id.x._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT && !scc_component_verified)
  {
    TYPELIB_TRACE ("SCC type object for %s without component verification\n",
                   typelib_trace_make_typeid_str (&tistr, &type->xt.id.x));
    return DDS_RETCODE_BAD_PARAMETER;
  }

  dds_return_t ret;
  ddsi_typeid_t type_id;
  ret = ddsi_typeobj_get_hash_id (type_obj, &type_id);
  if (ret != DDS_RETCODE_OK)
  {
    /* In case the object does not match the type id, reset the type's state to
       unresolved so that it can de resolved in case the correct type object
       is received */
    type->state = DDSI_TYPE_UNRESOLVED;
    return ret;
  }

  if (!type_id_matches_type_obj_id (&type->xt.id.x, &type_id))
  {
    /* In case the object does not match the type id, reset the type's state to
       unresolved so that it can de resolved in case the correct type object
       is received */
    type->state = DDSI_TYPE_UNRESOLVED;
    return DDS_RETCODE_BAD_PARAMETER;
  }

  const enum ddsi_type_state prev_state = type->state;
  const bool typeobj_pending = prev_state == DDSI_TYPE_UNRESOLVED || prev_state == DDSI_TYPE_REQUESTED;
  /* Installing a TypeObject can recursively validate dependencies that refer
     back to this type. Mark it as in-progress so those nested validations don't
     inspect members that are still being filled in. */
  if (typeobj_pending)
    type->state = DDSI_TYPE_COMPLETING;

  ret = ddsi_xt_type_add_typeobj (gv, type, type_obj);
  if (ret != DDS_RETCODE_OK)
  {
    if (typeobj_pending && type->state == DDSI_TYPE_COMPLETING)
      type->state = prev_state;
    ddsi_type_remove_typeobj_deps_locked (gv, type);
    if (invalidate_on_failure)
    {
      /* Mark this type and all types that (indirectly) depend on this type
         invalid, because at this point we know that the type object that matches
         the type id for this type is invalid (except in case of a hash collision
         and a different valid type object exists with the same id) */
      set_type_invalid (gv, type);
    }
    return ret;
  }

  if (typeobj_pending && type->state == DDSI_TYPE_COMPLETING)
    type->state = prev_state;
  ddsi_type_update_state_locked (gv, type);
  return DDS_RETCODE_OK;
}

static dds_return_t ddsi_type_add_typeobj_impl (struct ddsi_domaingv *gv, struct ddsi_type *type, const struct DDS_XTypes_TypeObject *type_obj, bool scc_component_verified)
{
  return ddsi_type_add_typeobj_impl_common (gv, type, type_obj, scc_component_verified, true);
}

dds_return_t ddsi_type_add_typeobj (struct ddsi_domaingv *gv, struct ddsi_type *type, const struct DDS_XTypes_TypeObject *type_obj)
{
  return ddsi_type_add_typeobj_impl (gv, type, type_obj, false);
}

struct type_scc_import_slot {
  struct ddsi_type *type;
  enum ddsi_type_state state;
  bool touched;
  bool created;
};

static bool type_scc_slot_has_valid_typeobj (
    const struct ddsi_type *type,
    const struct DDS_XTypes_TypeIdentifier *slot_type_id)
{
  return type != NULL &&
    type->xt.id.x._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT &&
    !ddsi_typeid_compare_impl (&type->xt.id.x, slot_type_id) &&
    type->scc != NULL &&
    ddsi_type_scc_materialized (type) &&
    type_state_has_valid_typeobj (type->state);
}

static dds_return_t type_scc_prepare_import_locked (
    struct ddsi_domaingv *gv,
    const struct DDS_XTypes_TypeIdentifierTypeObjectPair * const *slots,
    uint32_t nslots,
    struct type_scc_import_slot *import_slots,
    bool *already_installed)
{
  bool any_valid = false;
  bool all_valid = true;

  for (uint32_t n = 0; n < nslots; n++)
  {
    const struct DDS_XTypes_TypeIdentifier *slot_type_id = &slots[n]->type_identifier;
    struct ddsi_type *slot_type = ddsi_type_lookup_locked_impl (gv, slot_type_id);
    import_slots[n] = (struct type_scc_import_slot) {
      .type = slot_type,
      .state = slot_type ? slot_type->state : DDSI_TYPE_UNRESOLVED
    };

    if (slot_type == NULL)
    {
      all_valid = false;
      continue;
    }

    if (slot_type->xt.id.x._d != DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT ||
        ddsi_typeid_compare_impl (&slot_type->xt.id.x, slot_type_id) ||
        slot_type->scc == NULL ||
        !ddsi_type_scc_id_same_component_impl (&slot_type->xt.id.x._u.sc_component_id, &slot_type_id->_u.sc_component_id))
      return DDS_RETCODE_BAD_PARAMETER;

    if (type_state_has_valid_typeobj (slot_type->state))
    {
      if (!type_scc_slot_has_valid_typeobj (slot_type, slot_type_id))
        return DDS_RETCODE_BAD_PARAMETER;
      any_valid = true;
    }
    else
    {
      all_valid = false;
      if (slot_type->state != DDSI_TYPE_UNRESOLVED && slot_type->state != DDSI_TYPE_REQUESTED)
        return DDS_RETCODE_BAD_PARAMETER;
    }
  }

  /* A slot with a valid TypeObject is acceptable only when the whole verified
     component is already materialized.  Mixing valid slots with missing or
     unresolved slots would mean this import is trying to merge into a partial
     SCC, but the verified component is supposed to be exactly these slots. */
  if (any_valid && !all_valid)
    return DDS_RETCODE_BAD_PARAMETER;

  *already_installed = all_valid;
  return DDS_RETCODE_OK;
}

static void ddsi_type_remove_typeobj_deps_locked (struct ddsi_domaingv *gv, const struct ddsi_type *type)
{
  struct ddsi_type_dep cursor;
  memset (&cursor, 0, sizeof (cursor));
  ddsi_typeid_copy (&cursor.src_type_id, &type->xt.id);

  struct ddsi_type_dep *dep = &cursor;
  while ((dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_treedef, &gv->typedeps, dep)) != NULL &&
         !ddsi_typeid_compare (&type->xt.id, &dep->src_type_id))
  {
    /* TypeInformation dependencies can predate the TypeObject install attempt;
       rollback only removes dependency records owned by this TypeObject. */
    if (dep->from_type_info)
      continue;

    ddsi_typeid_fini (&cursor.src_type_id);
    ddsi_typeid_fini (&cursor.dep_type_id);
    ddsi_typeid_copy (&cursor.src_type_id, &dep->src_type_id);
    ddsi_typeid_copy (&cursor.dep_type_id, &dep->dep_type_id);

    type_dep_trace (gv, "rollback ", dep);
    ddsrt_avl_delete (&ddsi_typedeps_treedef, &gv->typedeps, dep);
    ddsrt_avl_delete (&ddsi_typedeps_reverse_treedef, &gv->typedeps_reverse, dep);
    ddsi_typeid_fini (&dep->src_type_id);
    ddsi_typeid_fini (&dep->dep_type_id);
    ddsrt_free (dep);
    dep = &cursor;
  }

  ddsi_typeid_fini (&cursor.src_type_id);
  ddsi_typeid_fini (&cursor.dep_type_id);
}

static void type_scc_import_rollback_locked (
    struct ddsi_domaingv *gv,
    struct type_scc_import_slot *import_slots,
    uint32_t nslots)
{
  /* Slot TypeObjects can reference one another.  Hold temporary references so
     finalizing one slot cannot free another slot still scheduled for cleanup. */
  for (uint32_t n = 0; n < nslots; n++)
  {
    if (import_slots[n].touched && import_slots[n].type != NULL)
      ddsi_type_ref_locked (gv, NULL, import_slots[n].type);
  }

  for (uint32_t n = 0; n < nslots; n++)
  {
    struct ddsi_type *type = import_slots[n].type;
    if (!import_slots[n].touched || type == NULL)
      continue;

    assert (type->scc == NULL || !type->scc->active);
    ddsi_type_remove_typeobj_deps_locked (gv, type);
    ddsi_xt_type_fini_owned (gv, type, &type->xt, false);
    if (!import_slots[n].created)
    {
      type->state = DDSI_TYPE_PARTIAL_RESOLVED;
      ddsi_type_update_state_locked (gv, type);
      type->state = import_slots[n].state;
    }
  }

  for (uint32_t n = 0; n < nslots; n++)
  {
    struct ddsi_type *type = import_slots[n].type;
    if (!import_slots[n].touched || !import_slots[n].created || type == NULL)
      continue;

    if (!ddsi_typeid_is_none (&type->xt.id) && ddsi_type_lookup_locked (gv, &type->xt.id) == type)
      ddsrt_avl_delete (&ddsi_typelib_treedef, &gv->typelib, type);
    ddsi_type_scc_detach (type);
  }

  for (uint32_t n = 0; n < nslots; n++)
  {
    if (import_slots[n].touched && import_slots[n].type != NULL)
      ddsi_type_unref_locked (gv, import_slots[n].type);
  }
}

dds_return_t ddsi_type_add_scc_typeobjs_locked (
    struct ddsi_domaingv *gv,
    const dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *pairs,
    const struct DDS_XTypes_TypeIdentifier *type_id,
    bool require_complete,
    bool *complete)
{
  assert (type_id->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  if (!gv->config.allow_recursive_types)
    return DDS_RETCODE_UNSUPPORTED;
  const struct DDS_XTypes_TypeIdentifierTypeObjectPair **slots;
  dds_return_t ret = type_scc_verify_component_pairs (pairs, &type_id->_u.sc_component_id, &slots, complete);
  TYPELIB_IMPORT_TRACE ("SCC import verify %s pairs=%"PRIu32" require-complete=%d ret=%d complete=%d\n",
                        typelib_trace_make_typeid_str (&tistr, type_id), pairs->_length,
                        require_complete, ret, *complete);
  if (ret != DDS_RETCODE_OK || !*complete)
    return (ret != DDS_RETCODE_OK || !require_complete) ? ret : DDS_RETCODE_BAD_PARAMETER;

  const uint32_t nslots = (uint32_t) type_id->_u.sc_component_id.scc_length;
  struct type_scc_import_slot import_slots[DDSI_TYPE_SCC_MAX_WIRE_TYPES] = { 0 };
  bool already_installed = false;
  ret = type_scc_prepare_import_locked (gv, slots, nslots, import_slots, &already_installed);
  if (ret != DDS_RETCODE_OK || already_installed)
  {
    ddsrt_free (slots);
    return ret;
  }

  for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < nslots; n++)
  {
    const struct DDS_XTypes_TypeIdentifier *slot_type_id = &slots[n]->type_identifier;
    struct ddsi_type *slot_type = import_slots[n].type;
    TYPELIB_IMPORT_TRACE ("SCC import slot %"PRIu32"/%"PRIu32" %s existing=%p state=%s scc=%p materialized=%d\n",
                          n + 1, nslots, typelib_trace_make_typeid_str (&tistr, slot_type_id),
                          (void *) slot_type, slot_type ? type_state_str (slot_type->state) : "(new)",
                          slot_type ? (void *) slot_type->scc : NULL, ddsi_type_scc_materialized (slot_type));
    if (slot_type == NULL)
    {
      ret = ddsi_type_new_impl (gv, &slot_type, slot_type_id, &slots[n]->type_object, true);
      if (ret == DDS_RETCODE_OK)
      {
        import_slots[n].type = slot_type;
        import_slots[n].touched = true;
        import_slots[n].created = true;
      }
    }
    else
    {
      import_slots[n].touched = true;
      ret = ddsi_type_add_typeobj_impl_common (gv, slot_type, &slots[n]->type_object, true, false);
    }
    TYPELIB_IMPORT_TRACE ("SCC import slot %"PRIu32"/%"PRIu32" %s ret=%d state=%s scc=%p materialized=%d\n",
                          n + 1, nslots, typelib_trace_make_typeid_str (&tistr, slot_type_id), ret,
                          slot_type ? type_state_str (slot_type->state) : "(none)",
                          slot_type ? (void *) slot_type->scc : NULL, ddsi_type_scc_materialized (slot_type));
  }
  if (ret != DDS_RETCODE_OK)
    type_scc_import_rollback_locked (gv, import_slots, nslots);
  ddsrt_free (slots);
  return ret;
}

static dds_return_t ddsi_type_register_dep_impl (struct ddsi_domaingv *gv, const ddsi_typeid_t *src_type_id, struct ddsi_type **dst_dep_type, const struct DDS_XTypes_TypeIdentifier *dep_tid, bool from_type_info)
  ddsrt_nonnull((1,2,3,4));

static dds_return_t ddsi_type_register_dep_impl (struct ddsi_domaingv *gv, const ddsi_typeid_t *src_type_id, struct ddsi_type **dst_dep_type, const struct DDS_XTypes_TypeIdentifier *dep_tid, bool from_type_info)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct ddsi_typeid dep_type_id;

  if (ddsi_typeid_is_none_impl (dep_tid))
    return DDS_RETCODE_BAD_PARAMETER;
  if (!gv->config.allow_recursive_types && ddsi_typeid_contains_scc_impl (dep_tid))
    return DDS_RETCODE_UNSUPPORTED;

  dep_type_id.x = *dep_tid;
  struct ddsi_type_dep *dep = ddsrt_calloc (1, sizeof (*dep));
  ddsi_typeid_copy (&dep->src_type_id, src_type_id);
  ddsi_typeid_copy (&dep->dep_type_id, &dep_type_id);
  bool existing = ddsrt_avl_lookup (&ddsi_typedeps_treedef, &gv->typedeps, dep) != NULL;
  bool dep_ref_acquired = false;
  type_dep_trace (gv, existing ? "has " : "add ", dep);
  if (!existing)
  {
    dep->from_type_info = from_type_info;
    ddsrt_avl_insert (&ddsi_typedeps_treedef, &gv->typedeps, dep);
    ddsrt_avl_insert (&ddsi_typedeps_reverse_treedef, &gv->typedeps_reverse, dep);
    if ((ret = ddsi_type_ref_id_locked (gv, dst_dep_type, &dep_type_id)) == DDS_RETCODE_OK)
      dep_ref_acquired = true;
    else
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
    {
      if ((ret = ddsi_type_ref_id_locked (gv, dst_dep_type, &dep_type_id)) == DDS_RETCODE_OK)
        dep_ref_acquired = true;
    }
    else
    {
      *dst_dep_type = ddsi_type_lookup_locked (gv, &dep_type_id);
      if (*dst_dep_type == NULL)
        ret = DDS_RETCODE_ERROR;
    }
  }
  if (ret == DDS_RETCODE_OK)
  {
    assert (*dst_dep_type != NULL);
    if ((*dst_dep_type)->state == DDSI_TYPE_INVALID)
    {
      struct ddsi_type *src_type = ddsi_type_lookup_locked (gv, src_type_id);
      if (src_type != NULL)
        set_type_invalid (gv, src_type);
      /* The registration failed after possibly adding a new edge and taking a
         reference for the destination field. Roll those back because callers
         only clean up dependencies for successful registrations. */
      if (!existing && dep != NULL)
      {
        ddsrt_avl_delete (&ddsi_typedeps_treedef, &gv->typedeps, dep);
        ddsrt_avl_delete (&ddsi_typedeps_reverse_treedef, &gv->typedeps_reverse, dep);
        ddsi_typeid_fini (&dep->src_type_id);
        ddsi_typeid_fini (&dep->dep_type_id);
        ddsrt_free (dep);
      }
      if (dep_ref_acquired)
        ddsi_type_unref_locked (gv, *dst_dep_type);
      *dst_dep_type = NULL;
      ret = DDS_RETCODE_BAD_PARAMETER;
    }
    else
    {
      struct ddsi_type *src_type = ddsi_type_lookup_locked (gv, src_type_id);
      ret = ddsi_type_scc_update_for_dep_locked (gv, src_type, *dst_dep_type);
      if (ret != DDS_RETCODE_OK)
      {
        if (!existing && dep != NULL)
        {
          ddsrt_avl_delete (&ddsi_typedeps_treedef, &gv->typedeps, dep);
          ddsrt_avl_delete (&ddsi_typedeps_reverse_treedef, &gv->typedeps_reverse, dep);
          ddsi_typeid_fini (&dep->src_type_id);
          ddsi_typeid_fini (&dep->dep_type_id);
          ddsrt_free (dep);
        }
        if (dep_ref_acquired)
          ddsi_type_unref_locked (gv, *dst_dep_type);
        *dst_dep_type = NULL;
      }
      else
      {
        const bool was_internal_active_scc_ref = ddsi_type_dep_is_active_scc_internal (src_type, *dst_dep_type);
        if (src_type != NULL)
          ddsi_type_scc_try_activate_locked (gv, src_type->scc);
        ddsi_type_scc_try_activate_locked (gv, (*dst_dep_type)->scc);
        if (dep_ref_acquired && was_internal_active_scc_ref)
        {
          ddsi_type_unref_locked (gv, *dst_dep_type);
          if (!existing && dep != NULL)
            dep->from_type_info = false;
        }
      }
    }
  }
  return ret;
}

dds_return_t ddsi_type_register_dep (struct ddsi_domaingv *gv, const ddsi_typeid_t *src_type_id, struct ddsi_type **dst_dep_type, const struct DDS_XTypes_TypeIdentifier *dep_tid)
{
  return ddsi_type_register_dep_impl (gv, src_type_id, dst_dep_type, dep_tid, false);
}

static void type_unref_many_locked (struct ddsi_domaingv *gv, struct ddsi_type **types, uint32_t ntypes)
{
  if (types == NULL)
    return;
  for (uint32_t n = 0; n < ntypes; n++)
    ddsi_type_unref_locked (gv, types[n]);
  ddsrt_free (types);
}

static dds_return_t type_ref_scc_pairseq_locked (
    struct ddsi_domaingv *gv,
    const dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *pairs,
    struct ddsi_type ***types,
    uint32_t *ntypes)
{
  dds_return_t ret = DDS_RETCODE_OK;
  *types = NULL;
  *ntypes = 0;
  if (pairs->_length == 0)
    return ret;

  struct ddsi_type **refs = ddsrt_calloc (pairs->_length, sizeof (*refs));
  if (refs == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  for (uint32_t n = 0; n < pairs->_length && ret == DDS_RETCODE_OK; n++)
  {
    struct ddsi_type *type = NULL;
    ret = ddsi_type_ref_id_locked_impl (gv, &type, &pairs->_buffer[n].type_identifier);
    if (ret == DDS_RETCODE_OK && type != NULL)
      refs[(*ntypes)++] = type;
  }
  if (ret != DDS_RETCODE_OK)
  {
    type_unref_many_locked (gv, refs, *ntypes);
    *ntypes = 0;
    return ret;
  }

  *types = refs;
  return ret;
}

static dds_return_t type_add_dep (
    struct ddsi_domaingv *gv,
    struct ddsi_type *type,
    const ddsi_typemap_t *type_map,
    const struct DDS_XTypes_TypeIdentifier *dep_type_id,
    uint32_t *n_match_upd,
    struct ddsi_generic_proxy_endpoint ***gpe_match_upd)
{
  dds_return_t ret;

  if (!ddsi_typeid_compare_impl (&type->xt.id.x, dep_type_id))
  {
    TYPELIB_IMPORT_TRACE ("skip self dependency %s\n", typelib_trace_make_typeid_str (&tistr, dep_type_id));
    return DDS_RETCODE_OK;
  }

  struct ddsi_type *dst_dep_type = NULL;
  if ((ret = ddsi_type_register_dep_impl (gv, &type->xt.id, &dst_dep_type, dep_type_id, true)) != DDS_RETCODE_OK)
  {
    /* If an error occurs, stop registering deps. The type will be finalized later, that
       will clean up the dependencies that are already added at this point */
    set_type_invalid (gv, type);
    return ret;
  }
  assert (dst_dep_type);
  {
    struct typelib_trace_typeid_str depstr;
    TYPELIB_IMPORT_TRACE ("registered dep %s -> %s dst=%p state=%s scc=%p materialized=%d typemap=%d\n",
                          typelib_trace_make_typeid_str (&tistr, &type->xt.id.x),
                          typelib_trace_make_typeid_str (&depstr, dep_type_id),
                          (void *) dst_dep_type, type_state_str (dst_dep_type->state),
                          (void *) dst_dep_type->scc, ddsi_type_scc_materialized (dst_dep_type),
                          type_map != NULL);
  }
  if (!type_map)
  {
    TYPELIB_IMPORT_TRACE ("skip dep import for %s: no TypeMap\n",
                          typelib_trace_make_typeid_str (&tistr, dep_type_id));
    return DDS_RETCODE_OK;
  }
  const bool dep_is_scc = dep_type_id->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT;
  if (ddsi_type_resolved_locked (gv, dst_dep_type, DDSI_TYPE_IGNORE_DEPS) &&
      (!dep_is_scc || ddsi_type_scc_materialized (dst_dep_type)))
  {
    TYPELIB_IMPORT_TRACE ("skip dep import for %s: resolved-ignore-deps state=%s materialized=%d\n",
                          typelib_trace_make_typeid_str (&tistr, dep_type_id),
                          type_state_str (dst_dep_type->state), ddsi_type_scc_materialized (dst_dep_type));
    assert_scc_materialized (gv, dst_dep_type, dep_type_id, "dependency import");
    return DDS_RETCODE_OK;
  }

  if (dep_is_scc)
  {
    bool complete = false;
    uint32_t nrefs = 0;
    struct ddsi_type **refs = NULL;
    const dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *pairs = typemap_typeobj_pairseq (type_map, dep_type_id);
    assert (pairs);
    assert (n_match_upd && gpe_match_upd);
    TYPELIB_IMPORT_TRACE ("import SCC dep %s pairs=%"PRIu32"\n",
                          typelib_trace_make_typeid_str (&tistr, dep_type_id), pairs->_length);
    if ((ret = type_ref_scc_pairseq_locked (gv, pairs, &refs, &nrefs)) == DDS_RETCODE_OK &&
        (ret = ddsi_type_add_scc_typeobjs_locked (gv, pairs, dep_type_id, true, &complete)) == DDS_RETCODE_OK)
    {
      assert (complete);
      const struct ddsi_type *dep_type = ddsi_type_lookup_locked_impl (gv, dep_type_id);
      TYPELIB_IMPORT_TRACE ("import SCC dep %s complete=%d dep=%p state=%s materialized=%d\n",
                            typelib_trace_make_typeid_str (&tistr, dep_type_id), complete,
                            (const void *) dep_type, dep_type ? type_state_str (dep_type->state) : "(none)",
                            ddsi_type_scc_materialized (dep_type));
      assert_scc_materialized (gv, dep_type, dep_type_id, "dependency SCC import");
      if (!ddsi_type_scc_materialized (dep_type))
        ret = DDS_RETCODE_BAD_PARAMETER;
      else
        ddsi_type_get_gpe_matches (gv, type, gpe_match_upd, n_match_upd);
    }
    type_unref_many_locked (gv, refs, nrefs);
  }
  else
  {
    const struct DDS_XTypes_TypeObject *dep_type_obj = ddsi_typemap_typeobj (type_map, dep_type_id);
    if (dep_type_obj)
    {
      assert (n_match_upd && gpe_match_upd);
      if ((ret = ddsi_type_add_typeobj (gv, dst_dep_type, dep_type_obj)) == DDS_RETCODE_OK)
        ddsi_type_get_gpe_matches (gv, type, gpe_match_upd, n_match_upd);
      TYPELIB_IMPORT_TRACE ("import dep TypeObject %s ret=%d state=%s materialized=%d\n",
                            typelib_trace_make_typeid_str (&tistr, dep_type_id), ret,
                            type_state_str (dst_dep_type->state), ddsi_type_scc_materialized (dst_dep_type));
    }
    else
    {
      TYPELIB_IMPORT_TRACE ("skip dep import for %s: TypeMap has no TypeObject\n",
                            typelib_trace_make_typeid_str (&tistr, dep_type_id));
    }
  }
  return ret;
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
    if (!gv->config.allow_recursive_types && ddsi_typeid_contains_scc_impl (dep_type_id))
    {
      set_type_invalid (gv, type);
      return DDS_RETCODE_UNSUPPORTED;
    }
    if (dep_type_id->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
    {
      if (!ddsi_type_scc_id_is_valid_impl (&dep_type_id->_u.sc_component_id))
      {
        set_type_invalid (gv, type);
        return DDS_RETCODE_BAD_PARAMETER;
      }
      for (int32_t i = 1; i <= dep_type_id->_u.sc_component_id.scc_length && ret == DDS_RETCODE_OK; i++)
      {
        struct DDS_XTypes_TypeIdentifier dep_type_id_i = *dep_type_id;
        dep_type_id_i._u.sc_component_id.scc_index = i;
        ret = type_add_dep (gv, type, type_map, &dep_type_id_i, n_match_upd, gpe_match_upd);
      }
    }
    else
      ret = type_add_dep (gv, type, type_map, dep_type_id, n_match_upd, gpe_match_upd);
  }
  if (ret == DDS_RETCODE_OK)
    ddsi_type_update_state_locked (gv, type);
  return ret;
}

void ddsi_type_ref_locked (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct ddsi_type *src)
{
  assert (src);
  struct ddsi_type *t = (struct ddsi_type *) src;
  if (t->scc != NULL && t->scc->active)
  {
    assert (!t->scc->freeing);
    t->scc->refc++;
    TYPELIB_TRACE ("ref ddsi_type %p scc %p refc %"PRIu32"\n", t, t->scc, t->scc->refc);
  }
  else
  {
    t->refc++;
    TYPELIB_TRACE ("ref ddsi_type %p refc %"PRIu32"\n", t, t->refc);
  }
  if (type)
    *type = t;
}

void ddsi_type_ref_dep_locked (struct ddsi_domaingv *gv, const struct ddsi_type *owner, struct ddsi_type **type, const struct ddsi_type *src)
{
  if (ddsi_type_dep_is_active_scc_internal (owner, src))
  {
    *type = (struct ddsi_type *) src;
  }
  else
    ddsi_type_ref_locked (gv, type, src);
}

void ddsi_type_ref (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct ddsi_type *src)
{
  ddsrt_mutex_lock (&gv->typelib_lock);
  ddsi_type_ref_locked (gv, type, src);
  ddsrt_mutex_unlock (&gv->typelib_lock);
}

dds_return_t ddsi_type_ref_id_locked_impl (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct DDS_XTypes_TypeIdentifier *type_id)
{
  dds_return_t ret = DDS_RETCODE_OK;
  if (ddsi_typeid_is_none_impl (type_id))
    return DDS_RETCODE_BAD_PARAMETER;
  if (!gv->config.allow_recursive_types && ddsi_typeid_contains_scc_impl (type_id))
  {
    if (type)
      *type = NULL;
    return DDS_RETCODE_UNSUPPORTED;
  }

  TYPELIB_TRACE ("ref ddsi_type type-id %s", typelib_trace_make_typeid_str (&tistr, type_id));
  struct ddsi_type *t = ddsi_type_lookup_locked_impl (gv, type_id);
  if (!t && (ret = ddsi_type_new (gv, &t, type_id, NULL)) != DDS_RETCODE_OK)
  {
    if (type)
      *type = NULL;
    return ret;
  }
  ddsi_type_ref_locked (gv, type, t);
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
  while (ddsi_xt_has_definition (&type->xt) && type->xt._d == DDS_XTypes_TK_ALIAS)
    type = type->xt._u.alias.related_type;
  return (ddsi_xt_missing_definition (&type->xt) || type->xt._d == DDS_XTypes_TK_STRUCTURE || type->xt._d == DDS_XTypes_TK_UNION);
}

static dds_return_t type_add_ref_impl (struct ddsi_domaingv *gv, struct ddsi_type **type, const ddsi_typeinfo_t *type_info, const ddsi_typemap_t *type_map, ddsi_typeid_kind_t kind)
{
  struct ddsi_generic_proxy_endpoint **gpe_match_upd = NULL;
  dds_return_t ret = DDS_RETCODE_OK;
  uint32_t n_match_upd = 0;
  bool resolved = false;
  uint32_t n_scc_import_refs = 0;
  struct ddsi_type **scc_import_refs = NULL;
  const struct DDS_XTypes_TypeIdentifier *type_id = (kind == DDSI_TYPEID_KIND_MINIMAL) ? &type_info->x.minimal.typeid_with_size.type_id : &type_info->x.complete.typeid_with_size.type_id;
  const struct DDS_XTypes_TypeObject *type_obj = ddsi_typemap_typeobj (type_map, type_id);

  assert (kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_COMPLETE);
  ddsrt_mutex_lock (&gv->typelib_lock);
  struct ddsi_type *t = ddsi_type_lookup_locked_impl (gv, type_id);
  const bool resolved_ignore_deps = t != NULL && ddsi_type_resolved_locked (gv, t, DDSI_TYPE_IGNORE_DEPS);
  TYPELIB_IMPORT_TRACE ("top-level add kind=%s id=%s existing=%p state=%s resolved-ignore-deps=%d scc=%p materialized=%d typemap=%d typeobj=%d\n",
                        kind == DDSI_TYPEID_KIND_MINIMAL ? "minimal" : "complete",
                        typelib_trace_make_typeid_str (&tistr, type_id),
                        (void *) t, t ? type_state_str (t->state) : "(new)",
                        resolved_ignore_deps, t ? (void *) t->scc : NULL,
                        ddsi_type_scc_materialized (t), type_map != NULL, type_obj != NULL);
  if (!gv->config.allow_recursive_types && ddsi_typeid_contains_scc_impl (type_id))
  {
    ret = DDS_RETCODE_UNSUPPORTED;
    ddsrt_mutex_unlock (&gv->typelib_lock);
    goto err;
  }
  if (type_id->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
  {
    if (type_map != NULL && (t == NULL || !ddsi_type_resolved_locked (gv, t, DDSI_TYPE_IGNORE_DEPS) || !ddsi_type_scc_materialized (t)))
    {
      bool complete = false;
      const dds_sequence_DDS_XTypes_TypeIdentifierTypeObjectPair *pairs = typemap_typeobj_pairseq (type_map, type_id);
      assert (pairs);
      TYPELIB_IMPORT_TRACE ("top-level SCC import %s pairs=%"PRIu32"\n",
                            typelib_trace_make_typeid_str (&tistr, type_id), pairs->_length);
      if ((ret = type_ref_scc_pairseq_locked (gv, pairs, &scc_import_refs, &n_scc_import_refs)) != DDS_RETCODE_OK ||
          (ret = ddsi_type_add_scc_typeobjs_locked (gv, pairs, type_id, true, &complete)) != DDS_RETCODE_OK)
      {
        type_unref_many_locked (gv, scc_import_refs, n_scc_import_refs);
        ddsrt_mutex_unlock (&gv->typelib_lock);
        goto err;
      }
      assert (complete);
      resolved = true;
      t = ddsi_type_lookup_locked_impl (gv, type_id);
      if (t == NULL)
      {
        ret = DDS_RETCODE_ERROR;
        type_unref_many_locked (gv, scc_import_refs, n_scc_import_refs);
        ddsrt_mutex_unlock (&gv->typelib_lock);
        goto err;
      }
      if (!ddsi_type_scc_materialized (t))
      {
        assert_scc_materialized (gv, t, type_id, "top-level SCC import");
        ret = DDS_RETCODE_BAD_PARAMETER;
        type_unref_many_locked (gv, scc_import_refs, n_scc_import_refs);
        ddsrt_mutex_unlock (&gv->typelib_lock);
        goto err;
      }
      TYPELIB_IMPORT_TRACE ("top-level SCC import %s complete=%d top=%p state=%s materialized=%d\n",
                            typelib_trace_make_typeid_str (&tistr, type_id), complete,
                            (void *) t, type_state_str (t->state), ddsi_type_scc_materialized (t));
    }
    else
    {
      TYPELIB_IMPORT_TRACE ("skip top-level SCC import %s: typemap=%d existing=%p state=%s materialized=%d\n",
                            typelib_trace_make_typeid_str (&tistr, type_id), type_map != NULL,
                            (void *) t, t ? type_state_str (t->state) : "(none)",
                            ddsi_type_scc_materialized (t));
    }
    type_obj = NULL;
    if (type_map != NULL)
      assert_scc_materialized (gv, t, type_id, "top-level SCC import");
  }
  if (!t)
  {
    TYPELIB_IMPORT_TRACE ("create top-level type %s typeobj=%d\n",
                          typelib_trace_make_typeid_str (&tistr, type_id), type_obj != NULL);
    ret = ddsi_type_new (gv, &t, type_id, type_obj);
    resolved = true;
  }
  else if (type_obj)
  {
    enum ddsi_type_state s = t->state;
    ret = ddsi_type_add_typeobj (gv, t, type_obj);
    resolved = (t->state == DDSI_TYPE_RESOLVED && t->state != s);
    TYPELIB_IMPORT_TRACE ("add top-level TypeObject %s ret=%d state %s -> %s materialized=%d\n",
                          typelib_trace_make_typeid_str (&tistr, type_id), ret,
                          type_state_str (s), type_state_str (t->state), ddsi_type_scc_materialized (t));
  }
  if (ret != DDS_RETCODE_OK)
  {
    type_unref_many_locked (gv, scc_import_refs, n_scc_import_refs);
    ddsrt_mutex_unlock (&gv->typelib_lock);
    goto err;
  }

  ddsi_type_ref_locked (gv, NULL, t);
  type_unref_many_locked (gv, scc_import_refs, n_scc_import_refs);
  scc_import_refs = NULL;
  n_scc_import_refs = 0;

  if ((ret = valid_top_level_type (t) ? DDS_RETCODE_OK : DDS_RETCODE_BAD_PARAMETER)
      || (ret = type_add_deps (gv, t, type_info, type_map, kind, &n_match_upd, &gpe_match_upd))
      || (ret = validate_type (gv, t)))
  {
    struct typelib_trace_typeid_str invalidstr;
    GVWARNING ("invalid top-level type %s\n", typelib_trace_make_typeid_str (&invalidstr, &t->xt.id.x));
    ddsi_type_unref_locked (gv, t);
    ddsrt_mutex_unlock (&gv->typelib_lock);
    goto err;
  }

  TYPELIB_IMPORT_TRACE ("top-level add done id=%s state=%s materialized=%d matches=%"PRIu32"\n",
                        typelib_trace_make_typeid_str (&tistr, type_id), type_state_str (t->state),
                        ddsi_type_scc_materialized (t), n_match_upd);

#ifdef DDS_HAS_TYPELIB
  if (resolved)
  {
    TYPELIB_TRACE ("type %s resolved\n", typelib_trace_make_typeid_str (&tistr, type_id));
    ddsrt_cond_etime_broadcast (&gv->typelib_resolved_cond);
  }
#else
  (void) resolved;
#endif
  ddsrt_mutex_unlock (&gv->typelib_lock);

  if (gpe_match_upd != NULL)
  {
    for (uint32_t e = 0; e < n_match_upd; e++)
    {
      TYPELIB_TRACE ("type %s trigger matching "PGUIDFMT"\n", typelib_trace_make_typeid_str (&tistr, type_id), PGUID(gpe_match_upd[e]->e.guid));
      ddsi_update_proxy_endpoint_matching (gv, gpe_match_upd[e]);
    }
    ddsrt_free (gpe_match_upd);
  }
  if (type)
    *type = t;

err:
  return ret;
}

dds_return_t ddsi_type_add (struct ddsi_domaingv *gv, struct ddsi_type **type_minimal, struct ddsi_type **type_complete, const ddsi_typeinfo_t *type_info, const ddsi_typemap_t *type_map)
{
  dds_return_t ret;
  struct ddsi_type *minimal = NULL, *complete = NULL;
  if (type_minimal != NULL)
    *type_minimal = NULL;
  if (type_complete != NULL)
    *type_complete = NULL;

  if ((ret = type_add_ref_impl (gv, &minimal, type_info, type_map, DDSI_TYPEID_KIND_MINIMAL)) == DDS_RETCODE_OK)
  {
    assert (minimal != NULL);
    ret = type_add_ref_impl (gv, &complete, type_info, type_map, DDSI_TYPEID_KIND_COMPLETE);
    if (ret != DDS_RETCODE_OK)
    {
      ddsi_type_unref (gv, minimal);
      minimal = NULL;
    }
    else
    {
      if (type_minimal != NULL)
        *type_minimal = minimal;
      else
        ddsi_type_unref (gv, minimal);
      if (type_complete != NULL)
        *type_complete = complete;
      else
        ddsi_type_unref (gv, complete);
    }
  }
  return ret;
}

dds_return_t ddsi_type_ref_local (struct ddsi_domaingv *gv, struct ddsi_type **type, const struct ddsi_sertype *sertype, ddsi_typeid_kind_t kind)
{
  dds_return_t ret = DDS_RETCODE_OK;
  assert (sertype);
  ddsi_typeinfo_t *type_info = ddsi_sertype_typeinfo (sertype);
  if (!type_info)
  {
    if (type)
      *type = NULL;
  }
  else
  {
    ddsi_typemap_t *type_map = ddsi_sertype_typemap (sertype);
    const struct DDS_XTypes_TypeIdentifier *type_id = (kind == DDSI_TYPEID_KIND_MINIMAL) ? &type_info->x.minimal.typeid_with_size.type_id : &type_info->x.complete.typeid_with_size.type_id;
    TYPELIB_TRACE ("ref ddsi_type local sertype %p id %s", sertype, typelib_trace_make_typeid_str (&tistr, type_id));
    ret = type_add_ref_impl (gv, type, type_info, type_map, kind);
    ddsi_typemap_fini (type_map);
    ddsrt_free (type_map);
    ddsi_typeinfo_fini (type_info);
    ddsrt_free (type_info);
  }
  return ret;
}
dds_return_t ddsi_type_ref_proxy (struct ddsi_domaingv *gv, struct ddsi_type **type, const ddsi_typeinfo_t *type_info, ddsi_typeid_kind_t kind, const ddsi_guid_t *proxy_guid)
{
  dds_return_t ret = DDS_RETCODE_OK;
  assert (type_info);
  assert (kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_COMPLETE);
  const struct DDS_XTypes_TypeIdentifier *type_id = (kind == DDSI_TYPEID_KIND_MINIMAL) ? &type_info->x.minimal.typeid_with_size.type_id : &type_info->x.complete.typeid_with_size.type_id;

  ddsrt_mutex_lock (&gv->typelib_lock);

  TYPELIB_TRACE ("ref ddsi_type proxy id %s", typelib_trace_make_typeid_str (&tistr, type_id));
  struct ddsi_type *t = ddsi_type_lookup_locked_impl (gv, type_id);
  if (!t && (ret = ddsi_type_new (gv, &t, type_id, NULL)) != DDS_RETCODE_OK)
    goto err;
  ddsi_type_ref_locked (gv, NULL, t);
  if (!valid_top_level_type (t))
  {
    ret = DDS_RETCODE_BAD_PARAMETER;
    ddsi_type_unref_locked (gv, t);
    TYPELIB_TRACE (" invalid top-level type\n");
    goto err;
  }

  if ((ret = type_add_deps (gv, t, type_info, NULL, kind, NULL, NULL))
      || (ret = validate_type (gv, t)))
  {
    ddsi_type_unref_locked (gv, t);
    goto err;
  }

  if (proxy_guid != NULL && !ddsi_type_proxy_guid_exists (t, proxy_guid))
  {
    ddsi_type_proxy_guid_list_insert (&t->proxy_guids, *proxy_guid);
    TYPELIB_TRACE ("type %s add ep "PGUIDFMT"\n", typelib_trace_make_typeid_str (&tistr, type_id), PGUID (*proxy_guid));
  }
  if (type)
    *type = t;
err:
  ddsrt_mutex_unlock (&gv->typelib_lock);
  return ret;
}

static dds_return_t xcdr2_ser (const void *obj, const struct dds_cdrstream_desc *desc, dds_ostreamLE_t *os)
{
  os->x.m_buffer = NULL;
  os->x.m_index = 0;
  os->x.m_size = 0;
  os->x.m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2;
  if (!dds_stream_write_sampleLE (os, &dds_cdrstream_default_allocator, obj, desc))
  {
    dds_ostreamLE_fini (os, &dds_cdrstream_default_allocator);
    os->x.m_buffer = NULL;
    os->x.m_index = 0;
    os->x.m_size = 0;
    return DDS_RETCODE_BAD_PARAMETER;
  }
  return DDS_RETCODE_OK;
}

static dds_return_t get_typeid_with_size (DDS_XTypes_TypeIdentifierWithSize *typeid_with_size, const DDS_XTypes_TypeIdentifier *ti, const DDS_XTypes_TypeObject *to)
{
  dds_return_t ret;
  dds_ostreamLE_t os;
  ddsi_typeid_copy_impl (&typeid_with_size->type_id, ti);
  if ((ret = xcdr2_ser (to, &DDS_XTypes_TypeObject_cdrstream_desc, &os)) < 0)
  {
    ddsi_typeid_fini_impl (&typeid_with_size->type_id);
    memset (typeid_with_size, 0, sizeof (*typeid_with_size));
    return ret;
  }
  typeid_with_size->typeobject_serialized_size = os.x.m_index;
  dds_ostreamLE_fini (&os, &dds_cdrstream_default_allocator);
  return DDS_RETCODE_OK;
}

static dds_return_t DDS_XTypes_TypeIdentifierWithDependencies_deps_init (DDS_XTypes_TypeIdentifierWithDependencies *x, uint32_t n_deps)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

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
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

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
    if (ddsi_typeid_compare_impl (&x->dependent_typeids._buffer[i].type_id,
        &x->dependent_typeids._buffer[x->dependent_typeids._length].type_id) == 0)
    {
      assert (x->dependent_typeids._buffer[i].typeobject_serialized_size == x->dependent_typeids._buffer[x->dependent_typeids._length].typeobject_serialized_size);
      ddsi_typeid_fini_impl (&x->dependent_typeids._buffer[x->dependent_typeids._length].type_id);
      memset (&x->dependent_typeids._buffer[x->dependent_typeids._length], 0, sizeof (x->dependent_typeids._buffer[x->dependent_typeids._length]));
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
  x->dependent_typeid_count = 0;
  memset (&x->dependent_typeids, 0, sizeof (x->dependent_typeids));
}

static dds_return_t ddsi_typeinfo_deps_init (struct ddsi_typeinfo *type_info, uint32_t n_deps)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

static dds_return_t ddsi_typeinfo_deps_init (struct ddsi_typeinfo *type_info, uint32_t n_deps)
{
  dds_return_t ret;
  if ((ret = DDS_XTypes_TypeIdentifierWithDependencies_deps_init (&type_info->x.minimal, n_deps)) != 0)
    return ret;
  if ((ret = DDS_XTypes_TypeIdentifierWithDependencies_deps_init (&type_info->x.complete, n_deps)) != 0)
    DDS_XTypes_TypeIdentifierWithDependencies_deps_fini (&type_info->x.minimal);
  return ret;
}

static bool ddsi_type_scc_generated_minimal_matches (
    const struct ddsi_type_scc *scc,
    const struct DDS_XTypes_TypeIdentifier *type_id_m)
{
  const struct ddsi_type *type_m = scc->generated_minimal_ref;
  return type_m != NULL &&
    type_m->xt.id.x._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT &&
    ddsi_type_scc_id_same_component_impl (&type_m->xt.id.x._u.sc_component_id, &type_id_m->_u.sc_component_id);
}

static dds_return_t ddsi_type_materialize_minimal_scc_locked (struct ddsi_domaingv *gv, struct ddsi_type *type_c, const struct DDS_XTypes_TypeIdentifier *type_id_m)
{
  assert (type_id_m->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT);
  struct ddsi_type_scc * const scc_c = type_c->scc;
  if (scc_c == NULL || !ddsi_type_scc_id_is_valid_impl (&type_id_m->_u.sc_component_id) || !ddsi_type_scc_wire_complete (scc_c))
    return DDS_RETCODE_BAD_PARAMETER;

  if (scc_c->generated_minimal_ref != NULL)
    return ddsi_type_scc_generated_minimal_matches (scc_c, type_id_m) &&
      ddsi_type_scc_materialized (scc_c->generated_minimal_ref) ? DDS_RETCODE_OK : DDS_RETCODE_BAD_PARAMETER;

  const uint32_t scc_length = (uint32_t) type_id_m->_u.sc_component_id.scc_length;
  bool seen[DDSI_TYPE_SCC_MAX_WIRE_TYPES] = { false };
  struct ddsi_type *slot_types_m[DDSI_TYPE_SCC_MAX_WIRE_TYPES] = { NULL };
  uint32_t nseen = 0;
  dds_return_t ret = DDS_RETCODE_OK;

  for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < scc_c->n_wire_types; n++)
  {
    const struct ddsi_type *slot_type_c = scc_c->types[n];
    if (slot_type_c == NULL)
      return DDS_RETCODE_BAD_PARAMETER;

    struct DDS_XTypes_TypeIdentifier slot_type_id_m;
    ddsi_xt_get_typeid_impl (&slot_type_c->xt, &slot_type_id_m, DDSI_TYPEID_KIND_MINIMAL);
    if (slot_type_id_m._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT &&
        ddsi_type_scc_id_same_component_impl (&slot_type_id_m._u.sc_component_id, &type_id_m->_u.sc_component_id))
    {
      if (!ddsi_type_scc_id_is_valid_impl (&slot_type_id_m._u.sc_component_id))
        ret = DDS_RETCODE_BAD_PARAMETER;
      else
      {
        const uint32_t index = (uint32_t) slot_type_id_m._u.sc_component_id.scc_index - 1;
        if (index >= scc_length)
          ret = DDS_RETCODE_BAD_PARAMETER;
        else
        {
          if (!seen[index])
          {
            seen[index] = true;
            nseen++;
          }

          struct DDS_XTypes_TypeObject slot_type_obj_m;
          ddsi_xt_get_typeobject_kind_impl (&slot_type_c->xt, &slot_type_obj_m, DDSI_TYPEID_KIND_MINIMAL);
          struct ddsi_type *slot_type_m = ddsi_type_lookup_locked_impl (gv, &slot_type_id_m);
          if (slot_type_m == NULL)
            ret = ddsi_type_new_impl (gv, &slot_type_m, &slot_type_id_m, &slot_type_obj_m, true);
          else
            ret = ddsi_type_add_typeobj_impl (gv, slot_type_m, &slot_type_obj_m, true);
          if (ret == DDS_RETCODE_OK)
          {
            assert (slot_type_m != NULL);
            slot_types_m[index] = slot_type_m;
          }
          ddsi_typeobj_fini_impl (&slot_type_obj_m);
        }
      }
    }
    ddsi_typeid_fini_impl (&slot_type_id_m);
  }

  if (ret != DDS_RETCODE_OK)
    return ret;
  if (nseen != scc_length)
    return DDS_RETCODE_BAD_PARAMETER;

  for (uint32_t n = 0; n < scc_length; n++)
  {
    if (slot_types_m[n] == NULL)
      return DDS_RETCODE_BAD_PARAMETER;
  }

  /* Keep one representative of the generated minimal SCC alive.  Once an SCC
     is active, any member reference owns the whole component; before activation,
     the verified SCC member references keep the generated component internally
     connected. */
  if (!ddsi_type_scc_materialized (slot_types_m[0]))
    return DDS_RETCODE_BAD_PARAMETER;
  ddsi_type_ref_locked (gv, &scc_c->generated_minimal_ref, slot_types_m[0]);
  return DDS_RETCODE_OK;
}

static dds_return_t ddsi_type_materialize_minimal_type_locked (
    struct ddsi_domaingv *gv,
    struct ddsi_type *type_c,
    const struct DDS_XTypes_TypeIdentifier *type_id_m,
    const struct DDS_XTypes_TypeObject *type_obj_m)
{
  if (type_id_m->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
    return ddsi_type_materialize_minimal_scc_locked (gv, type_c, type_id_m);

  struct ddsi_type *type_m = ddsi_type_lookup_locked_impl (gv, type_id_m);
  if (type_m == NULL)
    return ddsi_type_new (gv, &type_m, type_id_m, type_obj_m);
  else
    return ddsi_type_add_typeobj (gv, type_m, type_obj_m);
}

static dds_return_t ddsi_typeinfo_deps_append (struct ddsi_domaingv *gv, struct ddsi_typeinfo *type_info, const ddsi_typeid_t *dep_type_id)
  ddsrt_attribute_warn_unused_result ddsrt_nonnull_all;

static dds_return_t ddsi_typeinfo_deps_append (struct ddsi_domaingv *gv, struct ddsi_typeinfo *type_info, const ddsi_typeid_t *dep_type_id)
{
  struct ddsi_type * const dep_type_c = ddsi_type_lookup_locked (gv, dep_type_id);
  DDS_XTypes_TypeObject to_dep_m, to_dep_c;
  ddsi_typeid_t ti_dep_m;
  dds_return_t ret;

  if (dep_type_c == NULL)
    return DDS_RETCODE_ERROR;

  ddsi_xt_get_typeobject_kind_impl (&dep_type_c->xt, &to_dep_m, DDSI_TYPEID_KIND_MINIMAL);
  ddsi_xt_get_typeid_impl (&dep_type_c->xt, &ti_dep_m.x, DDSI_TYPEID_KIND_MINIMAL);

  if ((ret = ddsi_type_materialize_minimal_type_locked (gv, dep_type_c, &ti_dep_m.x, &to_dep_m)) != DDS_RETCODE_OK)
    goto err_dep_type_m;

  ddsi_xt_get_typeobject_kind_impl (&dep_type_c->xt, &to_dep_c, DDSI_TYPEID_KIND_COMPLETE);

  // append dedups because two different complete type ids can map to the same minimal type id
  // (it does so inefficiently, but ... well ... good enough for now)
  if ((ret = DDS_XTypes_TypeIdentifierWithDependencies_deps_append (&type_info->x.minimal, &ti_dep_m.x, &to_dep_m)) == DDS_RETCODE_OK)
    ret = DDS_XTypes_TypeIdentifierWithDependencies_deps_append (&type_info->x.complete, &dep_type_c->xt.id.x, &to_dep_c);

  ddsi_typeobj_fini_impl (&to_dep_c);
err_dep_type_m:
  ddsi_typeid_fini (&ti_dep_m);
  ddsi_typeobj_fini_impl (&to_dep_m);
  return ret;
}

static void ddsi_typeinfo_deps_fini (struct ddsi_typeinfo *type_info)
{
  DDS_XTypes_TypeIdentifierWithDependencies_deps_fini (&type_info->x.minimal);
  DDS_XTypes_TypeIdentifierWithDependencies_deps_fini (&type_info->x.complete);
}

static void ddsi_typeinfo_toplevel_fini (struct ddsi_typeinfo *type_info)
{
  ddsi_typeid_fini_impl (&type_info->x.minimal.typeid_with_size.type_id);
  ddsi_typeid_fini_impl (&type_info->x.complete.typeid_with_size.type_id);
  memset (&type_info->x.minimal.typeid_with_size, 0, sizeof (type_info->x.minimal.typeid_with_size));
  memset (&type_info->x.complete.typeid_with_size, 0, sizeof (type_info->x.complete.typeid_with_size));
}

static dds_return_t ddsi_type_get_typeinfo_toplevel (struct ddsi_domaingv *gv, struct ddsi_type *type_c, struct ddsi_typeinfo *type_info, struct ddsi_type **type_m)
{
  DDS_XTypes_TypeObject to_c, to_m;
  ddsi_typeid_t ti_m;
  dds_return_t ret;

  assert (type_c && type_m && type_info);
  ddsi_xt_get_typeobject_kind_impl (&type_c->xt, &to_c, DDSI_TYPEID_KIND_COMPLETE);
  ddsi_xt_get_typeobject_kind_impl (&type_c->xt, &to_m, DDSI_TYPEID_KIND_MINIMAL);
  ddsi_xt_get_typeid_impl (&type_c->xt, &ti_m.x, DDSI_TYPEID_KIND_MINIMAL);

  if ((ret = ddsi_type_materialize_minimal_type_locked (gv, type_c, &ti_m.x, &to_m)) != DDS_RETCODE_OK)
    goto err_type_new;

  *type_m = ddsi_type_lookup_locked_impl (gv, &ti_m.x);
  if (*type_m == NULL)
  {
    ret = DDS_RETCODE_ERROR;
    goto err_type_new;
  }
  ddsi_type_ref_locked (gv, NULL, *type_m);

  ddsi_type_ref_locked (gv, NULL, type_c);

#ifndef NDEBUG
  {
    ddsi_typeid_t ti_c;
    ddsi_xt_get_typeid_impl (&type_c->xt, &ti_c.x, DDSI_TYPEID_KIND_COMPLETE);
    assert (ddsi_typeid_compare (&type_c->xt.id, &ti_c) == 0);
    ddsi_typeid_fini (&ti_c);
  }
#endif

  if ((ret = get_typeid_with_size (&type_info->x.minimal.typeid_with_size, &ti_m.x, &to_m)) == 0)
    ret = get_typeid_with_size (&type_info->x.complete.typeid_with_size, &type_c->xt.id.x, &to_c);
  if (ret != DDS_RETCODE_OK)
  {
    ddsi_typeinfo_toplevel_fini (type_info);
    ddsi_type_unref_locked (gv, *type_m);
    *type_m = NULL;
    ddsi_type_unref_locked (gv, type_c);
  }

err_type_new:
  ddsi_typeid_fini (&ti_m);
  ddsi_typeobj_fini_impl (&to_c);
  ddsi_typeobj_fini_impl (&to_m);
  return ret;
}

static bool type_id_kind_is_hash_dependency (ddsi_typeid_kind_t kind)
{
  /* SCC identifiers are classified as minimal/complete by ddsi_typeid_kind:
     they are TypeObject-bearing dependencies even though their discriminator is
     not EK_MINIMAL/EK_COMPLETE. */
  return kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_COMPLETE;
}

#ifndef NDEBUG
static bool debug_typeinfo_minimal_contains (const struct ddsi_typeinfo *type_info, const struct DDS_XTypes_TypeIdentifier *type_id)
{
  if (type_id->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
  {
    if (type_info->x.minimal.typeid_with_size.type_id._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT &&
    ddsi_type_scc_id_same_component_impl (&type_info->x.minimal.typeid_with_size.type_id._u.sc_component_id, &type_id->_u.sc_component_id))
      return true;
    for (uint32_t n = 0; n < type_info->x.minimal.dependent_typeids._length; n++)
    {
      const struct DDS_XTypes_TypeIdentifier *dep_type_id = &type_info->x.minimal.dependent_typeids._buffer[n].type_id;
      if (dep_type_id->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT &&
          ddsi_type_scc_id_same_component_impl (&dep_type_id->_u.sc_component_id, &type_id->_u.sc_component_id))
        return true;
    }
  }
  else
  {
    if (ddsi_typeid_compare_impl (&type_info->x.minimal.typeid_with_size.type_id, type_id) == 0)
      return true;
    for (uint32_t n = 0; n < type_info->x.minimal.dependent_typeids._length; n++)
      if (ddsi_typeid_compare_impl (&type_info->x.minimal.dependent_typeids._buffer[n].type_id, type_id) == 0)
        return true;
  }
  return false;
}

static void debug_typeinfo_assert_minimal_ref (struct ddsi_domaingv *gv, const struct ddsi_typeinfo *type_info, const struct DDS_XTypes_TypeIdentifier *type_id)
{
  if (type_id == NULL || ddsi_typeid_is_none_impl (type_id))
    return;

  switch (type_id->_d)
  {
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      debug_typeinfo_assert_minimal_ref (gv, type_info, type_id->_u.seq_sdefn.element_identifier);
      return;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      debug_typeinfo_assert_minimal_ref (gv, type_info, type_id->_u.seq_ldefn.element_identifier);
      return;
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
      debug_typeinfo_assert_minimal_ref (gv, type_info, type_id->_u.array_sdefn.element_identifier);
      return;
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      debug_typeinfo_assert_minimal_ref (gv, type_info, type_id->_u.array_ldefn.element_identifier);
      return;
    case DDS_XTypes_TI_PLAIN_MAP_SMALL:
      debug_typeinfo_assert_minimal_ref (gv, type_info, type_id->_u.map_sdefn.key_identifier);
      debug_typeinfo_assert_minimal_ref (gv, type_info, type_id->_u.map_sdefn.element_identifier);
      return;
    case DDS_XTypes_TI_PLAIN_MAP_LARGE:
      debug_typeinfo_assert_minimal_ref (gv, type_info, type_id->_u.map_ldefn.key_identifier);
      debug_typeinfo_assert_minimal_ref (gv, type_info, type_id->_u.map_ldefn.element_identifier);
      return;
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT:
      break;
    default:
      break;
  }

  if (ddsi_typeid_kind ((const ddsi_typeid_t *) type_id) == DDSI_TYPEID_KIND_MINIMAL)
  {
    if (!debug_typeinfo_minimal_contains (type_info, type_id))
    {
      struct typelib_trace_typeid_str tistr;
      GVWARNING ("generated minimal TypeObject references %s, which is missing from generated TypeInformation.minimal\n",
                 typelib_trace_make_typeid_str (&tistr, type_id));
      assert (false);
    }
    if (type_id->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
      assert_scc_materialized (gv, ddsi_type_lookup_locked_impl (gv, type_id), type_id, "TypeInformation minimal reference generation");
  }
}

static void debug_typeinfo_visit_minimal_typeobj_refs (struct ddsi_domaingv *gv, const struct ddsi_typeinfo *type_info, const struct DDS_XTypes_MinimalTypeObject *type_obj)
{
  switch (type_obj->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      debug_typeinfo_assert_minimal_ref (gv, type_info, &type_obj->_u.alias_type.body.common.related_type);
      break;
    case DDS_XTypes_TK_ANNOTATION:
      for (uint32_t n = 0; n < type_obj->_u.annotation_type.member_seq._length; n++)
        debug_typeinfo_assert_minimal_ref (gv, type_info, &type_obj->_u.annotation_type.member_seq._buffer[n].common.member_type_id);
      break;
    case DDS_XTypes_TK_STRUCTURE:
      debug_typeinfo_assert_minimal_ref (gv, type_info, &type_obj->_u.struct_type.header.base_type);
      for (uint32_t n = 0; n < type_obj->_u.struct_type.member_seq._length; n++)
        debug_typeinfo_assert_minimal_ref (gv, type_info, &type_obj->_u.struct_type.member_seq._buffer[n].common.member_type_id);
      break;
    case DDS_XTypes_TK_UNION:
      debug_typeinfo_assert_minimal_ref (gv, type_info, &type_obj->_u.union_type.discriminator.common.type_id);
      for (uint32_t n = 0; n < type_obj->_u.union_type.member_seq._length; n++)
        debug_typeinfo_assert_minimal_ref (gv, type_info, &type_obj->_u.union_type.member_seq._buffer[n].common.type_id);
      break;
    case DDS_XTypes_TK_SEQUENCE:
      debug_typeinfo_assert_minimal_ref (gv, type_info, &type_obj->_u.sequence_type.element.common.type);
      break;
    case DDS_XTypes_TK_ARRAY:
      debug_typeinfo_assert_minimal_ref (gv, type_info, &type_obj->_u.array_type.element.common.type);
      break;
    case DDS_XTypes_TK_MAP:
      debug_typeinfo_assert_minimal_ref (gv, type_info, &type_obj->_u.map_type.key.common.type);
      debug_typeinfo_assert_minimal_ref (gv, type_info, &type_obj->_u.map_type.element.common.type);
      break;
    default:
      break;
  }
}

static void debug_typeinfo_assert_generated_minimal_typeobj (struct ddsi_domaingv *gv, const struct ddsi_typeinfo *type_info, const struct ddsi_type *type_c)
{
  struct DDS_XTypes_TypeIdentifier type_id;
  struct DDS_XTypes_TypeObject type_obj;
  ddsi_xt_get_typeid_impl (&type_c->xt, &type_id, DDSI_TYPEID_KIND_MINIMAL);
  ddsi_xt_get_typeobject_kind_impl (&type_c->xt, &type_obj, DDSI_TYPEID_KIND_MINIMAL);
  if (!debug_typeinfo_minimal_contains (type_info, &type_id))
  {
    struct typelib_trace_typeid_str tistr;
    GVWARNING ("generated TypeInformation.minimal is missing locally generated minimal TypeObject %s\n",
               typelib_trace_make_typeid_str (&tistr, &type_id));
    assert (false);
  }
  if (type_id._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
    assert_scc_materialized (gv, ddsi_type_lookup_locked_impl (gv, &type_id), &type_id, "TypeInformation top-level generation");

  assert (type_obj._d == DDS_XTypes_EK_MINIMAL);
  debug_typeinfo_visit_minimal_typeobj_refs (gv, type_info, &type_obj._u.minimal);
  ddsi_typeobj_fini_impl (&type_obj);
  ddsi_typeid_fini_impl (&type_id);
}

static void debug_typeinfo_assert_minimal_closure_r (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id_c, const struct ddsi_typeinfo *type_info, struct ddsrt_hh *visited, bool include_self)
{
  const struct ddsi_type *type_c = ddsi_type_lookup_locked (gv, type_id_c);
  if (type_c == NULL)
  {
    struct typelib_trace_typeid_str tistr;
    GVWARNING ("generated TypeInformation dependency %s is not in the local type library\n",
               typelib_trace_make_typeid_str (&tistr, &type_id_c->x));
    assert (false);
    return;
  }
  if (ddsi_type_visit_seen (visited, type_c))
    return;

  struct ddsi_type_dep tmpl, *dep = &tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.src_type_id, type_id_c);
  while ((dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_treedef, &gv->typedeps, dep)) && ddsi_typeid_compare (type_id_c, &dep->src_type_id) == 0)
  {
    switch (ddsi_typeid_kind (&dep->dep_type_id))
    {
      case DDSI_TYPEID_KIND_PLAIN_COLLECTION_MINIMAL:
      case DDSI_TYPEID_KIND_PLAIN_COLLECTION_COMPLETE:
        debug_typeinfo_assert_minimal_closure_r (gv, &dep->dep_type_id, type_info, visited, false);
        break;

      case DDSI_TYPEID_KIND_MINIMAL:
      case DDSI_TYPEID_KIND_COMPLETE:
        debug_typeinfo_assert_minimal_closure_r (gv, &dep->dep_type_id, type_info, visited, true);
        break;

      case DDSI_TYPEID_KIND_FULLY_DESCRIPTIVE:
        break;

      case DDSI_TYPEID_KIND_INVALID:
        assert (false);
        break;
    }
  }
  ddsi_typeid_fini (&tmpl.src_type_id);

  if (include_self && type_id_kind_is_hash_dependency (ddsi_typeid_kind (type_id_c)))
    debug_typeinfo_assert_generated_minimal_typeobj (gv, type_info, type_c);
}

static void debug_typeinfo_assert_minimal_closure (struct ddsi_domaingv *gv, const struct ddsi_type *type_c, const struct ddsi_typeinfo *type_info)
{
  struct ddsrt_hh *visited = ddsi_type_visit_new ();
  debug_typeinfo_assert_generated_minimal_typeobj (gv, type_info, type_c);
  debug_typeinfo_assert_minimal_closure_r (gv, &type_c->xt.id, type_info, visited, false);
  ddsi_type_visit_free (visited);
}
#endif

static bool type_id_visit_seen (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id, struct ddsrt_hh *visited)
{
  const struct ddsi_type *type = ddsi_type_lookup_locked (gv, type_id);
  return type != NULL && ddsi_type_visit_seen (visited, type);
}

static uint32_t get_type_ndeps_hash_r (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id, struct ddsrt_hh *visited, bool include_self)
{
  if (type_id_visit_seen (gv, type_id, visited))
    return 0;

  uint32_t n_deps = include_self && type_id_kind_is_hash_dependency (ddsi_typeid_kind (type_id)) ? 1 : 0;
  struct ddsi_type_dep tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.src_type_id, type_id);

  struct ddsi_type_dep *dep = &tmpl;
  while ((dep = ddsrt_avl_lookup_succ (&ddsi_typedeps_treedef, &gv->typedeps, dep)) && ddsi_typeid_compare (type_id, &dep->src_type_id) == 0)
  {
    switch (ddsi_typeid_kind (&dep->dep_type_id))
    {
      case DDSI_TYPEID_KIND_PLAIN_COLLECTION_MINIMAL:
      case DDSI_TYPEID_KIND_PLAIN_COLLECTION_COMPLETE:
        n_deps += get_type_ndeps_hash_r (gv, &dep->dep_type_id, visited, false);
        break;

      case DDSI_TYPEID_KIND_MINIMAL:
      case DDSI_TYPEID_KIND_COMPLETE:
        n_deps += get_type_ndeps_hash_r (gv, &dep->dep_type_id, visited, true);
        break;

      case DDSI_TYPEID_KIND_FULLY_DESCRIPTIVE:
      case DDSI_TYPEID_KIND_INVALID:
        break;
    }
  }
  assert (n_deps <= INT32_MAX);
  ddsi_typeid_fini (&tmpl.src_type_id);
  return n_deps;
}

static dds_return_t add_type_info_hash_deps_r (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id_c, struct ddsi_typeinfo *type_info, struct ddsrt_hh *visited, bool include_self)
{
  // appends dependencies to type_info, on error leaves it in well-formed state containing fewer dependencies than exist
  dds_return_t ret = DDS_RETCODE_OK;
  if (type_id_visit_seen (gv, type_id_c, visited))
    return ret;

  struct ddsi_type_dep tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.src_type_id, type_id_c);

  struct ddsi_type_dep *dep_c = &tmpl;
  while (ret == DDS_RETCODE_OK
      && (dep_c = ddsrt_avl_lookup_succ (&ddsi_typedeps_treedef, &gv->typedeps, dep_c))
      && ddsi_typeid_compare (type_id_c, &dep_c->src_type_id) == 0)
  {
    /* XTypes spec 7.6.3.2.1: The TypeIdentifiers included in the TypeInformation shall include only direct HASH TypeIdentifiers,
     so we'll skip fully descriptive and indirect hash identifiers (kind DDSI_TYPEID_KIND_PLAIN_COLLECTION_MINIMAL and
     DDSI_TYPEID_KIND_PLAIN_COLLECTION_COMPLETE), but we need to include the element type (in case this is a HASH type) and its
     dependent types */
    switch (ddsi_typeid_kind (&dep_c->dep_type_id))
    {
      case DDSI_TYPEID_KIND_PLAIN_COLLECTION_MINIMAL:
      case DDSI_TYPEID_KIND_PLAIN_COLLECTION_COMPLETE:
        ret = add_type_info_hash_deps_r (gv, &dep_c->dep_type_id, type_info, visited, false);
        break;

      case DDSI_TYPEID_KIND_MINIMAL:
      case DDSI_TYPEID_KIND_COMPLETE:
        ret = add_type_info_hash_deps_r (gv, &dep_c->dep_type_id, type_info, visited, true);
        break;

      case DDSI_TYPEID_KIND_FULLY_DESCRIPTIVE:
        break;

      case DDSI_TYPEID_KIND_INVALID:
        ret = DDS_RETCODE_BAD_PARAMETER;
        break;
    }
  }
  ddsi_typeid_fini (&tmpl.src_type_id);
  if (ret == DDS_RETCODE_OK && include_self && type_id_kind_is_hash_dependency (ddsi_typeid_kind (type_id_c)))
    ret = ddsi_typeinfo_deps_append (gv, type_info, type_id_c);
  return ret;
}

dds_return_t ddsi_type_get_typeinfo_locked (struct ddsi_domaingv *gv, struct ddsi_type *type_c, struct ddsi_typeinfo *type_info, struct ddsi_type **type_m)
{
  dds_return_t ret;

  assert (ddsi_typeid_kind (&type_c->xt.id) == DDSI_TYPEID_KIND_COMPLETE);
  memset (type_info, 0, sizeof (*type_info));
  *type_m = NULL;
  if ((ret = ddsi_type_get_typeinfo_toplevel (gv, type_c, type_info, type_m)) != DDS_RETCODE_OK)
    return ret;

  struct ddsrt_hh *visited = ddsi_type_visit_new ();
  uint32_t n_deps = get_type_ndeps_hash_r (gv, &type_c->xt.id, visited, false);
  ddsi_type_visit_free (visited);
  if ((ret = ddsi_typeinfo_deps_init (type_info, n_deps)) != DDS_RETCODE_OK)
  {
    ddsi_typeinfo_deps_fini (type_info);
    goto err_toplevel;
  }

  visited = ddsi_type_visit_new ();
  ret = add_type_info_hash_deps_r (gv, &type_c->xt.id, type_info, visited, false);
  ddsi_type_visit_free (visited);
#ifndef NDEBUG
  if (ret == DDS_RETCODE_OK)
    debug_typeinfo_assert_minimal_closure (gv, type_c, type_info);
#endif
  if (ret != DDS_RETCODE_OK)
  {
    // release memory allocated for partial set of dependencies
    ddsi_typeinfo_deps_fini (type_info);
    goto err_toplevel;
  }
  return ret;

err_toplevel:
  ddsi_typeinfo_toplevel_fini (type_info);
  ddsi_type_unref_locked (gv, type_c);
  ddsi_type_unref_locked (gv, *type_m);
  *type_m = NULL;
  return ret;
}

dds_return_t ddsi_type_get_typeinfo_ser (struct ddsi_domaingv *gv, const struct ddsi_type *type_c, unsigned char **data, uint32_t *sz)
{
  dds_return_t ret;
  dds_ostreamLE_t os = { .x = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 } };
  struct ddsi_typeinfo type_info;
  struct ddsi_type *type_m;

  *data = NULL;
  *sz = 0;

  ddsrt_mutex_lock (&gv->typelib_lock);

  // non-const complete type ptr, refc is decreased after getting type-info
  struct ddsi_type *type_c_nc = (struct ddsi_type *) type_c;

  if ((ret = ddsi_type_get_typeinfo_locked (gv, type_c_nc, &type_info, &type_m)))
  {
    ddsrt_mutex_unlock (&gv->typelib_lock);
    goto err_typeinfo;
  }

  // ddsi_type_get_typeinfo_locked increases the complete and minimal type refcs
  ddsi_type_unref_locked (gv, type_c_nc);
  ddsi_type_unref_locked (gv, (struct ddsi_type *) type_m);
  ddsrt_mutex_unlock (&gv->typelib_lock);

  if ((ret = xcdr2_ser (&type_info.x, &DDS_XTypes_TypeInformation_cdrstream_desc, &os)) != DDS_RETCODE_OK)
    goto err_ser;
  *data = os.x.m_buffer;
  *sz = os.x.m_index;

err_ser:
  ddsi_typeinfo_fini (&type_info);
err_typeinfo:
  return ret;
}

static void typemap_add_type (struct ddsi_typemap *type_map, const struct ddsi_type *type_c)
{
  // if identical to one already present, ignore it
  for (uint32_t i = 0; i < type_map->x.identifier_complete_minimal._length; i++)
  {
    if (ddsi_typeid_compare_impl (&type_map->x.identifier_complete_minimal._buffer[i].type_identifier1, &type_c->xt.id.x) == 0)
      return;
  }

  uint32_t n = type_map->x.identifier_complete_minimal._length;
  type_map->x.identifier_complete_minimal._length++;
  type_map->x.identifier_complete_minimal._maximum++;

  type_map->x.identifier_object_pair_minimal._length++;
  type_map->x.identifier_object_pair_minimal._maximum++;
  ddsi_xt_get_typeobject_kind_impl (&type_c->xt, &type_map->x.identifier_object_pair_minimal._buffer[n].type_object, DDSI_TYPEID_KIND_MINIMAL);
  ddsi_xt_get_typeid_impl (&type_c->xt, &type_map->x.identifier_object_pair_minimal._buffer[n].type_identifier, DDSI_TYPEID_KIND_MINIMAL);
  ddsi_typeid_copy_impl (&type_map->x.identifier_complete_minimal._buffer[n].type_identifier2, &type_map->x.identifier_object_pair_minimal._buffer[n].type_identifier);

  type_map->x.identifier_object_pair_complete._length++;
  type_map->x.identifier_object_pair_complete._maximum++;
  ddsi_xt_get_typeobject_kind_impl (&type_c->xt, &type_map->x.identifier_object_pair_complete._buffer[n].type_object, DDSI_TYPEID_KIND_COMPLETE);
  ddsi_xt_get_typeid_impl (&type_c->xt, &type_map->x.identifier_object_pair_complete._buffer[n].type_identifier, DDSI_TYPEID_KIND_COMPLETE);
  ddsi_typeid_copy_impl (&type_map->x.identifier_complete_minimal._buffer[n].type_identifier1, &type_map->x.identifier_object_pair_complete._buffer[n].type_identifier);
}

static dds_return_t add_type_map_hash_deps_r (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id, struct ddsi_typemap *type_map, struct ddsrt_hh *visited, bool include_self)
{
  dds_return_t ret = DDS_RETCODE_OK;
  if (type_id_visit_seen (gv, type_id, visited))
    return ret;

  struct ddsi_type_dep tmpl, *dep_c = &tmpl;
  memset (&tmpl, 0, sizeof (tmpl));
  ddsi_typeid_copy (&tmpl.src_type_id, type_id);

  while (ret == DDS_RETCODE_OK
      && (dep_c = ddsrt_avl_lookup_succ (&ddsi_typedeps_treedef, &gv->typedeps, dep_c))
      && !ddsi_typeid_compare (type_id, &dep_c->src_type_id))
  {
    switch (ddsi_typeid_kind (&dep_c->dep_type_id))
    {
      case DDSI_TYPEID_KIND_PLAIN_COLLECTION_MINIMAL:
      case DDSI_TYPEID_KIND_PLAIN_COLLECTION_COMPLETE:
        ret = add_type_map_hash_deps_r (gv, &dep_c->dep_type_id, type_map, visited, false);
        break;

      case DDSI_TYPEID_KIND_MINIMAL:
      case DDSI_TYPEID_KIND_COMPLETE:
        ret = add_type_map_hash_deps_r (gv, &dep_c->dep_type_id, type_map, visited, true);
        break;

      case DDSI_TYPEID_KIND_FULLY_DESCRIPTIVE:
        break;

      case DDSI_TYPEID_KIND_INVALID:
        ret = DDS_RETCODE_BAD_PARAMETER;
        break;
    }
  }
  ddsi_typeid_fini (&tmpl.src_type_id);
  if (ret == DDS_RETCODE_OK && include_self && type_id_kind_is_hash_dependency (ddsi_typeid_kind (type_id)))
  {
    struct ddsi_type *type_c = ddsi_type_lookup_locked (gv, type_id);
    if (type_c == NULL)
      ret = DDS_RETCODE_ERROR;
    else
      typemap_add_type (type_map, type_c);
  }
  return ret;
}

static dds_return_t ddsi_type_get_typemap_locked (struct ddsi_domaingv *gv, const struct ddsi_type *type_c, struct ddsi_typemap *type_map)
{
  dds_return_t ret = DDS_RETCODE_OK;
  bool release_initialized = false;
  memset (type_map, 0, sizeof (*type_map));

  struct ddsrt_hh *visited = ddsi_type_visit_new ();
  uint32_t n_deps = get_type_ndeps_hash_r (gv, &type_c->xt.id, visited, false);
  ddsi_type_visit_free (visited);

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
  release_initialized = true;

  // add top-level type to typemap
  typemap_add_type (type_map, type_c);

  // add dependent types
  if (n_deps > 0)
  {
    visited = ddsi_type_visit_new ();
    ret = add_type_map_hash_deps_r (gv, &type_c->xt.id, type_map, visited, false);
    ddsi_type_visit_free (visited);
  }

err:
  if (ret != DDS_RETCODE_OK)
  {
    if (release_initialized)
      ddsi_typemap_fini (type_map);
    else
    {
      if (type_map->x.identifier_complete_minimal._buffer)
        ddsrt_free (type_map->x.identifier_complete_minimal._buffer);
      if (type_map->x.identifier_object_pair_minimal._buffer)
        ddsrt_free (type_map->x.identifier_object_pair_minimal._buffer);
      if (type_map->x.identifier_object_pair_complete._buffer)
        ddsrt_free (type_map->x.identifier_object_pair_complete._buffer);
    }
    memset (type_map, 0, sizeof (*type_map));
  }
  return ret;
}

dds_return_t ddsi_type_get_typemap_ser (struct ddsi_domaingv *gv, const struct ddsi_type *type, unsigned char **data, uint32_t *sz)
{
  dds_return_t ret;
  dds_ostreamLE_t os = { .x = { NULL, 0, 0, DDSI_RTPS_CDR_ENC_VERSION_2 } };
  struct ddsi_typemap type_map;

  *data = NULL;
  *sz = 0;

  ddsrt_mutex_lock (&gv->typelib_lock);
  ret = ddsi_type_get_typemap_locked (gv, type, &type_map);
  ddsrt_mutex_unlock (&gv->typelib_lock);
  if (ret != DDS_RETCODE_OK)
    goto err;

  ret = xcdr2_ser (&type_map.x, &DDS_XTypes_TypeMapping_cdrstream_desc, &os);
  ddsi_typemap_fini (&type_map);
  if (ret != DDS_RETCODE_OK)
    goto err;

  *data = os.x.m_buffer;
  *sz = os.x.m_index;
err:
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

static void ddsi_type_scc_free_locked (struct ddsi_domaingv *gv, struct ddsi_type_scc *scc)
{
  const uint32_t n_types = scc->n_types;
  struct ddsi_type **types = ddsrt_malloc (n_types * sizeof (*types));
  memcpy (types, scc->types, n_types * sizeof (*types));

  /* Same-SCC fields are non-owning back-pointers after activation.  Keep all
     member objects addressable until every member has been finalized. */
  scc->freeing = true;

  struct ddsi_type *generated_minimal_ref = scc->generated_minimal_ref;
  scc->generated_minimal_ref = NULL;
  assert (generated_minimal_ref == NULL || generated_minimal_ref->scc != scc);
  if (generated_minimal_ref != NULL)
    ddsi_type_unref_locked (gv, generated_minimal_ref);

  for (uint32_t n = 0; n < n_types; n++)
  {
    if (types[n] == NULL)
      continue;
    types[n]->freeing = true;
    if (!ddsi_typeid_is_none (&types[n]->xt.id) && ddsi_type_lookup_locked (gv, &types[n]->xt.id) == types[n])
      ddsrt_avl_delete (&ddsi_typelib_treedef, &gv->typelib, types[n]);
  }
  for (uint32_t n = 0; n < n_types; n++)
  {
    if (types[n] != NULL)
      ddsi_type_fini (types[n], false);
  }
  for (uint32_t n = 0; n < n_types; n++)
  {
    if (types[n] != NULL)
    {
      ddsi_type_scc_detach (types[n]);
      ddsrt_free (types[n]);
    }
  }
  ddsrt_free (types);
  ddsrt_free (scc->types);
  ddsrt_free (scc);
}

static void ddsi_type_scc_unref_locked (struct ddsi_domaingv *gv, struct ddsi_type_scc *scc)
{
  if (scc->freeing)
  {
    assert (scc->refc == 0);
    return;
  }
  assert (scc->refc > 0);
  if (--scc->refc == 0)
  {
    TYPELIB_TRACE (" scc %p refc 0 remove type component ", scc);
    ddsi_type_scc_free_locked (gv, scc);
  }
  else
    TYPELIB_TRACE (" scc %p refc %" PRIu32 " ", scc, scc->refc);
}

static void ddsi_type_unref_impl_locked (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  if (type->scc != NULL && type->scc->active)
  {
    ddsi_type_scc_unref_locked (gv, type->scc);
    return;
  }
  if (type->freeing)
  {
    /* Recursive dynamic types can contain non-owning back-pointers to a type
       currently being finalized.  The object is still valid until ddsi_type_free
       returns, but its reference count has already reached zero. */
    assert (type->refc == 0);
    return;
  }
  assert (type->refc > 0);
  if (--type->refc == 0)
  {
    type->freeing = true;
    TYPELIB_TRACE (" refc 0 remove type ");
    if (!ddsi_typeid_is_none (&type->xt.id) && ddsi_type_lookup_locked (gv, &type->xt.id) == type)
      ddsrt_avl_delete (&ddsi_typelib_treedef, &gv->typelib, type);
    ddsi_type_free (type);
  }
  else
    TYPELIB_TRACE (" refc %" PRIu32 " ", type->refc);
}

void ddsi_type_unreg_proxy (struct ddsi_domaingv *gv, struct ddsi_type *type, const ddsi_guid_t *proxy_guid)
{
  assert (proxy_guid);
  if (!type)
    return;
  ddsrt_mutex_lock (&gv->typelib_lock);
  TYPELIB_TRACE ("unreg proxy guid " PGUIDFMT " ddsi_type id %s\n", PGUID (*proxy_guid), typelib_trace_make_typeid_str (&tistr, &type->xt.id.x));
  ddsi_type_proxy_guid_list_remove (&type->proxy_guids, *proxy_guid, ddsi_type_proxy_guids_eq);
  ddsrt_mutex_unlock (&gv->typelib_lock);
}

void ddsi_type_unref_locked (struct ddsi_domaingv *gv, struct ddsi_type *type)
{
  if (type == NULL)
    return;
  TYPELIB_TRACE ("unref ddsi_type id %s", typelib_trace_make_typeid_str (&tistr, &type->xt.id.x));
  ddsi_type_unref_impl_locked (gv, type);
  TYPELIB_TRACE ("\n");
}

void ddsi_type_unref_dep_locked (struct ddsi_domaingv *gv, const struct ddsi_type *owner, struct ddsi_type *type)
{
  if (!ddsi_type_dep_is_active_scc_internal (owner, type))
    ddsi_type_unref_locked (gv, type);
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
      TYPELIB_TRACE ("unref ddsi_type id %s", typelib_trace_make_typeid_str (&tistr, &type->xt.id.x));
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

static void ddsi_type_get_gpe_matches_r (struct ddsi_domaingv *gv, const struct ddsi_type *type, struct ddsi_generic_proxy_endpoint ***gpe_match_upd, uint32_t *n_match_upd, struct ddsrt_hh *visited)
{
  if (ddsi_type_visit_seen (visited, type))
    return;

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
    if (dep_src_type != NULL)
      ddsi_type_get_gpe_matches_r (gv, dep_src_type, gpe_match_upd, n_match_upd, visited);
  }
  ddsi_typeid_fini (&tmpl.dep_type_id);
}

void ddsi_type_get_gpe_matches (struct ddsi_domaingv *gv, const struct ddsi_type *type, struct ddsi_generic_proxy_endpoint ***gpe_match_upd, uint32_t *n_match_upd)
{
  struct ddsrt_hh *visited = ddsi_type_visit_new ();
  ddsi_type_get_gpe_matches_r (gv, type, gpe_match_upd, n_match_upd, visited);
  ddsi_type_visit_free (visited);
}

bool ddsi_type_resolved_locked (struct ddsi_domaingv *gv, const struct ddsi_type *type, enum ddsi_type_include_deps resolved_kind)
{
  (void) gv;
  assert (resolved_kind == DDSI_TYPE_IGNORE_DEPS || resolved_kind == DDSI_TYPE_INCLUDE_DEPS);
  return type && (resolved_kind == DDSI_TYPE_INCLUDE_DEPS ? type->state == DDSI_TYPE_RESOLVED : type_state_has_valid_typeobj (type->state));
}

bool ddsi_type_resolved (struct ddsi_domaingv *gv, const struct ddsi_type *type, enum ddsi_type_include_deps resolved_kind)
{
  ddsrt_mutex_lock (&gv->typelib_lock);
  bool ret = ddsi_type_resolved_locked (gv, type, resolved_kind);
  ddsrt_mutex_unlock (&gv->typelib_lock);
  return ret;
}

static const char *ddsi_non_assignability_code_str (enum ddsi_non_assignability_code code)
{
  switch (code)
  {
    case DDSI_NONASSIGN_ASSIGNABLE: assert (0); break;
    case DDSI_NONASSIGN_TYPE_UNRESOLVED: return "type unresolved";
    case DDSI_NONASSIGN_INCOMPATIBLE_TYPE: return "incompatible type";
    case DDSI_NONASSIGN_DIFFERENT_EXTENSIBILITY: return "different extensibility";
    case DDSI_NONASSIGN_WR_TYPE_NOT_DELIMITED: return "wr type not delimited";
    case DDSI_NONASSIGN_NAME_HASH_DIFFERS: return "name hash differs for same member id";
    case DDSI_NONASSIGN_MEMBER_ID_DIFFERS: return "member ids differ for same name hash";
    case DDSI_NONASSIGN_MISSING_CASE: return "missing case/enum label";
    case DDSI_NONASSIGN_NUMBER_OF_MEMBERS: return "number of members/enum labels";
    case DDSI_NONASSIGN_KEY_DIFFERS: return "key annotation differs";
    case DDSI_NONASSIGN_NO_OVERLAP: return "no common members/labels";
    case DDSI_NONASSIGN_STRUCT_MUST_UNDERSTAND: return "must understand mismatch";
    case DDSI_NONASSIGN_STRUCT_OPTIONAL: return "optional mismatch";
    case DDSI_NONASSIGN_STRUCT_MEMBER_MISMATCH: return "member mismatch";
    case DDSI_NONASSIGN_KEY_INCOMPATIBLE: return "key incompatible";
    case DDSI_NONASSIGN_BOUND: return "incompatible bound";
    case DDSI_NONASSIGN_UNKNOWN: return "unknown";
  }
  return "(invalid code)";
};


bool ddsi_is_assignable_from (struct ddsi_domaingv *gv, const struct ddsi_type_pair *rd_type_pair, uint32_t rd_resolved, const struct ddsi_type_pair *wr_type_pair, uint32_t wr_resolved, const dds_type_consistency_enforcement_qospolicy_t *tce)
{
  if (!rd_type_pair || !wr_type_pair)
    return false;
  ddsrt_mutex_lock (&gv->typelib_lock);
  const struct xt_type
    *rd_xt = (rd_resolved == DDS_XTypes_EK_BOTH || rd_resolved == DDS_XTypes_EK_MINIMAL) ? &rd_type_pair->minimal->xt : &rd_type_pair->complete->xt,
    *wr_xt = (wr_resolved == DDS_XTypes_EK_BOTH || wr_resolved == DDS_XTypes_EK_MINIMAL) ? &wr_type_pair->minimal->xt : &wr_type_pair->complete->xt;
  struct ddsi_non_assignability_reason reason;
  bool assignable = ddsi_xt_is_assignable_from (gv, rd_xt, wr_xt, tce, &reason);
  ddsrt_mutex_unlock (&gv->typelib_lock);

  if (!assignable)
  {
    struct typelib_trace_typeid_str trdstr, twrstr;
    struct typelib_trace_typeid_str t1str, t2str;
    // not supposed to perform an assignability check while there are still unresolved types involved
    const uint32_t lc_cat = DDS_LC_DISCOVERY | (reason.code == DDSI_NONASSIGN_TYPE_UNRESOLVED ? DDS_LC_WARNING : 0);
    GVLOG (lc_cat, "assignability check failed: rd type %s wr type %s, t1=%s (%s) t2=%s (%s) id %"PRIu32": %s\n",
           typelib_trace_make_typeid_str (&trdstr, &rd_xt->id.x),
           typelib_trace_make_typeid_str (&twrstr, &wr_xt->id.x),
           reason.t1_id._d != DDS_XTypes_TK_NONE ? typelib_trace_make_typeid_str (&t1str, &reason.t1_id) : "(none)",
           reason.t1_typekind ? ddsi_typekind_descr (reason.t1_typekind) : "",
           reason.t2_id._d != DDS_XTypes_TK_NONE ? typelib_trace_make_typeid_str (&t2str, &reason.t2_id) : "(none)",
           reason.t2_typekind ? ddsi_typekind_descr (reason.t2_typekind) : "",
           reason.id, ddsi_non_assignability_code_str (reason.code));
  }
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

ddsi_typeinfo_t *ddsi_type_pair_get_typeinfo (struct ddsi_domaingv *gv, const struct ddsi_type_pair *type_pair)
{
  if (type_pair == NULL || type_pair->complete == NULL)
    return NULL;
  ddsi_typeinfo_t *type_info;
  if (!(type_info = ddsrt_malloc (sizeof (*type_info))))
    return NULL;

  struct ddsi_type *type_m;
  ddsrt_mutex_lock (&gv->typelib_lock);
  if (ddsi_type_get_typeinfo_locked (gv, type_pair->complete, type_info, &type_m) != DDS_RETCODE_OK)
  {
    ddsrt_free (type_info);
    type_info = NULL;
  }
  else
  {
    ddsi_type_unref_locked (gv, type_pair->complete);
    ddsi_type_unref_locked (gv, type_m);
  }
  ddsrt_mutex_unlock (&gv->typelib_lock);
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


#ifdef DDS_HAS_TYPELIB

static dds_return_t check_type_resolved_impl_locked (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id, dds_duration_t timeout, struct ddsi_type **type, enum ddsi_type_include_deps resolved_kind, bool *resolved)
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

static dds_return_t wait_for_type_resolved_impl_locked (struct ddsi_domaingv *gv, dds_duration_t timeout, const struct ddsi_type *type, enum ddsi_type_include_deps resolved_kind)
{
  const ddsrt_etime_t tnow = ddsrt_time_elapsed ();
  const ddsrt_etime_t abstimeout = {(DDS_INFINITY - timeout <= tnow.v) ? DDS_NEVER : (tnow.v + timeout)};
  while (!ddsi_type_resolved_locked (gv, type, resolved_kind))
  {
    if (!ddsrt_cond_etime_waituntil (&gv->typelib_resolved_cond, &gv->typelib_lock, abstimeout))
      return DDS_RETCODE_TIMEOUT;
  }
  ddsi_type_ref_locked (gv, NULL, type);
  return DDS_RETCODE_OK;
}

dds_return_t ddsi_wait_for_type_resolved (struct ddsi_domaingv *gv, const ddsi_typeid_t *type_id, dds_duration_t timeout, struct ddsi_type **type, enum ddsi_type_include_deps resolved_kind, ddsi_type_request_t request)
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

#ifdef DDS_HAS_TYPE_DISCOVERY
  // TODO: provide proxy pp guid to ddsi_tl_request_type so that request can be sent to a specific node
  if (request == DDSI_TYPE_SEND_REQUEST && !ddsi_tl_request_type (gv, type_id, NULL, resolved_kind))
    return DDS_RETCODE_PRECONDITION_NOT_MET;
#else
  assert (request == DDSI_TYPE_NO_REQUEST);
  (void) request;
#endif

  ddsrt_mutex_lock (&gv->typelib_lock);
  ret = wait_for_type_resolved_impl_locked (gv, timeout, *type, resolved_kind);
  ddsrt_mutex_unlock (&gv->typelib_lock);

  return ret;
}

#endif /* DDS_HAS_TYPELIB */
