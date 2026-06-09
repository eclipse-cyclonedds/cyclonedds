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

#include "type_cache.h"
#include "compare_samples.h"

struct context {
  bool valid_data;
  bool key;
  size_t offset;
  size_t maxalign;
  bool needs_comma;
};

static void finish_sequence_element (struct context *c)
{
  if (c->offset % c->maxalign)
    c->offset += c->maxalign - (c->offset % c->maxalign);
}

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

static bool is_indirect_member (DDS_XTypes_MemberFlag flags)
{
  return (flags & (DDS_XTypes_IS_OPTIONAL | DDS_XTypes_IS_EXTERNAL)) != 0;
}

static bool is_optional_member (DDS_XTypes_MemberFlag flags)
{
  return (flags & DDS_XTypes_IS_OPTIONAL) != 0;
}

static bool union_member_has_label (const DDS_XTypes_CompleteUnionMember *member, int32_t disc_value)
{
  for (uint32_t l = 0; l < member->common.label_seq._length; l++)
    if (member->common.label_seq._buffer[l] == disc_value)
      return true;
  return false;
}

static const DDS_XTypes_CompleteUnionMember *find_union_member_for_disc (const DDS_XTypes_CompleteUnionType *type, int32_t disc_value)
{
  const DDS_XTypes_CompleteUnionMember *default_member = NULL;
  for (uint32_t i = 0; i < type->member_seq._length; i++)
  {
    const DDS_XTypes_CompleteUnionMember *member = &type->member_seq._buffer[i];
    if (union_member_has_label (member, disc_value))
      return member;
    if (member->common.member_flags & DDS_XTypes_IS_DEFAULT)
      default_member = member;
  }
  return default_member;
}

static uint64_t read_bitmask_value (const void *p, uint16_t bit_bound)
{
  if (bit_bound > 32)
    return *((const uint64_t *) p);
  else if (bit_bound > 16)
    return *((const uint32_t *) p);
  else if (bit_bound > 8)
    return *((const uint16_t *) p);
  else
    return *((const uint8_t *) p);
}

static int samples_eq1_to (struct type_cache *tc, const unsigned char *sample1, const unsigned char *sample2, const DDS_XTypes_CompleteTypeObject *typeobj, struct context *c1, struct context *c2, const char *label, bool is_base_type, bool is_opt);

static int samples_eq1_simple (const unsigned char *sample1, const unsigned char *sample2, const uint8_t disc, struct context *c1, struct context *c2, const char *label, int32_t *union_disc_value, bool is_opt)
{
  (void) label;
  switch (disc)
  {
#define CASEI(disc, type, fmt) DDS_XTypes_TK_##disc: { \
    const type *p1 = (const type *) align (sample1, c1, _Alignof(type), sizeof(type)); \
    const type *p2 = (const type *) align (sample2, c2, _Alignof(type), sizeof(type)); \
    if (union_disc_value) *union_disc_value = (int32_t) *p1; \
    if ((c1->key || c1->valid_data) && (c2->key || c2->valid_data)) { return fmt; } \
    return false; \
  }
#define CASE(disc, type, fmt) DDS_XTypes_TK_##disc: { \
    const type *p1 = (const type *) align (sample1, c1, _Alignof(type), sizeof(type)); \
    const type *p2 = (const type *) align (sample2, c2, _Alignof(type), sizeof(type)); \
    if ((c1->key || c1->valid_data) && (c2->key || c2->valid_data)) { return fmt;} \
    return false; \
  }
    case CASEI(BOOLEAN, uint8_t, *p1 == *p2);
    case CASEI(CHAR8, int8_t, *p1 == *p2);
    case CASEI(CHAR16, wchar_t, *p1 == *p2);
    case CASEI(INT16, int16_t, *p1 == *p2);
    case CASEI(INT32, int32_t, *p1 == *p2);
    case CASEI(INT64, int64_t, *p1 == *p2);
    case CASEI(BYTE, uint8_t, *p1 == *p2);
    case CASEI(UINT8, uint8_t, *p1 == *p2);
    case CASEI(UINT16, uint16_t, *p1 == *p2);
    case CASEI(UINT32, uint32_t, *p1 == *p2);
    case CASEI(UINT64, uint64_t, *p1 == *p2);
    case CASE(FLOAT32, float, *p1 == *p2);
    case CASE(FLOAT64, double, *p1 == *p2);
