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

#include <string.h>
#include <stdlib.h>
#include "dds/features.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_cdrstream.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_xt_impl.h"
#include "dds/ddsi/ddsi_xt_typemap.h"
#include "dds/ddsi/ddsi_typelookup.h"
#include "dds/ddsi/ddsi_typelib.h"
#include "dds/ddsc/dds_public_impl.h"

void ddsi_typeid_copy_impl (struct DDS_XTypes_TypeIdentifier *dst, const struct DDS_XTypes_TypeIdentifier *src)
{
  assert (src);
  assert (dst);
  dst->_d = src->_d;
  if (src->_d <= DDS_XTypes_TK_STRING16)
    return;
  switch (src->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING16_SMALL:
      dst->_u.string_sdefn.bound = src->_u.string_sdefn.bound;
      break;
    case DDS_XTypes_TI_STRING8_LARGE:
    case DDS_XTypes_TI_STRING16_LARGE:
      dst->_u.string_ldefn.bound = src->_u.string_ldefn.bound;
      break;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      dst->_u.seq_sdefn.header = src->_u.seq_sdefn.header;
      dst->_u.seq_sdefn.bound = src->_u.seq_sdefn.bound;
      dst->_u.seq_sdefn.element_identifier = ddsi_typeid_dup_impl (src->_u.seq_sdefn.element_identifier);
      break;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      dst->_u.seq_ldefn.header = src->_u.seq_ldefn.header;
      dst->_u.seq_ldefn.bound = src->_u.seq_ldefn.bound;
      dst->_u.seq_ldefn.element_identifier = ddsi_typeid_dup_impl (src->_u.seq_ldefn.element_identifier);
      break;
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
      dst->_u.array_sdefn.header = src->_u.array_sdefn.header;
      dst->_u.array_sdefn.array_bound_seq._length = dst->_u.array_sdefn.array_bound_seq._maximum = src->_u.array_sdefn.array_bound_seq._length;
      if (src->_u.array_sdefn.array_bound_seq._length > 0)
      {
        dst->_u.array_sdefn.array_bound_seq._buffer = ddsrt_memdup (src->_u.array_sdefn.array_bound_seq._buffer, src->_u.array_sdefn.array_bound_seq._length * sizeof (*src->_u.array_sdefn.array_bound_seq._buffer));
        dst->_u.array_sdefn.array_bound_seq._release = true;
      }
      else
        dst->_u.array_sdefn.array_bound_seq._release = false;
      dst->_u.array_sdefn.element_identifier = ddsi_typeid_dup_impl (src->_u.array_sdefn.element_identifier);
      break;
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      dst->_u.array_ldefn.header = src->_u.array_ldefn.header;
      dst->_u.array_ldefn.array_bound_seq._length = dst->_u.array_ldefn.array_bound_seq._maximum = src->_u.array_ldefn.array_bound_seq._length;
      if (src->_u.array_ldefn.array_bound_seq._length > 0)
      {
        dst->_u.array_ldefn.array_bound_seq._buffer = ddsrt_memdup (src->_u.array_ldefn.array_bound_seq._buffer, src->_u.array_ldefn.array_bound_seq._length * sizeof (*src->_u.array_ldefn.array_bound_seq._buffer));
        dst->_u.array_ldefn.array_bound_seq._release = true;
      }
      else
        dst->_u.array_ldefn.array_bound_seq._release = false;
      dst->_u.array_ldefn.element_identifier = ddsi_typeid_dup_impl (src->_u.array_ldefn.element_identifier);
      break;
    case DDS_XTypes_TI_PLAIN_MAP_SMALL:
      dst->_u.map_sdefn.header = src->_u.map_sdefn.header;
      dst->_u.map_sdefn.bound = src->_u.map_sdefn.bound;
      dst->_u.map_sdefn.element_identifier = ddsi_typeid_dup_impl (src->_u.map_sdefn.element_identifier);
      dst->_u.map_sdefn.key_flags = src->_u.map_sdefn.key_flags;
      dst->_u.map_sdefn.key_identifier = ddsi_typeid_dup_impl (src->_u.map_sdefn.key_identifier);
      break;
    case DDS_XTypes_TI_PLAIN_MAP_LARGE:
      dst->_u.map_ldefn.header = src->_u.map_ldefn.header;
      dst->_u.map_ldefn.bound = src->_u.map_ldefn.bound;
      dst->_u.map_ldefn.element_identifier = ddsi_typeid_dup_impl (src->_u.map_ldefn.element_identifier);
      dst->_u.map_ldefn.key_flags = src->_u.map_ldefn.key_flags;
      dst->_u.map_ldefn.key_identifier = ddsi_typeid_dup_impl (src->_u.map_ldefn.key_identifier);
      break;
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT:
      dst->_u.sc_component_id.sc_component_id = src->_u.sc_component_id.sc_component_id;
      dst->_u.sc_component_id.scc_length = src->_u.sc_component_id.scc_length;
      dst->_u.sc_component_id.scc_index = src->_u.sc_component_id.scc_index;
      break;
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_EK_MINIMAL:
      memcpy (dst->_u.equivalence_hash, src->_u.equivalence_hash, sizeof (dst->_u.equivalence_hash));
      break;
    default:
      dst->_d = DDS_XTypes_TK_NONE;
      break;
  }
}

void ddsi_typeid_copy (ddsi_typeid_t *dst, const ddsi_typeid_t *src)
{
  ddsi_typeid_copy_impl (&dst->x, &src->x);
}

void ddsi_typeid_copy_to_impl (struct DDS_XTypes_TypeIdentifier *dst, const ddsi_typeid_t *src)
{
  ddsi_typeid_copy_impl (dst, &src->x);
}

struct DDS_XTypes_TypeIdentifier * ddsi_typeid_dup_impl (const struct DDS_XTypes_TypeIdentifier *src)
{
  if (ddsi_typeid_is_none_impl (src))
    return NULL;
  struct DDS_XTypes_TypeIdentifier *tid = ddsrt_malloc (sizeof (*tid));
  ddsi_typeid_copy_impl (tid, src);
  return tid;
}

ddsi_typeid_t * ddsi_typeid_dup_from_impl (const struct DDS_XTypes_TypeIdentifier *src)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_typeid, x) == 0);
  return (ddsi_typeid_t *) ddsi_typeid_dup_impl (src);
}

ddsi_typeid_t * ddsi_typeid_dup (const ddsi_typeid_t *src)
{
  DDSRT_STATIC_ASSERT (offsetof (struct ddsi_typeid, x) == 0);
  return (ddsi_typeid_t *) ddsi_typeid_dup_impl (&src->x);
}

const char * ddsi_typekind_descr (unsigned char disc)
{
  switch (disc)
  {
    case DDS_XTypes_EK_MINIMAL: return "MINIMAL";
    case DDS_XTypes_EK_COMPLETE: return "COMPLETE";
    case DDS_XTypes_TK_NONE: return "NONE";
    case DDS_XTypes_TK_BOOLEAN: return "BOOLEAN";
    case DDS_XTypes_TK_BYTE: return "BYTE";
    case DDS_XTypes_TK_INT16: return "INT16";
    case DDS_XTypes_TK_INT32: return "INT32";
    case DDS_XTypes_TK_INT64: return "INT64";
    case DDS_XTypes_TK_UINT16: return "UINT16";
    case DDS_XTypes_TK_UINT32: return "UINT32";
    case DDS_XTypes_TK_UINT64: return "UINT64";
    case DDS_XTypes_TK_FLOAT32: return "FLOAT32";
    case DDS_XTypes_TK_FLOAT64: return "FLOAT64";
    case DDS_XTypes_TK_FLOAT128: return "FLOAT128";
    case DDS_XTypes_TK_CHAR8: return "CHAR";
    case DDS_XTypes_TK_CHAR16: return "CHAR16";
    case DDS_XTypes_TK_STRING8: return "STRING8";
    case DDS_XTypes_TK_STRING16: return "STRING16";
    case DDS_XTypes_TK_ALIAS: return "ALIAS";
    case DDS_XTypes_TK_ENUM: return "ENUM";
    case DDS_XTypes_TK_BITMASK: return "BITMASK";
    case DDS_XTypes_TK_ANNOTATION: return "ANNOTATION";
    case DDS_XTypes_TK_STRUCTURE: return "STRUCTURE";
    case DDS_XTypes_TK_UNION: return "UNION";
    case DDS_XTypes_TK_BITSET: return "BITSET";
    case DDS_XTypes_TK_SEQUENCE: return "SEQUENCE";
    case DDS_XTypes_TK_ARRAY: return "ARRAY";
    case DDS_XTypes_TK_MAP: return "MAP";
    case DDS_XTypes_TI_STRING8_SMALL: return "STRING8_SMALL";
    case DDS_XTypes_TI_STRING8_LARGE: return "STRING8_LARGE";
    case DDS_XTypes_TI_STRING16_SMALL: return "STRING16_SMALL";
    case DDS_XTypes_TI_STRING16_LARGE: return "STRING16_LARGE";
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL: return "PLAIN_SEQUENCE_SMALL";
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE: return "PLAIN_SEQUENCE_LARGE";
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL: return "PLAIN_ARRAY_SMALL";
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE: return "PLAIN_ARRAY_LARGE";
    case DDS_XTypes_TI_PLAIN_MAP_SMALL: return "PLAIN_MAP_SMALL";
    case DDS_XTypes_TI_PLAIN_MAP_LARGE: return "PLAIN_MAP_LARGE";
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT: return "STRONGLY_CONNECTED_COMPONENT";
    default: return "INVALID";
  }
}

static int plain_collection_header_compare (struct DDS_XTypes_PlainCollectionHeader a, struct DDS_XTypes_PlainCollectionHeader b)
{
  if (a.equiv_kind != b.equiv_kind)
    return a.equiv_kind > b.equiv_kind ? 1 : -1;
  if (a.element_flags != b.element_flags)
    return a.element_flags > b.element_flags ? 1 : -1;
  return 0;
}

static int equivalence_hash_compare (const DDS_XTypes_EquivalenceHash a, const DDS_XTypes_EquivalenceHash b)
{
  return memcmp (a, b, sizeof (DDS_XTypes_EquivalenceHash));
}

static int type_object_hashid_compare (struct DDS_XTypes_TypeObjectHashId a, struct DDS_XTypes_TypeObjectHashId b)
{
  if (a._d != b._d)
    return a._d > b._d ? 1 : -1;
  return equivalence_hash_compare (a._u.hash, b._u.hash);
}

static int strongly_connected_component_id_compare (struct DDS_XTypes_StronglyConnectedComponentId a, struct DDS_XTypes_StronglyConnectedComponentId b)
{
  if (a.scc_length != b.scc_length)
    return a.scc_length > b.scc_length ? 1 : -1;
  if (a.scc_index != b.scc_index)
    return a.scc_index > b.scc_index ? 1 : -1;
  return type_object_hashid_compare (a.sc_component_id, b.sc_component_id);
}

static bool type_id_with_size_equal (const struct DDS_XTypes_TypeIdentifierWithSize *a, const struct DDS_XTypes_TypeIdentifierWithSize *b)
{
  return a->typeobject_serialized_size == b->typeobject_serialized_size && !ddsi_typeid_compare_impl (&a->type_id, &b->type_id);
}

static bool type_id_with_sizeseq_equal (const struct dds_sequence_DDS_XTypes_TypeIdentifierWithSize *a, const struct dds_sequence_DDS_XTypes_TypeIdentifierWithSize *b)
{
    if (a->_length != b->_length)
      return false;
    for (uint32_t n = 0; n < a->_length; n++)
      if (!type_id_with_size_equal (&a->_buffer[n], &b->_buffer[n]))
        return false;
    return true;
}

bool ddsi_type_id_with_deps_equal (const struct DDS_XTypes_TypeIdentifierWithDependencies *a, const struct DDS_XTypes_TypeIdentifierWithDependencies *b)
{
  return type_id_with_size_equal (&a->typeid_with_size, &b->typeid_with_size)
    && a->dependent_typeid_count == b->dependent_typeid_count
    && type_id_with_sizeseq_equal (&a->dependent_typeids, &b->dependent_typeids);
}

