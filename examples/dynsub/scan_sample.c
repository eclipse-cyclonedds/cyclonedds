// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#define _CRT_SECURE_NO_WARNINGS // mbstowcs, strcpy, wcscpy

#include <locale.h>

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <wchar.h>

#include "dds/ddsrt/string.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsi/ddsi_xt_typeinfo.h"

#include "domtree.h"
#include "type_cache.h"
#include "dyntypelib.h"
#include "size_and_align.h"

static bool getbool (const char *data, bool *v)
{
  if (ddsrt_strcasecmp (data, "true") == 0) { *v = true; return true; }
  else if (ddsrt_strcasecmp (data, "false") == 0) { *v = false; return true; }
  else return false;
}

static bool getfloat128 (const char *data, unsigned char *v)
{
  char *endp;
  const double d = strtod (data, &endp);
  if (!(*data && strspn (endp, " \t") == strlen (endp)))
    return false;
  memset (v, 0, 16);
  uint64_t u;
  memcpy (&u, &d, sizeof (u));
  // no proper handling of NaN, Inf, subnormals, rounding
  // double: sign (1) + exp (11) + mantissa (52 + 1 implicit), exp bias = 16383
  // quad:   sign (1) + exp (15) + mantissa (112 + 1 implicit), exp bias = 1023
  const int exp = (int) ((u >> 52) & 0x7ff) - 1023;
  const uint64_t exp128 = (exp + 16383) & 0x7fff;
  const uint64_t mant = (u & ~((uint64_t)0xfff << 52));
  const uint64_t w = (u & (uint64_t)1 << 63) | (exp128 << 48) | (mant >> 4);
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
  memcpy (v + 8, &w, 8);
  v[7] = (uint8_t) (mant << 4);
#else
  memcpy (v, &w, 8);
  v[8] = (uint8_t) (mant << 4);
#endif
  return true;
}

static bool getfloat64 (const char *data, double *v)
{
  char *endp;
  *v = strtod (data, &endp);
  return *data && strspn (endp, " \t") == strlen (endp);
}

static bool getfloat32 (const char *data, float *v)
{
  double x;
  if (!getfloat64 (data, &x))
    return false;
  *v = (float) x;
  return true;
}

static bool getint64 (const char *data, int64_t *v)
{
  char *endp;
  long long x = strtoll (data, &endp, 0);
  if (*data && strspn (endp, " \t") == strlen (endp)) {
    *v = (int64_t) x;
    return true;
  }
  return false;
}

static bool getuint64 (const char *data, uint64_t *v)
{
  char *endp;
  unsigned long long x = strtoull (data, &endp, 0);
  if (*data && strspn (endp, " \t") == strlen (endp)) {
    *v = (uint64_t) x;
    return true;
  }
  return false;
}

static bool getint32 (const char *data, int32_t *v)
{
  int64_t x;
  if (!getint64 (data, &x) || x < INT32_MIN || x > INT32_MAX)
    return false;
  *v = (int32_t) x;
  return true;
}

static bool getuint32 (const char *data, uint32_t *v)
{
  uint64_t x;
  if (!getuint64 (data, &x) || x > UINT32_MAX)
    return false;
  *v = (uint32_t) x;
  return true;
}

static bool getint16 (const char *data, int16_t *v)
{
  int64_t x;
  if (!getint64 (data, &x) || x < INT16_MIN || x > INT16_MAX)
    return false;
  *v = (int16_t) x;
  return true;
}

static bool getuint16 (const char *data, uint16_t *v)
{
  uint64_t x;
  if (!getuint64 (data, &x) || x > UINT16_MAX)
    return false;
  *v = (uint16_t) x;
  return true;
}

static bool getint8 (const char *data, int8_t *v)
{
  int64_t x;
  if (!getint64 (data, &x) || x < INT8_MIN || x > INT8_MAX)
    return false;
  *v = (int8_t) x;
  return true;
}

