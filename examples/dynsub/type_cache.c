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

#include "dds/dds.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"

#include "dynsub.h"

// TypeObjects can (and often do) refer to other types via an opaque id.  We don't want to have to request
// the corresponding type object every time we need it (it can be quite costly) and so have cache them in
// our own hash table.  For the hash table implementation we use one that's part of the implementation of
// Cyclone DDS.  It is *not* part of the API, it is simply a convenient solution for a simple PoC.
#include "dds/ddsrt/hopscotch.h"

// For convenience, type caches are globals
static struct ddsrt_hh *type_hashid_map;
static struct ddsrt_hh *typecache;

// Hash table requires a hash function and an equality test.  The key in the hash table is the address
// of the type object or type identifier.  The hash function distinguishes between 32-bit and 64-bit
// pointers, the equality test can simply use pointer equality.
static uint32_t type_hashid_map_hash (const void *vinfo)
{
  const struct type_hashid_map *info = vinfo;
  uint32_t h;
  memcpy (&h, info->id, sizeof (h));
  return h;
}

static bool type_hashid_map_equal (const void *va, const void *vb)
{
  const struct type_hashid_map *a = va;
  const struct type_hashid_map *b = vb;
  return memcmp (a->id, b->id, sizeof (a->id)) == 0;
}

static void type_hashid_map_init (void)
{
  type_hashid_map = ddsrt_hh_new (1, type_hashid_map_hash, type_hashid_map_equal);
}

static struct type_hashid_map * type_hashid_map_lookup (struct type_hashid_map *templ)
{
  return ddsrt_hh_lookup (type_hashid_map, templ);
}

static void type_hashid_map_add (struct type_hashid_map *info)
{
  ddsrt_hh_add (type_hashid_map, info);
}

static void free_type_hashid_map (void *vinfo, void *varg)
{
  (void) varg;
  free (vinfo);
}

static void type_hashid_map_free (void)
{
  ddsrt_hh_enum (type_hashid_map, free_type_hashid_map, NULL);
  ddsrt_hh_free (type_hashid_map);
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

void type_cache_init (void)
{
  typecache = ddsrt_hh_new (1, typecache_hash, typecache_equal);
  type_hashid_map_init ();
}

struct typeinfo *type_cache_lookup (struct typeinfo *templ)
{
  return ddsrt_hh_lookup (typecache, templ);
}

void type_cache_add (struct typeinfo *info)
{
  ddsrt_hh_add (typecache, info);
}

static void free_typeinfo (void *vinfo, void *varg)
{
  struct typeinfo *info = vinfo;
  (void) varg;
  if (info->release)
    dds_free_typeobj ((dds_typeobj_t *) info->release);
  free (info);
}

void type_cache_free (void)
{
  ddsrt_hh_enum (typecache, free_typeinfo, NULL);
  ddsrt_hh_free (typecache);
  type_hashid_map_free ();
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
      // FLOAT128
    case CASE(INT8, int8_t);
    case CASE(UINT8, uint8_t);
    case CASE(CHAR8, int8_t);
    case CASE(CHAR16, uint16_t);
    case CASE(STRING8, unsigned char *);
    case CASE(STRING16, uint16_t *);
#undef CASE
  }
  return false;
}

struct type_hashid_map *lookup_hashid (const DDS_XTypes_EquivalenceHash hashid)
{
  struct type_hashid_map templ, *info;
  memcpy (templ.id, hashid, sizeof (templ.id));
  if ((info = type_hashid_map_lookup (&templ)) == NULL)
    abort ();
  return info;
}

const DDS_XTypes_CompleteTypeObject *get_complete_typeobj_for_hashid (const DDS_XTypes_EquivalenceHash hashid)
{
  struct type_hashid_map *info;
  if ((info = lookup_hashid (hashid)) == NULL)
    abort ();
  return &info->typeobj->_u.complete;
}

static void build_typecache_ti (const DDS_XTypes_TypeIdentifier *typeid, size_t *align, size_t *size)
{
  if (build_typecache_simple (typeid->_d, align, size))
    return;
  switch (typeid->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING8_LARGE: {
      uint32_t bound;
      if (typeid->_d == DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL) {
        bound = typeid->_u.string_sdefn.bound;
      } else {
        bound = typeid->_u.string_ldefn.bound;
      }
      if (bound == 0) {
        *align = _Alignof (unsigned char *);
        *size = sizeof (unsigned char *);
      } else {
        *align = 1;
        *size = bound;
      }
      break;
    }
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE: {
      const DDS_XTypes_TypeIdentifier *et;
      if (typeid->_d == DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL) {
        et = typeid->_u.seq_sdefn.element_identifier;
      } else {
        et = typeid->_u.seq_ldefn.element_identifier;
      }
      size_t a, s;
      build_typecache_ti (et, &a, &s);
      *align = _Alignof (dds_sequence_t);
      *size = sizeof (dds_sequence_t);
      break;
    }
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
      build_typecache_ti (et, &a, &s);
      *align = a;
      *size = bound * s;
      break;
    }
    case DDS_XTypes_EK_COMPLETE: {
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeid } }, *info;
      if ((info = type_cache_lookup (&templ)) != NULL) {
        *align = info->align;
        *size = info->size;
      } else {
        const DDS_XTypes_CompleteTypeObject *tobj = get_complete_typeobj_for_hashid (typeid->_u.equivalence_hash);
        build_typecache_to (tobj, align, size);
        info = malloc (sizeof (*info));
        assert (info);
        *info = (struct typeinfo){ .key = { .key = (uintptr_t) typeid }, .typeobj = tobj, .release = NULL, .align = *align, .size = *size };
        type_cache_add (info);
      }
      break;
    }
    default:
      printf ("type id discriminant %u encountered, sorry\n", (unsigned) typeid->_d);
      abort ();
  }
}

