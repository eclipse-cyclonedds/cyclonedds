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
#include <limits.h>
#include "dds/features.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_xt_typemap.h"
#include "ddsi__typewrap.h"
#include "ddsi__xt_impl.h"
#include "ddsi__typelookup.h"
#include "ddsi__typelib.h"
#include "dds/cdr/dds_cdrstream.h"
#include "dds/ddsc/dds_public_impl.h"

extern inline const struct ddsi_type *ddsi_type_from_xt_type (const struct xt_type *xt);

#define MEMBER_FLAG_COLLECTION_ELEMENT 1u
#define MEMBER_FLAG_STRUCT_MEMBER 2u
#define MEMBER_FLAG_UNION_MEMBER 3u
#define MEMBER_FLAG_UNION_DISC 4u
#define MEMBER_FLAG_ENUM_LITERAL 5u
#define MEMBER_FLAG_ANNOTATION_PARAM 6u
#define MEMBER_FLAG_ALIAS_MEMBER 7u
#define MEMBER_FLAG_BIT_FLAG 8u
#define MEMBER_FLAG_BITSET_MEMBER 9u
#define XT_VALIDATE_MAX_DEPTH 100u

static ddsi_typeid_kind_t ddsi_typeid_kind_impl (const struct DDS_XTypes_TypeIdentifier *type_id);
static bool xt_is_non_hash (const struct xt_type *xt);
static void xt_applied_member_annotations_fini (struct xt_applied_member_annotations *ann);
static void xt_applied_type_annotations_fini (struct xt_applied_type_annotations *ann);

enum xt_validate_state {
  XT_VALIDATE_STARTED,
  XT_VALIDATE_VALIDATED
};

enum xt_validate_edge {
  XT_VALIDATE_EDGE_TOP,
  XT_VALIDATE_EDGE_ALIAS,
  XT_VALIDATE_EDGE_AGGREGATE,
  XT_VALIDATE_EDGE_SEQUENCE,
  XT_VALIDATE_EDGE_ARRAY,
  XT_VALIDATE_EDGE_MAP
};

struct xt_validate_node {
  const struct xt_type *type;
  /* Key validation is stricter, so cache validation state separately for it. */
  bool in_key;
  enum xt_validate_state state;
  /* Index in the implicit DFS path; context->path[path_index] is the prefix
     before entering this node. */
  uint32_t path_index;
};

struct xt_validate_path_entry {
  uint32_t aggregate;
  uint32_t sequence;
  uint32_t indirection;
};

struct xt_validate_context {
  struct ddsrt_hh *types;
  /* path[0] is the empty path. A node entered at depth n writes path[n + 1],
     so XT_VALIDATE_MAX_DEPTH entered nodes need indices 0..MAX_DEPTH. */
  struct xt_validate_path_entry path[XT_VALIDATE_MAX_DEPTH + 1];
  bool allow_recursive_types;
};

struct xt_assignability_node {
  const struct xt_type *rd_type;
  const struct xt_type *wr_type;
};

struct xt_assignability_context {
  struct ddsrt_hh *type_pairs;
};

struct xt_typeid_gen_node;

struct xt_typeid_gen_scc {
  /* Strongly-connected components in the TypeObject dependency graph. The
     XTypes SCC TypeIdentifier is only used for recursive components; ordinary
     single-node components still use the normal hash TypeIdentifier. */
  /* Nodes belonging to this component, later sorted into the order used for the
     SCC TypeIdentifier indices and the SCC hash input. */
  struct xt_typeid_gen_node **nodes;
  uint32_t n_nodes;
  /* Discovery order, used only as a deterministic tie-breaker while scheduling
     components for finalization. */
  uint32_t order;
  /* True for multi-node components and for single-node self-loops. */
  bool recursive;
  /* Recursion guard while topologically scheduling SCC dependencies. */
  bool scheduling;
  /* Set once the component has been appended to scc_work. */
  bool scheduled;
};

struct xt_typeid_gen_node {
  /* Hash-identified type with a TypeObject; non-hash types are expanded into
     dependencies and are not represented by nodes. */
  const struct xt_type *type;
  /* Outgoing dependencies to other hash-identified TypeObjects. */
  struct xt_typeid_gen_node **deps;
  uint32_t n_deps;
  uint32_t deps_size;
  /* Cached TypeIdentifier for this node: either a normal hash id or an SCC id. */
  struct DDS_XTypes_TypeIdentifier type_id;
  /* Component assigned by Tarjan. */
  struct xt_typeid_gen_scc *scc;
  /* Tarjan discovery index and lowlink; UINT32_MAX means not visited yet. */
  uint32_t index;
  uint32_t lowlink;
  /* Whether the node is currently on Tarjan's active stack. */
  bool on_stack;
  /* Prevents re-expanding dependencies when the same node is reached again. */
  bool deps_done;
  /* Tracks ownership/validity of type_id for cleanup and reuse. */
  bool type_id_valid;
};

struct xt_typeid_gen_context {
  /* Node lookup by xt_type pointer. */
  struct ddsrt_hh *nodes;
  /* Insertion-ordered node list used to start Tarjan from every component. */
  struct xt_typeid_gen_node **node_seq;
  uint32_t n_nodes;
  uint32_t node_seq_size;
  /* Tarjan active stack. */
  struct xt_typeid_gen_node **stack;
  uint32_t stack_len;
  uint32_t stack_size;
  /* Components in discovery order. */
  struct xt_typeid_gen_scc **sccs;
  uint32_t n_sccs;
  uint32_t sccs_size;
  /* Components in dependency-before-user finalization order. */
  struct xt_typeid_gen_scc **scc_work;
  uint32_t n_scc_work;
  uint32_t scc_work_size;
  /* Whether this run emits minimal or complete identifiers/objects. */
  ddsi_typeid_kind_t kind;
  /* Next Tarjan discovery index. */
  uint32_t next_index;
};

ddsrt_nonnull_all
void ddsi_typeid_copy_impl (struct DDS_XTypes_TypeIdentifier *dst, const struct DDS_XTypes_TypeIdentifier *src)
{
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
  return (ddsi_typeid_t *) ddsi_typeid_dup_impl (src);
}

ddsi_typeid_t * ddsi_typeid_dup (const ddsi_typeid_t *src)
{
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
    case DDS_XTypes_TK_INT8: return "INT8";
    case DDS_XTypes_TK_INT16: return "INT16";
    case DDS_XTypes_TK_INT32: return "INT32";
    case DDS_XTypes_TK_INT64: return "INT64";
    case DDS_XTypes_TK_UINT8: return "UINT8";
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

static int plain_collection_header_compare (struct DDS_XTypes_PlainCollectionHeader a, struct DDS_XTypes_PlainCollectionHeader b, bool is_assignability_check)
{
  if (a.equiv_kind != b.equiv_kind)
    return a.equiv_kind > b.equiv_kind ? 1 : -1;
  uint16_t aef = a.element_flags, bef = b.element_flags;
  // Some implementations leave the "try construct" bits both at 0 in some cases. This is an invalid value but
  // we have to at least offer the possibility of accepting them in the type validation for compatibility. The
  // setting of the flag matters in the assignability check so we can rewrite them here if we're just doing an
  // assignability check.
  //
  // In other cases, we can't do this: it also affects the hash id of the type, and we have to store the types
  // with the same flag settings or we will treat it is equivalent in type lookups and fail to actually insert
  // the (incorrect) type from a peer in our type library if we happen to have the correct one already present
  // in the library, e.g., because of a topic definition.
  if (is_assignability_check)
  {
    if ((aef & (DDS_XTypes_TRY_CONSTRUCT1 | DDS_XTypes_TRY_CONSTRUCT2)) == 0)
      aef |= DDS_XTypes_TRY_CONSTRUCT_DISCARD;
    if ((bef & (DDS_XTypes_TRY_CONSTRUCT1 | DDS_XTypes_TRY_CONSTRUCT2)) == 0)
      bef |= DDS_XTypes_TRY_CONSTRUCT_DISCARD;
  }
  if (aef != bef)
    return aef > bef ? 1 : -1;
  return 0;
}

static int equivalence_hash_compare (const DDS_XTypes_EquivalenceHash a, const DDS_XTypes_EquivalenceHash b)
{
  return memcmp (a, b, sizeof (DDS_XTypes_EquivalenceHash));
}

int ddsi_typeobject_hashid_compare_impl (const struct DDS_XTypes_TypeObjectHashId *a, const struct DDS_XTypes_TypeObjectHashId *b)
{
  if (a->_d != b->_d)
    return a->_d > b->_d ? 1 : -1;
  return equivalence_hash_compare (a->_u.hash, b->_u.hash);
}

bool ddsi_typeobject_hashid_equal_impl (const struct DDS_XTypes_TypeObjectHashId *a, const struct DDS_XTypes_TypeObjectHashId *b)
{
  return ddsi_typeobject_hashid_compare_impl (a, b) == 0;
}

static uint32_t typeid_hash_bytes (const void *data, size_t size, uint32_t seed)
{
  return ddsrt_mh3 (data, size, seed);
}

static uint32_t typeid_hash_u8 (uint8_t value, uint32_t seed)
{
  return typeid_hash_bytes (&value, sizeof (value), seed);
}

static uint32_t typeid_hash_i32 (int32_t value, uint32_t seed)
{
  return typeid_hash_bytes (&value, sizeof (value), seed);
}

static uint32_t typeid_hash_u16 (uint16_t value, uint32_t seed)
{
  return typeid_hash_bytes (&value, sizeof (value), seed);
}

static uint32_t typeid_hash_u32 (uint32_t value, uint32_t seed)
{
  return typeid_hash_bytes (&value, sizeof (value), seed);
}

static uint32_t typeobject_hashid_hash_impl (const struct DDS_XTypes_TypeObjectHashId *id, uint32_t seed)
{
  uint32_t hash = typeid_hash_u8 ((uint8_t) id->_d, seed);
  return typeid_hash_bytes (id->_u.hash, sizeof (id->_u.hash), hash);
}

bool ddsi_type_scc_id_is_valid_impl (const struct DDS_XTypes_StronglyConnectedComponentId *id)
{
  return (id->sc_component_id._d == DDS_XTypes_EK_MINIMAL || id->sc_component_id._d == DDS_XTypes_EK_COMPLETE) &&
    id->scc_length > 0 && (uint32_t) id->scc_length <= DDSI_TYPE_SCC_MAX_WIRE_TYPES &&
    id->scc_index > 0 && id->scc_index <= id->scc_length;
}

bool ddsi_type_scc_id_same_component_impl (const struct DDS_XTypes_StronglyConnectedComponentId *a, const struct DDS_XTypes_StronglyConnectedComponentId *b)
{
  return a->scc_length == b->scc_length && ddsi_typeobject_hashid_equal_impl (&a->sc_component_id, &b->sc_component_id);
}

uint32_t ddsi_type_scc_id_component_hash_impl (const struct DDS_XTypes_StronglyConnectedComponentId *id)
{
  uint32_t hash = typeobject_hashid_hash_impl (&id->sc_component_id, 0);
  return typeid_hash_i32 (id->scc_length, hash);
}

static uint32_t type_scc_id_hash_impl (const struct DDS_XTypes_StronglyConnectedComponentId *id)
{
  uint32_t hash = ddsi_type_scc_id_component_hash_impl (id);
  return typeid_hash_i32 (id->scc_index, hash);
}

static int strongly_connected_component_id_compare (struct DDS_XTypes_StronglyConnectedComponentId a, struct DDS_XTypes_StronglyConnectedComponentId b)
{
  if (a.scc_length != b.scc_length)
    return a.scc_length > b.scc_length ? 1 : -1;
  if (a.scc_index != b.scc_index)
    return a.scc_index > b.scc_index ? 1 : -1;
  return ddsi_typeobject_hashid_compare_impl (&a.sc_component_id, &b.sc_component_id);
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
  {
    bool found = false;
    for (uint32_t m = 0; !found && m < b->_length; m++)
    {
      if (type_id_with_size_equal (&a->_buffer[n], &b->_buffer[m]))
        found = true;
    }
    if (!found)
      return false;
  }
  return true;
}

bool ddsi_type_id_with_deps_equal (const struct DDS_XTypes_TypeIdentifierWithDependencies *a, const struct DDS_XTypes_TypeIdentifierWithDependencies *b, enum ddsi_type_include_deps deps)
{
  return type_id_with_size_equal (&a->typeid_with_size, &b->typeid_with_size)
    && a->dependent_typeid_count == b->dependent_typeid_count
    && (!deps || type_id_with_sizeseq_equal (&a->dependent_typeids, &b->dependent_typeids));
}

static int ddsi_typeid_compare_acflag (const struct DDS_XTypes_TypeIdentifier *a, const struct DDS_XTypes_TypeIdentifier *b, bool is_assignability_check)
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
      if ((r = plain_collection_header_compare (a->_u.seq_sdefn.header, b->_u.seq_sdefn.header, is_assignability_check)) != 0)
        return r;
      if ((r = ddsi_typeid_compare_acflag (a->_u.seq_sdefn.element_identifier, b->_u.seq_sdefn.element_identifier, is_assignability_check)) != 0)
        return r;
      if (a->_u.seq_sdefn.bound != b->_u.seq_sdefn.bound)
        return a->_u.seq_sdefn.bound > b->_u.seq_sdefn.bound ? 1 : -1;
      return 0;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      if ((r = plain_collection_header_compare (a->_u.seq_ldefn.header, b->_u.seq_ldefn.header, is_assignability_check)) != 0)
        return r;
      if ((r = ddsi_typeid_compare_acflag (a->_u.seq_ldefn.element_identifier, b->_u.seq_ldefn.element_identifier, is_assignability_check)) != 0)
        return r;
      if (a->_u.seq_ldefn.bound != b->_u.seq_ldefn.bound)
        return a->_u.seq_ldefn.bound > b->_u.seq_ldefn.bound ? 1 : -1;
      return 0;
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
      if ((r = plain_collection_header_compare (a->_u.array_sdefn.header, b->_u.array_sdefn.header, is_assignability_check)) != 0)
        return r;
      if (a->_u.array_sdefn.array_bound_seq._length != b->_u.array_sdefn.array_bound_seq._length)
        return a->_u.array_sdefn.array_bound_seq._length > b->_u.array_sdefn.array_bound_seq._length ? 1 : -1;
      if (a->_u.array_sdefn.array_bound_seq._length > 0)
        if ((r = memcmp (a->_u.array_sdefn.array_bound_seq._buffer, b->_u.array_sdefn.array_bound_seq._buffer,
                          a->_u.array_sdefn.array_bound_seq._length * sizeof (*a->_u.array_sdefn.array_bound_seq._buffer))) != 0)
          return r;
      return ddsi_typeid_compare_acflag (a->_u.array_sdefn.element_identifier, b->_u.array_sdefn.element_identifier, is_assignability_check);
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      if ((r = plain_collection_header_compare (a->_u.array_ldefn.header, b->_u.array_ldefn.header, is_assignability_check)) != 0)
        return r;
      if (a->_u.array_ldefn.array_bound_seq._length != b->_u.array_ldefn.array_bound_seq._length)
        return a->_u.array_ldefn.array_bound_seq._length > b->_u.array_ldefn.array_bound_seq._length ? 1 : -1;
      if (a->_u.array_ldefn.array_bound_seq._length > 0)
        if ((r = memcmp (a->_u.array_ldefn.array_bound_seq._buffer, b->_u.array_ldefn.array_bound_seq._buffer,
                          a->_u.array_ldefn.array_bound_seq._length * sizeof (*a->_u.array_ldefn.array_bound_seq._buffer))) != 0)
          return r;
      return ddsi_typeid_compare_acflag (a->_u.array_ldefn.element_identifier, b->_u.array_ldefn.element_identifier, is_assignability_check);
    case DDS_XTypes_TI_PLAIN_MAP_SMALL:
      if ((r = plain_collection_header_compare (a->_u.map_sdefn.header, b->_u.map_sdefn.header, is_assignability_check)) != 0)
        return r;
      if (a->_u.map_sdefn.bound != b->_u.map_sdefn.bound)
        return a->_u.map_sdefn.bound > b->_u.map_sdefn.bound ? 1 : -1;
      if ((r = ddsi_typeid_compare_acflag (a->_u.map_sdefn.element_identifier, b->_u.map_sdefn.element_identifier, is_assignability_check)) != 0)
        return r;
      if (a->_u.map_sdefn.key_flags != b->_u.map_sdefn.key_flags)
        return a->_u.map_sdefn.key_flags > b->_u.map_sdefn.key_flags ? 1 : -1;
      return ddsi_typeid_compare_acflag (a->_u.map_sdefn.key_identifier, b->_u.map_sdefn.key_identifier, is_assignability_check);
    case DDS_XTypes_TI_PLAIN_MAP_LARGE:
      if ((r = plain_collection_header_compare (a->_u.map_ldefn.header, b->_u.map_ldefn.header, is_assignability_check)) != 0)
        return r;
      if (a->_u.map_ldefn.bound != b->_u.map_ldefn.bound)
        return a->_u.map_ldefn.bound > b->_u.map_ldefn.bound ? 1 : -1;
      if ((r = ddsi_typeid_compare_acflag (a->_u.map_ldefn.element_identifier, b->_u.map_ldefn.element_identifier, is_assignability_check)) != 0)
        return r;
      if (a->_u.map_ldefn.key_flags != b->_u.map_ldefn.key_flags)
        return a->_u.map_ldefn.key_flags > b->_u.map_ldefn.key_flags ? 1 : -1;
      return ddsi_typeid_compare_acflag (a->_u.map_ldefn.key_identifier, b->_u.map_ldefn.key_identifier, is_assignability_check);
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

int ddsi_typeid_compare_impl (const struct DDS_XTypes_TypeIdentifier *a, const struct DDS_XTypes_TypeIdentifier *b)
{
  return ddsi_typeid_compare_acflag (a, b, false);
}

int ddsi_typeid_compare (const ddsi_typeid_t *a, const ddsi_typeid_t *b)
{
  return ddsi_typeid_compare_acflag (&a->x, &b->x, false);
}

int ddsi_typeid_compare_assignability_check (const ddsi_typeid_t *a, const ddsi_typeid_t *b)
{
  return ddsi_typeid_compare_acflag (&a->x, &b->x, true);
}

static uint32_t typeid_hash_plain_collection_header (const struct DDS_XTypes_PlainCollectionHeader *header, uint32_t seed)
{
  uint32_t hash = typeid_hash_u8 (header->equiv_kind, seed);
  return typeid_hash_u16 (header->element_flags, hash);
}

static uint32_t typeid_hash_sbound_seq (const DDS_XTypes_SBoundSeq *seq, uint32_t seed)
{
  uint32_t hash = typeid_hash_u32 (seq->_length, seed);
  if (seq->_length > 0)
    hash = typeid_hash_bytes (seq->_buffer, seq->_length * sizeof (*seq->_buffer), hash);
  return hash;
}

static uint32_t typeid_hash_lbound_seq (const DDS_XTypes_LBoundSeq *seq, uint32_t seed)
{
  uint32_t hash = typeid_hash_u32 (seq->_length, seed);
  if (seq->_length > 0)
    hash = typeid_hash_bytes (seq->_buffer, seq->_length * sizeof (*seq->_buffer), hash);
  return hash;
}

static uint32_t typeid_hash_type_identifier (const struct DDS_XTypes_TypeIdentifier *type_id, uint32_t seed)
{
  return typeid_hash_u32 (ddsi_typeid_hash_impl (type_id), seed);
}

uint32_t ddsi_typeid_hash_impl (const struct DDS_XTypes_TypeIdentifier *type_id)
{
  if (type_id == NULL)
    return 0;

  uint32_t hash = typeid_hash_u8 (type_id->_d, 0);
  if (type_id->_d <= DDS_XTypes_TK_STRING16)
    return hash;

  switch (type_id->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING16_SMALL:
      return typeid_hash_u8 (type_id->_u.string_sdefn.bound, hash);
    case DDS_XTypes_TI_STRING8_LARGE:
    case DDS_XTypes_TI_STRING16_LARGE:
      return typeid_hash_u32 (type_id->_u.string_ldefn.bound, hash);
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      hash = typeid_hash_plain_collection_header (&type_id->_u.seq_sdefn.header, hash);
      hash = typeid_hash_type_identifier (type_id->_u.seq_sdefn.element_identifier, hash);
      return typeid_hash_u8 (type_id->_u.seq_sdefn.bound, hash);
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      hash = typeid_hash_plain_collection_header (&type_id->_u.seq_ldefn.header, hash);
      hash = typeid_hash_type_identifier (type_id->_u.seq_ldefn.element_identifier, hash);
      return typeid_hash_u32 (type_id->_u.seq_ldefn.bound, hash);
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
      hash = typeid_hash_plain_collection_header (&type_id->_u.array_sdefn.header, hash);
      hash = typeid_hash_sbound_seq (&type_id->_u.array_sdefn.array_bound_seq, hash);
      return typeid_hash_type_identifier (type_id->_u.array_sdefn.element_identifier, hash);
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      hash = typeid_hash_plain_collection_header (&type_id->_u.array_ldefn.header, hash);
      hash = typeid_hash_lbound_seq (&type_id->_u.array_ldefn.array_bound_seq, hash);
      return typeid_hash_type_identifier (type_id->_u.array_ldefn.element_identifier, hash);
    case DDS_XTypes_TI_PLAIN_MAP_SMALL:
      hash = typeid_hash_plain_collection_header (&type_id->_u.map_sdefn.header, hash);
      hash = typeid_hash_u8 (type_id->_u.map_sdefn.bound, hash);
      hash = typeid_hash_type_identifier (type_id->_u.map_sdefn.element_identifier, hash);
      hash = typeid_hash_u16 (type_id->_u.map_sdefn.key_flags, hash);
      return typeid_hash_type_identifier (type_id->_u.map_sdefn.key_identifier, hash);
    case DDS_XTypes_TI_PLAIN_MAP_LARGE:
      hash = typeid_hash_plain_collection_header (&type_id->_u.map_ldefn.header, hash);
      hash = typeid_hash_u32 (type_id->_u.map_ldefn.bound, hash);
      hash = typeid_hash_type_identifier (type_id->_u.map_ldefn.element_identifier, hash);
      hash = typeid_hash_u16 (type_id->_u.map_ldefn.key_flags, hash);
      return typeid_hash_type_identifier (type_id->_u.map_ldefn.key_identifier, hash);
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT:
      return typeid_hash_u32 (type_scc_id_hash_impl (&type_id->_u.sc_component_id), hash);
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_EK_MINIMAL:
      return typeid_hash_bytes (type_id->_u.equivalence_hash, sizeof (type_id->_u.equivalence_hash), hash);
    default:
      assert (false);
      return hash;
  }
}

uint32_t ddsi_typeid_hash (const ddsi_typeid_t *type_id)
{
  return type_id == NULL ? 0 : ddsi_typeid_hash_impl (&type_id->x);
}

void ddsi_typeid_ser (const ddsi_typeid_t *type_id, unsigned char **buf, uint32_t *sz)
{
  dds_ostreamLE_t os = { .x = { .m_buffer = NULL, .m_index = 0, .m_size = 0, .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 } };
  if (!dds_stream_writeLE (&os, &dds_cdrstream_default_allocator, (const void *) type_id, DDS_XTypes_TypeIdentifier_desc.m_ops))
  {
    // input is always valid
    abort ();
  }
  *buf = os.x.m_buffer;
  *sz = os.x.m_index;
}

void ddsi_typeid_fini_impl (struct DDS_XTypes_TypeIdentifier *type_id)
{
  dds_stream_free_sample (type_id, &dds_cdrstream_default_allocator, DDS_XTypes_TypeIdentifier_desc.m_ops);
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
  return ddsi_typeid_is_minimal_impl (type_id) || ddsi_typeid_is_complete_impl (type_id) ||
    (type_id != NULL && type_id->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT &&
     (type_id->_u.sc_component_id.sc_component_id._d == DDS_XTypes_EK_MINIMAL ||
      type_id->_u.sc_component_id.sc_component_id._d == DDS_XTypes_EK_COMPLETE));
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

bool ddsi_typeid_is_fully_descriptive (const ddsi_typeid_t *type_id)
{
  return type_id != NULL && ddsi_typeid_kind_impl (&type_id->x) == DDSI_TYPEID_KIND_FULLY_DESCRIPTIVE;
}

ddsrt_nonnull_all
void ddsi_typeid_get_equivalence_hash (const ddsi_typeid_t *type_id, DDS_XTypes_EquivalenceHash *hash)
{
  assert (ddsi_typeid_is_hash (type_id));
  if (type_id->x._d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT)
    memcpy (hash, type_id->x._u.sc_component_id.sc_component_id._u.hash, sizeof (*hash));
  else
    memcpy (hash, type_id->x._u.equivalence_hash, sizeof (*hash));
}

ddsrt_nonnull_all
static dds_return_t ddsi_typeobj_get_hash_id_impl1 (const struct DDS_XTypes_TypeObject *type_obj, struct DDS_XTypes_TypeIdentifier *type_id)
{
  if (type_obj->_d != DDS_XTypes_EK_MINIMAL && type_obj->_d != DDS_XTypes_EK_COMPLETE)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_ostreamLE_t os = { .x = { .m_buffer = NULL, .m_index = 0, .m_size = 0, .m_xcdr_version = DDSI_RTPS_CDR_ENC_VERSION_2 } };
  if (!dds_stream_writeLE (&os, &dds_cdrstream_default_allocator, (const void *) type_obj, DDS_XTypes_TypeObject_desc.m_ops))
  {
    dds_ostreamLE_fini (&os, &dds_cdrstream_default_allocator);
    return DDS_RETCODE_BAD_PARAMETER;
  }

  char buf[16];
  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) os.x.m_buffer, os.x.m_index);
  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf);
  type_id->_d = type_obj->_d;
  memcpy (type_id->_u.equivalence_hash, buf, sizeof(DDS_XTypes_EquivalenceHash));
  dds_ostreamLE_fini (&os, &dds_cdrstream_default_allocator);
  return DDS_RETCODE_OK;
}

ddsrt_nonnull_all
void ddsi_typeobj_get_hash_id_impl (const struct DDS_XTypes_TypeObject *type_obj, struct DDS_XTypes_TypeIdentifier *type_id)
{
  if (ddsi_typeobj_get_hash_id_impl1 (type_obj, type_id) != DDS_RETCODE_OK)
    /* This helper is used for TypeObjects generated from validated xt_type
       definitions.  Failure means the generator produced an internally
       inconsistent TypeObject, whereas externally supplied TypeObjects go
       through ddsi_typeobj_get_hash_id and get a return code instead. */
    abort ();
}

ddsrt_nonnull_all
dds_return_t ddsi_typeobj_get_hash_id (const struct DDS_XTypes_TypeObject *type_obj, ddsi_typeid_t *type_id)
{
  return ddsi_typeobj_get_hash_id_impl1 (type_obj, &type_id->x);
}

ddsrt_nonnull_all
const char *ddsi_typeobj_get_type_name_impl (const struct DDS_XTypes_TypeObject *type_obj)
{
  if (type_obj->_d != DDS_XTypes_EK_COMPLETE)
    return NULL;

  switch (type_obj->_u.complete._d)
  {
    case DDS_XTypes_TK_ALIAS:
      return type_obj->_u.complete._u.alias_type.header.detail.type_name;
    case DDS_XTypes_TK_STRUCTURE:
      return type_obj->_u.complete._u.struct_type.header.detail.type_name;
    case DDS_XTypes_TK_UNION:
      return type_obj->_u.complete._u.union_type.header.detail.type_name;
    case DDS_XTypes_TK_BITSET:
      return type_obj->_u.complete._u.bitset_type.header.detail.type_name;
    case DDS_XTypes_TK_ENUM:
      return type_obj->_u.complete._u.enumerated_type.header.detail.type_name;
    case DDS_XTypes_TK_BITMASK:
      return type_obj->_u.complete._u.bitmask_type.header.detail.type_name;
    default:
      return NULL;
  }
}