int ddsi_typeid_compare_impl (const struct DDS_XTypes_TypeIdentifier *a, const struct DDS_XTypes_TypeIdentifier *b)
{
  int r;
  if (a == NULL && b == NULL)
    return 0;
  if (a == NULL || b == NULL)
    return a > b ? 1 : -1;
  if (a->_d != b->_d)
    return a->_d > b->_d ? 1 : -1;
  if (a->_d <= DDS_XTypes_TK_STRING16)
    return 0;
  switch (a->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING16_SMALL:
      if (a->_u.string_sdefn.bound != b->_u.string_sdefn.bound)
        return a->_u.string_sdefn.bound > b->_u.string_sdefn.bound ? 1 : -1;
      return 0;
    case DDS_XTypes_TI_STRING8_LARGE:
    case DDS_XTypes_TI_STRING16_LARGE:
      if (a->_u.string_ldefn.bound != b->_u.string_ldefn.bound)
        return a->_u.string_ldefn.bound > b->_u.string_ldefn.bound ? 1 : -1;
      return 0;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      if ((r = plain_collection_header_compare (a->_u.seq_sdefn.header, b->_u.seq_sdefn.header)) != 0)
        return r;
      if ((r = ddsi_typeid_compare_impl (a->_u.seq_sdefn.element_identifier, b->_u.seq_sdefn.element_identifier)) != 0)
        return r;
      if (a->_u.seq_sdefn.bound != b->_u.seq_sdefn.bound)
        return a->_u.seq_sdefn.bound > b->_u.seq_sdefn.bound ? 1 : -1;
      return 0;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      if ((r = plain_collection_header_compare (a->_u.seq_ldefn.header, b->_u.seq_ldefn.header)) != 0)
        return r;
      if ((r = ddsi_typeid_compare_impl (a->_u.seq_ldefn.element_identifier, b->_u.seq_ldefn.element_identifier)) != 0)
        return r;
      if (a->_u.seq_ldefn.bound != b->_u.seq_ldefn.bound)
        return a->_u.seq_ldefn.bound > b->_u.seq_ldefn.bound ? 1 : -1;
      return 0;
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
      if ((r = plain_collection_header_compare (a->_u.array_sdefn.header, b->_u.array_sdefn.header)) != 0)
        return r;
      if (a->_u.array_sdefn.array_bound_seq._length != b->_u.array_sdefn.array_bound_seq._length)
        return a->_u.array_sdefn.array_bound_seq._length > b->_u.array_sdefn.array_bound_seq._length ? 1 : -1;
      if (a->_u.array_sdefn.array_bound_seq._length > 0)
        if ((r = memcmp (a->_u.array_sdefn.array_bound_seq._buffer, b->_u.array_sdefn.array_bound_seq._buffer,
                          a->_u.array_sdefn.array_bound_seq._length * sizeof (*a->_u.array_sdefn.array_bound_seq._buffer))) != 0)
          return r;
      return ddsi_typeid_compare_impl (a->_u.array_sdefn.element_identifier, b->_u.array_sdefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      if ((r = plain_collection_header_compare (a->_u.array_ldefn.header, b->_u.array_ldefn.header)) != 0)
        return r;
      if (a->_u.array_ldefn.array_bound_seq._length != b->_u.array_ldefn.array_bound_seq._length)
        return a->_u.array_ldefn.array_bound_seq._length > b->_u.array_ldefn.array_bound_seq._length ? 1 : -1;
      if (a->_u.array_ldefn.array_bound_seq._length > 0)
        if ((r = memcmp (a->_u.array_ldefn.array_bound_seq._buffer, b->_u.array_ldefn.array_bound_seq._buffer,
                          a->_u.array_ldefn.array_bound_seq._length * sizeof (*a->_u.array_ldefn.array_bound_seq._buffer))) != 0)
          return r;
      return ddsi_typeid_compare_impl (a->_u.array_ldefn.element_identifier, b->_u.array_ldefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_MAP_SMALL:
      if ((r = plain_collection_header_compare (a->_u.map_sdefn.header, b->_u.map_sdefn.header)) != 0)
        return r;
      if (a->_u.map_sdefn.bound != b->_u.map_sdefn.bound)
        return a->_u.map_sdefn.bound > b->_u.map_sdefn.bound ? 1 : -1;
      if ((r = ddsi_typeid_compare_impl (a->_u.map_sdefn.element_identifier, b->_u.map_sdefn.element_identifier)) != 0)
        return r;
      if (a->_u.map_sdefn.key_flags != b->_u.map_sdefn.key_flags)
        return a->_u.map_sdefn.key_flags != b->_u.map_sdefn.key_flags ? 1 : -1;
      return ddsi_typeid_compare_impl (a->_u.map_sdefn.key_identifier, b->_u.map_sdefn.key_identifier);
    case DDS_XTypes_TI_PLAIN_MAP_LARGE:
      if ((r = plain_collection_header_compare (a->_u.map_ldefn.header, b->_u.map_ldefn.header)) != 0)
        return r;
      if (a->_u.map_ldefn.bound != b->_u.map_ldefn.bound)
        return a->_u.map_ldefn.bound > b->_u.map_ldefn.bound ? 1 : -1;
      if ((r = ddsi_typeid_compare_impl (a->_u.map_ldefn.element_identifier, b->_u.map_ldefn.element_identifier)) != 0)
        return r;
      if (a->_u.map_ldefn.key_flags != b->_u.map_ldefn.key_flags)
        return a->_u.map_ldefn.key_flags > b->_u.map_ldefn.key_flags ? 1 : -1;
      return ddsi_typeid_compare_impl (a->_u.map_ldefn.key_identifier, b->_u.map_ldefn.key_identifier);
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT:
      return strongly_connected_component_id_compare (a->_u.sc_component_id, b->_u.sc_component_id);
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_EK_MINIMAL:
      return equivalence_hash_compare (a->_u.equivalence_hash, b->_u.equivalence_hash);
    default:
      assert (false);
      return 1;
  }
}

int ddsi_typeid_compare (const ddsi_typeid_t *a, const ddsi_typeid_t *b)
{
  return ddsi_typeid_compare_impl (&a->x, &b->x);
}

void ddsi_typeid_ser (const ddsi_typeid_t *type_id, unsigned char **buf, uint32_t *sz)
{
  dds_ostream_t os = { .m_buffer = NULL, .m_index = 0, .m_size = 0, .m_xcdr_version = CDR_ENC_VERSION_2 };
  dds_stream_writeLE ((dds_ostreamLE_t *) &os, (const void *) type_id, DDS_XTypes_TypeIdentifier_desc.m_ops);
  *buf = os.m_buffer;
  *sz = os.m_index;
}

void ddsi_typeid_fini_impl (struct DDS_XTypes_TypeIdentifier *type_id)
{
  dds_stream_free_sample (type_id, DDS_XTypes_TypeIdentifier_desc.m_ops);
}

void ddsi_typeid_fini (ddsi_typeid_t *type_id)
{
  ddsi_typeid_fini_impl (&type_id->x);
}

bool ddsi_typeid_is_none_impl (const struct DDS_XTypes_TypeIdentifier *type_id)
{
  return type_id == NULL || type_id->_d == DDS_XTypes_TK_NONE;
}

bool ddsi_typeid_is_none (const ddsi_typeid_t *type_id)
{
  return type_id == NULL || ddsi_typeid_is_none_impl (&type_id->x);
}

bool ddsi_typeid_is_hash_impl (const struct DDS_XTypes_TypeIdentifier *type_id)
{
  return ddsi_typeid_is_minimal_impl (type_id) || ddsi_typeid_is_complete_impl (type_id);
}

bool ddsi_typeid_is_hash (const ddsi_typeid_t *type_id)
{
  return type_id != NULL && ddsi_typeid_is_hash_impl (&type_id->x);
}

bool ddsi_typeid_is_minimal_impl (const struct DDS_XTypes_TypeIdentifier *type_id)
{
  return type_id != NULL && type_id->_d == DDS_XTypes_EK_MINIMAL;
}

bool ddsi_typeid_is_minimal (const ddsi_typeid_t *type_id)
{
  return type_id != NULL && ddsi_typeid_is_minimal_impl (&type_id->x);
}

bool ddsi_typeid_is_complete_impl (const struct DDS_XTypes_TypeIdentifier *type_id)
{
  return type_id != NULL && type_id->_d == DDS_XTypes_EK_COMPLETE;
}

bool ddsi_typeid_is_complete (const ddsi_typeid_t *type_id)
{
  return type_id != NULL && ddsi_typeid_is_complete_impl (&type_id->x);
}

void ddsi_typeid_get_equivalence_hash (const ddsi_typeid_t *type_id, DDS_XTypes_EquivalenceHash *hash)
{
  assert (ddsi_typeid_is_hash (type_id));
  memcpy (hash, type_id->x._u.equivalence_hash, sizeof (*hash));
}

void ddsi_typeobj_fini_impl (struct DDS_XTypes_TypeObject *typeobj)
{
  dds_stream_free_sample (typeobj, DDS_XTypes_TypeObject_desc.m_ops);
}

void ddsi_typeobj_fini (ddsi_typeobj_t *typeobj)
{
  ddsi_typeobj_fini_impl (&typeobj->x);
}

static void xt_collection_common_init (struct xt_collection_common *xtcc, const struct DDS_XTypes_PlainCollectionHeader *hdr)
{
  xtcc->ek = hdr->equiv_kind;
  xtcc->element_flags = hdr->element_flags;
}

static void xt_sbounds_to_lbounds (struct DDS_XTypes_LBoundSeq *lb, const struct DDS_XTypes_SBoundSeq *sb)
{
  lb->_length = sb->_length;
  lb->_buffer = ddsrt_malloc (sb->_length * sizeof (*lb->_buffer));
  for (uint32_t n = 0; n < sb->_length; n++)
    lb->_buffer[n] = (DDS_XTypes_LBound) sb->_buffer[n];
}

static void xt_lbounds_dup (struct DDS_XTypes_LBoundSeq *dst, const struct DDS_XTypes_LBoundSeq *src)
{
  dst->_length = src->_length;
  dst->_buffer = ddsrt_memdup (src->_buffer, dst->_length * sizeof (*dst->_buffer));
}

static void DDS_XTypes_AppliedBuiltinMemberAnnotations_copy (struct DDS_XTypes_AppliedBuiltinMemberAnnotations *dst, const struct DDS_XTypes_AppliedBuiltinMemberAnnotations *src);
static void DDS_XTypes_AppliedAnnotationSeq_copy (struct DDS_XTypes_AppliedAnnotationSeq *dst, const struct DDS_XTypes_AppliedAnnotationSeq *src);
static void set_member_detail (struct xt_member_detail *dst, const DDS_XTypes_CompleteMemberDetail *src)
{
  ddsrt_strlcpy (dst->name, src->name, sizeof (dst->name));
  if (src->ann_builtin) {
    dst->annotations.ann_builtin = ddsrt_calloc(1, sizeof(struct DDS_XTypes_AppliedBuiltinMemberAnnotations));
    DDS_XTypes_AppliedBuiltinMemberAnnotations_copy (dst->annotations.ann_builtin, src->ann_builtin);
  } else {
    dst->annotations.ann_builtin = NULL;
  }

  if (src->ann_custom) {
    dst->annotations.ann_custom = ddsrt_calloc(1, sizeof(DDS_XTypes_AppliedAnnotationSeq));
    DDS_XTypes_AppliedAnnotationSeq_copy (dst->annotations.ann_custom, src->ann_custom);
  } else {
    dst->annotations.ann_custom = NULL;
  }
}

static bool xt_is_unresolved (const struct xt_type *t)
{
  return (t->kind == DDSI_TYPEID_KIND_MINIMAL || t->kind == DDSI_TYPEID_KIND_COMPLETE) && t->_d == DDS_XTypes_TK_NONE;
}

static dds_return_t add_minimal_typeobj (struct ddsi_domaingv *gv, struct xt_type *xt, const struct DDS_XTypes_TypeObject *to)
{
  const struct DDS_XTypes_MinimalTypeObject *mto = &to->_u.minimal;
  dds_return_t ret = DDS_RETCODE_OK;
  xt->has_obj = 1;
  if (!xt->_d)
    xt->_d = mto->_d;
  else if (xt->_d != mto->_d)
  {
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto err_tk;
  }
  switch (mto->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      xt->_u.alias.related_type = ddsi_type_ref_id_locked_impl (gv, &mto->_u.alias_type.body.common.related_type);
      xt->_u.alias.related_flags = mto->_u.alias_type.body.common.related_flags;
      break;
    case DDS_XTypes_TK_ANNOTATION:
      ret = DDS_RETCODE_UNSUPPORTED; /* FIXME: not implemented */
      goto err_tk;
    case DDS_XTypes_TK_STRUCTURE:
      xt->_u.structure.flags = mto->_u.struct_type.struct_flags;
      if (mto->_u.struct_type.header.base_type._d)
        xt->_u.structure.base_type = ddsi_type_ref_id_locked_impl (gv, &mto->_u.struct_type.header.base_type);
      else
        xt->_u.structure.base_type = NULL;
      xt->_u.structure.members.length = mto->_u.struct_type.member_seq._length;
      xt->_u.structure.members.seq = ddsrt_calloc (xt->_u.structure.members.length, sizeof (*xt->_u.structure.members.seq));
      for (uint32_t n = 0; n < xt->_u.structure.members.length; n++)
      {
        xt->_u.structure.members.seq[n].id = mto->_u.struct_type.member_seq._buffer[n].common.member_id;
        xt->_u.structure.members.seq[n].flags = mto->_u.struct_type.member_seq._buffer[n].common.member_flags;
        xt->_u.structure.members.seq[n].type = ddsi_type_ref_id_locked_impl (gv, &mto->_u.struct_type.member_seq._buffer[n].common.member_type_id);
        memcpy (xt->_u.structure.members.seq[n].detail.name_hash, mto->_u.struct_type.member_seq._buffer[n].detail.name_hash,
          sizeof (xt->_u.structure.members.seq[n].detail.name_hash));
      }
      break;
    case DDS_XTypes_TK_UNION:
      xt->_u.union_type.flags = mto->_u.union_type.union_flags;
      xt->_u.union_type.disc_type = ddsi_type_ref_id_locked_impl (gv, &mto->_u.union_type.discriminator.common.type_id);
      xt->_u.union_type.disc_flags = mto->_u.union_type.discriminator.common.member_flags;
      xt->_u.union_type.members.length = mto->_u.union_type.member_seq._length;
      xt->_u.union_type.members.seq = ddsrt_calloc (xt->_u.union_type.members.length, sizeof (*xt->_u.union_type.members.seq));
      for (uint32_t n = 0; n < xt->_u.union_type.members.length; n++)
      {
        xt->_u.union_type.members.seq[n].id = mto->_u.union_type.member_seq._buffer[n].common.member_id;
        xt->_u.union_type.members.seq[n].flags = mto->_u.union_type.member_seq._buffer[n].common.member_flags;
        xt->_u.union_type.members.seq[n].type = ddsi_type_ref_id_locked_impl (gv, &mto->_u.union_type.member_seq._buffer[n].common.type_id);
        xt->_u.union_type.members.seq[n].label_seq._length = mto->_u.union_type.member_seq._buffer[n].common.label_seq._length;
        xt->_u.union_type.members.seq[n].label_seq._buffer = ddsrt_memdup (mto->_u.union_type.member_seq._buffer[n].common.label_seq._buffer,
          mto->_u.union_type.member_seq._buffer[n].common.label_seq._length * sizeof (*mto->_u.union_type.member_seq._buffer[n].common.label_seq._buffer));
        memcpy (xt->_u.union_type.members.seq[n].detail.name_hash, mto->_u.union_type.member_seq._buffer[n].detail.name_hash,
          sizeof (xt->_u.union_type.members.seq[n].detail.name_hash));
      }
      break;
    case DDS_XTypes_TK_BITSET:
      xt->_u.bitset.fields.length = mto->_u.bitset_type.field_seq._length;
      xt->_u.bitset.fields.seq = ddsrt_calloc (xt->_u.bitset.fields.length, sizeof (*xt->_u.bitset.fields.seq));
      for (uint32_t n = 0; n < xt->_u.bitset.fields.length; n++)
      {
        xt->_u.bitset.fields.seq[n].position = mto->_u.bitset_type.field_seq._buffer[n].common.position;
        xt->_u.bitset.fields.seq[n].flags = mto->_u.bitset_type.field_seq._buffer[n].common.flags;
        xt->_u.bitset.fields.seq[n].bitcount = mto->_u.bitset_type.field_seq._buffer[n].common.bitcount;
        xt->_u.bitset.fields.seq[n].holder_type = mto->_u.bitset_type.field_seq._buffer[n].common.holder_type;
        memcpy (xt->_u.bitset.fields.seq[n].detail.name_hash, mto->_u.bitset_type.field_seq._buffer[n].name_hash,
          sizeof (xt->_u.bitset.fields.seq[n].detail.name_hash));
      }
      break;
    case DDS_XTypes_TK_SEQUENCE:
      xt->_u.seq.c.element_type = ddsi_type_ref_id_locked_impl (gv, &mto->_u.sequence_type.element.common.type);
      xt->_u.seq.c.element_flags = mto->_u.sequence_type.element.common.element_flags;
      xt->_u.seq.bound = mto->_u.sequence_type.header.common.bound;
      break;
    case DDS_XTypes_TK_ARRAY:
      xt->_u.array.c.element_type = ddsi_type_ref_id_locked_impl (gv, &mto->_u.array_type.element.common.type);
      xt->_u.array.c.element_flags = mto->_u.array_type.element.common.element_flags;
      xt_lbounds_dup (&xt->_u.array.bounds, &mto->_u.array_type.header.common.bound_seq);
      break;
    case DDS_XTypes_TK_MAP:
      xt->_u.map.c.element_type = ddsi_type_ref_id_locked_impl (gv, &mto->_u.map_type.element.common.type);
      xt->_u.map.c.element_flags = mto->_u.map_type.element.common.element_flags;
      xt->_u.map.key_type = ddsi_type_ref_id_locked_impl (gv, &mto->_u.map_type.key.common.type);
      xt->_u.map.bound = mto->_u.map_type.header.common.bound;
      break;
    case DDS_XTypes_TK_ENUM:
      xt->_u.enum_type.flags = mto->_u.enumerated_type.enum_flags;
      xt->_u.enum_type.bit_bound = mto->_u.enumerated_type.header.common.bit_bound;
      xt->_u.enum_type.literals.length = mto->_u.enumerated_type.literal_seq._length;
      xt->_u.enum_type.literals.seq = ddsrt_calloc (xt->_u.enum_type.literals.length, sizeof (*xt->_u.enum_type.literals.seq));
      for (uint32_t n = 0; n < xt->_u.enum_type.literals.length; n++)
      {
        xt->_u.enum_type.literals.seq[n].value = mto->_u.enumerated_type.literal_seq._buffer[n].common.value;
        xt->_u.enum_type.literals.seq[n].flags = mto->_u.enumerated_type.literal_seq._buffer[n].common.flags;
        memcpy (xt->_u.enum_type.literals.seq[n].detail.name_hash, mto->_u.enumerated_type.literal_seq._buffer[n].detail.name_hash,
          sizeof (xt->_u.enum_type.literals.seq[n].detail.name_hash));
      }
      break;
    case DDS_XTypes_TK_BITMASK:
      xt->_u.bitmask.flags = mto->_u.bitmask_type.bitmask_flags;
      xt->_u.bitmask.bit_bound = mto->_u.bitmask_type.header.common.bit_bound;
      xt->_u.bitmask.bitflags.length = mto->_u.bitmask_type.flag_seq._length;
      xt->_u.bitmask.bitflags.seq = ddsrt_calloc (xt->_u.bitmask.bitflags.length, sizeof (*xt->_u.bitmask.bitflags.seq));
      for (uint32_t n = 0; n < xt->_u.bitmask.bitflags.length; n++)
      {
        xt->_u.bitmask.bitflags.seq[n].position = mto->_u.bitmask_type.flag_seq._buffer[n].common.position;
        xt->_u.bitmask.bitflags.seq[n].flags = mto->_u.bitmask_type.flag_seq._buffer[n].common.flags;
        memcpy (xt->_u.bitmask.bitflags.seq[n].detail.name_hash, mto->_u.bitmask_type.flag_seq._buffer[n].detail.name_hash,
          sizeof (xt->_u.bitmask.bitflags.seq[n].detail.name_hash));
      }
      break;
    default:
      ret = DDS_RETCODE_UNSUPPORTED; /* not supported */
      goto err_tk;
  }
  return ret;

err_tk:
  xt->has_obj = 0;
  xt->_d = DDS_XTypes_TK_NONE;
  return ret;
}

static dds_return_t add_complete_typeobj (struct ddsi_domaingv *gv, struct xt_type *xt, const struct DDS_XTypes_TypeObject *to)
{
  const struct DDS_XTypes_CompleteTypeObject *cto = &to->_u.complete;
  dds_return_t ret = DDS_RETCODE_OK;
  xt->has_obj = 1;
  if (!xt->_d)
    xt->_d = cto->_d;
  else if (xt->_d != cto->_d)
  {
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto err_tk;
  }
  switch (cto->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      xt->_u.alias.related_type = ddsi_type_ref_id_locked_impl (gv, &cto->_u.alias_type.body.common.related_type);
      xt->_u.alias.related_flags = cto->_u.alias_type.body.common.related_flags;
      memcpy(&xt->_u.alias.detail.type_name, cto->_u.alias_type.header.detail.type_name, sizeof(xt->_u.alias.detail.type_name));
      break;
    case DDS_XTypes_TK_ANNOTATION:
      ret = DDS_RETCODE_UNSUPPORTED; /* FIXME: not implemented */
      goto err_tk;
    case DDS_XTypes_TK_STRUCTURE:
      xt->_u.structure.flags = cto->_u.struct_type.struct_flags;
      if (cto->_u.struct_type.header.base_type._d)
        xt->_u.structure.base_type = ddsi_type_ref_id_locked_impl (gv, &cto->_u.struct_type.header.base_type);
      else
        xt->_u.structure.base_type = NULL;
      memcpy(&xt->_u.structure.detail.type_name, cto->_u.struct_type.header.detail.type_name, sizeof(xt->_u.structure.detail.type_name));
      xt->_u.structure.members.length = cto->_u.struct_type.member_seq._length;
      xt->_u.structure.members.seq = ddsrt_calloc (xt->_u.structure.members.length, sizeof (*xt->_u.structure.members.seq));
      for (uint32_t n = 0; n < xt->_u.structure.members.length; n++)
      {
        xt->_u.structure.members.seq[n].id = cto->_u.struct_type.member_seq._buffer[n].common.member_id;
        xt->_u.structure.members.seq[n].flags = cto->_u.struct_type.member_seq._buffer[n].common.member_flags;
        xt->_u.structure.members.seq[n].type = ddsi_type_ref_id_locked_impl (gv, &cto->_u.struct_type.member_seq._buffer[n].common.member_type_id);
        set_member_detail(&xt->_u.structure.members.seq[n].detail, &cto->_u.struct_type.member_seq._buffer[n].detail);
      }
      break;
    case DDS_XTypes_TK_UNION:
      xt->_u.union_type.flags = cto->_u.union_type.union_flags;
      xt->_u.union_type.disc_type = ddsi_type_ref_id_locked_impl (gv, &cto->_u.union_type.discriminator.common.type_id);
      xt->_u.union_type.disc_flags = cto->_u.union_type.discriminator.common.member_flags;
      memcpy(&xt->_u.union_type.detail.type_name, cto->_u.union_type.header.detail.type_name, sizeof(xt->_u.union_type.detail.type_name));
      xt->_u.union_type.members.length = cto->_u.union_type.member_seq._length;
      xt->_u.union_type.members.seq = ddsrt_calloc (xt->_u.union_type.members.length, sizeof (*xt->_u.union_type.members.seq));
      for (uint32_t n = 0; n < xt->_u.union_type.members.length; n++)
      {
        xt->_u.union_type.members.seq[n].id = cto->_u.union_type.member_seq._buffer[n].common.member_id;
        xt->_u.union_type.members.seq[n].flags = cto->_u.union_type.member_seq._buffer[n].common.member_flags;
        xt->_u.union_type.members.seq[n].type = ddsi_type_ref_id_locked_impl (gv, &cto->_u.union_type.member_seq._buffer[n].common.type_id);
        xt->_u.union_type.members.seq[n].label_seq._length = cto->_u.union_type.member_seq._buffer[n].common.label_seq._length;
        xt->_u.union_type.members.seq[n].label_seq._buffer = ddsrt_memdup (cto->_u.union_type.member_seq._buffer[n].common.label_seq._buffer,
          cto->_u.union_type.member_seq._buffer[n].common.label_seq._length * sizeof (*cto->_u.union_type.member_seq._buffer[n].common.label_seq._buffer));
        set_member_detail(&xt->_u.union_type.members.seq[n].detail, &cto->_u.union_type.member_seq._buffer[n].detail);
      }
      break;
    case DDS_XTypes_TK_BITSET:
      memcpy(&xt->_u.bitset.detail.type_name, cto->_u.bitset_type.header.detail.type_name, sizeof(xt->_u.bitset.detail.type_name));
      xt->_u.bitset.fields.length = cto->_u.bitset_type.field_seq._length;
      xt->_u.bitset.fields.seq = ddsrt_calloc (xt->_u.bitset.fields.length, sizeof (*xt->_u.bitset.fields.seq));
      for (uint32_t n = 0; n < xt->_u.bitset.fields.length; n++)
      {
        xt->_u.bitset.fields.seq[n].position = cto->_u.bitset_type.field_seq._buffer[n].common.position;
        xt->_u.bitset.fields.seq[n].flags = cto->_u.bitset_type.field_seq._buffer[n].common.flags;
        xt->_u.bitset.fields.seq[n].bitcount = cto->_u.bitset_type.field_seq._buffer[n].common.bitcount;
        xt->_u.bitset.fields.seq[n].holder_type = cto->_u.bitset_type.field_seq._buffer[n].common.holder_type;
        set_member_detail(&xt->_u.bitset.fields.seq[n].detail, &cto->_u.bitset_type.field_seq._buffer[n].detail);
      }
      break;
    case DDS_XTypes_TK_SEQUENCE:
      xt->_u.seq.c.element_type = ddsi_type_ref_id_locked_impl (gv, &cto->_u.sequence_type.element.common.type);
      xt->_u.seq.c.element_flags = cto->_u.sequence_type.element.common.element_flags;
      xt->_u.seq.bound = cto->_u.sequence_type.header.common.bound;
      break;
    case DDS_XTypes_TK_ARRAY:
      xt->_u.array.c.element_type = ddsi_type_ref_id_locked_impl (gv, &cto->_u.array_type.element.common.type);
      xt->_u.array.c.element_flags = cto->_u.array_type.element.common.element_flags;
      xt_lbounds_dup (&xt->_u.array.bounds, &cto->_u.array_type.header.common.bound_seq);
      break;
    case DDS_XTypes_TK_MAP:
      xt->_u.map.c.element_type = ddsi_type_ref_id_locked_impl (gv, &cto->_u.map_type.element.common.type);
      xt->_u.map.c.element_flags = cto->_u.map_type.element.common.element_flags;
      xt->_u.map.key_type = ddsi_type_ref_id_locked_impl (gv, &cto->_u.map_type.key.common.type);
      xt->_u.map.bound = cto->_u.map_type.header.common.bound;
      break;
    case DDS_XTypes_TK_ENUM:
      xt->_u.enum_type.flags = cto->_u.enumerated_type.enum_flags;
      xt->_u.enum_type.bit_bound = cto->_u.enumerated_type.header.common.bit_bound;
      memcpy(&xt->_u.enum_type.detail.type_name, cto->_u.enumerated_type.header.detail.type_name, sizeof(xt->_u.enum_type.detail.type_name));
      xt->_u.enum_type.literals.length = cto->_u.enumerated_type.literal_seq._length;
      xt->_u.enum_type.literals.seq = ddsrt_calloc (xt->_u.enum_type.literals.length, sizeof (*xt->_u.enum_type.literals.seq));
      for (uint32_t n = 0; n < xt->_u.enum_type.literals.length; n++)
      {
        xt->_u.enum_type.literals.seq[n].value = cto->_u.enumerated_type.literal_seq._buffer[n].common.value;
        xt->_u.enum_type.literals.seq[n].flags = cto->_u.enumerated_type.literal_seq._buffer[n].common.flags;
        set_member_detail(&xt->_u.enum_type.literals.seq[n].detail, &cto->_u.enumerated_type.literal_seq._buffer[n].detail);
      }
      break;
    case DDS_XTypes_TK_BITMASK:
      xt->_u.bitmask.flags = cto->_u.bitmask_type.bitmask_flags;
      xt->_u.bitmask.bit_bound = cto->_u.bitmask_type.header.common.bit_bound;
      memcpy(&xt->_u.bitmask.detail.type_name, cto->_u.bitmask_type.header.detail.type_name, sizeof(xt->_u.bitmask.detail.type_name));
      xt->_u.bitmask.bitflags.length = cto->_u.bitmask_type.flag_seq._length;
      xt->_u.bitmask.bitflags.seq = ddsrt_calloc (xt->_u.bitmask.bitflags.length, sizeof (*xt->_u.bitmask.bitflags.seq));
      for (uint32_t n = 0; n < xt->_u.bitmask.bitflags.length; n++)
      {
        xt->_u.bitmask.bitflags.seq[n].position = cto->_u.bitmask_type.flag_seq._buffer[n].common.position;
        xt->_u.bitmask.bitflags.seq[n].flags = cto->_u.bitmask_type.flag_seq._buffer[n].common.flags;
        set_member_detail(&xt->_u.bitmask.bitflags.seq[n].detail, &cto->_u.bitmask_type.flag_seq._buffer[n].detail);
      }
      break;
    default:
      ret = DDS_RETCODE_UNSUPPORTED; /* not supported */
      goto err_tk;
  }
  return ret;

err_tk:
  xt->has_obj = 0;
  xt->_d = DDS_XTypes_TK_NONE;
  return ret;
}

dds_return_t ddsi_xt_type_add_typeobj (struct ddsi_domaingv *gv, struct xt_type *xt, const struct DDS_XTypes_TypeObject *to)
{
  assert (xt);
  assert (to);
  assert (xt->kind == DDSI_TYPEID_KIND_MINIMAL || xt->kind == DDSI_TYPEID_KIND_COMPLETE);
  if (xt->has_obj)
    return DDS_RETCODE_OK;
  if (xt->kind == DDSI_TYPEID_KIND_MINIMAL)
  {
    assert (to->_d == DDS_XTypes_EK_MINIMAL);
    return add_minimal_typeobj (gv, xt, to);
  }
  else if (xt->kind == DDSI_TYPEID_KIND_COMPLETE)
  {
    assert (to->_d == DDS_XTypes_EK_COMPLETE);
    return add_complete_typeobj (gv, xt, to);
  }
  return DDS_RETCODE_BAD_PARAMETER;
}

dds_return_t ddsi_xt_type_init_impl (struct ddsi_domaingv *gv, struct xt_type *xt, const struct DDS_XTypes_TypeIdentifier *ti, const struct DDS_XTypes_TypeObject *to)
{
  assert (xt);
  assert (ti);
  dds_return_t ret = DDS_RETCODE_OK;

  ddsi_typeid_copy_impl (&xt->id.x, ti);
  if (!ddsi_typeid_is_hash_impl (ti))
    xt->kind = DDSI_TYPEID_KIND_FULLY_DESCRIPTIVE;
  else if (ddsi_typeid_is_complete_impl (ti))
    xt->kind = DDSI_TYPEID_KIND_COMPLETE;
  else
    xt->kind = DDSI_TYPEID_KIND_MINIMAL;

  if (ti->_d <= DDS_XTypes_TK_STRING16)
  {
    assert (to == NULL);
    xt->_d = ti->_d;
  }
  else
  {
    xt->is_plain_collection = ti->_d >= DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL && ti->_d <= DDS_XTypes_TI_PLAIN_MAP_LARGE;
    switch (ti->_d)
    {
      case DDS_XTypes_TI_STRING8_SMALL:
        xt->_d = DDS_XTypes_TK_STRING8;
        xt->_u.str8.bound = (DDS_XTypes_LBound) ti->_u.string_sdefn.bound;
        break;
      case DDS_XTypes_TI_STRING8_LARGE:
        xt->_d = DDS_XTypes_TK_STRING8;
        xt->_u.str8.bound = ti->_u.string_ldefn.bound;
        break;
      case DDS_XTypes_TI_STRING16_SMALL:
        xt->_d = DDS_XTypes_TK_STRING16;
        xt->_u.str16.bound = (DDS_XTypes_LBound) ti->_u.string_sdefn.bound;
        break;
      case DDS_XTypes_TI_STRING16_LARGE:
        xt->_d = DDS_XTypes_TK_STRING16;
        xt->_u.str16.bound = ti->_u.string_ldefn.bound;
        break;
      case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
        xt->_d = DDS_XTypes_TK_SEQUENCE;
        xt->_u.seq.c.element_type = ddsi_type_ref_id_locked_impl (gv, ti->_u.seq_sdefn.element_identifier);
        xt->_u.seq.bound = (DDS_XTypes_LBound) ti->_u.seq_sdefn.bound;
        xt_collection_common_init (&xt->_u.seq.c, &ti->_u.seq_sdefn.header);
        break;
      case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
        xt->_d = DDS_XTypes_TK_SEQUENCE;
        xt->_u.seq.c.element_type = ddsi_type_ref_id_locked_impl (gv, ti->_u.seq_ldefn.element_identifier);
        xt->_u.seq.bound = ti->_u.seq_ldefn.bound;
        xt_collection_common_init (&xt->_u.seq.c, &ti->_u.seq_ldefn.header);
        break;
      case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
        xt->_d = DDS_XTypes_TK_ARRAY;
        xt->_u.array.c.element_type = ddsi_type_ref_id_locked_impl (gv, ti->_u.array_sdefn.element_identifier);
        xt_collection_common_init (&xt->_u.array.c, &ti->_u.array_sdefn.header);
        xt_sbounds_to_lbounds (&xt->_u.array.bounds, &ti->_u.array_sdefn.array_bound_seq);
        break;
      case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
        xt->_d = DDS_XTypes_TK_ARRAY;
        xt->_u.array.c.element_type = ddsi_type_ref_id_locked_impl (gv, ti->_u.array_ldefn.element_identifier);
        xt_collection_common_init (&xt->_u.array.c, &ti->_u.array_ldefn.header);
        xt_lbounds_dup (&xt->_u.array.bounds, &ti->_u.array_ldefn.array_bound_seq);
        break;
      case DDS_XTypes_TI_PLAIN_MAP_SMALL:
        xt->_d = DDS_XTypes_TK_MAP;
        xt->_u.map.c.element_type = ddsi_type_ref_id_locked_impl (gv, ti->_u.map_sdefn.element_identifier);
        xt->_u.map.bound = (DDS_XTypes_LBound) ti->_u.map_sdefn.bound;
        xt_collection_common_init (&xt->_u.map.c, &ti->_u.map_sdefn.header);
        xt->_u.map.key_type = ddsi_type_ref_id_locked_impl (gv, ti->_u.map_sdefn.key_identifier);
        break;
      case DDS_XTypes_TI_PLAIN_MAP_LARGE:
        xt->_d = DDS_XTypes_TK_MAP;
        xt->_u.map.c.element_type = ddsi_type_ref_id_locked_impl (gv, ti->_u.map_ldefn.element_identifier);
        xt->_u.map.bound = (DDS_XTypes_LBound) ti->_u.map_ldefn.bound;
        xt_collection_common_init (&xt->_u.map.c, &ti->_u.map_ldefn.header);
        xt->_u.map.key_type = ddsi_type_ref_id_locked_impl (gv, ti->_u.map_ldefn.key_identifier);
        break;
      case DDS_XTypes_EK_MINIMAL:
        if (to != NULL)
          ret = add_minimal_typeobj (gv, xt, to);
        break;
      case DDS_XTypes_EK_COMPLETE:
        if (to != NULL)
          ret = add_complete_typeobj (gv, xt, to);
        break;
      case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT:
        xt->_d = DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT;
        xt->sc_component_id = ti->_u.sc_component_id;
        break;
      default:
        ddsi_typeid_fini (&xt->id);
        ret = DDS_RETCODE_UNSUPPORTED; /* not supported */
        break;
    }
  }
  return ret;
}

dds_return_t ddsi_xt_type_init (struct ddsi_domaingv *gv, struct xt_type *xt, const ddsi_typeid_t *ti, const ddsi_typeobj_t *to)
{
  return ddsi_xt_type_init_impl (gv, xt, &ti->x, &to->x);
}

static void DDS_XTypes_AppliedVerbatimAnnotation_copy (struct DDS_XTypes_AppliedVerbatimAnnotation *dst, const struct DDS_XTypes_AppliedVerbatimAnnotation *src)
{
  assert (dst);
  if (src)
  {
    ddsrt_strlcpy (dst->placement, src->placement, sizeof (dst->placement));
    ddsrt_strlcpy (dst->language, src->language, sizeof (dst->language));
    dst->text = ddsrt_strdup (src->text);
  }
}

static void DDS_XTypes_AppliedBuiltinTypeAnnotations_copy (struct DDS_XTypes_AppliedBuiltinTypeAnnotations *dst, const struct DDS_XTypes_AppliedBuiltinTypeAnnotations *src)
{
  if (src)
  {
    dst->verbatim = calloc (1, sizeof (*dst->verbatim));
    DDS_XTypes_AppliedVerbatimAnnotation_copy (dst->verbatim, src->verbatim);
  }
}

static void DDS_XTypes_AppliedAnnotationParameter_copy (struct DDS_XTypes_AppliedAnnotationParameter *dst, const struct DDS_XTypes_AppliedAnnotationParameter *src)
{
  if (src)
  {
    memcpy (dst->paramname_hash, src->paramname_hash, sizeof (dst->paramname_hash));
    dst->value = src->value;
  }
}

static void DDS_XTypes_AppliedAnnotationParameterSeq_copy (struct DDS_XTypes_AppliedAnnotationParameterSeq **dst, const struct DDS_XTypes_AppliedAnnotationParameterSeq *src)
{
  if (src)
  {
    (*dst) = ddsrt_calloc (1, sizeof (**dst));
    (*dst)->_maximum = src->_maximum;
    (*dst)->_length = src->_length;
    (*dst)->_buffer = ddsrt_calloc (src->_length, sizeof (*(*dst)->_buffer));
    for (uint32_t n = 0; n < src->_length; n++)
      DDS_XTypes_AppliedAnnotationParameter_copy (&(*dst)->_buffer[n], &src->_buffer[n]);
    (*dst)->_release = src->_release;
  }
}

static void DDS_XTypes_AppliedAnnotation_copy (struct DDS_XTypes_AppliedAnnotation *dst, const struct DDS_XTypes_AppliedAnnotation *src)
{
  if (src)
  {
    ddsi_typeid_copy_impl (&dst->annotation_typeid, &src->annotation_typeid);
    DDS_XTypes_AppliedAnnotationParameterSeq_copy (&dst->param_seq, src->param_seq);
  }
}

static void DDS_XTypes_AppliedAnnotationSeq_copy (struct DDS_XTypes_AppliedAnnotationSeq *dst, const struct DDS_XTypes_AppliedAnnotationSeq *src)
{
  if (src)
  {
    dst->_maximum = src->_maximum;
    dst->_length = src->_length;
    dst->_buffer = ddsrt_calloc (src->_length, sizeof (*dst->_buffer));
    for (uint32_t n = 0; n < src->_length; n++)
      DDS_XTypes_AppliedAnnotation_copy (&dst->_buffer[n], &src->_buffer[n]);
    dst->_release = src->_release;
  }
}

static void DDS_XTypes_AppliedBuiltinMemberAnnotations_copy (struct DDS_XTypes_AppliedBuiltinMemberAnnotations *dst, const struct DDS_XTypes_AppliedBuiltinMemberAnnotations *src)
{
  if (src)
  {
    dst->unit = src->unit ? ddsrt_strdup (src->unit) : NULL;
    if (src->min) {
      dst->min = ddsrt_memdup(src->min, sizeof(struct DDS_XTypes_AnnotationParameterValue));
    } else {
      dst->min = NULL;
    }
    if (src->max) {
      dst->max = ddsrt_memdup(src->max, sizeof(struct DDS_XTypes_AnnotationParameterValue));
    } else {
      dst->max = NULL;
    }
    dst->hash_id = src->hash_id ? ddsrt_strdup (src->hash_id) : NULL;
  }
}

static void get_type_detail (DDS_XTypes_CompleteTypeDetail *dst, const struct xt_type_detail *src)
{
  ddsrt_strlcpy (dst->type_name, src->type_name, sizeof (dst->type_name));
  if (src->annotations.ann_builtin) {
    dst->ann_builtin = ddsrt_calloc(1, sizeof(struct DDS_XTypes_AppliedBuiltinTypeAnnotations));
    DDS_XTypes_AppliedBuiltinTypeAnnotations_copy (dst->ann_builtin, src->annotations.ann_builtin);
  } else {
    dst->ann_builtin = NULL;
  }

  if (src->annotations.ann_custom) {
    dst->ann_custom = ddsrt_calloc(1, sizeof(DDS_XTypes_AppliedAnnotationSeq));
    DDS_XTypes_AppliedAnnotationSeq_copy (dst->ann_custom, src->annotations.ann_custom);
  } else {
    dst->ann_custom = NULL;
  }
}

static void get_member_detail (DDS_XTypes_CompleteMemberDetail *dst, const struct xt_member_detail *src)
{
  ddsrt_strlcpy (dst->name, src->name, sizeof (dst->name));
  if (src->annotations.ann_builtin) {
    dst->ann_builtin = ddsrt_calloc(1, sizeof(struct DDS_XTypes_AppliedBuiltinMemberAnnotations));
    DDS_XTypes_AppliedBuiltinMemberAnnotations_copy (dst->ann_builtin, src->annotations.ann_builtin);
  } else {
    dst->ann_builtin = NULL;
  }

  if (src->annotations.ann_custom) {
    dst->ann_custom = ddsrt_calloc(1, sizeof(DDS_XTypes_AppliedAnnotationSeq));
    DDS_XTypes_AppliedAnnotationSeq_copy (dst->ann_custom, src->annotations.ann_custom);
  } else {
    dst->ann_custom = NULL;
  }
}

static void get_minimal_member_detail (DDS_XTypes_MinimalMemberDetail *dst, const struct xt_member_detail *src)
{
  memcpy (dst->name_hash, src->name_hash, sizeof (dst->name_hash));
}

void ddsi_xt_type_fini (struct ddsi_domaingv *gv, struct xt_type *xt)
{
  switch (xt->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      ddsi_type_unref_locked (gv, xt->_u.alias.related_type);
      break;
    case DDS_XTypes_TK_ANNOTATION:
      abort (); /* FIXME: not implemented */
      break;
    case DDS_XTypes_TK_STRUCTURE:
      ddsi_type_unref_locked (gv, xt->_u.structure.base_type);
      for (uint32_t n = 0; n < xt->_u.structure.members.length; n++)
        ddsi_type_unref_locked (gv, xt->_u.structure.members.seq[n].type);
      ddsrt_free (xt->_u.structure.members.seq);
      break;
    case DDS_XTypes_TK_UNION:
      ddsi_type_unref_locked (gv, xt->_u.union_type.disc_type);
      for (uint32_t n = 0; n < xt->_u.union_type.members.length; n++)
      {
        ddsi_type_unref_locked (gv, xt->_u.union_type.members.seq[n].type);
        ddsrt_free (xt->_u.union_type.members.seq[n].label_seq._buffer);
      }
      ddsrt_free (xt->_u.union_type.members.seq);
      break;
    case DDS_XTypes_TK_BITSET:
      ddsrt_free (xt->_u.bitset.fields.seq);
      break;
    case DDS_XTypes_TK_SEQUENCE:
      ddsi_type_unref_locked (gv, xt->_u.seq.c.element_type);
      break;
    case DDS_XTypes_TK_ARRAY:
      ddsi_type_unref_locked (gv, xt->_u.array.c.element_type);
      ddsrt_free (xt->_u.array.bounds._buffer);
      break;
    case DDS_XTypes_TK_MAP:
      ddsi_type_unref_locked (gv, xt->_u.map.c.element_type);
      ddsi_type_unref_locked (gv, xt->_u.map.key_type);
      break;
    case DDS_XTypes_TK_ENUM:
      ddsrt_free (xt->_u.enum_type.literals.seq);
      break;
    case DDS_XTypes_TK_BITMASK:
      ddsrt_free (xt->_u.bitmask.bitflags.seq);
      break;
    default:
      break;
  }
  ddsi_typeid_fini (&xt->id);
}

static void xt_applied_type_annotations_copy (struct xt_applied_type_annotations *dst, const struct xt_applied_type_annotations *src)
{
  if (src->ann_builtin) {
    dst->ann_builtin = ddsrt_calloc(1, sizeof(DDS_XTypes_AppliedBuiltinTypeAnnotations));
    DDS_XTypes_AppliedBuiltinTypeAnnotations_copy (dst->ann_builtin, src->ann_builtin);
  } else {
    dst->ann_builtin = NULL;
  }

  if (src->ann_custom) {
    dst->ann_custom = ddsrt_calloc(1, sizeof(DDS_XTypes_AppliedAnnotationSeq));
    DDS_XTypes_AppliedAnnotationSeq_copy (dst->ann_custom, src->ann_custom);
  } else {
    dst->ann_custom = NULL;
  }
}

static void xt_applied_member_annotations_copy (struct xt_applied_member_annotations *dst, const struct xt_applied_member_annotations *src)
{
  if (src->ann_builtin) {
    dst->ann_builtin = ddsrt_calloc(1, sizeof(DDS_XTypes_AppliedBuiltinMemberAnnotations));
    DDS_XTypes_AppliedBuiltinMemberAnnotations_copy (dst->ann_builtin, src->ann_builtin);
  } else {
    dst->ann_builtin = NULL;
  }

  if (src->ann_custom) {
    dst->ann_custom = ddsrt_calloc(1, sizeof(DDS_XTypes_AppliedAnnotationSeq));
    DDS_XTypes_AppliedAnnotationSeq_copy (dst->ann_custom, src->ann_custom);
  } else {
    dst->ann_custom = NULL;
  }
}

static void xt_annotation_parameter_copy (struct ddsi_domaingv *gv, struct xt_annotation_parameter *dst, const struct xt_annotation_parameter *src)
{
  if (src)
  {
    dst->member_type = ddsi_type_ref_locked (gv, src->member_type);
    dst->flags = src->flags;
    ddsrt_strlcpy (dst->name, src->name, sizeof (dst->name));
    memcpy (dst->name_hash, src->name_hash, sizeof (dst->name_hash));
    dst->default_value = src->default_value;
  }
}

static void xt_annotation_parameter_seq_copy (struct ddsi_domaingv *gv, struct xt_annotation_parameter_seq *dst, const struct xt_annotation_parameter_seq *src)
{
  if (src)
  {
    dst->length = src->length;
    dst->seq = ddsrt_calloc (src->length, sizeof (*dst->seq));
    for (uint32_t n = 0; n < src->length; n++)
      xt_annotation_parameter_copy (gv, &dst->seq[n], &src->seq[n]);
  }
}

static void xt_type_detail_copy (struct xt_type_detail *dst, const struct xt_type_detail *src)
{
  if (src)
  {
    ddsrt_strlcpy (dst->type_name, src->type_name, sizeof (dst->type_name));
    xt_applied_type_annotations_copy (&dst->annotations, &src->annotations);
  }
}

static void xt_collection_common_copy (struct ddsi_domaingv *gv, struct xt_collection_common *dst, const struct xt_collection_common *src)
{
  if (src)
  {
    dst->flags = src->flags;
    dst->ek = src->ek;
    xt_type_detail_copy (&dst->detail, &src->detail);
    dst->element_type = ddsi_type_ref_locked (gv, src->element_type);
    dst->element_flags = src->element_flags;
    xt_applied_member_annotations_copy (&dst->element_annotations, &src->element_annotations);
  }
}

static void DDS_XTypes_LBoundSeq_copy (struct DDS_XTypes_LBoundSeq *dst, const struct DDS_XTypes_LBoundSeq *src)
{
  if (src)
  {
    dst->_maximum = src->_maximum;
    dst->_length = src->_length;
    dst->_buffer = ddsrt_calloc (src->_length, sizeof (*dst->_buffer));
    for (uint32_t n = 0; n < src->_length; n++)
      dst->_buffer[n] = src->_buffer[n];
    dst->_release = src->_release;
  }
}

static void xt_member_detail_copy (struct xt_member_detail *dst, const struct xt_member_detail *src)
{
  if (src)
  {
    ddsrt_strlcpy (dst->name, src->name, sizeof (dst->name));
    memcpy (dst->name_hash, src->name_hash, sizeof (dst->name_hash));
    xt_applied_member_annotations_copy (&dst->annotations, &src->annotations);
  }
}

static void xt_struct_member_copy (struct ddsi_domaingv *gv, struct xt_struct_member *dst, const struct xt_struct_member *src)
{
  if (src)
  {
    dst->id = src->id;
    dst->flags = src->flags;
    dst->type = ddsi_type_ref_locked (gv, src->type);
    xt_member_detail_copy (&dst->detail, &src->detail);
  }
}

static void xt_struct_member_seq_copy (struct ddsi_domaingv *gv, struct xt_struct_member_seq *dst, const struct xt_struct_member_seq *src)
{
  if (src)
  {
    dst->length = src->length;
    dst->seq = ddsrt_calloc (src->length, sizeof (*dst->seq));
    for (uint32_t n = 0; n < src->length; n++)
      xt_struct_member_copy (gv, &dst->seq[n], &src->seq[n]);
  }
}

static void DDS_XTypes_UnionCaseLabelSeq_copy (struct DDS_XTypes_UnionCaseLabelSeq *dst, const struct DDS_XTypes_UnionCaseLabelSeq *src)
{
  if (src)
  {
    dst->_maximum = src->_maximum;
    dst->_length = src->_length;
    dst->_buffer = ddsrt_calloc (src->_length, sizeof (*dst->_buffer));
    for (uint32_t n = 0; n < src->_length; n++)
      dst->_buffer[n] = src->_buffer[n];
    dst->_release = src->_release;
  }
}

static void xt_union_member_copy (struct ddsi_domaingv *gv, struct xt_union_member *dst, const struct xt_union_member *src)
{
  if (src)
  {
    dst->id = src->id;
    dst->flags = src->flags;
    DDS_XTypes_UnionCaseLabelSeq_copy (&dst->label_seq, &src->label_seq);
    dst->type = ddsi_type_ref_locked (gv, src->type);
    xt_member_detail_copy (&dst->detail, &src->detail);
  }
}

static void xt_union_member_seq_copy (struct ddsi_domaingv *gv, struct xt_union_member_seq *dst, const struct xt_union_member_seq *src)
{
  if (src)
  {
    dst->length = src->length;
    dst->seq = ddsrt_calloc (src->length, sizeof (*dst->seq));
    for (uint32_t n = 0; n < src->length; n++)
      xt_union_member_copy (gv, &dst->seq[n], &src->seq[n]);
  }
}

static void xt_bitfield_copy (struct xt_bitfield *dst, const struct xt_bitfield *src)
{
  if (src)
  {
    dst->position = src->position;
    dst->flags = src->flags;
    dst->bitcount = src->bitcount;
    dst->holder_type = src->holder_type; // Must be primitive integer type
    xt_member_detail_copy (&dst->detail, &src->detail);
  }
}

static void xt_bitfield_seq_copy (struct xt_bitfield_seq *dst, const struct xt_bitfield_seq *src)
{
  if (src)
  {
    dst->length = src->length;
    dst->seq = ddsrt_calloc (src->length, sizeof (*dst->seq));
    for (uint32_t n = 0; n < src->length; n++)
      xt_bitfield_copy (&dst->seq[n], &src->seq[n]);
  }
}

static void xt_enum_literal_seq_copy (struct xt_enum_literal_seq *dst, const struct xt_enum_literal_seq *src)
{
  if (src)
  {
    dst->length = src->length;
    dst->seq = ddsrt_calloc (src->length, sizeof (*dst->seq));
    for (uint32_t n = 0; n < src->length; n++)
      dst->seq[n] = src->seq[n];
  }
}

static void xt_bitflag_copy (struct xt_bitflag *dst, const struct xt_bitflag *src)
{
  if (src)
  {
    dst->position = src->position;
    dst->flags = src->flags;
    xt_member_detail_copy (&dst->detail, &src->detail);
  }
}

static void xt_bitflag_seq_copy (struct xt_bitflag_seq *dst, const struct xt_bitflag_seq *src)
{
  if (src)
  {
    dst->length = src->length;
    dst->seq = ddsrt_calloc (src->length, sizeof (*dst->seq));
    for (uint32_t n = 0; n < src->length; n++)
      xt_bitflag_copy (&dst->seq[n], &src->seq[n]);
  }
}

static struct xt_type * xt_dup (struct ddsi_domaingv *gv, const struct xt_type *src)
{
  struct xt_type *dst = ddsrt_calloc (1, sizeof (*dst));
  ddsi_typeid_copy (&dst->id, &src->id);
  dst->kind = src->kind;
  dst->_d = src->_d;
  switch (src->_d)
  {
    case DDS_XTypes_TK_STRING8:
      dst->_u.str8 = src->_u.str8;
      break;
    case DDS_XTypes_TK_STRING16:
      dst->_u.str16 = src->_u.str16;
      break;
    case DDS_XTypes_TK_SEQUENCE:
      xt_collection_common_copy (gv, &dst->_u.seq.c, &src->_u.seq.c);
      dst->_u.seq.bound = src->_u.seq.bound;
      break;
    case DDS_XTypes_TK_ARRAY:
      xt_collection_common_copy (gv, &dst->_u.array.c, &src->_u.array.c);
      DDS_XTypes_LBoundSeq_copy (&dst->_u.array.bounds, &src->_u.array.bounds);
      break;
    case DDS_XTypes_TK_MAP:
      xt_collection_common_copy (gv, &dst->_u.map.c, &src->_u.map.c);
      dst->_u.map.bound = src->_u.map.bound;
      dst->_u.map.key_flags = src->_u.map.key_flags;
      dst->_u.map.key_type = ddsi_type_ref_locked (gv, src->_u.map.key_type);
      xt_applied_member_annotations_copy (&dst->_u.map.key_annotations, &src->_u.map.key_annotations);
      break;
    case DDS_XTypes_TK_ALIAS:
      dst->_u.alias.related_type = ddsi_type_ref_locked (gv, src->_u.alias.related_type);
      dst->_u.alias.related_flags = src->_u.alias.related_flags;
      xt_type_detail_copy (&dst->_u.alias.detail, &src->_u.alias.detail);
      break;
    case DDS_XTypes_TK_ANNOTATION:
      ddsrt_strlcpy (dst->_u.annotation.annotation_name, src->_u.annotation.annotation_name, sizeof (dst->_u.annotation.annotation_name));
      xt_annotation_parameter_seq_copy (gv, dst->_u.annotation.members, src->_u.annotation.members);
      break;
    case DDS_XTypes_TK_STRUCTURE:
      dst->_u.structure.flags = src->_u.structure.flags;
      if (src->_u.structure.base_type)
        dst->_u.structure.base_type = ddsi_type_ref_locked (gv, src->_u.structure.base_type);
      xt_struct_member_seq_copy (gv, &dst->_u.structure.members, &src->_u.structure.members);
      xt_type_detail_copy (&dst->_u.structure.detail, &src->_u.structure.detail);
      break;
    case DDS_XTypes_TK_UNION:
      dst->_u.union_type.flags = src->_u.union_type.flags;
      dst->_u.union_type.disc_type = ddsi_type_ref_locked (gv, src->_u.union_type.disc_type);
      dst->_u.union_type.disc_flags = src->_u.union_type.disc_flags;
      xt_applied_type_annotations_copy (&dst->_u.union_type.disc_annotations, &src->_u.union_type.disc_annotations);
      xt_union_member_seq_copy (gv, &dst->_u.union_type.members, &src->_u.union_type.members);
      xt_type_detail_copy (&dst->_u.union_type.detail, &src->_u.union_type.detail);
      break;
    case DDS_XTypes_TK_BITSET:
      xt_bitfield_seq_copy (&dst->_u.bitset.fields, &src->_u.bitset.fields);
      xt_type_detail_copy (&dst->_u.bitset.detail, &src->_u.bitset.detail);
      break;
    case DDS_XTypes_TK_ENUM:
      dst->_u.enum_type.flags = src->_u.enum_type.flags;
      dst->_u.enum_type.bit_bound = src->_u.enum_type.bit_bound;
      xt_enum_literal_seq_copy (&dst->_u.enum_type.literals, &src->_u.enum_type.literals);
      xt_type_detail_copy (&dst->_u.enum_type.detail, &src->_u.enum_type.detail);
      break;
    case DDS_XTypes_TK_BITMASK:
      dst->_u.bitmask.flags = src->_u.bitmask.flags;
      dst->_u.bitmask.bit_bound = src->_u.bitmask.bit_bound;
      xt_bitflag_seq_copy (&dst->_u.bitmask.bitflags, &src->_u.bitmask.bitflags);
      xt_type_detail_copy (&dst->_u.bitmask.detail, &src->_u.bitmask.detail);
      break;
  }
  return dst;
}

static const struct xt_type *ddsi_xt_unalias (const struct xt_type *t)
{
  return t->_d == DDS_XTypes_TK_ALIAS ? ddsi_xt_unalias (&t->_u.alias.related_type->xt) : t;
}

static bool xt_has_basetype (const struct xt_type *t)
{
  assert (t->_d == DDS_XTypes_TK_STRUCTURE);
  return t->_u.structure.base_type != NULL;
}

static struct xt_type *xt_expand_basetype (struct ddsi_domaingv *gv, const struct xt_type *t)
{
  assert (t->_d == DDS_XTypes_TK_STRUCTURE);
  assert (t->_u.structure.base_type);
  const struct xt_type *b = ddsi_xt_unalias (&t->_u.structure.base_type->xt);
  if (xt_is_unresolved (b))
  {
    struct ddsi_typeid_str tidstr;
    GVWARNING ("assignability check: base type %s unresolved in xt_expand_basetype\n", ddsi_make_typeid_str (&tidstr, &b->id));
    return NULL;
  }
  struct xt_type *te = xt_has_basetype (b) ? xt_expand_basetype (gv, b) : xt_dup (gv, t);
  if (!te)
    return NULL;
  struct xt_struct_member_seq *ms = &te->_u.structure.members;

  /* Expand members of the base type in the resulting type
     before the members of the derived type */
  uint32_t incr = b->_u.structure.members.length;
  ms->length += incr;
  ms->seq = ddsrt_realloc (ms->seq, ms->length * sizeof (*ms->seq));
  memmove (&ms->seq[incr], ms->seq, incr * sizeof (*ms->seq));
  for (uint32_t i = 0; i < b->_u.structure.members.length; i++)
    xt_struct_member_copy (gv, &ms->seq[i], &b->_u.structure.members.seq[i]);
  return te;
}

static struct xt_type *xt_type_key_erased (struct ddsi_domaingv *gv, const struct xt_type *t)
{
  switch (t->_d)
  {
    case DDS_XTypes_TK_STRUCTURE: {
      struct xt_type *tke = xt_dup (gv, t);
      for (uint32_t i = 0; i < tke->_u.structure.members.length; i++)
      {
        struct xt_struct_member *m = &tke->_u.structure.members.seq[i];
        if (m->flags & DDS_XTypes_IS_KEY)
          m->flags &= (DDS_XTypes_MemberFlag) ~DDS_XTypes_IS_KEY;
      }
      return tke;
    }
    case DDS_XTypes_TK_UNION: {
      struct xt_type *tke = xt_dup (gv, t);
      for (uint32_t i = 0; i < tke->_u.union_type.members.length; i++)
      {
        struct xt_union_member *m = &tke->_u.union_type.members.seq[i];
        if (m->flags & DDS_XTypes_IS_KEY)
          m->flags &= (DDS_XTypes_MemberFlag) ~DDS_XTypes_IS_KEY;
      }
      return tke;
    }
    default:
      return xt_dup (gv, t);
  }
}

static bool xt_struct_has_key (const struct xt_type *t)
{
  assert (t->_d == DDS_XTypes_TK_STRUCTURE);
  for (uint32_t i = 0; i < t->_u.structure.members.length; i++)
    if (t->_u.structure.members.seq[i].flags & DDS_XTypes_IS_KEY)
      return true;
  return false;
}

static bool xt_check_bound (uint32_t rd_bound, uint32_t wr_bound)
{
  return !rd_bound || (wr_bound && rd_bound >= wr_bound);
}

static struct xt_type *xt_type_keyholder (struct ddsi_domaingv *gv, const struct xt_type *t)
{
  struct xt_type *tkh = xt_dup (gv, t);
  switch (tkh->_d)
  {
    case DDS_XTypes_TK_STRUCTURE: {
      if (xt_struct_has_key (tkh))
      {
        /* Rule: If T has any members designated as key members see 7.2.2.4.4.4.8), then KeyHolder(T) removes any
          members of T that are not designated as key members. */
        uint32_t i = 0, l = t->_u.structure.members.length;
        while (i < l)
        {
          if (tkh->_u.structure.members.seq[i].flags & DDS_XTypes_IS_KEY)
          {
            i++;
            continue;
          }

          /* Unref the member type for non-key fields for this copy of the type,
             because the member is removed */
          ddsi_type_unref_locked (gv, tkh->_u.structure.members.seq[i].type);

          if (i < l - 1)
            memmove (&tkh->_u.structure.members.seq[i], &tkh->_u.structure.members.seq[i + 1], (l - i - 1) * sizeof (*tkh->_u.structure.members.seq));
          l--;
        }
        tkh->_u.structure.members.length = l;
      }
      else
      {
        /* Rule: If T is a structure with no key members, then KeyHolder(T) adds a key designator to each member. */
        for (uint32_t i = 0; i < t->_u.structure.members.length; i++)
          t->_u.structure.members.seq[i].flags |= DDS_XTypes_IS_KEY;
      }
      return tkh;
    }
    case DDS_XTypes_TK_UNION: {
      /* Rules:
         - If T has discriminator as key, then KeyHolder(T) removes any members of T that are not designated as key members.
         - If T is a union and the discriminator is not marked as key, then KeyHolder(T) is the same type T. */
      if (tkh->_u.union_type.disc_flags & DDS_XTypes_IS_KEY)
      {
        /* Unref type for members, because all members are removed from this copy of the type */
        for (uint32_t n = 0; n < tkh->_u.union_type.members.length; n++)
          ddsi_type_unref_locked (gv, tkh->_u.union_type.members.seq[n].type);

        tkh->_u.union_type.members.length = 0;
        ddsrt_free (tkh->_u.union_type.members.seq);
      }
      return tkh;
    }
    default:
      assert (false);
      ddsi_xt_type_fini (gv, tkh);
      ddsrt_free (tkh);
      return NULL;
  }
}

static bool xt_is_plain_collection (const struct xt_type *t)
{
  return t->is_plain_collection;
}

static bool xt_is_plain_collection_equiv_kind (const struct xt_type *t, DDS_XTypes_EquivalenceKind ek)
{
  if (!xt_is_plain_collection (t))
    return false;
  switch (t->_d)
  {
    case DDS_XTypes_TK_SEQUENCE:
      return t->_u.seq.c.ek == ek;
    case DDS_XTypes_TK_ARRAY:
      return t->_u.array.c.ek == ek;
    case DDS_XTypes_TK_MAP:
      return t->_u.map.c.ek == ek;
    default:
      abort ();
  }
}

static bool xt_is_plain_collection_fully_descriptive_typeid (const struct xt_type *t)
{
  return xt_is_plain_collection_equiv_kind (t, DDS_XTypes_EK_BOTH);
}

static bool xt_is_equiv_kind_hash_typeid (const struct xt_type *t, DDS_XTypes_EquivalenceKind ek)
{
  return (ek == DDS_XTypes_EK_MINIMAL && t->kind == DDSI_TYPEID_KIND_MINIMAL)
    || (ek == DDS_XTypes_EK_COMPLETE && t->kind == DDSI_TYPEID_KIND_COMPLETE)
    || (t->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT && t->sc_component_id.sc_component_id._d == ek)
    || xt_is_plain_collection_equiv_kind (t, ek);
}

static bool xt_is_minimal_hash_typeid (const struct xt_type *t)
{
  return xt_is_equiv_kind_hash_typeid (t, DDS_XTypes_EK_MINIMAL);
}

static bool xt_is_primitive (const struct xt_type *t)
{
  return t->_d > DDS_XTypes_TK_NONE && t->_d <= DDS_XTypes_TK_CHAR16;
}

static bool xt_is_string (const struct xt_type *t)
{
  return t->_d == DDS_XTypes_TK_STRING8 || t->_d == DDS_XTypes_TK_STRING16;
}

static DDS_XTypes_LBound xt_string_bound (const struct xt_type *t)
{
  switch (t->_d)
  {
    case DDS_XTypes_TK_STRING8:
      return t->_u.str8.bound;
    case DDS_XTypes_TK_STRING16:
      return t->_u.str16.bound;
    default:
      abort (); /* not supported */
  }
}

static bool xt_is_fully_descriptive (const struct xt_type *t)
{
  return xt_is_primitive (t) || xt_is_string (t) || (t->_d != DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT && xt_is_plain_collection_fully_descriptive_typeid (t));
}

static bool xt_is_enumerated (const struct xt_type *t)
{
  return t->_d == DDS_XTypes_TK_ENUM || t->_d == DDS_XTypes_TK_BITMASK;
}

static uint32_t xt_get_extensibility (const struct xt_type *t)
{
  uint32_t ext_flags;
  switch (t->_d)
  {
    case DDS_XTypes_TK_ENUM:
      ext_flags = t->_u.enum_type.flags & XT_FLAG_EXTENSIBILITY_MASK;
      break;
    case DDS_XTypes_TK_BITMASK:
      ext_flags = t->_u.bitmask.flags & XT_FLAG_EXTENSIBILITY_MASK;
      break;
    case DDS_XTypes_TK_STRUCTURE:
      ext_flags = t->_u.structure.flags & XT_FLAG_EXTENSIBILITY_MASK;
      break;
    case DDS_XTypes_TK_UNION:
      ext_flags = t->_u.union_type.flags & XT_FLAG_EXTENSIBILITY_MASK;
      break;
    default:
      return 0;
  }
  assert (ext_flags == DDS_XTypes_IS_FINAL || ext_flags == DDS_XTypes_IS_APPENDABLE || ext_flags == DDS_XTypes_IS_MUTABLE);
  return ext_flags;
}

static bool xt_is_delimited (struct ddsi_domaingv *gv, const struct xt_type *t)
{
  if (xt_is_primitive (t) || xt_is_string (t) || xt_is_enumerated (t))
    return true;
  switch (t->_d)
  {
    case DDS_XTypes_TK_SEQUENCE:
      return xt_is_delimited (gv, ddsi_xt_unalias (&t->_u.seq.c.element_type->xt));
    case DDS_XTypes_TK_ARRAY:
      return xt_is_delimited (gv, ddsi_xt_unalias (&t->_u.array.c.element_type->xt));
    case DDS_XTypes_TK_MAP:
      return xt_is_delimited (gv, ddsi_xt_unalias (&t->_u.map.key_type->xt)) && xt_is_delimited (gv, ddsi_xt_unalias (&t->_u.map.c.element_type->xt));
  }
  uint32_t ext = xt_get_extensibility (t);
  if (ext == DDS_XTypes_IS_APPENDABLE) /* FIXME: && encoding == XCDR2 */
    return true;
  return ext == DDS_XTypes_IS_MUTABLE;
}

static bool xt_is_equivalent_minimal (const struct xt_type *t1, const struct xt_type *t2)
{
  // Minimal equivalence relation (XTypes spec v1.3 section 7.3.4.7)
  if (xt_is_fully_descriptive (t1) || xt_is_minimal_hash_typeid (t1))
  {
    if (!ddsi_typeid_compare (&t1->id, &t2->id))
      return true;
  }
  return false;
}

static bool xt_is_strongly_assignable_from (struct ddsi_domaingv *gv, const struct xt_type *t1a, const struct xt_type *t2a, const dds_type_consistency_enforcement_qospolicy_t *tce)
{
  const struct xt_type *t1 = ddsi_xt_unalias (t1a), *t2 = ddsi_xt_unalias (t2a);
  if (xt_is_equivalent_minimal (t1, t2))
    return true;
  return xt_is_delimited (gv, t2) && ddsi_xt_is_assignable_from (gv, t1, t2, tce);
}

static bool xt_bounds_eq (const struct DDS_XTypes_LBoundSeq *a, const struct DDS_XTypes_LBoundSeq *b)
{
  if (!a || !b)
    return false;
  if (a->_length != b->_length)
    return false;
  return !memcmp (a->_buffer, b->_buffer, a->_length * sizeof (*a->_buffer));
}

static bool xt_namehash_eq (const DDS_XTypes_NameHash *n1, const DDS_XTypes_NameHash *n2)
{
  return !memcmp (n1, n2, sizeof (*n1));
}

static bool xt_union_label_selects (const struct DDS_XTypes_UnionCaseLabelSeq *ls1, const struct DDS_XTypes_UnionCaseLabelSeq *ls2)
{
  /* UnionCaseLabelSeq is ordered by value (as noted in typeobject idl) */
  uint32_t i1 = 0, i2 = 0;
  while (i1 < ls1->_length && i2 < ls2->_length)
  {
    if (ls1->_buffer[i1] == ls2->_buffer[i2])
      return true;
    else if (ls1->_buffer[i1] < ls2->_buffer[i2])
      i1++;
    else
      i2++;
  }
  return false;
}

static bool xt_union_labels_match (const struct DDS_XTypes_UnionCaseLabelSeq *ls1, const struct DDS_XTypes_UnionCaseLabelSeq *ls2)
{
  /* UnionCaseLabelSeq is ordered by value (as noted in typeobject idl) */
  for (uint32_t i = 0; i < ls1->_length; i++)
    if (i >= ls2->_length || ls1->_buffer[i] != ls2->_buffer[i])
      return false;
  return true;
}

static bool xt_is_assignable_from_enum (const struct xt_type *t1, const struct xt_type *t2)
{
  assert (t1->_d == DDS_XTypes_TK_ENUM);
  assert (t2->_d == DDS_XTypes_TK_ENUM);
  /* Note: extensibility flags not defined, see https://issues.omg.org/issues/DDSXTY14-24 */
  if (xt_get_extensibility (t1) != xt_get_extensibility (t2))
    return false;
  /* Members are ordered by increasing value (XTypes 1.3 spec 7.3.4.5) */
  uint32_t i1 = 0, i2 = 0, i1_max = t1->_u.enum_type.literals.length, i2_max = t2->_u.enum_type.literals.length;
  while (i1 < i1_max && i2 < i2_max)
  {
    struct xt_enum_literal *l1 = &t1->_u.enum_type.literals.seq[i1], *l2 = &t2->_u.enum_type.literals.seq[i2];
    if (l1->value == l2->value)
    {
      /* FIXME: implement @ignore_literal_names */
      if (!xt_namehash_eq (&l1->detail.name_hash, &l2->detail.name_hash))
        return false;
      i1++;
      i2++;
    }
    else if (xt_get_extensibility (t1) == DDS_XTypes_IS_FINAL)
      return false;
    else if (l1->value < l2->value)
      i1++;
    else
      i2++;
  }
  if ((i1 != i1_max || i2 != i2_max) && xt_get_extensibility (t1) == DDS_XTypes_IS_FINAL)
    return false;
  return true;
}

static bool xt_is_assignable_from_union (struct ddsi_domaingv *gv, const struct xt_type *t1, const struct xt_type *t2, const dds_type_consistency_enforcement_qospolicy_t *tce)
{
  assert (t1->_d == DDS_XTypes_TK_UNION);
  assert (t2->_d == DDS_XTypes_TK_UNION);
  if (xt_get_extensibility (t1) != xt_get_extensibility (t2))
    return false;
  if (!xt_is_strongly_assignable_from (gv, ddsi_xt_unalias (&t1->_u.union_type.disc_type->xt), ddsi_xt_unalias (&t2->_u.union_type.disc_type->xt), tce))
    return false;

  /* Rule: Either the discriminators of both T1 and T2 are keys or neither are keys. */
  if ((t1->_u.union_type.disc_flags & DDS_XTypes_IS_KEY) != (t2->_u.union_type.disc_flags & DDS_XTypes_IS_KEY))
    return false;

  /* Note that union members are ordered by their member index (=ordering in idl) and not by their member ID */
  uint32_t i1_max = t1->_u.union_type.members.length, i2_max = t2->_u.union_type.members.length;
  bool any_match = false;
  for (uint32_t i1 = 0; i1 < i1_max; i1++)
  {
    struct xt_union_member *m1 = &t1->_u.union_type.members.seq[i1];
    const struct xt_type *m1t = ddsi_xt_unalias (&m1->type->xt);
    bool m2_id_match = false, m2_labels_match = true, t1_selects_t2_member = false;
    struct xt_union_member *def_m2 = NULL;
    for (uint32_t i2 = i1; i2 < i2_max + i1; i2++)
    {
      struct xt_union_member *m2 = &t2->_u.union_type.members.seq[i2 % i2_max];
      const struct xt_type *m2t = ddsi_xt_unalias (&m2->type->xt);
      if (m1->id == m2->id)
      {
        m2_id_match = true;

        /* Rule: Any members in T1 and T2 that have the same name also have the same ID and any members
        with the same ID also have the same name. */
        if (!xt_namehash_eq (&m1->detail.name_hash, &m2->detail.name_hash) && (!tce || !tce->ignore_member_names))
          return false;
      }

      /* Rule: If T1 and T2 both have default labels, the type associated with T1 default member is assignable from
          the type associated with T2 default member. */
      if ((m1->flags & DDS_XTypes_IS_DEFAULT) && (m2->flags & DDS_XTypes_IS_DEFAULT))
      {
        if (!ddsi_xt_is_assignable_from (gv, m1t, m2t, tce))
          return false;
      }

      if (m1->id == m2->id && !xt_union_labels_match (&m1->label_seq, &m2->label_seq))
        m2_labels_match = false;
      if (xt_union_label_selects (&m1->label_seq, &m2->label_seq))
        t1_selects_t2_member = true;
      if (m2->flags & DDS_XTypes_IS_DEFAULT)
        def_m2 = m2;
    } /* loop T2 members */

    /* Rule: If any non-default labels in T1 that select the default member in T2, the type of the member in T1 is
      assignable from the type of the T2 default member. */
    if (!(m1->flags & DDS_XTypes_IS_DEFAULT) && !t1_selects_t2_member && def_m2)
    {
      if (!ddsi_xt_is_assignable_from (gv, m1t, ddsi_xt_unalias (&def_m2->type->xt), tce))
        return false;
    }

    /* Rule: If T1 (and therefore T2) extensibility is final or prevent type widening is set then the
       set of labels is identical. */
    if ((xt_get_extensibility (t1) == DDS_XTypes_IS_FINAL || (tce && tce->prevent_type_widening)) && (!m2_id_match || !m2_labels_match))
      return false;
    if (t1_selects_t2_member)
      any_match = true;
  } /* loop T1 members */

  /* Rule: For all non-default labels in T2 that select some member in T1 (including selecting the member in T1’s
    default label), the type of the selected member in T1 is assignable from the type of the T2 member. */
  for (uint32_t i2 = 0; i2 < i2_max; i2++)
  {
    // FIXME: integrate this in the loop above to get better performance
    struct xt_union_member *m2 = &t2->_u.union_type.members.seq[i2];
    if (m2->flags & DDS_XTypes_IS_DEFAULT)
      continue;
    struct xt_union_member *def_m1 = NULL, *sel_m1 = NULL;
    for (uint32_t i1 = i2; i1 < i1_max + i2; i1++)
    {
      struct xt_union_member *m1 = &t1->_u.union_type.members.seq[i1 % i1_max];
      if (xt_union_label_selects (&m2->label_seq, &m1->label_seq))
        sel_m1 = m1;
      if (m1->flags & DDS_XTypes_IS_DEFAULT)
        def_m1 = m1;
    }
    if ((sel_m1 || def_m1) && !ddsi_xt_is_assignable_from (gv, ddsi_xt_unalias (sel_m1 ? &sel_m1->type->xt : &def_m1->type->xt), ddsi_xt_unalias(&m2->type->xt), tce))
      return false;
    if (!sel_m1 && tce && tce->prevent_type_widening)
      return false;
  }

  /* Rule: [extensibility is final], otherwise, they have at least one common label other than the default label. */
  if (!any_match)
    return false;
  return true;
}

static bool xt_is_assignable_from_struct (struct ddsi_domaingv *gv, const struct xt_type *t1, const struct xt_type *t2, const dds_type_consistency_enforcement_qospolicy_t *tce)
{
  assert (t1->_d == DDS_XTypes_TK_STRUCTURE);
  assert (t2->_d == DDS_XTypes_TK_STRUCTURE);
  bool result = false;
  struct xt_type *te1 = (struct xt_type *) t1, *te2 = (struct xt_type *) t2;
  if (xt_get_extensibility (t1) != xt_get_extensibility (t2))
    goto struct_failed;
  if (xt_has_basetype (t1))
    if ((te1 = xt_expand_basetype (gv, t1)) == NULL)
      goto struct_failed;
  if (xt_has_basetype (t2))
    if ((te2 = xt_expand_basetype (gv, t2)) == NULL)
      goto struct_failed;
  /* Note that struct members are ordered by their member index (=ordering in idl) and not by their member ID (although the
      TypeObject idl states that its ordered by member_id...) */
  uint32_t i1_max = te1->_u.structure.members.length, i2_max = te2->_u.structure.members.length;
  bool any_member_match = false;
  for (uint32_t i1 = 0; i1 < i1_max; i1++)
  {
    const struct xt_struct_member *m1 = &te1->_u.structure.members.seq[i1];
    const struct xt_type *m1t = ddsi_xt_unalias (&m1->type->xt);
    if (xt_is_unresolved (m1t))
    {
      struct ddsi_typeid_str tidstr;
      GVWARNING ("assignability check: member %"PRIu32" type %s unresolved in xt_is_assignable_from_struct\n", m1->id, ddsi_make_typeid_str (&tidstr, &m1t->id));
      goto struct_failed;
    }

    bool match = false,
      m1_opt = (m1->flags & DDS_XTypes_IS_OPTIONAL),
      m1_mu = (m1->flags & DDS_XTypes_IS_MUST_UNDERSTAND),
      m1_k = (m1->flags & DDS_XTypes_IS_KEY);
    for (uint32_t i2 = i1; i2 < i2_max + i1; i2++)
    {
      struct xt_struct_member *m2 = &te2->_u.structure.members.seq[i2 % i2_max];
      if (m1->id == m2->id)
      {
        bool m2_k = (m2->flags & DDS_XTypes_IS_KEY);
        any_member_match = true;
        match = true;
        const struct xt_type *m2t = ddsi_xt_unalias (&m2->type->xt);
        if (xt_is_unresolved (m2t))
        {
          struct ddsi_typeid_str tidstr;
          GVWARNING ("assignability check: member %"PRIu32" type %s unresolved in xt_is_assignable_from_struct\n", m2->id, ddsi_make_typeid_str (&tidstr, &m2t->id));
          goto struct_failed;
        }

        /* Rule: "Any members in T1 and T2 that have the same name also have the same ID and any members with the
            same ID also have the same name." */
        if (!xt_namehash_eq (&m1->detail.name_hash, &m2->detail.name_hash) && (!tce || !tce->ignore_member_names))
          goto struct_failed;

        /* Rule: "For any member m2 in T2, if there is a member m1 in T1 with the same member ID, then the type
            KeyErased(m1.type) is-assignable from the type KeyErased(m2.type) */
        struct xt_type *m1_ke = xt_type_key_erased (gv, m1t),
          *m2_ke = xt_type_key_erased (gv, m2t);
        bool ke_assignable = ddsi_xt_is_assignable_from (gv, m1_ke, m2_ke, tce);
        ddsi_xt_type_fini (gv, m1_ke);
        ddsrt_free (m1_ke);
        ddsi_xt_type_fini (gv, m2_ke);
        ddsrt_free (m2_ke);

        if (!ke_assignable)
          goto struct_failed;
        /* Rule: "For any string key member m2 in T2, the m1 member of T1 with the same member ID verifies m1.type.length >= m2.type.length. */
        if (m2_k && xt_is_string (m2t) && xt_string_bound (m1t) < xt_string_bound (m2t))
          goto struct_failed;
        /* Rule: "For any enumerated key member m2 in T2, the m1 member of T1 with the same member ID verifies that all
            literals in m2.type appear as literals in m1.type" */
        if (m2_k && xt_is_enumerated (m2t))
        {
          uint32_t ki1 = 0, ki2 = 0, ki1_max = m1t->_u.enum_type.literals.length, ki2_max = m2t->_u.enum_type.literals.length;
          while (ki1 < ki1_max && ki2 < ki2_max)
          {
            struct xt_enum_literal *kl1 = &m1t->_u.enum_type.literals.seq[ki1], *kl2 = &m2t->_u.enum_type.literals.seq[ki2];
            if (kl1->value == kl2->value)
            {
              ki1++;
              ki2++;
            }
            else if (kl1->value < kl2->value)
              ki1++;
            else
              goto struct_failed;
          }
        }

        /* Rule: "For any sequence or map key member m2 in T2, the m1 member of T1 with the same member ID verifies m1.type.length >= m2.type.length" */
        if (m2_k && m2t->_d == DDS_XTypes_TK_SEQUENCE && m1t->_u.seq.bound < m2t->_u.seq.bound)
          goto struct_failed;
        if (m2_k && m2t->_d == DDS_XTypes_TK_MAP && m1t->_u.map.bound < m2t->_u.map.bound)
          goto struct_failed;
        /* Rule: "For any structure or union key member m2 in T2, the m1 member of T1 with the same member ID verifies that KeyHolder(m1.type)
            isassignable-from KeyHolder(m2.type)." */
        if (m2_k && (m2t->_d == DDS_XTypes_TK_STRUCTURE || m2t->_d == DDS_XTypes_TK_UNION))
        {
          struct xt_type *m1_kh = xt_type_keyholder (gv, m1t),
            *m2_kh = xt_type_keyholder (gv, m2t);
          bool kh_assignable = ddsi_xt_is_assignable_from (gv, m1_kh, m2_kh, tce);
          ddsi_xt_type_fini (gv, m1_kh);
          ddsrt_free (m1_kh);
          ddsi_xt_type_fini (gv, m2_kh);
          ddsrt_free (m2_kh);
          if (!kh_assignable)
            goto struct_failed;
        }
        /* Rule: "For any union key member m2 in T2, the m1 member of T1 with the same member ID verifies that: For every discriminator value of m2.type
            that selects a member m22 in m2.type, the discriminator value selects a member m11 in m1.type that verifies KeyHolder(m11.type)
            is-assignable-from KeyHolder(m22.type)." */
        if (m2_k && m2t->_d == DDS_XTypes_TK_UNION)
        {
          uint32_t ki1_max = m1t->_u.union_type.members.length, ki2_max = m2t->_u.union_type.members.length;
          for (uint32_t ki1 = 0; ki1 < ki1_max; ki1++)
          {
            struct xt_union_member *km1 = &m1t->_u.union_type.members.seq[i1];
            for (uint32_t ki2 = ki1; ki2 < ki2_max + ki1; ki2++)
            {
              struct xt_union_member *km2 = &m2t->_u.union_type.members.seq[ki2 % ki2_max];
              if (xt_union_label_selects (&km1->label_seq, &km2->label_seq))
              {
                const struct xt_type *km1_t = ddsi_xt_unalias (&km1->type->xt),
                  *km2_t = ddsi_xt_unalias (&km2->type->xt);
                if (xt_is_unresolved (km1_t) || xt_is_unresolved (km2_t))
                {
                  struct ddsi_typeid_str tidstr;
                  GVWARNING ("assignability check: union member %"PRIu32" type %s unresolved in xt_is_assignable_from_struct\n",
                      (xt_is_unresolved (km1_t) ? km1 : km2)->id, ddsi_make_typeid_str (&tidstr, &(xt_is_unresolved (km1_t) ? km1_t : km2_t)->id));
                  goto struct_failed;
                }
                struct xt_type *km1_kh = xt_type_keyholder (gv, km1_t),
                  *km2_kh = xt_type_keyholder (gv, km2_t);
                bool kh_assignable = ddsi_xt_is_assignable_from (gv, km1_kh, km2_kh, tce);
                ddsi_xt_type_fini (gv, km1_kh);
                ddsrt_free (km1_kh);
                ddsi_xt_type_fini (gv, km2_kh);
                ddsrt_free (km2_kh);
                if (!kh_assignable)
                  goto struct_failed;
              }
            }
          }
        }
        break;
      }
    } /* for members in T2 */

    /* Rule (for T1 members): "Members for which both optional is false and must_understand is true in either T1 or T2 appear (i.e., have a
        corresponding member of the same member ID) in both T1 and T2. */
    if (!m1_opt && m1_mu && !match)
      goto struct_failed;
    /* Rule (for T1 members): "Members marked as key in either T1 or T2 appear (i.e., have a corresponding member of the same member ID)
        in both T1 and T2." */
    if (m1_k && !match)
      goto struct_failed;
    /* Rules:
        - if T1 is appendable, then members with the same member_index have the same member ID, the same setting for the
          optional attribute and the T1 member type is strongly assignable from the T2 member type
        - if T1 is final, then they meet the same condition as for T1 being appendable and ... (see below) */
    struct xt_struct_member *m2 = &te2->_u.structure.members.seq[i1];
    if ((xt_get_extensibility (te1) == DDS_XTypes_IS_APPENDABLE && i1 < i2_max) || xt_get_extensibility (te1) == DDS_XTypes_IS_FINAL)
    {
      if (i1 >= i2_max)
        goto struct_failed;
      if (m1->id != m2->id || (m1->flags & DDS_XTypes_IS_OPTIONAL) != (m2->flags & DDS_XTypes_IS_OPTIONAL) || !xt_is_strongly_assignable_from (gv, m1t, ddsi_xt_unalias (&m2->type->xt), tce))
        goto struct_failed;
    }
    /* if T1 is final, or prevent type-widening is set: ... [continued] in addition T1 and T2 have the same set of member IDs */
    if ((xt_get_extensibility (te1) == DDS_XTypes_IS_FINAL || (tce && tce->prevent_type_widening && !(m2->flags & DDS_XTypes_IS_OPTIONAL))) && !match)
      goto struct_failed;
  } /* for members in T1 */


  /* Rules (for T2 members):
      - Members for which both optional is false and must_understand is true in either T1 or T2 appear (i.e., have a corresponding member
        of the same member ID) in both T1 and T2
      - Members marked as key in either T1 or T2 appear (i.e., have a corresponding member of the same member ID) in both T1 and T2.
      - If T1 is final, or prevent type-widening is set: ... [continued] in addition T1 and T2 have the same set of member IDs
    [Note that the first 2 rules are checked here for T2 members only, in the loop above this was checked for T1 members] */
  for (uint32_t i2 = 0; i2 < i2_max; i2++)
  {
    struct xt_struct_member *m2 = &te2->_u.structure.members.seq[i2 % i2_max];
    bool match = false;
    if ((!(m2->flags & DDS_XTypes_IS_OPTIONAL) && (m2->flags & DDS_XTypes_IS_MUST_UNDERSTAND))
        || (m2->flags & DDS_XTypes_IS_KEY)
        || xt_get_extensibility (te1) == DDS_XTypes_IS_FINAL
        || (tce && tce->prevent_type_widening && !(m2->flags & DDS_XTypes_IS_OPTIONAL)))
    {
      for (uint32_t i1 = i2; !match && i1 < i1_max + i2; i1++)
        match = (te1->_u.structure.members.seq[i1 % i1_max].id == m2->id);
      if (!match)
        goto struct_failed;
    }
  }
  /* Rule: There is at least one member m1 of T1 and one corresponding member m2 of T2 such that m1.id == m2.id */
  if (!any_member_match)
    goto struct_failed;
  result = true;

struct_failed:
  if (te1 && te1 != t1)
  {
    ddsi_xt_type_fini (gv, te1);
    ddsrt_free (te1);
  }
  if (te2 && te2 != t2)
  {
    ddsi_xt_type_fini (gv, te2);
    ddsrt_free (te2);
  }
  return result;
}

bool ddsi_xt_is_assignable_from (struct ddsi_domaingv *gv, const struct xt_type *rd_xt, const struct xt_type *wr_xt, const dds_type_consistency_enforcement_qospolicy_t *tce)
{
  const struct xt_type *t1 = ddsi_xt_unalias (rd_xt), *t2 = ddsi_xt_unalias (wr_xt);
  if (xt_is_unresolved (t1) || xt_is_unresolved (t2))
  {
    struct ddsi_typeid_str tidstr;
    GVWARNING ("assignability check: unresolved type %s in ddsi_xt_is_assignable_from\n", ddsi_make_typeid_str (&tidstr, &(xt_is_unresolved (t1) ? t1 : t2)->id));
    return false;
  }

  if (xt_is_equivalent_minimal (t1, t2))
    return true;

  /* Bitmask type: must be equal, except bitmask can be assigned to uint types and vv */
  if (t1->_d == DDS_XTypes_TK_BITMASK || t2->_d == DDS_XTypes_TK_BITMASK)
  {
    const struct xt_type *t_bm = t1->_d == DDS_XTypes_TK_BITMASK ? t1 : t2;
    const struct xt_type *t_other = t1->_d == DDS_XTypes_TK_BITMASK ? t2 : t1;
    DDS_XTypes_BitBound bb = t_bm->_u.bitmask.bit_bound;
    switch (t_other->_d)
    {
      case DDS_XTypes_TK_BITMASK:
        return bb == t_other->_u.bitmask.bit_bound;
      // case TK_UINT8:   /* FIXME: TK_UINT8 not defined in idl */
      //   return bb >= 1 && bb <= 8;
      case DDS_XTypes_TK_UINT16:
        return bb >= 9 && bb <= 16;
      case DDS_XTypes_TK_UINT32:
        return bb >= 17 && bb <= 32;
      case DDS_XTypes_TK_UINT64:
        return bb >= 33 && bb <= 64;
      default:
        return false;
    }
  }
  /* Enum type */
  if (t1->_d == DDS_XTypes_TK_ENUM && t2->_d == DDS_XTypes_TK_ENUM)
    return xt_is_assignable_from_enum (t1, t2);

  /* String types: character type must be assignable, bound not checked for assignability, unless ignore_string_bounds is false */
  if ((t1->_d == DDS_XTypes_TK_STRING8 && t2->_d == DDS_XTypes_TK_STRING8))
    return !tce || tce->ignore_string_bounds || xt_check_bound (t1->_u.str8.bound, t2->_u.str8.bound);
  if ((t1->_d == DDS_XTypes_TK_STRING16 && t2->_d == DDS_XTypes_TK_STRING16))
    return !tce || tce->ignore_string_bounds || xt_check_bound (t1->_u.str16.bound, t2->_u.str16.bound);

  /* Collection types */
  if (t1->_d == DDS_XTypes_TK_ARRAY && t2->_d == DDS_XTypes_TK_ARRAY)
    return xt_bounds_eq (&t1->_u.array.bounds, &t2->_u.array.bounds)
      && xt_is_strongly_assignable_from (gv, &t1->_u.array.c.element_type->xt, &t2->_u.array.c.element_type->xt, tce);
  if (t1->_d == DDS_XTypes_TK_SEQUENCE && t2->_d == DDS_XTypes_TK_SEQUENCE)
    return (!tce || tce->ignore_sequence_bounds || xt_check_bound (t1->_u.seq.bound, t2->_u.seq.bound))
      && xt_is_strongly_assignable_from (gv, &t1->_u.seq.c.element_type->xt, &t2->_u.seq.c.element_type->xt, tce);
  if (t1->_d == DDS_XTypes_TK_MAP && t2->_d == DDS_XTypes_TK_MAP)
    return xt_is_strongly_assignable_from (gv, &t1->_u.map.key_type->xt, &t2->_u.map.key_type->xt, tce)
      && xt_is_strongly_assignable_from (gv, &t1->_u.map.c.element_type->xt, &t2->_u.map.c.element_type->xt, tce);

  // Aggregated types
  if (t1->_d == DDS_XTypes_TK_UNION && t2->_d == DDS_XTypes_TK_UNION)
    return xt_is_assignable_from_union (gv, t1, t2, tce);
  if (t1->_d == DDS_XTypes_TK_STRUCTURE && t2->_d == DDS_XTypes_TK_STRUCTURE)
    return xt_is_assignable_from_struct (gv, t1, t2, tce);

  return false;
}

ddsi_typeid_kind_t ddsi_typeid_kind (const ddsi_typeid_t *type_id)
{
  if (!ddsi_typeid_is_hash (type_id))
    return DDSI_TYPEID_KIND_FULLY_DESCRIPTIVE;
  return ddsi_typeid_is_minimal (type_id) ? DDSI_TYPEID_KIND_MINIMAL : DDSI_TYPEID_KIND_COMPLETE;
}

void ddsi_xt_get_typeobject_impl (const struct xt_type *xt, struct DDS_XTypes_TypeObject *to)
{
  assert (xt);
  assert (to);
  assert (xt->has_obj);
  assert (!xt_is_fully_descriptive (xt));

  memset (to, 0, sizeof (*to));
  if (xt->kind == DDSI_TYPEID_KIND_MINIMAL)
  {
    to->_d = DDS_XTypes_EK_MINIMAL;
    struct DDS_XTypes_MinimalTypeObject *mto = &to->_u.minimal;
    mto->_d = xt->_d;
    switch (xt->_d)
    {
      case DDS_XTypes_TK_ALIAS:
      {
        struct DDS_XTypes_MinimalAliasType *malias = &mto->_u.alias_type;
        ddsi_typeid_copy_to_impl (&malias->body.common.related_type, &xt->_u.alias.related_type->xt.id);
        malias->body.common.related_flags = xt->_u.alias.related_flags;
        break;
      }
      case DDS_XTypes_TK_ANNOTATION:
        abort (); /* FIXME: not implemented */
        break;
      case DDS_XTypes_TK_STRUCTURE:
      {
        struct DDS_XTypes_MinimalStructType *mstruct = &mto->_u.struct_type;
        mstruct->struct_flags = xt->_u.structure.flags;
        if (xt->_u.structure.base_type)
          ddsi_typeid_copy_to_impl (&mstruct->header.base_type, &xt->_u.structure.base_type->xt.id);
        mstruct->member_seq._buffer = ddsrt_malloc (xt->_u.structure.members.length * sizeof (*mstruct->member_seq._buffer));
        mstruct->member_seq._length = xt->_u.structure.members.length;
        mstruct->member_seq._release = true;
        for (uint32_t n = 0; n < xt->_u.structure.members.length; n++)
        {
          mstruct->member_seq._buffer[n].common.member_id = xt->_u.structure.members.seq[n].id;
          mstruct->member_seq._buffer[n].common.member_flags = xt->_u.structure.members.seq[n].flags;
          ddsi_typeid_copy_to_impl (&mstruct->member_seq._buffer[n].common.member_type_id, &xt->_u.structure.members.seq[n].type->xt.id);
          get_minimal_member_detail (&mstruct->member_seq._buffer[n].detail, &xt->_u.structure.members.seq[n].detail);
        }
        break;
      }
      case DDS_XTypes_TK_UNION:
      {
        struct DDS_XTypes_MinimalUnionType *munion = &mto->_u.union_type;
        munion->union_flags = xt->_u.union_type.flags;
        ddsi_typeid_copy_to_impl (&munion->discriminator.common.type_id, &xt->_u.union_type.disc_type->xt.id);
        munion->discriminator.common.member_flags = xt->_u.union_type.disc_flags;
        munion->member_seq._buffer = ddsrt_malloc (xt->_u.union_type.members.length * sizeof (*munion->member_seq._buffer));
        munion->member_seq._length = munion->member_seq._maximum = xt->_u.union_type.members.length;
        munion->member_seq._release = true;
        for (uint32_t n = 0; n < xt->_u.union_type.members.length; n++)
        {
          munion->member_seq._buffer[n].common.member_id = xt->_u.union_type.members.seq[n].id;
          munion->member_seq._buffer[n].common.member_flags = xt->_u.union_type.members.seq[n].flags;
          ddsi_typeid_copy_to_impl (&munion->member_seq._buffer[n].common.type_id, &xt->_u.union_type.members.seq[n].type->xt.id);
          munion->member_seq._buffer[n].common.label_seq._length = xt->_u.union_type.members.seq[n].label_seq._length;
          munion->member_seq._buffer[n].common.label_seq._buffer = ddsrt_memdup (xt->_u.union_type.members.seq[n].label_seq._buffer,
            xt->_u.union_type.members.seq[n].label_seq._length * sizeof (*xt->_u.union_type.members.seq[n].label_seq._buffer));
          munion->member_seq._buffer[n].common.label_seq._release = true;
          get_minimal_member_detail (&munion->member_seq._buffer[n].detail, &xt->_u.union_type.members.seq[n].detail);
        }
        break;
      }
      case DDS_XTypes_TK_BITSET:
      {
        struct DDS_XTypes_MinimalBitsetType *mbitset = &mto->_u.bitset_type;
        mbitset->field_seq._length = xt->_u.bitset.fields.length;
        mbitset->field_seq._buffer = ddsrt_malloc (xt->_u.bitset.fields.length * sizeof (*mbitset->field_seq._buffer));
        mbitset->field_seq._release = true;
        for (uint32_t n = 0; n < xt->_u.bitset.fields.length; n++)
        {
          mbitset->field_seq._buffer[n].common.position = xt->_u.bitset.fields.seq[n].position;
          mbitset->field_seq._buffer[n].common.flags = xt->_u.bitset.fields.seq[n].flags;
          mbitset->field_seq._buffer[n].common.bitcount = xt->_u.bitset.fields.seq[n].bitcount;
          mbitset->field_seq._buffer[n].common.holder_type = xt->_u.bitset.fields.seq[n].holder_type;
          memcpy (mbitset->field_seq._buffer[n].name_hash, xt->_u.bitset.fields.seq[n].detail.name_hash, sizeof (mbitset->field_seq._buffer[n].name_hash));
        }
        break;
      }
      case DDS_XTypes_TK_SEQUENCE:
        ddsi_typeid_copy_to_impl (&mto->_u.sequence_type.element.common.type, &xt->_u.seq.c.element_type->xt.id);
        mto->_u.sequence_type.element.common.element_flags = xt->_u.seq.c.element_flags;
        mto->_u.sequence_type.header.common.bound = xt->_u.seq.bound;
        break;
      case DDS_XTypes_TK_ARRAY:
        ddsi_typeid_copy_to_impl (&mto->_u.array_type.element.common.type, &xt->_u.array.c.element_type->xt.id);
        mto->_u.array_type.element.common.element_flags = xt->_u.array.c.element_flags;
        xt_lbounds_dup (&mto->_u.array_type.header.common.bound_seq, &xt->_u.array.bounds);
        break;
      case DDS_XTypes_TK_MAP:
        ddsi_typeid_copy_to_impl (&mto->_u.map_type.element.common.type, &xt->_u.map.c.element_type->xt.id);
        mto->_u.array_type.element.common.element_flags = xt->_u.map.c.element_flags;
        ddsi_typeid_copy_to_impl (&mto->_u.map_type.key.common.type, &xt->_u.map.key_type->xt.id);
        mto->_u.map_type.header.common.bound = xt->_u.map.bound;
        break;
      case DDS_XTypes_TK_ENUM:
      {
        struct DDS_XTypes_MinimalEnumeratedType *menum = &mto->_u.enumerated_type;
        menum->enum_flags = xt->_u.enum_type.flags;
        menum->header.common.bit_bound = xt->_u.enum_type.bit_bound;
        menum->literal_seq._length = xt->_u.enum_type.literals.length;
        menum->literal_seq._buffer = ddsrt_malloc (xt->_u.enum_type.literals.length * sizeof (*menum->literal_seq._buffer));
        menum->literal_seq._release = true;
        for (uint32_t n = 0; n < xt->_u.enum_type.literals.length; n++)
        {
          menum->literal_seq._buffer[n].common.value = xt->_u.enum_type.literals.seq[n].value;
          menum->literal_seq._buffer[n].common.flags = xt->_u.enum_type.literals.seq[n].flags;
          get_minimal_member_detail (&menum->literal_seq._buffer[n].detail, &xt->_u.enum_type.literals.seq[n].detail);
        }
        break;
      }
      case DDS_XTypes_TK_BITMASK:
      {
        struct DDS_XTypes_MinimalBitmaskType *mbitmask = &mto->_u.bitmask_type;
        mbitmask->bitmask_flags = xt->_u.bitmask.flags;
        mbitmask->header.common.bit_bound = xt->_u.bitmask.bit_bound;
        mbitmask->flag_seq._length = xt->_u.bitmask.bitflags.length;
        mbitmask->flag_seq._buffer = ddsrt_malloc (xt->_u.bitmask.bitflags.length * sizeof (*mbitmask->flag_seq._buffer));
        mbitmask->flag_seq._release = true;
        for (uint32_t n = 0; n < xt->_u.bitmask.bitflags.length; n++)
        {
          mbitmask->flag_seq._buffer[n].common.position = xt->_u.bitmask.bitflags.seq[n].position;
          mbitmask->flag_seq._buffer[n].common.flags = xt->_u.bitmask.bitflags.seq[n].flags;
          get_minimal_member_detail (&mbitmask->flag_seq._buffer[n].detail, &xt->_u.bitmask.bitflags.seq[n].detail);
        }
        break;
      }
      default:
        abort (); /* not supported */
        break;
    }
  }
  else
  {
    assert (xt->kind == DDSI_TYPEID_KIND_COMPLETE);
    to->_d = DDS_XTypes_EK_COMPLETE;
    struct DDS_XTypes_CompleteTypeObject *cto = &to->_u.complete;
    cto->_d = xt->_d;
    switch (xt->_d)
    {
      case DDS_XTypes_TK_ALIAS:
      {
        struct DDS_XTypes_CompleteAliasType *calias = &cto->_u.alias_type;
        get_type_detail (&calias->header.detail, &xt->_u.alias.detail);
        ddsi_typeid_copy_to_impl (&calias->body.common.related_type, &xt->_u.alias.related_type->xt.id);
        calias->body.common.related_flags = xt->_u.alias.related_flags;
        break;
      }
      case DDS_XTypes_TK_ANNOTATION:
        abort (); /* FIXME: not implemented */
        break;
      case DDS_XTypes_TK_STRUCTURE:
      {
        struct DDS_XTypes_CompleteStructType *cstruct = &cto->_u.struct_type;
        cstruct->struct_flags = xt->_u.structure.flags;
        if (xt->_u.structure.base_type)
          ddsi_typeid_copy_to_impl (&cstruct->header.base_type, &xt->_u.structure.base_type->xt.id);
        else
          cstruct->header.base_type._d = 0;

        get_type_detail (&cstruct->header.detail, &xt->_u.structure.detail);
        cstruct->member_seq._buffer = ddsrt_malloc (xt->_u.structure.members.length * sizeof (*cstruct->member_seq._buffer));
        cstruct->member_seq._length = xt->_u.structure.members.length;
        cstruct->member_seq._release = true;
        for (uint32_t n = 0; n < xt->_u.structure.members.length; n++)
        {
          cstruct->member_seq._buffer[n].common.member_id = xt->_u.structure.members.seq[n].id;
          cstruct->member_seq._buffer[n].common.member_flags = xt->_u.structure.members.seq[n].flags;
          ddsi_typeid_copy_to_impl (&cstruct->member_seq._buffer[n].common.member_type_id, &xt->_u.structure.members.seq[n].type->xt.id);
          get_member_detail (&cstruct->member_seq._buffer[n].detail, &xt->_u.structure.members.seq[n].detail);
        }
        break;
      }
      case DDS_XTypes_TK_UNION:
      {
        struct DDS_XTypes_CompleteUnionType *cunion = &cto->_u.union_type;
        cunion->union_flags = xt->_u.union_type.flags;
        get_type_detail (&cunion->header.detail, &xt->_u.union_type.detail);
        ddsi_typeid_copy_to_impl (&cunion->discriminator.common.type_id, &xt->_u.union_type.disc_type->xt.id);
        cunion->discriminator.common.member_flags = xt->_u.union_type.disc_flags;
        cunion->member_seq._buffer = ddsrt_malloc (xt->_u.union_type.members.length * sizeof (*cunion->member_seq._buffer));
        cunion->member_seq._length = cunion->member_seq._maximum = xt->_u.union_type.members.length;
        cunion->member_seq._release = true;
        for (uint32_t n = 0; n < xt->_u.union_type.members.length; n++)
        {
          cunion->member_seq._buffer[n].common.member_id = xt->_u.union_type.members.seq[n].id;
          cunion->member_seq._buffer[n].common.member_flags = xt->_u.union_type.members.seq[n].flags;
          ddsi_typeid_copy_to_impl (&cunion->member_seq._buffer[n].common.type_id, &xt->_u.union_type.members.seq[n].type->xt.id);
          cunion->member_seq._buffer[n].common.label_seq._length = xt->_u.union_type.members.seq[n].label_seq._length;
          cunion->member_seq._buffer[n].common.label_seq._buffer = ddsrt_memdup (xt->_u.union_type.members.seq[n].label_seq._buffer,
            xt->_u.union_type.members.seq[n].label_seq._length * sizeof (*xt->_u.union_type.members.seq[n].label_seq._buffer));
          cunion->member_seq._buffer[n].common.label_seq._release = true;
          get_member_detail (&cunion->member_seq._buffer[n].detail, &xt->_u.union_type.members.seq[n].detail);
        }
        break;
      }
      case DDS_XTypes_TK_BITSET:
      {
        struct DDS_XTypes_CompleteBitsetType *cbitset = &cto->_u.bitset_type;
        get_type_detail (&cbitset->header.detail, &xt->_u.bitset.detail);
        cbitset->field_seq._length = xt->_u.bitset.fields.length;
        cbitset->field_seq._buffer = ddsrt_malloc (xt->_u.bitset.fields.length * sizeof (*cbitset->field_seq._buffer));
        cbitset->field_seq._release = true;
        for (uint32_t n = 0; n < xt->_u.bitset.fields.length; n++)
        {
          cbitset->field_seq._buffer[n].common.position = xt->_u.bitset.fields.seq[n].position;
          cbitset->field_seq._buffer[n].common.flags = xt->_u.bitset.fields.seq[n].flags;
          cbitset->field_seq._buffer[n].common.bitcount = xt->_u.bitset.fields.seq[n].bitcount;
          cbitset->field_seq._buffer[n].common.holder_type = xt->_u.bitset.fields.seq[n].holder_type;
          get_member_detail (&cbitset->field_seq._buffer[n].detail, &xt->_u.bitset.fields.seq[n].detail);
        }
        break;
      }
      case DDS_XTypes_TK_SEQUENCE:
        ddsi_typeid_copy_to_impl (&cto->_u.sequence_type.element.common.type, &xt->_u.seq.c.element_type->xt.id);
        cto->_u.sequence_type.element.common.element_flags = xt->_u.seq.c.element_flags;
        cto->_u.sequence_type.header.common.bound = xt->_u.seq.bound;
        break;
      case DDS_XTypes_TK_ARRAY:
        ddsi_typeid_copy_to_impl (&cto->_u.array_type.element.common.type, &xt->_u.array.c.element_type->xt.id);
        cto->_u.array_type.element.common.element_flags = xt->_u.array.c.element_flags;
        xt_lbounds_dup (&cto->_u.array_type.header.common.bound_seq, &xt->_u.array.bounds);
        break;
      case DDS_XTypes_TK_MAP:
        ddsi_typeid_copy_to_impl (&cto->_u.map_type.element.common.type, &xt->_u.map.c.element_type->xt.id);
        cto->_u.array_type.element.common.element_flags = xt->_u.map.c.element_flags;
        ddsi_typeid_copy_to_impl (&cto->_u.map_type.key.common.type, &xt->_u.map.key_type->xt.id);
        cto->_u.map_type.header.common.bound = xt->_u.map.bound;
        break;
      case DDS_XTypes_TK_ENUM:
      {
        struct DDS_XTypes_CompleteEnumeratedType *cenum = &cto->_u.enumerated_type;
        get_type_detail (&cenum->header.detail, &xt->_u.enum_type.detail);
        cenum->enum_flags = xt->_u.enum_type.flags;
        cenum->header.common.bit_bound = xt->_u.enum_type.bit_bound;
        cenum->literal_seq._length = xt->_u.enum_type.literals.length;
        cenum->literal_seq._buffer = ddsrt_malloc (xt->_u.enum_type.literals.length * sizeof (*cenum->literal_seq._buffer));
        cenum->literal_seq._release = true;
        for (uint32_t n = 0; n < xt->_u.enum_type.literals.length; n++)
        {
          cenum->literal_seq._buffer[n].common.value = xt->_u.enum_type.literals.seq[n].value;
          cenum->literal_seq._buffer[n].common.flags = xt->_u.enum_type.literals.seq[n].flags;
          get_member_detail (&cenum->literal_seq._buffer[n].detail, &xt->_u.enum_type.literals.seq[n].detail);
        }
        break;
      }
      case DDS_XTypes_TK_BITMASK:
      {
        struct DDS_XTypes_CompleteBitmaskType *cbitmask = &cto->_u.bitmask_type;
        get_type_detail (&cbitmask->header.detail, &xt->_u.bitmask.detail);
        cbitmask->bitmask_flags = xt->_u.bitmask.flags;
        cbitmask->header.common.bit_bound = xt->_u.bitmask.bit_bound;
        cbitmask->flag_seq._length = xt->_u.bitmask.bitflags.length;
        cbitmask->flag_seq._buffer = ddsrt_malloc (xt->_u.bitmask.bitflags.length * sizeof (*cbitmask->flag_seq._buffer));
        cbitmask->flag_seq._release = true;
        for (uint32_t n = 0; n < xt->_u.bitmask.bitflags.length; n++)
        {
          cbitmask->flag_seq._buffer[n].common.position = xt->_u.bitmask.bitflags.seq[n].position;
          cbitmask->flag_seq._buffer[n].common.flags = xt->_u.bitmask.bitflags.seq[n].flags;
          get_member_detail (&cbitmask->flag_seq._buffer[n].detail, &xt->_u.bitmask.bitflags.seq[n].detail);
        }
        break;
      }
      default:
        abort (); /* not supported */
        break;
    }
  }
}

void ddsi_xt_get_typeobject (const struct xt_type *xt, ddsi_typeobj_t *to)
{
  ddsi_xt_get_typeobject_impl (xt, &to->x);
}
