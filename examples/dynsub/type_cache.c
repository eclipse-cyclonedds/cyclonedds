// Copyright(c) 2022 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <wchar.h>
#include <inttypes.h>

#include "dds/dds.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/mh3.h"

#include "type_cache.h"
#include "print_type.h"

// Hash table requires a hash function and an equality test.  The key in the hash table is the address
// of the type object or type identifier.  The hash function distinguishes between 32-bit and 64-bit
// pointers, the equality test can simply use pointer equality.
static uint32_t type_identifier_hash (const DDS_XTypes_TypeIdentifier *id)
{
  uint32_t h = ddsrt_mh3 (&id->_d, sizeof (id->_d), 0);
  switch (id->_d)
  {
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_EK_MINIMAL:
      return ddsrt_mh3 (id->_u.equivalence_hash, sizeof (id->_u.equivalence_hash), h);
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT: {
      const DDS_XTypes_StronglyConnectedComponentId *scc = &id->_u.sc_component_id;
      h = ddsrt_mh3 (&scc->sc_component_id._d, sizeof (scc->sc_component_id._d), h);
      h = ddsrt_mh3 (scc->sc_component_id._u.hash, sizeof (scc->sc_component_id._u.hash), h);
      h = ddsrt_mh3 (&scc->scc_length, sizeof (scc->scc_length), h);
      return ddsrt_mh3 (&scc->scc_index, sizeof (scc->scc_index), h);
    }
    default:
      return h;
  }
}

static bool type_identifier_equal (const DDS_XTypes_TypeIdentifier *a, const DDS_XTypes_TypeIdentifier *b)
{
  if (a->_d != b->_d)
    return false;
  switch (a->_d)
  {
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_EK_MINIMAL:
      return memcmp (a->_u.equivalence_hash, b->_u.equivalence_hash, sizeof (a->_u.equivalence_hash)) == 0;
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT:
      return a->_u.sc_component_id.sc_component_id._d == b->_u.sc_component_id.sc_component_id._d
          && memcmp (a->_u.sc_component_id.sc_component_id._u.hash, b->_u.sc_component_id.sc_component_id._u.hash, sizeof (a->_u.sc_component_id.sc_component_id._u.hash)) == 0
          && a->_u.sc_component_id.scc_length == b->_u.sc_component_id.scc_length
          && a->_u.sc_component_id.scc_index == b->_u.sc_component_id.scc_index;
    default:
      return false;
  }
}

static uint32_t type_hashid_map_hash (const void *vinfo)
{
  const struct type_hashid_map *info = vinfo;
  return type_identifier_hash (&info->id);
}

static bool type_hashid_map_equal (const void *va, const void *vb)
{
  const struct type_hashid_map *a = va;
  const struct type_hashid_map *b = vb;
  return type_identifier_equal (&a->id, &b->id);
}

void type_hashid_map_add (struct type_cache *tc, struct  type_hashid_map *info)
{
  ddsrt_hh_add (tc->thm, info);
}

void type_hashid_map_init_id (struct type_hashid_map *info, const DDS_XTypes_TypeIdentifier *typeid)
{
  memset (info, 0, sizeof (*info));
  info->id = *typeid;
}

void type_hashid_map_init_hashid (struct type_hashid_map *info, DDS_XTypes_EquivalenceKind kind, const DDS_XTypes_EquivalenceHash hashid)
{
  memset (info, 0, sizeof (*info));
  info->id._d = kind;
  memcpy (info->id._u.equivalence_hash, hashid, sizeof (info->id._u.equivalence_hash));
}

// Hash table requires a hash function and an equality test.  The key in the hash table is the address
// of the type object or type identifier.  The hash function distinguishes between 32-bit and 64-bit
// pointers, the equality test can simply use pointer equality.
static uint32_t typecache_hash (const void *vinfo)
{
  const struct typeinfo *info = vinfo;
  if (sizeof (uintptr_t) == 4)
    return (uint32_t) (((info->key.u32[0] + UINT64_C (16292676669999574021)) * UINT64_C (10242350189706880077)) >> 32);
  else
    return (uint32_t) (((info->key.u32[0] + UINT64_C (16292676669999574021)) * (info->key.u32[1] + UINT64_C (10242350189706880077))) >> 32);
}

static bool typecache_equal (const void *va, const void *vb)
{
  const struct typeinfo *a = va;
  const struct typeinfo *b = vb;
  return a->key.key == b->key.key;
}