void ddsi_typeobj_fini_impl (struct DDS_XTypes_TypeObject *typeobj)
{
  dds_stream_free_sample (typeobj, &dds_cdrstream_default_allocator, DDS_XTypes_TypeObject_desc.m_ops);
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
  lb->_maximum = sb->_length;
  lb->_release = true;
  lb->_buffer = ddsrt_malloc (sb->_length * sizeof (*lb->_buffer));
  for (uint32_t n = 0; n < sb->_length; n++)
    lb->_buffer[n] = (DDS_XTypes_LBound) sb->_buffer[n];
}

static void xt_lbounds_to_sbounds (struct DDS_XTypes_SBoundSeq *sb, const struct DDS_XTypes_LBoundSeq *lb)
{
  sb->_length = lb->_length;
  sb->_maximum = lb->_length;
  sb->_release = true;
  sb->_buffer = ddsrt_malloc (lb->_length * sizeof (*sb->_buffer));
  for (uint32_t n = 0; n < lb->_length; n++)
  {
    assert (lb->_buffer[n] <= 255);
    sb->_buffer[n] = (DDS_XTypes_SBound) lb->_buffer[n];
  }
}

static void xt_lbounds_dup (struct DDS_XTypes_LBoundSeq *dst, const struct DDS_XTypes_LBoundSeq *src)
{
  dst->_length = src->_length;
  dst->_maximum = src->_length;
  dst->_release = true;
  dst->_buffer = ddsrt_memdup (src->_buffer, dst->_length * sizeof (*dst->_buffer));
}

void ddsi_xt_get_namehash (DDS_XTypes_NameHash name_hash, const char *name)
{
  /* FIXME: multi byte utf8 chars? */
  char buf[16];
  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) name, (uint32_t) strlen (name));
  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf);
  memcpy (name_hash, buf, sizeof (DDS_XTypes_NameHash));
}

static void DDS_XTypes_AppliedBuiltinTypeAnnotations_copy (struct DDS_XTypes_AppliedBuiltinTypeAnnotations *dst, const struct DDS_XTypes_AppliedBuiltinTypeAnnotations *src);
static void DDS_XTypes_AppliedBuiltinMemberAnnotations_copy (struct DDS_XTypes_AppliedBuiltinMemberAnnotations *dst, const struct DDS_XTypes_AppliedBuiltinMemberAnnotations *src);
static void DDS_XTypes_AppliedAnnotationSeq_copy (struct DDS_XTypes_AppliedAnnotationSeq *dst, const struct DDS_XTypes_AppliedAnnotationSeq *src);
static void set_type_annotations (struct xt_applied_type_annotations *dst, const struct DDS_XTypes_AppliedBuiltinTypeAnnotations *ann_builtin, const DDS_XTypes_AppliedAnnotationSeq *ann_custom)
{
  if (ann_builtin)
  {
    dst->ann_builtin = ddsrt_calloc (1, sizeof (*dst->ann_builtin));
    DDS_XTypes_AppliedBuiltinTypeAnnotations_copy (dst->ann_builtin, ann_builtin);
  }
  else
  {
    dst->ann_builtin = NULL;
  }

  if (ann_custom)
  {
    dst->ann_custom = ddsrt_calloc (1, sizeof (*dst->ann_custom));
    DDS_XTypes_AppliedAnnotationSeq_copy (dst->ann_custom, ann_custom);
  }
  else
  {
    dst->ann_custom = NULL;
  }
}

static void set_type_detail (struct xt_type_detail *dst, const DDS_XTypes_CompleteTypeDetail *src)
{
  ddsrt_strlcpy (dst->type_name, src->type_name, sizeof (dst->type_name));
  set_type_annotations (&dst->annotations, src->ann_builtin, src->ann_custom);
}

static void set_member_annotations (struct xt_applied_member_annotations *dst, const struct DDS_XTypes_AppliedBuiltinMemberAnnotations *ann_builtin, const DDS_XTypes_AppliedAnnotationSeq *ann_custom)
{
  if (ann_builtin)
  {
    dst->ann_builtin = ddsrt_calloc (1, sizeof (*dst->ann_builtin));
    DDS_XTypes_AppliedBuiltinMemberAnnotations_copy (dst->ann_builtin, ann_builtin);
  }
  else
  {
    dst->ann_builtin = NULL;
  }

  if (ann_custom)
  {
    dst->ann_custom = ddsrt_calloc (1, sizeof (*dst->ann_custom));
    DDS_XTypes_AppliedAnnotationSeq_copy (dst->ann_custom, ann_custom);
  }
  else
  {
    dst->ann_custom = NULL;
  }
}

static void set_member_detail (struct xt_member_detail *dst, const DDS_XTypes_CompleteMemberDetail *src)
{
  ddsrt_strlcpy (dst->name, src->name, sizeof (dst->name));
  ddsi_xt_get_namehash (dst->name_hash, dst->name);
  set_member_annotations (&dst->annotations, src->ann_builtin, src->ann_custom);
}

bool ddsi_xt_missing_definition (const struct xt_type *t)
{
  /* Types with kind FULLY_DESCRIPTIVE carry their definition in the type identifier.
     Plain collections are also considered to have a definition here: dependency
     definitions are tracked separately from the collection itself. */
  return (t->kind == DDSI_TYPEID_KIND_MINIMAL || t->kind == DDSI_TYPEID_KIND_COMPLETE) && t->_d == DDS_XTypes_TK_NONE;
}

bool ddsi_xt_has_definition (const struct xt_type *t)
{
  return !ddsi_xt_missing_definition (t);
}

static const struct xt_type *ddsi_xt_unalias (const struct xt_type *t)
{
  return t->_d == DDS_XTypes_TK_ALIAS ? ddsi_xt_unalias (&t->_u.alias.related_type->xt) : t;
}

