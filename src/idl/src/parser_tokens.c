// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "idl/processor.h"
#include "idl/string.h"
#include "parser_tokens.h"

typedef struct idl_keyword {
  const char *name;
  int32_t token;
  bool extended;
} idl_keyword_t;

static const idl_keyword_t idl_keywords[] = {
  { "module", IDL_TOKEN_MODULE, false },
  { "const", IDL_TOKEN_CONST, false },
  { "native", IDL_TOKEN_NATIVE, false },
  { "struct", IDL_TOKEN_STRUCT, false },
  { "typedef", IDL_TOKEN_TYPEDEF, false },
  { "union", IDL_TOKEN_UNION, false },
  { "switch", IDL_TOKEN_SWITCH, false },
  { "case", IDL_TOKEN_CASE, false },
  { "default", IDL_TOKEN_DEFAULT, false },
  { "enum", IDL_TOKEN_ENUM, false },
  { "unsigned", IDL_TOKEN_UNSIGNED, false },
  { "fixed", IDL_TOKEN_FIXED, false },
  { "sequence", IDL_TOKEN_SEQUENCE, false },
  { "string", IDL_TOKEN_STRING, false },
  { "wstring", IDL_TOKEN_WSTRING, false },
  { "float", IDL_TOKEN_FLOAT, false },
  { "double", IDL_TOKEN_DOUBLE, false },
  { "short", IDL_TOKEN_SHORT, false },
  { "long", IDL_TOKEN_LONG, false },
  { "char", IDL_TOKEN_CHAR, false },
  { "wchar", IDL_TOKEN_WCHAR, false },
  { "boolean", IDL_TOKEN_BOOLEAN, false },
  { "octet", IDL_TOKEN_OCTET, false },
  { "any", IDL_TOKEN_ANY, false },
  { "map", IDL_TOKEN_MAP, true },
  { "bitset", IDL_TOKEN_BITSET, false },
  { "bitfield", IDL_TOKEN_BITFIELD, false },
  { "bitmask", IDL_TOKEN_BITMASK, false },
  { "int8", IDL_TOKEN_INT8, true },
  { "int16", IDL_TOKEN_INT16, true },
  { "int32", IDL_TOKEN_INT32, true },
  { "int64", IDL_TOKEN_INT64, true },
  { "uint8", IDL_TOKEN_UINT8, true },
  { "uint16", IDL_TOKEN_UINT16, true },
  { "uint32", IDL_TOKEN_UINT32, true },
  { "uint64", IDL_TOKEN_UINT64, true },
  { "TRUE", IDL_TOKEN_TRUE, false },
  { "FALSE", IDL_TOKEN_FALSE, false }
};

int
idl_iskeyword(
  idl_pstate_t *pstate,
  const char *str,
  int nc)
{
  int(*cmp)(const char *s1, const char *s2, size_t n);
  size_t n;

  assert(str != NULL);

  cmp = (nc ? &idl_strncasecmp : strncmp);
  n = strlen(str);
  for (size_t i = 0; i < sizeof(idl_keywords) / sizeof(idl_keywords[0]); i++) {
    const idl_keyword_t *kw = &idl_keywords[i];
    if (cmp(kw->name, str, n) == 0 && kw->name[n] == '\0') {
      if (kw->extended && !(pstate->config.flags & IDL_FLAG_EXTENDED_DATA_TYPES))
        return 0;
      return kw->token;
    }
  }

  return 0;
}
