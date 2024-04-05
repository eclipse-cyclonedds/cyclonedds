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

#include "dds/dds.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"

#include "dynsub.h"

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

static void print_sample1_to (const unsigned char *sample, const DDS_XTypes_CompleteTypeObject *typeobj, struct context *c, const char *sep, const char *label, bool is_base_type);

static bool print_sample1_simple (const unsigned char *sample, const uint8_t disc, struct context *c, const char *sep, const char *label, int32_t *union_disc_value)
{
  switch (disc)
  {
#define CASEI(disc, type, fmt) DDS_XTypes_TK_##disc: { \
    const type *p = (const type *) align (sample, c, _Alignof(type), sizeof(type)); \
    if (union_disc_value) *union_disc_value = (int32_t) *p; \
    if (c->key || c->valid_data) { printf ("%s", sep); if (label) printf ("\"%s\":", label); fmt; } \
    return true; \
  }
#define CASE(disc, type, fmt) DDS_XTypes_TK_##disc: { \
    const type *p = (const type *) align (sample, c, _Alignof(type), sizeof(type)); \
    if (c->key || c->valid_data) { printf ("%s", sep); if (label) printf ("\"%s\":", label); fmt; } \
    return true; \
  }
    case CASEI(BOOLEAN, uint8_t, printf ("%s", *p ? "true" : "false"));
    case CASEI(CHAR8, int8_t, printf ("\"%c\"", (char) *p));
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
    case CASE(STRING8, char *, printf ("\"%s\"", *p));
#undef CASE
  }
  return false;
}

static const char *get_string_pointer (const unsigned char *sample, const DDS_XTypes_TypeIdentifier *typeid, struct context *c)
{
  bool bounded;
  if (typeid->_d == DDS_XTypes_TI_STRING8_SMALL)
    bounded = typeid->_u.string_sdefn.bound != 0;
  else
    bounded = typeid->_u.string_ldefn.bound != 0;
  // must always call align for its side effects
  if (bounded)
    return align (sample, c, _Alignof (char), sizeof (char));
  else
  {
    // if not "valid_data" and not a key field, this'll be a null pointer
    return *((const char **) align (sample, c, _Alignof (char *), sizeof (char *)));
  }
}

static void print_sample1_ti (const unsigned char *sample, const DDS_XTypes_TypeIdentifier *typeid, struct context *c, const char *sep, const char *label, bool is_base_type)
{
  if (print_sample1_simple (sample, typeid->_d, c, sep, label, NULL))
    return;
  switch (typeid->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING8_LARGE: {
      const char *p = get_string_pointer (sample, typeid, c);
      if (c->key || c->valid_data)
      {
        printf ("%s", sep);
        if (label) printf ("\"%s\":", label);
        printf ("\"%s\"", p);
      }
      break;
    }
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE: {
      const DDS_XTypes_TypeIdentifier *et = (typeid->_d == DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL) ? typeid->_u.seq_sdefn.element_identifier : typeid->_u.seq_ldefn.element_identifier;
      const dds_sequence_t *p = align (sample, c, _Alignof (dds_sequence_t), sizeof (dds_sequence_t));
      if (c->key || c->valid_data)
      {
        struct context c1 = *c; c1.offset = 0; c1.maxalign = 1;
        printf ("%s", sep);
        if (label) printf ("\"%s\":", label);
        printf ("[");
        sep = "";
        for (uint32_t i = 0; i < p->_length; i++)
        {
          print_sample1_ti (p->_buffer, et, &c1, sep, NULL, false);
          sep = ",";
        }
        printf ("]");
      }
      break;
    }
    case DDS_XTypes_EK_COMPLETE: {
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeid } }, *info = type_cache_lookup (&templ);
      print_sample1_to (sample, info->typeobj, c, sep, label, is_base_type);
      break;
    }
  }
}