struct type_cache *type_cache_new (void)
{
  struct type_cache *tc = ddsrt_malloc (sizeof (*tc));
  tc->tc = ddsrt_hh_new (1, typecache_hash, typecache_equal);
  tc->thm = ddsrt_hh_new (1, type_hashid_map_hash, type_hashid_map_equal);
  return tc;
}

static void free_type_hashid_map (void *vinfo, void *varg)
{
  struct type_hashid_map *info = vinfo;
  (void) varg;
  if (info->release)
    dds_free_typeobj ((dds_typeobj_t *) info->release);
  ddsrt_free (info);
}

struct typeinfo *type_cache_lookup (struct type_cache *tc, struct typeinfo *templ)
{
  return ddsrt_hh_lookup (tc->tc, templ);
}

static void build_typecache_ti (struct type_cache *tc, const DDS_XTypes_TypeIdentifier *typeid, size_t *align, size_t *size);

void type_cache_add (struct type_cache *tc, struct typeinfo *info)
{
  ddsrt_hh_add (tc->tc, info);
}

struct typeinfo *type_cache_lookup_typeid (struct type_cache *tc, const DDS_XTypes_TypeIdentifier *typeid)
{
  if (typeid->_d != DDS_XTypes_EK_COMPLETE &&
      (typeid->_d != DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT ||
       typeid->_u.sc_component_id.sc_component_id._d != DDS_XTypes_EK_COMPLETE))
    abort ();
  struct typeinfo templ = { .key = { .key = (uintptr_t) typeid } };
  struct typeinfo *info = type_cache_lookup (tc, &templ);
  if (info == NULL)
  {
    size_t align, size;
    build_typecache_ti (tc, typeid, &align, &size);
    info = type_cache_lookup (tc, &templ);
  }
  if (info == NULL)
    abort ();
  return info;
}

struct typeinfo *type_cache_lookup_typeobj (struct type_cache *tc, const DDS_XTypes_CompleteTypeObject *typeobj)
{
  struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } };
  struct typeinfo *info = type_cache_lookup (tc, &templ);
  if (info == NULL)
  {
    size_t align, size;
    build_typecache_to (tc, typeobj, &align, &size);
    info = type_cache_lookup (tc, &templ);
  }
  if (info == NULL)
    abort ();
  return info;
}

static void free_typeinfo (void *vinfo, void *varg)
{
  struct typeinfo *info = vinfo;
  (void) varg;
  if (info->release)
    dds_free_typeobj ((dds_typeobj_t *) info->release);
  ddsrt_free (info);
}

void type_cache_free (struct type_cache *tc)
{
  ddsrt_hh_enum (tc->tc, free_typeinfo, NULL);
  ddsrt_hh_free (tc->tc);
  ddsrt_hh_enum (tc->thm, free_type_hashid_map, NULL);
  ddsrt_hh_free (tc->thm);
  ddsrt_free (tc);
}

// Building the type cache: the TypeObjects come in a variety of formats (see the spec for the details,
// much is omitted here for simplicity), but it comes down to:
// - a TypeObject describing a "simple" type
// - a TypeObject describing any other type
// - a TypeIdentifier that actually is a "simple" type
// - a TypeIdentifier that references some type
// The two "simple" types can be factored out, resulting in a function for computing alignment and
// sizeof for a simple type and functions for the other cases when it turns out not to be a simple
// case.
//
// Beware that a lot of cases are missing!

static bool build_typecache_simple (const uint8_t disc, size_t *align, size_t *size)
{
  switch (disc)
  {
#define CASE(disc, type) DDS_XTypes_TK_##disc: \
  *align = _Alignof(type); \
  *size = sizeof(type); \
  return true
    case DDS_XTypes_TK_NONE:
      *align = 1;
      *size = 0; // FIXME: Better check this!
      return true;
    case CASE(BOOLEAN, uint8_t);
    case CASE(BYTE, uint8_t);
    case CASE(INT16, int16_t);
    case CASE(INT32, int32_t);
    case CASE(INT64, int64_t);
    case CASE(UINT16, uint16_t);
    case CASE(UINT32, uint32_t);
    case CASE(UINT64, uint64_t);
    case CASE(FLOAT32, float);
    case CASE(FLOAT64, double);
    case CASE(INT8, int8_t);
    case CASE(UINT8, uint8_t);
    case CASE(CHAR8, int8_t);
    case CASE(CHAR16, wchar_t);
    case CASE(STRING8, unsigned char *);
    case CASE(STRING16, wchar_t *);
#undef CASE
    case DDS_XTypes_TK_FLOAT128: // FIXME:
      *align = 8;
      *size = 16;
      return true;
  }
  return false;
}