void build_typecache_to (const DDS_XTypes_CompleteTypeObject *typeobj, size_t *align, size_t *size)
{
  if (build_typecache_simple (typeobj->_d, size, align))
    return;
  switch (typeobj->_d)
  {
    case DDS_XTypes_TK_ALIAS: {
      const DDS_XTypes_CompleteAliasType *x = &typeobj->_u.alias_type;
      build_typecache_ti (&x->body.common.related_type, align, size);
      break;
    }
    case DDS_XTypes_TK_ENUM: {
      const DDS_XTypes_CompleteEnumeratedType *x = &typeobj->_u.enumerated_type;
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info;
      if ((info = ddsrt_hh_lookup (typecache, &templ)) != NULL) {
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
        info = malloc (sizeof (*info));
        assert (info);
        *info = (struct typeinfo){ .key = { .key = (uintptr_t) typeobj }, .typeobj = typeobj, .release = NULL, .align = *align, .size = *size };
        ddsrt_hh_add (typecache, info);
      }
      break;
    }
    case DDS_XTypes_TK_BITMASK: {
      const DDS_XTypes_CompleteBitmaskType *x = &typeobj->_u.bitmask_type;
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info;
      if ((info = ddsrt_hh_lookup (typecache, &templ)) != NULL) {
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
        info = malloc (sizeof (*info));
        assert (info);
        *info = (struct typeinfo){ .key = { .key = (uintptr_t) typeobj }, .typeobj = typeobj, .release = NULL, .align = *align, .size = *size };
        ddsrt_hh_add (typecache, info);
      }
      break;
    }
    case DDS_XTypes_TK_SEQUENCE: {
      const DDS_XTypes_CompleteSequenceType *x = &typeobj->_u.sequence_type;
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info;
      if ((info = ddsrt_hh_lookup (typecache, &templ)) != NULL) {
        *align = info->align;
        *size = info->size;
      } else {
        size_t a, s;
        build_typecache_ti (&x->element.common.type, &a, &s);
        *align = a;
        *size = s;
        info = malloc (sizeof (*info));
        assert (info);
        *info = (struct typeinfo){ .key = { .key = (uintptr_t) typeobj }, .typeobj = typeobj, .release = NULL, .align = *align, .size = *size };
        ddsrt_hh_add (typecache, info);
      }
      break;
    }
    case DDS_XTypes_TK_STRUCTURE: {
      const DDS_XTypes_CompleteStructType *t = &typeobj->_u.struct_type;
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info;
      if ((info = ddsrt_hh_lookup (typecache, &templ)) != NULL) {
        *align = info->align;
        *size = info->size;
      } else {
        build_typecache_ti (&t->header.base_type, align, size);
        for (uint32_t i = 0; i < t->member_seq._length; i++)
        {
          const DDS_XTypes_CompleteStructMember *m = &t->member_seq._buffer[i];
          size_t a, s;
          build_typecache_ti (&m->common.member_type_id, &a, &s);
          if (m->common.member_flags & DDS_XTypes_IS_OPTIONAL) {
            a = _Alignof (void *); s = sizeof (void *);
          }
          if (a > *align)
            *align = a;
          if (*size % a)
            *size += a - (*size % a);
          *size += s;
        }
        if (*size % *align)
          *size += *align - (*size % *align);
        info = malloc (sizeof (*info));
        assert (info);
        *info = (struct typeinfo){ .key = { .key = (uintptr_t) typeobj }, .typeobj = typeobj, .release = NULL, .align = *align, .size = *size };
        ddsrt_hh_add (typecache, info);
      }
      break;
    }
    case DDS_XTypes_TK_UNION: {
      const DDS_XTypes_CompleteUnionType *t = &typeobj->_u.union_type;
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info;
      if ((info = ddsrt_hh_lookup (typecache, &templ)) != NULL) {
        *align = info->align;
        *size = info->size;
      } else {
        const DDS_XTypes_CompleteDiscriminatorMember *disc = &t->discriminator;
        size_t disc_align, disc_size;
        build_typecache_ti (&disc->common.type_id, &disc_align, &disc_size);
        *align = 1; *size = 0;
        for (uint32_t i = 0; i < t->member_seq._length; i++)
        {
          const DDS_XTypes_CompleteUnionMember *m = &t->member_seq._buffer[i];
          size_t a, s;
          build_typecache_ti (&m->common.type_id, &a, &s);
          if (a > *align)
            *align = a;
          if (s > *size)
            *size = s;
        }
        // FIXME: check this ...
        if (*size % *align)
          *size += *align - (*size % *align);
        if (*align > disc_size)
          disc_size = *align;
        *size += disc_size;
        if (disc_align > *align)
          *align = disc_align;
        if (*size % *align)
          *size += *align - (*size % *align);
        info = malloc (sizeof (*info));
        assert (info);
        *info = (struct typeinfo){ .key = { .key = (uintptr_t) typeobj }, .typeobj = typeobj, .release = NULL, .align = *align, .size = *size };
        ddsrt_hh_add (typecache, info);
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

static bool load_deps_to (dds_entity_t participant, const DDS_XTypes_CompleteTypeObject *typeobj);

static bool load_deps_ti (dds_entity_t participant, const DDS_XTypes_TypeIdentifier *typeid)
{
  if (load_deps_simple (typeid->_d))
    return true;
  switch (typeid->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING8_LARGE:
      return true;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      return load_deps_ti (participant, typeid->_u.seq_sdefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      return load_deps_ti (participant, typeid->_u.seq_ldefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
      return load_deps_ti (participant, typeid->_u.array_sdefn.element_identifier);
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      return load_deps_ti (participant, typeid->_u.array_ldefn.element_identifier);
    case DDS_XTypes_EK_COMPLETE: {
      struct type_hashid_map templ, *info;
      memcpy (templ.id, typeid->_u.equivalence_hash, sizeof (templ.id));
      if (type_hashid_map_lookup (&templ) != NULL)
        return true;
      else
      {
        dds_typeobj_t *typeobj;
        if (dds_get_typeobj (participant, (const dds_typeid_t *) typeid, 0, &typeobj) < 0)
          return load_deps_failed ();
        DDS_XTypes_TypeObject * const xtypeobj = (DDS_XTypes_TypeObject *) typeobj;
        info = malloc (sizeof (*info));
        assert (info);
        memcpy (info->id, typeid->_u.equivalence_hash, sizeof (info->id));
        info->typeobj = xtypeobj;
        info->lineno = 0;
        type_hashid_map_add (info);
        return load_deps_to (participant, &xtypeobj->_u.complete);
      }
    }
    default: {
      printf ("type id discriminant %u encountered, sorry\n", (unsigned) typeid->_d);
      abort ();
      return load_deps_failed ();
    }
  }
}

static bool load_deps_to (dds_entity_t participant, const DDS_XTypes_CompleteTypeObject *typeobj)
{
  if (load_deps_simple (typeobj->_d))
    return true;
  switch (typeobj->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      return load_deps_ti (participant, &typeobj->_u.alias_type.body.common.related_type);
    case DDS_XTypes_TK_ENUM:
    case DDS_XTypes_TK_BITMASK:
      return true;
    case DDS_XTypes_TK_SEQUENCE:
      return load_deps_ti (participant, &typeobj->_u.sequence_type.element.common.type);
    case DDS_XTypes_TK_STRUCTURE: {
      const DDS_XTypes_CompleteStructType *t = &typeobj->_u.struct_type;
      if (!load_deps_ti (participant, &t->header.base_type))
        return load_deps_failed ();
      for (uint32_t i = 0; i < t->member_seq._length; i++) {
        if (!load_deps_ti (participant, &t->member_seq._buffer[i].common.member_type_id))
          return load_deps_failed ();
      }
      return true;
    }
    case DDS_XTypes_TK_UNION: {
      const DDS_XTypes_CompleteUnionType *t = &typeobj->_u.union_type;
      if (!load_deps_ti (participant, &t->discriminator.common.type_id))
        return load_deps_failed ();
      for (uint32_t i = 0; i < t->member_seq._length; i++) {
        if (!load_deps_ti (participant, &t->member_seq._buffer[i].common.type_id))
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

DDS_XTypes_TypeObject *load_type_with_deps (dds_entity_t participant, const dds_typeinfo_t *typeinfo)
{
  DDS_XTypes_TypeInformation const * const xtypeinfo = (DDS_XTypes_TypeInformation *) typeinfo;
  if (!load_deps_ti (participant, &xtypeinfo->complete.typeid_with_size.type_id))
    return NULL;
  struct type_hashid_map templ, *info;
  memcpy (templ.id, &xtypeinfo->complete.typeid_with_size.type_id._u.equivalence_hash, sizeof (templ.id));
  if ((info = type_hashid_map_lookup (&templ)) == NULL)
    return NULL;
  return (DDS_XTypes_TypeObject *) info->typeobj;
}