static bool getuint8 (const char *data, uint8_t *v)
{
  uint64_t x;
  if (!getuint64 (data, &x) || x > UINT8_MAX)
    return false;
  *v = (uint8_t) x;
  return true;
}

static wchar_t *s2w_strdup (const char *s)
{
  // FIXME: Should probably not be setting locale
  setlocale(LC_ALL, "");
  size_t n = mbstowcs (NULL, s, 0);
  wchar_t *w = ddsrt_malloc ((n + 1) * sizeof (*w));
  if (mbstowcs (w, s, n + 1) == (size_t) -1)
  {
    ddsrt_free (w);
    return NULL;
  }
  return w;
}

static bool scan_sample1_simple (unsigned char * const base, const uint8_t disc, struct elem const * const elem, struct dyntypelib_error *err)
{
  switch (disc)
  {
    case DDS_XTypes_TK_BOOLEAN:
      return getbool (elem->data, (bool *) base);
    case DDS_XTypes_TK_INT8:
      return getint8 (elem->data, (int8_t *) base);
    case DDS_XTypes_TK_INT16:
      return getint16 (elem->data, (int16_t *) base);
    case DDS_XTypes_TK_INT32:
      return getint32 (elem->data, (int32_t *) base);
    case DDS_XTypes_TK_INT64:
      return getint64 (elem->data, (int64_t *) base);
    case DDS_XTypes_TK_UINT8: case DDS_XTypes_TK_BYTE:
      return getuint8 (elem->data, (uint8_t *) base);
    case DDS_XTypes_TK_UINT16:
      return getuint16 (elem->data, (uint16_t *) base);
    case DDS_XTypes_TK_UINT32:
      return getuint32 (elem->data, (uint32_t *) base);
    case DDS_XTypes_TK_UINT64:
      return getuint64 (elem->data, (uint64_t *) base);
    case DDS_XTypes_TK_CHAR8:
      *(char *) base = elem->data[0];
      return true;
    case DDS_XTypes_TK_STRING8:
      *(char **) base = ddsrt_strdup (elem->data);
      return true;
    case DDS_XTypes_TK_FLOAT32:
      return getfloat32 (elem->data, (float *) base);
    case DDS_XTypes_TK_FLOAT64:
      return getfloat64 (elem->data, (double *) base);
    case DDS_XTypes_TK_FLOAT128:
      return getfloat128 (elem->data, base);
    case DDS_XTypes_TK_CHAR16:
      if (mbtowc ((wchar_t *) base, elem->data, strlen (elem->data)) < 0)
        return false;
      return true;
    case DDS_XTypes_TK_STRING16:
      if ((*((wchar_t **) base) = s2w_strdup (elem->data)) == NULL) {
        dtl_set_error (err, elem, "invalid multibyte string\n");
        return false;
      }
      return true;
  }
  return false;
}

ddsrt_attribute_warn_unused_result
static bool scan_sample1_to (struct dyntypelib *dtl, unsigned char *obj, DDS_XTypes_CompleteTypeObject const * const typeobj, struct elem const * const elem, const bool is_opt_or_ext, const bool ignore_unknown_members, struct dyntypelib_error *err);

ddsrt_attribute_warn_unused_result
static bool scan_sample1_ti (struct dyntypelib *dtl, unsigned char *obj, DDS_XTypes_TypeIdentifier const * const typeid, struct elem const * const elem, const bool is_opt_or_ext, const bool ignore_unknown_members, struct dyntypelib_error *err);