static dds_return_t xt_resolve_valid_struct_base_type (struct ddsi_domaingv *gv, const struct xt_type *t, const struct xt_type **base_type)
{
  assert (t->_d == DDS_XTypes_TK_STRUCTURE);
  *base_type = NULL;
  if (!t->_u.structure.base_type)
    return DDS_RETCODE_OK;

  /* Only a base type that has a definition, i.e. has a type object or is fully
     descriptive, can be used to check the type. */
  const struct xt_type *bt = &t->_u.structure.base_type->xt;
  if (ddsi_xt_missing_definition (bt))
    return DDS_RETCODE_OK;
  bt = ddsi_xt_unalias (bt);
  if (bt->_d != DDS_XTypes_TK_STRUCTURE)
  {
    GVTRACE ("base type for struct is not a struct type\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  *base_type = bt;
  return DDS_RETCODE_OK;
}

static dds_return_t xt_valid_struct_base_type (struct ddsi_domaingv *gv, const struct xt_type *t)
{
  const struct xt_type *base_type;
  return xt_resolve_valid_struct_base_type (gv, t, &base_type);
}

static dds_return_t xt_valid_union_disc_type (struct ddsi_domaingv *gv, const struct xt_type *t)
{
  if (ddsi_xt_missing_definition (&t->_u.union_type.disc_type->xt))
    return DDS_RETCODE_OK;
  uint8_t d = ddsi_xt_unalias (&t->_u.union_type.disc_type->xt)->_d;
  if (d != DDS_XTypes_TK_BOOLEAN
      && d != DDS_XTypes_TK_BYTE && d != DDS_XTypes_TK_CHAR8 && d != DDS_XTypes_TK_CHAR16
      && d != DDS_XTypes_TK_INT8 && d != DDS_XTypes_TK_INT16 && d != DDS_XTypes_TK_INT32 && d != DDS_XTypes_TK_INT64
      && d != DDS_XTypes_TK_UINT8 && d != DDS_XTypes_TK_UINT16 && d != DDS_XTypes_TK_UINT32 && d != DDS_XTypes_TK_UINT64
      && d != DDS_XTypes_TK_ENUM && d != DDS_XTypes_TK_BITMASK)
  {
    GVTRACE ("discriminator type for union is invalid\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  return DDS_RETCODE_OK;
}

static int xt_member_id_cmp (const void *va, const void *vb)
{
  const DDS_XTypes_MemberId *m1 = va, *m2 = vb;
  return (*m1 == *m2) ? 0 : (*m1 < *m2) ? -1 : 1;
}

static int xt_union_label_cmp (const void *va, const void *vb)
{
  const int32_t *l1 = va, *l2 = vb;
  return (*l1 == *l2) ? 0 : (*l1 < *l2) ? -1 : 1;
}

struct xt_union_label_range {
  int64_t min;
  uint64_t max;
};

static dds_return_t xt_union_label_range (const struct xt_type *disc_type, struct xt_union_label_range *range)
{
  if (ddsi_xt_missing_definition (disc_type))
  {
    range->min = INT64_MIN;
    range->max = UINT64_MAX;
    return DDS_RETCODE_OK;
  }

  const struct xt_type *dt = ddsi_xt_unalias (disc_type);
  switch (dt->_d)
  {
    case DDS_XTypes_TK_BOOLEAN:
      range->min = 0; range->max = 1;
      break;
    case DDS_XTypes_TK_BYTE:
    case DDS_XTypes_TK_CHAR8:
    case DDS_XTypes_TK_UINT8:
      range->min = 0; range->max = UINT8_MAX;
      break;
    case DDS_XTypes_TK_CHAR16:
    case DDS_XTypes_TK_UINT16:
      range->min = 0; range->max = UINT16_MAX;
      break;
    case DDS_XTypes_TK_INT8:
      range->min = INT8_MIN; range->max = INT8_MAX;
      break;
    case DDS_XTypes_TK_INT16:
      range->min = INT16_MIN; range->max = INT16_MAX;
      break;
    case DDS_XTypes_TK_INT32:
      range->min = INT32_MIN; range->max = INT32_MAX;
      break;
    case DDS_XTypes_TK_INT64:
      range->min = INT64_MIN; range->max = INT64_MAX;
      break;
    case DDS_XTypes_TK_UINT32:
      range->min = 0; range->max = UINT32_MAX;
      break;
    case DDS_XTypes_TK_UINT64:
      range->min = 0; range->max = UINT64_MAX;
      break;
    case DDS_XTypes_TK_ENUM: {
      // Enum values are signed 32-bit integers in type objects, so the maximum value is INT32_MAX
      const DDS_XTypes_BitBound bit_bound = dt->_u.enum_type.bit_bound;
      range->min = 0; range->max = (bit_bound >= 31) ? INT32_MAX : ((1u << bit_bound) - 1);
      break;
    }
    case DDS_XTypes_TK_BITMASK: {
      const DDS_XTypes_BitBound bit_bound = dt->_u.bitmask.bit_bound;
      range->min = INT64_MIN; range->max = (bit_bound >= 64) ? UINT64_MAX : (((uint64_t)1 << bit_bound) - 1);
      break;
    }
    default:
      return DDS_RETCODE_UNSUPPORTED;
  }
  return DDS_RETCODE_OK;
}

static bool xt_union_label_in_enum (const struct xt_type *disc_type, int32_t label)
{
  assert (ddsi_xt_has_definition (disc_type) && disc_type->_d == DDS_XTypes_TK_ENUM);
  for (uint32_t n = 0; n < disc_type->_u.enum_type.literals.length; n++)
    if (label == disc_type->_u.enum_type.literals.seq[n].value)
      return true;
  return false;
}

static uint64_t xt_union_label_bitmask_allowed_mask (const struct xt_type *disc_type)
{
  assert (ddsi_xt_has_definition (disc_type) && disc_type->_d == DDS_XTypes_TK_BITMASK);
  uint64_t mask = 0;
  for (uint32_t n = 0; n < disc_type->_u.bitmask.bitflags.length; n++)
    mask |= (uint64_t)1 << disc_type->_u.bitmask.bitflags.seq[n].position;
  return mask;
}

static dds_return_t xt_valid_struct_member_ids (struct ddsi_domaingv *gv, const struct xt_type *t)
{
  assert (ddsi_xt_has_definition (t) && t->_d == DDS_XTypes_TK_STRUCTURE);
  dds_return_t ret = DDS_RETCODE_OK;

  uint32_t cnt = 0;
  for (const struct xt_type *t1 = t; t1; )
  {
    cnt += t1->_u.structure.members.length;
    if ((ret = xt_resolve_valid_struct_base_type (gv, t1, &t1)) != DDS_RETCODE_OK)
      return ret;
  }
  if (cnt == 0 && !t->_u.structure.base_type)
  {
    ret = DDS_RETCODE_OK;
    goto empty;
  }

  DDS_XTypes_MemberId *ids = ddsrt_malloc (cnt * sizeof (*ids));
  if (ids == NULL)
  {
    GVTRACE ("out-of-memory while checking struct member ids\n");
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto failed;
  }

  uint32_t cnt1 = cnt;
  for (const struct xt_type *t1 = t; t1; )
  {
    for (uint32_t n = 0; n < t1->_u.structure.members.length; n++)
      ids[--cnt1] = t1->_u.structure.members.seq[n].id;
    if ((ret = xt_resolve_valid_struct_base_type (gv, t1, &t1)) != DDS_RETCODE_OK)
      goto failed_duplicate;
  }
  qsort (ids, cnt, sizeof (*ids), xt_member_id_cmp);
  for (uint32_t n = 1; n < cnt; n++)
  {
    if (ids[n] == ids[n - 1])
    {
      GVTRACE ("duplicate member id %"PRIu32" in struct\n", ids[n]);
      ret = DDS_RETCODE_BAD_PARAMETER;
      goto failed_duplicate;
    }
  }

failed_duplicate:
  ddsrt_free (ids);
failed:
empty:
  return ret;
}

static dds_return_t xt_valid_union_member_ids (struct ddsi_domaingv *gv, const struct xt_type *t)
{
  assert (ddsi_xt_has_definition (t) && t->_d == DDS_XTypes_TK_UNION);
  dds_return_t ret = DDS_RETCODE_OK;

  uint32_t cnt = t->_u.union_type.members.length;
  if (cnt == 0)
  {
    GVTRACE ("union has no members\n");
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto failed;
  }

  DDS_XTypes_MemberId *ids = ddsrt_malloc (cnt * sizeof (*ids));
  if (ids == NULL)
  {
    GVTRACE ("out-of-memory while checking union member ids\n");
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto failed;
  }

  for (uint32_t n = 0; n < cnt; n++)
    ids[n] = t->_u.union_type.members.seq[n].id;
  qsort (ids, cnt, sizeof (*ids), xt_member_id_cmp);
  for (uint32_t n = 0; n < cnt - 1; n++)
  {
    if (ids[n] == ids[n + 1])
    {
      GVTRACE ("duplicate member id %"PRIu32" in union\n", ids[n]);
      ret = DDS_RETCODE_BAD_PARAMETER;
      goto failed_duplicate;
    }
  }

failed_duplicate:
  ddsrt_free (ids);
failed:
  return ret;
}

static dds_return_t xt_valid_union_case_labels (struct ddsi_domaingv *gv, const struct xt_type *t)
{
  assert (ddsi_xt_has_definition (t) && t->_d == DDS_XTypes_TK_UNION);
  dds_return_t ret = DDS_RETCODE_OK;

  uint32_t cnt = 0;
  for (uint32_t n = 0; n < t->_u.union_type.members.length; n++)
    cnt += t->_u.union_type.members.seq[n].label_seq._length;
  if (cnt == 0)
    goto empty;

  int32_t *labels = ddsrt_malloc (cnt * sizeof (*labels));
  if (labels == NULL)
  {
    GVTRACE ("out-of-memory while checking union case labels\n");
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto failed;
  }

  const struct xt_type *disc_type = &t->_u.union_type.disc_type->xt;
  const struct xt_type *disc_type_def = ddsi_xt_missing_definition (disc_type) ? NULL : ddsi_xt_unalias (disc_type);
  struct xt_union_label_range range = { 0, 0 };
  if ((ret = xt_union_label_range (disc_type, &range)) != DDS_RETCODE_OK)
    goto failed_labels;
  uint64_t bitmask_allowed_mask = 0;
  if (disc_type_def && disc_type_def->_d == DDS_XTypes_TK_BITMASK)
    bitmask_allowed_mask = xt_union_label_bitmask_allowed_mask (disc_type_def);

  uint32_t cnt1 = 0;
  for (uint32_t n = 0; n < t->_u.union_type.members.length; n++)
  {
    const DDS_XTypes_UnionCaseLabelSeq *labels1 = &t->_u.union_type.members.seq[n].label_seq;
    for (uint32_t m = 0; m < labels1->_length; m++)
    {
      const int32_t label = labels1->_buffer[m];
      if (label < range.min || (label >= 0 && (uint64_t) label > range.max))
      {
        GVTRACE ("union case label %"PRId32" outside discriminator range\n", label);
        ret = DDS_RETCODE_BAD_PARAMETER;
        goto failed_labels;
      }
      if (disc_type_def && disc_type_def->_d == DDS_XTypes_TK_ENUM && !xt_union_label_in_enum (disc_type_def, label))
      {
        GVTRACE ("union case label %"PRId32" not present in enum discriminator\n", label);
        ret = DDS_RETCODE_BAD_PARAMETER;
        goto failed_labels;
      }
      if (disc_type_def && disc_type_def->_d == DDS_XTypes_TK_BITMASK && (((uint64_t) (uint32_t) label) & ~bitmask_allowed_mask) != 0)
      {
        GVTRACE ("union case label %"PRId32" sets bits not present in bitmask discriminator\n", label);
        ret = DDS_RETCODE_BAD_PARAMETER;
        goto failed_labels;
      }
      labels[cnt1++] = labels1->_buffer[m];
    }
  }
  qsort (labels, cnt, sizeof (*labels), xt_union_label_cmp);
  for (uint32_t n = 1; n < cnt; n++)
  {
    if (labels[n] == labels[n - 1])
    {
      GVTRACE ("duplicate union case label %"PRId32"\n", labels[n]);
      ret = DDS_RETCODE_BAD_PARAMETER;
      goto failed_duplicate;
    }
  }

failed_duplicate:
failed_labels:
  ddsrt_free (labels);
failed:
empty:
  return ret;
}

static int xt_enum_value_cmp (const void *va, const void *vb)
{
  const int32_t *m1 = va, *m2 = vb;
  return (*m1 == *m2) ? 0 : (*m1 < *m2) ? -1 : 1;
}

static int xt_namehash_cmp (const void *va, const void *vb)
{
  return memcmp (va, vb, sizeof (DDS_XTypes_NameHash));
}

static dds_return_t xt_valid_enum_values (struct ddsi_domaingv *gv, const struct xt_type *t)
{
  assert (ddsi_xt_has_definition (t) && t->_d == DDS_XTypes_TK_ENUM);
  dds_return_t ret = DDS_RETCODE_OK;
  const int32_t max = (t->_u.enum_type.bit_bound >= 31) ? INT32_MAX : (int32_t) ((1u << t->_u.enum_type.bit_bound) - 1);

  uint32_t cnt = t->_u.enum_type.literals.length;
  if (cnt == 0)
  {
    GVTRACE ("enum has no members\n");
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto failed;
  }

  int32_t *values = ddsrt_malloc (cnt * sizeof (*values));
  if (values == NULL)
  {
    GVTRACE ("out-of-memory while checking enum values\n");
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto failed;
  }
  DDS_XTypes_NameHash *name_hashes = ddsrt_malloc (cnt * sizeof (*name_hashes));
  if (name_hashes == NULL)
  {
    GVTRACE ("out-of-memory while checking enum literal names\n");
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto failed_values;
  }

  for (uint32_t n = 0; n < cnt; n++)
  {
    const int32_t value = t->_u.enum_type.literals.seq[n].value;
    if (value < 0 || value > max)
    {
      GVTRACE ("enum value %"PRId32" cannot be represented by bit_bound %"PRIu16"\n", value, t->_u.enum_type.bit_bound);
      ret = DDS_RETCODE_BAD_PARAMETER;
      goto failed_duplicate;
    }
    values[n] = t->_u.enum_type.literals.seq[n].value;
    memcpy (name_hashes[n], t->_u.enum_type.literals.seq[n].detail.name_hash, sizeof (name_hashes[n]));
  }
  qsort (values, cnt, sizeof (*values), xt_enum_value_cmp);
  qsort (name_hashes, cnt, sizeof (*name_hashes), xt_namehash_cmp);
  for (uint32_t n = 0; n < cnt - 1; n++)
  {
    if (values[n] == values[n + 1])
    {
      GVTRACE ("duplicate enum value %"PRIi32"\n", values[n]);
      ret = DDS_RETCODE_BAD_PARAMETER;
      goto failed_duplicate;
    }
    if (memcmp (name_hashes[n], name_hashes[n + 1], sizeof (name_hashes[n])) == 0)
    {
      GVTRACE ("duplicate enum literal name hash\n");
      ret = DDS_RETCODE_BAD_PARAMETER;
      goto failed_duplicate;
    }
  }

failed_duplicate:
  ddsrt_free (name_hashes);
failed_values:
  ddsrt_free (values);
failed:
  return ret;
}

static int xt_bitmask_position_cmp (const void *va, const void *vb)
{
  const uint16_t *m1 = va, *m2 = vb;
  return (*m1 == *m2) ? 0 : (*m1 < *m2) ? -1 : 1;
}

static dds_return_t xt_valid_bitmask_positions (struct ddsi_domaingv *gv, const struct xt_type *t)
{
  assert (ddsi_xt_has_definition (t) && t->_d == DDS_XTypes_TK_BITMASK);
  dds_return_t ret = DDS_RETCODE_OK;
  uint32_t cnt = t->_u.bitmask.bitflags.length;
  if (cnt == 0)
  {
    GVTRACE ("bitmask has no bitflags\n");
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto failed;
  }

  uint16_t *positions = ddsrt_malloc (cnt * sizeof (*positions));
  if (positions == NULL)
  {
    GVTRACE ("out-of-memory while checking bitmask positions\n");
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto failed;
  }
  DDS_XTypes_NameHash *name_hashes = ddsrt_malloc (cnt * sizeof (*name_hashes));
  if (name_hashes == NULL)
  {
    GVTRACE ("out-of-memory while checking bitmask flag names\n");
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto failed_positions;
  }

  for (uint32_t n = 0; n < cnt; n++)
  {
    if (t->_u.bitmask.bitflags.seq[n].position >= t->_u.bitmask.bit_bound)
    {
      GVTRACE ("bitmask position %"PRIu16" cannot be represented by bit_bound %"PRIu16"\n",
          t->_u.bitmask.bitflags.seq[n].position, t->_u.bitmask.bit_bound);
      ret = DDS_RETCODE_BAD_PARAMETER;
      goto failed_duplicate;
    }
    positions[n] = t->_u.bitmask.bitflags.seq[n].position;
    memcpy (name_hashes[n], t->_u.bitmask.bitflags.seq[n].detail.name_hash, sizeof (name_hashes[n]));
  }
  qsort (positions, cnt, sizeof (*positions), xt_bitmask_position_cmp);
  qsort (name_hashes, cnt, sizeof (*name_hashes), xt_namehash_cmp);
  for (uint32_t n = 0; n < cnt - 1; n++)
  {
    if (positions[n] == positions[n + 1])
    {
      GVTRACE ("duplicate bitmask position %"PRIu16"\n", positions[n]);
      ret = DDS_RETCODE_BAD_PARAMETER;
      goto failed_duplicate;
    }
    if (memcmp (name_hashes[n], name_hashes[n + 1], sizeof (name_hashes[n])) == 0)
    {
      GVTRACE ("duplicate bitmask flag name hash\n");
      ret = DDS_RETCODE_BAD_PARAMETER;
      goto failed_duplicate;
    }
  }

failed_duplicate:
  ddsrt_free (name_hashes);
failed_positions:
  ddsrt_free (positions);
failed:
  return ret;
}

#define F DDS_XTypes_IS_FINAL
#define A DDS_XTypes_IS_APPENDABLE
#define M DDS_XTypes_IS_MUTABLE
#define N DDS_XTypes_IS_NESTED
#define H DDS_XTypes_IS_AUTOID_HASH
static dds_return_t xt_valid_type_flags (struct ddsi_domaingv *gv, uint16_t flags, uint8_t type_kind)
{
  dds_return_t ret = DDS_RETCODE_OK;
  switch (type_kind)
  {
    case DDS_XTypes_TK_STRUCTURE:
    case DDS_XTypes_TK_UNION:
      if (flags & ~(F|A|M|N|H))
        ret = DDS_RETCODE_BAD_PARAMETER;
      if (!(flags & (F|A|M)))
        ret = DDS_RETCODE_BAD_PARAMETER;
      break;
    case DDS_XTypes_TK_ALIAS:
    case DDS_XTypes_TK_BITSET:
    case DDS_XTypes_TK_SEQUENCE:
    case DDS_XTypes_TK_ARRAY:
    case DDS_XTypes_TK_MAP:
      if (flags)
        ret = DDS_RETCODE_BAD_PARAMETER;
      break;
    case DDS_XTypes_TK_ENUM:
    case DDS_XTypes_TK_BITMASK:
      // spec says unused, but this flag is actually used for extensibility
      if (flags & ~(F|A))
        ret = DDS_RETCODE_BAD_PARAMETER;
      if (!(flags & (F|A)))
        ret = DDS_RETCODE_BAD_PARAMETER;
      break;
    case DDS_XTypes_TK_ANNOTATION:
      // not supported yet, no validation
      break;
    default:
      ret = DDS_RETCODE_UNSUPPORTED;
      break;
  }
  if (ret != DDS_RETCODE_OK)
    GVTRACE ("invalid flags %"PRIx16" for type\n", flags);
  return ret;
}
#undef F
#undef A
#undef M
#undef N
#undef H


#define T1 DDS_XTypes_TRY_CONSTRUCT1
#define T2 DDS_XTypes_TRY_CONSTRUCT2
#define X DDS_XTypes_IS_EXTERNAL
#define O DDS_XTypes_IS_OPTIONAL
#define M DDS_XTypes_IS_MUST_UNDERSTAND
#define K DDS_XTypes_IS_KEY
#define D DDS_XTypes_IS_DEFAULT
static dds_return_t xt_valid_member_flags (struct ddsi_domaingv *gv, uint16_t flags, uint8_t member_flag_kind, bool in_key)
{
  dds_return_t ret = DDS_RETCODE_OK;

  /* (flags & (T1|T2)) == 0 is invalid, but because there are some implementations
     that set this incorrectly, we can't reject it outright. (Cyclone got it wrong
     in the 0.9 release, but that is no longer sufficient reason to allow it.) */

  switch (member_flag_kind)
  {
    case MEMBER_FLAG_COLLECTION_ELEMENT:
      if (flags & ~(T1|T2|X))
        ret = DDS_RETCODE_BAD_PARAMETER;
      if ((flags & (T1|T2)) == 0 && !gv->config.allow_invalid_try_construct)
        ret = DDS_RETCODE_BAD_PARAMETER;
      break;
    case MEMBER_FLAG_STRUCT_MEMBER:
      if (flags & ~(T1|T2|O|M|K|X))
        ret = DDS_RETCODE_BAD_PARAMETER;
      if ((flags & (T1|T2)) == 0 && !gv->config.allow_invalid_try_construct)
        ret = DDS_RETCODE_BAD_PARAMETER;
      if (in_key && (flags & O))
        ret = DDS_RETCODE_BAD_PARAMETER;
      if ((flags & O) && (flags & K))
        ret = DDS_RETCODE_BAD_PARAMETER;
      break;
    case MEMBER_FLAG_UNION_MEMBER:
      if (flags & ~(T1|T2|D|X))
        ret = DDS_RETCODE_BAD_PARAMETER;
      if ((flags & (T1|T2)) == 0 && !gv->config.allow_invalid_try_construct)
        ret = DDS_RETCODE_BAD_PARAMETER;
      break;
    case MEMBER_FLAG_UNION_DISC:
      // must-understand not in spec
      if (flags & ~(T1|T2|M|K))
        ret = DDS_RETCODE_BAD_PARAMETER;
      if ((flags & (T1|T2)) == 0 && !gv->config.allow_invalid_try_construct)
        ret = DDS_RETCODE_BAD_PARAMETER;
      if (in_key && (flags & O))
        ret = DDS_RETCODE_BAD_PARAMETER;
      break;
    case MEMBER_FLAG_ENUM_LITERAL:
      if (flags & ~(D))
        ret = DDS_RETCODE_BAD_PARAMETER;
      break;
    case MEMBER_FLAG_ANNOTATION_PARAM:
    case MEMBER_FLAG_ALIAS_MEMBER:
    case MEMBER_FLAG_BIT_FLAG:
    case MEMBER_FLAG_BITSET_MEMBER:
      if (flags)
        ret = DDS_RETCODE_BAD_PARAMETER;
      break;
    default:
      ret = DDS_RETCODE_UNSUPPORTED;
      break;
  }
  if (ret != DDS_RETCODE_OK)
    GVTRACE ("invalid member flags %"PRIx16" for kind %"PRIx8"\n", flags, member_flag_kind);
  return ret;
}
#undef T1
#undef T2
#undef X
#undef O
#undef M
#undef K
#undef D

static dds_return_t xt_valid_struct_member_flags_raw (struct ddsi_domaingv *gv, const struct xt_type *t)
{
  dds_return_t ret;
  for (uint32_t n = 0; n < t->_u.structure.members.length; n++)
    if ((ret = xt_valid_member_flags (gv, t->_u.structure.members.seq[n].flags, MEMBER_FLAG_STRUCT_MEMBER, false)) != DDS_RETCODE_OK)
      return ret;
  return DDS_RETCODE_OK;
}

static dds_return_t xt_valid_array_bounds (struct ddsi_domaingv *gv, const struct xt_type *arr)
{
  const struct xt_type *t = arr;
  uint32_t dims = 1;
  while (t->_d == DDS_XTypes_TK_ARRAY)
  {
    if (t->_u.array.bounds._length == 0)
      return DDS_RETCODE_BAD_PARAMETER;
    for (uint32_t n = 0; n < t->_u.array.bounds._length; n++)
    {
      DDSRT_STATIC_ASSERT_IS_UNSIGNED(t->_u.array.bounds._buffer[n]);
      if (t->_u.array.bounds._buffer[n] == 0)
        return DDS_RETCODE_BAD_PARAMETER;
      if (UINT32_MAX / dims < t->_u.array.bounds._buffer[n])
      {
        GVTRACE ("array bound overflow\n");
        return DDS_RETCODE_BAD_PARAMETER;
      }
      dims *= t->_u.array.bounds._buffer[n];
    }
    t = ddsi_xt_unalias (&t->_u.array.c.element_type->xt);
  }
  return DDS_RETCODE_OK;
}

static uint32_t xt_validate_node_hash (const void *vnode)
{
  const struct xt_validate_node *node = vnode;
  const uintptr_t x = (uintptr_t) node->type;
  const uint32_t in_key = node->in_key ? 1u : 0u;
  return ddsrt_mh3 (&in_key, sizeof (in_key), ddsrt_mh3 (&x, sizeof (x), 0));
}

static bool xt_validate_node_equal (const void *vnode_a, const void *vnode_b)
{
  const struct xt_validate_node *a = vnode_a;
  const struct xt_validate_node *b = vnode_b;
  return a->type == b->type && a->in_key == b->in_key;
}

static void xt_validate_node_free (void *vnode, void *arg)
{
  (void) arg;
  ddsrt_free (vnode);
}

static dds_return_t xt_validate_context_init (struct xt_validate_context *context, bool allow_recursive_types)
{
  memset (context, 0, sizeof (*context));
  context->allow_recursive_types = allow_recursive_types;
  if ((context->types = ddsrt_hh_new (1, xt_validate_node_hash, xt_validate_node_equal)) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  return DDS_RETCODE_OK;
}

static void xt_validate_context_fini (struct xt_validate_context *context)
{
  ddsrt_hh_enum (context->types, xt_validate_node_free, NULL);
  ddsrt_hh_free (context->types);
}

static bool xt_validate_is_aggregate (const struct xt_type *t)
{
  return t->_d == DDS_XTypes_TK_STRUCTURE || t->_d == DDS_XTypes_TK_UNION;
}

static bool xt_validate_is_indirect_edge (uint16_t flags)
{
  return (flags & (DDS_XTypes_IS_EXTERNAL | DDS_XTypes_IS_OPTIONAL)) != 0;
}

static void xt_validate_path_enter (struct xt_validate_context *context, const struct xt_type *t, bool indirect_edge,
    uint32_t depth)
{
  assert (depth < XT_VALIDATE_MAX_DEPTH);
  context->path[depth + 1] = context->path[depth];
  if (xt_validate_is_aggregate (t))
    context->path[depth + 1].aggregate++;
  else if (t->_d == DDS_XTypes_TK_SEQUENCE)
    context->path[depth + 1].sequence++;
  if (indirect_edge)
    context->path[depth + 1].indirection++;
}

static bool xt_validate_cycle_is_valid (const struct xt_validate_context *context, const struct xt_validate_node *node,
    bool indirect_edge, uint32_t depth)
{
  assert (node->state == XT_VALIDATE_STARTED);
  assert (node->path_index < depth);
  /* Recursive IDL types must be aggregates, with indirection provided by a
     sequence or an @external/@optional edge. Aliases are transparent here
     because they count as neither. */
  const struct xt_validate_path_entry *from = &context->path[node->path_index];
  const struct xt_validate_path_entry *to = &context->path[depth];
  const bool has_aggregate = to->aggregate != from->aggregate;
  const bool has_indirection = to->sequence != from->sequence || to->indirection != from->indirection
      || indirect_edge;
  return has_aggregate && has_indirection;
}

static const char *xt_validate_edge_descr (enum xt_validate_edge edge)
{
  switch (edge)
  {
    case XT_VALIDATE_EDGE_TOP: return "top-level";
    case XT_VALIDATE_EDGE_ALIAS: return "alias";
    case XT_VALIDATE_EDGE_AGGREGATE: return "aggregate";
    case XT_VALIDATE_EDGE_SEQUENCE: return "sequence";
    case XT_VALIDATE_EDGE_ARRAY: return "array";
    case XT_VALIDATE_EDGE_MAP: return "map";
  }
  return "unknown";
}

static bool xt_validate_can_skip_committed_hash (const struct xt_validate_context *context,
    const struct xt_type *t, bool in_key, enum xt_validate_edge edge)
{
  if (!context->allow_recursive_types)
    return false;
  if (edge == XT_VALIDATE_EDGE_TOP || xt_is_non_hash (t))
    return false;

  const struct ddsi_type *type = ddsi_type_from_xt_type (t);
  if (type->state == DDSI_TYPE_CONSTRUCTING || type->state == DDSI_TYPE_COMPLETING)
    return true;
  if (in_key)
    return false;
  return type->state == DDSI_TYPE_RESOLVED;
}

static dds_return_t xt_validate_enter (struct ddsi_domaingv *gv, struct xt_validate_context *context,
    const struct xt_type *t, bool in_key, enum xt_validate_edge edge, bool indirect_edge, uint32_t depth,
    struct xt_validate_node **node, bool *skip)
{
  *skip = false;
  *node = NULL;

  if (ddsi_xt_missing_definition (t))
    return DDS_RETCODE_OK;

  const struct xt_validate_node templ = { .type = t, .in_key = in_key };
  if ((*node = ddsrt_hh_lookup (context->types, &templ)) != NULL)
  {
    if ((*node)->state == XT_VALIDATE_VALIDATED ||
        (context->allow_recursive_types && xt_validate_cycle_is_valid (context, *node, indirect_edge, depth)))
    {
      *skip = true;
      return DDS_RETCODE_OK;
    }
    GVTRACE ("%srecursive type dependency through %s\n",
             context->allow_recursive_types ? "invalid " : "disabled ",
             xt_validate_edge_descr (edge));
    return DDS_RETCODE_BAD_PARAMETER;
  }

  if ((*node = ddsrt_calloc (1, sizeof (**node))) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  (*node)->type = t;
  (*node)->in_key = in_key;
  (*node)->state = XT_VALIDATE_STARTED;
  (*node)->path_index = depth;
  ddsrt_hh_add_absent (context->types, *node);
  xt_validate_path_enter (context, t, indirect_edge, depth);
  return DDS_RETCODE_OK;
}

static dds_return_t xt_validate_impl (struct ddsi_domaingv *gv, struct xt_validate_context *context,
    const struct xt_type *t, bool in_key, enum xt_validate_edge edge, bool indirect_edge, uint32_t depth)
{
  dds_return_t ret;
  struct xt_validate_node *node;
  bool skip;

  if (depth >= XT_VALIDATE_MAX_DEPTH)
  {
    GVTRACE ("type validation recursion depth exceeded\n");
    return DDS_RETCODE_BAD_PARAMETER;
  }

  if ((ret = xt_validate_enter (gv, context, t, in_key, edge, indirect_edge, depth, &node, &skip)) != DDS_RETCODE_OK || skip)
    return ret;

  if (ddsi_xt_missing_definition (t))
    return DDS_RETCODE_OK;
  if (xt_validate_can_skip_committed_hash (context, t, in_key, edge))
  {
    node->state = XT_VALIDATE_VALIDATED;
    return DDS_RETCODE_OK;
  }
  switch (t->_d)
  {
    case DDS_XTypes_TK_ANNOTATION:
      // FIXME: annotation type not supported yet, no validation
      break;
    case DDS_XTypes_TK_STRUCTURE:
      if ((ret = xt_valid_type_flags (gv, t->_u.structure.flags, t->_d))
          || (ret = xt_valid_struct_member_flags_raw (gv, t))
          || (t->_u.structure.base_type && (ret = xt_validate_impl (gv, context, &t->_u.structure.base_type->xt, in_key, XT_VALIDATE_EDGE_AGGREGATE, false, depth + 1)))
          || (t->_u.structure.base_type && (ret = xt_valid_struct_base_type (gv, t)))
          || (ret = xt_valid_struct_member_ids (gv, t)))
        return ret;

      bool has_key_members = false;
      for (const struct xt_type *t1 = t; t1; )
      {
        for (uint32_t n = 0; !has_key_members && n < t1->_u.structure.members.length; n++)
          has_key_members = (t1->_u.structure.members.seq[n].flags & DDS_XTypes_IS_KEY);
        if ((ret = xt_resolve_valid_struct_base_type (gv, t1, &t1)) != DDS_RETCODE_OK)
          return ret;
      }

      for (uint32_t n = 0; n < t->_u.structure.members.length; n++)
      {
        DDS_XTypes_StructMemberFlag flags = t->_u.structure.members.seq[n].flags;
        /* A member is considered a key-member (and therefore cannot be optional)
           in case (1) the member has a key flag or (2) no member of the struct
           and it's base types has a key flag and the 'parent' member (the one
           that has this struct as it's member type) is a key (by either rule 1
           or 2). As a result, a type can be valid or invalid based on the context
           it is used in:
              struct A { @optional long x; };
              struct B { @key A y; };
              struct C { B z; };
           Type B is an invalid top-level type for a topic. Type C is a valid
           top-level type, but will still be rejected because of the key flag
           in B.
        */
        bool key = (in_key && !has_key_members) || (flags & DDS_XTypes_IS_KEY);
        if ((ret = xt_valid_member_flags (gv, flags, MEMBER_FLAG_STRUCT_MEMBER, key)))
          return ret;
        if ((ret = xt_validate_impl (gv, context, &t->_u.structure.members.seq[n].type->xt, key,
            XT_VALIDATE_EDGE_AGGREGATE, xt_validate_is_indirect_edge (flags), depth + 1)))
          return ret;
      }
      break;
    case DDS_XTypes_TK_UNION: {
      if ((ret = xt_validate_impl (gv, context, &t->_u.union_type.disc_type->xt, in_key, XT_VALIDATE_EDGE_AGGREGATE, false, depth + 1))
          || (ret = xt_valid_union_disc_type (gv, t))
          || (ret = xt_valid_union_member_ids (gv, t))
          || (ret = xt_valid_union_case_labels (gv, t))
          || (ret = xt_valid_type_flags (gv, t->_u.union_type.flags, t->_d))
          || (ret = xt_valid_member_flags (gv, t->_u.union_type.disc_flags, MEMBER_FLAG_UNION_DISC, in_key)))
        return ret;
      bool has_default = false;
      for (uint32_t n = 0; n < t->_u.union_type.members.length; n++)
      {
        DDS_XTypes_UnionMemberFlag flags = t->_u.union_type.members.seq[n].flags;
        if ((ret = xt_valid_member_flags (gv, flags, MEMBER_FLAG_UNION_MEMBER, in_key)))
          return ret;
        if ((ret = xt_validate_impl (gv, context, &t->_u.union_type.members.seq[n].type->xt, in_key,
            XT_VALIDATE_EDGE_AGGREGATE, xt_validate_is_indirect_edge (flags), depth + 1)))
          return ret;
        if (flags & DDS_XTypes_IS_DEFAULT)
        {
          if (has_default)
          {
            GVTRACE ("multiple default flags in union members (index %"PRIu32")\n", n);
            return DDS_RETCODE_BAD_PARAMETER;
          }
          has_default = true;
        }
      }
      break;
    }
    case DDS_XTypes_TK_ENUM:
      if (t->_u.enum_type.bit_bound == 0 || t->_u.enum_type.bit_bound > 32)
        return DDS_RETCODE_BAD_PARAMETER;
      if ((ret = xt_valid_type_flags (gv, t->_u.enum_type.flags, t->_d))
          || (ret = xt_valid_enum_values (gv, t)))
        return ret;
      for (uint32_t n = 0; n < t->_u.enum_type.literals.length; n++)
        if ((ret = xt_valid_member_flags (gv, t->_u.enum_type.literals.seq[n].flags, MEMBER_FLAG_ENUM_LITERAL, in_key)))
          return ret;
      break;
    case DDS_XTypes_TK_BITMASK:
      if (t->_u.bitmask.bit_bound == 0 || t->_u.bitmask.bit_bound > 64)
        return DDS_RETCODE_BAD_PARAMETER;
      if ((ret = xt_valid_type_flags (gv, t->_u.bitmask.flags, t->_d))
          || (ret = xt_valid_bitmask_positions (gv, t)))
        return ret;
      for (uint32_t n = 0; n < t->_u.bitmask.bitflags.length; n++)
        if ((ret = xt_valid_member_flags (gv, t->_u.bitmask.bitflags.seq[n].flags, MEMBER_FLAG_BIT_FLAG, in_key)))
          return ret;
      break;
    case DDS_XTypes_TK_ALIAS:
      if ((ret = xt_valid_type_flags (gv, t->_u.alias.flags, t->_d))
          || (ret = xt_valid_member_flags (gv, t->_u.alias.related_flags, MEMBER_FLAG_ALIAS_MEMBER, in_key))
          || (ret = xt_validate_impl (gv, context, &t->_u.alias.related_type->xt, in_key, XT_VALIDATE_EDGE_ALIAS, false, depth + 1)))
        return ret;
      break;
    case DDS_XTypes_TK_BITSET:
      if ((ret = xt_valid_type_flags (gv, t->_u.bitset.flags, t->_d)))
        return ret;
      // FIXME: add validation for holder type and bit positions when bitset type is implemented
      for (uint32_t n = 0; n < t->_u.bitset.fields.length; n++)
        if ((ret = xt_valid_member_flags (gv, t->_u.bitset.fields.seq[n].flags, MEMBER_FLAG_BITSET_MEMBER, in_key)))
          return ret;
      break;
    case DDS_XTypes_TK_SEQUENCE:
      if ((ret = xt_valid_type_flags (gv, t->_u.seq.c.flags, t->_d))
          || (ret = xt_valid_member_flags (gv, t->_u.seq.c.element_flags, MEMBER_FLAG_COLLECTION_ELEMENT, in_key))
          || (ret = xt_validate_impl (gv, context, &t->_u.seq.c.element_type->xt, in_key,
              XT_VALIDATE_EDGE_SEQUENCE, xt_validate_is_indirect_edge (t->_u.seq.c.element_flags), depth + 1)))
        return ret;
      break;
    case DDS_XTypes_TK_ARRAY:
      if ((ret = xt_valid_type_flags (gv, t->_u.array.c.flags, t->_d))
          || (ret = xt_valid_member_flags (gv, t->_u.array.c.element_flags, MEMBER_FLAG_COLLECTION_ELEMENT, in_key))
          || (ret = xt_validate_impl (gv, context, &t->_u.array.c.element_type->xt, in_key,
              XT_VALIDATE_EDGE_ARRAY, xt_validate_is_indirect_edge (t->_u.array.c.element_flags), depth + 1))
          || (ret = xt_valid_array_bounds (gv, t)))
        return ret;
      break;
    case DDS_XTypes_TK_MAP:
      if ((ret = xt_valid_type_flags (gv, t->_u.map.c.flags, t->_d))
          || (ret = xt_valid_member_flags (gv, t->_u.map.c.element_flags, MEMBER_FLAG_COLLECTION_ELEMENT, in_key))
          || (ret = xt_valid_member_flags (gv, t->_u.map.key_flags, MEMBER_FLAG_COLLECTION_ELEMENT, in_key))
          || (ret = xt_validate_impl (gv, context, &t->_u.map.key_type->xt, in_key,
              XT_VALIDATE_EDGE_MAP, xt_validate_is_indirect_edge (t->_u.map.key_flags), depth + 1))
          || (ret = xt_validate_impl (gv, context, &t->_u.map.c.element_type->xt, in_key,
              XT_VALIDATE_EDGE_MAP, xt_validate_is_indirect_edge (t->_u.map.c.element_flags), depth + 1)))
        return ret;
      break;
    case DDS_XTypes_TK_BOOLEAN: case DDS_XTypes_TK_BYTE:
    case DDS_XTypes_TK_INT8: case DDS_XTypes_TK_INT16: case DDS_XTypes_TK_INT32: case DDS_XTypes_TK_INT64:
    case DDS_XTypes_TK_UINT8: case DDS_XTypes_TK_UINT16: case DDS_XTypes_TK_UINT32: case DDS_XTypes_TK_UINT64:
    case DDS_XTypes_TK_FLOAT32: case DDS_XTypes_TK_FLOAT64: case DDS_XTypes_TK_FLOAT128:
    case DDS_XTypes_TK_CHAR8: case DDS_XTypes_TK_CHAR16: case DDS_XTypes_TK_STRING8: case DDS_XTypes_TK_STRING16:
      // no validations
      break;
    default:
      return DDS_RETCODE_UNSUPPORTED;
  }
  node->state = XT_VALIDATE_VALIDATED;
  return DDS_RETCODE_OK;
}

dds_return_t ddsi_xt_validate (struct ddsi_domaingv *gv, const struct ddsi_type *type)
{
  struct xt_validate_context context;
  dds_return_t ret;
  if ((ret = xt_validate_context_init (&context, gv->config.allow_recursive_types)) != DDS_RETCODE_OK)
    return ret;
  ret = xt_validate_impl (gv, &context, &type->xt, false, XT_VALIDATE_EDGE_TOP, false, 0);
  xt_validate_context_fini (&context);
  return ret;
}

static dds_return_t add_minimal_typeobj (struct ddsi_domaingv *gv, struct xt_type *xt, const struct DDS_XTypes_TypeObject *to)
{
  const struct DDS_XTypes_MinimalTypeObject *mto = &to->_u.minimal;
  dds_return_t ret = DDS_RETCODE_OK;
  if (!xt->_d)
    xt->_d = mto->_d;
  else if (xt->_d != mto->_d)
  {
    GVTRACE ("typeobject has invalid type kind\n");
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto err_tk;
  }
  switch (mto->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      xt->_u.alias.flags = mto->_u.alias_type.alias_flags;
      if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.alias.related_type, &mto->_u.alias_type.body.common.related_type)) != DDS_RETCODE_OK)
        goto err_to;
      xt->_u.alias.related_flags = mto->_u.alias_type.body.common.related_flags;
      break;
    case DDS_XTypes_TK_ANNOTATION:
      ret = DDS_RETCODE_UNSUPPORTED; /* FIXME: not implemented */
      goto err_to;
    case DDS_XTypes_TK_STRUCTURE:
      xt->_u.structure.flags = mto->_u.struct_type.struct_flags;
      if (mto->_u.struct_type.header.base_type._d)
      {
        if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.structure.base_type, &mto->_u.struct_type.header.base_type)) != DDS_RETCODE_OK)
          goto err_to;
      }
      else
        xt->_u.structure.base_type = NULL;
      xt->_u.structure.members.length = mto->_u.struct_type.member_seq._length;
      xt->_u.structure.members.seq = ddsrt_calloc (xt->_u.structure.members.length, sizeof (*xt->_u.structure.members.seq));
      for (uint32_t n = 0; n < xt->_u.structure.members.length; n++)
      {
        xt->_u.structure.members.seq[n].id = mto->_u.struct_type.member_seq._buffer[n].common.member_id;
        xt->_u.structure.members.seq[n].flags = mto->_u.struct_type.member_seq._buffer[n].common.member_flags;
        if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.structure.members.seq[n].type, &mto->_u.struct_type.member_seq._buffer[n].common.member_type_id)) != DDS_RETCODE_OK)
        {
          for (uint32_t m = 0; m < n; m++)
            ddsi_type_unref_locked (gv, xt->_u.structure.members.seq[m].type);
          if (xt->_u.structure.base_type)
            ddsi_type_unref_locked (gv, xt->_u.structure.base_type);
          ddsrt_free (xt->_u.structure.members.seq);
          goto err_to;
        }
        memcpy (xt->_u.structure.members.seq[n].detail.name_hash, mto->_u.struct_type.member_seq._buffer[n].detail.name_hash,
          sizeof (xt->_u.structure.members.seq[n].detail.name_hash));
      }
      break;
    case DDS_XTypes_TK_UNION:
      xt->_u.union_type.flags = mto->_u.union_type.union_flags;
      if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.union_type.disc_type, &mto->_u.union_type.discriminator.common.type_id)) != DDS_RETCODE_OK)
        goto err_to;
      xt->_u.union_type.disc_flags = mto->_u.union_type.discriminator.common.member_flags;
      xt->_u.union_type.members.length = mto->_u.union_type.member_seq._length;
      xt->_u.union_type.members.seq = ddsrt_calloc (xt->_u.union_type.members.length, sizeof (*xt->_u.union_type.members.seq));
      for (uint32_t n = 0; n < xt->_u.union_type.members.length; n++)
      {
        xt->_u.union_type.members.seq[n].id = mto->_u.union_type.member_seq._buffer[n].common.member_id;
        xt->_u.union_type.members.seq[n].flags = mto->_u.union_type.member_seq._buffer[n].common.member_flags;
        if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.union_type.members.seq[n].type, &mto->_u.union_type.member_seq._buffer[n].common.type_id)) != DDS_RETCODE_OK)
        {
          for (uint32_t m = 0; m < n; m++)
          {
            ddsi_type_unref_locked (gv, xt->_u.union_type.members.seq[m].type);
            ddsrt_free (xt->_u.union_type.members.seq[m].label_seq._buffer);
          }
          ddsi_type_unref_locked (gv, xt->_u.union_type.disc_type);
          ddsrt_free (xt->_u.union_type.members.seq);
          goto err_to;
        }
        xt->_u.union_type.members.seq[n].label_seq._length = mto->_u.union_type.member_seq._buffer[n].common.label_seq._length;
        if (xt->_u.union_type.members.seq[n].label_seq._length > 0) {
          xt->_u.union_type.members.seq[n].label_seq._buffer =
            ddsrt_memdup (mto->_u.union_type.member_seq._buffer[n].common.label_seq._buffer,
                          mto->_u.union_type.member_seq._buffer[n].common.label_seq._length * sizeof (*mto->_u.union_type.member_seq._buffer[n].common.label_seq._buffer));
          xt->_u.union_type.members.seq[n].label_seq._release = true;
        } else {
          xt->_u.union_type.members.seq[n].label_seq._buffer = NULL;
          xt->_u.union_type.members.seq[n].label_seq._release = false;
        }
        memcpy (xt->_u.union_type.members.seq[n].detail.name_hash, mto->_u.union_type.member_seq._buffer[n].detail.name_hash,
          sizeof (xt->_u.union_type.members.seq[n].detail.name_hash));
      }
      break;
    case DDS_XTypes_TK_BITSET:
      xt->_u.bitset.flags = mto->_u.bitset_type.bitset_flags;
      xt->_u.bitset.fields.length = mto->_u.bitset_type.field_seq._length;
      xt->_u.bitset.fields.seq = ddsrt_calloc (xt->_u.bitset.fields.length, sizeof (*xt->_u.bitset.fields.seq));
      for (uint32_t n = 0; n < xt->_u.bitset.fields.length; n++)
      {
        xt->_u.bitset.fields.seq[n].position = mto->_u.bitset_type.field_seq._buffer[n].common.position;
        xt->_u.bitset.fields.seq[n].flags = mto->_u.bitset_type.field_seq._buffer[n].common.flags;
        xt->_u.bitset.fields.seq[n].bitcount = mto->_u.bitset_type.field_seq._buffer[n].common.bitcount;
        xt->_u.bitset.fields.seq[n].holder_type = mto->_u.bitset_type.field_seq._buffer[n].common.holder_type;
        xt->_u.bitset.fields.seq[n].flags = mto->_u.bitset_type.field_seq._buffer[n].common.flags;
        memcpy (xt->_u.bitset.fields.seq[n].detail.name_hash, mto->_u.bitset_type.field_seq._buffer[n].name_hash,
          sizeof (xt->_u.bitset.fields.seq[n].detail.name_hash));
      }
      break;
    case DDS_XTypes_TK_SEQUENCE:
      xt->_u.seq.c.flags = mto->_u.sequence_type.collection_flag;
      if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.seq.c.element_type, &mto->_u.sequence_type.element.common.type)) != DDS_RETCODE_OK)
        goto err_to;
      xt->_u.seq.c.element_flags = mto->_u.sequence_type.element.common.element_flags;
      xt->_u.seq.bound = mto->_u.sequence_type.header.common.bound;
      break;
    case DDS_XTypes_TK_ARRAY:
      xt->_u.array.c.flags = mto->_u.array_type.collection_flag;
      if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.array.c.element_type, &mto->_u.array_type.element.common.type)) != DDS_RETCODE_OK)
        goto err_to;
      xt->_u.array.c.element_flags = mto->_u.array_type.element.common.element_flags;
      xt_lbounds_dup (&xt->_u.array.bounds, &mto->_u.array_type.header.common.bound_seq);
      break;
    case DDS_XTypes_TK_MAP:
      xt->_u.map.c.flags = mto->_u.map_type.collection_flag;
      if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.map.c.element_type, &mto->_u.map_type.element.common.type)) != DDS_RETCODE_OK)
        goto err_to;
      xt->_u.map.c.element_flags = mto->_u.map_type.element.common.element_flags;
      if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.map.key_type, &mto->_u.map_type.key.common.type)) != DDS_RETCODE_OK)
      {
        ddsi_type_unref_locked (gv, xt->_u.map.c.element_type);
        goto err_to;
      }
      xt->_u.map.key_flags = mto->_u.map_type.key.common.element_flags;
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
err_to:
  xt->_d = DDS_XTypes_TK_NONE;
  return ret;
}