static bool is_pointer_member (DDS_XTypes_MemberFlag flags)
{
  return (flags & (DDS_XTypes_IS_OPTIONAL | DDS_XTypes_IS_EXTERNAL)) != 0;
}

static void build_typecache_pointer (size_t *align, size_t *size)
{
  *align = _Alignof (void *);
  *size = sizeof (void *);
}

static size_t align_size (size_t size, size_t align)
{
  return (size % align) ? size + align - (size % align) : size;
}

struct type_hashid_map *lookup_hashid (struct type_cache *tc, const DDS_XTypes_EquivalenceHash hashid)
{
  struct type_hashid_map templ, *info;
  type_hashid_map_init_hashid (&templ, DDS_XTypes_EK_COMPLETE, hashid);
  if ((info = ddsrt_hh_lookup (tc->thm, &templ)) == NULL)
    abort ();
  return info;
}

struct type_hashid_map *lookup_typeid (struct type_cache *tc, const DDS_XTypes_TypeIdentifier *typeid)
{
  struct type_hashid_map templ, *info;
  type_hashid_map_init_id (&templ, typeid);
  if ((info = ddsrt_hh_lookup (tc->thm, &templ)) == NULL)
    abort ();
  return info;
}

const DDS_XTypes_CompleteTypeObject *get_complete_typeobj_for_hashid (struct type_cache *tc, const DDS_XTypes_EquivalenceHash hashid)
{
  DDS_XTypes_TypeIdentifier typeid = { ._d = DDS_XTypes_EK_COMPLETE };
  memcpy (typeid._u.equivalence_hash, hashid, sizeof (typeid._u.equivalence_hash));
  return get_complete_typeobj_for_typeid (tc, &typeid);
}

const DDS_XTypes_MinimalTypeObject *get_minimal_typeobj_for_hashid (struct type_cache *tc, const DDS_XTypes_EquivalenceHash hashid)
{
  DDS_XTypes_TypeIdentifier typeid = { ._d = DDS_XTypes_EK_MINIMAL };
  memcpy (typeid._u.equivalence_hash, hashid, sizeof (typeid._u.equivalence_hash));
  return get_minimal_typeobj_for_typeid (tc, &typeid);
}

const DDS_XTypes_CompleteTypeObject *get_complete_typeobj_for_typeid (struct type_cache *tc, const DDS_XTypes_TypeIdentifier *typeid)
{
  struct type_hashid_map *info = lookup_typeid (tc, typeid);
  return &info->typeobj->_u.complete;
}

const DDS_XTypes_MinimalTypeObject *get_minimal_typeobj_for_typeid (struct type_cache *tc, const DDS_XTypes_TypeIdentifier *typeid)
{
  struct type_hashid_map *info = lookup_typeid (tc, typeid);
  return &info->typeobj->_u.minimal;
}

