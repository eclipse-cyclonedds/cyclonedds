// Copyright(c) 2022 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

// Printing JSON: the TypeObject/TypeIdentifier handling follows the pattern used for building the type cache,
// except now it just looks up entries in the cache (that are always present).  We do pass a bit of context:
// - whether the sample is a "valid sample", that is: whether all fields are valid, or only the key fields
// - whether all fields in the path from the top-level had the "key" annotation set, because only in that case
//   is the field actually a key field
// The trouble with skipping non-key fields is that we still need to go over them to compute the offset of the
// key fields that follow it.  Of course it would be possible to store more information in the type cache, but
// that is left as an exercise to the reader.

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <wchar.h>

#include "dyntypelib.h"
#include "size_and_align.h"

void *dtl_align (unsigned char *base, size_t *off, size_t align, size_t size)
{
  if (*off % align)
    *off += align - (*off % align);
  const size_t o = *off;
  *off += size;
  return base + o;
}

bool dtl_simple_alignof_sizeof (const uint8_t disc, size_t *align, size_t *size)
{
#define CASE(type) *align = _Alignof (type); *size = sizeof (type); return true
  switch (disc)
  {
    case DDS_XTypes_TK_BOOLEAN: CASE(uint8_t);
    case DDS_XTypes_TK_CHAR8: CASE(int8_t);
    case DDS_XTypes_TK_CHAR16: CASE(wchar_t);
    case DDS_XTypes_TK_INT8: CASE(int16_t);
    case DDS_XTypes_TK_INT16: CASE(int16_t);
    case DDS_XTypes_TK_INT32: CASE(int32_t);
    case DDS_XTypes_TK_INT64: CASE(int64_t);
    case DDS_XTypes_TK_BYTE: CASE(uint8_t);
    case DDS_XTypes_TK_UINT8: CASE(uint8_t);
    case DDS_XTypes_TK_UINT16: CASE(uint16_t);
    case DDS_XTypes_TK_UINT32: CASE(uint32_t);
    case DDS_XTypes_TK_UINT64: CASE(uint64_t);
    case DDS_XTypes_TK_FLOAT32: CASE(float);
    case DDS_XTypes_TK_FLOAT64: CASE(double);
    case DDS_XTypes_TK_STRING8: CASE(char *);
    case DDS_XTypes_TK_STRING16: CASE(wchar_t *);
    case DDS_XTypes_TK_FLOAT128: // FIXME
      *align = 8;
      *size = 16;
      return true;
  }
  return false;
#undef CASE
}

size_t dtl_simple_size (const uint8_t disc)
{
  size_t a, s;
  if (!dtl_simple_alignof_sizeof (disc, &a, &s))
    return 0;
  else
    return s;
}

size_t dtl_simple_align (const uint8_t disc)
{
  size_t a, s;
  if (!dtl_simple_alignof_sizeof (disc, &a, &s))
    return 0;
  else
    return a;
}

void *dtl_advance_simple (unsigned char *base, size_t *off, const uint8_t disc)
{
  size_t a, s;
  if (!dtl_simple_alignof_sizeof (disc, &a, &s))
    return NULL;
  else
    return dtl_align (base, off, a, s);
}

bool dtl_is_unbounded_string_ti (const DDS_XTypes_TypeIdentifier *typeid)
{
  if (typeid->_d == DDS_XTypes_TK_STRING8 || typeid->_d == DDS_XTypes_TK_STRING16)
    return true;
  if (typeid->_d == DDS_XTypes_TI_STRING8_SMALL || typeid->_d == DDS_XTypes_TI_STRING16_SMALL)
    return (typeid->_u.string_sdefn.bound == 0);
  if (typeid->_d == DDS_XTypes_TI_STRING8_LARGE || typeid->_d == DDS_XTypes_TI_STRING16_LARGE)
    return (typeid->_u.string_ldefn.bound == 0);
  return false;
}