static dds_return_t add_complete_typeobj (struct ddsi_domaingv *gv, struct xt_type *xt, const struct DDS_XTypes_TypeObject *to)
{
  const struct DDS_XTypes_CompleteTypeObject *cto = &to->_u.complete;
  dds_return_t ret = DDS_RETCODE_OK;
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
      xt->_u.alias.flags = cto->_u.alias_type.alias_flags;
      if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.alias.related_type, &cto->_u.alias_type.body.common.related_type)) != DDS_RETCODE_OK)
        goto err_to;
      xt->_u.alias.related_flags = cto->_u.alias_type.body.common.related_flags;
      set_type_detail (&xt->_u.alias.detail, &cto->_u.alias_type.header.detail);
      xt->_u.alias.flags = cto->_u.alias_type.alias_flags;
      xt->_u.alias.related_flags = cto->_u.alias_type.body.common.related_flags;
      break;
    case DDS_XTypes_TK_ANNOTATION:
      ret = DDS_RETCODE_UNSUPPORTED; /* FIXME: not implemented */
      goto err_to;
    case DDS_XTypes_TK_STRUCTURE:
      xt->_u.structure.flags = cto->_u.struct_type.struct_flags;
      if (cto->_u.struct_type.header.base_type._d)
      {
        if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.structure.base_type, &cto->_u.struct_type.header.base_type)) != DDS_RETCODE_OK)
          goto err_to;
      }
      else
        xt->_u.structure.base_type = NULL;
      set_type_detail (&xt->_u.structure.detail, &cto->_u.struct_type.header.detail);
      xt->_u.structure.members.length = cto->_u.struct_type.member_seq._length;
      xt->_u.structure.members.seq = ddsrt_calloc (xt->_u.structure.members.length, sizeof (*xt->_u.structure.members.seq));
      for (uint32_t n = 0; n < xt->_u.structure.members.length; n++)
      {
        xt->_u.structure.members.seq[n].id = cto->_u.struct_type.member_seq._buffer[n].common.member_id;
        xt->_u.structure.members.seq[n].flags = cto->_u.struct_type.member_seq._buffer[n].common.member_flags;
        if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.structure.members.seq[n].type, &cto->_u.struct_type.member_seq._buffer[n].common.member_type_id)) != DDS_RETCODE_OK)
        {
          for (uint32_t m = 0; m < n; m++)
          {
            ddsi_type_unref_locked (gv, xt->_u.structure.members.seq[m].type);
            xt_applied_member_annotations_fini (&xt->_u.structure.members.seq[m].detail.annotations);
          }
          xt_applied_type_annotations_fini (&xt->_u.structure.detail.annotations);
          if (xt->_u.structure.base_type)
            ddsi_type_unref_locked (gv, xt->_u.structure.base_type);
          ddsrt_free (xt->_u.structure.members.seq);
          goto err_to;
        }
        set_member_detail (&xt->_u.structure.members.seq[n].detail, &cto->_u.struct_type.member_seq._buffer[n].detail);
      }
      break;
    case DDS_XTypes_TK_UNION:
      xt->_u.union_type.flags = cto->_u.union_type.union_flags;
      if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.union_type.disc_type, &cto->_u.union_type.discriminator.common.type_id)) != DDS_RETCODE_OK)
        goto err_to;
      xt->_u.union_type.disc_flags = cto->_u.union_type.discriminator.common.member_flags;
      set_type_annotations (&xt->_u.union_type.disc_annotations, cto->_u.union_type.discriminator.ann_builtin, cto->_u.union_type.discriminator.ann_custom);
      set_type_detail (&xt->_u.union_type.detail, &cto->_u.union_type.header.detail);
      xt->_u.union_type.members.length = cto->_u.union_type.member_seq._length;
      xt->_u.union_type.members.seq = ddsrt_calloc (xt->_u.union_type.members.length, sizeof (*xt->_u.union_type.members.seq));
      for (uint32_t n = 0; n < xt->_u.union_type.members.length; n++)
      {
        xt->_u.union_type.members.seq[n].id = cto->_u.union_type.member_seq._buffer[n].common.member_id;
        xt->_u.union_type.members.seq[n].flags = cto->_u.union_type.member_seq._buffer[n].common.member_flags;
        if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.union_type.members.seq[n].type, &cto->_u.union_type.member_seq._buffer[n].common.type_id)) != DDS_RETCODE_OK)
        {
          for (uint32_t m = 0; m < n; m++)
          {
            ddsi_type_unref_locked (gv, xt->_u.union_type.members.seq[m].type);
            ddsrt_free (xt->_u.union_type.members.seq[m].label_seq._buffer);
            xt_applied_member_annotations_fini (&xt->_u.union_type.members.seq[m].detail.annotations);
          }
          xt_applied_type_annotations_fini (&xt->_u.union_type.disc_annotations);
          xt_applied_type_annotations_fini (&xt->_u.union_type.detail.annotations);
          ddsi_type_unref_locked (gv, xt->_u.union_type.disc_type);
          ddsrt_free (xt->_u.union_type.members.seq);
          goto err_to;
        }
        xt->_u.union_type.members.seq[n].label_seq._length = cto->_u.union_type.member_seq._buffer[n].common.label_seq._length;
        if (xt->_u.union_type.members.seq[n].label_seq._length > 0) {
          xt->_u.union_type.members.seq[n].label_seq._buffer =
            ddsrt_memdup (cto->_u.union_type.member_seq._buffer[n].common.label_seq._buffer,
                          cto->_u.union_type.member_seq._buffer[n].common.label_seq._length * sizeof (*cto->_u.union_type.member_seq._buffer[n].common.label_seq._buffer));
          xt->_u.union_type.members.seq[n].label_seq._release = true;
        } else {
          xt->_u.union_type.members.seq[n].label_seq._buffer = NULL;
          xt->_u.union_type.members.seq[n].label_seq._release = false;
        }
        set_member_detail (&xt->_u.union_type.members.seq[n].detail, &cto->_u.union_type.member_seq._buffer[n].detail);
      }
      break;
    case DDS_XTypes_TK_BITSET:
      set_type_detail (&xt->_u.bitset.detail, &cto->_u.bitset_type.header.detail);
      xt->_u.bitset.flags = cto->_u.bitset_type.bitset_flags;
      xt->_u.bitset.fields.length = cto->_u.bitset_type.field_seq._length;
      xt->_u.bitset.fields.seq = ddsrt_calloc (xt->_u.bitset.fields.length, sizeof (*xt->_u.bitset.fields.seq));
      for (uint32_t n = 0; n < xt->_u.bitset.fields.length; n++)
      {
        xt->_u.bitset.fields.seq[n].position = cto->_u.bitset_type.field_seq._buffer[n].common.position;
        xt->_u.bitset.fields.seq[n].flags = cto->_u.bitset_type.field_seq._buffer[n].common.flags;
        xt->_u.bitset.fields.seq[n].bitcount = cto->_u.bitset_type.field_seq._buffer[n].common.bitcount;
        xt->_u.bitset.fields.seq[n].holder_type = cto->_u.bitset_type.field_seq._buffer[n].common.holder_type;
        xt->_u.bitset.fields.seq[n].flags = cto->_u.bitset_type.field_seq._buffer[n].common.flags;
        set_member_detail (&xt->_u.bitset.fields.seq[n].detail, &cto->_u.bitset_type.field_seq._buffer[n].detail);
      }
      break;
    case DDS_XTypes_TK_SEQUENCE:
      xt->_u.seq.c.flags = cto->_u.sequence_type.collection_flag;
      if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.seq.c.element_type, &cto->_u.sequence_type.element.common.type)) != DDS_RETCODE_OK)
        goto err_to;
      xt->_u.seq.c.element_flags = cto->_u.sequence_type.element.common.element_flags;
      xt->_u.seq.bound = cto->_u.sequence_type.header.common.bound;
      if (cto->_u.sequence_type.header.detail)
        set_type_detail (&xt->_u.seq.c.detail, cto->_u.sequence_type.header.detail);
      set_member_annotations (&xt->_u.seq.c.element_annotations, cto->_u.sequence_type.element.detail.ann_builtin, cto->_u.sequence_type.element.detail.ann_custom);
      break;
    case DDS_XTypes_TK_ARRAY:
      xt->_u.array.c.flags = cto->_u.array_type.collection_flag;
      if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.array.c.element_type, &cto->_u.array_type.element.common.type)) != DDS_RETCODE_OK)
        goto err_to;
      xt->_u.array.c.element_flags = cto->_u.array_type.element.common.element_flags;
      xt_lbounds_dup (&xt->_u.array.bounds, &cto->_u.array_type.header.common.bound_seq);
      set_type_detail (&xt->_u.array.c.detail, &cto->_u.array_type.header.detail);
      set_member_annotations (&xt->_u.array.c.element_annotations, cto->_u.array_type.element.detail.ann_builtin, cto->_u.array_type.element.detail.ann_custom);
      break;
    case DDS_XTypes_TK_MAP:
      xt->_u.map.c.flags = cto->_u.map_type.collection_flag;
      if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.map.c.element_type, &cto->_u.map_type.element.common.type)) != DDS_RETCODE_OK)
        goto err_to;
      xt->_u.map.c.element_flags = cto->_u.map_type.element.common.element_flags;
      if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.map.key_type, &cto->_u.map_type.key.common.type)) != DDS_RETCODE_OK)
      {
        ddsi_type_unref_locked (gv, xt->_u.map.c.element_type);
        goto err_to;
      }
      xt->_u.map.key_flags = cto->_u.map_type.key.common.element_flags;
      xt->_u.map.bound = cto->_u.map_type.header.common.bound;
      if (cto->_u.map_type.header.detail)
        set_type_detail (&xt->_u.map.c.detail, cto->_u.map_type.header.detail);
      set_member_annotations (&xt->_u.map.c.element_annotations, cto->_u.map_type.element.detail.ann_builtin, cto->_u.map_type.element.detail.ann_custom);
      set_member_annotations (&xt->_u.map.key_annotations, cto->_u.map_type.key.detail.ann_builtin, cto->_u.map_type.key.detail.ann_custom);
      break;
    case DDS_XTypes_TK_ENUM:
      xt->_u.enum_type.flags = cto->_u.enumerated_type.enum_flags;
      xt->_u.enum_type.bit_bound = cto->_u.enumerated_type.header.common.bit_bound;
      set_type_detail (&xt->_u.enum_type.detail, &cto->_u.enumerated_type.header.detail);
      xt->_u.enum_type.literals.length = cto->_u.enumerated_type.literal_seq._length;
      xt->_u.enum_type.literals.seq = ddsrt_calloc (xt->_u.enum_type.literals.length, sizeof (*xt->_u.enum_type.literals.seq));
      for (uint32_t n = 0; n < xt->_u.enum_type.literals.length; n++)
      {
        xt->_u.enum_type.literals.seq[n].value = cto->_u.enumerated_type.literal_seq._buffer[n].common.value;
        xt->_u.enum_type.literals.seq[n].flags = cto->_u.enumerated_type.literal_seq._buffer[n].common.flags;
        set_member_detail (&xt->_u.enum_type.literals.seq[n].detail, &cto->_u.enumerated_type.literal_seq._buffer[n].detail);
      }
      break;
    case DDS_XTypes_TK_BITMASK:
      xt->_u.bitmask.flags = cto->_u.bitmask_type.bitmask_flags;
      xt->_u.bitmask.bit_bound = cto->_u.bitmask_type.header.common.bit_bound;
      set_type_detail (&xt->_u.bitmask.detail, &cto->_u.bitmask_type.header.detail);
      xt->_u.bitmask.bitflags.length = cto->_u.bitmask_type.flag_seq._length;
      xt->_u.bitmask.bitflags.seq = ddsrt_calloc (xt->_u.bitmask.bitflags.length, sizeof (*xt->_u.bitmask.bitflags.seq));
      for (uint32_t n = 0; n < xt->_u.bitmask.bitflags.length; n++)
      {
        xt->_u.bitmask.bitflags.seq[n].position = cto->_u.bitmask_type.flag_seq._buffer[n].common.position;
        xt->_u.bitmask.bitflags.seq[n].flags = cto->_u.bitmask_type.flag_seq._buffer[n].common.flags;
        set_member_detail (&xt->_u.bitmask.bitflags.seq[n].detail, &cto->_u.bitmask_type.flag_seq._buffer[n].detail);
      }
      break;
    default:
      ret = DDS_RETCODE_UNSUPPORTED; /* not supported */
      goto err_tk;
  }
  return ret;

err_tk:
err_to:
  xt->_d = DDS_XTypes_TK_NONE;
  return ret;
}

ddsrt_nonnull_all
dds_return_t ddsi_xt_type_add_typeobj (struct ddsi_domaingv *gv, struct ddsi_type *type, const struct DDS_XTypes_TypeObject *to)
{
  dds_return_t ret = DDS_RETCODE_OK, ret_validate = DDS_RETCODE_OK;
  struct xt_type *xt = &type->xt;
  assert (xt->kind == DDSI_TYPEID_KIND_MINIMAL || xt->kind == DDSI_TYPEID_KIND_COMPLETE);
  if (xt->_d != DDS_XTypes_TK_NONE)
    return DDS_RETCODE_OK;

  if (xt->kind == DDSI_TYPEID_KIND_MINIMAL)
    ret = (to->_d != DDS_XTypes_EK_MINIMAL) ? DDS_RETCODE_BAD_PARAMETER : add_minimal_typeobj (gv, xt, to);
  else
    ret = (to->_d != DDS_XTypes_EK_COMPLETE) ? DDS_RETCODE_BAD_PARAMETER : add_complete_typeobj (gv, xt, to);

  if (ret != DDS_RETCODE_OK || (ret_validate = ddsi_xt_validate (gv, type)) != DDS_RETCODE_OK)
  {
    if (ret == DDS_RETCODE_OK)
    {
      ddsi_xt_type_fini_owned (gv, type, xt, false);
      ret = ret_validate;
    }
    GVWARNING ("type " PTYPEIDFMT ": ddsi_xt_type_add_typeobj with invalid type object\n", PTYPEID (xt->id.x));
  }

  return ret;
}

ddsrt_nonnull ((1, 2, 3))
dds_return_t ddsi_xt_type_init_impl (struct ddsi_domaingv *gv, struct ddsi_type *type, const struct DDS_XTypes_TypeIdentifier *ti, const struct DDS_XTypes_TypeObject *to)
{
  struct xt_type *xt = &type->xt;
  dds_return_t ret = DDS_RETCODE_OK;

  ddsi_typeid_copy_impl (&xt->id.x, ti);
  if (ti->_d <= DDS_XTypes_TK_STRING16)
  {
    if (to != NULL)
      return DDS_RETCODE_BAD_PARAMETER;
    xt->_d = ti->_d;
  }
  else
  {
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
        if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.seq.c.element_type, ti->_u.seq_sdefn.element_identifier)) != DDS_RETCODE_OK)
          goto err;
        xt->_u.seq.bound = (DDS_XTypes_LBound) ti->_u.seq_sdefn.bound;
        xt_collection_common_init (&xt->_u.seq.c, &ti->_u.seq_sdefn.header);
        break;
      case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
        xt->_d = DDS_XTypes_TK_SEQUENCE;
        if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.seq.c.element_type, ti->_u.seq_ldefn.element_identifier)) != DDS_RETCODE_OK)
          goto err;
        xt->_u.seq.bound = ti->_u.seq_ldefn.bound;
        xt_collection_common_init (&xt->_u.seq.c, &ti->_u.seq_ldefn.header);
        break;
      case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
        xt->_d = DDS_XTypes_TK_ARRAY;
        if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.array.c.element_type, ti->_u.array_sdefn.element_identifier)) != DDS_RETCODE_OK)
          goto err;
        xt_collection_common_init (&xt->_u.array.c, &ti->_u.array_sdefn.header);
        xt_sbounds_to_lbounds (&xt->_u.array.bounds, &ti->_u.array_sdefn.array_bound_seq);
        break;
      case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
        xt->_d = DDS_XTypes_TK_ARRAY;
        if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.array.c.element_type, ti->_u.array_ldefn.element_identifier)) != DDS_RETCODE_OK)
          goto err;
        xt_collection_common_init (&xt->_u.array.c, &ti->_u.array_ldefn.header);
        xt_lbounds_dup (&xt->_u.array.bounds, &ti->_u.array_ldefn.array_bound_seq);
        break;
      case DDS_XTypes_TI_PLAIN_MAP_SMALL:
        xt->_d = DDS_XTypes_TK_MAP;
        if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.map.c.element_type, ti->_u.map_sdefn.element_identifier)) != DDS_RETCODE_OK)
          goto err;
        xt->_u.map.bound = (DDS_XTypes_LBound) ti->_u.map_sdefn.bound;
        xt_collection_common_init (&xt->_u.map.c, &ti->_u.map_sdefn.header);
        if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.map.key_type, ti->_u.map_sdefn.key_identifier)) != DDS_RETCODE_OK)
        {
          ddsi_type_unref_locked (gv, xt->_u.map.c.element_type);
          xt->_u.map.c.element_type = NULL;
          goto err;
        }
        xt->_u.map.key_flags = ti->_u.map_sdefn.key_flags;
        break;
      case DDS_XTypes_TI_PLAIN_MAP_LARGE:
        xt->_d = DDS_XTypes_TK_MAP;
        if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.map.c.element_type, ti->_u.map_ldefn.element_identifier)) != DDS_RETCODE_OK)
          goto err;
        xt->_u.map.bound = (DDS_XTypes_LBound) ti->_u.map_ldefn.bound;
        xt_collection_common_init (&xt->_u.map.c, &ti->_u.map_ldefn.header);
        if ((ret = ddsi_type_register_dep (gv, &xt->id, &xt->_u.map.key_type, ti->_u.map_ldefn.key_identifier)) != DDS_RETCODE_OK)
        {
          ddsi_type_unref_locked (gv, xt->_u.map.c.element_type);
          xt->_u.map.c.element_type = NULL;
          goto err;
        }
        xt->_u.map.key_flags = ti->_u.map_ldefn.key_flags;
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
        if (to == NULL)
          xt->sc_component_id = ti->_u.sc_component_id;
        else if (ti->_u.sc_component_id.sc_component_id._d == DDS_XTypes_EK_MINIMAL && to->_d == DDS_XTypes_EK_MINIMAL)
          ret = add_minimal_typeobj (gv, xt, to);
        else if (ti->_u.sc_component_id.sc_component_id._d == DDS_XTypes_EK_COMPLETE && to->_d == DDS_XTypes_EK_COMPLETE)
          ret = add_complete_typeobj (gv, xt, to);
        else
          ret = DDS_RETCODE_BAD_PARAMETER;
        break;
      default:
        ddsi_typeid_fini (&xt->id);
        ret = DDS_RETCODE_UNSUPPORTED; /* not supported */
        break;
    }
  }
  if (ret != DDS_RETCODE_OK || (ret = ddsi_xt_validate (gv, type)) != DDS_RETCODE_OK)
  {
    GVWARNING ("type " PTYPEIDFMT ": ddsi_xt_type_init_impl with invalid type object\n", PTYPEID (xt->id.x));
    goto err;
  }
  if ((xt->kind = ddsi_typeid_kind_impl (ti)) == DDSI_TYPEID_KIND_INVALID)
  {
    GVWARNING ("type " PTYPEIDFMT ": ddsi_xt_type_init_impl with invalid typeid\n", PTYPEID (xt->id.x));
    ret = DDS_RETCODE_BAD_PARAMETER;
    goto err;
  }

err:
  return ret;
}

ddsrt_nonnull_all
dds_return_t ddsi_xt_type_init (struct ddsi_domaingv *gv, struct ddsi_type *type, const ddsi_typeid_t *ti, const ddsi_typeobj_t *to)
{
  return ddsi_xt_type_init_impl (gv, type, &ti->x, &to->x);
}

ddsrt_nonnull ((1))
static void DDS_XTypes_AppliedVerbatimAnnotation_copy (struct DDS_XTypes_AppliedVerbatimAnnotation *dst, const struct DDS_XTypes_AppliedVerbatimAnnotation *src)
{
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
    (*dst)->_release = true;
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
    dst->_release = true;
  }
}

static void DDS_XTypes_AppliedBuiltinMemberAnnotations_copy (struct DDS_XTypes_AppliedBuiltinMemberAnnotations *dst, const struct DDS_XTypes_AppliedBuiltinMemberAnnotations *src)
{
  if (src)
  {
    dst->unit = src->unit ? ddsrt_strdup (src->unit) : NULL;
    dst->min = src->min ? ddsrt_memdup (src->min, sizeof(struct DDS_XTypes_AnnotationParameterValue)) : NULL;
    dst->max = src->max ? ddsrt_memdup (src->max, sizeof(struct DDS_XTypes_AnnotationParameterValue)) : NULL;
    dst->hash_id = src->hash_id ? ddsrt_strdup (src->hash_id) : NULL;
  }
}

static void get_type_detail (DDS_XTypes_CompleteTypeDetail *dst, const struct xt_type_detail *src)
{
  ddsrt_strlcpy (dst->type_name, src->type_name, sizeof (dst->type_name));
  if (src->annotations.ann_builtin)
  {
    dst->ann_builtin = ddsrt_calloc (1, sizeof(struct DDS_XTypes_AppliedBuiltinTypeAnnotations));
    DDS_XTypes_AppliedBuiltinTypeAnnotations_copy (dst->ann_builtin, src->annotations.ann_builtin);
  }
  else
  {
    dst->ann_builtin = NULL;
  }

  if (src->annotations.ann_custom)
  {
    dst->ann_custom = ddsrt_calloc(1, sizeof(DDS_XTypes_AppliedAnnotationSeq));
    DDS_XTypes_AppliedAnnotationSeq_copy (dst->ann_custom, src->annotations.ann_custom);
  }
  else
  {
    dst->ann_custom = NULL;
  }
}

static void get_member_detail (DDS_XTypes_CompleteMemberDetail *dst, const struct xt_member_detail *src)
{
  ddsrt_strlcpy (dst->name, src->name, sizeof (dst->name));
  if (src->annotations.ann_builtin)
  {
    dst->ann_builtin = ddsrt_calloc (1, sizeof(struct DDS_XTypes_AppliedBuiltinMemberAnnotations));
    DDS_XTypes_AppliedBuiltinMemberAnnotations_copy (dst->ann_builtin, src->annotations.ann_builtin);
  }
  else
  {
    dst->ann_builtin = NULL;
  }

  if (src->annotations.ann_custom)
  {
    dst->ann_custom = ddsrt_calloc (1, sizeof(DDS_XTypes_AppliedAnnotationSeq));
    DDS_XTypes_AppliedAnnotationSeq_copy (dst->ann_custom, src->annotations.ann_custom);
  }
  else
  {
    dst->ann_custom = NULL;
  }
}

static void get_element_detail (DDS_XTypes_CompleteElementDetail *dst, const struct xt_applied_member_annotations *src)
{
  if (src->ann_builtin)
  {
    dst->ann_builtin = ddsrt_calloc (1, sizeof (*dst->ann_builtin));
    DDS_XTypes_AppliedBuiltinMemberAnnotations_copy (dst->ann_builtin, src->ann_builtin);
  }
  else
  {
    dst->ann_builtin = NULL;
  }

  if (src->ann_custom)
  {
    dst->ann_custom = ddsrt_calloc (1, sizeof (*dst->ann_custom));
    DDS_XTypes_AppliedAnnotationSeq_copy (dst->ann_custom, src->ann_custom);
  }
  else
  {
    dst->ann_custom = NULL;
  }
}

static void get_minimal_member_detail (DDS_XTypes_MinimalMemberDetail *dst, const struct xt_member_detail *src)
{
  memcpy (dst->name_hash, src->name_hash, sizeof (dst->name_hash));
}

static void xt_applied_annotation_seq_fini (DDS_XTypes_AppliedAnnotationSeq *ann_custom)
{
  if (ann_custom)
  {
    if (ann_custom->_release)
    {
      for (uint32_t n = 0; n < ann_custom->_length; n++)
      {
        ddsi_typeid_fini_impl (&ann_custom->_buffer[n].annotation_typeid);
        if (ann_custom->_buffer[n].param_seq)
        {
          if (ann_custom->_buffer[n].param_seq->_release)
            ddsrt_free (ann_custom->_buffer[n].param_seq->_buffer);
          ddsrt_free (ann_custom->_buffer[n].param_seq);
        }
      }
      ddsrt_free (ann_custom->_buffer);
    }
    ddsrt_free (ann_custom);
  }
}

static void xt_applied_type_annotations_fini (struct xt_applied_type_annotations *ann)
{
  if (ann->ann_builtin)
  {
    if (ann->ann_builtin->verbatim)
    {
      ddsrt_free (ann->ann_builtin->verbatim->text);
      ddsrt_free (ann->ann_builtin->verbatim);
    }
    ddsrt_free (ann->ann_builtin);
    ann->ann_builtin = NULL;
  }
  xt_applied_annotation_seq_fini (ann->ann_custom);
  ann->ann_custom = NULL;
}

static void xt_applied_member_annotations_fini (struct xt_applied_member_annotations *ann)
{
  if (ann->ann_builtin)
  {
    ddsrt_free (ann->ann_builtin->unit);
    ddsrt_free (ann->ann_builtin->min);
    ddsrt_free (ann->ann_builtin->max);
    ddsrt_free (ann->ann_builtin->hash_id);
    ddsrt_free (ann->ann_builtin);
    ann->ann_builtin = NULL;
  }
  xt_applied_annotation_seq_fini (ann->ann_custom);
  ann->ann_custom = NULL;
}