#undef CASE
    case DDS_XTypes_TK_STRING8: {
      const void *p1 = align (sample1, c1, _Alignof (char *), sizeof (char *));
      const void *p2 = align (sample2, c2, _Alignof (char *), sizeof (char *));
      const char *s1 = is_opt ? p1 : *(const char * const *) p1;
      const char *s2 = is_opt ? p2 : *(const char * const *) p2;
      if ((c1->key || c1->valid_data) && (c2->key || c2->valid_data))
        return (s1 == NULL || s2 == NULL) ? (s1 == s2) : (strcmp (s1, s2) == 0);
      return false;
    }
    case DDS_XTypes_TK_STRING16: {
      const void *p1 = align (sample1, c1, _Alignof (wchar_t *), sizeof (wchar_t *));
      const void *p2 = align (sample2, c2, _Alignof (wchar_t *), sizeof (wchar_t *));
      const wchar_t *s1 = is_opt ? p1 : *(const wchar_t * const *) p1;
      const wchar_t *s2 = is_opt ? p2 : *(const wchar_t * const *) p2;
      if ((c1->key || c1->valid_data) && (c2->key || c2->valid_data))
        return (s1 == NULL || s2 == NULL) ? (s1 == s2) : (wcscmp (s1, s2) == 0);
      return false;
    }
    case DDS_XTypes_TK_FLOAT128: { // FIXME
      const unsigned char *p1 = align (sample1, c1, 8, 16);
      const unsigned char *p2 = align (sample2, c2, 8, 16);
      return memcmp(p1, p2, 16) == 0;
    }
  }
  return -1;
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

static int samples_eq1_ti (struct type_cache *tc, const unsigned char *sample1, const unsigned char *sample2, const DDS_XTypes_TypeIdentifier *typeid, uint32_t rank, struct context *c1, struct context *c2, const char *label, bool is_base_type, bool is_opt)
{
  int tmp = samples_eq1_simple(sample1, sample2, typeid->_d, c1, c2, label, NULL, is_opt);
  if (tmp >= 0)
    return tmp;
  switch (typeid->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING8_LARGE: {
      const char *p1 = get_string_pointer (sample1, typeid, c1, is_opt);
      const char *p2 = get_string_pointer (sample2, typeid, c2, is_opt);
      if ((c1->key || c1->valid_data) && (c2->key || c2->valid_data))
      {
        return strcmp(p1, p2) == 0;
      }
      return false;
    }
    case DDS_XTypes_TI_STRING16_SMALL:
    case DDS_XTypes_TI_STRING16_LARGE: {
      const wchar_t *p1 = get_wstring_pointer (sample1, typeid, c1, is_opt);
      const wchar_t *p2 = get_wstring_pointer (sample2, typeid, c2, is_opt);
      if ((c1->key || c1->valid_data) && (c2->key || c2->valid_data))
      {
        return wcscmp(p1, p2) == 0;
      }
      return false;
    }
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE: {
      const DDS_XTypes_TypeIdentifier *et = (typeid->_d == DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL) ? typeid->_u.seq_sdefn.element_identifier : typeid->_u.seq_ldefn.element_identifier;
      const dds_sequence_t *p1 = align (sample1, c1, _Alignof (dds_sequence_t), sizeof (dds_sequence_t));
      const dds_sequence_t *p2 = align (sample2, c2, _Alignof (dds_sequence_t), sizeof (dds_sequence_t));
      if ((c1->key || c1->valid_data) && (c2->key || c2->valid_data))
      {
        if (p1->_length != p2->_length)
        {
          return false;
        }
        struct context c1_1 = { .key = c1->key, .valid_data = c1->valid_data, .offset = 0, .maxalign = 1, .needs_comma = false };
        struct context c2_1 = { .key = c2->key, .valid_data = c2->valid_data, .offset = 0, .maxalign = 1, .needs_comma = false };
        for (uint32_t i = 0; i < p1->_length; i++)
        {
          int temp = samples_eq1_ti (tc, p1->_buffer, p2->_buffer, et, 0, &c1_1, &c2_1, NULL, false, false);
          if(temp != 1) return temp;
          finish_sequence_element (&c1_1);
          finish_sequence_element (&c2_1);
        }
        return true;
      }
      return false;
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
      for (uint32_t i = 0; i < n; i++)
      {
        int temp = 0;
        if (rank + 1 < m)
          temp = samples_eq1_ti (tc, sample1, sample2, typeid, rank + 1, c1, c2, NULL, is_base_type, false);
        else
          temp = samples_eq1_ti (tc, sample1, sample2, et, 0, c1, c2, NULL, is_base_type, false);
        if(temp != 1) return temp;
      }
      break;
    }
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT: {
      struct typeinfo *info = type_cache_lookup_typeid (tc, typeid);
      int temp = samples_eq1_to (tc, sample1, sample2, info->typeobj, c1, c2, label, is_base_type, is_opt);
      if (temp != 1) return temp;
      break;
    }
  }
  return -1;
}

