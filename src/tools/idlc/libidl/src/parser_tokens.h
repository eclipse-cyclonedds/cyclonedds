// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef PARSER_TOKENS_H
#define PARSER_TOKENS_H

#include "idl/processor.h"

#ifndef IDL_TOKEN_KIND_H
#define IDL_TOKEN_KIND_H
enum idl_token_kind {
  IDL_TOKEN_EOF = 0,
  IDL_TOKEN_ERROR = 256,
  IDL_TOKEN_UNDEFINED = 257,
  IDL_TOKEN_LINE_COMMENT = 258,
  IDL_TOKEN_COMMENT = 259,
  IDL_TOKEN_PP_NUMBER = 260,
  IDL_TOKEN_IDENTIFIER = 261,
  IDL_TOKEN_CHAR_LITERAL = 262,
  IDL_TOKEN_STRING_LITERAL = 263,
  IDL_TOKEN_INTEGER_LITERAL = 264,
  IDL_TOKEN_FLOATING_PT_LITERAL = 265,
  IDL_TOKEN_ANNOTATION_SYMBOL = 266,
  IDL_TOKEN_ANNOTATION = 267,
  IDL_TOKEN_SCOPE = 268,
  IDL_TOKEN_SCOPE_NO_SPACE = 269,
  IDL_TOKEN_MODULE = 270,
  IDL_TOKEN_CONST = 271,
  IDL_TOKEN_NATIVE = 272,
  IDL_TOKEN_STRUCT = 273,
  IDL_TOKEN_TYPEDEF = 274,
  IDL_TOKEN_UNION = 275,
  IDL_TOKEN_SWITCH = 276,
  IDL_TOKEN_CASE = 277,
  IDL_TOKEN_DEFAULT = 278,
  IDL_TOKEN_ENUM = 279,
  IDL_TOKEN_UNSIGNED = 280,
  IDL_TOKEN_FIXED = 281,
  IDL_TOKEN_SEQUENCE = 282,
  IDL_TOKEN_STRING = 283,
  IDL_TOKEN_WSTRING = 284,
  IDL_TOKEN_FLOAT = 285,
  IDL_TOKEN_DOUBLE = 286,
  IDL_TOKEN_SHORT = 287,
  IDL_TOKEN_LONG = 288,
  IDL_TOKEN_CHAR = 289,
  IDL_TOKEN_WCHAR = 290,
  IDL_TOKEN_BOOLEAN = 291,
  IDL_TOKEN_OCTET = 292,
  IDL_TOKEN_ANY = 293,
  IDL_TOKEN_MAP = 294,
  IDL_TOKEN_BITSET = 295,
  IDL_TOKEN_BITFIELD = 296,
  IDL_TOKEN_BITMASK = 297,
  IDL_TOKEN_INT8 = 298,
  IDL_TOKEN_INT16 = 299,
  IDL_TOKEN_INT32 = 300,
  IDL_TOKEN_INT64 = 301,
  IDL_TOKEN_UINT8 = 302,
  IDL_TOKEN_UINT16 = 303,
  IDL_TOKEN_UINT32 = 304,
  IDL_TOKEN_UINT64 = 305,
  IDL_TOKEN_TRUE = 306,
  IDL_TOKEN_FALSE = 307,
  IDL_TOKEN_LSHIFT = 308,
  IDL_TOKEN_RSHIFT = 309
};
typedef enum idl_token_kind idl_token_kind_t;
#endif

int idl_iskeyword(idl_pstate_t *pstate, const char *str, int nc);

#endif /* PARSER_TOKENS_H */