void ddsi_xt_type_fini_owned (struct ddsi_domaingv *gv, const struct ddsi_type *owner, struct xt_type *xt, bool include_typeid)
{
  switch (xt->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      ddsi_type_unref_dep_locked (gv, owner, xt->_u.alias.related_type);
      xt_applied_type_annotations_fini (&xt->_u.alias.detail.annotations);
      break;
    case DDS_XTypes_TK_ANNOTATION:
      abort (); /* FIXME: not implemented */
      break;
    case DDS_XTypes_TK_STRUCTURE:
      if (xt->_u.structure.base_type)
        ddsi_type_unref_dep_locked (gv, owner, xt->_u.structure.base_type);
      for (uint32_t n = 0; n < xt->_u.structure.members.length; n++)
      {
        ddsi_type_unref_dep_locked (gv, owner, xt->_u.structure.members.seq[n].type);
        xt_applied_member_annotations_fini (&xt->_u.structure.members.seq[n].detail.annotations);
      }
      ddsrt_free (xt->_u.structure.members.seq);
      xt_applied_type_annotations_fini (&xt->_u.structure.detail.annotations);
      break;
    case DDS_XTypes_TK_UNION:
      ddsi_type_unref_dep_locked (gv, owner, xt->_u.union_type.disc_type);
      xt_applied_type_annotations_fini (&xt->_u.union_type.disc_annotations);
      for (uint32_t n = 0; n < xt->_u.union_type.members.length; n++)
      {
        ddsi_type_unref_dep_locked (gv, owner, xt->_u.union_type.members.seq[n].type);
        ddsrt_free (xt->_u.union_type.members.seq[n].label_seq._buffer);
        xt_applied_member_annotations_fini (&xt->_u.union_type.members.seq[n].detail.annotations);
      }
      ddsrt_free (xt->_u.union_type.members.seq);
      xt_applied_type_annotations_fini (&xt->_u.union_type.detail.annotations);
      break;
    case DDS_XTypes_TK_BITSET:
      for (uint32_t n = 0; n < xt->_u.bitset.fields.length; n++)
        xt_applied_member_annotations_fini (&xt->_u.bitset.fields.seq[n].detail.annotations);
      ddsrt_free (xt->_u.bitset.fields.seq);
      xt_applied_type_annotations_fini (&xt->_u.bitset.detail.annotations);
      break;
    case DDS_XTypes_TK_SEQUENCE:
      ddsi_type_unref_dep_locked (gv, owner, xt->_u.seq.c.element_type);
      xt_applied_type_annotations_fini (&xt->_u.seq.c.detail.annotations);
      xt_applied_member_annotations_fini (&xt->_u.seq.c.element_annotations);
      break;
    case DDS_XTypes_TK_ARRAY:
      ddsi_type_unref_dep_locked (gv, owner, xt->_u.array.c.element_type);
      ddsrt_free (xt->_u.array.bounds._buffer);
      xt_applied_type_annotations_fini (&xt->_u.array.c.detail.annotations);
      xt_applied_member_annotations_fini (&xt->_u.array.c.element_annotations);
      break;
    case DDS_XTypes_TK_MAP:
      ddsi_type_unref_dep_locked (gv, owner, xt->_u.map.c.element_type);
      ddsi_type_unref_dep_locked (gv, owner, xt->_u.map.key_type);
      xt_applied_type_annotations_fini (&xt->_u.map.c.detail.annotations);
      xt_applied_member_annotations_fini (&xt->_u.map.c.element_annotations);
      xt_applied_member_annotations_fini (&xt->_u.map.key_annotations);
      break;
    case DDS_XTypes_TK_ENUM:
      for (uint32_t n = 0; n < xt->_u.enum_type.literals.length; n++)
        xt_applied_member_annotations_fini (&xt->_u.enum_type.literals.seq[n].detail.annotations);
      ddsrt_free (xt->_u.enum_type.literals.seq);
      xt_applied_type_annotations_fini (&xt->_u.enum_type.detail.annotations);
      break;
    case DDS_XTypes_TK_BITMASK:
      for (uint32_t n = 0; n < xt->_u.bitmask.bitflags.length; n++)
        xt_applied_member_annotations_fini (&xt->_u.bitmask.bitflags.seq[n].detail.annotations);
      ddsrt_free (xt->_u.bitmask.bitflags.seq);
      xt_applied_type_annotations_fini (&xt->_u.bitmask.detail.annotations);
      break;
    default:
      break;
  }
  xt->_d = DDS_XTypes_TK_NONE;
  if (include_typeid)
    ddsi_typeid_fini (&xt->id);
}

