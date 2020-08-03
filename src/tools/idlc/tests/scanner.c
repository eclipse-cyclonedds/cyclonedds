/*
 * Copyright(c) 2020 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <inttypes.h>

#include "idl.h"
#include "parser.h"

#include "dds/ddsrt/heap.h"

#include "CUnit/Theory.h"

static void
setup_scanner(idl_processor_t *proc, const char *str)
{
  memset(proc, 0, sizeof(*proc));
  proc->scanner.cursor = str;
  proc->scanner.limit = str + strlen(str);
  proc->scanner.position.line = 1;
  proc->scanner.position.column = 1;
}

static int
compare_position(idl_position_t *a, idl_position_t *b)
{
  if (!a || !b)
    return !a && !b;
  if (a->line != b->line)
    return (int)a->line - (int)b->line;
  if (a->column != b->column)
    return (int)a->column - (int)b->column;
  if (!a->file)
    return 0;
  if (!b->file)
    return 1;
  return strcmp(a->file, b->file);
}

static void
assert_location(idl_location_t *a, idl_location_t *b)
{
  fprintf(stderr, "first.line: %"PRIu32", %"PRIu32"\n", a->first.line, b->first.line);
  fprintf(stderr, "first.column: %"PRIu32", %"PRIu32"\n", a->first.column, b->first.column);
  CU_ASSERT_EQUAL(compare_position(&a->first, &b->first), 0);
  fprintf(stderr, "last.line: %"PRIu32", %"PRIu32"\n", a->last.line, b->last.line);
  fprintf(stderr, "last.column: %"PRIu32", %"PRIu32"\n", a->last.column, b->last.column);
  CU_ASSERT_EQUAL(compare_position(&a->last, &b->last), 0);
}

static void
assert_token(idl_token_t *tok, idl_token_t *xtok)
{
  fprintf(stderr, "code: %"PRId32", %"PRId32"\n", tok->code, xtok->code);
  CU_ASSERT_EQUAL(tok->code, xtok->code);
  assert_location(&tok->location, &xtok->location);
  if (tok->code != xtok->code)
    return;
  switch (xtok->code) {
    case IDL_TOKEN_IDENTIFIER:
    case IDL_TOKEN_STRING_LITERAL:
    case IDL_TOKEN_PP_NUMBER:
      if (!xtok->value.str)
        break;
      CU_ASSERT_PTR_NOT_NULL(tok->value.str);
      if (tok->value.str) {
        fprintf(stderr, "string: '%s', '%s'\n", tok->value.str, xtok->value.str);
      } else {
        fprintf(stderr, "string: null, '%s'\n", xtok->value.str);
        break;
      }
      CU_ASSERT_STRING_EQUAL(tok->value.str, xtok->value.str);
      break;
    default:
      break;
  }
}

static void
test_scanner(idl_processor_t *proc, idl_token_t *tokvec)
{
  int32_t code;
  idl_token_t tok;

  for (int i = 0; tokvec[i].code >= 0; i++) {
    code = idl_scan(proc, &tok);
    assert_token(&tok, &tokvec[i]);
    switch (code) {
      case IDL_TOKEN_IDENTIFIER:
      case IDL_TOKEN_PP_NUMBER:
      case IDL_TOKEN_CHAR_LITERAL:
      case IDL_TOKEN_STRING_LITERAL:
        ddsrt_free(tok.value.str);
        break;
      default:
        break;
    }
    if (tokvec[i].code == 0)
      break;
  }
}

static void
test(const char *str, idl_token_t *tokvec)
{
  idl_processor_t proc;
  setup_scanner(&proc, str);
  test_scanner(&proc, tokvec);
}

#define C(c, fl, fc, ll, lc) \
  { c, { c }, { { NULL, fl, fc }, { NULL, ll, lc } } }
#define T(c, fl, fc, ll, lc) \
  { c, { c }, { { NULL, fl, fc }, { NULL, ll, lc } } }
#define T_STR(c, s, fl, fc, ll, lc) \
  { c, { .str = s }, { { NULL, fl, fc }, { NULL, ll, lc } } }

#define T0(fl, fc) T('\0', fl, fc, fl, fc)
#define TLC(fl, fc, ll, lc) T(IDL_TOKEN_LINE_COMMENT, fl, fc, ll, lc)
#define TC(fl, fc, ll, lc) T(IDL_TOKEN_COMMENT, fl, fc, ll, lc)
#define TCL(fl, fc, ll, lc) T(IDL_TOKEN_CHAR_LITERAL, fl, fc, ll, lc)
#define TSL(fl, fc, ll, lc) \
  T_STR(IDL_TOKEN_STRING_LITERAL, NULL, fl, fc, ll, lc)
#define TSL_STR(str, fl, fc, ll, lc) \
  T_STR(IDL_TOKEN_STRING_LITERAL, str, fl, fc ll, lc)
#define TI(fl, fc, ll, lc) \
  T_STR(IDL_TOKEN_IDENTIFIER, NULL, fl, fc, ll, lc)
#define TI_STR(str, fl, fc, ll, lc) \
  T_STR(IDL_TOKEN_IDENTIFIER, str, fl, fc, ll, lc)
#define TN_STR(str, fl, fc, ll, lc) \
  T_STR(IDL_TOKEN_PP_NUMBER, str, fl, fc, ll, lc)

#define TS(fl, fc, ll, lc) T(IDL_TOKEN_SCOPE, fl, fc, ll, lc)
#define TS_L(fl, fc, ll, lc) T(IDL_TOKEN_SCOPE_L, fl, fc, ll, lc)
#define TS_R(fl, fc, ll, lc) T(IDL_TOKEN_SCOPE_R, fl, fc, ll, lc)
#define TS_LR(fl, fc, ll, lc) T(IDL_TOKEN_SCOPE_LR, fl, fc, ll, lc)

#define TOKVEC(...) (idl_token_t[]){ __VA_ARGS__}

/* blank */
CU_Test(idlc_scanner, blank)
{ test("", TOKVEC( T0(1,1) )); }