static void build_typecache_ti (struct type_cache *tc, const DDS_XTypes_TypeIdentifier *typeid, size_t *align, size_t *size)
{
  if (build_typecache_simple (typeid->_d, align, size))
    return;
  switch (typeid->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING8_LARGE: {
      uint32_t bound;
      if (typeid->_d == DDS_XTypes_TI_STRING8_SMALL) {
        bound = typeid->_u.string_sdefn.bound;
      } else {
        bound = typeid->_u.string_ldefn.bound;
      }
      if (bound == 0) {
        *align = _Alignof (unsigned char *);
        *size = sizeof (unsigned char *);
      } else {
        *align = 1;
        *size = bound + 1;
      }
      break;
    }
    case DDS_XTypes_TI_STRING16_SMALL:
    case DDS_XTypes_TI_STRING16_LARGE: {
      uint32_t bound;
      if (typeid->_d == DDS_XTypes_TI_STRING16_SMALL) {
        bound = typeid->_u.string_sdefn.bound;
      } else {
        bound = typeid->_u.string_ldefn.bound;
      }
      if (bound == 0) {
        *align = _Alignof (wchar_t *);
        *size = sizeof (wchar_t *);
      } else {
        *align = 1;
        *size = (bound + 1) * sizeof (wchar_t);
      }
      break;
    }
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      *align = _Alignof (dds_sequence_t);
      *size = sizeof (dds_sequence_t);
      break;
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE: {
      const DDS_XTypes_TypeIdentifier *et;
      uint32_t bound = 1;
      if (typeid->_d == DDS_XTypes_TI_PLAIN_ARRAY_SMALL) {
        et = typeid->_u.array_sdefn.element_identifier;
        for (uint32_t i = 0; i < typeid->_u.array_sdefn.array_bound_seq._length; i++)
          bound *= typeid->_u.array_sdefn.array_bound_seq._buffer[i];
      } else {
        et = typeid->_u.array_ldefn.element_identifier;
        for (uint32_t i = 0; i < typeid->_u.array_ldefn.array_bound_seq._length; i++)
          bound *= typeid->_u.array_ldefn.array_bound_seq._buffer[i];
      }
      size_t a, s;
      build_typecache_ti (tc, et, &a, &s);
      *align = a;
      *size = bound * align_size (s, a);
      break;
    }
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT: {
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeid } }, *info;
      if ((info = type_cache_lookup (tc, &templ)) != NULL) {
        *align = info->align;
        *size = info->size;
      } else {
        const DDS_XTypes_CompleteTypeObject *tobj = (typeid->_d == DDS_XTypes_EK_COMPLETE) ?
          get_complete_typeobj_for_hashid (tc, typeid->_u.equivalence_hash) :
          get_complete_typeobj_for_typeid (tc, typeid);
        build_typecache_to (tc, tobj, align, size);
        info = ddsrt_malloc (sizeof (*info));
        *info = (struct typeinfo){ .key = { .key = (uintptr_t) typeid }, .typeobj = tobj, .release = NULL, .align = *align, .size = *size };
        type_cache_add (tc, info);
      }
      break;
    }
    default:
      printf ("type id discriminant %u encountered, sorry\n", (unsigned) typeid->_d);
      abort ();
  }
}

void type_cache_typeid_align_size (struct type_cache *tc, const DDS_XTypes_TypeIdentifier *typeid, size_t *align, size_t *size)
{
  build_typecache_ti (tc, typeid, align, size);
}

size_t type_cache_union_data_offset (struct type_cache *tc, const DDS_XTypes_CompleteUnionType *type)
{
  size_t disc_align, disc_size, member_align = 1;
  type_cache_typeid_align_size (tc, &type->discriminator.common.type_id, &disc_align, &disc_size);
  (void) disc_align;
  for (uint32_t i = 0; i < type->member_seq._length; i++)
  {
    const DDS_XTypes_CompleteUnionMember *m = &type->member_seq._buffer[i];
    size_t align, size;
    if (is_pointer_member (m->common.member_flags))
      build_typecache_pointer (&align, &size);
    else
      type_cache_typeid_align_size (tc, &m->common.type_id, &align, &size);
    if (align > member_align)
      member_align = align;
  }
  if (disc_size % member_align)
    disc_size += member_align - (disc_size % member_align);
  return disc_size;
}