void ddsi_xt_type_fini (struct ddsi_domaingv *gv, struct xt_type *xt, bool include_typeid)
{
  ddsi_xt_type_fini_owned (gv, NULL, xt, include_typeid);
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
    ddsi_type_ref_locked (gv, &dst->member_type, src->member_type);
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
    ddsi_type_ref_locked (gv, &dst->element_type, src->element_type);
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
    ddsi_type_ref_locked (gv, &dst->type, src->type);
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
    ddsi_type_ref_locked (gv, &dst->type, src->type);
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

static void xt_enum_literal_copy (struct xt_enum_literal *dst, const struct xt_enum_literal *src)
{
  if (src)
  {
    dst->value = src->value;
    dst->flags = src->flags;
    xt_member_detail_copy (&dst->detail, &src->detail);
  }
}

static void xt_enum_literal_seq_copy (struct xt_enum_literal_seq *dst, const struct xt_enum_literal_seq *src)
{
  if (src)
  {
    dst->length = src->length;
    dst->seq = ddsrt_calloc (src->length, sizeof (*dst->seq));
    for (uint32_t n = 0; n < src->length; n++)
      xt_enum_literal_copy (&dst->seq[n], &src->seq[n]);
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

ddsrt_nonnull_all
void ddsi_xt_copy (struct ddsi_domaingv *gv, struct xt_type *dst, const struct xt_type *src)
{
  if (!ddsi_typeid_is_none (&src->id))
    ddsi_typeid_copy (&dst->id, &src->id);
  else
    dst->id.x._d = DDS_XTypes_TK_NONE;

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
      ddsi_type_ref_locked (gv, &dst->_u.map.key_type, src->_u.map.key_type);
      xt_applied_member_annotations_copy (&dst->_u.map.key_annotations, &src->_u.map.key_annotations);
      break;
    case DDS_XTypes_TK_ALIAS:
      dst->_u.alias.flags = src->_u.alias.flags;
      ddsi_type_ref_locked (gv, &dst->_u.alias.related_type, src->_u.alias.related_type);
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
        ddsi_type_ref_locked (gv, &dst->_u.structure.base_type, src->_u.structure.base_type);
      xt_struct_member_seq_copy (gv, &dst->_u.structure.members, &src->_u.structure.members);
      xt_type_detail_copy (&dst->_u.structure.detail, &src->_u.structure.detail);
      break;
    case DDS_XTypes_TK_UNION:
      dst->_u.union_type.flags = src->_u.union_type.flags;
      ddsi_type_ref_locked (gv, &dst->_u.union_type.disc_type, src->_u.union_type.disc_type);
      dst->_u.union_type.disc_flags = src->_u.union_type.disc_flags;
      xt_applied_type_annotations_copy (&dst->_u.union_type.disc_annotations, &src->_u.union_type.disc_annotations);
      xt_union_member_seq_copy (gv, &dst->_u.union_type.members, &src->_u.union_type.members);
      xt_type_detail_copy (&dst->_u.union_type.detail, &src->_u.union_type.detail);
      break;
    case DDS_XTypes_TK_BITSET:
      dst->_u.bitset.flags = src->_u.bitset.flags;
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
}

ddsrt_nonnull_all ddsrt_attribute_returns_nonnull
static struct xt_type * xt_dup (struct ddsi_domaingv *gv, const struct xt_type *src)
{
  struct xt_type *dst = ddsrt_calloc (1, sizeof (*dst));
  ddsi_xt_copy (gv, dst, src);
  return dst;
}

ddsrt_nonnull_all
static bool xt_has_basetype (const struct xt_type *t)
{
  assert (t->_d == DDS_XTypes_TK_STRUCTURE);
  return t->_u.structure.base_type != NULL;
}

ddsrt_nonnull ((1, 3))
static bool xt_non_assignable (struct ddsi_non_assignability_reason *reason, enum ddsi_non_assignability_code code, const struct xt_type *t1, const struct xt_type *t2, uint32_t id)
{
  reason->code = code;
  reason->id = id;
  reason->t1_id = t1->id.x;
  reason->t1_typekind = t1->_d;
  if (t2)
  {
    reason->t2_id = t2->id.x;
    reason->t2_typekind = t2->_d;
  }
  else
  {
    reason->t2_id._d = DDS_XTypes_TK_NONE;
    reason->t2_typekind = DDS_XTypes_TK_NONE;
  }
  return false;
}

ddsrt_nonnull ((1, 2))
static bool xt_is_assignable_check_has_definition (const struct xt_type *t, struct ddsi_non_assignability_reason *reason, const struct xt_type *referrer_type, uint32_t id)
{
  assert (referrer_type == NULL || ddsi_xt_has_definition (referrer_type));
  if (ddsi_xt_has_definition (t))
    return true;
  if (referrer_type)
    return xt_non_assignable (reason, DDSI_NONASSIGN_TYPE_UNRESOLVED, referrer_type, t, id);
  else
    return xt_non_assignable (reason, DDSI_NONASSIGN_TYPE_UNRESOLVED, t, NULL, id);
}

ddsrt_nonnull_all
static struct xt_type *xt_expand_basetype (struct ddsi_domaingv *gv, const struct xt_type *t, struct ddsi_non_assignability_reason *reason)
{
  assert (t->_d == DDS_XTypes_TK_STRUCTURE);
  if (!xt_has_basetype (t))
    return xt_dup (gv, t);
  else
  {
    const struct xt_type * const b = ddsi_xt_unalias (&t->_u.structure.base_type->xt);
    if (!xt_is_assignable_check_has_definition (b, reason, t, 0))
      return NULL;
    struct xt_type * const te = xt_expand_basetype (gv, b, reason);
    if (!te)
      return NULL;

    const uint32_t incr = t->_u.structure.members.length;
    struct xt_struct_member_seq * const ms = &te->_u.structure.members;
    ms->seq = ddsrt_realloc (ms->seq, (ms->length + incr) * sizeof (*ms->seq));
    for (uint32_t i = 0; i < incr; i++)
      xt_struct_member_copy (gv, &ms->seq[ms->length++], &t->_u.structure.members.seq[i]);
    return te;
  }
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
          tkh->_u.structure.members.seq[i].flags |= DDS_XTypes_IS_KEY;
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
      /* KeyHolder is also applied to union case member types selected by a
         discriminator value. Those member types can be non-aggregate types, for
         which KeyHolder(T) is just T. */
      return tkh;
  }
}

static bool xt_is_plain_collection (const struct xt_type *t)
{
  DDS_XTypes_CollectionElementFlag plain_flags = DDS_XTypes_TRY_CONSTRUCT1 | DDS_XTypes_TRY_CONSTRUCT2 | DDS_XTypes_IS_EXTERNAL;
  return ((t->_d == DDS_XTypes_TK_SEQUENCE && !(t->_u.seq.c.element_flags & ~plain_flags))
      || (t->_d == DDS_XTypes_TK_ARRAY && !(t->_u.array.c.element_flags & ~plain_flags))
      || (t->_d == DDS_XTypes_TK_MAP && !(t->_u.map.c.element_flags & ~plain_flags)));
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

struct xt_delimited_path {
  const struct xt_type *type;
  const struct xt_delimited_path *parent;
};

static bool xt_is_delimited_impl (struct ddsi_domaingv *gv, const struct xt_type *t, const struct xt_delimited_path *path)
{
  if (xt_is_primitive (t) || xt_is_string (t) || xt_is_enumerated (t))
    return true;
  for (const struct xt_delimited_path *p = path; p; p = p->parent)
  {
    if (p->type == t)
    {
      /* Validation rejects collection-only cycles. If one still reaches
         assignability, treat it as not delimited rather than recursing. */
      assert (false);
      return false;
    }
  }

  const struct xt_delimited_path t_path = { .type = t, .parent = path };
  switch (t->_d)
  {
    case DDS_XTypes_TK_SEQUENCE:
      return xt_is_delimited_impl (gv, ddsi_xt_unalias (&t->_u.seq.c.element_type->xt), &t_path);
    case DDS_XTypes_TK_ARRAY:
      return xt_is_delimited_impl (gv, ddsi_xt_unalias (&t->_u.array.c.element_type->xt), &t_path);
    case DDS_XTypes_TK_MAP:
      return xt_is_delimited_impl (gv, ddsi_xt_unalias (&t->_u.map.key_type->xt), &t_path)
        && xt_is_delimited_impl (gv, ddsi_xt_unalias (&t->_u.map.c.element_type->xt), &t_path);
  }
  uint32_t ext = xt_get_extensibility (t);
  if (ext == DDS_XTypes_IS_APPENDABLE) /* FIXME: && encoding == XCDR2 */
    return true;
  return ext == DDS_XTypes_IS_MUTABLE;
}

static bool xt_is_delimited (struct ddsi_domaingv *gv, const struct xt_type *t)
{
  return xt_is_delimited_impl (gv, t, NULL);
}

static bool xt_is_equivalent_minimal (const struct xt_type *t1, const struct xt_type *t2, bool is_assignability_check)
{
  // Minimal equivalence relation (XTypes spec v1.3 section 7.3.4.7)
  if (xt_is_fully_descriptive (t1) || xt_is_minimal_hash_typeid (t1))
  {
    if (!ddsi_typeid_compare_acflag (&t1->id.x, &t2->id.x, is_assignability_check))
      return true;
  }
  return false;
}

static uint32_t xt_assignability_node_hash (const void *vnode)
{
  const struct xt_assignability_node *node = vnode;
  const uintptr_t rd = (uintptr_t) node->rd_type;
  const uintptr_t wr = (uintptr_t) node->wr_type;
  return ddsrt_mh3 (&wr, sizeof (wr), ddsrt_mh3 (&rd, sizeof (rd), 0));
}

static bool xt_assignability_node_equal (const void *vnode_a, const void *vnode_b)
{
  const struct xt_assignability_node *a = vnode_a;
  const struct xt_assignability_node *b = vnode_b;
  return a->rd_type == b->rd_type && a->wr_type == b->wr_type;
}

static void xt_assignability_node_free (void *vnode, void *arg)
{
  (void) arg;
  ddsrt_free (vnode);
}

static dds_return_t xt_assignability_context_init (struct xt_assignability_context *context)
{
  if ((context->type_pairs = ddsrt_hh_new (1, xt_assignability_node_hash, xt_assignability_node_equal)) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  return DDS_RETCODE_OK;
}

static void xt_assignability_context_fini (struct xt_assignability_context *context)
{
  ddsrt_hh_enum (context->type_pairs, xt_assignability_node_free, NULL);
  ddsrt_hh_free (context->type_pairs);
}

static bool xt_assignability_seen (struct xt_assignability_context *context, const struct xt_type *rd_type, const struct xt_type *wr_type)
{
  const struct xt_assignability_node templ = { .rd_type = rd_type, .wr_type = wr_type };
  return ddsrt_hh_lookup (context->type_pairs, &templ) != NULL;
}

static dds_return_t xt_assignability_enter (struct xt_assignability_context *context, const struct xt_type *rd_type, const struct xt_type *wr_type, struct xt_assignability_node **node)
{
  assert (!xt_assignability_seen (context, rd_type, wr_type));

  if ((*node = ddsrt_calloc (1, sizeof (**node))) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  (*node)->rd_type = rd_type;
  (*node)->wr_type = wr_type;
  ddsrt_hh_add_absent (context->type_pairs, *node);
  return DDS_RETCODE_OK;
}

static void xt_assignability_leave (struct xt_assignability_context *context, struct xt_assignability_node *node)
{
  ddsrt_hh_remove_present (context->type_pairs, node);
  ddsrt_free (node);
}

ddsrt_nonnull_all
static bool xt_is_assignable_from_impl (struct ddsi_domaingv *gv, struct xt_assignability_context *context, const struct xt_type *rd_xt, const struct xt_type *wr_xt, const dds_type_consistency_enforcement_qospolicy_t *tce, struct ddsi_non_assignability_reason *reason);

ddsrt_nonnull_all
static bool xt_is_strongly_assignable_from (struct ddsi_domaingv *gv, struct xt_assignability_context *context, const struct xt_type *t1a, const struct xt_type *t2a, const dds_type_consistency_enforcement_qospolicy_t *tce, struct ddsi_non_assignability_reason *reason)
{
  const struct xt_type *t1 = ddsi_xt_unalias (t1a), *t2 = ddsi_xt_unalias (t2a);
  if (xt_is_equivalent_minimal (t1, t2, true) || xt_assignability_seen (context, t1, t2))
    return true;
  if (xt_is_delimited (gv, t2))
    return xt_is_assignable_from_impl (gv, context, t1, t2, tce, reason);
  return xt_non_assignable (reason, DDSI_NONASSIGN_WR_TYPE_NOT_DELIMITED, t1, t2, 0);
}

static bool xt_bounds_eq (const struct DDS_XTypes_LBoundSeq *a, const struct DDS_XTypes_LBoundSeq *b)
{
  if (!a || !b)
    return false;
  if (a->_length != b->_length)
    return false;
  return !memcmp (a->_buffer, b->_buffer, a->_length * sizeof (*a->_buffer));
}

ddsrt_nonnull_all
static bool xt_namehash_eq (const DDS_XTypes_NameHash *n1, const DDS_XTypes_NameHash *n2)
{
  return !memcmp (n1, n2, sizeof (*n1));
}

ddsrt_nonnull_all
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

ddsrt_nonnull_all
static bool xt_union_labels_match (const struct DDS_XTypes_UnionCaseLabelSeq *ls1, const struct DDS_XTypes_UnionCaseLabelSeq *ls2)
{
  /* UnionCaseLabelSeq is ordered by value (as noted in typeobject idl) */
  for (uint32_t i = 0; i < ls1->_length; i++)
    if (i >= ls2->_length || ls1->_buffer[i] != ls2->_buffer[i])
      return false;
  return true;
}

ddsrt_nonnull_all
static bool xt_is_assignable_from_enum (const struct xt_type *t1, const struct xt_type *t2, struct ddsi_non_assignability_reason *reason)
{
  assert (t1->_d == DDS_XTypes_TK_ENUM);
  assert (t2->_d == DDS_XTypes_TK_ENUM);
  // Note: extensibility flags not defined, see https://issues.omg.org/issues/DDSXTY14-24
  if (xt_get_extensibility (t1) != xt_get_extensibility (t2))
    return xt_non_assignable (reason, DDSI_NONASSIGN_DIFFERENT_EXTENSIBILITY, t1, t2, 0);
  /* Members are ordered by increasing value (XTypes 1.3 spec 7.3.4.5) */
  uint32_t i1 = 0, i2 = 0, i1_max = t1->_u.enum_type.literals.length, i2_max = t2->_u.enum_type.literals.length;
  while (i1 < i1_max && i2 < i2_max)
  {
    struct xt_enum_literal *l1 = &t1->_u.enum_type.literals.seq[i1], *l2 = &t2->_u.enum_type.literals.seq[i2];
    if (l1->value == l2->value)
    {
      /* FIXME: implement @ignore_literal_names */
      if (!xt_namehash_eq (&l1->detail.name_hash, &l2->detail.name_hash))
        return xt_non_assignable (reason, DDSI_NONASSIGN_NAME_HASH_DIFFERS, t1, t2, (uint32_t) l1->value);
      i1++;
      i2++;
    }
    else if (xt_get_extensibility (t1) == DDS_XTypes_IS_FINAL)
      return xt_non_assignable (reason, DDSI_NONASSIGN_MISSING_CASE, t1, t2, (uint32_t) l1->value);
    else if (l1->value < l2->value)
      i1++;
    else
      i2++;
  }
  if ((i1 != i1_max || i2 != i2_max) && xt_get_extensibility (t1) == DDS_XTypes_IS_FINAL)
    return xt_non_assignable (reason, DDSI_NONASSIGN_NUMBER_OF_MEMBERS, t1, t2, 0);
  return true;
}

ddsrt_nonnull_all
static bool xt_is_assignable_from_union (struct ddsi_domaingv *gv, struct xt_assignability_context *context, const struct xt_type *t1, const struct xt_type *t2, const dds_type_consistency_enforcement_qospolicy_t *tce, struct ddsi_non_assignability_reason *reason)
{
  /* Note: T1 is the type of the writer, T2 is the type of the writer. These impractical names
     are used because this is reader the definition of assignability in the specification. */
  assert (t1->_d == DDS_XTypes_TK_UNION);
  assert (t2->_d == DDS_XTypes_TK_UNION);
  if (xt_get_extensibility (t1) != xt_get_extensibility (t2))
    return xt_non_assignable (reason, DDSI_NONASSIGN_DIFFERENT_EXTENSIBILITY, t1, t2, 0);
  if (!xt_is_strongly_assignable_from (gv, context, ddsi_xt_unalias (&t1->_u.union_type.disc_type->xt), ddsi_xt_unalias (&t2->_u.union_type.disc_type->xt), tce, reason))
    return false;

  /* Rule: Either the discriminators of both T1 and T2 are keys or neither are keys. */
  if ((t1->_u.union_type.disc_flags & DDS_XTypes_IS_KEY) != (t2->_u.union_type.disc_flags & DDS_XTypes_IS_KEY))
    return xt_non_assignable (reason, DDSI_NONASSIGN_KEY_DIFFERS, t1, t2, 0);

  /* Note that union members are ordered by their member index (=ordering in idl) and not by their member ID */
  uint32_t i1_max = t1->_u.union_type.members.length, i2_max = t2->_u.union_type.members.length;
  /* Rule: If T1 (and therefore T2) extensibility is final then the set of labels is identical. By marking it
     non-assignable early if the number of labels is different, the remainder only needs to check that the
     cases in T1 exist in T2. */
  if (i1_max != i2_max && xt_get_extensibility (t1) == DDS_XTypes_IS_FINAL)
    return xt_non_assignable (reason, DDSI_NONASSIGN_MISSING_CASE, t1, t2, 0);
  bool any_match = false;
  for (uint32_t i1 = 0; i1 < i1_max; i1++)
  {
    struct xt_union_member *m1 = &t1->_u.union_type.members.seq[i1];
    const struct xt_type *m1t = ddsi_xt_unalias (&m1->type->xt);
    bool m2_id_match = false, m2_labels_match = true, t1_selects_t2_member = false;
    struct xt_union_member *def_m2 = NULL;
    /* Rule: Any members in T1 and T2 that have the same name also have the same ID and any members
       with the same ID also have the same name. */
    if (!tce->ignore_member_names)
    {
      for (uint32_t i2 = i1; i2 < i2_max + i1; i2++)
      {
        struct xt_union_member *m2 = &t2->_u.union_type.members.seq[i2 % i2_max];
        if ((m1->id == m2->id) != xt_namehash_eq (&m1->detail.name_hash, &m2->detail.name_hash))
        {
          enum ddsi_non_assignability_code code;
          code = (m1->id == m2->id) ? DDSI_NONASSIGN_NAME_HASH_DIFFERS : DDSI_NONASSIGN_MEMBER_ID_DIFFERS;
          return xt_non_assignable (reason, code, t1, t2, m1->id);
        }
      }
    }
    for (uint32_t i2 = i1; i2 < i2_max + i1; i2++)
    {
      struct xt_union_member *m2 = &t2->_u.union_type.members.seq[i2 % i2_max];
      const struct xt_type *m2t = ddsi_xt_unalias (&m2->type->xt);
      if (m1->id == m2->id)
        m2_id_match = true;

      /* Rule: If T1 and T2 both have default labels, the type associated with T1 default member is assignable from
          the type associated with T2 default member. */
      if ((m1->flags & DDS_XTypes_IS_DEFAULT) && (m2->flags & DDS_XTypes_IS_DEFAULT))
      {
        if (!xt_is_assignable_from_impl (gv, context, m1t, m2t, tce, reason))
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
      if (!xt_is_assignable_from_impl (gv, context, m1t, ddsi_xt_unalias (&def_m2->type->xt), tce, reason))
        return false;
    }

    /* Rule: If T1 (and therefore T2) extensibility is final or prevent type widening is set then the
       set of labels is identical. */
    if ((xt_get_extensibility (t1) == DDS_XTypes_IS_FINAL || tce->prevent_type_widening) && (!m2_id_match || !m2_labels_match))
      return xt_non_assignable (reason, DDSI_NONASSIGN_MISSING_CASE, t1, t2, 0);
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
    if ((sel_m1 || def_m1) && !xt_is_assignable_from_impl (gv, context, ddsi_xt_unalias (sel_m1 ? &sel_m1->type->xt : &def_m1->type->xt), ddsi_xt_unalias(&m2->type->xt), tce, reason))
      return false;
    if (!sel_m1 && tce->prevent_type_widening)
      return xt_non_assignable (reason, DDSI_NONASSIGN_MISSING_CASE, t1, t2, m2->id);
  }

  /* Rule: [extensibility is final], otherwise, they have at least one common label other than the default label. */
  if (!any_match)
    return xt_non_assignable (reason, DDSI_NONASSIGN_NO_OVERLAP, t1, t2, 0);
  return true;
}

ddsrt_nonnull_all
static bool xt_is_assignable_from_struct (struct ddsi_domaingv *gv, struct xt_assignability_context *context, const struct xt_type *t1, const struct xt_type *t2, const dds_type_consistency_enforcement_qospolicy_t *tce, struct ddsi_non_assignability_reason *reason)
{
  assert (t1->_d == DDS_XTypes_TK_STRUCTURE);
  assert (t2->_d == DDS_XTypes_TK_STRUCTURE);
  bool result = false;
  struct xt_type *te1 = (struct xt_type *) t1, *te2 = (struct xt_type *) t2;
  if (xt_get_extensibility (t1) != xt_get_extensibility (t2)) {
    xt_non_assignable (reason, DDSI_NONASSIGN_DIFFERENT_EXTENSIBILITY, t1, t2, 0);
    goto struct_failed;
  }
  if (xt_has_basetype (t1))
    if ((te1 = xt_expand_basetype (gv, t1, reason)) == NULL)
      goto struct_failed;
  if (xt_has_basetype (t2))
    if ((te2 = xt_expand_basetype (gv, t2, reason)) == NULL)
      goto struct_failed;
  /* Note that struct members are ordered by their member index (=ordering in idl) and not by their member ID (although the
      TypeObject idl states that its ordered by member_id...) */
  uint32_t i1_max = te1->_u.structure.members.length, i2_max = te2->_u.structure.members.length;
  bool any_member_match = false;

  /* Pre-processing: check member type definitions and count the number of keys. */
  uint32_t t1_nkeys = 0, t2_nkeys = 0;
  for (uint32_t i1 = 0; i1 < i1_max; i1++)
  {
    const struct xt_struct_member *m1 = &te1->_u.structure.members.seq[i1];
    const struct xt_type *m1t = ddsi_xt_unalias (&m1->type->xt);
    if (!xt_is_assignable_check_has_definition (m1t, reason, t1, m1->id))
      goto struct_failed;
    if (m1->flags & DDS_XTypes_IS_KEY)
      t1_nkeys++;
  }
  for (uint32_t i2 = 0; i2 < i2_max; i2++)
  {
    const struct xt_struct_member *m2 = &te2->_u.structure.members.seq[i2];
    const struct xt_type *m2t = ddsi_xt_unalias (&m2->type->xt);
    if (!xt_is_assignable_check_has_definition (m2t, reason, t2, m2->id))
      goto struct_failed;
    if (m2->flags & DDS_XTypes_IS_KEY)
      t2_nkeys++;
  }
  /* Pre-process for rule: "Members marked as key in either T1 or T2 appear (i.e., have a
     corresponding member of the same member ID) in both T1 and T2." If the number of keys
     differs, this is trivially false. If the number of keys is the same, we only need to
     check that we found a match for each key in T1. */
  if (t1_nkeys != t2_nkeys)
  {
    xt_non_assignable (reason, DDSI_NONASSIGN_KEY_DIFFERS, t1, t2, 0);
    goto struct_failed;
  }
  /* Implement assignability rules */
  for (uint32_t i1 = 0; i1 < i1_max; i1++)
  {
    const struct xt_struct_member *m1 = &te1->_u.structure.members.seq[i1];
    const struct xt_type *m1t = ddsi_xt_unalias (&m1->type->xt);

    bool match = false,
      m1_opt = (m1->flags & DDS_XTypes_IS_OPTIONAL),
      m1_mu = (m1->flags & DDS_XTypes_IS_MUST_UNDERSTAND),
      m1_k = (m1->flags & DDS_XTypes_IS_KEY);

    /* Rule: Any members in T1 and T2 that have the same name also have the same ID and any members
       with the same ID also have the same name. */
    if (!tce->ignore_member_names)
    {
      for (uint32_t i2 = i1; i2 < i2_max + i1; i2++)
      {
        struct xt_struct_member *m2 = &te2->_u.structure.members.seq[i2 % i2_max];
        if ((m1->id == m2->id) != xt_namehash_eq (&m1->detail.name_hash, &m2->detail.name_hash))
        {
          enum ddsi_non_assignability_code code;
          code = (m1->id == m2->id) ? DDSI_NONASSIGN_NAME_HASH_DIFFERS : DDSI_NONASSIGN_MEMBER_ID_DIFFERS;
          xt_non_assignable (reason, code, t1, t2, m1->id);
          goto struct_failed;
        }
      }
    }
    for (uint32_t i2 = i1; i2 < i2_max + i1; i2++)
    {
      struct xt_struct_member *m2 = &te2->_u.structure.members.seq[i2 % i2_max];
      if (m1->id == m2->id)
      {
        bool m2_k = (m2->flags & DDS_XTypes_IS_KEY);
        any_member_match = true;
        match = true;
        const struct xt_type *m2t = ddsi_xt_unalias (&m2->type->xt);

        /* Rule: "For any member m2 in T2, if there is a member m1 in T1 with the same member ID, then the type
            KeyErased(m1.type) is-assignable from the type KeyErased(m2.type) */
        struct xt_type *m1_ke = xt_type_key_erased (gv, m1t),
          *m2_ke = xt_type_key_erased (gv, m2t);
        bool ke_assignable = xt_is_assignable_from_impl (gv, context, m1_ke, m2_ke, tce, reason);
        ddsi_xt_type_fini (gv, m1_ke, true);
        ddsrt_free (m1_ke);
        ddsi_xt_type_fini (gv, m2_ke, true);
        ddsrt_free (m2_ke);
        if (!ke_assignable)
          goto struct_failed;

        /* Rule: "For any string key member m2 in T2, the m1 member of T1 with the same member ID verifies m1.type.length >= m2.type.length. */
        if (m2_k && xt_is_string (m2t) && !xt_check_bound (xt_string_bound (m1t), xt_string_bound (m2t)))
        {
          xt_non_assignable (reason, DDSI_NONASSIGN_KEY_INCOMPATIBLE, m1t, m2t, m1->id);
          goto struct_failed;
        }
        /* Rule: "For any enumerated key member m2 in T2, the m1 member of T1 with the same member ID verifies that all
            literals in m2.type appear as literals in m1.type" */
        if (m2_k && xt_is_enumerated (m2t))
        {
          uint32_t ki1 = 0, ki2 = 0, ki1_max = m1t->_u.enum_type.literals.length, ki2_max = m2t->_u.enum_type.literals.length;
          while (ki1 < ki1_max && ki2 < ki2_max)
          {
            struct xt_enum_literal *kl1 = &m1t->_u.enum_type.literals.seq[ki1], *kl2 = &m2t->_u.enum_type.literals.seq[ki2];
            if (kl1->value == kl2->value) {
              ki1++;
              ki2++;
            } else if (kl1->value < kl2->value) {
              ki1++;
            } else {
              xt_non_assignable (reason, DDSI_NONASSIGN_KEY_INCOMPATIBLE, m1t, m2t, m1->id);
              goto struct_failed;
            }
          }
        }

        /* Rule: "For any sequence or map key member m2 in T2, the m1 member of T1 with the same member ID verifies m1.type.length >= m2.type.length" */
        if (m2_k && m2t->_d == DDS_XTypes_TK_SEQUENCE && !xt_check_bound (m1t->_u.seq.bound, m2t->_u.seq.bound))
        {
          xt_non_assignable (reason, DDSI_NONASSIGN_KEY_INCOMPATIBLE, m1t, m2t, m1->id);
          goto struct_failed;
        }
        if (m2_k && m2t->_d == DDS_XTypes_TK_MAP && !xt_check_bound (m1t->_u.map.bound, m2t->_u.map.bound))
        {
          xt_non_assignable (reason, DDSI_NONASSIGN_KEY_INCOMPATIBLE, m1t, m2t, m1->id);
          goto struct_failed;
        }
        /* Rule: "For any structure or union key member m2 in T2, the m1 member of T1 with the same member ID verifies that KeyHolder(m1.type)
            isassignable-from KeyHolder(m2.type)." */
        if (m2_k && (m2t->_d == DDS_XTypes_TK_STRUCTURE || m2t->_d == DDS_XTypes_TK_UNION))
        {
          struct xt_type *m1_kh = xt_type_keyholder (gv, m1t),
            *m2_kh = xt_type_keyholder (gv, m2t);
          bool kh_assignable = xt_is_assignable_from_impl (gv, context, m1_kh, m2_kh, tce, reason);
          ddsi_xt_type_fini (gv, m1_kh, true);
          ddsrt_free (m1_kh);
          ddsi_xt_type_fini (gv, m2_kh, true);
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
            struct xt_union_member *km1 = &m1t->_u.union_type.members.seq[ki1];
            for (uint32_t ki2 = ki1; ki2 < ki2_max + ki1; ki2++)
            {
              struct xt_union_member *km2 = &m2t->_u.union_type.members.seq[ki2 % ki2_max];
              if (xt_union_label_selects (&km1->label_seq, &km2->label_seq))
              {
                const struct xt_type *km1_t = ddsi_xt_unalias (&km1->type->xt),
                  *km2_t = ddsi_xt_unalias (&km2->type->xt);
                if (!xt_is_assignable_check_has_definition (km1_t, reason, t1, m1->id) ||
                    !xt_is_assignable_check_has_definition (km2_t, reason, t2, m2->id))
                  goto struct_failed;
                struct xt_type *km1_kh = xt_type_keyholder (gv, km1_t),
                  *km2_kh = xt_type_keyholder (gv, km2_t);
                bool kh_assignable = xt_is_assignable_from_impl (gv, context, km1_kh, km2_kh, tce, reason);
                ddsi_xt_type_fini (gv, km1_kh, true);
                ddsrt_free (km1_kh);
                ddsi_xt_type_fini (gv, km2_kh, true);
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
    {
      xt_non_assignable (reason, DDSI_NONASSIGN_STRUCT_MUST_UNDERSTAND, t1, m1t, m1->id);
      goto struct_failed;
    }
    /* Rule (for T1 members): "Members marked as key in either T1 or T2 appear (i.e., have a corresponding member of the same member ID)
        in both T1 and T2." */
    if (m1_k && !match)
    {
      xt_non_assignable (reason, DDSI_NONASSIGN_KEY_DIFFERS, t1, t2, 0);
      goto struct_failed;
    }
    /* Rules:
        - if T1 is appendable, then members with the same member_index have the same member ID, the same setting for the
          optional attribute and the T1 member type is strongly assignable from the T2 member type
        - if T1 is final, then they meet the same condition as for T1 being appendable and ... (see below) */
    struct xt_struct_member *m2 = (i1 < i2_max) ? &te2->_u.structure.members.seq[i1] : NULL;
    if ((xt_get_extensibility (te1) == DDS_XTypes_IS_APPENDABLE && i1 < i2_max) || xt_get_extensibility (te1) == DDS_XTypes_IS_FINAL)
    {
      if (m2 == NULL) {
        xt_non_assignable (reason, DDSI_NONASSIGN_NUMBER_OF_MEMBERS, t1, t2, 0);
        goto struct_failed;
      } else if (m1->id != m2->id) {
        xt_non_assignable (reason, DDSI_NONASSIGN_STRUCT_MEMBER_MISMATCH, t1, t2, m1->id);
        goto struct_failed;
      } else if ((m1->flags & DDS_XTypes_IS_OPTIONAL) != (m2->flags & DDS_XTypes_IS_OPTIONAL)) {
        xt_non_assignable (reason, DDSI_NONASSIGN_STRUCT_OPTIONAL, t1, t2, m1->id);
        goto struct_failed;
      } else if (!xt_is_strongly_assignable_from (gv, context, m1t, ddsi_xt_unalias (&m2->type->xt), tce, reason)) {
        goto struct_failed;
      }
    }
    /* if T1 is final, or prevent type-widening is set: ... [continued] in addition T1 and T2 have the same set of member IDs */
    if ((xt_get_extensibility (te1) == DDS_XTypes_IS_FINAL || (tce->prevent_type_widening && !(m1->flags & DDS_XTypes_IS_OPTIONAL))) && !match)
    {
      xt_non_assignable (reason, DDSI_NONASSIGN_NUMBER_OF_MEMBERS, t1, t2, m1->id);
      goto struct_failed;
    }
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
        || xt_get_extensibility (te1) == DDS_XTypes_IS_FINAL)
    {
      for (uint32_t i1 = i2; !match && i1 < i1_max + i2; i1++)
        match = (te1->_u.structure.members.seq[i1 % i1_max].id == m2->id);
      if (!match)
      {
        xt_non_assignable (reason, DDSI_NONASSIGN_STRUCT_MEMBER_MISMATCH, t1, t2, m2->id);
        goto struct_failed;
      }
    }
  }
  /* Rule: There is at least one member m1 of T1 and one corresponding member m2 of T2 such that m1.id == m2.id */
  if (!any_member_match)
  {
    xt_non_assignable (reason, DDSI_NONASSIGN_NO_OVERLAP, t1, t2, 0);
    goto struct_failed;
  }
  result = true;

struct_failed:
  if (te1 && te1 != t1)
  {
    ddsi_xt_type_fini (gv, te1, true);
    ddsrt_free (te1);
  }
  if (te2 && te2 != t2)
  {
    ddsi_xt_type_fini (gv, te2, true);
    ddsrt_free (te2);
  }
  return result;
}

ddsrt_nonnull_all
static bool xt_is_assignable_from_bitmask (const struct xt_type *t1, const struct xt_type *t2, struct ddsi_non_assignability_reason *reason)
{
  const struct xt_type *t_bm = t1->_d == DDS_XTypes_TK_BITMASK ? t1 : t2;
  const struct xt_type *t_other = t1->_d == DDS_XTypes_TK_BITMASK ? t2 : t1;
  DDS_XTypes_BitBound bb = t_bm->_u.bitmask.bit_bound;
  enum ddsi_non_assignability_code code = DDSI_NONASSIGN_BOUND;
  switch (t_other->_d)
  {
    case DDS_XTypes_TK_BITMASK:
      if (bb == t_other->_u.bitmask.bit_bound)
        return true;
      break;
    case DDS_XTypes_TK_UINT8:
      if (bb >= 1 && bb <= 8)
        return true;
      break;
    case DDS_XTypes_TK_UINT16:
      if (bb >= 9 && bb <= 16)
        return true;
      break;
    case DDS_XTypes_TK_UINT32:
      if (bb >= 17 && bb <= 32)
        return true;
      break;
    case DDS_XTypes_TK_UINT64:
      if (bb >= 33 && bb <= 64)
        return true;
      break;
    default:
      code = DDSI_NONASSIGN_INCOMPATIBLE_TYPE;
      break;
  }
  return xt_non_assignable (reason, code, t1, t2, 0);
}

static bool xt_is_assignable_from_impl_body (struct ddsi_domaingv *gv, struct xt_assignability_context *context, const struct xt_type *t1, const struct xt_type *t2, const dds_type_consistency_enforcement_qospolicy_t *tce, struct ddsi_non_assignability_reason *reason)
{
  /* Bitmask type: must be equal, except bitmask can be assigned to uint types and vv */
  if (t1->_d == DDS_XTypes_TK_BITMASK || t2->_d == DDS_XTypes_TK_BITMASK)
    return xt_is_assignable_from_bitmask (t1, t2, reason);

  /* Enum type */
  if (t1->_d == DDS_XTypes_TK_ENUM && t2->_d == DDS_XTypes_TK_ENUM)
    return xt_is_assignable_from_enum (t1, t2, reason);

  /* String types: character type must be assignable, bound not checked for assignability, unless ignore_string_bounds is false */
  if ((t1->_d == DDS_XTypes_TK_STRING8 && t2->_d == DDS_XTypes_TK_STRING8)) {
    if (tce->ignore_string_bounds || xt_check_bound (t1->_u.str8.bound, t2->_u.str8.bound))
      return true;
    return xt_non_assignable (reason, DDSI_NONASSIGN_BOUND, t1, t2, 0);
  }
  if ((t1->_d == DDS_XTypes_TK_STRING16 && t2->_d == DDS_XTypes_TK_STRING16)) {
    if (tce->ignore_string_bounds || xt_check_bound (t1->_u.str16.bound, t2->_u.str16.bound))
      return true;
    return xt_non_assignable (reason, DDSI_NONASSIGN_BOUND, t1, t2, 0);
  }

  /* Collection types */
  if (t1->_d == DDS_XTypes_TK_ARRAY && t2->_d == DDS_XTypes_TK_ARRAY) {
    if (xt_bounds_eq (&t1->_u.array.bounds, &t2->_u.array.bounds))
      return xt_is_strongly_assignable_from (gv, context, &t1->_u.array.c.element_type->xt, &t2->_u.array.c.element_type->xt, tce, reason);
    return xt_non_assignable (reason, DDSI_NONASSIGN_BOUND, t1, t2, 0);
  }
  if (t1->_d == DDS_XTypes_TK_SEQUENCE && t2->_d == DDS_XTypes_TK_SEQUENCE) {
    if (tce->ignore_sequence_bounds || xt_check_bound (t1->_u.seq.bound, t2->_u.seq.bound))
      return xt_is_strongly_assignable_from (gv, context, &t1->_u.seq.c.element_type->xt, &t2->_u.seq.c.element_type->xt, tce, reason);
    return xt_non_assignable (reason, DDSI_NONASSIGN_BOUND, t1, t2, 0);
  }
  if (t1->_d == DDS_XTypes_TK_MAP && t2->_d == DDS_XTypes_TK_MAP) {
    return xt_is_strongly_assignable_from (gv, context, &t1->_u.map.key_type->xt, &t2->_u.map.key_type->xt, tce, reason)
      && xt_is_strongly_assignable_from (gv, context, &t1->_u.map.c.element_type->xt, &t2->_u.map.c.element_type->xt, tce, reason);
  }

  // Aggregated types
  if (t1->_d == DDS_XTypes_TK_UNION && t2->_d == DDS_XTypes_TK_UNION)
    return xt_is_assignable_from_union (gv, context, t1, t2, tce, reason);
  if (t1->_d == DDS_XTypes_TK_STRUCTURE && t2->_d == DDS_XTypes_TK_STRUCTURE)
    return xt_is_assignable_from_struct (gv, context, t1, t2, tce, reason);

  return xt_non_assignable (reason, DDSI_NONASSIGN_INCOMPATIBLE_TYPE, t1, t2, 0);
}

static bool xt_is_assignable_from_impl (struct ddsi_domaingv *gv, struct xt_assignability_context *context, const struct xt_type *rd_xt, const struct xt_type *wr_xt, const dds_type_consistency_enforcement_qospolicy_t *tce, struct ddsi_non_assignability_reason *reason)
{
  const struct xt_type *t1 = ddsi_xt_unalias (rd_xt), *t2 = ddsi_xt_unalias (wr_xt);
  struct xt_assignability_node *node;
  if (!xt_is_assignable_check_has_definition (t1, reason, NULL, 0) || !xt_is_assignable_check_has_definition (t2, reason, NULL, 0))
    return false;

  if (xt_is_equivalent_minimal (t1, t2, true) || xt_assignability_seen (context, t1, t2))
    return true;
  if (xt_assignability_enter (context, t1, t2, &node) != DDS_RETCODE_OK)
    return xt_non_assignable (reason, DDSI_NONASSIGN_UNKNOWN, t1, t2, 0);

  const bool assignable = xt_is_assignable_from_impl_body (gv, context, t1, t2, tce, reason);
  /* The context is a recursion stack, not a cache: some checks use temporary
     key-erased/key-holder types, so their raw pointers must not outlive this call. */
  xt_assignability_leave (context, node);
  return assignable;
}

static bool typeid_kind_to_equiv_kind (ddsi_typeid_kind_t kind, DDS_XTypes_EquivalenceKind *ek)
{
  switch (kind)
  {
    case DDSI_TYPEID_KIND_MINIMAL:
    case DDSI_TYPEID_KIND_PLAIN_COLLECTION_MINIMAL:
      *ek = DDS_XTypes_EK_MINIMAL;
      return true;
    case DDSI_TYPEID_KIND_COMPLETE:
    case DDSI_TYPEID_KIND_PLAIN_COLLECTION_COMPLETE:
      *ek = DDS_XTypes_EK_COMPLETE;
      return true;
    case DDSI_TYPEID_KIND_FULLY_DESCRIPTIVE:
      *ek = DDS_XTypes_EK_BOTH;
      return true;
    case DDSI_TYPEID_KIND_INVALID:
      return false;
  }
  return false;
}

static ddsi_typeid_kind_t collection_kind_from_equiv_kind (DDS_XTypes_EquivalenceKind ek)
{
  switch (ek)
  {
    case DDS_XTypes_EK_MINIMAL:
      return DDSI_TYPEID_KIND_PLAIN_COLLECTION_MINIMAL;
    case DDS_XTypes_EK_COMPLETE:
      return DDSI_TYPEID_KIND_PLAIN_COLLECTION_COMPLETE;
    case DDS_XTypes_EK_BOTH:
      return DDSI_TYPEID_KIND_FULLY_DESCRIPTIVE;
    default:
      return DDSI_TYPEID_KIND_INVALID;
  }
}

static ddsi_typeid_kind_t ddsi_typeid_kind_impl (const struct DDS_XTypes_TypeIdentifier *type_id)
{
  ddsi_typeid_kind_t kind = DDSI_TYPEID_KIND_INVALID;
  const bool is_scc = type_id->_d == DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT;
  const bool is_minimal = ddsi_typeid_is_minimal_impl (type_id) ||
    (is_scc && type_id->_u.sc_component_id.sc_component_id._d == DDS_XTypes_EK_MINIMAL);
  const bool is_complete = ddsi_typeid_is_complete_impl (type_id) ||
    (is_scc && type_id->_u.sc_component_id.sc_component_id._d == DDS_XTypes_EK_COMPLETE);

  if (is_minimal)
    kind = DDSI_TYPEID_KIND_MINIMAL;
  else if (is_complete)
    kind = DDSI_TYPEID_KIND_COMPLETE;
  else if (is_scc)
    kind = DDSI_TYPEID_KIND_INVALID;
  else
  {
    if (type_id->_d < DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL)
      kind = DDSI_TYPEID_KIND_FULLY_DESCRIPTIVE;
    else
    {
      DDS_XTypes_EquivalenceKind header_ek, element_ek;
      ddsi_typeid_kind_t element_kind = DDSI_TYPEID_KIND_INVALID;
      switch (type_id->_d)
      {
        case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
          header_ek = type_id->_u.array_sdefn.header.equiv_kind;
          element_kind = ddsi_typeid_kind_impl (type_id->_u.array_sdefn.element_identifier);
          break;
        case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
          header_ek = type_id->_u.array_ldefn.header.equiv_kind;
          element_kind = ddsi_typeid_kind_impl (type_id->_u.array_ldefn.element_identifier);
          break;
        case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
          header_ek = type_id->_u.seq_sdefn.header.equiv_kind;
          element_kind = ddsi_typeid_kind_impl (type_id->_u.seq_sdefn.element_identifier);
          break;
        case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
          header_ek = type_id->_u.seq_ldefn.header.equiv_kind;
          element_kind = ddsi_typeid_kind_impl (type_id->_u.seq_ldefn.element_identifier);
          break;
        case DDS_XTypes_TI_PLAIN_MAP_SMALL: {
          header_ek = type_id->_u.map_sdefn.header.equiv_kind;
          DDS_XTypes_EquivalenceKind key_ek;
          if (!typeid_kind_to_equiv_kind (ddsi_typeid_kind_impl (type_id->_u.map_sdefn.key_identifier), &key_ek) ||
              !typeid_kind_to_equiv_kind (ddsi_typeid_kind_impl (type_id->_u.map_sdefn.element_identifier), &element_ek))
            return DDSI_TYPEID_KIND_INVALID;
          if (key_ek != DDS_XTypes_EK_BOTH && element_ek != DDS_XTypes_EK_BOTH && key_ek != element_ek)
            return DDSI_TYPEID_KIND_INVALID;
          element_ek = key_ek == DDS_XTypes_EK_BOTH ? element_ek : key_ek;
          if (header_ek != element_ek)
            return DDSI_TYPEID_KIND_INVALID;
          return collection_kind_from_equiv_kind (header_ek);
        }
        case DDS_XTypes_TI_PLAIN_MAP_LARGE: {
          header_ek = type_id->_u.map_ldefn.header.equiv_kind;
          DDS_XTypes_EquivalenceKind key_ek;
          if (!typeid_kind_to_equiv_kind (ddsi_typeid_kind_impl (type_id->_u.map_ldefn.key_identifier), &key_ek) ||
              !typeid_kind_to_equiv_kind (ddsi_typeid_kind_impl (type_id->_u.map_ldefn.element_identifier), &element_ek))
            return DDSI_TYPEID_KIND_INVALID;
          if (key_ek != DDS_XTypes_EK_BOTH && element_ek != DDS_XTypes_EK_BOTH && key_ek != element_ek)
            return DDSI_TYPEID_KIND_INVALID;
          element_ek = key_ek == DDS_XTypes_EK_BOTH ? element_ek : key_ek;
          if (header_ek != element_ek)
            return DDSI_TYPEID_KIND_INVALID;
          return collection_kind_from_equiv_kind (header_ek);
        }
          break;
        default:
          return DDSI_TYPEID_KIND_INVALID;
      }
      if (!typeid_kind_to_equiv_kind (element_kind, &element_ek) || header_ek != element_ek)
        return DDSI_TYPEID_KIND_INVALID;
      kind = collection_kind_from_equiv_kind (header_ek);
    }
  }
  return kind;
}

bool ddsi_typeid_contains_scc_impl (const struct DDS_XTypes_TypeIdentifier *type_id)
{
  if (type_id == NULL)
    return false;

  switch (type_id->_d)
  {
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT:
      return true;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      return ddsi_typeid_contains_scc_impl (type_id->_u.seq_sdefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      return ddsi_typeid_contains_scc_impl (type_id->_u.seq_ldefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
      return ddsi_typeid_contains_scc_impl (type_id->_u.array_sdefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      return ddsi_typeid_contains_scc_impl (type_id->_u.array_ldefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_MAP_SMALL:
      return ddsi_typeid_contains_scc_impl (type_id->_u.map_sdefn.key_identifier) ||
        ddsi_typeid_contains_scc_impl (type_id->_u.map_sdefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_MAP_LARGE:
      return ddsi_typeid_contains_scc_impl (type_id->_u.map_ldefn.key_identifier) ||
        ddsi_typeid_contains_scc_impl (type_id->_u.map_ldefn.element_identifier);
    default:
      return false;
  }
}

bool ddsi_xt_is_assignable_from (struct ddsi_domaingv *gv, const struct xt_type *rd_xt, const struct xt_type *wr_xt, const dds_type_consistency_enforcement_qospolicy_t *tce, struct ddsi_non_assignability_reason *reason)
{
  struct xt_assignability_context context;
  reason->code = DDSI_NONASSIGN_ASSIGNABLE;
  reason->t1_id._d = DDS_XTypes_TK_NONE;
  reason->t2_id._d = DDS_XTypes_TK_NONE;
  reason->t1_typekind = DDS_XTypes_TK_NONE;
  reason->t2_typekind = DDS_XTypes_TK_NONE;
  if (xt_assignability_context_init (&context) != DDS_RETCODE_OK)
    return xt_non_assignable (reason, DDSI_NONASSIGN_UNKNOWN, rd_xt, wr_xt, 0);
  const bool assignable = xt_is_assignable_from_impl (gv, &context, rd_xt, wr_xt, tce, reason);
  xt_assignability_context_fini (&context);
  if (assignable)
    return true;
  if (reason->code == DDSI_NONASSIGN_ASSIGNABLE)
    xt_non_assignable (reason, DDSI_NONASSIGN_UNKNOWN, rd_xt, wr_xt, 0);
  return false;
}

ddsi_typeid_kind_t ddsi_typeid_kind (const ddsi_typeid_t *type_id)
{
  return ddsi_typeid_kind_impl (&type_id->x);
}

static bool xt_is_non_hash (const struct xt_type *xt)
{
  return xt_is_fully_descriptive (xt) || xt_is_plain_collection (xt);
}

static uint32_t xt_typeid_gen_node_hash (const void *vnode)
{
  const struct xt_typeid_gen_node *node = vnode;
  const uintptr_t x = (uintptr_t) node->type;
  return ddsrt_mh3 (&x, sizeof (x), 0);
}

static bool xt_typeid_gen_node_equal (const void *vnode_a, const void *vnode_b)
{
  const struct xt_typeid_gen_node *a = vnode_a;
  const struct xt_typeid_gen_node *b = vnode_b;
  return a->type == b->type;
}

static dds_return_t xt_typeid_gen_nodes_append (struct xt_typeid_gen_context *context, struct xt_typeid_gen_node *node)
{
  if (context->n_nodes == context->node_seq_size)
  {
    const uint32_t size = context->node_seq_size ? 2 * context->node_seq_size : 8;
    struct xt_typeid_gen_node **node_seq = ddsrt_realloc (context->node_seq, size * sizeof (*node_seq));
    if (node_seq == NULL)
      return DDS_RETCODE_OUT_OF_RESOURCES;
    context->node_seq = node_seq;
    context->node_seq_size = size;
  }
  context->node_seq[context->n_nodes++] = node;
  return DDS_RETCODE_OK;
}

static dds_return_t xt_typeid_gen_deps_append (struct xt_typeid_gen_node *node, struct xt_typeid_gen_node *dep)
{
  if (node->n_deps == node->deps_size)
  {
    const uint32_t size = node->deps_size ? 2 * node->deps_size : 4;
    struct xt_typeid_gen_node **deps = ddsrt_realloc (node->deps, size * sizeof (*deps));
    if (deps == NULL)
      return DDS_RETCODE_OUT_OF_RESOURCES;
    node->deps = deps;
    node->deps_size = size;
  }
  node->deps[node->n_deps++] = dep;
  return DDS_RETCODE_OK;
}

static dds_return_t xt_typeid_gen_sccs_append (struct xt_typeid_gen_context *context, struct xt_typeid_gen_scc *scc)
{
  if (context->n_sccs == context->sccs_size)
  {
    const uint32_t size = context->sccs_size ? 2 * context->sccs_size : 4;
    struct xt_typeid_gen_scc **sccs = ddsrt_realloc (context->sccs, size * sizeof (*sccs));
    if (sccs == NULL)
      return DDS_RETCODE_OUT_OF_RESOURCES;
    context->sccs = sccs;
    context->sccs_size = size;
  }
  context->sccs[context->n_sccs++] = scc;
  return DDS_RETCODE_OK;
}

static dds_return_t xt_typeid_gen_scc_work_append (struct xt_typeid_gen_context *context, struct xt_typeid_gen_scc *scc)
{
  if (context->n_scc_work == context->scc_work_size)
  {
    const uint32_t size = context->scc_work_size ? 2 * context->scc_work_size : 4;
    struct xt_typeid_gen_scc **scc_work = ddsrt_realloc (context->scc_work, size * sizeof (*scc_work));
    if (scc_work == NULL)
      return DDS_RETCODE_OUT_OF_RESOURCES;
    context->scc_work = scc_work;
    context->scc_work_size = size;
  }
  scc->order = context->n_scc_work;
  context->scc_work[context->n_scc_work++] = scc;
  return DDS_RETCODE_OK;
}

static dds_return_t xt_typeid_gen_stack_push (struct xt_typeid_gen_context *context, struct xt_typeid_gen_node *node)
{
  if (context->stack_len == context->stack_size)
  {
    const uint32_t size = context->stack_size ? 2 * context->stack_size : 8;
    struct xt_typeid_gen_node **stack = ddsrt_realloc (context->stack, size * sizeof (*stack));
    if (stack == NULL)
      return DDS_RETCODE_OUT_OF_RESOURCES;
    context->stack = stack;
    context->stack_size = size;
  }
  context->stack[context->stack_len++] = node;
  node->on_stack = true;
  return DDS_RETCODE_OK;
}

static void xt_typeid_gen_node_free (void *vnode, void *arg)
{
  struct xt_typeid_gen_node *node = vnode;
  (void) arg;
  if (node->type_id_valid)
    ddsi_typeid_fini_impl (&node->type_id);
  ddsrt_free (node->deps);
  ddsrt_free (node);
}

static void xt_typeid_gen_context_fini (struct xt_typeid_gen_context *context)
{
  if (context->nodes != NULL)
  {
    ddsrt_hh_enum (context->nodes, xt_typeid_gen_node_free, NULL);
    ddsrt_hh_free (context->nodes);
  }
  for (uint32_t n = 0; n < context->n_sccs; n++)
  {
    ddsrt_free (context->sccs[n]->nodes);
    ddsrt_free (context->sccs[n]);
  }
  ddsrt_free (context->sccs);
  ddsrt_free (context->scc_work);
  ddsrt_free (context->stack);
  ddsrt_free (context->node_seq);
}

static dds_return_t xt_typeid_gen_node_get (struct xt_typeid_gen_context *context, const struct xt_type *type, struct xt_typeid_gen_node **node)
{
  const struct xt_typeid_gen_node templ = { .type = type };
  if ((*node = ddsrt_hh_lookup (context->nodes, &templ)) != NULL)
    return DDS_RETCODE_OK;

  if ((*node = ddsrt_calloc (1, sizeof (**node))) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  (*node)->type = type;
  (*node)->index = UINT32_MAX;
  (*node)->lowlink = UINT32_MAX;
  ddsrt_hh_add_absent (context->nodes, *node);
  dds_return_t ret;
  if ((ret = xt_typeid_gen_nodes_append (context, *node)) != DDS_RETCODE_OK)
  {
    ddsrt_hh_remove_present (context->nodes, *node);
    xt_typeid_gen_node_free (*node, NULL);
    *node = NULL;
  }
  return ret;
}

static struct xt_typeid_gen_node *xt_typeid_gen_node_lookup (const struct xt_typeid_gen_context *context, const struct xt_type *type)
{
  if (context->nodes == NULL)
    return NULL;
  const struct xt_typeid_gen_node templ = { .type = type };
  return ddsrt_hh_lookup (context->nodes, &templ);
}

static dds_return_t xt_typeid_gen_add_dep (struct xt_typeid_gen_context *context, struct xt_typeid_gen_node *src, const struct xt_type *dep);

static dds_return_t xt_typeid_gen_add_dep_non_hash (struct xt_typeid_gen_context *context, struct xt_typeid_gen_node *src, const struct xt_type *dep)
{
  assert (xt_is_non_hash (dep));
  /* Plain collections and fully descriptive types have TypeIdentifiers without
     generated TypeObjects, so they are not SCC nodes. Collection dependencies
     are forwarded to their hash-identified element/key types; this can turn,
     for example, sequence recursion into a self-loop on the aggregate node. */
  switch (dep->_d)
  {
    case DDS_XTypes_TK_SEQUENCE:
      return xt_typeid_gen_add_dep (context, src, &dep->_u.seq.c.element_type->xt);
    case DDS_XTypes_TK_ARRAY:
      return xt_typeid_gen_add_dep (context, src, &dep->_u.array.c.element_type->xt);
    case DDS_XTypes_TK_MAP:
    {
      dds_return_t ret;
      if ((ret = xt_typeid_gen_add_dep (context, src, &dep->_u.map.key_type->xt)) != DDS_RETCODE_OK)
        return ret;
      return xt_typeid_gen_add_dep (context, src, &dep->_u.map.c.element_type->xt);
    }
    default:
      return DDS_RETCODE_OK;
  }
}

static dds_return_t xt_typeid_gen_node_add_deps (struct xt_typeid_gen_context *context, struct xt_typeid_gen_node *node)
{
  dds_return_t ret = DDS_RETCODE_OK;
  if (node->deps_done)
    return ret;
  node->deps_done = true;

  switch (node->type->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      ret = xt_typeid_gen_add_dep (context, node, &node->type->_u.alias.related_type->xt);
      break;
    case DDS_XTypes_TK_STRUCTURE:
      if (node->type->_u.structure.base_type != NULL)
        ret = xt_typeid_gen_add_dep (context, node, &node->type->_u.structure.base_type->xt);
      for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < node->type->_u.structure.members.length; n++)
        ret = xt_typeid_gen_add_dep (context, node, &node->type->_u.structure.members.seq[n].type->xt);
      break;
    case DDS_XTypes_TK_UNION:
      ret = xt_typeid_gen_add_dep (context, node, &node->type->_u.union_type.disc_type->xt);
      for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < node->type->_u.union_type.members.length; n++)
        ret = xt_typeid_gen_add_dep (context, node, &node->type->_u.union_type.members.seq[n].type->xt);
      break;
    case DDS_XTypes_TK_SEQUENCE:
      ret = xt_typeid_gen_add_dep (context, node, &node->type->_u.seq.c.element_type->xt);
      break;
    case DDS_XTypes_TK_ARRAY:
      ret = xt_typeid_gen_add_dep (context, node, &node->type->_u.array.c.element_type->xt);
      break;
    case DDS_XTypes_TK_MAP:
      if ((ret = xt_typeid_gen_add_dep (context, node, &node->type->_u.map.key_type->xt)) == DDS_RETCODE_OK)
        ret = xt_typeid_gen_add_dep (context, node, &node->type->_u.map.c.element_type->xt);
      break;
    default:
      break;
  }
  return ret;
}

static dds_return_t xt_typeid_gen_add_dep (struct xt_typeid_gen_context *context, struct xt_typeid_gen_node *src, const struct xt_type *dep)
{
  if (ddsi_xt_missing_definition (dep))
    return DDS_RETCODE_OK;
  if (xt_is_non_hash (dep))
    return xt_typeid_gen_add_dep_non_hash (context, src, dep);

  struct xt_typeid_gen_node *dep_node;
  dds_return_t ret;
  if ((ret = xt_typeid_gen_node_get (context, dep, &dep_node)) != DDS_RETCODE_OK)
    return ret;
  if (src != NULL && (ret = xt_typeid_gen_deps_append (src, dep_node)) != DDS_RETCODE_OK)
    return ret;
  return xt_typeid_gen_node_add_deps (context, dep_node);
}

static dds_return_t xt_typeid_gen_component_new (struct xt_typeid_gen_context *context, struct xt_typeid_gen_node *root)
{
  struct xt_typeid_gen_scc *scc = ddsrt_calloc (1, sizeof (*scc));
  if (scc == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  uint32_t n_nodes = 0;
  for (uint32_t n = context->stack_len; n > 0; n--)
  {
    n_nodes++;
    if (context->stack[n - 1] == root)
      break;
  }
  scc->nodes = ddsrt_malloc (n_nodes * sizeof (*scc->nodes));
  if (scc->nodes == NULL)
  {
    ddsrt_free (scc);
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  scc->n_nodes = n_nodes;

  for (uint32_t n = n_nodes; n > 0; n--)
  {
    struct xt_typeid_gen_node *node = context->stack[--context->stack_len];
    node->on_stack = false;
    node->scc = scc;
    scc->nodes[n - 1] = node;
  }

  dds_return_t ret = xt_typeid_gen_sccs_append (context, scc);
  if (ret != DDS_RETCODE_OK)
  {
    ddsrt_free (scc->nodes);
    ddsrt_free (scc);
  }
  return ret;
}

static dds_return_t xt_typeid_gen_strongconnect (struct xt_typeid_gen_context *context, struct xt_typeid_gen_node *node)
{
  dds_return_t ret;
  node->index = context->next_index;
  node->lowlink = context->next_index++;
  if ((ret = xt_typeid_gen_stack_push (context, node)) != DDS_RETCODE_OK)
    return ret;

  for (uint32_t n = 0; n < node->n_deps; n++)
  {
    struct xt_typeid_gen_node *dep = node->deps[n];
    if (dep->index == UINT32_MAX)
    {
      if ((ret = xt_typeid_gen_strongconnect (context, dep)) != DDS_RETCODE_OK)
        return ret;
      if (dep->lowlink < node->lowlink)
        node->lowlink = dep->lowlink;
    }
    else if (dep->on_stack && dep->index < node->lowlink)
      node->lowlink = dep->index;
  }

  return node->lowlink == node->index ? xt_typeid_gen_component_new (context, node) : DDS_RETCODE_OK;
}

static bool xt_typeid_gen_scc_is_recursive (const struct xt_typeid_gen_scc *scc)
{
  if (scc->n_nodes > 1)
    return true;
  for (uint32_t n = 0; n < scc->nodes[0]->n_deps; n++)
    if (scc->nodes[0]->deps[n] == scc->nodes[0])
      return true;
  return false;
}

static dds_return_t xt_typeid_gen_schedule_scc (struct xt_typeid_gen_context *context, struct xt_typeid_gen_scc *scc)
{
  if (scc->scheduled)
    return DDS_RETCODE_OK;
  if (scc->scheduling)
    return DDS_RETCODE_BAD_PARAMETER;

  scc->scheduling = true;
  dds_return_t ret = DDS_RETCODE_OK;
  /* TypeObjects for a component may refer to identifiers from other components,
     so finalize dependencies before users of those identifiers. Cycles inside
     one component are handled by SCC identifiers below. */
  for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < scc->n_nodes; n++)
  {
    struct xt_typeid_gen_node *node = scc->nodes[n];
    for (uint32_t d = 0; ret == DDS_RETCODE_OK && d < node->n_deps; d++)
      if (node->deps[d]->scc != scc)
        ret = xt_typeid_gen_schedule_scc (context, node->deps[d]->scc);
  }
  scc->scheduling = false;
  if (ret != DDS_RETCODE_OK)
    return ret;

  scc->recursive = xt_typeid_gen_scc_is_recursive (scc);
  scc->scheduled = true;
  return xt_typeid_gen_scc_work_append (context, scc);
}

static dds_return_t xt_typeid_gen_schedule_sccs (struct xt_typeid_gen_context *context)
{
  dds_return_t ret = DDS_RETCODE_OK;
  for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < context->n_sccs; n++)
    ret = xt_typeid_gen_schedule_scc (context, context->sccs[n]);
  return ret;
}

static dds_return_t xt_typeid_gen_context_init (struct xt_typeid_gen_context *context, const struct xt_type *root, ddsi_typeid_kind_t kind)
{
  dds_return_t ret = DDS_RETCODE_OK;
  memset (context, 0, sizeof (*context));
  context->kind = kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_PLAIN_COLLECTION_MINIMAL
    ? DDSI_TYPEID_KIND_MINIMAL : DDSI_TYPEID_KIND_COMPLETE;
  if ((context->nodes = ddsrt_hh_new (1, xt_typeid_gen_node_hash, xt_typeid_gen_node_equal)) == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  if (ddsi_xt_has_definition (root))
  {
    if (xt_is_non_hash (root))
      ret = xt_typeid_gen_add_dep_non_hash (context, NULL, root);
    else
      ret = xt_typeid_gen_add_dep (context, NULL, root);
  }
  /* Find SCCs after all reachable hash TypeObjects have been added. Recursive
     SCCs are the points where XTypes requires SCC TypeIdentifiers instead of
     ordinary hash identifiers. */
  for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < context->n_nodes; n++)
    if (context->node_seq[n]->index == UINT32_MAX)
      ret = xt_typeid_gen_strongconnect (context, context->node_seq[n]);
  if (ret == DDS_RETCODE_OK)
    ret = xt_typeid_gen_schedule_sccs (context);
  return ret;
}

ddsrt_nonnull_all
static void xt_typeid_gen_emit_typeid (struct xt_typeid_gen_context *context, const struct xt_type *xt, struct DDS_XTypes_TypeIdentifier *ti, ddsi_typeid_kind_t kind);
ddsrt_nonnull_all
static void xt_typeid_gen_emit_typeobject (struct xt_typeid_gen_context *context, const struct xt_type *xt, struct DDS_XTypes_TypeObject *to, ddsi_typeid_kind_t kind);

static const char *xt_typeid_gen_type_name (const struct xt_type *type)
{
  switch (type->_d)
  {
    case DDS_XTypes_TK_SEQUENCE: return type->_u.seq.c.detail.type_name;
    case DDS_XTypes_TK_ARRAY: return type->_u.array.c.detail.type_name;
    case DDS_XTypes_TK_MAP: return type->_u.map.c.detail.type_name;
    case DDS_XTypes_TK_ALIAS: return type->_u.alias.detail.type_name;
    case DDS_XTypes_TK_STRUCTURE: return type->_u.structure.detail.type_name;
    case DDS_XTypes_TK_UNION: return type->_u.union_type.detail.type_name;
    case DDS_XTypes_TK_BITSET: return type->_u.bitset.detail.type_name;
    case DDS_XTypes_TK_ENUM: return type->_u.enum_type.detail.type_name;
    case DDS_XTypes_TK_BITMASK: return type->_u.bitmask.detail.type_name;
    default: return "";
  }
}

static int xt_typeid_gen_node_name_cmp (const void *va, const void *vb)
{
  const struct xt_typeid_gen_node *a = *(struct xt_typeid_gen_node * const *) va;
  const struct xt_typeid_gen_node *b = *(struct xt_typeid_gen_node * const *) vb;
  int cmp = strcmp (xt_typeid_gen_type_name (a->type), xt_typeid_gen_type_name (b->type));
  if (cmp != 0)
    return cmp;
  if (a->type->_d != b->type->_d)
    return a->type->_d < b->type->_d ? -1 : 1;
  return a->index < b->index ? -1 : a->index > b->index;
}

static DDS_XTypes_EquivalenceKind xt_typeid_gen_equiv_kind (ddsi_typeid_kind_t kind)
{
  assert (kind == DDSI_TYPEID_KIND_MINIMAL || kind == DDSI_TYPEID_KIND_COMPLETE);
  return kind == DDSI_TYPEID_KIND_MINIMAL ? DDS_XTypes_EK_MINIMAL : DDS_XTypes_EK_COMPLETE;
}

static void xt_typeid_gen_set_scc_id (struct xt_typeid_gen_node *node, DDS_XTypes_EquivalenceKind equiv_kind, int32_t scc_length, int32_t scc_index, const DDS_XTypes_EquivalenceHash *hash)
{
  if (node->type_id_valid)
    ddsi_typeid_fini_impl (&node->type_id);
  memset (&node->type_id, 0, sizeof (node->type_id));
  node->type_id._d = DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT;
  node->type_id._u.sc_component_id.sc_component_id._d = equiv_kind;
  if (hash != NULL)
    memcpy (node->type_id._u.sc_component_id.sc_component_id._u.hash, *hash, sizeof (*hash));
  node->type_id._u.sc_component_id.scc_length = scc_length;
  node->type_id._u.sc_component_id.scc_index = scc_index;
  node->type_id_valid = true;
}

static bool xt_typeid_gen_finalize_runtime_scc (struct xt_typeid_gen_context *context, struct xt_typeid_gen_scc *scc)
{
  const DDS_XTypes_EquivalenceKind equiv_kind = xt_typeid_gen_equiv_kind (context->kind);
  const struct ddsi_type_scc *runtime_scc = NULL;
  struct xt_typeid_gen_node **ordered_nodes = ddsrt_calloc (scc->n_nodes, sizeof (*ordered_nodes));
  if (ordered_nodes == NULL)
    abort ();

  for (uint32_t n = 0; n < scc->n_nodes; n++)
  {
    const struct ddsi_type *type = ddsi_type_from_xt_type (scc->nodes[n]->type);
    if (type->scc == NULL || type->xt.id.x._d != DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT ||
        type->xt.id.x._u.sc_component_id.sc_component_id._d != equiv_kind ||
        type->scc->sc_component_id._d != equiv_kind ||
        type->scc->n_wire_types != scc->n_nodes)
    {
      ddsrt_free (ordered_nodes);
      return false;
    }
    for (uint32_t i = 0; i < type->scc->n_wire_types; i++)
    {
      if (type->scc->types[i] == NULL)
      {
        ddsrt_free (ordered_nodes);
        return false;
      }
    }
    if (runtime_scc == NULL)
      runtime_scc = type->scc;
    else if (runtime_scc != type->scc)
    {
      ddsrt_free (ordered_nodes);
      return false;
    }

    const int32_t scc_index = type->xt.id.x._u.sc_component_id.scc_index;
    if (scc_index <= 0 || (uint32_t) scc_index > scc->n_nodes || ordered_nodes[scc_index - 1] != NULL)
    {
      ddsrt_free (ordered_nodes);
      return false;
    }
    ordered_nodes[scc_index - 1] = scc->nodes[n];
  }

  for (uint32_t n = 0; n < scc->n_nodes; n++)
  {
    if (ordered_nodes[n] == NULL)
    {
      ddsrt_free (ordered_nodes);
      return false;
    }
  }

  memcpy (scc->nodes, ordered_nodes, scc->n_nodes * sizeof (*scc->nodes));
  for (uint32_t n = 0; n < scc->n_nodes; n++)
    xt_typeid_gen_set_scc_id (scc->nodes[n], equiv_kind, (int32_t) scc->n_nodes, (int32_t) n + 1, &runtime_scc->sc_component_id._u.hash);
  ddsrt_free (ordered_nodes);
  return true;
}

ddsrt_nonnull_all
dds_return_t ddsi_typeobj_get_scc_hash (DDS_XTypes_EquivalenceHash hash, const struct DDS_XTypes_TypeObject *type_objects, uint32_t n_type_objects)
{
  dds_ostreamLE_t os;
  dds_ostreamLE_init (&os, &dds_cdrstream_default_allocator, sizeof (uint32_t), DDSI_RTPS_CDR_ENC_VERSION_2);
  if (os.x.m_buffer == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  os.x.m_buffer[0] = (unsigned char) (n_type_objects & 0xff);
  os.x.m_buffer[1] = (unsigned char) ((n_type_objects >> 8) & 0xff);
  os.x.m_buffer[2] = (unsigned char) ((n_type_objects >> 16) & 0xff);
  os.x.m_buffer[3] = (unsigned char) ((n_type_objects >> 24) & 0xff);
  os.x.m_index = sizeof (uint32_t);
  for (uint32_t n = 0; n < n_type_objects; n++)
  {
    if (!dds_stream_writeLE (&os, &dds_cdrstream_default_allocator, (const char *) &type_objects[n], DDS_XTypes_TypeObject_desc.m_ops))
    {
      dds_ostreamLE_fini (&os, &dds_cdrstream_default_allocator);
      return DDS_RETCODE_BAD_PARAMETER;
    }
  }

  char buf[16];
  ddsrt_md5_state_t md5st;
  ddsrt_md5_init (&md5st);
  ddsrt_md5_append (&md5st, (ddsrt_md5_byte_t *) os.x.m_buffer, os.x.m_index);
  ddsrt_md5_finish (&md5st, (ddsrt_md5_byte_t *) buf);
  memcpy (hash, buf, sizeof (DDS_XTypes_EquivalenceHash));
  dds_ostreamLE_fini (&os, &dds_cdrstream_default_allocator);
  return DDS_RETCODE_OK;
}

static dds_return_t type_scc_graph_add_typeid (
    bool edges[DDSI_TYPE_SCC_MAX_WIRE_TYPES][DDSI_TYPE_SCC_MAX_WIRE_TYPES],
    const struct DDS_XTypes_StronglyConnectedComponentId *scc_id,
    const struct DDS_XTypes_TypeIdentifier *type_id,
    uint32_t src,
    bool *has_internal_edge)
{
  if (type_id == NULL)
    return DDS_RETCODE_OK;

  switch (type_id->_d)
  {
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      return type_scc_graph_add_typeid (edges, scc_id, type_id->_u.seq_sdefn.element_identifier, src, has_internal_edge);
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      return type_scc_graph_add_typeid (edges, scc_id, type_id->_u.seq_ldefn.element_identifier, src, has_internal_edge);
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
      return type_scc_graph_add_typeid (edges, scc_id, type_id->_u.array_sdefn.element_identifier, src, has_internal_edge);
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      return type_scc_graph_add_typeid (edges, scc_id, type_id->_u.array_ldefn.element_identifier, src, has_internal_edge);
    case DDS_XTypes_TI_PLAIN_MAP_SMALL:
    {
      dds_return_t ret = type_scc_graph_add_typeid (edges, scc_id, type_id->_u.map_sdefn.key_identifier, src, has_internal_edge);
      return ret != DDS_RETCODE_OK ? ret :
        type_scc_graph_add_typeid (edges, scc_id, type_id->_u.map_sdefn.element_identifier, src, has_internal_edge);
    }
    case DDS_XTypes_TI_PLAIN_MAP_LARGE:
    {
      dds_return_t ret = type_scc_graph_add_typeid (edges, scc_id, type_id->_u.map_ldefn.key_identifier, src, has_internal_edge);
      return ret != DDS_RETCODE_OK ? ret :
        type_scc_graph_add_typeid (edges, scc_id, type_id->_u.map_ldefn.element_identifier, src, has_internal_edge);
    }
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT:
      if (!ddsi_type_scc_id_is_valid_impl (&type_id->_u.sc_component_id))
        return DDS_RETCODE_BAD_PARAMETER;
      if (ddsi_typeobject_hashid_equal_impl (&type_id->_u.sc_component_id.sc_component_id, &scc_id->sc_component_id))
      {
        if (!ddsi_type_scc_id_same_component_impl (&type_id->_u.sc_component_id, scc_id))
          return DDS_RETCODE_BAD_PARAMETER;
        const uint32_t dst = (uint32_t) type_id->_u.sc_component_id.scc_index - 1;
        edges[src][dst] = true;
        *has_internal_edge = true;
      }
      return DDS_RETCODE_OK;
    default:
      return DDS_RETCODE_OK;
  }
}

static dds_return_t type_scc_graph_add_complete_typeobj (
    bool edges[DDSI_TYPE_SCC_MAX_WIRE_TYPES][DDSI_TYPE_SCC_MAX_WIRE_TYPES],
    const struct DDS_XTypes_StronglyConnectedComponentId *scc_id,
    const struct DDS_XTypes_CompleteTypeObject *type_obj,
    uint32_t src,
    bool *has_internal_edge)
{
  dds_return_t ret = DDS_RETCODE_OK;
  switch (type_obj->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      return type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.alias_type.body.common.related_type, src, has_internal_edge);
    case DDS_XTypes_TK_ANNOTATION:
      for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < type_obj->_u.annotation_type.member_seq._length; n++)
        ret = type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.annotation_type.member_seq._buffer[n].common.member_type_id, src, has_internal_edge);
      return ret;
    case DDS_XTypes_TK_STRUCTURE:
      if ((ret = type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.struct_type.header.base_type, src, has_internal_edge)) != DDS_RETCODE_OK)
        return ret;
      for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < type_obj->_u.struct_type.member_seq._length; n++)
        ret = type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.struct_type.member_seq._buffer[n].common.member_type_id, src, has_internal_edge);
      return ret;
    case DDS_XTypes_TK_UNION:
      if ((ret = type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.union_type.discriminator.common.type_id, src, has_internal_edge)) != DDS_RETCODE_OK)
        return ret;
      for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < type_obj->_u.union_type.member_seq._length; n++)
        ret = type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.union_type.member_seq._buffer[n].common.type_id, src, has_internal_edge);
      return ret;
    case DDS_XTypes_TK_SEQUENCE:
      return type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.sequence_type.element.common.type, src, has_internal_edge);
    case DDS_XTypes_TK_ARRAY:
      return type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.array_type.element.common.type, src, has_internal_edge);
    case DDS_XTypes_TK_MAP:
      if ((ret = type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.map_type.key.common.type, src, has_internal_edge)) != DDS_RETCODE_OK)
        return ret;
      return type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.map_type.element.common.type, src, has_internal_edge);
    default:
      return DDS_RETCODE_OK;
  }
}

static dds_return_t type_scc_graph_add_minimal_typeobj (
    bool edges[DDSI_TYPE_SCC_MAX_WIRE_TYPES][DDSI_TYPE_SCC_MAX_WIRE_TYPES],
    const struct DDS_XTypes_StronglyConnectedComponentId *scc_id,
    const struct DDS_XTypes_MinimalTypeObject *type_obj,
    uint32_t src,
    bool *has_internal_edge)
{
  dds_return_t ret = DDS_RETCODE_OK;
  switch (type_obj->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      return type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.alias_type.body.common.related_type, src, has_internal_edge);
    case DDS_XTypes_TK_ANNOTATION:
      for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < type_obj->_u.annotation_type.member_seq._length; n++)
        ret = type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.annotation_type.member_seq._buffer[n].common.member_type_id, src, has_internal_edge);
      return ret;
    case DDS_XTypes_TK_STRUCTURE:
      if ((ret = type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.struct_type.header.base_type, src, has_internal_edge)) != DDS_RETCODE_OK)
        return ret;
      for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < type_obj->_u.struct_type.member_seq._length; n++)
        ret = type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.struct_type.member_seq._buffer[n].common.member_type_id, src, has_internal_edge);
      return ret;
    case DDS_XTypes_TK_UNION:
      if ((ret = type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.union_type.discriminator.common.type_id, src, has_internal_edge)) != DDS_RETCODE_OK)
        return ret;
      for (uint32_t n = 0; ret == DDS_RETCODE_OK && n < type_obj->_u.union_type.member_seq._length; n++)
        ret = type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.union_type.member_seq._buffer[n].common.type_id, src, has_internal_edge);
      return ret;
    case DDS_XTypes_TK_SEQUENCE:
      return type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.sequence_type.element.common.type, src, has_internal_edge);
    case DDS_XTypes_TK_ARRAY:
      return type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.array_type.element.common.type, src, has_internal_edge);
    case DDS_XTypes_TK_MAP:
      if ((ret = type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.map_type.key.common.type, src, has_internal_edge)) != DDS_RETCODE_OK)
        return ret;
      return type_scc_graph_add_typeid (edges, scc_id, &type_obj->_u.map_type.element.common.type, src, has_internal_edge);
    default:
      return DDS_RETCODE_OK;
  }
}