static int samples_eq1_to (struct type_cache *tc, const unsigned char *sample1, const unsigned char *sample2, const DDS_XTypes_CompleteTypeObject *typeobj, struct context *c1, struct context *c2, const char *label, bool is_base_type, bool is_opt)
{
  (void) is_base_type;
  int tmp = samples_eq1_simple(sample1, sample2, typeobj->_d, c1, c2, label, NULL, is_opt);
  if (tmp >= 0)
    return tmp;
  switch (typeobj->_d)
  {
    case DDS_XTypes_TK_ALIAS: {
      int temp = samples_eq1_ti (tc, sample1, sample2, &typeobj->_u.alias_type.body.common.related_type, 0, c1, c2, label, false, is_opt);
      if (temp != 1) return temp;
      break;
    }
    case DDS_XTypes_TK_SEQUENCE: {
      const DDS_XTypes_TypeIdentifier *et = &typeobj->_u.sequence_type.element.common.type;
      const dds_sequence_t *p1 = align (sample1, c1, _Alignof (dds_sequence_t), sizeof (dds_sequence_t));
      const dds_sequence_t *p2 = align (sample2, c2, _Alignof (dds_sequence_t), sizeof (dds_sequence_t));
      struct context c1_1 = { .key = c1->key, .valid_data = c1->valid_data, .offset = 0, .maxalign = 1, .needs_comma = false };
      struct context c2_1 = { .key = c2->key, .valid_data = c2->valid_data, .offset = 0, .maxalign = 1, .needs_comma = false };
      if (p1->_length != p2->_length)
      {
        return 0;
      }
      for (uint32_t i = 0; i < p1->_length; i++)
      {
        int temp = samples_eq1_ti (tc, (const unsigned char *) p1->_buffer, (const unsigned char *) p2->_buffer, et, 0, &c1_1, &c2_1, NULL, false, false);
        if (temp != 1) return temp;
        finish_sequence_element (&c1_1);
        finish_sequence_element (&c2_1);
      }
      break;
    }
    case DDS_XTypes_TK_STRUCTURE: {
      struct typeinfo *info = type_cache_lookup_typeobj (tc, typeobj);
      const DDS_XTypes_CompleteStructType *t = &typeobj->_u.struct_type;
      const unsigned char *p1 = align (sample1, c1, info->align, info->size);
      const unsigned char *p2 = align (sample2, c2, info->align, info->size);
      struct context c1_1 = { .key = c1->key, .valid_data = c1->valid_data, .offset = 0, .maxalign = 1, .needs_comma = c1->needs_comma };
      struct context c2_1 = { .key = c2->key, .valid_data = c2->valid_data, .offset = 0, .maxalign = 1, .needs_comma = c2->needs_comma };
      if (t->header.base_type._d != DDS_XTypes_TK_NONE) {
        int temp = samples_eq1_ti (tc, p1, p2, &t->header.base_type, 0, &c1_1, &c2_1, NULL, true, false);
        if (temp != 1) return temp;
      }
      for (uint32_t i = 0; i < t->member_seq._length; i++)
      {
        const DDS_XTypes_CompleteStructMember *m = &t->member_seq._buffer[i];
        c1_1.key = c1->key && (m->common.member_flags & DDS_XTypes_IS_KEY);
        c2_1.key = c2->key && (m->common.member_flags & DDS_XTypes_IS_KEY);
        if (!is_indirect_member (m->common.member_flags)) {
          int temp = samples_eq1_ti (tc, p1, p2, &m->common.member_type_id, 0, &c1_1, &c2_1, *m->detail.name ? m->detail.name : NULL, false, false);
          if (temp != 1) return temp;
        } else {
          void const * const *p1_1 = (const void *) align (p1, &c1_1, _Alignof (void *), sizeof (void *));
          void const * const *p2_1 = (const void *) align (p2, &c2_1, _Alignof (void *), sizeof (void *));
          if ((c1_1.key || c1_1.valid_data) && (c2_1.key || c2_1.valid_data)) {
            if (*p1_1 == NULL && *p2_1 == NULL)
              continue;
            if (*p1_1 == NULL || *p2_1 == NULL)
            {
              return 0;
            }
            struct context c1_2 = {.key = c1_1.key && !is_optional_member (m->common.member_flags), .valid_data = c1_1.valid_data, .offset = 0, .maxalign = 1, .needs_comma = c1_1.needs_comma};
            struct context c2_2 = {.key = c2_1.key && !is_optional_member (m->common.member_flags), .valid_data = c2_1.valid_data, .offset = 0, .maxalign = 1, .needs_comma = c2_1.needs_comma};
            int temp = samples_eq1_ti(tc, *p1_1, *p2_1, &m->common.member_type_id, 0, &c1_2, &c2_2, *m->detail.name ? m->detail.name : NULL, false, true);
            if (temp != 1) return temp;
            c1_1.needs_comma = c1_2.needs_comma;
            c2_1.needs_comma = c2_2.needs_comma;
          }
        }
      }
      c1->needs_comma = c1_1.needs_comma;
      c2->needs_comma = c2_1.needs_comma;
      break;
    }
    case DDS_XTypes_TK_ENUM: {
      struct typeinfo *info = type_cache_lookup_typeobj (tc, typeobj);
      const int *p1 = align (sample1, c1, info->align, info->size);
      const int *p2 = align (sample2, c2, info->align, info->size);
      return *p1 == *p2;
    }
    case DDS_XTypes_TK_BITMASK: {
      struct typeinfo *info = type_cache_lookup_typeobj (tc, typeobj);
      const DDS_XTypes_CompleteBitmaskType *t = &typeobj->_u.bitmask_type;
      const void *p1 = align (sample1, c1, info->align, info->size);
      const void *p2 = align (sample2, c2, info->align, info->size);
      return read_bitmask_value (p1, t->header.common.bit_bound) == read_bitmask_value (p2, t->header.common.bit_bound);
    }
    case DDS_XTypes_TK_UNION: {
      struct typeinfo *info = type_cache_lookup_typeobj (tc, typeobj);
      const DDS_XTypes_CompleteUnionType *t = &typeobj->_u.union_type;
      const unsigned char *p1 = align (sample1, c1, info->align, info->size);
      const unsigned char *p2 = align (sample2, c2, info->align, info->size);
      int32_t disc_value = 0;
      struct context c1_1 = { .key = c1->key, .valid_data = c1->valid_data, .offset = 0, .maxalign = 1, .needs_comma = false };
      struct context c2_1 = { .key = c2->key, .valid_data = c2->valid_data, .offset = 0, .maxalign = 1, .needs_comma = false };
      if (t->discriminator.common.type_id._d == DDS_XTypes_EK_COMPLETE)
      {
        struct typeinfo *info_disc = type_cache_lookup_typeid (tc, &t->discriminator.common.type_id);
        if (info_disc->typeobj->_d == DDS_XTypes_TK_ENUM)
        {
          if (*(int32_t *) p1 != *(int32_t *) p2)
            return 0;
          disc_value = * (int32_t *) p1;
        }
        else if (info_disc->typeobj->_d == DDS_XTypes_TK_BITMASK)
        {
          if (info_disc->typeobj->_u.bitmask_type.header.common.bit_bound > 32) {
            if (*(int64_t *) p1 != *(int64_t *) p2)
              return 0;
            disc_value = (int32_t) (* (int64_t *) p1);
          } else if (info_disc->typeobj->_u.bitmask_type.header.common.bit_bound > 16) {
            if (*(int32_t *) p1 != *(int32_t *) p2)
              return 0;
            disc_value = * (int32_t *) p1;
          } else if (info_disc->typeobj->_u.bitmask_type.header.common.bit_bound > 8) {
            if (*(int16_t *) p1 != *(int16_t *) p2)
              return 0;
            disc_value = * (int16_t *) p1;
          } else {
            if (*(int8_t *) p1 != *(int8_t *) p2)
              return 0;
            disc_value = * (int8_t *) p1;
          }
        }
        else
        {
          printf ("unsupported union discriminant value %u\n", info_disc->typeobj->_d);
          abort ();
        }
        int temp = samples_eq1_to (tc, p1, p2, info_disc->typeobj, &c1_1, &c2_1, "_d", false, false);
        if (temp != 1) return temp;
      }
      samples_eq1_simple (p1, p2, t->discriminator.common.type_id._d, &c1_1, &c2_1, "_d", &disc_value, false);
      const DDS_XTypes_CompleteUnionMember *m = find_union_member_for_disc (t, disc_value);
      if (m != NULL)
      {
        const unsigned char *data1 = p1 + type_cache_union_data_offset (tc, t);
        const unsigned char *data2 = p2 + type_cache_union_data_offset (tc, t);
        struct context c1_2 = { .key = c1_1.key, .valid_data = c1_1.valid_data, .offset = 0, .maxalign = 1, .needs_comma = c1_1.needs_comma };
        struct context c2_2 = { .key = c2_1.key, .valid_data = c2_1.valid_data, .offset = 0, .maxalign = 1, .needs_comma = c2_1.needs_comma };
        if (!is_indirect_member (m->common.member_flags))
        {
          int temp = samples_eq1_ti (tc, data1, data2, &m->common.type_id, 0, &c1_2, &c2_2, *m->detail.name ? m->detail.name : NULL, false, false);
          if (temp != 1) return temp;
        }
        else
        {
          void const * const *p1_1 = (const void *) align (data1, &c1_2, _Alignof (void *), sizeof (void *));
          void const * const *p2_1 = (const void *) align (data2, &c2_2, _Alignof (void *), sizeof (void *));
          if ((c1_2.key || c1_2.valid_data) && (c2_2.key || c2_2.valid_data) && (*p1_1 != NULL || *p2_1 != NULL))
          {
            if (*p1_1 == NULL || *p2_1 == NULL)
              return 0;
            struct context c1_3 = { .key = c1_2.key && !is_optional_member (m->common.member_flags), .valid_data = c1_2.valid_data, .offset = 0, .maxalign = 1, .needs_comma = c1_2.needs_comma };
            struct context c2_3 = { .key = c2_2.key && !is_optional_member (m->common.member_flags), .valid_data = c2_2.valid_data, .offset = 0, .maxalign = 1, .needs_comma = c2_2.needs_comma };
            int temp = samples_eq1_ti (tc, *p1_1, *p2_1, &m->common.type_id, 0, &c1_3, &c2_3, *m->detail.name ? m->detail.name : NULL, false, true);
            if (temp != 1) return temp;
          }
        }
        c1_1.needs_comma = c1_2.needs_comma;
        c2_1.needs_comma = c2_2.needs_comma;
      }
      c1->needs_comma = true;
      c2->needs_comma = true;
    }
  }
  return -1;
}

// FIXME: Still requires support for mutable types when ordering of members may be different
int compare_samples (struct type_cache *tc, bool valid_data, const void *sample1, const void* sample2, const DDS_XTypes_CompleteTypeObject *typeobj)
{
  struct context c1 = { .valid_data = valid_data, .key = true, .offset = 0, .maxalign = 1, .needs_comma = false };
  struct context c2 = { .valid_data = valid_data, .key = true, .offset = 0, .maxalign = 1, .needs_comma = false };
  return samples_eq1_to (tc, sample1, sample2, typeobj, &c1, &c2, NULL, false, false);
}