static bool scan_sequence (struct dyntypelib *dtl, struct dds_sequence * const seq, DDS_XTypes_TypeIdentifier const * const typeid, uint32_t bound, struct elem const * const elem, const bool ignore_unknown_members, struct dyntypelib_error *err)
{
  uint32_t n = 0;
  for (const struct elem *it = elem->children; it; n++, it = it->next)
    if (strcmp (it->name, "item") != 0)
      return (dtl_set_error (err, it, "expected \"item\", got \"%s\"\n", it->name) == DDS_RETCODE_OK);
  if (bound && n > bound)
    return (dtl_set_error (err, elem, "%"PRIu32" items but bound is %"PRIu32"\n", n, bound) == DDS_RETCODE_OK);
  seq->_maximum = seq->_length = n;
  seq->_release = true;
  if (n == 0)
    seq->_buffer = NULL;
  else
  {
    seq->_buffer = ddsrt_calloc (seq->_maximum, dtl_get_typeid_size (dtl, typeid));
    size_t off = 0;
    // FIXME: @external, @optional?
    for (const struct elem *it = elem->children; it; n++, it = it->next)
    {
      unsigned char *obj = dtl_advance_ti (dtl, seq->_buffer, &off, typeid, false);
      if (!scan_sample1_ti (dtl, obj, typeid, it, false, ignore_unknown_members, err))
        return false;
    }
  }
  return true;
}

static bool scan_array (struct dyntypelib *dtl, void * const ary, DDS_XTypes_TypeIdentifier const * const typeid, uint32_t bound, struct elem const * const elem, const bool ignore_unknown_members, struct dyntypelib_error *err)
{
  const struct elem *it;
  uint32_t idx = 0;
  size_t off = 0;
  for (it = elem->children; it && idx < bound; idx++, it = it->next)
  {
    if (strcmp (it->name, "item") != 0)
      return (dtl_set_error (err, it, "expected \"item\", got \"%s\"\n", it->name) == DDS_RETCODE_OK);
    if (!scan_sample1_ti (dtl, dtl_advance_ti (dtl, ary, &off, typeid, false), typeid, it, false, ignore_unknown_members, err))
      return false;
  }
  if (it != NULL || idx != bound)
    return (dtl_set_error (err, elem, "wrong number of items\n") == DDS_RETCODE_OK);
  return true;
}

static bool scan_sample1_ti (struct dyntypelib *dtl, unsigned char * obj, DDS_XTypes_TypeIdentifier const * const typeid, struct elem const * const elem, const bool is_opt_or_ext, const bool ignore_unknown_members, struct dyntypelib_error *err)
{
  if (is_opt_or_ext && !dtl_is_unbounded_string_ti (typeid))
  {
    *((void **) obj) = ddsrt_calloc (1, dtl_get_typeid_size (dtl, typeid));
    obj = *((void **) obj);
  }

  if (scan_sample1_simple (obj, typeid->_d, elem, err))
    return true;

  switch (typeid->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING8_LARGE:
      if (dtl_is_unbounded_string_ti (typeid))
        *(char **) obj = ddsrt_strdup (elem->data);
      else if (!elem->data)
        strcpy ((char *) obj, "");
      else if (strlen (elem->data) > dtl_bounded_string_bound_ti (typeid))
        return (dtl_set_error (err, elem, "oversize bounded string\n") == DDS_RETCODE_OK);
      else
        strcpy ((char *) obj, elem->data);
      return true;

    case DDS_XTypes_TI_STRING16_SMALL:
    case DDS_XTypes_TI_STRING16_LARGE: {
      wchar_t *ws = s2w_strdup (elem->data);
      if (ws == NULL)
        return (dtl_set_error (err, elem, "invalid multibyte string\n") == DDS_RETCODE_OK);
      if (dtl_is_unbounded_string_ti (typeid))
        *(wchar_t **) obj = ws;
      else if (wcslen (ws) > dtl_bounded_string_bound_ti (typeid))
        return (dtl_set_error (err, elem, "oversize bounded string\n") == DDS_RETCODE_OK);
      else
      {
        wcscpy ((wchar_t *) obj, ws);
        ddsrt_free (ws);
      }
      return true;
    }

    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      return scan_sequence (dtl, (struct dds_sequence *) obj, typeid->_u.seq_sdefn.element_identifier, typeid->_u.seq_sdefn.bound, elem, ignore_unknown_members, err);

    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      return scan_sequence (dtl, (struct dds_sequence *) obj, typeid->_u.seq_ldefn.element_identifier, typeid->_u.seq_ldefn.bound, elem, ignore_unknown_members, err);

    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL: {
      uint32_t nelem = 1;
      for (uint32_t i = 0; i < typeid->_u.array_sdefn.array_bound_seq._length; i++)
        nelem *= typeid->_u.array_sdefn.array_bound_seq._buffer[i];
      return scan_array (dtl, obj, typeid->_u.array_sdefn.element_identifier, nelem, elem, ignore_unknown_members, err);
    }

    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE: {
      uint32_t nelem = 1;
      for (uint32_t i = 0; i < typeid->_u.array_ldefn.array_bound_seq._length; i++)
        nelem *= typeid->_u.array_ldefn.array_bound_seq._buffer[i];
      return scan_array (dtl, obj, typeid->_u.array_ldefn.element_identifier, nelem, elem, ignore_unknown_members, err);
    }

    case DDS_XTypes_EK_COMPLETE: {
      struct typeinfo templ = { .key = { .key = (uintptr_t) typeid } }, *info = type_cache_lookup (dtl->typecache, &templ);
      return scan_sample1_to (dtl, obj, info->typeobj, elem, false, ignore_unknown_members, err);
    }
  }

  abort ();
  return false;
}