static dds_return_t type_scc_graph_add_typeobj (
    bool edges[DDSI_TYPE_SCC_MAX_WIRE_TYPES][DDSI_TYPE_SCC_MAX_WIRE_TYPES],
    const struct DDS_XTypes_StronglyConnectedComponentId *scc_id,
    const struct DDS_XTypes_TypeObject *type_obj,
    uint32_t src,
    bool *has_internal_edge)
{
  if (type_obj->_d == DDS_XTypes_EK_COMPLETE)
    return type_scc_graph_add_complete_typeobj (edges, scc_id, &type_obj->_u.complete, src, has_internal_edge);
  else if (type_obj->_d == DDS_XTypes_EK_MINIMAL)
    return type_scc_graph_add_minimal_typeobj (edges, scc_id, &type_obj->_u.minimal, src, has_internal_edge);
  else
    return DDS_RETCODE_BAD_PARAMETER;
}

static void type_scc_graph_reach (
    bool reachable[DDSI_TYPE_SCC_MAX_WIRE_TYPES],
    bool edges[DDSI_TYPE_SCC_MAX_WIRE_TYPES][DDSI_TYPE_SCC_MAX_WIRE_TYPES],
    uint32_t nslots,
    uint32_t src,
    bool reverse)
{
  if (reachable[src])
    return;
  reachable[src] = true;
  for (uint32_t n = 0; n < nslots; n++)
  {
    if (reverse ? edges[n][src] : edges[src][n])
      type_scc_graph_reach (reachable, edges, nslots, n, reverse);
  }
}

