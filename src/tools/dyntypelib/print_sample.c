// Copyright(c) 2022 to 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>

#include "dds/ddsrt/heap.h"

#include "dyntypelib.h"
#include "float128_io.h"
#include "size_and_align.h"

#define PRINT_STACK_MAX 64

enum container_kind {
  CONTAINER_OBJECT,
  CONTAINER_COLLECTION
};

struct container {
  enum container_kind kind;
  size_t count;
};

struct string_output {
  char *buf;
  size_t len;
  size_t cap;
};

struct print_ctx {
  struct dyntypelib *dtl;
  bool valid_data;
  bool key_path;
  struct dtl_sample_print_options opts;
  const struct dtl_sample_output *out;
  size_t written;
  struct dyntypelib_error *err;
  struct container stack[PRINT_STACK_MAX];
  uint32_t depth;
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

struct collection_window {
  bool limited;
  uint32_t head;
  uint32_t tail;
};

ddsrt_attribute_format_printf (2, 3)
static dds_return_t print_error (struct print_ctx *ctx, const char *fmt, ...)
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

static dds_return_t out_write (struct print_ctx *ctx, const char *data, size_t size)
{
  if (size == 0)
    return DDS_RETCODE_OK;
  if (ctx->opts.max_output_bytes != 0)
  {
    if (ctx->written > ctx->opts.max_output_bytes ||
        size > ctx->opts.max_output_bytes - ctx->written)
      return print_error (ctx, "sample output limit exceeded");
  }
  dds_return_t rc = ctx->out->write (ctx->out->state, data, size);
  if (rc == DDS_RETCODE_OK)
    ctx->written += size;
  return rc;
}

static dds_return_t out_puts (struct print_ctx *ctx, const char *s)
{
  return out_write (ctx, s, strlen (s));
}

ddsrt_attribute_format_printf (2, 3)
static dds_return_t out_printf (struct print_ctx *ctx, const char *fmt, ...)
{
  char buf[128];
  va_list ap;
  va_start (ap, fmt);
  va_list ap1;
  va_copy (ap1, ap);
  const int n = vsnprintf (buf, sizeof (buf), fmt, ap1);
  va_end (ap1);
  if (n < 0)
  {
    va_end (ap);
    return print_error (ctx, "sample output formatting failed");
  }
  if ((size_t) n < sizeof (buf))
  {
    va_end (ap);
    return out_write (ctx, buf, (size_t) n);
  }

  char *tmp = ddsrt_malloc ((size_t) n + 1);
  if (tmp == NULL)
  {
    va_end (ap);
    return DDS_RETCODE_OUT_OF_RESOURCES;
  }
  (void) vsnprintf (tmp, (size_t) n + 1, fmt, ap);
  va_end (ap);
  dds_return_t rc = out_write (ctx, tmp, (size_t) n);
  ddsrt_free (tmp);
  return rc;
}

static dds_return_t string_output_write (void *state, const char *data, size_t size)
{
  struct string_output *out = state;
  if (size == 0)
    return DDS_RETCODE_OK;
  if (out->len > SIZE_MAX - size - 1)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  const size_t mincap = out->len + size + 1;
  if (mincap > out->cap)
  {
    size_t cap = out->cap ? out->cap : 256;
    while (cap < mincap)
    {
      if (cap > SIZE_MAX / 2)
      {
        cap = mincap;
        break;
      }
      cap *= 2;
    }
    char *buf = ddsrt_realloc (out->buf, cap);
    if (buf == NULL)
      return DDS_RETCODE_OUT_OF_RESOURCES;
    out->buf = buf;
    out->cap = cap;
  }
  memcpy (out->buf + out->len, data, size);
  out->len += size;
  out->buf[out->len] = 0;
  return DDS_RETCODE_OK;
}

static bool is_json (const struct print_ctx *ctx)
{
  return ctx->opts.format == DTL_SAMPLE_FORMAT_JSON;
}

static bool visible (const struct print_ctx *ctx)
{
  return ctx->valid_data || ctx->key_path;
}

static dds_return_t push_container (struct print_ctx *ctx, enum container_kind kind)
{
  if (ctx->depth == PRINT_STACK_MAX)
    return print_error (ctx, "sample nesting too deep");
  ctx->stack[ctx->depth++] = (struct container){ .kind = kind, .count = 0 };
  return DDS_RETCODE_OK;
}

static void pop_container (struct print_ctx *ctx)
{
  assert (ctx->depth > 0);
  ctx->depth--;
}

static const char *xml_value_name (const struct print_ctx *ctx, const char *label)
{
  if (label != NULL)
    return label;
  if (ctx->depth > 0 && ctx->stack[ctx->depth - 1].kind == CONTAINER_COLLECTION)
    return "item";
  return NULL;
}

static dds_return_t write_utf8_codepoint (struct print_ctx *ctx, uint32_t cp)
{
  char buf[4];
  size_t n;
  if (cp <= 0x7f)
  {
    buf[0] = (char) cp;
    n = 1;
  }
  else if (cp <= 0x7ff)
  {
    buf[0] = (char) (0xc0 | (cp >> 6));
    buf[1] = (char) (0x80 | (cp & 0x3f));
    n = 2;
  }
  else if (cp <= 0xffff)
  {
    buf[0] = (char) (0xe0 | (cp >> 12));
    buf[1] = (char) (0x80 | ((cp >> 6) & 0x3f));
    buf[2] = (char) (0x80 | (cp & 0x3f));
    n = 3;
  }
  else if (cp <= 0x10ffff)
  {
    buf[0] = (char) (0xf0 | (cp >> 18));
    buf[1] = (char) (0x80 | ((cp >> 12) & 0x3f));
    buf[2] = (char) (0x80 | ((cp >> 6) & 0x3f));
    buf[3] = (char) (0x80 | (cp & 0x3f));
    n = 4;
  }
  else
  {
    buf[0] = '?';
    n = 1;
  }
  return out_write (ctx, buf, n);
}

static dds_return_t write_json_escaped_byte (struct print_ctx *ctx, unsigned char c)
{
  switch (c)
  {
    case '"': return out_puts (ctx, "\\\"");
    case '\\': return out_puts (ctx, "\\\\");
    case '\b': return out_puts (ctx, "\\b");
    case '\f': return out_puts (ctx, "\\f");
    case '\n': return out_puts (ctx, "\\n");
    case '\r': return out_puts (ctx, "\\r");
    case '\t': return out_puts (ctx, "\\t");
    default:
      if (c < 0x20)
        return out_printf (ctx, "\\u%04x", (unsigned) c);
      return out_write (ctx, (const char *) &c, 1);
  }
}

static dds_return_t write_json_escaped_codepoint (struct print_ctx *ctx, uint32_t cp)
{
  switch (cp)
  {
    case '"': return out_puts (ctx, "\\\"");
    case '\\': return out_puts (ctx, "\\\\");
    case '\b': return out_puts (ctx, "\\b");
    case '\f': return out_puts (ctx, "\\f");
    case '\n': return out_puts (ctx, "\\n");
    case '\r': return out_puts (ctx, "\\r");
    case '\t': return out_puts (ctx, "\\t");
    default:
      if (cp < 0x20)
        return out_printf (ctx, "\\u%04x", (unsigned) cp);
      return write_utf8_codepoint (ctx, cp);
  }
}

static dds_return_t write_xml_escaped_byte (struct print_ctx *ctx, unsigned char c)
{
  switch (c)
  {
    case '&': return out_puts (ctx, "&amp;");
    case '<': return out_puts (ctx, "&lt;");
    case '>': return out_puts (ctx, "&gt;");
    default:
      if (c < 0x20 && c != '\n' && c != '\r' && c != '\t')
        return out_printf (ctx, "&#x%x;", (unsigned) c);
      return out_write (ctx, (const char *) &c, 1);
  }
}

static dds_return_t write_xml_escaped_codepoint (struct print_ctx *ctx, uint32_t cp)
{
  switch (cp)
  {
    case '&': return out_puts (ctx, "&amp;");
    case '<': return out_puts (ctx, "&lt;");
    case '>': return out_puts (ctx, "&gt;");
    default:
      if (cp < 0x20 && cp != '\n' && cp != '\r' && cp != '\t')
        return out_printf (ctx, "&#x%x;", (unsigned) cp);
      return write_utf8_codepoint (ctx, cp);
  }
}

static dds_return_t write_json_string_bytes (struct print_ctx *ctx, const unsigned char *s, size_t len)
{
  dds_return_t rc;
  if ((rc = out_puts (ctx, "\"")) != DDS_RETCODE_OK)
    return rc;
  for (size_t i = 0; i < len; i++)
    if ((rc = write_json_escaped_byte (ctx, s[i])) != DDS_RETCODE_OK)
      return rc;
  return out_puts (ctx, "\"");
}

static dds_return_t write_json_string_impl (struct print_ctx *ctx, const char *s, size_t max)
{
  dds_return_t rc;
  size_t len = 0;
  bool truncated = false;
  if ((rc = out_puts (ctx, "\"")) != DDS_RETCODE_OK)
    return rc;
  while (s[len] != 0)
  {
    if (max != 0 && len == max)
    {
      truncated = true;
      break;
    }
    if ((rc = write_json_escaped_byte (ctx, (unsigned char) s[len])) != DDS_RETCODE_OK)
      return rc;
    len++;
  }
  if (truncated && (rc = out_puts (ctx, "...")) != DDS_RETCODE_OK)
    return rc;
  return out_puts (ctx, "\"");
}

static dds_return_t write_json_string (struct print_ctx *ctx, const char *s)
{
  return write_json_string_impl (ctx, s, ctx->opts.max_string_chars);
}

static dds_return_t write_xml_string (struct print_ctx *ctx, const char *s)
{
  dds_return_t rc;
  const size_t max = ctx->opts.max_string_chars;
  size_t len = 0;
  bool truncated = false;
  while (s[len] != 0)
  {
    if (max != 0 && len == max)
    {
      truncated = true;
      break;
    }
    if ((rc = write_xml_escaped_byte (ctx, (unsigned char) s[len])) != DDS_RETCODE_OK)
      return rc;
    len++;
  }
  if (truncated)
    return out_puts (ctx, "...");
  return DDS_RETCODE_OK;
}

static dds_return_t write_json_wstring (struct print_ctx *ctx, const wchar_t *s)
{
  dds_return_t rc;
  const size_t max = ctx->opts.max_string_chars;
  size_t len = 0;
  bool truncated = false;
  if ((rc = out_puts (ctx, "\"")) != DDS_RETCODE_OK)
    return rc;
  while (s[len] != 0)
  {
    if (max != 0 && len == max)
    {
      truncated = true;
      break;
    }
    if ((rc = write_json_escaped_codepoint (ctx, (uint32_t) s[len])) != DDS_RETCODE_OK)
      return rc;
    len++;
  }
  if (truncated && (rc = out_puts (ctx, "...")) != DDS_RETCODE_OK)
    return rc;
  return out_puts (ctx, "\"");
}

static dds_return_t write_xml_wstring (struct print_ctx *ctx, const wchar_t *s)
{
  dds_return_t rc;
  const size_t max = ctx->opts.max_string_chars;
  size_t len = 0;
  bool truncated = false;
  while (s[len] != 0)
  {
    if (max != 0 && len == max)
    {
      truncated = true;
      break;
    }
    if ((rc = write_xml_escaped_codepoint (ctx, (uint32_t) s[len])) != DDS_RETCODE_OK)
      return rc;
    len++;
  }
  if (truncated)
    return out_puts (ctx, "...");
  return DDS_RETCODE_OK;
}

static dds_return_t begin_value (struct print_ctx *ctx, const char *label, const char **opened_xml_name)
{
  *opened_xml_name = NULL;
  if (is_json (ctx))
  {
    if (ctx->depth > 0)
    {
      struct container *parent = &ctx->stack[ctx->depth - 1];
      if (parent->count++ != 0)
      {
        dds_return_t rc = out_puts (ctx, ",");
        if (rc != DDS_RETCODE_OK)
          return rc;
      }
      if (parent->kind == CONTAINER_OBJECT)
      {
        if (label == NULL)
          return print_error (ctx, "missing member name while printing JSON sample");
        dds_return_t rc = write_json_string_impl (ctx, label, 0);
        if (rc != DDS_RETCODE_OK)
          return rc;
        return out_puts (ctx, ":");
      }
    }
    return DDS_RETCODE_OK;
  }
  else
  {
    const char *name = xml_value_name (ctx, label);
    if (name == NULL)
      return DDS_RETCODE_OK;
    dds_return_t rc = out_printf (ctx, "<%s>", name);
    if (rc == DDS_RETCODE_OK)
      *opened_xml_name = name;
    return rc;
  }
}

static dds_return_t end_value (struct print_ctx *ctx, const char *opened_xml_name)
{
  if (is_json (ctx) || opened_xml_name == NULL)
    return DDS_RETCODE_OK;
  return out_printf (ctx, "</%s>", opened_xml_name);
}

static dds_return_t begin_struct (struct print_ctx *ctx, const char *label, bool flatten, const char **opened_xml_name)
{
  *opened_xml_name = NULL;
  if (flatten)
    return DDS_RETCODE_OK;
  dds_return_t rc = begin_value (ctx, label, opened_xml_name);
  if (rc != DDS_RETCODE_OK)
    return rc;
  if (is_json (ctx) && (rc = out_puts (ctx, "{")) != DDS_RETCODE_OK)
    return rc;
  return push_container (ctx, CONTAINER_OBJECT);
}

static dds_return_t end_struct (struct print_ctx *ctx, bool flatten, const char *opened_xml_name)
{
  if (flatten)
    return DDS_RETCODE_OK;
  pop_container (ctx);
  dds_return_t rc = DDS_RETCODE_OK;
  if (is_json (ctx))
    rc = out_puts (ctx, "}");
  if (rc == DDS_RETCODE_OK)
    rc = end_value (ctx, opened_xml_name);
  return rc;
}

static dds_return_t begin_collection (struct print_ctx *ctx, const char *label, const char **opened_xml_name)
{
  dds_return_t rc = begin_value (ctx, label, opened_xml_name);
  if (rc != DDS_RETCODE_OK)
    return rc;
  if (is_json (ctx) && (rc = out_puts (ctx, "[")) != DDS_RETCODE_OK)
    return rc;
  return push_container (ctx, CONTAINER_COLLECTION);
}

static dds_return_t end_collection (struct print_ctx *ctx, const char *opened_xml_name)
{
  pop_container (ctx);
  dds_return_t rc = DDS_RETCODE_OK;
  if (is_json (ctx))
    rc = out_puts (ctx, "]");
  if (rc == DDS_RETCODE_OK)
    rc = end_value (ctx, opened_xml_name);
  return rc;
}

static dds_return_t emit_raw_scalar (struct print_ctx *ctx, const char *label, const char *text)
{
  const char *opened_xml_name;
  dds_return_t rc = begin_value (ctx, label, &opened_xml_name);
  if (rc != DDS_RETCODE_OK)
    return rc;
  rc = out_puts (ctx, text);
  if (rc == DDS_RETCODE_OK)
    rc = end_value (ctx, opened_xml_name);
  return rc;
}

static dds_return_t emit_null (struct print_ctx *ctx, const char *label)
{
  if (is_json (ctx))
    return emit_raw_scalar (ctx, label, "null");
  const char *opened_xml_name;
  dds_return_t rc = begin_value (ctx, label, &opened_xml_name);
  if (rc == DDS_RETCODE_OK)
    rc = end_value (ctx, opened_xml_name);
  return rc;
}

static dds_return_t emit_bool (struct print_ctx *ctx, const char *label, bool v)
{
  return emit_raw_scalar (ctx, label, v ? "true" : "false");
}

static dds_return_t emit_string (struct print_ctx *ctx, const char *label, const char *s)
{
  if (s == NULL)
    return emit_null (ctx, label);
  const char *opened_xml_name;
  dds_return_t rc = begin_value (ctx, label, &opened_xml_name);
  if (rc != DDS_RETCODE_OK)
    return rc;
  rc = is_json (ctx) ? write_json_string (ctx, s) : write_xml_string (ctx, s);
  if (rc == DDS_RETCODE_OK)
    rc = end_value (ctx, opened_xml_name);
  return rc;
}

static dds_return_t emit_string_bytes (struct print_ctx *ctx, const char *label, const unsigned char *s, size_t len)
{
  const char *opened_xml_name;
  dds_return_t rc = begin_value (ctx, label, &opened_xml_name);
  if (rc != DDS_RETCODE_OK)
    return rc;
  if (is_json (ctx))
    rc = write_json_string_bytes (ctx, s, len);
  else
  {
    for (size_t i = 0; rc == DDS_RETCODE_OK && i < len; i++)
      rc = write_xml_escaped_byte (ctx, s[i]);
  }
  if (rc == DDS_RETCODE_OK)
    rc = end_value (ctx, opened_xml_name);
  return rc;
}

static dds_return_t emit_wstring (struct print_ctx *ctx, const char *label, const wchar_t *s)
{
  if (s == NULL)
    return emit_null (ctx, label);
  const char *opened_xml_name;
  dds_return_t rc = begin_value (ctx, label, &opened_xml_name);
  if (rc != DDS_RETCODE_OK)
    return rc;
  rc = is_json (ctx) ? write_json_wstring (ctx, s) : write_xml_wstring (ctx, s);
  if (rc == DDS_RETCODE_OK)
    rc = end_value (ctx, opened_xml_name);
  return rc;
}

static dds_return_t emit_wchar (struct print_ctx *ctx, const char *label, wchar_t c)
{
  wchar_t s[2] = { c, 0 };
  return emit_wstring (ctx, label, s);
}

static dds_return_t emit_omitted (struct print_ctx *ctx, uint32_t n)
{
  if (n == 0)
    return DDS_RETCODE_OK;
  if (is_json (ctx))
  {
    const char *opened_xml_name;
    dds_return_t rc = begin_value (ctx, NULL, &opened_xml_name);
    if (rc != DDS_RETCODE_OK)
      return rc;
    (void) opened_xml_name;
    return out_printf (ctx, "{\"_omitted_items\":%"PRIu32"}", n);
  }
  return out_printf (ctx, "<!-- %"PRIu32" items omitted -->", n);
}

static struct collection_window collection_window (const struct print_ctx *ctx, uint32_t n)
{
  const uint32_t max = ctx->opts.max_collection_items;
  if (max == 0 || n <= max)
    return (struct collection_window){ .limited = false, .head = n, .tail = 0 };

