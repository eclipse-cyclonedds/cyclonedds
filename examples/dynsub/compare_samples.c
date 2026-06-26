// Copyright(c) 2022 to 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>

#include "compare_samples.h"
#include "dyntypelib.h"
#include "size_and_align.h"

struct compare_ctx {
  struct type_cache *tc;
  bool valid_data;
  bool equal;
  struct dyntypelib_error *err;
};

struct array_info {
  const DDS_XTypes_TypeIdentifier *elem_type;
  uint32_t nbounds;
  bool small_bounds;
  union {
    const DDS_XTypes_SBound *small;
    const DDS_XTypes_LBound *large;
  } bounds;
};

ddsrt_attribute_format_printf (2, 3)
static dds_return_t compare_error (struct compare_ctx *ctx, const char *fmt, ...)
{
  if (ctx->err)
  {
    va_list ap;
    va_start (ap, fmt);
    (void) vsnprintf (ctx->err->errmsg, sizeof (ctx->err->errmsg), fmt, ap);
    va_end (ap);
  }
  return DDS_RETCODE_ERROR;
}

static bool visible (const struct compare_ctx *ctx, bool key_path)
{
  return ctx->valid_data || key_path;
}

static bool simple_kind (DDS_XTypes_TypeKind kind)
{
  size_t align, size;
  return dtl_simple_alignof_sizeof (kind, &align, &size);
}

static bool is_indirect_member (DDS_XTypes_MemberFlag flags)
{
  return (flags & (DDS_XTypes_IS_OPTIONAL | DDS_XTypes_IS_EXTERNAL)) != 0;
}

static bool is_optional_member (DDS_XTypes_MemberFlag flags)
{
  return (flags & DDS_XTypes_IS_OPTIONAL) != 0;
}

static bool strings_equal (const char *a, const char *b)
{
  return (a == NULL || b == NULL) ? (a == b) : strcmp (a, b) == 0;
}