static const DDS_XTypes_CompleteStructType *get_base_struct_type_ti (struct dyntypelib *dtl, DDS_XTypes_TypeIdentifier const * const typeid);

static const DDS_XTypes_CompleteStructType *get_base_struct_type_to (struct dyntypelib *dtl, DDS_XTypes_CompleteTypeObject const * const typeobj)
{
  switch (typeobj->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      return get_base_struct_type_ti (dtl, &typeobj->_u.alias_type.body.common.related_type);
    case DDS_XTypes_TK_STRUCTURE:
      return &typeobj->_u.struct_type;
  }
  abort ();
  return NULL;
}

static const DDS_XTypes_CompleteStructType *get_base_struct_type_ti (struct dyntypelib *dtl, DDS_XTypes_TypeIdentifier const * const typeid)
{
  struct typeinfo templ = { .key = { .key = (uintptr_t) typeid } }, *info = type_cache_lookup (dtl->typecache, &templ);
  return get_base_struct_type_to (dtl, info->typeobj);
}

static const DDS_XTypes_CompleteStructMember *find_struct_member1 (struct dyntypelib *dtl, unsigned char ** const m_base, unsigned char * const obj, size_t *off, DDS_XTypes_CompleteStructType const * const t, const char *name)
{
  if (t->header.base_type._d != DDS_XTypes_TK_NONE)
  {
    DDS_XTypes_CompleteStructType const * const bt = get_base_struct_type_ti (dtl, &t->header.base_type);
    DDS_XTypes_CompleteStructMember const * const m = find_struct_member1 (dtl, m_base, obj, off, bt, name);
    if (m != NULL)
      return m;
  }
  for (uint32_t i = 0; i < t->member_seq._length; i++)
  {
    const DDS_XTypes_CompleteStructMember *m = &t->member_seq._buffer[i];
    const bool m_is_opt_or_ext = m->common.member_flags & (DDS_XTypes_IS_OPTIONAL | DDS_XTypes_IS_EXTERNAL);
    *m_base = dtl_advance_ti (dtl, obj, off, &m->common.member_type_id, m_is_opt_or_ext);
    if (strcmp (name, m->detail.name) == 0)
      return m;
  }
  return NULL;
}

static const DDS_XTypes_CompleteStructMember *find_struct_member (struct dyntypelib *dtl, unsigned char ** const m_base, unsigned char * const obj, DDS_XTypes_CompleteStructType const * const t, const char *name)
{
  size_t off = 0;
  return find_struct_member1 (dtl, m_base, obj, &off, t, name);
}