bool dtl_is_bounded_string_ti (const DDS_XTypes_TypeIdentifier *typeid)
{
  if (typeid->_d == DDS_XTypes_TI_STRING8_SMALL || typeid->_d == DDS_XTypes_TI_STRING16_SMALL)
    return (typeid->_u.string_sdefn.bound != 0);
  if (typeid->_d == DDS_XTypes_TI_STRING8_LARGE || typeid->_d == DDS_XTypes_TI_STRING16_LARGE)
    return (typeid->_u.string_ldefn.bound != 0);
  return false;
}

bool dtl_is_unbounded_string_to (const DDS_XTypes_CompleteTypeObject *typeobj)
{
  if (typeobj->_d == DDS_XTypes_TK_STRING8 || typeobj->_d == DDS_XTypes_TK_STRING16)
    return true;
  return false;
}

size_t dtl_bounded_string_bound_ti (const DDS_XTypes_TypeIdentifier *typeid)
{
  if (typeid->_d == DDS_XTypes_TI_STRING8_SMALL || typeid->_d == DDS_XTypes_TI_STRING16_SMALL)
    return typeid->_u.string_sdefn.bound;
  if (typeid->_d == DDS_XTypes_TI_STRING8_LARGE || typeid->_d == DDS_XTypes_TI_STRING16_LARGE)
    return typeid->_u.string_ldefn.bound;
  abort ();
}

void *dtl_advance_string_ti (unsigned char *base, size_t *off, const DDS_XTypes_TypeIdentifier *typeid)
{
  uint32_t bound;
  if (typeid->_d == DDS_XTypes_TI_STRING8_SMALL || typeid->_d == DDS_XTypes_TI_STRING16_SMALL)
    bound = typeid->_u.string_sdefn.bound;
  else
    bound = typeid->_u.string_ldefn.bound;
  if (bound == 0)
  {
    if (typeid->_d == DDS_XTypes_TI_STRING8_SMALL || typeid->_d == DDS_XTypes_TI_STRING8_LARGE)
      return dtl_advance_simple (base, off, DDS_XTypes_TK_STRING8);
    else
      return dtl_advance_simple (base, off, DDS_XTypes_TK_STRING16);
  }
  else
  {
    unsigned char *p;
    // advance call accounts for terminating 0
    if (typeid->_d == DDS_XTypes_TI_STRING8_SMALL || typeid->_d == DDS_XTypes_TI_STRING8_LARGE) {
      p = dtl_advance_simple (base, off, DDS_XTypes_TK_CHAR8);
      *off += bound;
    } else {
      p = dtl_advance_simple (base, off, DDS_XTypes_TK_CHAR16);
      *off += sizeof (wchar_t) * bound;
    }
    return p;
  }
}

void *dtl_advance_ti (struct dyntypelib *dtl, unsigned char *base, size_t *off, const DDS_XTypes_TypeIdentifier *typeid, bool is_opt_or_ext)
{
  // Advancing over an @optional or @external always means advancing over a pointer
  if (is_opt_or_ext)
    return dtl_align (base, off, _Alignof (void *), sizeof (void *));

  void *p = dtl_advance_simple (base, off, typeid->_d);
  if (p != NULL)
    return p;

  switch (typeid->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING8_LARGE:
    case DDS_XTypes_TI_STRING16_SMALL:
    case DDS_XTypes_TI_STRING16_LARGE:
      return dtl_advance_string_ti (base, off, typeid);

    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      return dtl_align (base, off, _Alignof (dds_sequence_t), sizeof (dds_sequence_t));

    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE: {
      const DDS_XTypes_TypeIdentifier *et;
      uint32_t n = 1;
      if (typeid->_d == DDS_XTypes_TI_PLAIN_ARRAY_SMALL) {
        et = typeid->_u.array_sdefn.element_identifier;
        for (uint32_t i = 0; i < typeid->_u.array_sdefn.array_bound_seq._length; i++)
          n *= typeid->_u.array_sdefn.array_bound_seq._buffer[i];
      } else {
        et = typeid->_u.array_ldefn.element_identifier;
        for (uint32_t i = 0; i < typeid->_u.array_ldefn.array_bound_seq._length; i++)
          n *= typeid->_u.array_ldefn.array_bound_seq._buffer[i];
      }
      p = dtl_advance_ti (dtl, base, off, et, false);
      if (n > 1)
      {
        size_t off1 = *off;
        (void) dtl_advance_ti (dtl, base, off, et, false);
        size_t elem_size_aligned = *off - off1;
        *off += elem_size_aligned * (n - 2);
      }
      return p;
    }

    case DDS_XTypes_EK_COMPLETE: {
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeid } }, *info = type_cache_lookup (dtl->typecache, &templ);
      return dtl_advance_to (dtl, base, off, info->typeobj, false);
    }
  }

  abort ();
  return NULL;
}

