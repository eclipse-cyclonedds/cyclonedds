// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include <inttypes.h>

#include "idl/heap.h"
#include "idl/print.h"
#include "idl/string.h"

#if defined(_MSC_VER)
# define idl_thread_local __declspec(thread)
#elif defined(__GNUC__) || (defined(__clang__) && __clang_major__ >= 2)
# define idl_thread_local __thread
#elif defined(__SUNPROC_C) || defined(__SUNPRO_CC)
# define idl_thread_local __thread
#else
# error "Thread-local storage is not supported"
#endif

struct printa {
  idl_print_t print;
  const void *object;
  void *user_data;
  size_t size;
  char *str, **strp;
};

static idl_thread_local struct printa printa;

int idl_printa_arguments__(
  char **strp, idl_print_t print, const void *object, void *user_data)
{
  int cnt;
  char buf[1];

  assert(strp);
  assert(print);
  assert(object);

  if ((cnt = print(buf, sizeof(buf), object, user_data)) < 0)
    return cnt;

  printa.print = print;
  printa.object = object;
  printa.user_data = user_data;
  printa.size = (size_t)cnt + 1;
  printa.strp = strp;
  printa.str = NULL;

  return cnt;
}

size_t idl_printa_size__(void)
{
  assert(!printa.str);
  return printa.size;
}

char **idl_printa_strp__(void)
{
  return &printa.str;
}

int idl_printa__(void)
{
  int cnt;

  assert(printa.size);
  assert(printa.str);
  assert(printa.strp);

  cnt = printa.print(
    printa.str, printa.size, printa.object, printa.user_data);
  if (cnt >= 0)
    *printa.strp = printa.str;
  printa.print = 0;
  printa.object = NULL;
  printa.user_data = NULL;
  printa.size = 0;
  printa.str = NULL;
  printa.strp = NULL;
  return cnt;
}

int idl_print__(
  char **strp, idl_print_t print, const void *object, void *user_data)
{
  int cnt, len;
  char buf[1], *str = NULL;

  if ((len = print(buf, sizeof(buf), object, user_data)) < 0)
    return len;
  if (!(str = idl_malloc((size_t)len + 1)))
    return -1;
  if ((cnt = print(str, (size_t)len + 1, object, user_data)) >= 0)
    *strp = str;
  else
    idl_free(str);
  return cnt;
}

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