static bool scan_sample1_to (struct dyntypelib *dtl, unsigned char *obj, DDS_XTypes_CompleteTypeObject const * const typeobj, struct elem const * const elem, const bool is_opt_or_ext, const bool ignore_unknown_members, struct dyntypelib_error *err)
{
  if (is_opt_or_ext && !dtl_is_unbounded_string_to (typeobj))
  {
    *((void **) obj) = ddsrt_calloc (1, dtl_get_typeobj_size (dtl, typeobj));
    obj = *((void **) obj);
  }

  if (scan_sample1_simple (obj, typeobj->_d, elem, err))
  {
    return true;
  }

  switch (typeobj->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      return scan_sample1_ti (dtl, obj, &typeobj->_u.alias_type.body.common.related_type, elem, false, ignore_unknown_members, err);

    case DDS_XTypes_TK_SEQUENCE:
      return scan_sequence (dtl, (struct dds_sequence *) obj, &typeobj->_u.sequence_type.element.common.type, typeobj->_u.sequence_type.header.common.bound, elem, ignore_unknown_members, err);

    case DDS_XTypes_TK_STRUCTURE: {
      const DDS_XTypes_CompleteStructType *t = &typeobj->_u.struct_type;
      for (const struct elem *melem = elem->children; melem; melem = melem->next)
      {
        const DDS_XTypes_CompleteStructMember *m;
        unsigned char *m_base;
        if ((m = find_struct_member (dtl, &m_base, obj, t, melem->name)) == NULL)
        {
          if (!ignore_unknown_members)
            return (dtl_set_error (err, melem, "member %s not found\n", melem->name) == DDS_RETCODE_OK);
        }
        else
        {
          const bool m_is_opt_or_ext = m->common.member_flags & (DDS_XTypes_IS_OPTIONAL | DDS_XTypes_IS_EXTERNAL);
          if (!scan_sample1_ti (dtl, m_base, &m->common.member_type_id, melem, m_is_opt_or_ext, ignore_unknown_members, err))
            return false;
        }
      }
      return true;
    }

    case DDS_XTypes_TK_ENUM: {
      const DDS_XTypes_CompleteEnumeratedType *t = &typeobj->_u.enumerated_type;
      for (uint32_t l = 0; l < t->literal_seq._length; l++)
      {
        if (elem->data == NULL)
          return (dtl_set_error (err, elem, "enum value expected\n") == DDS_RETCODE_OK);
        if (strcmp (t->literal_seq._buffer[l].detail.name, elem->data) == 0)
        {
          // FIXME: bit bound
          *((int *) obj) = (int) t->literal_seq._buffer[l].common.value;
          return true;
        }
      }
      return (dtl_set_error (err, elem, "literal \"%s\" not found in enum\n", elem->data) == DDS_RETCODE_OK);
    }

    case DDS_XTypes_TK_BITMASK: { // FIXME: symbolic names?
      const DDS_XTypes_CompleteBitmaskType *t = &typeobj->_u.bitmask_type;
      uint64_t v;
      if (!getuint64 (elem->data, &v))
        return (dtl_set_error (err, elem, "bitmask value \"%s\" not handled\n", elem->data) == DDS_RETCODE_OK);
      uint64_t flags = 0;
      for (uint32_t i = 0; i < t->flag_seq._length; i++)
        flags |= (uint64_t)1 << t->flag_seq._buffer[i].common.position;
      if (v & ~flags)
        return (dtl_set_error (err, elem, "bitmask value \"%s\" sets undefined bits\n", elem->data) == DDS_RETCODE_OK);
      if (t->header.common.bit_bound > 32)
        *((uint64_t *) obj) = v;
      else if (t->header.common.bit_bound > 16)
        *((uint32_t *) obj) = (uint32_t) v;
      else if (t->header.common.bit_bound > 16)
        *((uint16_t *) obj) = (uint16_t) v;
      else
        *((uint8_t *) obj) = (uint8_t) v;
      return true;
    }

    case DDS_XTypes_TK_UNION: {
      const DDS_XTypes_CompleteUnionType *t = &typeobj->_u.union_type;
      uint64_t disc_value = 0;
      // discriminator is always at offset 0
      if (elem->children == NULL ||
          strcmp (elem->children->name, "discriminator") != 0 ||
          elem->children->next == NULL || elem->children->next->next != NULL)
      {
        return (dtl_set_error (err, elem, "union: expected first child 'discriminator' and second child matching union case\n") == DDS_RETCODE_OK);
      }
      if (!scan_sample1_ti (dtl, obj, &t->discriminator.common.type_id, elem->children, false, false, err))
        return false;
      const size_t disc_size = dtl_get_typeid_size (dtl, &t->discriminator.common.type_id);
      switch (t->discriminator.common.type_id._d)
      {
        case DDS_XTypes_TK_INT8:  disc_value = (uint64_t) *((int8_t *) obj); break;
        case DDS_XTypes_TK_INT16: disc_value = (uint64_t) *((int16_t *) obj); break;
        case DDS_XTypes_TK_INT32: disc_value = (uint64_t) *((int32_t *) obj); break;
        case DDS_XTypes_TK_INT64: disc_value = (uint64_t) *((int64_t *) obj); break;
        default:
          switch (disc_size)
          {
            case 1: disc_value = *((uint8_t *) obj); break;
            case 2: disc_value = *((uint16_t *) obj); break;
            case 4: disc_value = *((uint32_t *) obj); break;
            case 8: disc_value = *((uint64_t *) obj); break;
            default: abort ();
          }
          break;
      }
      size_t data_off = disc_size;
      for (uint32_t i = 0; i < t->member_seq._length; i++)
      {
        // FIXME: shouldn't need to recompute this every time
        DDS_XTypes_CompleteUnionMember const * const m = &t->member_seq._buffer[i];
        size_t a;
        if (m->common.member_flags & (DDS_XTypes_IS_OPTIONAL | DDS_XTypes_IS_EXTERNAL))
          a = _Alignof (char *);
        else
          a = dtl_get_typeid_align (dtl, &m->common.type_id);
        if (a > data_off)
          data_off = a;
      }
      uint32_t memberidx;
      for (memberidx = 0; memberidx < t->member_seq._length; memberidx++)
      {
        DDS_XTypes_CompleteUnionMember const * const m = &t->member_seq._buffer[memberidx];
        if (strcmp (m->detail.name, elem->children->next->name) == 0)
          break;
      }
      if (memberidx == t->member_seq._length)
        return (dtl_set_error (err, elem, "union case not found") == DDS_RETCODE_OK);
      DDS_XTypes_CompleteUnionMember const * const m = &t->member_seq._buffer[memberidx];
      if (!(m->common.member_flags & DDS_XTypes_IS_DEFAULT))
      {
        uint32_t labelidx;
        for (labelidx = 0; labelidx < m->common.label_seq._length; labelidx++)
        {
          // FIXME: it looks like label_seq holds 32-bit ints in type obj. If so, how are
          // 64-bit discriminators supposed to be supported?
          if (disc_value == (uint64_t) m->common.label_seq._buffer[labelidx])
            break;
        }
        if (labelidx == m->common.label_seq._length)
          return (dtl_set_error (err, elem, "case labels do not include discriminator") == DDS_RETCODE_OK);
      }
      const bool case_is_opt_or_ext = m->common.member_flags & (DDS_XTypes_IS_OPTIONAL | DDS_XTypes_IS_EXTERNAL);
      return scan_sample1_ti (dtl, obj + data_off, &m->common.type_id, elem->children->next, case_is_opt_or_ext, ignore_unknown_members, err);
    }
  }

  abort ();
  return false;
}

void *dtl_scan_sample (struct dyntypelib *dtl, const struct elem *input, const DDS_XTypes_CompleteTypeObject *typeobj, const bool ignore_unknown_members, struct dyntypelib_error *err)
{
  unsigned char *sample = ddsrt_calloc (1, dtl_get_typeobj_size (dtl, typeobj));
  if (scan_sample1_to (dtl, sample, typeobj, input, false, ignore_unknown_members, err))
    return sample;
  else
  {
    // FIXME: leaks
    ddsrt_free (sample);
    return NULL;
  }
}