void *dtl_advance_to (struct dyntypelib *dtl, unsigned char *base, size_t *off, const DDS_XTypes_CompleteTypeObject *typeobj, bool is_opt_or_ext)
{
  // Advancing over an @optional or @external always means advancing over a pointer
  if (is_opt_or_ext)
    return dtl_align (base, off, _Alignof (void *), sizeof (void *));

  void *p = dtl_advance_simple (base, off, typeobj->_d);
  if (p != NULL)
    return p;

  switch (typeobj->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      return dtl_advance_ti (dtl, base, off, &typeobj->_u.alias_type.body.common.related_type, false);

    case DDS_XTypes_TK_SEQUENCE:
      return dtl_align (base, off, _Alignof (dds_sequence_t), sizeof (dds_sequence_t));

    case DDS_XTypes_TK_ENUM:
      return dtl_align (base, off, _Alignof (int), sizeof (int));

    case DDS_XTypes_TK_STRUCTURE:
    case DDS_XTypes_TK_UNION: {
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info = type_cache_lookup (dtl->typecache, &templ);
      return dtl_align (base, off, info->align, info->size);
    }
  }

  abort ();
  return NULL;
}

static size_t get_typeid_typeobj_size (struct dyntypelib *dtl, const uint8_t disc, void const * const key)
{
  size_t size = dtl_simple_size (disc);
  if (size != 0)
    return size;
  else
  {
    struct typeinfo templ = { .key = { .key = (uintptr_t) key } }, *info = type_cache_lookup (dtl->typecache, &templ);
    return info->size;
  }
}

static size_t get_typeid_typeobj_align (struct dyntypelib *dtl, const uint8_t disc, void const * const key)
{
  size_t size = dtl_simple_align (disc);
  if (size != 0)
    return size;
  else
  {
    struct typeinfo templ = { .key = { .key = (uintptr_t) key } }, *info = type_cache_lookup (dtl->typecache, &templ);
    return info->align;
  }
}

size_t dtl_get_typeid_size (struct dyntypelib *dtl, DDS_XTypes_TypeIdentifier const * const typeid)
{
  if (dtl_is_unbounded_string_ti (typeid))
    return sizeof (char *);
  else if (dtl_is_bounded_string_ti (typeid))
  {
    size_t es = (typeid->_d == DDS_XTypes_TI_STRING8_SMALL || typeid->_d == DDS_XTypes_TI_STRING8_LARGE) ? 1 : sizeof (wchar_t);
    return es * (1 + dtl_bounded_string_bound_ti (typeid));
  }
  else
  {
    return get_typeid_typeobj_size (dtl, typeid->_d, typeid);
  }
}

size_t dtl_get_typeid_align (struct dyntypelib *dtl, DDS_XTypes_TypeIdentifier const * const typeid)
{
  if (dtl_is_unbounded_string_ti (typeid))
    return _Alignof (char *);
  else if (dtl_is_bounded_string_ti (typeid))
    return 1;
  else
  {
    switch (typeid->_d)
    {
      case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
        return _Alignof (dds_sequence_t);
      case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
        return dtl_get_typeid_align (dtl, typeid->_u.array_sdefn.element_identifier);
      case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
        return dtl_get_typeid_align (dtl, typeid->_u.array_ldefn.element_identifier);
      default:
        return get_typeid_typeobj_align (dtl, typeid->_d, typeid);
    }
  }
}

size_t dtl_get_typeobj_size (struct dyntypelib *dtl, DDS_XTypes_CompleteTypeObject const * const typeobj)
{
  return get_typeid_typeobj_size (dtl, typeobj->_d, typeobj);
}

