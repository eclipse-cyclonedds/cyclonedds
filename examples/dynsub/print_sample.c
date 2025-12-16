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

#include "dds/dds.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"

#include "dynsub.h"

struct context {
  bool valid_data;
  bool key;
  size_t offset;
  size_t maxalign;
  bool needs_comma;
};

static const void *align (const unsigned char *base, struct context *c, size_t align, size_t size)
{
  if (align > c->maxalign)
    c->maxalign = align;
  if (c->offset % align)
    c->offset += align - (c->offset % align);
  const size_t o = c->offset;
  c->offset += size;
  return base + o;
}

static void print_sample1_to (const unsigned char *sample, const DDS_XTypes_CompleteTypeObject *typeobj, struct context *c, const char *label, bool is_base_type, bool is_opt);

static bool print_sample1_simple (const unsigned char *sample, const uint8_t disc, struct context *c, const char *label, int32_t *union_disc_value, bool is_opt)
{
  switch (disc)
  {
#define CASEI(disc, type, fmt) DDS_XTypes_TK_##disc: { \
    const type *p = (const type *) align (sample, c, _Alignof(type), sizeof(type)); \
    if (union_disc_value) *union_disc_value = (int32_t) *p; \
    if (c->key || c->valid_data) { if (c->needs_comma) fputc (',', stdout); if (label) printf ("\"%s\":", label); fmt; c->needs_comma = true; } \
    return true; \
  }
#define CASE(disc, type, fmt) DDS_XTypes_TK_##disc: { \
    const type *p = (const type *) align (sample, c, _Alignof(type), sizeof(type)); \
    if (c->key || c->valid_data) { if (c->needs_comma) fputc (',', stdout); if (label) printf ("\"%s\":", label); fmt; c->needs_comma = true; } \
    return true; \
  }
    case CASEI(BOOLEAN, uint8_t, printf ("%s", *p ? "true" : "false"));
    case CASEI(CHAR8, int8_t, printf ("\"%c\"", (char) *p));
    case CASEI(CHAR16, wchar_t, printf ("\"%lc\"", (wchar_t) *p));
    case CASEI(INT16, int16_t, printf ("%"PRId16, *p));
    case CASEI(INT32, int32_t, printf ("%"PRId32, *p));
    case CASEI(INT64, int64_t, printf ("%"PRId64, *p));
    case CASEI(BYTE, uint8_t, printf ("%"PRIu8, *p));
    case CASEI(UINT8, uint8_t, printf ("%"PRIu8, *p));
    case CASEI(UINT16, uint16_t, printf ("%"PRIu16, *p));
    case CASEI(UINT32, uint32_t, printf ("%"PRIu32, *p));
    case CASEI(UINT64, uint64_t, printf ("%"PRIu64, *p));
    case CASE(FLOAT32, float, printf ("%f", *p));
    case CASE(FLOAT64, double, printf ("%f", *p));
    case CASE(STRING8, char *, printf ("\"%s\"", is_opt ? (char *)p : *p));
    case CASE(STRING16, wchar_t *, printf ("\"%ls\"", is_opt ? (wchar_t *)p : *p));
#undef CASE
  }
  return false;
}

static const char *get_string_pointer (const unsigned char *sample, const DDS_XTypes_TypeIdentifier *typeid, struct context *c, bool is_opt)
{
  uint32_t bound;
  if (typeid->_d == DDS_XTypes_TI_STRING8_SMALL)
    bound = typeid->_u.string_sdefn.bound;
  else
    bound = typeid->_u.string_ldefn.bound;
  // must always call align for its side effects
  if (bound != 0)
  {
    return align (sample, c, _Alignof (char), bound + 1);
  }
  else if (!is_opt)
  {
    // if not "valid_data" and not a key field, this'll be a null pointer
    return *((const char **) align (sample, c, _Alignof (char *), sizeof (char *)));
  }
  else
  {
    return (const char *) sample;
  }
}

static const wchar_t *get_wstring_pointer (const unsigned char *sample, const DDS_XTypes_TypeIdentifier *typeid, struct context *c, bool is_opt)
{
  uint32_t bound;
  if (typeid->_d == DDS_XTypes_TI_STRING16_SMALL)
    bound = typeid->_u.string_sdefn.bound;
  else
    bound = typeid->_u.string_ldefn.bound;
  // must always call align for its side effects
  if (bound != 0)
  {
    return align (sample, c, _Alignof (wchar_t), (bound + 1) * sizeof (wchar_t));
  }
  else if (!is_opt)
  {
    // if not "valid_data" and not a key field, this'll be a null pointer
    return *((const wchar_t **) align (sample, c, _Alignof (wchar_t *), sizeof (wchar_t *)));
  }
  else
  {
    return (const wchar_t *) sample;
  }
}