void build_typecache_to (struct type_cache *tc, const DDS_XTypes_CompleteTypeObject *typeobj, size_t *align, size_t *size)
{
  if (build_typecache_simple (typeobj->_d, align, size))
    return;
  switch (typeobj->_d)
  {
    case DDS_XTypes_TK_ALIAS: {
      const DDS_XTypes_CompleteAliasType *x = &typeobj->_u.alias_type;
      build_typecache_ti (tc, &x->body.common.related_type, align, size);
      break;
    }
    case DDS_XTypes_TK_ENUM: {
      const DDS_XTypes_CompleteEnumeratedType *x = &typeobj->_u.enumerated_type;
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info;
      if ((info = ddsrt_hh_lookup (tc->tc, &templ)) != NULL) {
        *align = info->align;
        *size = info->size;
      } else {
        if (x->header.common.bit_bound != 32)
        {
          printf ("unsupported enum bit-bound %u\n", x->header.common.bit_bound);
          abort ();
        }
        *align = sizeof (int);
        *size = sizeof (int);
        info = ddsrt_malloc (sizeof (*info));
        *info = (struct typeinfo){ .key = { .key = (uintptr_t) typeobj }, .typeobj = typeobj, .release = NULL, .align = *align, .size = *size };
        type_cache_add (tc, info);
      }
      break;
    }
    case DDS_XTypes_TK_BITMASK: {
      const DDS_XTypes_CompleteBitmaskType *x = &typeobj->_u.bitmask_type;
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info;
      if ((info = ddsrt_hh_lookup (tc->tc, &templ)) != NULL) {
        *align = info->align;
        *size = info->size;
      } else {
        if (x->header.common.bit_bound > 32)
          *align = *size = 8;
        else if (x->header.common.bit_bound > 16)
          *align = *size = 4;
        else if (x->header.common.bit_bound > 8)
          *align = *size = 2;
        else
          *align = *size = 1;
        info = ddsrt_malloc (sizeof (*info));
        *info = (struct typeinfo){ .key = { .key = (uintptr_t) typeobj }, .typeobj = typeobj, .release = NULL, .align = *align, .size = *size };
        type_cache_add (tc, info);
      }
      break;
    }
    case DDS_XTypes_TK_SEQUENCE: {
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info;
      if ((info = ddsrt_hh_lookup (tc->tc, &templ)) != NULL) {
        *align = info->align;
        *size = info->size;
      } else {
        *align = _Alignof (dds_sequence_t);
        *size = sizeof (dds_sequence_t);
        info = ddsrt_malloc (sizeof (*info));
        *info = (struct typeinfo){ .key = { .key = (uintptr_t) typeobj }, .typeobj = typeobj, .release = NULL, .align = *align, .size = *size };
        type_cache_add (tc, info);
      }
      break;
    }
    case DDS_XTypes_TK_STRUCTURE: {
      const DDS_XTypes_CompleteStructType *t = &typeobj->_u.struct_type;
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info;
      if ((info = ddsrt_hh_lookup (tc->tc, &templ)) != NULL) {
        *align = info->align;
        *size = info->size;
      } else {
        build_typecache_ti (tc, &t->header.base_type, align, size);
        for (uint32_t i = 0; i < t->member_seq._length; i++)
        {
          const DDS_XTypes_CompleteStructMember *m = &t->member_seq._buffer[i];
          size_t a, s;
          if (is_pointer_member (m->common.member_flags))
            build_typecache_pointer (&a, &s);
          else
            build_typecache_ti (tc, &m->common.member_type_id, &a, &s);
          if (a > *align)
            *align = a;
          *size = align_size (*size, a);
          *size += s;
        }
        *size = align_size (*size, *align);
        info = ddsrt_malloc (sizeof (*info));
        *info = (struct typeinfo){ .key = { .key = (uintptr_t) typeobj }, .typeobj = typeobj, .release = NULL, .align = *align, .size = *size };
        type_cache_add (tc, info);
      }
      break;
    }
    case DDS_XTypes_TK_UNION: {
      const DDS_XTypes_CompleteUnionType *t = &typeobj->_u.union_type;
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info;
      if ((info = ddsrt_hh_lookup (tc->tc, &templ)) != NULL) {
        *align = info->align;
        *size = info->size;
      } else {
        const DDS_XTypes_CompleteDiscriminatorMember *disc = &t->discriminator;
        size_t disc_align, disc_size;
        build_typecache_ti (tc, &disc->common.type_id, &disc_align, &disc_size);
        *align = 1; *size = 0;
        for (uint32_t i = 0; i < t->member_seq._length; i++)
        {
          const DDS_XTypes_CompleteUnionMember *m = &t->member_seq._buffer[i];
          size_t a, s;
          if (is_pointer_member (m->common.member_flags))
            build_typecache_pointer (&a, &s);
          else
            build_typecache_ti (tc, &m->common.type_id, &a, &s);
          if (a > *align)
            *align = a;
          if (s > *size)
            *size = s;
        }
        // FIXME: check this ...
        *size = align_size (*size, *align);
        if (*align > disc_size)
          disc_size = *align;
        *size += disc_size;
        if (disc_align > *align)
          *align = disc_align;
        *size = align_size (*size, *align);
        info = ddsrt_malloc (sizeof (*info));
        *info = (struct typeinfo){ .key = { .key = (uintptr_t) typeobj }, .typeobj = typeobj, .release = NULL, .align = *align, .size = *size };
        type_cache_add (tc, info);
      }
      break;
    }
    default: {
      printf ("type object discriminant %u encountered, sorry\n", (unsigned) typeobj->_d);
      abort ();
    }
  }
}