static void print_sample1_to (const unsigned char *sample, const DDS_XTypes_CompleteTypeObject *typeobj, struct context *c, const char *sep, const char *label, bool is_base_type)
{
  if (print_sample1_simple (sample, typeobj->_d, c, sep, label, NULL))
    return;
  switch (typeobj->_d)
  {
    case DDS_XTypes_TK_ALIAS: {
      print_sample1_ti (sample, &typeobj->_u.alias_type.body.common.related_type, c, sep, label, false);
      break;
    }
    case DDS_XTypes_TK_SEQUENCE: {
      const DDS_XTypes_TypeIdentifier *et = &typeobj->_u.sequence_type.element.common.type;
      const dds_sequence_t *p = align (sample, c, _Alignof (dds_sequence_t), sizeof (dds_sequence_t));
      struct context c1 = *c; c1.offset = 0; c1.maxalign = 1;
      printf ("%s", sep);
      if (label) printf ("\"%s\":", label);
      printf ("[");
      sep = "";
      for (uint32_t i = 0; i < p->_length; i++)
      {
        print_sample1_ti ((const unsigned char *) p->_buffer, et, &c1, sep, NULL, false);
        sep = ",";
        if (c1.offset % c1.maxalign)
          c1.offset += c1.maxalign - (c1.offset % c1.maxalign);
      }
      printf ("]");
      break;
    }
    case DDS_XTypes_TK_STRUCTURE: {
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info = type_cache_lookup (&templ);
      const DDS_XTypes_CompleteStructType *t = &typeobj->_u.struct_type;
      const unsigned char *p = align (sample, c, info->align, info->size);
      printf ("%s", sep);
      if (label) printf ("\"%s\":", label);
      if (!is_base_type) printf ("{");
      struct context c1 = *c; c1.offset = 0; c1.maxalign = 1;
      sep = "";
      if (t->header.base_type._d != DDS_XTypes_TK_NONE)
      {
        print_sample1_ti (p, &t->header.base_type, &c1, sep, NULL, true);
        sep = ",";
      }
      for (uint32_t i = 0; i < t->member_seq._length; i++)
      {
        const DDS_XTypes_CompleteStructMember *m = &t->member_seq._buffer[i];
        if (m->common.member_flags & DDS_XTypes_IS_OPTIONAL) {
          void const * const *p1 = (const void *) align (p, &c1, _Alignof (void *), sizeof (void *));
          if (*p1 == NULL) {
            printf ("%s", sep);
            if (*m->detail.name) printf ("\"%s\":", m->detail.name);
            printf ("(nothing)");
          } else {
            struct context c2 = { .valid_data = c->valid_data, .key = false, .offset = 0, .maxalign = 1 };
            print_sample1_ti (*p1, &m->common.member_type_id, &c2, sep, *m->detail.name ? m->detail.name : NULL, false);
          }
        } else {
          c1.key = c->key && m->common.member_flags & DDS_XTypes_IS_KEY;
          print_sample1_ti (p, &m->common.member_type_id, &c1, sep, *m->detail.name ? m->detail.name : NULL, false);
        }
        sep = ",";
      }
      if (!is_base_type) printf ("}");
      break;
    }
    case DDS_XTypes_TK_ENUM: {
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info = type_cache_lookup (&templ);
      const DDS_XTypes_CompleteEnumeratedType *t = &typeobj->_u.enumerated_type;
      const int *p = align (sample, c, info->align, info->size);
      printf ("%s", sep);
      if (label) printf ("\"%s\":", label);
      for (uint32_t l = 0; l < t->literal_seq._length; l++)
      {
        if (t->literal_seq._buffer[l].common.value == *p)
          printf ("\"%s\"", t->literal_seq._buffer[l].detail.name);
      }
      break;
    }
    case DDS_XTypes_TK_UNION: {
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeobj } }, *info = type_cache_lookup (&templ);
      const DDS_XTypes_CompleteUnionType *t = &typeobj->_u.union_type;
      const unsigned char *p = align (sample, c, info->align, info->size);;
      printf ("%s", sep);
      if (label) printf ("\"%s\":", label);
      printf ("{");
      int32_t disc_value = 0;
      sep = "";
      struct context c1 = *c; c1.offset = 0; c1.maxalign = 1;
      if (t->discriminator.common.type_id._d == DDS_XTypes_EK_COMPLETE)
      {
        struct typeinfo templ_disc = { .key = { .key = (uintptr_t) &t->discriminator.common.type_id } }, *info_disc = type_cache_lookup (&templ_disc);
        if (info_disc->typeobj->_d != DDS_XTypes_TK_ENUM)
        {
          printf ("unsupported union discriminant value %u\n", info_disc->typeobj->_d);
          abort ();
        }
        disc_value = * (int32_t *) p;
        print_sample1_to (p, info_disc->typeobj, &c1, sep, "_d", false);
      }
      else if (!print_sample1_simple (p, t->discriminator.common.type_id._d, &c1, sep, "_d", &disc_value))
      {
        abort ();
      }

      sep = ",";
      for (uint32_t i = 0; i < t->member_seq._length; i++)
      {
        const DDS_XTypes_CompleteUnionMember *m = &t->member_seq._buffer[i];
        for (uint32_t l = 0; l < m->common.label_seq._length; l++)
        {
          if (m->common.label_seq._buffer[l] == disc_value)
            print_sample1_ti (p, &m->common.type_id, &c1, sep, *m->detail.name ? m->detail.name : NULL, false);
        }
      }
      printf ("}");
    }
  }
}

void print_sample (bool valid_data, const void *sample, const DDS_XTypes_CompleteTypeObject *typeobj)
{
  struct context c1 = { .valid_data = valid_data, .key = true, .offset = 0, .maxalign = 1 };
  print_sample1_to (sample, typeobj, &c1, "", NULL, false);
  printf ("\n");
}