static void print_sample1_ti (const unsigned char *sample, const DDS_XTypes_TypeIdentifier *typeid, uint32_t rank, struct context *c, const char *label, bool is_base_type, bool is_opt)
{
  if (print_sample1_simple (sample, typeid->_d, c, label, NULL, is_opt))
    return;
  switch (typeid->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING8_LARGE: {
      const char *p = get_string_pointer (sample, typeid, c, is_opt);
      if (c->key || c->valid_data)
      {
        if (c->needs_comma) fputc (',', stdout);
        if (label) printf ("\"%s\":", label);
        printf ("\"%s\"", p);
        c->needs_comma = true;
      }
      break;
    }
    case DDS_XTypes_TI_STRING16_SMALL:
    case DDS_XTypes_TI_STRING16_LARGE: {
      const wchar_t *p = get_wstring_pointer (sample, typeid, c, is_opt);
      if (c->key || c->valid_data)
      {
        if (c->needs_comma) fputc (',', stdout);
        if (label) printf ("\"%s\":", label);
        printf ("\"%ls\"", p);
        c->needs_comma = true;
      }
      break;
    }
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE: {
      const DDS_XTypes_TypeIdentifier *et = (typeid->_d == DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL) ? typeid->_u.seq_sdefn.element_identifier : typeid->_u.seq_ldefn.element_identifier;
      const dds_sequence_t *p = align (sample, c, _Alignof (dds_sequence_t), sizeof (dds_sequence_t));
      if (c->key || c->valid_data)
      {
        struct context c1 = { .key = c->key, .valid_data = c->valid_data, .offset = 0, .maxalign = 1, .needs_comma = false };
        if (c->needs_comma) fputc (',', stdout);
        if (label) printf ("\"%s\":", label);
        printf ("[");
        for (uint32_t i = 0; i < p->_length; i++)
          print_sample1_ti (p->_buffer, et, 0, &c1, NULL, false, false);
        printf ("]");
        c->needs_comma = true;
      }
      break;
    }
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE: {
      const DDS_XTypes_TypeIdentifier *et;
      uint32_t m, n;
      if (typeid->_d == DDS_XTypes_TI_PLAIN_ARRAY_SMALL) {
        et = typeid->_u.array_sdefn.element_identifier;
        m = typeid->_u.array_sdefn.array_bound_seq._length;
        n = typeid->_u.array_sdefn.array_bound_seq._buffer[rank];
      } else {
        et = typeid->_u.array_ldefn.element_identifier;
        m = typeid->_u.array_ldefn.array_bound_seq._length;
        n = typeid->_u.array_ldefn.array_bound_seq._buffer[rank];
      }
      if (c->key || c->valid_data) {
        if (c->needs_comma) fputc (',', stdout);
        if (label) printf ("\"%s\":", label);
        printf ("[");
        c->needs_comma = false;
      }
      for (uint32_t i = 0; i < n; i++)
      {
        if (rank + 1 < m)
          print_sample1_ti (sample, typeid, rank + 1, c, NULL, is_base_type, false);
        else
          print_sample1_ti (sample, et, 0, c, NULL, is_base_type, false);
      }
      if (c->key || c->valid_data) {
        printf ("]");
        c->needs_comma = true;
      }
      break;
    }
    case DDS_XTypes_EK_COMPLETE: {
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeid } }, *info = type_cache_lookup (&templ);
      print_sample1_to (sample, info->typeobj, c, label, is_base_type, is_opt);
      break;
    }
  }
}