  uint32_t tail = ctx->opts.collection_tail_items;
  if (tail >= max)
    tail = max / 2;
  return (struct collection_window){ .limited = true, .head = max - tail, .tail = tail };
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

static bool simple_kind (uint8_t disc)
{
  switch (disc)
  {
    case DDS_XTypes_TK_BOOLEAN:
    case DDS_XTypes_TK_CHAR8:
    case DDS_XTypes_TK_CHAR16:
    case DDS_XTypes_TK_INT8:
    case DDS_XTypes_TK_INT16:
    case DDS_XTypes_TK_INT32:
    case DDS_XTypes_TK_INT64:
    case DDS_XTypes_TK_BYTE:
    case DDS_XTypes_TK_UINT8:
    case DDS_XTypes_TK_UINT16:
    case DDS_XTypes_TK_UINT32:
    case DDS_XTypes_TK_UINT64:
    case DDS_XTypes_TK_FLOAT32:
    case DDS_XTypes_TK_FLOAT64:
    case DDS_XTypes_TK_FLOAT128:
    case DDS_XTypes_TK_STRING8:
    case DDS_XTypes_TK_STRING16:
      return true;
  }
  return false;
}

static dds_return_t emit_float128 (struct print_ctx *ctx, const char *label, const unsigned char *v)
{
  dtl_float128_t f128;
  char buf[DTL_FLOAT128_STRING_BUFSZ];
  memcpy (&f128, v, sizeof (f128));
  const dds_return_t rc = dtl_float128_to_string (buf, sizeof (buf), &f128);
  if (rc != DDS_RETCODE_OK)
    return rc;
  if (is_json (ctx) && (strcmp (buf, "nan") == 0 || strcmp (buf, "inf") == 0 || strcmp (buf, "-inf") == 0))
    return emit_string (ctx, label, buf);
  return emit_raw_scalar (ctx, label, buf);
}

static dds_return_t emit_float32 (struct print_ctx *ctx, const char *label, float v)
{
  char buf[DTL_FLOAT128_STRING_BUFSZ];
  const dds_return_t rc = dtl_float32_to_string (buf, sizeof (buf), v);
  if (rc != DDS_RETCODE_OK)
    return rc;
  if (is_json (ctx) && (strcmp (buf, "nan") == 0 || strcmp (buf, "inf") == 0 || strcmp (buf, "-inf") == 0))
    return emit_string (ctx, label, buf);
  return emit_raw_scalar (ctx, label, buf);
}

static dds_return_t emit_float64 (struct print_ctx *ctx, const char *label, double v)
{
  char buf[DTL_FLOAT128_STRING_BUFSZ];
  const dds_return_t rc = dtl_float64_to_string (buf, sizeof (buf), v);
  if (rc != DDS_RETCODE_OK)
    return rc;
  if (is_json (ctx) && (strcmp (buf, "nan") == 0 || strcmp (buf, "inf") == 0 || strcmp (buf, "-inf") == 0))
    return emit_string (ctx, label, buf);
  return emit_raw_scalar (ctx, label, buf);
}

static dds_return_t print_simple (struct print_ctx *ctx, const unsigned char *obj, uint8_t disc, const char *label, bool direct_string)
{
  switch (disc)
  {
    case DDS_XTypes_TK_BOOLEAN:
      return emit_bool (ctx, label, *((const uint8_t *) obj) != 0);
    case DDS_XTypes_TK_CHAR8: {
      const unsigned char c = *((const unsigned char *) obj);
      return emit_string_bytes (ctx, label, &c, 1);
    }
    case DDS_XTypes_TK_CHAR16:
      return emit_wchar (ctx, label, *((const wchar_t *) obj));
    default:
      break;
  }

  char buf[64];
  switch (disc)
  {
    case DDS_XTypes_TK_INT8:
      (void) snprintf (buf, sizeof (buf), "%"PRId8, *((const int8_t *) obj));
      return emit_raw_scalar (ctx, label, buf);
    case DDS_XTypes_TK_INT16:
      (void) snprintf (buf, sizeof (buf), "%"PRId16, *((const int16_t *) obj));
      return emit_raw_scalar (ctx, label, buf);
    case DDS_XTypes_TK_INT32:
      (void) snprintf (buf, sizeof (buf), "%"PRId32, *((const int32_t *) obj));
      return emit_raw_scalar (ctx, label, buf);
    case DDS_XTypes_TK_INT64:
      (void) snprintf (buf, sizeof (buf), "%"PRId64, *((const int64_t *) obj));
      return emit_raw_scalar (ctx, label, buf);
    case DDS_XTypes_TK_BYTE:
    case DDS_XTypes_TK_UINT8:
      (void) snprintf (buf, sizeof (buf), "%"PRIu8, *((const uint8_t *) obj));
      return emit_raw_scalar (ctx, label, buf);
    case DDS_XTypes_TK_UINT16:
      (void) snprintf (buf, sizeof (buf), "%"PRIu16, *((const uint16_t *) obj));
      return emit_raw_scalar (ctx, label, buf);
    case DDS_XTypes_TK_UINT32:
      (void) snprintf (buf, sizeof (buf), "%"PRIu32, *((const uint32_t *) obj));
      return emit_raw_scalar (ctx, label, buf);
    case DDS_XTypes_TK_UINT64:
      (void) snprintf (buf, sizeof (buf), "%"PRIu64, *((const uint64_t *) obj));
      return emit_raw_scalar (ctx, label, buf);
    case DDS_XTypes_TK_FLOAT32:
      return emit_float32 (ctx, label, *((const float *) obj));
    case DDS_XTypes_TK_FLOAT64:
      return emit_float64 (ctx, label, *((const double *) obj));
    case DDS_XTypes_TK_FLOAT128:
      return emit_float128 (ctx, label, obj);
    case DDS_XTypes_TK_STRING8:
      return emit_string (ctx, label, direct_string ? (const char *) obj : *((const char * const *) obj));
    case DDS_XTypes_TK_STRING16:
      return emit_wstring (ctx, label, direct_string ? (const wchar_t *) obj : *((const wchar_t * const *) obj));
  }
  abort ();
}

static dds_return_t print_ti (struct print_ctx *ctx, const unsigned char *obj, const DDS_XTypes_TypeIdentifier *typeid, const char *label, bool direct_string, bool flatten_struct);
static dds_return_t print_to (struct print_ctx *ctx, const unsigned char *obj, const DDS_XTypes_CompleteTypeObject *typeobj, const char *label, bool direct_string, bool flatten_struct);

static const char *fallback_name (char *buf, size_t bufsz, const char *name, const char *prefix, uint32_t idx)
{
  if (name != NULL && *name != 0)
    return name;
  (void) snprintf (buf, bufsz, "%s%"PRIu32, prefix, idx);
  return buf;
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

static void advance_array_rank (struct print_ctx *ctx, const unsigned char *obj, const DDS_XTypes_TypeIdentifier *typeid, uint32_t rank, size_t *off)
{
  struct array_info info = { 0 };
  if (!get_array_info (typeid, &info))
    abort ();
  for (uint32_t i = 0; i < array_bound (&info, rank); i++)
  {
    if (rank + 1 < info.nbounds)
      advance_array_rank (ctx, obj, typeid, rank + 1, off);
    else
      (void) dtl_advance_ti (ctx->dtl, (unsigned char *) obj, off, info.elem_type, false);
  }
}

static dds_return_t print_array_json_rank (struct print_ctx *ctx, const unsigned char *obj, const DDS_XTypes_TypeIdentifier *typeid, uint32_t rank, size_t *off, const char *label)
{
  struct array_info info = { 0 };
  if (!get_array_info (typeid, &info))
    abort ();
  const char *opened_xml_name;
  dds_return_t rc = begin_collection (ctx, label, &opened_xml_name);
  if (rc != DDS_RETCODE_OK)
    return rc;

  const uint32_t n = array_bound (&info, rank);
  const struct collection_window win = collection_window (ctx, n);
  for (uint32_t i = 0; i < n; i++)
  {
    if (win.limited && i == win.head)
    {
      const uint32_t end = n - win.tail;
      if ((rc = emit_omitted (ctx, end - i)) != DDS_RETCODE_OK)
        return rc;
      while (i < end)
      {
        if (rank + 1 < info.nbounds)
          advance_array_rank (ctx, obj, typeid, rank + 1, off);
        else
          (void) dtl_advance_ti (ctx->dtl, (unsigned char *) obj, off, info.elem_type, false);
        i++;
      }
      i--;
      continue;
    }
    if (rank + 1 < info.nbounds)
      rc = print_array_json_rank (ctx, obj, typeid, rank + 1, off, NULL);
    else
    {
      const unsigned char *elem = dtl_advance_ti (ctx->dtl, (unsigned char *) obj, off, info.elem_type, false);
      rc = print_ti (ctx, elem, info.elem_type, NULL, false, false);
    }
    if (rc != DDS_RETCODE_OK)
      return rc;
  }
  return end_collection (ctx, opened_xml_name);
}

static dds_return_t print_array_xml_flat (struct print_ctx *ctx, const unsigned char *obj, const DDS_XTypes_TypeIdentifier *typeid, const char *label)
{
  struct array_info info = { 0 };
  if (!get_array_info (typeid, &info))
    abort ();
  uint32_t n = 1;
  for (uint32_t i = 0; i < info.nbounds; i++)
    n *= array_bound (&info, i);

  const char *opened_xml_name;
  dds_return_t rc = begin_collection (ctx, label, &opened_xml_name);
  if (rc != DDS_RETCODE_OK)
    return rc;

  size_t off = 0;
  const struct collection_window win = collection_window (ctx, n);
  for (uint32_t i = 0; i < n; i++)
  {
    if (win.limited && i == win.head)
    {
      const uint32_t end = n - win.tail;
      if ((rc = emit_omitted (ctx, end - i)) != DDS_RETCODE_OK)
        return rc;
      while (i < end)
      {
        (void) dtl_advance_ti (ctx->dtl, (unsigned char *) obj, &off, info.elem_type, false);
        i++;
      }
      i--;
      continue;
    }
    const unsigned char *elem = dtl_advance_ti (ctx->dtl, (unsigned char *) obj, &off, info.elem_type, false);
    if ((rc = print_ti (ctx, elem, info.elem_type, NULL, false, false)) != DDS_RETCODE_OK)
      return rc;
  }
  return end_collection (ctx, opened_xml_name);
}

static dds_return_t print_array (struct print_ctx *ctx, const unsigned char *obj, const DDS_XTypes_TypeIdentifier *typeid, const char *label)
{
  if (is_json (ctx))
  {
    size_t off = 0;
    return print_array_json_rank (ctx, obj, typeid, 0, &off, label);
  }
  return print_array_xml_flat (ctx, obj, typeid, label);
}

static dds_return_t print_sequence (struct print_ctx *ctx, const unsigned char *obj, const DDS_XTypes_TypeIdentifier *elem_type, const char *label)
{
  const dds_sequence_t *seq = (const dds_sequence_t *) obj;
  const char *opened_xml_name;
  dds_return_t rc = begin_collection (ctx, label, &opened_xml_name);
  if (rc != DDS_RETCODE_OK)
    return rc;

  size_t off = 0;
  const uint32_t n = seq->_length;
  const struct collection_window win = collection_window (ctx, n);
  for (uint32_t i = 0; i < n; i++)
  {
    if (win.limited && i == win.head)
    {
      const uint32_t end = n - win.tail;
      if ((rc = emit_omitted (ctx, end - i)) != DDS_RETCODE_OK)
        return rc;
      while (i < end)
      {
        (void) dtl_advance_ti (ctx->dtl, (unsigned char *) seq->_buffer, &off, elem_type, false);
        i++;
      }
      i--;
      continue;
    }
    const unsigned char *elem = dtl_advance_ti (ctx->dtl, (unsigned char *) seq->_buffer, &off, elem_type, false);
    if ((rc = print_ti (ctx, elem, elem_type, NULL, false, false)) != DDS_RETCODE_OK)
      return rc;
  }
  return end_collection (ctx, opened_xml_name);
}

static dds_return_t print_struct (struct print_ctx *ctx, const unsigned char *obj, const DDS_XTypes_CompleteStructType *type, const char *label, bool flatten)
{
  const char *opened_xml_name;
  dds_return_t rc = begin_struct (ctx, label, flatten, &opened_xml_name);
  if (rc != DDS_RETCODE_OK)
    return rc;

  size_t off = 0;
  if (type->header.base_type._d != DDS_XTypes_TK_NONE)
  {
    const unsigned char *base = dtl_advance_ti (ctx->dtl, (unsigned char *) obj, &off, &type->header.base_type, false);
    if ((rc = print_ti (ctx, base, &type->header.base_type, NULL, false, true)) != DDS_RETCODE_OK)
      return rc;
  }

  const bool parent_key_path = ctx->key_path;
  for (uint32_t i = 0; i < type->member_seq._length; i++)
  {
    const DDS_XTypes_CompleteStructMember *m = &type->member_seq._buffer[i];
    const bool is_indirect = is_indirect_member (m->common.member_flags);
    const unsigned char *member = dtl_advance_ti (ctx->dtl, (unsigned char *) obj, &off, &m->common.member_type_id, is_indirect);
    const bool member_key_path = parent_key_path && ((m->common.member_flags & DDS_XTypes_IS_KEY) != 0);
    if (!ctx->valid_data && !member_key_path)
      continue;

    char namebuf[32];
    const char *name = fallback_name (namebuf, sizeof (namebuf), m->detail.name, "member", i);
    ctx->key_path = member_key_path;
    if (is_indirect)
    {
      void const * const *p = (void const * const *) member;
      if (*p != NULL)
      {
        ctx->key_path = member_key_path && !is_optional_member (m->common.member_flags);
        const bool direct_string = dtl_is_unbounded_string_ti (&m->common.member_type_id);
        rc = print_ti (ctx, *p, &m->common.member_type_id, name, direct_string, false);
      }
    }
    else
    {
      rc = print_ti (ctx, member, &m->common.member_type_id, name, false, false);
    }
    ctx->key_path = parent_key_path;
    if (rc != DDS_RETCODE_OK)
      return rc;
  }
  ctx->key_path = parent_key_path;
  return end_struct (ctx, flatten, opened_xml_name);
}

static dds_return_t print_enum (struct print_ctx *ctx, const unsigned char *obj, const DDS_XTypes_CompleteEnumeratedType *type, const char *label)
{
  const int32_t value = *((const int32_t *) obj);
  for (uint32_t i = 0; i < type->literal_seq._length; i++)
  {
    const DDS_XTypes_CompleteEnumeratedLiteral *lit = &type->literal_seq._buffer[i];
    if (lit->common.value == value)
      return emit_string (ctx, label, lit->detail.name);
  }
  char buf[32];
  (void) snprintf (buf, sizeof (buf), "%"PRId32, value);
  return emit_raw_scalar (ctx, label, buf);
}

static bool read_discriminator (struct print_ctx *ctx, const unsigned char *obj, const DDS_XTypes_TypeIdentifier *typeid, int32_t *value)
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
      struct typeinfo *info = type_cache_lookup_typeid (ctx->dtl->typecache, typeid);
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

static dds_return_t print_discriminator (struct print_ctx *ctx, const unsigned char *obj, const DDS_XTypes_TypeIdentifier *typeid, const char *label)
{
  switch (typeid->_d)
  {
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT: {
      struct typeinfo *info = type_cache_lookup_typeid (ctx->dtl->typecache, typeid);
      return print_to (ctx, obj, info->typeobj, label, false, false);
    }
    default:
      return print_ti (ctx, obj, typeid, label, false, false);
  }
}

static dds_return_t print_union (struct print_ctx *ctx, const unsigned char *obj, const DDS_XTypes_CompleteUnionType *type, const char *label)
{
  const char *opened_xml_name;
  dds_return_t rc = begin_struct (ctx, label, false, &opened_xml_name);
  if (rc != DDS_RETCODE_OK)
    return rc;

  int32_t disc_value;
  if (!read_discriminator (ctx, obj, &type->discriminator.common.type_id, &disc_value))
    return print_error (ctx, "unsupported union discriminator type");

  const char *disc_label = is_json (ctx) ? "_d" : "discriminator";
  if ((rc = print_discriminator (ctx, obj, &type->discriminator.common.type_id, disc_label)) != DDS_RETCODE_OK)
    return rc;

  const DDS_XTypes_CompleteUnionMember *m = find_union_member_for_disc (type, disc_value);
  if (m != NULL)
  {
    const unsigned char *data = obj + type_cache_union_data_offset (ctx->dtl->typecache, type);
    size_t off = 0;
    const bool is_indirect = is_indirect_member (m->common.member_flags);
    const unsigned char *member = dtl_advance_ti (ctx->dtl, (unsigned char *) data, &off, &m->common.type_id, is_indirect);
    char namebuf[32];
    const char *name = fallback_name (namebuf, sizeof (namebuf), m->detail.name, "case", 0);
    const bool parent_key_path = ctx->key_path;
    if (is_indirect)
    {
      void const * const *p = (void const * const *) member;
      if (*p != NULL)
      {
        ctx->key_path = parent_key_path && !is_optional_member (m->common.member_flags);
        const bool direct_string = dtl_is_unbounded_string_ti (&m->common.type_id);
        rc = print_ti (ctx, *p, &m->common.type_id, name, direct_string, false);
      }
    }
    else
    {
      rc = print_ti (ctx, member, &m->common.type_id, name, false, false);
    }
    ctx->key_path = parent_key_path;
    if (rc != DDS_RETCODE_OK)
      return rc;
  }

  return end_struct (ctx, false, opened_xml_name);
}

static dds_return_t print_ti (struct print_ctx *ctx, const unsigned char *obj, const DDS_XTypes_TypeIdentifier *typeid, const char *label, bool direct_string, bool flatten_struct)
{
  if (!visible (ctx))
    return DDS_RETCODE_OK;

  if (simple_kind (typeid->_d))
    return print_simple (ctx, obj, typeid->_d, label, direct_string);

  switch (typeid->_d)
  {
    case DDS_XTypes_TI_STRING8_SMALL:
    case DDS_XTypes_TI_STRING8_LARGE: {
      const char *s = dtl_is_bounded_string_ti (typeid) ? (const char *) obj : direct_string ? (const char *) obj : *((const char * const *) obj);
      return emit_string (ctx, label, s);
    }
    case DDS_XTypes_TI_STRING16_SMALL:
    case DDS_XTypes_TI_STRING16_LARGE: {
      const wchar_t *s = dtl_is_bounded_string_ti (typeid) ? (const wchar_t *) obj : direct_string ? (const wchar_t *) obj : *((const wchar_t * const *) obj);
      return emit_wstring (ctx, label, s);
    }
    case DDS_XTypes_TI_PLAIN_SEQUENCE_SMALL:
      return print_sequence (ctx, obj, typeid->_u.seq_sdefn.element_identifier, label);
    case DDS_XTypes_TI_PLAIN_SEQUENCE_LARGE:
      return print_sequence (ctx, obj, typeid->_u.seq_ldefn.element_identifier, label);
    case DDS_XTypes_TI_PLAIN_ARRAY_SMALL:
    case DDS_XTypes_TI_PLAIN_ARRAY_LARGE:
      return print_array (ctx, obj, typeid, label);
    case DDS_XTypes_EK_COMPLETE:
    case DDS_XTypes_TI_STRONGLY_CONNECTED_COMPONENT: {
      struct typeinfo *info = type_cache_lookup_typeid (ctx->dtl->typecache, typeid);
      return print_to (ctx, obj, info->typeobj, label, direct_string, flatten_struct);
    }
  }

  return print_error (ctx, "unsupported type identifier discriminator %u", (unsigned) typeid->_d);
}

static dds_return_t print_to (struct print_ctx *ctx, const unsigned char *obj, const DDS_XTypes_CompleteTypeObject *typeobj, const char *label, bool direct_string, bool flatten_struct)
{
  if (!visible (ctx))
    return DDS_RETCODE_OK;

  if (simple_kind (typeobj->_d))
    return print_simple (ctx, obj, typeobj->_d, label, direct_string);

  switch (typeobj->_d)
  {
    case DDS_XTypes_TK_ALIAS:
      return print_ti (ctx, obj, &typeobj->_u.alias_type.body.common.related_type, label, direct_string, flatten_struct);
    case DDS_XTypes_TK_SEQUENCE:
      return print_sequence (ctx, obj, &typeobj->_u.sequence_type.element.common.type, label);
    case DDS_XTypes_TK_STRUCTURE:
      return print_struct (ctx, obj, &typeobj->_u.struct_type, label, flatten_struct);
    case DDS_XTypes_TK_ENUM:
      return print_enum (ctx, obj, &typeobj->_u.enumerated_type, label);
    case DDS_XTypes_TK_BITMASK: {
      char buf[32];
      const uint64_t value = read_bitmask_value (obj, typeobj->_u.bitmask_type.header.common.bit_bound);
      (void) snprintf (buf, sizeof (buf), "%"PRIu64, value);
      return emit_raw_scalar (ctx, label, buf);
    }
    case DDS_XTypes_TK_UNION:
      return print_union (ctx, obj, &typeobj->_u.union_type, label);
  }

  return print_error (ctx, "unsupported type object discriminator %u", (unsigned) typeobj->_d);
}

static dds_return_t stdout_string_print (const char *s)
{
  return fputs (s, stdout) >= 0 ? DDS_RETCODE_OK : DDS_RETCODE_ERROR;
}

dds_return_t dtl_print_sample_to (struct dyntypelib *dtl, bool valid_data, const void *sample, const DDS_XTypes_CompleteTypeObject *typeobj, const struct dtl_sample_print_options *opts, const struct dtl_sample_output *out, struct dyntypelib_error *err)
{
  if (dtl == NULL || sample == NULL || typeobj == NULL || out == NULL || out->write == NULL)
  {
    if (err)
      (void) snprintf (err->errmsg, sizeof (err->errmsg), "invalid sample print argument");
    return DDS_RETCODE_BAD_PARAMETER;
  }

  struct dtl_sample_print_options use_opts = {
    .format = DTL_SAMPLE_FORMAT_JSON,
    .max_output_bytes = 0,
    .max_string_chars = 0,
    .max_collection_items = 0,
    .collection_tail_items = 0,
    .trailing_newline = false
  };
  if (opts)
    use_opts = *opts;
  if (use_opts.format != DTL_SAMPLE_FORMAT_JSON && use_opts.format != DTL_SAMPLE_FORMAT_XML)
  {
    if (err)
      (void) snprintf (err->errmsg, sizeof (err->errmsg), "invalid sample output format");
    return DDS_RETCODE_BAD_PARAMETER;
  }

  struct print_ctx ctx = {
    .dtl = dtl,
    .valid_data = valid_data,
    .key_path = true,
    .opts = use_opts,
    .out = out,
    .written = 0,
    .err = err,
    .depth = 0
  };

  dds_return_t rc;
  if (use_opts.format == DTL_SAMPLE_FORMAT_XML)
  {
    if ((rc = out_puts (&ctx, "<sample>")) != DDS_RETCODE_OK)
      return rc;
    if ((rc = print_to (&ctx, sample, typeobj, NULL, false, false)) != DDS_RETCODE_OK)
      return rc;
    if ((rc = out_puts (&ctx, "</sample>")) != DDS_RETCODE_OK)
      return rc;
  }
  else
  {
    if ((rc = print_to (&ctx, sample, typeobj, NULL, false, false)) != DDS_RETCODE_OK)
      return rc;
  }
  if (use_opts.trailing_newline)
    rc = out_puts (&ctx, "\n");
  return rc;
}

dds_return_t dtl_print_sample_to_string (struct dyntypelib *dtl, bool valid_data, const void *sample, const DDS_XTypes_CompleteTypeObject *typeobj, const struct dtl_sample_print_options *opts, char **str, size_t *len, struct dyntypelib_error *err)
{
  if (str == NULL)
  {
    if (err)
      (void) snprintf (err->errmsg, sizeof (err->errmsg), "invalid sample print string argument");
    return DDS_RETCODE_BAD_PARAMETER;
  }
  *str = NULL;
  if (len)
    *len = 0;

  struct string_output string_out = { 0 };
  const struct dtl_sample_output out = {
    .write = string_output_write,
    .state = &string_out
  };
  dds_return_t rc = dtl_print_sample_to (dtl, valid_data, sample, typeobj, opts, &out, err);
  if (rc != DDS_RETCODE_OK)
  {
    ddsrt_free (string_out.buf);
    return rc;
  }
  if (string_out.buf == NULL)
  {
    string_out.buf = ddsrt_malloc (1);
    if (string_out.buf == NULL)
      return DDS_RETCODE_OUT_OF_RESOURCES;
    string_out.buf[0] = 0;
  }
  *str = string_out.buf;
  if (len)
    *len = string_out.len;
  return DDS_RETCODE_OK;
}

void dtl_print_sample (struct dyntypelib *dtl, bool valid_data, const void *sample, const DDS_XTypes_CompleteTypeObject *typeobj)
{
  struct dyntypelib_error err = { .errmsg = "" };
  char *str = NULL;
  const struct dtl_sample_print_options opts = {
    .format = DTL_SAMPLE_FORMAT_JSON,
    .trailing_newline = true
  };
  dds_return_t rc = dtl_print_sample_to_string (dtl, valid_data, sample, typeobj, &opts, &str, NULL, &err);
  if (rc == DDS_RETCODE_OK)
  {
    (void) stdout_string_print (str);
    ddsrt_free (str);
  }
  else
  {
    printf ("(sample print failed: %s)\n", err.errmsg);
  }
}