ddsrt_nonnull_all
dds_return_t ddsi_typeobj_scc_verify_strongly_connected (
    const struct DDS_XTypes_StronglyConnectedComponentId *scc_id,
    const struct DDS_XTypes_TypeIdentifierTypeObjectPair * const *slots)
{
  if (!ddsi_type_scc_id_is_valid_impl (scc_id))
    return DDS_RETCODE_BAD_PARAMETER;

  const uint32_t nslots = (uint32_t) scc_id->scc_length;
  bool edges[DDSI_TYPE_SCC_MAX_WIRE_TYPES][DDSI_TYPE_SCC_MAX_WIRE_TYPES] = {{ false }};
  bool has_internal_edge = false;

  for (uint32_t n = 0; n < nslots; n++)
  {
    if (slots[n] == NULL)
      return DDS_RETCODE_BAD_PARAMETER;
    dds_return_t ret = type_scc_graph_add_typeobj (edges, scc_id, &slots[n]->type_object, n, &has_internal_edge);
    if (ret != DDS_RETCODE_OK)
      return ret;
  }
  if (!has_internal_edge)
    return DDS_RETCODE_BAD_PARAMETER;

  bool reachable[DDSI_TYPE_SCC_MAX_WIRE_TYPES] = { false };
  type_scc_graph_reach (reachable, edges, nslots, 0, false);
  for (uint32_t n = 0; n < nslots; n++)
    if (!reachable[n])
      return DDS_RETCODE_BAD_PARAMETER;

  memset (reachable, 0, sizeof (reachable));
  type_scc_graph_reach (reachable, edges, nslots, 0, true);
  for (uint32_t n = 0; n < nslots; n++)
    if (!reachable[n])
      return DDS_RETCODE_BAD_PARAMETER;

  return DDS_RETCODE_OK;
}

static void xt_typeid_gen_hash_scc_typeobjects (DDS_XTypes_EquivalenceHash hash, const struct DDS_XTypes_TypeObject *type_objects, uint32_t n_type_objects)
{
  if (ddsi_typeobj_get_scc_hash (hash, type_objects, n_type_objects) != DDS_RETCODE_OK)
    abort ();
}

static void xt_typeid_gen_finalize_node (struct xt_typeid_gen_context *context, struct xt_typeid_gen_node *node)
{
  if (node->type_id_valid)
    return;

  struct DDS_XTypes_TypeObject type_object;
  xt_typeid_gen_emit_typeobject (context, node->type, &type_object, context->kind);
  ddsi_typeobj_get_hash_id_impl (&type_object, &node->type_id);
  ddsi_typeobj_fini_impl (&type_object);
  node->type_id_valid = true;
}

static void xt_typeid_gen_finalize_recursive_scc (struct xt_typeid_gen_context *context, struct xt_typeid_gen_scc *scc)
{
  assert (scc->recursive);
  assert (scc->n_nodes <= INT32_MAX);
  if (xt_typeid_gen_finalize_runtime_scc (context, scc))
    return;
  qsort (scc->nodes, scc->n_nodes, sizeof (*scc->nodes), xt_typeid_gen_node_name_cmp);

  const int32_t scc_length = (int32_t) scc->n_nodes;
  const DDS_XTypes_EquivalenceKind equiv_kind = xt_typeid_gen_equiv_kind (context->kind);
  struct DDS_XTypes_TypeObject *type_objects = ddsrt_calloc (scc->n_nodes, sizeof (*type_objects));
  if (type_objects == NULL)
    abort ();

  /* SCC identifiers include the hash of the ordered TypeObject list, but those
     TypeObjects themselves contain SCC references for edges within the same
     component. First assign placeholder SCC ids, emit and hash the list, then
     replace the placeholders with SCC ids carrying the final hash. */
  for (int32_t n = 0; n < scc_length; n++)
    xt_typeid_gen_set_scc_id (scc->nodes[n], equiv_kind, scc_length, n + 1, NULL);
  for (uint32_t n = 0; n < scc->n_nodes; n++)
    xt_typeid_gen_emit_typeobject (context, scc->nodes[n]->type, &type_objects[n], context->kind);

  DDS_XTypes_EquivalenceHash hash;
  xt_typeid_gen_hash_scc_typeobjects (hash, type_objects, scc->n_nodes);
  for (int32_t n = 0; n < scc_length; n++)
    xt_typeid_gen_set_scc_id (scc->nodes[n], equiv_kind, scc_length, n + 1, &hash);

  for (uint32_t n = 0; n < scc->n_nodes; n++)
    ddsi_typeobj_fini_impl (&type_objects[n]);
  ddsrt_free (type_objects);
}

static void xt_typeid_gen_finalize_sccs (struct xt_typeid_gen_context *context)
{
  for (uint32_t n = 0; n < context->n_scc_work; n++)
  {
    struct xt_typeid_gen_scc *scc = context->scc_work[n];
    if (scc->recursive)
      xt_typeid_gen_finalize_recursive_scc (context, scc);
    else
      xt_typeid_gen_finalize_node (context, scc->nodes[0]);
  }
}

static void get_plain_collection_element_id (struct xt_typeid_gen_context *context, const struct xt_type *xt_el, struct DDS_XTypes_TypeIdentifier *ti, DDS_XTypes_EquivalenceKind *equiv_kind, ddsi_typeid_kind_t kind)
{
  xt_typeid_gen_emit_typeid (context, xt_el, ti, kind);
  if (!typeid_kind_to_equiv_kind (ddsi_typeid_kind_impl (ti), equiv_kind))
    /* The TypeIdentifier was just generated from a validated xt_type.  If it
       cannot be classified, the generator has produced an internally invalid
       collection element identifier rather than rejecting malformed input. */
    abort ();
}

static DDS_XTypes_EquivalenceKind map_plain_collection_equiv_kind (DDS_XTypes_EquivalenceKind key_equiv_kind, DDS_XTypes_EquivalenceKind element_equiv_kind)
{
  if (key_equiv_kind == DDS_XTypes_EK_BOTH)
    return element_equiv_kind;
  if (element_equiv_kind == DDS_XTypes_EK_BOTH)
    return key_equiv_kind;
  assert (key_equiv_kind == element_equiv_kind);
  return key_equiv_kind;
}

ddsrt_nonnull_all
static void ddsi_xt_get_non_hash_id (struct xt_typeid_gen_context *context, const struct xt_type *xt, struct DDS_XTypes_TypeIdentifier *ti, ddsi_typeid_kind_t kind)
{
  assert (xt_is_non_hash (xt));

  memset (ti, 0, sizeof (*ti));
  if (xt->_d <= DDS_XTypes_TK_CHAR16)
  {
    ti->_d = xt->_d;
  }
  else
  {
    switch (xt->_d)
    {
      case DDS_XTypes_TK_STRING8:
        if (xt->_u.str8.bound <= 255)
        {
          ti->_d = DDS_XTypes_TI_STRING8_SMALL;
          ti->_u.string_sdefn.bound = (DDS_XTypes_SBound) xt->_u.str8.bound;
        }
        else
        {
          ti->_d = DDS_XTypes_TI_STRING8_LARGE;
          ti->_u.string_ldefn.bound = xt->_u.str8.bound;
        }
        break;
      case DDS_XTypes_TK_STRING16:
        if (xt->_u.str16.bound <= 255)
        {
          ti->_d = DDS_XTypes_TI_STRING16_SMALL;
          ti->_u.string_sdefn.bound = (DDS_XTypes_SBound) xt->_u.str16.bound;
        }
        else
        {
          ti->_d = DDS_XTypes_TI_STRING16_LARGE;
          ti->_u.string_ldefn.bound = xt->_u.str16.bound;
        }
        break;
      case DDS_XTypes_TK_SEQUENCE:
        if (xt->_u.seq.bound <= 255)
        {
          ti->_d = DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL;
          ti->_u.seq_sdefn.bound = (DDS_XTypes_SBound) xt->_u.seq.bound;
          ti->_u.seq_sdefn.header.element_flags = xt->_u.seq.c.element_flags;
          ti->_u.seq_sdefn.element_identifier = ddsrt_malloc (sizeof (*ti->_u.seq_sdefn.element_identifier));
          get_plain_collection_element_id (context, &xt->_u.seq.c.element_type->xt, ti->_u.seq_sdefn.element_identifier, &ti->_u.seq_sdefn.header.equiv_kind, kind);
        }
        else
        {
          ti->_d = DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE;
          ti->_u.seq_ldefn.bound = (DDS_XTypes_LBound) xt->_u.seq.bound;
          ti->_u.seq_ldefn.header.element_flags = xt->_u.seq.c.element_flags;
          ti->_u.seq_ldefn.element_identifier = ddsrt_malloc (sizeof (*ti->_u.seq_ldefn.element_identifier));
          get_plain_collection_element_id (context, &xt->_u.seq.c.element_type->xt, ti->_u.seq_ldefn.element_identifier, &ti->_u.seq_ldefn.header.equiv_kind, kind);
        }
        break;
      case DDS_XTypes_TK_ARRAY: {
        bool sdefn = true;
        for (uint32_t n = 0; sdefn && n < xt->_u.array.bounds._length; n++)
          sdefn = xt->_u.array.bounds._buffer[n] <= 255;
        if (sdefn)
        {
          ti->_d = DDS_XTypes_TI_PLAIN_ARRAY_SMALL;
          xt_lbounds_to_sbounds (&ti->_u.array_sdefn.array_bound_seq, &xt->_u.array.bounds);
          ti->_u.array_sdefn.header.element_flags = xt->_u.array.c.element_flags;
          ti->_u.array_sdefn.element_identifier = ddsrt_malloc (sizeof (*ti->_u.array_sdefn.element_identifier));
          get_plain_collection_element_id (context, &xt->_u.array.c.element_type->xt, ti->_u.array_sdefn.element_identifier, &ti->_u.array_sdefn.header.equiv_kind, kind);
        }
        else
        {
          ti->_d = DDS_XTypes_TI_PLAIN_ARRAY_LARGE;
          xt_lbounds_dup (&ti->_u.array_ldefn.array_bound_seq, &xt->_u.array.bounds);
          ti->_u.array_ldefn.header.element_flags = xt->_u.array.c.element_flags;
          ti->_u.array_ldefn.element_identifier = ddsrt_malloc (sizeof (*ti->_u.array_ldefn.element_identifier));
          get_plain_collection_element_id (context, &xt->_u.array.c.element_type->xt, ti->_u.array_ldefn.element_identifier, &ti->_u.array_ldefn.header.equiv_kind, kind);
        }
        break;
      }

      case DDS_XTypes_TK_MAP: {
        DDS_XTypes_EquivalenceKind key_equiv_kind, element_equiv_kind;
        if (xt->_u.map.bound <= 255)
        {
          ti->_d = DDS_XTypes_TI_PLAIN_MAP_SMALL;
          ti->_u.map_sdefn.bound = (DDS_XTypes_SBound) xt->_u.map.bound;
          ti->_u.map_sdefn.key_flags = xt->_u.map.key_flags;
          ti->_u.map_sdefn.header.element_flags = xt->_u.map.c.element_flags;
          ti->_u.map_sdefn.key_identifier = ddsrt_malloc (sizeof (*ti->_u.map_sdefn.key_identifier));
          get_plain_collection_element_id (context, &xt->_u.map.key_type->xt, ti->_u.map_sdefn.key_identifier, &key_equiv_kind, kind);
          ti->_u.map_sdefn.element_identifier = ddsrt_malloc (sizeof (*ti->_u.map_sdefn.element_identifier));
          get_plain_collection_element_id (context, &xt->_u.map.c.element_type->xt, ti->_u.map_sdefn.element_identifier, &element_equiv_kind, kind);
          ti->_u.map_sdefn.header.equiv_kind = map_plain_collection_equiv_kind (key_equiv_kind, element_equiv_kind);
        }
        else
        {
          ti->_d = DDS_XTypes_TI_PLAIN_MAP_LARGE;
          ti->_u.map_ldefn.bound = (DDS_XTypes_LBound) xt->_u.map.bound;
          ti->_u.map_ldefn.key_flags = xt->_u.map.key_flags;
          ti->_u.map_ldefn.header.element_flags = xt->_u.map.c.element_flags;
          ti->_u.map_ldefn.key_identifier = ddsrt_malloc (sizeof (*ti->_u.map_ldefn.key_identifier));
          get_plain_collection_element_id (context, &xt->_u.map.key_type->xt, ti->_u.map_ldefn.key_identifier, &key_equiv_kind, kind);
          ti->_u.map_ldefn.element_identifier = ddsrt_malloc (sizeof (*ti->_u.map_ldefn.element_identifier));
          get_plain_collection_element_id (context, &xt->_u.map.c.element_type->xt, ti->_u.map_ldefn.element_identifier, &element_equiv_kind, kind);
          ti->_u.map_ldefn.header.equiv_kind = map_plain_collection_equiv_kind (key_equiv_kind, element_equiv_kind);
        }
        break;
      }
    }
  }
}

ddsrt_nonnull_all
static void xt_typeid_gen_emit_typeid (struct xt_typeid_gen_context *context, const struct xt_type *xt, struct DDS_XTypes_TypeIdentifier *ti, ddsi_typeid_kind_t kind)
{
  if (xt_is_non_hash (xt))
  {
    /* Get the non hash type ID: plain collection or fully descriptive */
    ddsi_xt_get_non_hash_id (context, xt, ti, kind);
  }
  else if (ddsi_xt_missing_definition (xt))
  {
    /* Copy the hash type id from an xt_type missing its definition; complete/minimal
       kind must match because there is no type object available to derive it from. */
    assert (xt->kind == kind);
    ddsi_typeid_copy_to_impl (ti, &xt->id);
  }
  else
  {
    struct xt_typeid_gen_node *node = xt_typeid_gen_node_lookup (context, xt);
    if (node != NULL && node->type_id_valid)
    {
      ddsi_typeid_copy_impl (ti, &node->type_id);
      return;
    }

    /* Calculate the hash type identifier from the type object. In case the type has a complete
       type object, both minimal and complete type ids can be extracted. */
    struct DDS_XTypes_TypeObject to;
    xt_typeid_gen_emit_typeobject (context, xt, &to, kind);
    ddsi_typeobj_get_hash_id_impl (&to, ti);
    ddsi_typeobj_fini_impl (&to);
  }
}

ddsrt_nonnull_all
void ddsi_xt_get_typeid_impl (const struct xt_type *xt, struct DDS_XTypes_TypeIdentifier *ti, ddsi_typeid_kind_t kind)
{
  struct xt_typeid_gen_context context;
  if (xt_typeid_gen_context_init (&context, xt, kind) == DDS_RETCODE_OK)
    xt_typeid_gen_finalize_sccs (&context);
  xt_typeid_gen_emit_typeid (&context, xt, ti, kind);
  xt_typeid_gen_context_fini (&context);
}

ddsrt_nonnull_all
static void xt_typeid_gen_emit_typeobject (struct xt_typeid_gen_context *context, const struct xt_type *xt, struct DDS_XTypes_TypeObject *to, ddsi_typeid_kind_t kind)
{
  assert (ddsi_xt_has_definition (xt));
  assert (!xt_is_non_hash (xt));

  memset (to, 0, sizeof (*to));
  if (kind == DDSI_TYPEID_KIND_MINIMAL)
  {
    to->_d = DDS_XTypes_EK_MINIMAL;
    struct DDS_XTypes_MinimalTypeObject *mto = &to->_u.minimal;
    mto->_d = xt->_d;
    switch (xt->_d)
    {
      case DDS_XTypes_TK_ALIAS:
      {
        struct DDS_XTypes_MinimalAliasType *malias = &mto->_u.alias_type;
        xt_typeid_gen_emit_typeid (context, &xt->_u.alias.related_type->xt, &malias->body.common.related_type, DDSI_TYPEID_KIND_MINIMAL);
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
          xt_typeid_gen_emit_typeid (context, &xt->_u.structure.base_type->xt, &mstruct->header.base_type, DDSI_TYPEID_KIND_MINIMAL);
        mstruct->member_seq._buffer = ddsrt_malloc (xt->_u.structure.members.length * sizeof (*mstruct->member_seq._buffer));
        mstruct->member_seq._length = xt->_u.structure.members.length;
        mstruct->member_seq._release = true;
        for (uint32_t n = 0; n < xt->_u.structure.members.length; n++)
        {
          mstruct->member_seq._buffer[n].common.member_id = xt->_u.structure.members.seq[n].id;
          mstruct->member_seq._buffer[n].common.member_flags = xt->_u.structure.members.seq[n].flags;
          xt_typeid_gen_emit_typeid (context, &xt->_u.structure.members.seq[n].type->xt, &mstruct->member_seq._buffer[n].common.member_type_id, DDSI_TYPEID_KIND_MINIMAL);
          get_minimal_member_detail (&mstruct->member_seq._buffer[n].detail, &xt->_u.structure.members.seq[n].detail);
        }
        break;
      }
      case DDS_XTypes_TK_UNION:
      {
        struct DDS_XTypes_MinimalUnionType *munion = &mto->_u.union_type;
        munion->union_flags = xt->_u.union_type.flags;
        xt_typeid_gen_emit_typeid (context, &xt->_u.union_type.disc_type->xt, &munion->discriminator.common.type_id, DDSI_TYPEID_KIND_MINIMAL);
        munion->discriminator.common.member_flags = xt->_u.union_type.disc_flags;
        munion->member_seq._buffer = ddsrt_malloc (xt->_u.union_type.members.length * sizeof (*munion->member_seq._buffer));
        munion->member_seq._length = munion->member_seq._maximum = xt->_u.union_type.members.length;
        munion->member_seq._release = true;
        for (uint32_t n = 0; n < xt->_u.union_type.members.length; n++)
        {
          munion->member_seq._buffer[n].common.member_id = xt->_u.union_type.members.seq[n].id;
          munion->member_seq._buffer[n].common.member_flags = xt->_u.union_type.members.seq[n].flags;
          xt_typeid_gen_emit_typeid (context, &xt->_u.union_type.members.seq[n].type->xt, &munion->member_seq._buffer[n].common.type_id, DDSI_TYPEID_KIND_MINIMAL);
          munion->member_seq._buffer[n].common.label_seq._length = xt->_u.union_type.members.seq[n].label_seq._length;
          if (munion->member_seq._buffer[n].common.label_seq._length > 0) {
            munion->member_seq._buffer[n].common.label_seq._buffer =
              ddsrt_memdup (xt->_u.union_type.members.seq[n].label_seq._buffer,
                            xt->_u.union_type.members.seq[n].label_seq._length * sizeof (*xt->_u.union_type.members.seq[n].label_seq._buffer));
            munion->member_seq._buffer[n].common.label_seq._release = true;
          } else {
            munion->member_seq._buffer[n].common.label_seq._buffer = NULL;
            munion->member_seq._buffer[n].common.label_seq._release = false;
          }
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
        xt_typeid_gen_emit_typeid (context, &xt->_u.seq.c.element_type->xt, &mto->_u.sequence_type.element.common.type, DDSI_TYPEID_KIND_MINIMAL);
        mto->_u.sequence_type.collection_flag = xt->_u.seq.c.flags;
        mto->_u.sequence_type.element.common.element_flags = xt->_u.seq.c.element_flags;
        mto->_u.sequence_type.header.common.bound = xt->_u.seq.bound;
        break;
      case DDS_XTypes_TK_ARRAY:
        xt_typeid_gen_emit_typeid (context, &xt->_u.array.c.element_type->xt, &mto->_u.array_type.element.common.type, DDSI_TYPEID_KIND_MINIMAL);
        mto->_u.array_type.element.common.element_flags = xt->_u.array.c.element_flags;
        xt_lbounds_dup (&mto->_u.array_type.header.common.bound_seq, &xt->_u.array.bounds);
        break;
      case DDS_XTypes_TK_MAP:
        xt_typeid_gen_emit_typeid (context, &xt->_u.map.c.element_type->xt, &mto->_u.map_type.element.common.type, DDSI_TYPEID_KIND_MINIMAL);
        mto->_u.map_type.element.common.element_flags = xt->_u.map.c.element_flags;
        xt_typeid_gen_emit_typeid (context, &xt->_u.map.key_type->xt, &mto->_u.map_type.key.common.type, DDSI_TYPEID_KIND_MINIMAL);
        mto->_u.map_type.key.common.element_flags = xt->_u.map.key_flags;
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
        calias->alias_flags = xt->_u.alias.flags;
        get_type_detail (&calias->header.detail, &xt->_u.alias.detail);
        xt_typeid_gen_emit_typeid (context, &xt->_u.alias.related_type->xt, &calias->body.common.related_type, DDSI_TYPEID_KIND_COMPLETE);
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
          xt_typeid_gen_emit_typeid (context, &xt->_u.structure.base_type->xt, &cstruct->header.base_type, DDSI_TYPEID_KIND_COMPLETE);

        get_type_detail (&cstruct->header.detail, &xt->_u.structure.detail);
        cstruct->member_seq._buffer = ddsrt_malloc (xt->_u.structure.members.length * sizeof (*cstruct->member_seq._buffer));
        cstruct->member_seq._length = xt->_u.structure.members.length;
        cstruct->member_seq._release = true;
        for (uint32_t n = 0; n < xt->_u.structure.members.length; n++)
        {
          cstruct->member_seq._buffer[n].common.member_id = xt->_u.structure.members.seq[n].id;
          cstruct->member_seq._buffer[n].common.member_flags = xt->_u.structure.members.seq[n].flags;
          xt_typeid_gen_emit_typeid (context, &xt->_u.structure.members.seq[n].type->xt, &cstruct->member_seq._buffer[n].common.member_type_id, DDSI_TYPEID_KIND_COMPLETE);
          get_member_detail (&cstruct->member_seq._buffer[n].detail, &xt->_u.structure.members.seq[n].detail);
        }
        break;
      }
      case DDS_XTypes_TK_UNION:
      {
        struct DDS_XTypes_CompleteUnionType *cunion = &cto->_u.union_type;
        cunion->union_flags = xt->_u.union_type.flags;
        get_type_detail (&cunion->header.detail, &xt->_u.union_type.detail);
        xt_typeid_gen_emit_typeid (context, &xt->_u.union_type.disc_type->xt, &cunion->discriminator.common.type_id, DDSI_TYPEID_KIND_COMPLETE);
        cunion->discriminator.common.member_flags = xt->_u.union_type.disc_flags;
        if (xt->_u.union_type.disc_annotations.ann_builtin)
        {
          cunion->discriminator.ann_builtin = ddsrt_calloc (1, sizeof (*cunion->discriminator.ann_builtin));
          DDS_XTypes_AppliedBuiltinTypeAnnotations_copy (cunion->discriminator.ann_builtin, xt->_u.union_type.disc_annotations.ann_builtin);
        }
        if (xt->_u.union_type.disc_annotations.ann_custom)
        {
          cunion->discriminator.ann_custom = ddsrt_calloc (1, sizeof (*cunion->discriminator.ann_custom));
          DDS_XTypes_AppliedAnnotationSeq_copy (cunion->discriminator.ann_custom, xt->_u.union_type.disc_annotations.ann_custom);
        }
        cunion->member_seq._buffer = ddsrt_malloc (xt->_u.union_type.members.length * sizeof (*cunion->member_seq._buffer));
        cunion->member_seq._length = cunion->member_seq._maximum = xt->_u.union_type.members.length;
        cunion->member_seq._release = true;
        for (uint32_t n = 0; n < xt->_u.union_type.members.length; n++)
        {
          cunion->member_seq._buffer[n].common.member_id = xt->_u.union_type.members.seq[n].id;
          cunion->member_seq._buffer[n].common.member_flags = xt->_u.union_type.members.seq[n].flags;
          xt_typeid_gen_emit_typeid (context, &xt->_u.union_type.members.seq[n].type->xt, &cunion->member_seq._buffer[n].common.type_id, DDSI_TYPEID_KIND_COMPLETE);
          cunion->member_seq._buffer[n].common.label_seq._length = xt->_u.union_type.members.seq[n].label_seq._length;
          if (cunion->member_seq._buffer[n].common.label_seq._length > 0) {
            cunion->member_seq._buffer[n].common.label_seq._buffer =
              ddsrt_memdup (xt->_u.union_type.members.seq[n].label_seq._buffer,
                            xt->_u.union_type.members.seq[n].label_seq._length * sizeof (*xt->_u.union_type.members.seq[n].label_seq._buffer));
            cunion->member_seq._buffer[n].common.label_seq._release = true;
          } else {
            cunion->member_seq._buffer[n].common.label_seq._buffer = NULL;
            cunion->member_seq._buffer[n].common.label_seq._release = false;
          }
          get_member_detail (&cunion->member_seq._buffer[n].detail, &xt->_u.union_type.members.seq[n].detail);
        }
        break;
      }
      case DDS_XTypes_TK_BITSET:
      {
        struct DDS_XTypes_CompleteBitsetType *cbitset = &cto->_u.bitset_type;
        cbitset->bitset_flags = xt->_u.bitset.flags;
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
        xt_typeid_gen_emit_typeid (context, &xt->_u.seq.c.element_type->xt, &cto->_u.sequence_type.element.common.type, DDSI_TYPEID_KIND_COMPLETE);
        cto->_u.sequence_type.collection_flag = xt->_u.seq.c.flags;
        cto->_u.sequence_type.header.common.bound = xt->_u.seq.bound;
        cto->_u.sequence_type.header.detail = ddsrt_calloc (1, sizeof (*cto->_u.sequence_type.header.detail));
        get_type_detail (cto->_u.sequence_type.header.detail, &xt->_u.seq.c.detail);
        cto->_u.sequence_type.element.common.element_flags = xt->_u.seq.c.element_flags;
        get_element_detail (&cto->_u.sequence_type.element.detail, &xt->_u.seq.c.element_annotations);
        break;
      case DDS_XTypes_TK_ARRAY:
        xt_typeid_gen_emit_typeid (context, &xt->_u.array.c.element_type->xt, &cto->_u.array_type.element.common.type, DDSI_TYPEID_KIND_COMPLETE);
        cto->_u.array_type.element.common.element_flags = xt->_u.array.c.element_flags;
        xt_lbounds_dup (&cto->_u.array_type.header.common.bound_seq, &xt->_u.array.bounds);
        get_type_detail (&cto->_u.array_type.header.detail, &xt->_u.array.c.detail);
        get_element_detail (&cto->_u.array_type.element.detail, &xt->_u.array.c.element_annotations);
        break;
      case DDS_XTypes_TK_MAP:
        xt_typeid_gen_emit_typeid (context, &xt->_u.map.c.element_type->xt, &cto->_u.map_type.element.common.type, DDSI_TYPEID_KIND_COMPLETE);
        cto->_u.map_type.element.common.element_flags = xt->_u.map.c.element_flags;
        xt_typeid_gen_emit_typeid (context, &xt->_u.map.key_type->xt, &cto->_u.map_type.key.common.type, DDSI_TYPEID_KIND_COMPLETE);
        cto->_u.map_type.key.common.element_flags = xt->_u.map.key_flags;
        cto->_u.map_type.header.common.bound = xt->_u.map.bound;
        cto->_u.map_type.header.detail = ddsrt_calloc (1, sizeof (*cto->_u.map_type.header.detail));
        get_type_detail (cto->_u.map_type.header.detail, &xt->_u.map.c.detail);
        get_element_detail (&cto->_u.map_type.element.detail, &xt->_u.map.c.element_annotations);
        get_element_detail (&cto->_u.map_type.key.detail, &xt->_u.map.key_annotations);
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

ddsrt_nonnull_all
void ddsi_xt_get_typeobject_kind_impl (const struct xt_type *xt, struct DDS_XTypes_TypeObject *to, ddsi_typeid_kind_t kind)
{
  struct xt_typeid_gen_context context;
  if (xt_typeid_gen_context_init (&context, xt, kind) == DDS_RETCODE_OK)
    xt_typeid_gen_finalize_sccs (&context);
  xt_typeid_gen_emit_typeobject (&context, xt, to, kind);
  xt_typeid_gen_context_fini (&context);
}

ddsrt_nonnull_all
void ddsi_xt_get_typeobject_impl (const struct xt_type *xt, struct DDS_XTypes_TypeObject *to)
{
  ddsi_xt_get_typeobject_kind_impl (xt, to, xt->kind);
}

ddsrt_nonnull_all
void ddsi_xt_get_typeobject (const struct xt_type *xt, ddsi_typeobj_t *to)
{
  ddsi_xt_get_typeobject_impl (xt, &to->x);
}