static void print_sample1_to (const unsigned char *sample, const DDS_XTypes_CompleteTypeObject *typeobj, struct context *c, const char *label, bool is_base_type, bool is_opt)
{
  if (print_sample1_simple (sample, typeobj->_d, c, label, NULL, is_opt))
    return;
  switch (typeobj->_d)
  {
    case DDS_XTypes_TK_ALIAS: {
      print_sample1_ti (sample, &typeobj->_u.alias_type.body.common.related_type, 0, c, label, false, is_opt);
      break;
    }
    case DDS_XTypes_TK_SEQUENCE: {
      const DDS_XTypes_TypeIdentifier *et = &typeobj->_u.sequence_type.element.common.type;
      const dds_sequence_t *p = align (sample, c, _Alignof (dds_sequence_t), sizeof (dds_sequence_t));
      struct context c1 = { .key = c->key, .valid_data = c->valid_data, .offset = 0, .maxalign = 1, .needs_comma = false };
      if (c->needs_comma) fputc (',', stdout);
      if (label) printf ("\"%s\":", label);
      printf ("[");
      for (uint32_t i = 0; i < p->_length; i++)
      {
        print_sample1_ti ((const unsigned char *) p->_buffer, et, 0, &c1, NULL, false, false);
        if (c1.offset % c1.maxalign)
          c1.offset += c1.maxalign - (c1.offset % c1.maxalign);
      }
      printf ("]");
      c->needs_comma = true;
      break;
    }
    case DDS_XTypes_TK_STRUCTURE: {
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info = type_cache_lookup (&templ);
      const DDS_XTypes_CompleteStructType *t = &typeobj->_u.struct_type;
      const unsigned char *p = align (sample, c, info->align, info->size);
      if (c->needs_comma) fputc (',', stdout);
      if (label) printf ("\"%s\":", label);
      if (!is_base_type) { printf ("{"); c->needs_comma = false; }
      struct context c1 = { .key = c->key, .valid_data = c->valid_data, .offset = 0, .maxalign = 1, .needs_comma = c->needs_comma };
      if (t->header.base_type._d != DDS_XTypes_TK_NONE)
        print_sample1_ti (p, &t->header.base_type, 0, &c1, NULL, true, false);
      for (uint32_t i = 0; i < t->member_seq._length; i++)
      {
        const DDS_XTypes_CompleteStructMember *m = &t->member_seq._buffer[i];
        if (!(m->common.member_flags & DDS_XTypes_IS_OPTIONAL)) {
          c1.key = c->key && m->common.member_flags & DDS_XTypes_IS_KEY;
          print_sample1_ti (p, &m->common.member_type_id, 0, &c1, *m->detail.name ? m->detail.name : NULL, false, false);
        } else {
          void const * const *p1 = (const void *) align (p, &c1, _Alignof (void *), sizeof (void *));
          if (c->valid_data) { // optional is never a key
            if (*p1 != NULL) { // missing optional values are not visible at all in the output
              struct context c2 = { .key = false, .valid_data = c1.valid_data, .offset = 0, .maxalign = 1, .needs_comma = c1.needs_comma };
              print_sample1_ti (*p1, &m->common.member_type_id, 0, &c2, *m->detail.name ? m->detail.name : NULL, false, true);
              c1.needs_comma = c2.needs_comma;
            }
          }
        }
      }
      if (!is_base_type) {
        printf ("}");
        c1.needs_comma = true;
      }
      c->needs_comma = c1.needs_comma;
      break;
    }
    case DDS_XTypes_TK_ENUM: {
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info = type_cache_lookup (&templ);
      const DDS_XTypes_CompleteEnumeratedType *t = &typeobj->_u.enumerated_type;
      const int *p = align (sample, c, info->align, info->size);
      if (c->needs_comma) fputc (',', stdout);
      if (label) printf ("\"%s\":", label);
      for (uint32_t l = 0; l < t->literal_seq._length; l++)
      {
        if (t->literal_seq._buffer[l].common.value == *p)
          printf ("\"%s\"", t->literal_seq._buffer[l].detail.name);
      }
      c->needs_comma = true;
      break;
    }
    case DDS_XTypes_TK_UNION: {
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info = type_cache_lookup (&templ);
      const DDS_XTypes_CompleteUnionType *t = &typeobj->_u.union_type;
      const unsigned char *p = align (sample, c, info->align, info->size);
      if (c->needs_comma) fputc (',', stdout);
      if (label) printf ("\"%s\":", label);
      printf ("{");
      int32_t disc_value = 0;
      struct context c1 = { .key = c->key, .valid_data = c->valid_data, .offset = 0, .maxalign = 1, .needs_comma = false };
      if (t->discriminator.common.type_id._d == DDS_XTypes_EK_COMPLETE)
      {
        struct typeinfo templ_disc = { .key = { .key = (uintptr_t) &t->discriminator.common.type_id } }, *info_disc = type_cache_lookup (&templ_disc);
        if (info_disc->typeobj->_d != DDS_XTypes_TK_ENUM)
        {
          printf ("unsupported union discriminant value %u\n", info_disc->typeobj->_d);
          abort ();
        }
        disc_value = * (int32_t *) p;
        print_sample1_to (p, info_disc->typeobj, &c1, "_d", false, false);
      }
      else if (!print_sample1_simple (p, t->discriminator.common.type_id._d, &c1, "_d", &disc_value, false))
      {
        abort ();
      }
      for (uint32_t i = 0; i < t->member_seq._length; i++)
      {
        const DDS_XTypes_CompleteUnionMember *m = &t->member_seq._buffer[i];
        for (uint32_t l = 0; l < m->common.label_seq._length; l++)
        {
          if (m->common.label_seq._buffer[l] == disc_value)
            print_sample1_ti (p, &m->common.type_id, 0, &c1, *m->detail.name ? m->detail.name : NULL, false, false);
        }
      }
      printf ("}");
      c->needs_comma = true;
    }
  }
}

void print_sample (bool valid_data, const void *sample, const DDS_XTypes_CompleteTypeObject *typeobj)
{
  struct context c1 = { .valid_data = valid_data, .key = true, .offset = 0, .maxalign = 1, .needs_comma = false };
  print_sample1_to (sample, typeobj, &c1, NULL, false, false);
  printf ("\n");
}
