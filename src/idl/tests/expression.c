// Copyright(c) 2020 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <inttypes.h>

#include "idl/processor.h"
#include "idl/retcode.h"
#include "idl/tree.h"
#include "expression.h"
#include "tree.h"

#include "CUnit/Test.h"

#define SYMBOL { \
    .location = { { NULL, NULL, 0, 0 }, { NULL, NULL, 0, 0 } } \
  }

#define NODE(type) { \
    .symbol = SYMBOL, \
    .mask = (IDL_LITERAL|type), \
    .destructor = 0, \
    .iterate = 0, \
    .describe = 0, \
    .references = 0, \
    .annotations = NULL, \
    .declaration = NULL, \
    .parent = NULL, \
    .previous = NULL, \
    .next = NULL \
  }

#define LITERAL(type, assignment) \
  (idl_literal_t){ .node = NODE(type), .value = { assignment } }

#define CHAR(value) LITERAL(IDL_CHAR, .chr = (value))
#define BOOL(value) LITERAL(IDL_BOOL, .bln = (value))
#define OCTET(value) LITERAL(IDL_OCTET, .uint8 = (value))
#define SHORT(value) LITERAL(IDL_SHORT, .int16 = (value))
#define USHORT(value) LITERAL(IDL_USHORT, .uint16 = (value))
#define LONG(value) LITERAL(IDL_LONG, .int32 = (value))
#define ULONG(value) LITERAL(IDL_ULONG, .uint32 = (value))
#define LLONG(value) LITERAL(IDL_LLONG, .int64 = (value))
#define ULLONG(value) LITERAL(IDL_ULLONG, .uint64 = (value))
#define INT8(value) LITERAL(IDL_INT8, .int8 = (value))
#define UINT8(value) LITERAL(IDL_UINT8, .uint8 = (value))
#define INT16(value) LITERAL(IDL_INT16, .int16 = (value))
#define UINT16(value) LITERAL(IDL_UINT16, .uint16 = (value))
#define INT32(value) LITERAL(IDL_INT32, .int32 = (value))
#define UINT32(value) LITERAL(IDL_UINT32, .uint32 = (value))
#define INT64(value) LITERAL(IDL_INT64, .int64 = (value))
#define UINT64(value) LITERAL(IDL_UINT64, .uint64 = (value))

static void
test_expr(
  const char *str,
  const idl_retcode_t ret,
  const idl_literal_t *exp)
{
  idl_retcode_t r;
  idl_pstate_t *pstate = NULL;
  idl_const_t *c;
  idl_literal_t *cv;

  r = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(r, IDL_RETCODE_OK);
  r = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(r, ret);
  if (r != IDL_RETCODE_OK) {
    idl_delete_pstate(pstate);
    return;
  }
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);
  c = (void *)pstate->root;
  do {
    if (idl_is_const(c) && strcmp(idl_identifier(c), "x") == 0)
      break;
    c = idl_next(c);
  } while (c);
  CU_ASSERT_FATAL(idl_is_const(c));
  assert(c);
  cv = c->const_expr;
  CU_ASSERT_FATAL(idl_is_literal(cv));
  CU_ASSERT(idl_compare(cv, exp) == IDL_EQUAL);
  idl_delete_pstate(pstate);
}

#define ok                 (IDL_RETCODE_OK)
#define semantic_error     (IDL_RETCODE_SEMANTIC_ERROR)
#define illegal_expression (IDL_RETCODE_ILLEGAL_EXPRESSION)
#define out_of_range       (IDL_RETCODE_OUT_OF_RANGE)

struct expr {
  const char *str;
  const idl_retcode_t ret;
  const idl_literal_t *val;
};

#define x "const char x"
static struct expr chr_exprs[] = {
  {  x " = 'c';",    ok,                  &CHAR('c')  },
  {  x " = '\\n';",  ok,                  &CHAR('\n') },
  {  x " = \"c\";",  illegal_expression,   NULL       }
};
#undef x

CU_Test(idl_expression, character)
{
  const size_t n = (sizeof(chr_exprs)/sizeof(chr_exprs[0]));
  for (size_t i=0; i < n; i++)
    test_expr(chr_exprs[i].str, chr_exprs[i].ret, chr_exprs[i].val);
}

#define x "const octet x"
#define y "const octet y"
static struct expr oct_exprs[] = {
  {  x " = +1;",                   ok,            &OCTET(1)    },
  {  x " = -1;",                   out_of_range,   NULL        },
  {  x " = -1 - -1;",              ok,            &OCTET(0)    },
  {  x " = -1 * -1;",              ok,            &OCTET(1)    },
  {  x " = (65535 * 0) / 1;",      ok,            &OCTET(0)    },
  {  x " = (65535 * 1) / 65535;",  ok,            &OCTET(1)    },
  {  y " = 1; " x " = y;",         ok,            &OCTET(1)    },
  {  x " = 1 - 2;",                out_of_range,   NULL        },
  {  x " = 255;",                  ok,            &OCTET(255)  },
  {  x " = 256;",                  out_of_range,   NULL        },
  {  x " = 256 - 1;",              ok,            &OCTET(255)  },
  {  x " = 1 << 1;",               ok,            &OCTET(2)    },
  {  x " = 1 >> 1;",               ok,            &OCTET(0)    }
};
#undef x
#undef y

CU_Test(idl_expression, octet)
{
  const size_t n = (sizeof(oct_exprs)/sizeof(oct_exprs[0]));
  for (size_t i=0; i < n; i++)
    test_expr(oct_exprs[i].str, oct_exprs[i].ret, oct_exprs[i].val);
}