CU_Test(idlc_scanner, blank_cseq)
{ test("\\\n", TOKVEC( T0(2,1) )); }

CU_Test(idlc_scanner, blank_wsp_cseq_wsp)
{ test("  \\\n  ", TOKVEC( T0(2,3) )); }

CU_Test(idlc_scanner, blank_2x_cseq)
{ test("\\\n\\\n", TOKVEC( T0(3,1) )); }

/* line comment */
CU_Test(idlc_scanner, line_comment)
{ test("//", TOKVEC( TLC(1,1,1,3), T0(1,3) )); }

CU_Test(idlc_scanner, line_comment_wrp_cseq)
{ test("/\\\n/\\\n",  TOKVEC( TLC(1,1,3,1), T0(3,1) )); }

CU_Test(idlc_scanner, line_comment_wrp_cseq_ident)
{ test("//\\\nfoo\\\nbar\nbaz", TOKVEC( TLC(1,1,3,4), C('\n',3,4,4,1), TI(4,1,4,4), T0(4,4) )); }

/* comment */
CU_Test(idlc_scanner, comment)
{ test("/**/", TOKVEC( TC(1,1,1,5), T0(1,5) )); }

CU_Test(idlc_scanner, comment_x)
{ test("/*/*/", TOKVEC( TC(1,1,1,6), T0(1,6) )); }

CU_Test(idlc_scanner, comment_wrp_cseq)
{ test("/\\\n*\\\n*\\\n/", TOKVEC( TC(1,1,4,2), T0(4,2) )); }

CU_Test(idlc_scanner, comment_wrp_cseq_ident)
{ test("/\\\n*foo\\\n/bar\\\n*\\\n/baz", TOKVEC( TC(1,1,5,2), TI(5,2,5,5), T0(5,5) )); }

/* char literal */
CU_Test(idlc_scanner, char_literal)
{ test("\'foo\'", TOKVEC( TCL(1,1,1,6), T0(1,6) )); }

CU_Test(idlc_scanner, char_literal_wrp_cseq)
{ test("\'foo\\\n\'\\\n", TOKVEC( TCL(1,1,2,2), T0(3,1) )); }

/* string literal */
CU_Test(idlc_scanner, string_literal)
{ test("\"foo bar baz\"", TOKVEC( TSL(1,1,1,14), T0(1,14) )); }

CU_Test(idlc_scanner, string_literal_wrp_seq)
{ test("\"foo bar baz\\\n\"\\\n", TOKVEC( TSL(1,1,2,2), T0(3,1) )); }