static bool load_deps_failed (void)
{
  return false;
}

static void load_deps_print_hash (FILE *fp, const DDS_XTypes_EquivalenceHash hash)
{
  for (uint32_t n = 0; n < sizeof (DDS_XTypes_EquivalenceHash); n++)
    fprintf (fp, "%02x", hash[n]);
}

static const char *load_deps_equivalence_kind_str (uint8_t kind)
{
  switch (kind)
  {
    case DDS_XTypes_EK_COMPLETE:
      return "COMPLETE";
    case DDS_XTypes_EK_MINIMAL:
      return "MINIMAL";
    default:
      return "UNKNOWN";
  }
}

static void load_deps_print_typeid (FILE *fp, const DDS_XTypes_TypeIdentifier *typeid)
{
  switch (typeid->_d)
  {
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_EK_MINIMAL:
      fprintf (fp, "%s ", load_deps_equivalence_kind_str (typeid->_d));
      load_deps_print_hash (fp, typeid->_u.equivalence_hash);
      break;
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT: {
      const DDS_XTypes_StronglyConnectedComponentId *scc = &typeid->_u.sc_component_id;
      fprintf (fp, "SCC_%s length=%"PRId32" index=%"PRId32" ",
        load_deps_equivalence_kind_str (scc->sc_component_id._d),
        scc->scc_length, scc->scc_index);
      load_deps_print_hash (fp, scc->sc_component_id._u.hash);
      break;
    }
    default:
      fprintf (fp, "type-id discriminant %u", (unsigned) typeid->_d);
      break;
  }
}

static bool load_deps_get_typeobj_failed (const DDS_XTypes_TypeIdentifier *typeid, dds_return_t rc)
{
  fprintf (stderr, "type_cache: dds_get_typeobj(timeout=0) failed for ");
  load_deps_print_typeid (stderr, typeid);
  fprintf (stderr, ": %s\n", dds_strretcode (rc));
  return load_deps_failed ();
}

static bool load_deps_simple (uint8_t disc)
{
  switch (disc)
  {
    case DDS_XTypes_TK_NONE:
    case DDS_XTypes_TK_BOOLEAN:
    case DDS_XTypes_TK_BYTE:
    case DDS_XTypes_TK_INT16:
    case DDS_XTypes_TK_INT32:
    case DDS_XTypes_TK_INT64:
    case DDS_XTypes_TK_UINT16:
    case DDS_XTypes_TK_UINT32:
    case DDS_XTypes_TK_UINT64:
    case DDS_XTypes_TK_FLOAT32:
    case DDS_XTypes_TK_FLOAT64:
    case DDS_XTypes_TK_FLOAT128:
    case DDS_XTypes_TK_INT8:
    case DDS_XTypes_TK_UINT8:
    case DDS_XTypes_TK_CHAR8:
    case DDS_XTypes_TK_CHAR16:
    case DDS_XTypes_TK_STRING8:
    case DDS_XTypes_TK_STRING16:
      return true;
    default:
      return false;
  }
}

static bool load_deps_to (struct type_cache *tc, dds_entity_t participant, const DDS_XTypes_CompleteTypeObject *typeobj);
static bool load_deps_to_min (struct type_cache *tc, dds_entity_t participant, const DDS_XTypes_MinimalTypeObject *typeobj);

