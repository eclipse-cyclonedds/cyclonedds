// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "common.h"
#include "idl/processor.h"
#include "idl/heap.h"
#include "idl/print.h"
#include "idl/stream.h"
#include "idl/string.h"
#include "idl/misc.h"

static void copy(
  char *dest, size_t off, size_t size, const char *src, size_t len)
{
  size_t cnt;

  if (off >= size)
    return;
  cnt = size - off;
  cnt = cnt > len ? len : cnt;
  memmove(dest+off, src, cnt);
}

static int print_base_type(
  char *str, size_t size, const void *node, void *user_data)
{
  const char *type;

  (void)user_data;

  switch (idl_type(node)) {
    case IDL_BOOL:    type = "bool";        break;
    case IDL_CHAR:    type = "char";        break;
    case IDL_INT8:    type = "int8_t";      break;
    case IDL_OCTET:
    case IDL_UINT8:   type = "uint8_t";     break;
    case IDL_SHORT:
    case IDL_INT16:   type = "int16_t";     break;
    case IDL_USHORT:
    case IDL_UINT16:  type = "uint16_t";    break;
    case IDL_LONG:
    case IDL_INT32:   type = "int32_t";     break;
    case IDL_ULONG:
    case IDL_UINT32:  type = "uint32_t";    break;
    case IDL_LLONG:
    case IDL_INT64:   type = "int64_t";     break;
    case IDL_ULLONG:
    case IDL_UINT64:  type = "uint64_t";    break;
    case IDL_FLOAT:   type = "float";       break;
    case IDL_DOUBLE:  type = "double";      break;
    case IDL_LDOUBLE: type = "long double"; break;
    case IDL_STRING:  type = "char";        break;
    default:
      abort();
  }

  return idl_snprintf(str, size, "%s", type);
}

static int print_decl_type(
  char *str, size_t size, const void *node, void *user_data)
{
  const char *ident, *sep = user_data;
  size_t cnt, off, len = 0;

  assert(sep);
  for (const idl_node_t *n = node; n; n = n->parent) {
    if ((idl_mask(n) & IDL_TYPEDEF) == IDL_TYPEDEF)
      continue;
    if ((idl_mask(n) & IDL_ENUM) == IDL_ENUM && n != node)
      continue;
    if ((idl_mask(n) & IDL_BITMASK) == IDL_BITMASK && n != node)
      continue;
    ident = idl_identifier(n);
    assert(ident);
    len += strlen(ident) + (len ? strlen(sep) : 0);
  }

  off = len;
  for (const idl_node_t *n = node; n; n = n->parent) {
    if ((idl_mask(n) & IDL_TYPEDEF) == IDL_TYPEDEF)
      continue;
    if ((idl_mask(n) & IDL_ENUM) == IDL_ENUM && n != node)
      continue;
    if ((idl_mask(n) & IDL_BITMASK) == IDL_BITMASK && n != node)
      continue;
    ident = idl_identifier(n);
    assert(ident);
    cnt = strlen(ident);
    assert(cnt <= off);
    off -= cnt;
    copy(str, off, size, ident, cnt);
    if (off == 0)
      break;
    cnt = strlen(sep);
    off -= cnt;
    copy(str, off, size, sep, cnt);
  }
  str[ (size > len ? len : size - 1) ] = '\0';
  return (int)len;
}

static int print_templ_type(
  char *str, size_t size, const void *node, void *user_data)
{
  const idl_type_spec_t *type_spec;
  char dims[32], *name = NULL;
  const char *seg, *type;
  size_t cnt, len = 0, seq = 0;

  (void)user_data;
  if (idl_type(node) == IDL_STRING)
    return idl_snprintf(str, size, "char");
  /* sequences require a little magic */
  assert(idl_type(node) == IDL_SEQUENCE);

  dims[0] = '\0';
  type_spec = idl_type_spec(node);
  for (; idl_is_sequence(type_spec); type_spec = idl_type_spec(type_spec))
    seq++;

  if (idl_is_base_type(type_spec) || idl_is_string(type_spec)) {
    switch (idl_type(type_spec)) {
      case IDL_BOOL:     type = "bool";                break;
      case IDL_CHAR:     type = "char";                break;
      case IDL_INT8:     type = "int8";                break;
      case IDL_OCTET:    type = "octet";               break;
      case IDL_UINT8:    type = "uint8";               break;
      case IDL_SHORT:    type = "short";               break;
      case IDL_INT16:    type = "int16";               break;
      case IDL_USHORT:   type = "unsigned_short";      break;
      case IDL_UINT16:   type = "uint16";              break;
      case IDL_LONG:     type = "long";                break;
      case IDL_INT32:    type = "int32";               break;
      case IDL_ULONG:    type = "unsigned_long";       break;
      case IDL_UINT32:   type = "uint32";              break;
      case IDL_LLONG:    type = "long_long";           break;
      case IDL_INT64:    type = "int64";               break;
      case IDL_ULLONG:   type = "unsigned_long_long";  break;
      case IDL_UINT64:   type = "uint64";              break;
      case IDL_FLOAT:    type = "float";               break;
      case IDL_DOUBLE:   type = "double";              break;
      case IDL_LDOUBLE:  type = "long_double";         break;
      case IDL_STRING:
        if (idl_is_bounded(type_spec))
          idl_snprintf(dims, sizeof(dims), "%"PRIu32, idl_bound(type_spec));
        type = "string";
        break;
      default:
        abort();
    }
  } else {
    if (IDL_PRINT(&name, print_type, type_spec) < 0)
      return -1;
    type = name;
  }

  seg = "dds_sequence_";
  cnt = strlen(seg);
  copy(str, len, size, seg, cnt);
  len += cnt;
  seg = "sequence_";
  cnt = strlen(seg);
  for (; seq; len += cnt, seq--)
    copy(str, len, size, seg, cnt);
  cnt = strlen(type);
  copy(str, len, size, type, cnt);
  len += cnt;
  cnt = strlen(dims);
  copy(str, len, size, dims, cnt);
  len += cnt;
  str[ (size > len ? len : size - 1) ] = '\0';
  if (name)
    idl_free(name);
  return (int)len;
}

int print_type(char *str, size_t size, const void *ptr, void *user_data)
{
  if (idl_is_base_type(ptr))
    return print_base_type(str, size, ptr, user_data);
  if (idl_is_templ_type(ptr))
    return print_templ_type(str, size, ptr, user_data);
  return print_decl_type(str, size, ptr, "_");
}

int print_scoped_name(char *str, size_t size, const void *ptr, void *user_data)
{
  if (idl_is_base_type(ptr))
    return print_base_type(str, size, ptr, user_data);
  if (idl_is_templ_type(ptr))
    return print_templ_type(str, size, ptr, user_data);
  return print_decl_type(str, size, ptr, "::");
}