static bool wstrings_equal (const wchar_t *a, const wchar_t *b)
{
  return (a == NULL || b == NULL) ? (a == b) : wcscmp (a, b) == 0;
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

static const char *string8_value (const unsigned char *obj, const DDS_XTypes_TypeIdentifier *typeid, bool direct_string)
{
  if (dtl_is_bounded_string_ti (typeid) || direct_string)
    return (const char *) obj;
  return *((const char * const *) obj);
}

static const wchar_t *string16_value (const unsigned char *obj, const DDS_XTypes_TypeIdentifier *typeid, bool direct_string)
{
  if (dtl_is_bounded_string_ti (typeid) || direct_string)
    return (const wchar_t *) obj;
  return *((const wchar_t * const *) obj);
}

static const unsigned char *advance_ti (struct compare_ctx *ctx, const unsigned char *base, size_t *off, const DDS_XTypes_TypeIdentifier *typeid, bool indirect)
{
  if (indirect)
    return dtl_align ((unsigned char *) base, off, _Alignof (void *), sizeof (void *));

  size_t align, size;
  type_cache_typeid_align_size (ctx->tc, typeid, &align, &size);
  return dtl_align ((unsigned char *) base, off, align, size);
}

static bool typeobj_is_unbounded_string (struct compare_ctx *ctx, const DDS_XTypes_CompleteTypeObject *typeobj);

static bool typeid_is_unbounded_string (struct compare_ctx *ctx, const DDS_XTypes_TypeIdentifier *typeid)
{
  if (dtl_is_unbounded_string_ti (typeid))
    return true;
  switch (typeid->_d)
  {
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT: {
      struct typeinfo *info = type_cache_lookup_typeid (ctx->tc, typeid);
      return typeobj_is_unbounded_string (ctx, info->typeobj);
    }
  }
  return false;
}

static bool typeobj_is_unbounded_string (struct compare_ctx *ctx, const DDS_XTypes_CompleteTypeObject *typeobj)
{
  if (dtl_is_unbounded_string_to (typeobj))
    return true;
  if (typeobj->_d == DDS_XTypes_TK_ALIAS)
    return typeid_is_unbounded_string (ctx, &typeobj->_u.alias_type.body.common.related_type);
  return false;
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

static bool get_array_info (const DDS_XTypes_TypeIdentifier *typeid, struct array_info *info)
{
  switch (typeid->_d)
  {
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
      *info = (struct array_info){
        .elem_type = typeid->_u.array_sdefn.element_identifier,
        .nbounds = typeid->_u.array_sdefn.array_bound_seq._length,
        .small_bounds = true,
        .bounds.small = typeid->_u.array_sdefn.array_bound_seq._buffer
      };
      return true;
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      *info = (struct array_info){
        .elem_type = typeid->_u.array_ldefn.element_identifier,
        .nbounds = typeid->_u.array_ldefn.array_bound_seq._length,
        .small_bounds = false,
        .bounds.large = typeid->_u.array_ldefn.array_bound_seq._buffer
      };
      return true;
  }
  return false;
}

static uint32_t array_bound (const struct array_info *info, uint32_t rank)
{
  return info->small_bounds ? info->bounds.small[rank] : info->bounds.large[rank];
}

static uint32_t array_nelem (const struct array_info *info)
{
  uint32_t n = 1;
  for (uint32_t i = 0; i < info->nbounds; i++)
    n *= array_bound (info, i);
  return n;
}

static dds_return_t compare_ti (struct compare_ctx *ctx, const unsigned char *obj1, const unsigned char *obj2, const DDS_XTypes_TypeIdentifier *typeid, bool direct_string, bool key_path);
static dds_return_t compare_to (struct compare_ctx *ctx, const unsigned char *obj1, const unsigned char *obj2, const DDS_XTypes_CompleteTypeObject *typeobj, bool direct_string, bool key_path);

static dds_return_t compare_simple (struct compare_ctx *ctx, const unsigned char *obj1, const unsigned char *obj2, DDS_XTypes_TypeKind kind, bool direct_string)
{
  switch (kind)
  {
    case DDS_XTypes_TK_BOOLEAN:
      ctx->equal = *((const uint8_t *) obj1) == *((const uint8_t *) obj2);
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_CHAR8:
      ctx->equal = *((const int8_t *) obj1) == *((const int8_t *) obj2);
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_CHAR16:
      ctx->equal = *((const wchar_t *) obj1) == *((const wchar_t *) obj2);
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_INT8:
      ctx->equal = *((const int8_t *) obj1) == *((const int8_t *) obj2);
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_INT16:
      ctx->equal = *((const int16_t *) obj1) == *((const int16_t *) obj2);
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_INT32:
      ctx->equal = *((const int32_t *) obj1) == *((const int32_t *) obj2);
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_INT64:
      ctx->equal = *((const int64_t *) obj1) == *((const int64_t *) obj2);
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_BYTE:
    case DDS_XTypes_TK_UINT8:
      ctx->equal = *((const uint8_t *) obj1) == *((const uint8_t *) obj2);
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_UINT16:
      ctx->equal = *((const uint16_t *) obj1) == *((const uint16_t *) obj2);
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_UINT32:
      ctx->equal = *((const uint32_t *) obj1) == *((const uint32_t *) obj2);
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_UINT64:
      ctx->equal = *((const uint64_t *) obj1) == *((const uint64_t *) obj2);
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_FLOAT32:
      ctx->equal = *((const float *) obj1) == *((const float *) obj2);
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_FLOAT64:
      ctx->equal = *((const double *) obj1) == *((const double *) obj2);
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_FLOAT128:
      ctx->equal = memcmp (obj1, obj2, 16) == 0;
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_STRING8: {
      const char *s1 = direct_string ? (const char *) obj1 : *((const char * const *) obj1);
      const char *s2 = direct_string ? (const char *) obj2 : *((const char * const *) obj2);
      ctx->equal = strings_equal (s1, s2);
      return DDS_RETCODE_OK;
    }
    case DDS_XTypes_TK_STRING16: {
      const wchar_t *s1 = direct_string ? (const wchar_t *) obj1 : *((const wchar_t * const *) obj1);
      const wchar_t *s2 = direct_string ? (const wchar_t *) obj2 : *((const wchar_t * const *) obj2);
      ctx->equal = wstrings_equal (s1, s2);
      return DDS_RETCODE_OK;
    }
  }
  return compare_error (ctx, "unsupported simple type discriminator %u", (unsigned) kind);
}

static dds_return_t compare_sequence (struct compare_ctx *ctx, const unsigned char *obj1, const unsigned char *obj2, const DDS_XTypes_TypeIdentifier *elem_type, bool key_path)
{
  const dds_sequence_t *seq1 = (const dds_sequence_t *) obj1;
  const dds_sequence_t *seq2 = (const dds_sequence_t *) obj2;
  if (seq1->_length != seq2->_length)
  {
    ctx->equal = false;
    return DDS_RETCODE_OK;
  }

  size_t off1 = 0;
  size_t off2 = 0;
  for (uint32_t i = 0; ctx->equal && i < seq1->_length; i++)
  {
    const unsigned char *elem1 = advance_ti (ctx, seq1->_buffer, &off1, elem_type, false);
    const unsigned char *elem2 = advance_ti (ctx, seq2->_buffer, &off2, elem_type, false);
    dds_return_t rc = compare_ti (ctx, elem1, elem2, elem_type, false, key_path);
    if (rc != DDS_RETCODE_OK)
      return rc;
  }
  return DDS_RETCODE_OK;
}

static dds_return_t compare_array (struct compare_ctx *ctx, const unsigned char *obj1, const unsigned char *obj2, const DDS_XTypes_TypeIdentifier *typeid, bool key_path)
{
  struct array_info info;
  if (!get_array_info (typeid, &info))
    return compare_error (ctx, "unsupported array type discriminator %u", (unsigned) typeid->_d);

  size_t off1 = 0;
  size_t off2 = 0;
  const uint32_t n = array_nelem (&info);
  for (uint32_t i = 0; ctx->equal && i < n; i++)
  {
    const unsigned char *elem1 = advance_ti (ctx, obj1, &off1, info.elem_type, false);
    const unsigned char *elem2 = advance_ti (ctx, obj2, &off2, info.elem_type, false);
    dds_return_t rc = compare_ti (ctx, elem1, elem2, info.elem_type, false, key_path);
    if (rc != DDS_RETCODE_OK)
      return rc;
  }
  return DDS_RETCODE_OK;
}

static dds_return_t compare_indirect_ti (struct compare_ctx *ctx, const void * const *ptr1, const void * const *ptr2, const DDS_XTypes_TypeIdentifier *typeid, DDS_XTypes_MemberFlag flags, bool key_path)
{
  if (*ptr1 == NULL || *ptr2 == NULL)
  {
    ctx->equal = (*ptr1 == *ptr2);
    return DDS_RETCODE_OK;
  }

  const bool child_key_path = key_path && !is_optional_member (flags);
  const bool direct_string = typeid_is_unbounded_string (ctx, typeid);
  return compare_ti (ctx, *ptr1, *ptr2, typeid, direct_string, child_key_path);
}

static dds_return_t compare_struct (struct compare_ctx *ctx, const unsigned char *obj1, const unsigned char *obj2, const DDS_XTypes_CompleteStructType *type, bool key_path)
{
  size_t off1 = 0;
  size_t off2 = 0;

  if (type->header.base_type._d != DDS_XTypes_TK_NONE)
  {
    const unsigned char *base1 = advance_ti (ctx, obj1, &off1, &type->header.base_type, false);
    const unsigned char *base2 = advance_ti (ctx, obj2, &off2, &type->header.base_type, false);
    dds_return_t rc = compare_ti (ctx, base1, base2, &type->header.base_type, false, key_path);
    if (rc != DDS_RETCODE_OK || !ctx->equal)
      return rc;
  }

  for (uint32_t i = 0; ctx->equal && i < type->member_seq._length; i++)
  {
    const DDS_XTypes_CompleteStructMember *m = &type->member_seq._buffer[i];
    const bool indirect = is_indirect_member (m->common.member_flags);
    const unsigned char *member1 = advance_ti (ctx, obj1, &off1, &m->common.member_type_id, indirect);
    const unsigned char *member2 = advance_ti (ctx, obj2, &off2, &m->common.member_type_id, indirect);
    const bool member_key_path = key_path && ((m->common.member_flags & DDS_XTypes_IS_KEY) != 0);
    if (!visible (ctx, member_key_path))
      continue;

    dds_return_t rc;
    if (indirect)
      rc = compare_indirect_ti (ctx, (const void * const *) member1, (const void * const *) member2, &m->common.member_type_id, m->common.member_flags, member_key_path);
    else
      rc = compare_ti (ctx, member1, member2, &m->common.member_type_id, false, member_key_path);
    if (rc != DDS_RETCODE_OK)
      return rc;
  }

  return DDS_RETCODE_OK;
}

static bool read_discriminator (struct compare_ctx *ctx, const unsigned char *obj, const DDS_XTypes_TypeIdentifier *typeid, int32_t *value)
{
  switch (typeid->_d)
  {
    case DDS_XTypes_TK_BOOLEAN:
    case DDS_XTypes_TK_BYTE:
    case DDS_XTypes_TK_UINT8:
      *value = (int32_t) *((const uint8_t *) obj);
      return true;
    case DDS_XTypes_TK_CHAR8:
    case DDS_XTypes_TK_INT8:
      *value = (int32_t) *((const int8_t *) obj);
      return true;
    case DDS_XTypes_TK_CHAR16:
      *value = (int32_t) *((const wchar_t *) obj);
      return true;
    case DDS_XTypes_TK_INT16:
      *value = (int32_t) *((const int16_t *) obj);
      return true;
    case DDS_XTypes_TK_INT32:
      *value = *((const int32_t *) obj);
      return true;
    case DDS_XTypes_TK_UINT16:
      *value = (int32_t) *((const uint16_t *) obj);
      return true;
    case DDS_XTypes_TK_UINT32:
      *value = (int32_t) *((const uint32_t *) obj);
      return true;
    case DDS_XTypes_TK_INT64:
      *value = (int32_t) *((const int64_t *) obj);
      return true;
    case DDS_XTypes_TK_UINT64:
      *value = (int32_t) *((const uint64_t *) obj);
      return true;
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT: {
      struct typeinfo *info = type_cache_lookup_typeid (ctx->tc, typeid);
      if (info->typeobj->_d == DDS_XTypes_TK_ENUM)
      {
        *value = *((const int32_t *) obj);
        return true;
      }
      if (info->typeobj->_d == DDS_XTypes_TK_BITMASK)
      {
        *value = (int32_t) read_bitmask_value (obj, info->typeobj->_u.bitmask_type.header.common.bit_bound);
        return true;
      }
      return false;
    }
  }
  return false;
}

static dds_return_t compare_union (struct compare_ctx *ctx, const unsigned char *obj1, const unsigned char *obj2, const DDS_XTypes_CompleteUnionType *type, bool key_path)
{
  int32_t disc_value;
  const DDS_XTypes_TypeIdentifier *disc_type = &type->discriminator.common.type_id;
  dds_return_t rc = compare_ti (ctx, obj1, obj2, disc_type, false, key_path);
  if (rc != DDS_RETCODE_OK || !ctx->equal)
    return rc;
  if (!read_discriminator (ctx, obj1, disc_type, &disc_value))
    return compare_error (ctx, "unsupported union discriminator type %u", (unsigned) disc_type->_d);

  const DDS_XTypes_CompleteUnionMember *m = find_union_member_for_disc (type, disc_value);
  if (m == NULL)
    return DDS_RETCODE_OK;

  const unsigned char *data1 = obj1 + type_cache_union_data_offset (ctx->tc, type);
  const unsigned char *data2 = obj2 + type_cache_union_data_offset (ctx->tc, type);
  size_t off1 = 0;
  size_t off2 = 0;
  const bool indirect = is_indirect_member (m->common.member_flags);
  const unsigned char *member1 = advance_ti (ctx, data1, &off1, &m->common.type_id, indirect);
  const unsigned char *member2 = advance_ti (ctx, data2, &off2, &m->common.type_id, indirect);

  if (indirect)
    return compare_indirect_ti (ctx, (const void * const *) member1, (const void * const *) member2, &m->common.type_id, m->common.member_flags, key_path);
  return compare_ti (ctx, member1, member2, &m->common.type_id, false, key_path);
}

static dds_return_t compare_ti (struct compare_ctx *ctx, const unsigned char *obj1, const unsigned char *obj2, const DDS_XTypes_TypeIdentifier *typeid, bool direct_string, bool key_path)
{
  if (!visible (ctx, key_path) || !ctx->equal)
    return DDS_RETCODE_OK;

  if (simple_kind (typeid->_d))
    return compare_simple (ctx, obj1, obj2, typeid->_d, direct_string);

  switch (typeid->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING8_LARGE:
      ctx->equal = strings_equal (string8_value (obj1, typeid, direct_string), string8_value (obj2, typeid, direct_string));
      return DDS_RETCODE_OK;
    case DDS_XTypes_TI_STRING16_SMALL:
    case DDS_XTypes_TI_STRING16_LARGE:
      ctx->equal = wstrings_equal (string16_value (obj1, typeid, direct_string), string16_value (obj2, typeid, direct_string));
      return DDS_RETCODE_OK;
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      return compare_sequence (ctx, obj1, obj2, typeid->_u.seq_sdefn.element_identifier, key_path);
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      return compare_sequence (ctx, obj1, obj2, typeid->_u.seq_ldefn.element_identifier, key_path);
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      return compare_array (ctx, obj1, obj2, typeid, key_path);
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT: {
      struct typeinfo *info = type_cache_lookup_typeid (ctx->tc, typeid);
      return compare_to (ctx, obj1, obj2, info->typeobj, direct_string, key_path);
    }
  }

  return compare_error (ctx, "unsupported type identifier discriminator %u", (unsigned) typeid->_d);
}

static dds_return_t compare_to (struct compare_ctx *ctx, const unsigned char *obj1, const unsigned char *obj2, const DDS_XTypes_CompleteTypeObject *typeobj, bool direct_string, bool key_path)
{
  if (!visible (ctx, key_path) || !ctx->equal)
    return DDS_RETCODE_OK;

  if (simple_kind (typeobj->_d))
    return compare_simple (ctx, obj1, obj2, typeobj->_d, direct_string);

  switch (typeobj->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      return compare_ti (ctx, obj1, obj2, &typeobj->_u.alias_type.body.common.related_type, direct_string, key_path);
    case DDS_XTypes_TK_SEQUENCE:
      return compare_sequence (ctx, obj1, obj2, &typeobj->_u.sequence_type.element.common.type, key_path);
    case DDS_XTypes_TK_STRUCTURE:
      return compare_struct (ctx, obj1, obj2, &typeobj->_u.struct_type, key_path);
    case DDS_XTypes_TK_ENUM: {
      struct typeinfo *info = type_cache_lookup_typeobj (ctx->tc, typeobj);
      ctx->equal = memcmp (obj1, obj2, info->size) == 0;
      return DDS_RETCODE_OK;
    }
    case DDS_XTypes_TK_BITMASK:
      ctx->equal = read_bitmask_value (obj1, typeobj->_u.bitmask_type.header.common.bit_bound) ==
                   read_bitmask_value (obj2, typeobj->_u.bitmask_type.header.common.bit_bound);
      return DDS_RETCODE_OK;
    case DDS_XTypes_TK_UNION:
      return compare_union (ctx, obj1, obj2, &typeobj->_u.union_type, key_path);
  }

  return compare_error (ctx, "unsupported type object discriminator %u", (unsigned) typeobj->_d);
}

dds_return_t compare_samples_equal (struct type_cache *tc, bool valid_data, const void *sample1, const void *sample2, const DDS_XTypes_CompleteTypeObject *typeobj, bool *equal, struct dyntypelib_error *err)
{
  if (equal == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  struct compare_ctx ctx = {
    .tc = tc,
    .valid_data = valid_data,
    .equal = true,
    .err = err
  };
  dds_return_t rc = compare_to (&ctx, sample1, sample2, typeobj, false, true);
  *equal = ctx.equal;
  return rc;
}

int compare_samples (struct type_cache *tc, bool valid_data, const void *sample1, const void *sample2, const DDS_XTypes_CompleteTypeObject *typeobj)
{
  bool equal = false;
  dds_return_t rc = compare_samples_equal (tc, valid_data, sample1, sample2, typeobj, &equal, NULL);
  if (rc != DDS_RETCODE_OK)
    return -1;
  return equal ? 1 : 0;
}