/* identifier */
CU_Test(idlc_scanner, ident)
{ test("a", TOKVEC( TI(1,1,1,2), T0(1,2) )); }

CU_Test(idlc_scanner, ident_tr_cseq)
{ test("a\\\r\n", TOKVEC( TI(1,1,1,2), T0(2,1) )); }

CU_Test(idlc_scanner, ident_wrp_cseq)
{ test("\\\na\\\r\nb\\\n", TOKVEC( TI(2,1,3,2), T0(4,1) )); }

/* scope */
CU_Test(idlc_scanner, scope)
{ test("::", TOKVEC( TS(1,1,1,3), T0(1,3) )); }

CU_Test(idlc_scanner, scope_wrp_cseq)
{ test("\\\n:\\\r\n:\\\n", TOKVEC( TS(2,1,3,2), T0(4,1) )); }

/* scoped name (IDL_TOKEN_SCOPE_L) */
CU_Test(idlc_scanner, scope_l)
{ test("foo::", TOKVEC( TI(1,1,1,4), TS_L(1,4,1,6), T0(1,6) )); }

CU_Test(idlc_scanner, scope_l_wrp_cseq)
{ test("foo\\\n::", TOKVEC( TI(1,1,1,4), TS_L(2,1,2,3), T0(2,3) )); }

CU_Test(idlc_scanner, scope_non_l)
{ test("foo ::", TOKVEC( TI(1,1,1,4), TS(1,5,1,7), T0(1,7) )); }

/* scoped name (IDL_TOKEN_SCOPE_R) */
CU_Test(idlc_scanner, scope_r)
{ test("::foo", TOKVEC( TS_R(1,1,1,3), TI(1,3,1,6), T0(1,6) )); }

CU_Test(idlc_scanner, scope_r_wrp_cseq)
{ test("::\\\r\nfoo", TOKVEC( TS_R(1,1,1,3), TI(2,1,2,4), T0(2,4) )); }

CU_Test(idlc_scanner, scope_non_r)
{ test(":: foo", TOKVEC( TS(1,1,1,3), TI(1,4,1,7), T0(1,7) )); }

/* scoped name (IDL_TOKEN_SCOPE_LR) */
CU_Test(idlc_scanner, scope_lr)
{ test("foo::bar",
    TOKVEC( TI(1,1,1,4), TS_LR(1,4,1,6), TI(1,6,1,9), T0(1,9) )); }

CU_Test(idlc_scanner, scope_lr_wrp_cseq)
{ test("foo\\\n::\\\r\nbar",
    TOKVEC( TI(1,1,1,4), TS_LR(2,1,2,3), TI(3,1,3,4), T0(3,4) )); }

CU_Test(idlc_scanner, scope_non_lr)
{ test("foo :: bar",
    TOKVEC( TI(1,1,1,4), TS(1,5,1,7), TI(1,8,1,11), T0(1,11) )); }

CU_Test(idlc_scanner, scope_non_lr_l)
{ test("foo:: bar",
    TOKVEC( TI(1,1,1,4), TS_L(1,4,1,6), TI(1,7,1,10), T0(1,10) )); }

CU_Test(idlc_scanner, scope_non_lr_r)
{ test("foo ::bar",
    TOKVEC( TI(1,1,1,4), TS_R(1,5,1,7), TI(1,7,1,10), T0(1,10) )); }

// extended char literal tests
//  x. escape sequences
//  x. unterminated char literal
//  x. see formal-18-01-05.pdf section 7.2.6.2

// extended string literal tests
//  x. escape sequences
//  x. unterminated string literal

CU_Test(idlc_scanner, hash_line)
{ test("#line\\\n 1",
    TOKVEC( T('#',1,1,1,2), TI_STR("line",1,2,1,6), TN_STR("1",2,2,2,3), T0(2,3) )); }

CU_Test(idlc_scanner, hash_pragma)
{ test("#pragma keylist foo bar baz\n",
    TOKVEC( T('#',1,1,1,2),
            TI_STR("pragma",1,2,1,8),
            TI_STR("keylist",1,9,1,16),
            TI_STR("foo",1,17,1,20),
            TI_STR("bar",1,21,1,24),
            TI_STR("baz",1,25,1,28),
            T('\n',1,28,2,1),
            T0(2,1) ));
}