static bool load_deps_ti (struct type_cache *tc, dds_entity_t participant, const DDS_XTypes_TypeIdentifier *typeid)
{
  if (load_deps_simple (typeid->_d))
    return true;
  switch (typeid->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING8_LARGE:
      return true;
    case DDS_XTypes_TI_STRING16_SMALL:
    case DDS_XTypes_TI_STRING16_LARGE:
      return true;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      return load_deps_ti (tc, participant, typeid->_u.seq_sdefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      return load_deps_ti (tc, participant, typeid->_u.seq_ldefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
      return load_deps_ti (tc, participant, typeid->_u.array_sdefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      return load_deps_ti (tc, participant, typeid->_u.array_ldefn.element_identifier);
    case DDS_XTypes_EK_COMPLETE: {
      struct type_hashid_map templ, *info;
      type_hashid_map_init_id (&templ, typeid);
      if (ddsrt_hh_lookup (tc->thm, &templ) != NULL)
        return true;
      else
      {
        dds_typeobj_t *typeobj;
        dds_return_t rc;
        if ((rc = dds_get_typeobj (participant, (const dds_typeid_t *) typeid, 0, &typeobj)) < 0)
          return load_deps_get_typeobj_failed (typeid, rc);
        DDS_XTypes_TypeObject * const xtypeobj = (DDS_XTypes_TypeObject *) typeobj;
        info = ddsrt_malloc (sizeof (*info));
        type_hashid_map_init_id (info, typeid);
        info->typeobj = xtypeobj;
        info->release = xtypeobj;
        info->lineno = 0;
        type_hashid_map_add (tc, info);
        return load_deps_to (tc, participant, &xtypeobj->_u.complete);
      }
    }
    case DDS_XTypes_EK_MINIMAL: {
      struct type_hashid_map templ, *info;
      type_hashid_map_init_id (&templ, typeid);
      if (ddsrt_hh_lookup (tc->thm, &templ) != NULL)
        return true;
      else
      {
        dds_typeobj_t *typeobj;
        dds_return_t rc;
        if ((rc = dds_get_typeobj (participant, (const dds_typeid_t *) typeid, 0, &typeobj)) < 0)
          return load_deps_get_typeobj_failed (typeid, rc);
        DDS_XTypes_TypeObject * const xtypeobj = (DDS_XTypes_TypeObject *) typeobj;
        info = ddsrt_malloc (sizeof (*info));
        type_hashid_map_init_id (info, typeid);
        info->typeobj = xtypeobj;
        info->release = xtypeobj;
        info->lineno = 0;
        type_hashid_map_add (tc, info);
        return load_deps_to_min (tc, participant, &xtypeobj->_u.minimal);
      }
    }
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT: {
      struct type_hashid_map templ, *info;
      type_hashid_map_init_id (&templ, typeid);
      if (ddsrt_hh_lookup (tc->thm, &templ) != NULL)
        return true;
      else
      {
        dds_typeobj_t *typeobj;
        dds_return_t rc;
        if ((rc = dds_get_typeobj (participant, (const dds_typeid_t *) typeid, 0, &typeobj)) < 0)
          return load_deps_get_typeobj_failed (typeid, rc);
        DDS_XTypes_TypeObject * const xtypeobj = (DDS_XTypes_TypeObject *) typeobj;
        info = ddsrt_malloc (sizeof (*info));
        type_hashid_map_init_id (info, typeid);
        info->typeobj = xtypeobj;
        info->release = xtypeobj;
        info->lineno = 0;
        type_hashid_map_add (tc, info);
        switch (typeid->_u.sc_component_id.sc_component_id._d)
        {
          case DDS_XTypes_EK_COMPLETE:
            return load_deps_to (tc, participant, &xtypeobj->_u.complete);
          case DDS_XTypes_EK_MINIMAL:
            return load_deps_to_min (tc, participant, &xtypeobj->_u.minimal);
          default:
            return load_deps_failed ();
        }
      }
    }
    default: {
      printf ("type id discriminant %u encountered, sorry\n", (unsigned) typeid->_d);
      abort ();
      return load_deps_failed ();
    }
  }
}

static bool load_deps_to (struct type_cache *tc, dds_entity_t participant, const DDS_XTypes_CompleteTypeObject *typeobj)
{
  if (load_deps_simple (typeobj->_d))
    return true;
  switch (typeobj->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      return load_deps_ti (tc, participant, &typeobj->_u.alias_type.body.common.related_type);
    case DDS_XTypes_TK_ENUM:
    case DDS_XTypes_TK_BITMASK:
      return true;
    case DDS_XTypes_TK_SEQUENCE:
      return load_deps_ti (tc, participant, &typeobj->_u.sequence_type.element.common.type);
    case DDS_XTypes_TK_STRUCTURE: {
      const DDS_XTypes_CompleteStructType *t = &typeobj->_u.struct_type;
      if (!load_deps_ti (tc, participant, &t->header.base_type))
        return load_deps_failed ();
      for (uint32_t i = 0; i < t->member_seq._length; i++) {
        if (!load_deps_ti (tc, participant, &t->member_seq._buffer[i].common.member_type_id))
          return load_deps_failed ();
      }
      return true;
    }
    case DDS_XTypes_TK_UNION: {
      const DDS_XTypes_CompleteUnionType *t = &typeobj->_u.union_type;
      if (!load_deps_ti (tc, participant, &t->discriminator.common.type_id))
        return load_deps_failed ();
      for (uint32_t i = 0; i < t->member_seq._length; i++) {
        if (!load_deps_ti (tc, participant, &t->member_seq._buffer[i].common.type_id))
          return load_deps_failed ();
      }
      return true;
    }
    default: {
      printf ("type object discriminant %u encountered, sorry\n", (unsigned) typeobj->_d);
      abort ();
      return load_deps_failed ();
    }
  }
}

static bool load_deps_to_min (struct type_cache *tc, dds_entity_t participant, const DDS_XTypes_MinimalTypeObject *typeobj)
{
  if (load_deps_simple (typeobj->_d))
    return true;
  switch (typeobj->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      return load_deps_ti (tc, participant, &typeobj->_u.alias_type.body.common.related_type);
    case DDS_XTypes_TK_ENUM:
    case DDS_XTypes_TK_BITMASK:
      return true;
    case DDS_XTypes_TK_SEQUENCE:
      return load_deps_ti (tc, participant, &typeobj->_u.sequence_type.element.common.type);
    case DDS_XTypes_TK_STRUCTURE: {
      const DDS_XTypes_MinimalStructType *t = &typeobj->_u.struct_type;
      if (!load_deps_ti (tc, participant, &t->header.base_type))
        return load_deps_failed ();
      for (uint32_t i = 0; i < t->member_seq._length; i++) {
        if (!load_deps_ti (tc, participant, &t->member_seq._buffer[i].common.member_type_id))
          return load_deps_failed ();
      }
      return true;
    }
    case DDS_XTypes_TK_UNION: {
      const DDS_XTypes_MinimalUnionType *t = &typeobj->_u.union_type;
      if (!load_deps_ti (tc, participant, &t->discriminator.common.type_id))
        return load_deps_failed ();
      for (uint32_t i = 0; i < t->member_seq._length; i++) {
        if (!load_deps_ti (tc, participant, &t->member_seq._buffer[i].common.type_id))
          return load_deps_failed ();
      }
      return true;
    }
    default: {
      printf ("type object discriminant %u encountered, sorry\n", (unsigned) typeobj->_d);
      abort ();
      return load_deps_failed ();
    }
  }
}

const DDS_XTypes_TypeObject *load_type_with_deps_impl (struct type_cache *tc, dds_entity_t participant, const DDS_XTypes_TypeInformation *xtypeinfo, struct ppc *ppc)
{
  const DDS_XTypes_TypeIdentifier *typeid = &xtypeinfo->complete.typeid_with_size.type_id;
  if (!load_deps_ti (tc, participant, typeid))
    return NULL;
  struct type_hashid_map templ, *info;
  type_hashid_map_init_id (&templ, typeid);
  if ((info = ddsrt_hh_lookup (tc->thm, &templ)) == NULL)
    return NULL;
  if (ppc)
    ppc_print_ti (tc, ppc, typeid);
  return (DDS_XTypes_TypeObject *) info->typeobj;
}

const DDS_XTypes_TypeObject *load_type_with_deps_min_impl (struct type_cache *tc, dds_entity_t participant, const DDS_XTypes_TypeInformation *xtypeinfo, struct ppc *ppc)
{
  const DDS_XTypes_TypeIdentifier *typeid = &xtypeinfo->minimal.typeid_with_size.type_id;
  if (!load_deps_ti (tc, participant, typeid))
    return NULL;
  struct type_hashid_map templ, *info;
  type_hashid_map_init_id (&templ, typeid);
  if ((info = ddsrt_hh_lookup (tc->thm, &templ)) == NULL)
    return NULL;
  if (ppc)
    ppc_print_ti (tc, ppc, typeid);
  return (DDS_XTypes_TypeObject *) info->typeobj;
}

const DDS_XTypes_TypeObject *load_type_with_deps (struct type_cache *tc, dds_entity_t participant, const dds_typeinfo_t *typeinfo, struct ppc *ppc)
{
  return load_type_with_deps_impl (tc, participant, (DDS_XTypes_TypeInformation *) typeinfo, ppc);
}

const DDS_XTypes_TypeObject *load_type_with_deps_min (struct type_cache *tc, dds_entity_t participant, const dds_typeinfo_t *typeinfo, struct ppc *ppc)
{
  return load_type_with_deps_min_impl (tc, participant, (DDS_XTypes_TypeInformation *) typeinfo, ppc);
}
