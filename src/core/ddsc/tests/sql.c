// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Theory.h"
#include "CUnit/Test.h"

#include <assert.h>
#include <time.h>

#include "dds/dds.h"
#include "dds/ddsrt/hopscotch.h"
#include "dds__sql_expr.h"

static bool test_get_token(const unsigned char *s, const size_t sz, int exp_tokens[], size_t exp_sz)
{
  (void) sz;
  const unsigned char *cur = s;
  bool passed = true;
  for (size_t i = 0; *cur != '\0' && passed; i++) {
    int token = 0;
    int tok_sz = dds_sql_get_token(cur, &token);
    if (!(passed = !(tok_sz < 1)))
    {
      fprintf (stderr, "[FAILED]:%s\n", s);
      for (int j = 0; j < (cur - s) + 9; j++)
        fprintf (stderr, " ");
      fprintf (stderr, "^--- error here (failed to get token)\n");
    } else if (!(passed = !(i >= exp_sz))) {
      fprintf (stderr, "[FAILED]:%s\n", s);
      fprintf (stderr, "         ^--- error here (unexpected number of tokens %lu)\n", exp_sz);
    } else if (!(passed = !(token != exp_tokens[i]))) {
      fprintf (stderr, "[FAILED]:%s\n", s);
      for (int j = 0; j < (cur - s) + 9; j++)
        fprintf (stderr, " ");
      fprintf (stderr, "^--- error here (expected token %d, actual %d)\n", exp_tokens[i], token);
    }
    cur += tok_sz;
  }

  return passed;
}

#define TEST_GET_TOKEN(expr,sz,...) \
  do { \
    char *e = expr; \
    size_t esz = strlen(e); \
    int t[sz] = {__VA_ARGS__}; \
    CU_ASSERT(test_get_token((const unsigned char *)e,esz,t,sz)); \
  } while (0)

CU_Test(ddsc_sql, get_token)
{
#define S DDS_SQL_TK_SPACE

  TEST_GET_TOKEN("",                  1,  -1);
  TEST_GET_TOKEN("-",                 1,  DDS_SQL_TK_MINUS);
  TEST_GET_TOKEN("---",               1,  DDS_SQL_TK_COMMENT);
  TEST_GET_TOKEN("-/--",              3,  DDS_SQL_TK_MINUS,DDS_SQL_TK_SLASH,DDS_SQL_TK_COMMENT);
  TEST_GET_TOKEN("--abcdr",           1,  DDS_SQL_TK_COMMENT);
  TEST_GET_TOKEN("/**/",              1,  DDS_SQL_TK_COMMENT);
  TEST_GET_TOKEN("/*\tabcd*/",        1,  DDS_SQL_TK_COMMENT);
  TEST_GET_TOKEN("/*\n\tabcd*/\t1",   3,  DDS_SQL_TK_COMMENT,S,DDS_SQL_TK_INTEGER);
  TEST_GET_TOKEN("/*",                2,  DDS_SQL_TK_SLASH,DDS_SQL_TK_STAR);

  TEST_GET_TOKEN("%",                 1,  DDS_SQL_TK_REM);

  TEST_GET_TOKEN("?",                 1,  DDS_SQL_TK_ILLEGAL);
  TEST_GET_TOKEN("?0",                1,  DDS_SQL_TK_VARIABLE);
  TEST_GET_TOKEN("?10",               1,  DDS_SQL_TK_VARIABLE);
  TEST_GET_TOKEN("?a",                2,  DDS_SQL_TK_ILLEGAL,DDS_SQL_TK_ID);

  TEST_GET_TOKEN("=",                 1,  DDS_SQL_TK_EQ);
  TEST_GET_TOKEN("==",                1,  DDS_SQL_TK_EQ);
  TEST_GET_TOKEN("===<",              3,  DDS_SQL_TK_EQ,DDS_SQL_TK_EQ,DDS_SQL_TK_LT);
  TEST_GET_TOKEN("===<<",             4,  DDS_SQL_TK_EQ,DDS_SQL_TK_EQ,DDS_SQL_TK_LSHIFT);
  TEST_GET_TOKEN("<>",                1,  DDS_SQL_TK_NE);
  TEST_GET_TOKEN("<=>",               2,  DDS_SQL_TK_LE, DDS_SQL_TK_GT);
  TEST_GET_TOKEN("<=>>",              2,  DDS_SQL_TK_LE,DDS_SQL_TK_RSHIFT);
  TEST_GET_TOKEN("<<=>>",             3,  DDS_SQL_TK_LSHIFT,DDS_SQL_TK_EQ,DDS_SQL_TK_RSHIFT);
  TEST_GET_TOKEN("<=>=>",             4,  DDS_SQL_TK_LE,DDS_SQL_TK_GE,DDS_SQL_TK_GT);

  TEST_GET_TOKEN("!",                 1,  DDS_SQL_TK_ILLEGAL);
  TEST_GET_TOKEN("!=",                1,  DDS_SQL_TK_NE);
  TEST_GET_TOKEN("!=>",               2,  DDS_SQL_TK_NE, DDS_SQL_TK_GT);

  TEST_GET_TOKEN("|||||",             3,  DDS_SQL_TK_CONCAT,DDS_SQL_TK_CONCAT,DDS_SQL_TK_BITOR);

  TEST_GET_TOKEN(",",                 1,  DDS_SQL_TK_COMMA);
  TEST_GET_TOKEN("&&",                2,  DDS_SQL_TK_BITAND,DDS_SQL_TK_BITAND);

  TEST_GET_TOKEN("~",                 1,  DDS_SQL_TK_BITNOT);

  TEST_GET_TOKEN("`abcd`",            1,  DDS_SQL_TK_ID);
  TEST_GET_TOKEN("`abcd",             2,  DDS_SQL_TK_ILLEGAL);
  TEST_GET_TOKEN("\"abcd`",           1,  DDS_SQL_TK_ILLEGAL);
  TEST_GET_TOKEN("`abcd`abcd'",       3,  DDS_SQL_TK_ID,DDS_SQL_TK_ID,DDS_SQL_TK_ILLEGAL);
  TEST_GET_TOKEN("'abcd`abcd'",       1,  DDS_SQL_TK_STRING);
  TEST_GET_TOKEN("'abcd abcd'",       1,  DDS_SQL_TK_STRING);
  TEST_GET_TOKEN("'abcd\"abcd'",      1,  DDS_SQL_TK_STRING);
  TEST_GET_TOKEN("'abcd",             1,  DDS_SQL_TK_ILLEGAL);

  TEST_GET_TOKEN("`a`.b.`c`",         5,  DDS_SQL_TK_ID,DDS_SQL_TK_DOT,DDS_SQL_TK_ID,DDS_SQL_TK_DOT, DDS_SQL_TK_ID);
  TEST_GET_TOKEN("`a.b.c`",           1,  DDS_SQL_TK_ID);

  TEST_GET_TOKEN(".a",                2,  DDS_SQL_TK_DOT,DDS_SQL_TK_ID);
  TEST_GET_TOKEN(".0",                1,  DDS_SQL_TK_FLOAT);
  TEST_GET_TOKEN("1.0",               1,  DDS_SQL_TK_FLOAT);
  TEST_GET_TOKEN("1_000.0_1",         1,  DDS_SQL_TK_QNUMBER);
  TEST_GET_TOKEN(".1.0",              2,  DDS_SQL_TK_FLOAT,DDS_SQL_TK_FLOAT);
  TEST_GET_TOKEN(".0E",               1,  DDS_SQL_TK_ILLEGAL);
  TEST_GET_TOKEN(".0EA",              2,  DDS_SQL_TK_ILLEGAL,DDS_SQL_TK_ID);
  TEST_GET_TOKEN(".1E10",             1,  DDS_SQL_TK_FLOAT);
  TEST_GET_TOKEN(".1E+10",            1,  DDS_SQL_TK_FLOAT);
  TEST_GET_TOKEN(".1E+",              2,  DDS_SQL_TK_ILLEGAL,DDS_SQL_TK_PLUS);
  TEST_GET_TOKEN(".1E+-10",           4,  DDS_SQL_TK_ILLEGAL,DDS_SQL_TK_PLUS,DDS_SQL_TK_MINUS,DDS_SQL_TK_INTEGER);
  TEST_GET_TOKEN(".1E-10",            1,  DDS_SQL_TK_FLOAT);
  TEST_GET_TOKEN("1E-10",             1,  DDS_SQL_TK_FLOAT);
  TEST_GET_TOKEN("1E",                1,  DDS_SQL_TK_ILLEGAL);
  TEST_GET_TOKEN("1Eabcd",            1,  DDS_SQL_TK_ILLEGAL);
  TEST_GET_TOKEN("1.E",               1,  DDS_SQL_TK_ILLEGAL);

  TEST_GET_TOKEN("1_000_000",         1,  DDS_SQL_TK_QNUMBER);
  TEST_GET_TOKEN("_0",                2,  DDS_SQL_TK_ID,DDS_SQL_TK_INTEGER);
  TEST_GET_TOKEN("_0_",               2,  DDS_SQL_TK_ID,DDS_SQL_TK_QNUMBER);
  TEST_GET_TOKEN("1_0",               1,  DDS_SQL_TK_QNUMBER);
  TEST_GET_TOKEN("1__0",              1,  DDS_SQL_TK_QNUMBER);

  TEST_GET_TOKEN("0x01",              1,  DDS_SQL_TK_INTEGER);
  TEST_GET_TOKEN("0x",                1,  DDS_SQL_TK_ILLEGAL);
  TEST_GET_TOKEN("0XA",               1,  DDS_SQL_TK_INTEGER);
  TEST_GET_TOKEN("0xFFFFFF",          1,  DDS_SQL_TK_INTEGER);
  TEST_GET_TOKEN("0xFFF_FFF",         1,  DDS_SQL_TK_QNUMBER);
  TEST_GET_TOKEN("0x_FFF_FFF",        1,  DDS_SQL_TK_ILLEGAL);
  TEST_GET_TOKEN("0xFFF_FFF_FFF",     1,  DDS_SQL_TK_QNUMBER);

#define I DDS_SQL_TK_ILLEGAL
  TEST_GET_TOKEN("0xG 0xH 0xI 0xK "
                 "0xL 0xM 0xN 0xO "
                 "0xP 0xQ 0xR 0xS "
                 "0xT 0xV 0xX 0xY "
                 "0xZ",              33,  I,S,I,S,I,S,I,S,I,S,I,S,
                                          I,S,I,S,I,S,I,S,I,S,I,S,
                                          I,S,I,S,I,S,I,S,I);
#undef I

  TEST_GET_TOKEN("OR",                1,  DDS_SQL_TK_OR);
  TEST_GET_TOKEN("NOT",               1,  DDS_SQL_TK_NOT);
  TEST_GET_TOKEN("AND",               1,  DDS_SQL_TK_AND);
  TEST_GET_TOKEN("LIKE",              1,  DDS_SQL_TK_LIKE_KW);
  TEST_GET_TOKEN("ESCAPE",            1,  DDS_SQL_TK_ESCAPE);
  TEST_GET_TOKEN("COLLATE",           1,  DDS_SQL_TK_COLLATE);
  TEST_GET_TOKEN("BETWEEN",           1,  DDS_SQL_TK_BETWEEN);
  TEST_GET_TOKEN("GLOB",              1,  DDS_SQL_TK_ID);

  TEST_GET_TOKEN("x\'414243\'",       1,  DDS_SQL_TK_BLOB);
  TEST_GET_TOKEN("X'414243'",         1,  DDS_SQL_TK_BLOB);
  TEST_GET_TOKEN("X'41424'",          1,  DDS_SQL_TK_ILLEGAL);
  TEST_GET_TOKEN("X'414243",          1,  DDS_SQL_TK_ILLEGAL);
  TEST_GET_TOKEN("x",                 1,  DDS_SQL_TK_ID);
  TEST_GET_TOKEN("x''",               1,  DDS_SQL_TK_BLOB);

  TEST_GET_TOKEN("1 AND 0",           5,  DDS_SQL_TK_INTEGER,S,DDS_SQL_TK_AND,S,DDS_SQL_TK_INTEGER);
  TEST_GET_TOKEN("ORAND",             1,  DDS_SQL_TK_ID);
  TEST_GET_TOKEN("1.1E-10AND0",       2,  DDS_SQL_TK_ILLEGAL,DDS_SQL_TK_INTEGER);
  TEST_GET_TOKEN("1.1E-10 AND0.0",    4,  DDS_SQL_TK_FLOAT,S,DDS_SQL_TK_AND,DDS_SQL_TK_FLOAT);
  TEST_GET_TOKEN("ORANDLIKE",         1,  DDS_SQL_TK_ID);

  TEST_GET_TOKEN("()",                2,  DDS_SQL_TK_LP,DDS_SQL_TK_RP);
  /* FIXME: avoid usage of '$'|'@'|':' (even computation of length of those
   * tokens is missing). */
  TEST_GET_TOKEN("?12345",            1,  DDS_SQL_TK_VARIABLE);
  TEST_GET_TOKEN("?1abcd",            2,  DDS_SQL_TK_VARIABLE,DDS_SQL_TK_ID);
  TEST_GET_TOKEN("?abcd",             2,  DDS_SQL_TK_ILLEGAL,DDS_SQL_TK_ID);

#undef S
}

#define TEST_GET_NUMERIC(ex,tki,tko,sz,fld,val) \
  do { \
    bool passed = true; \
    struct dds_sql_token exp; \
    const char *e = ex; \
    int t = tki; \
    (void) dds_sql_get_numeric(&(exp.n),(const unsigned char **)&e, &t, sz); \
    if ((passed = (exp.fld == val) && (t == tko))) { \
      ; \
    } else { \
      if (t == DDS_SQL_TK_INTEGER) \
        fprintf (stderr, "[FAILED]:%-20s -> expected: %li actual: %li\n", e, (int64_t)val, exp.n.i); \
      else if (t == DDS_SQL_TK_FLOAT) \
        fprintf (stderr, "[FAILED]:%-20s -> expected: %f actual: %f\n", e, (double)val, exp.n.r); \
    } \
    CU_ASSERT (passed); \
  } while (0)

CU_Test (ddsc_sql, get_numeric)
{
#define Q DDS_SQL_TK_QNUMBER
#define I DDS_SQL_TK_INTEGER
#define F DDS_SQL_TK_FLOAT

  TEST_GET_NUMERIC("1______0____00_00__0",  Q, I,   20,   n.i,  1000000);
  TEST_GET_NUMERIC("1",                     I, I,   1,    n.i,  1);
  TEST_GET_NUMERIC("0x12345",               I, I,   7,    n.i,  74565);
  TEST_GET_NUMERIC("0x1_2_3_4_5",           Q, I,   11,   n.i,  74565);

  TEST_GET_NUMERIC(".0",                    F, F,   2,    n.r,  0.0);
  TEST_GET_NUMERIC("1.0abcd",               F, F,   3,    n.r,  1.0);
  TEST_GET_NUMERIC(".001E+2",               F, F,   7,    n.r,  0.1);
  TEST_GET_NUMERIC(".000_1E+3",             Q, F,   9,    n.r,  0.1);
  TEST_GET_NUMERIC("1.000000e+01",          F, F,   12,   n.r,  10.0);

#undef F
#undef I
#undef Q

}

#define TEST_PRINT_BLOB(b,sz) \
  do { \
    fprintf (stderr, "{"); \
    for (size_t i = 0; i < sz; i++) { \
      fprintf (stderr, "%.2X", ((unsigned char *)b)[i]); \
      if (i+1 < sz) \
        fprintf (stderr, ", "); \
    } \
    fprintf (stderr, "}"); \
    size_t rest = 20u - ((sz*2)+((sz-1)*2)) - 2; \
    for (size_t k = 0; k < rest && rest > 0; k++) \
      fprintf (stderr, " "); \
  } while (0)

#define TEST_GET_STRING(ex,tki,tko,sz,esz,val) \
  do { \
    bool passed = true; \
    struct dds_sql_token exp; \
    const char *e = ex; \
    int t = tki; \
    int size = dds_sql_get_string((void **)&exp.s,(const unsigned char **)&e, &t, sz); \
    assert (size != 0); \
    char *test = (char *)val; \
    if (t == DDS_SQL_TK_STRING || t == DDS_SQL_TK_ID) { \
      if ((passed = (!memcmp(test, exp.s, (unsigned int)size) && (t == tko) && (size == esz)))) { \
        ; \
      } else { \
        fprintf (stderr, "[FAILED]:%-20s -> expected: %s (%ld) actual: %s (%ld)\n", e, (char *)test, (int64_t)esz, (char *)exp.s, (int64_t)size); \
      } \
    } else if (t == DDS_SQL_TK_BLOB) { \
      if ((passed = (!memcmp(test, exp.s, (unsigned int)size) && (t == tko) && (size == esz)))) { \
        ; \
      } else { \
        fprintf (stderr, "[FAILED]:%-20s -> expected: ", e); \
        TEST_PRINT_BLOB(test,esz); \
        fprintf (stderr, "actual: "); \
        TEST_PRINT_BLOB(exp.s,(unsigned int) size); \
      } \
    } \
    free (exp.s); \
    CU_ASSERT(passed); \
  } while(0)

#define TEST_GET_BLOB(ex,tki,tko,sz,esz,...) \
  do { \
    unsigned char u[] = {__VA_ARGS__}; \
    TEST_GET_STRING(ex,tki,tko,sz,esz,u); \
  } while (0)

CU_Test(ddsc_sql, get_string)
{
#define S DDS_SQL_TK_STRING
#define B DDS_SQL_TK_BLOB
#define I DDS_SQL_TK_ID

  TEST_GET_STRING("\'abcde\'",              S, S, 7, 6, "abcde");
  TEST_GET_BLOB  ("X'414243'",              B, B, 9, 3, 'A','B','C');
  TEST_GET_STRING("`abcde`",                I, I, 7, 5, "abcde");
  TEST_GET_STRING("\"abcde",                I, I, 7, 5, "abcde");
  TEST_GET_STRING("\"abcde\"",              I, I, 7, 5, "abcde");
  TEST_GET_STRING("abcde",                  I, I, 5, 5, "abcde");

#undef I
#undef B
#undef S

}

#define TEST_APPLY_AFFINITY_NUM(ex,af,val,field,fldsz) \
  do { \
    const int affinity = af; \
    struct dds_sql_token exp; \
    char *e = ex; \
    const unsigned char *cursor = (const unsigned char *)e; \
    int token = 0; \
    int token_sz = dds_sql_get_token(cursor, &token); \
    CU_ASSERT (token > 0); \
    exp.tok = token; \
    int res = 0; \
    if (token == DDS_SQL_TK_INTEGER || token == DDS_SQL_TK_FLOAT) \
    { \
      res = dds_sql_get_numeric(&(exp.n),&cursor,&token,token_sz); \
      CU_ASSERT (res == 0); \
      exp.aff = (token == DDS_SQL_TK_INTEGER)? DDS_SQL_AFFINITY_INTEGER: DDS_SQL_AFFINITY_REAL; \
    } else if (token == DDS_SQL_TK_STRING || token == DDS_SQL_TK_BLOB) { \
      res = dds_sql_get_string((void **)&(exp.s),&cursor,&token,token_sz); \
      CU_ASSERT (res != 0); \
      exp.n.i = res; \
      exp.aff = (token == DDS_SQL_TK_BLOB)? DDS_SQL_AFFINITY_BLOB: DDS_SQL_AFFINITY_TEXT; \
    } else { \
      CU_ASSERT(false); \
      break; \
    } \
    res = dds_sql_apply_affinity(&exp, affinity); \
    if (exp.aff == DDS_SQL_AFFINITY_REAL && affinity == DDS_SQL_AFFINITY_INTEGER) { \
      CU_ASSERT (res < 0); /* (real -> integer) well known forbidden case. */ \
      break; \
    } else if (res != affinity && affinity != DDS_SQL_AFFINITY_NUMERIC) { \
      fprintf (stderr, "[FAILED]:%-20s expected affinity: %c actual %c\n", e, affinity, res); \
      CU_ASSERT (false); \
      break; \
    } \
    if (exp.field != val) { \
      if (affinity == DDS_SQL_AFFINITY_INTEGER) \
        fprintf (stderr, "[FAILED]:%-20s -> expected: %li actual: %li\n", e, (int64_t)val, exp.n.i); \
      else if (affinity == DDS_SQL_AFFINITY_REAL) \
        fprintf (stderr, "[FAILED]:%-20s -> expected: %f actual: %f\n", e, (double)val, exp.n.r); \
      CU_ASSERT (false); \
      break; \
    } \
  } while (0)



#define TEST_APPLY_AFFINITY_STR(ex,af,val,field,fldsz) \
  do { \
    const int affinity = af; \
    struct dds_sql_token exp; \
    char *e = ex; \
    const unsigned char *cursor = (const unsigned char *)e; \
    int token = 0; \
    int token_sz = dds_sql_get_token(cursor, &token); \
    CU_ASSERT (token > 0); \
    exp.tok = token; \
    int res = 0; \
    if (token == DDS_SQL_TK_INTEGER || token == DDS_SQL_TK_FLOAT) \
    { \
      res = dds_sql_get_numeric(&(exp.n),&cursor,&token,token_sz); \
      CU_ASSERT (res == 0); \
      exp.aff = (token == DDS_SQL_TK_INTEGER)? DDS_SQL_AFFINITY_INTEGER: DDS_SQL_AFFINITY_REAL; \
    } else if (token == DDS_SQL_TK_STRING || token == DDS_SQL_TK_BLOB) { \
      res = dds_sql_get_string((void **)&(exp.s),&cursor,&token,token_sz); \
      CU_ASSERT (res != 0); \
      exp.n.i = res; \
      exp.aff = (token == DDS_SQL_TK_BLOB)? DDS_SQL_AFFINITY_BLOB: DDS_SQL_AFFINITY_TEXT; \
    } else { \
      CU_ASSERT(false); \
      break; \
    } \
    res = dds_sql_apply_affinity(&exp, affinity); \
    if (res != affinity) { \
      fprintf (stderr, "[FAILED]:%-20s expected affinity: %c actual %c\n", e, affinity, res); \
      CU_ASSERT (false); \
      break; \
    } \
    if (memcmp(exp.s, (char *)val, (unsigned int)exp.n.i)) { \
      if (affinity == DDS_SQL_AFFINITY_TEXT) { \
        fprintf (stderr, "[FAILED]:%-20s -> expected: %s (%ld) actual: %s (%ld)\n", e, (char *)val, (int64_t)fldsz, (char *)exp.s, (int64_t)exp.n.i); \
      } else if (affinity == DDS_SQL_AFFINITY_BLOB) { \
        fprintf (stderr, "[FAILED]:%-20s -> expected: ", e); \
        TEST_PRINT_BLOB(val,fldsz); \
        fprintf (stderr, "actual: "); \
        TEST_PRINT_BLOB(exp.s,(unsigned int)exp.n.i); \
      } \
      CU_ASSERT (false); \
      break; \
    } \
    free (exp.s); \
  } while (0)

CU_Test(ddsc_sql, apply_affinity)
{
#define T DDS_SQL_AFFINITY_TEXT
#define B DDS_SQL_AFFINITY_BLOB
#define I DDS_SQL_AFFINITY_INTEGER
#define R DDS_SQL_AFFINITY_REAL
#define N DDS_SQL_AFFINITY_NUMERIC

  TEST_APPLY_AFFINITY_STR("1.1",                T,  "1.100000e+00",    s,    13);
  TEST_APPLY_AFFINITY_STR("1",                  T,             "1",    s,    2);
  TEST_APPLY_AFFINITY_STR("x'414243'",          T,           "ABC",    s,    4);
  TEST_APPLY_AFFINITY_STR("'ABCD'",             T,          "ABCD",    s,    5);
{ unsigned char u[] = {0x41,0x42,0x43};
  TEST_APPLY_AFFINITY_STR("'ABC'",              B,       (char *)u,    s,    3); }
{ unsigned char u[] = {0x31};
  TEST_APPLY_AFFINITY_STR("1",                  B,       (char *)u,    s,    1); }
{ unsigned char u[] = {0x31,0x32,0x33};
  TEST_APPLY_AFFINITY_STR("123",                B,       (char *)u,    s,    3); }
{ unsigned char u[] = {0x31,0x2E,0x30,0x30,0x30,0x30,0x30,0x30,0x65,0x2B,0x30,0x30};
  TEST_APPLY_AFFINITY_STR("1.0",                B,       (char *)u,    s,    12);}
  TEST_APPLY_AFFINITY_NUM("1.0",                I,               0,    n.i,  0);
  /*                           (no way, `real` to `integer`!) ---^            */
  TEST_APPLY_AFFINITY_NUM("1",                  I,               1,    n.i,  0);
  TEST_APPLY_AFFINITY_NUM("1",                  R,               1,    n.r,  0);
  TEST_APPLY_AFFINITY_NUM("x'312E30652B3030'",  R,               1,    n.r,  0);
  TEST_APPLY_AFFINITY_NUM("5",                  N,               5,    n.i,  0);
  TEST_APPLY_AFFINITY_NUM("5.1",                N,               5.1,  n.r,  0);
  TEST_APPLY_AFFINITY_NUM("'5.1'",              N,               5.1,  n.r,  0);
  TEST_APPLY_AFFINITY_NUM("'5'",                N,               5,    n.i,  0);
  TEST_APPLY_AFFINITY_NUM("'abcde'",            N,               0,    n.r,  0);
  TEST_APPLY_AFFINITY_NUM("x'414243'",          N,               0,    n.r,  0);
  TEST_APPLY_AFFINITY_NUM("''",                 N,               0,    n.r,  0);

#undef N
#undef R
#undef I
#undef B
#undef T
}

#define EXPBUF_MAX_LEN 300U
static void expr_node_explore(const struct dds_sql_expr_node *node, int depth, char *out, size_t *pos)
{
  assert (out != NULL && pos != NULL && *pos < EXPBUF_MAX_LEN);
  if (node == NULL)
    return;
  assert (node->token != NULL);
  int token = node->token->tok;
  if (token == DDS_SQL_TK_VARIABLE) {
    *pos += (size_t) sprintf (out + *pos, "%c", '?');
    token = DDS_SQL_TK_INTEGER;
  } else if (token == DDS_SQL_TK_ID) {
    *pos += (size_t)sprintf (out + *pos, "%c", '?');
  }
  if (token == DDS_SQL_TK_INTEGER || token == DDS_SQL_TK_VARIABLE) {
    *pos += (size_t)sprintf (out + *pos, "%li", node->token->n.i);
  } else if (token == DDS_SQL_TK_FLOAT) {
    *pos += (size_t)sprintf (out + *pos, "%.6f", node->token->n.r);
  } else if (token == DDS_SQL_TK_STRING || token == DDS_SQL_TK_ID) {
    *pos += (size_t)sprintf (out + *pos, "%s", node->token->s);
  } else if (token == DDS_SQL_TK_BLOB) {
    for (int i = 0; i < node->token->n.i; i++) {
      *pos += (size_t)sprintf (out + *pos, "%c", node->token->s[i]);
    }
  } else {
    char *op = NULL;
    switch (token)
    {
      case DDS_SQL_TK_UMINUS:   /* fall-through */
      case DDS_SQL_TK_MINUS:    op = "-";       break;
      case DDS_SQL_TK_UPLUS:    /* fall-through */
      case DDS_SQL_TK_PLUS:     op = "+";       break;
      case DDS_SQL_TK_STAR:     op = "*";       break;
      case DDS_SQL_TK_SLASH:    op = "/";       break;
      case DDS_SQL_TK_REM:      op = "%";       break;
      case DDS_SQL_TK_EQ:       op = "=";       break;
      case DDS_SQL_TK_LT:       op = "<";       break;
      case DDS_SQL_TK_LE:       op = "<=";      break;
      case DDS_SQL_TK_NE:       op = "!=";      break;
      case DDS_SQL_TK_LSHIFT:   op = "<<";      break;
      case DDS_SQL_TK_GT:       op = ">";       break;
      case DDS_SQL_TK_GE:       op = ">=";      break;
      case DDS_SQL_TK_RSHIFT:   op = ">>";      break;
      case DDS_SQL_TK_BITOR:    op = "|";       break;
      case DDS_SQL_TK_CONCAT:   op = "||";      break;
      case DDS_SQL_TK_BITAND:   op = "&";       break;
      case DDS_SQL_TK_BITNOT:   op = "~";       break;
      case DDS_SQL_TK_OR:       op = "OR";      break;
      case DDS_SQL_TK_NOT:      op = "NOT";     break;
      case DDS_SQL_TK_AND:      op = "AND";     break;
      case DDS_SQL_TK_DOT:      op = ".";       break;
      /* FIXME: missing implementation. */
      case DDS_SQL_TK_LIKE_KW:/*  op = "LIKE";    break;*/
      case DDS_SQL_TK_ESCAPE: /*  op = "ESCAPE";  break;*/
      case DDS_SQL_TK_COLLATE:/*  op = "COLLATE"; break;*/
      case DDS_SQL_TK_BETWEEN:/*  op = "BETWEEN"; break;*/
      default: abort();

    }
    *pos += (size_t)sprintf (out + *pos, "%s", op);
    *pos += (size_t)sprintf (out + *pos, "%c", '(');
    expr_node_explore(node->l, depth+1, out, pos);
    if (node->l != NULL) *pos += (size_t)sprintf (out + *pos, "%c", ',');
    expr_node_explore(node->r, depth+1, out, pos);
    *pos += (size_t)sprintf (out + *pos, "%c", ')');
  }
}

/* NOTE:
 * Be carefull, this test more strict then original parser. For tree definition
 * tokens from above function should be used only. All inconsitency will be
 * treat as failed test.
 * */
CU_TheoryDataPoints(ddsc_sql, expr_parse) = {
  CU_DataPoints(char *,
    "height-1*2-3",
    "height -1 -2 -3",
    "((-?1-2*?2-3+5) < (?1 +?2)) <= height",
    "NOT ((2 + (bob+8/2+7) - 5) AND (?1 + ((?2 + height - 1) * length) - 3)) OR g",
    "1+x\'32\'+?1*height*3*(\'1\'+1)-2*?2-1",
    "2 - 1 + 2/2*3*(1+?1) - ?2 * height",
    "-(+(-(a)))-(+(-(b)))",
    "-+-?1-+-2",
    "a AND ?1 OR b AND c AND ?2 OR 1+h",
    "a AND ?1 OR b AND 0 AND ?2 OR 1+h",
    "1+2*5-3%2*6-(7*3/(1+1))",
  ),
  CU_DataPoints(char *,
    "+(?height,-5)",
    "+(?height,-6)",
    "<=(<(+(-(?1),+(*(-2,?2),2)),+(?1,?2)),?height)",
    "OR(NOT(AND(+(-3,+(?bob,11)),+(+(?1,*(+(+(?2,?height),-1),?length)),-3))),?g)",
    "+(+(3,*(*(?1,?height),6)),+(*(-2,?2),-1))",
    "+(+(1,*(3,+(1,?1))),*(-(?2),?height))",
    "+(-(+(-(?a))),-(+(-(?b))))",
    "+(-(+(-(?1))),2)",
    "OR(OR(AND(?a,?1),AND(AND(?b,?c),?2)),+(1,?h))",
    "OR(OR(AND(?a,?1),AND(AND(?b,0),?2)),+(1,?h))",
    "-5"
  )
};

CU_Theory((char *s, char *e), ddsc_sql, expr_parse)
{
  struct dds_sql_expr *exp = NULL;
  dds_return_t ret = dds_sql_expr_init(&exp, DDS_SQL_EXPR_KIND_PARAMETER);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);

  ret = dds_sql_expr_parse((const unsigned char *)s, &exp);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);

  char res[EXPBUF_MAX_LEN] = {0};
  size_t pos = 0;
  expr_node_explore(exp->node, 0, (char *)res, &pos);
  CU_ASSERT(pos == strlen(e));

  dds_sql_expr_fini(exp);

  bool result = false;
  if (!(result = !memcmp(res, e, pos)))
    fprintf (stderr, "[FAILED]:\t%s\nexpected:\t%s (%ld)\nactual:\t%s (%ld)\n", s, e, strlen(e), res, pos);
  CU_ASSERT(result);
}

#define TEST_BIND_PARAMETER_NUM(e,ex,ix,fn,p,rx) \
  do { \
    dds_return_t ret = dds_sql_expr_bind_##fn(ex, ix, p); \
    bool result = false; \
    if (!(result = (ret == rx))) { \
      fprintf (stderr, "[FAILED]:\t%s bind param id: %d\n", e, ix); \
    } else if (ret == DDS_RETCODE_OK) { \
      struct dds_sql_param tmpl = {.id.index=ix}; \
      struct dds_sql_param *param = ddsrt_hh_lookup(ex->param_tokens, &tmpl); \
      CU_ASSERT(param != NULL); \
      if (param->token.tok == DDS_SQL_TK_INTEGER) \
        CU_ASSERT(param->token.n.i == (int64_t)p); \
      else \
        CU_ASSERT(param->token.n.r == (double)p); \
    } \
    CU_ASSERT(result); \
  } while (0)

#define TEST_BIND_PARAMETER_S(e,ex,ix,fn_call,p,len,rx) \
  do { \
    dds_return_t ret = fn_call; \
    bool result = false; \
    if (!(result = (ret == rx))) { \
      fprintf (stderr, "[FAILED]:\t%s bind param id: %d\n", e, ix); \
    } else if (ret == DDS_RETCODE_OK) { \
      struct dds_sql_param tmpl = {.id.index=ix}; \
      struct dds_sql_param *param = ddsrt_hh_lookup(ex->param_tokens, &tmpl); \
      CU_ASSERT(param != NULL); \
      CU_ASSERT(!memcmp(param->token.s, p, len)); \
    } \
    CU_ASSERT(result); \
  } while (0)

#define TEST_BIND_PARAMETER_STR(e,ex,ix,fn,p,len,rx) \
  TEST_BIND_PARAMETER_S(e,ex,ix,dds_sql_expr_bind_##fn(ex, ix, p),p,len,rx)
#define TEST_BIND_PARAMETER_BLOB(e,ex,ix,fn,p,len,rx) \
  TEST_BIND_PARAMETER_S(e,ex,ix,dds_sql_expr_bind_##fn(ex, ix, p, len),p,len,rx)

CU_Test(ddsc_sql, bind_parameter)
{
  char *s = "?1";
  struct dds_sql_expr *exp = NULL;
  dds_return_t retcode = dds_sql_expr_init(&exp, DDS_SQL_EXPR_KIND_PARAMETER);
  CU_ASSERT(retcode == DDS_RETCODE_OK);
  retcode = dds_sql_expr_parse((const unsigned char *)s, &exp);
  CU_ASSERT(retcode == DDS_RETCODE_OK);
  TEST_BIND_PARAMETER_NUM(s,    exp,    1U,   integer,    12345,    DDS_RETCODE_OK);
  TEST_BIND_PARAMETER_NUM(s,    exp,    0U,   integer,    12345,    DDS_RETCODE_BAD_PARAMETER);
  TEST_BIND_PARAMETER_NUM(s,    exp,    1U,   real,       10.0,     DDS_RETCODE_OK);

  TEST_BIND_PARAMETER_STR(s,    exp,    1U,   string,     "ABC",    4,    DDS_RETCODE_OK);
{ unsigned char u[] = {'A','B','C'};
  TEST_BIND_PARAMETER_BLOB(s,   exp,    1U,   blob,           u,    3,    DDS_RETCODE_OK); }

  dds_sql_expr_fini(exp);
}

typedef struct {
  struct {
    int   t;
    char *v;
  } params[2];
} expr_build_param_t;

#define EXPR_BUILD_PARAMS(t1,p1,t2,p2) \
  {.params={{t1,p1},{t2,p2}}}

CU_TheoryDataPoints(ddsc_sql, expr_build) = {
  CU_DataPoints(char *,
    "height-1*2-3",
    "height -1 -2 -3",
    "((-?1-2*?2-3+5) < (?1 +?2)) <= height",
    "NOT ((2 + (bob+8/2+7) - 5) AND (?1 + ((?2 + height - 1) * length) - 3)) OR g",
    "NOT ((2 + (?1+8/2+7) - 5) AND (?1 + ((?2 + height - 1) * length) - 3)) OR ?2",
    "1+x\'32\'+?1*height*3*(\'1\'+1)-2*?2-1",
    "2 - 1 + 2/2*3*(1+?1) - ?2 * height",
    "-(+(-(a)))-(+(-(b)))",
    "-+-?1-+-2",
    "a AND ?1 OR b AND c AND ?2 OR 1+h",
    "1+2*5-3%2*6-(7*3/(1+1))",
    "(height-?1) > (?1+?2*(?1-?2/?1*(1+?2)))",
    "a AND ?1 OR b",
    "length = 0 AND length * 1 AND length * ?1",
    "length = 0 AND length * 1 OR length * ?1"
  ),
  CU_DataPoints(expr_build_param_t,
    EXPR_BUILD_PARAMS(0,0,0,0),
    EXPR_BUILD_PARAMS(0,0,0,0),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_INTEGER, "12345",    DDS_SQL_TK_BLOB,    "x'31'"),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_FLOAT,   "0",        DDS_SQL_TK_INTEGER, "31"),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_FLOAT,   "0",        DDS_SQL_TK_INTEGER, "31"),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_STRING,  "'ABC'",    DDS_SQL_TK_BLOB,    "x'41'"),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_STRING,  "'ABC'",    DDS_SQL_TK_BLOB,    "x'41'"),
    EXPR_BUILD_PARAMS(0,0,0,0),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_INTEGER, "1",        0,0),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_INTEGER, "1",        DDS_SQL_TK_FLOAT,   "0.1"),
    EXPR_BUILD_PARAMS(0,0,0,0),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_FLOAT,   "1.0",      DDS_SQL_TK_INTEGER, "0"),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_INTEGER, "0",        0,0),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_INTEGER, "0",        0,0),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_INTEGER, "0",        0,0),
  ),
  CU_DataPoints(char *,
    "+(?height,-5)",
    "+(?height,-6)",
    "<=(1,?height)",
    "OR(NOT(AND(+(-3,+(?bob,11)),+(+(0.000000,*(+(+(31,?height),-1),?length)),-3))),?g)",
    "1",
    "2",
    "4",
    "+(-(+(-(?a))),-(+(-(?b))))",
    "3",
    "OR(OR(AND(?a,1),AND(AND(?b,?c),0.100000)),+(1,?h))",
    "-5",
    ">(+(?height,-1.000000),1.000000)",
    "OR(0,?b)",
    "0",
    "OR(AND(=(?length,0),*(?length,1)),0)",
  )
};

#define PARAM_APPLY(exp,p) \
  do { \
    int token; \
    if ((token = p.t) != 0) \
    { \
      if (token == DDS_SQL_TK_INTEGER || token == DDS_SQL_TK_FLOAT) { \
        union{ int64_t i; double  r; } b; \
        char *ss= p.v; \
        ret = dds_sql_get_numeric((void **)&b, (const unsigned char **)&ss, &token, (int)strlen(ss)); \
        CU_ASSERT(ret == 0); \
        if (token == DDS_SQL_TK_INTEGER) \
          ret = dds_sql_expr_bind_integer(exp, i+1, b.i); \
        else \
          ret = dds_sql_expr_bind_real(exp, i+1, b.r); \
        CU_ASSERT(ret == DDS_RETCODE_OK); \
      } else if (token == DDS_SQL_TK_STRING || token == DDS_SQL_TK_BLOB) { \
        char *b = NULL; \
        char *ss = p.v; \
        ret = dds_sql_get_string((void **)&b, (const unsigned char **)&ss, &token, (int)strlen(ss)); \
        size_t ss_s = strlen(ss); \
        if (token == DDS_SQL_TK_STRING) \
          CU_ASSERT(ret == (int)(ss_s - 1)); \
        else \
          CU_ASSERT(ret == (int)(ss_s - 3)/2); \
        if (token == DDS_SQL_TK_STRING) \
          ret = dds_sql_expr_bind_string(exp, i+1, b); \
        else \
          ret = dds_sql_expr_bind_blob(exp, i+1, (unsigned char *)b, (uint32_t)ret); \
        CU_ASSERT(ret == DDS_RETCODE_OK); \
        free (b); \
      } \
    }  \
  } \
  while (0);

CU_Theory((char *s, expr_build_param_t p, char *e), ddsc_sql, expr_build)
{
  struct dds_sql_expr *exp = NULL;
  dds_return_t ret = dds_sql_expr_init(&exp, DDS_SQL_EXPR_KIND_PARAMETER);
  CU_ASSERT(ret == DDS_RETCODE_OK);
  ret = dds_sql_expr_parse((const unsigned char *)s, &exp);
  CU_ASSERT(ret == DDS_RETCODE_OK);

  for (uint32_t i = 0; i < 2; i++) { PARAM_APPLY(exp, p.params[i]); }

  struct dds_sql_expr *exo = NULL;
  ret = dds_sql_expr_init(&exo, DDS_SQL_EXPR_KIND_VARIABLE);
  CU_ASSERT(ret == DDS_RETCODE_OK);
  ret = dds_sql_expr_build(exp, &exo);
  CU_ASSERT(ret == DDS_RETCODE_OK);

  char res[EXPBUF_MAX_LEN] = {'\0'};
  size_t pos = 0;
  expr_node_explore(exo->node, 0, (char *)res, &pos);
  CU_ASSERT(pos == strlen(e));
  bool result = false;
  if (!(result = !memcmp(res, e, pos)))
    fprintf (stderr, "[FAILED]:\t%s\nexpected:\t%s (%ld)\nactual:\t%s (%ld)\n", s, e, strlen(e), res, pos);
  CU_ASSERT(result);

  dds_sql_expr_fini(exo);
  dds_sql_expr_fini(exp);
}

typedef struct {
  struct {
    char *n;
    int t;
    char *v;
  } vars[2];
} expr_build_var_t;

#define EXPR_BUILD_VARS(n1,t1,p1,n2,t2,p2) \
  {.vars={{n1,t1,p1},{n2,t2,p2}}}

CU_TheoryDataPoints(ddsc_sql, expr_eval) = {
  CU_DataPoints(char *,
    "height-1*2-3",
    "height -1 -2 -3",
    "((-?1-2*?2-3+5) < (?1 +?2)) <= height",
    "NOT ((2 + (?1+8/2+7) - 5) AND (?1 + ((?2 + height - 1) * length) - 3)) OR ?1",
    "1+x\'32\'+?1*height*3*(\'1\'+1)-2*?2-1",
    "2 - 1 + 2/2*3*(1+?1) - ?2 * height",
    "-(+(-(a.b.`c`)))-(+(-(b.`a.c`)))",
    "-+-?1-+-height",
    "a AND ?1 OR b AND 1 AND ?2 OR 1+?2",
    "(height-?1) > (?1+?2*(?1-?2/?1*(1+?2)))",
    "a AND ?1 OR b",
    "length = 0 AND length * 1 OR length * ?1",
    /* FIXME: case that really asking for some optimization on `eval` phase. */
    "(height.c + length > ?1) AND length != 10 OR ?2 <> height.c"
  ),
  CU_DataPoints(expr_build_param_t,
    EXPR_BUILD_PARAMS(0,0,0,0),
    EXPR_BUILD_PARAMS(0,0,0,0),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_INTEGER, "12345",    DDS_SQL_TK_BLOB,    "x'31'"),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_FLOAT,   "0.0",      DDS_SQL_TK_INTEGER, "31"),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_STRING,  "'ABC'",    DDS_SQL_TK_BLOB,    "x'41'"),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_STRING,  "'ABC'",    DDS_SQL_TK_BLOB,    "x'41'"),
    EXPR_BUILD_PARAMS(0,0,0,0),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_INTEGER, "1",        0,0),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_INTEGER, "1",        DDS_SQL_TK_FLOAT,   "0.1"),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_INTEGER, "0",        0,0),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_INTEGER, "0",        0,0),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_INTEGER, "0",        0,0),
    EXPR_BUILD_PARAMS(DDS_SQL_TK_INTEGER, "1",        DDS_SQL_TK_INTEGER, "11"),
  ),
  CU_DataPoints(expr_build_var_t,
    EXPR_BUILD_VARS("height", DDS_SQL_TK_INTEGER, "0",    0,0,0),
    EXPR_BUILD_VARS("height", DDS_SQL_TK_FLOAT,   "0.1",  0,0,0),
    EXPR_BUILD_VARS("height", DDS_SQL_TK_INTEGER, "0",    0,0,0),
    EXPR_BUILD_VARS("height", DDS_SQL_TK_STRING,  "'0'",  "length", DDS_SQL_TK_BLOB,    "x'42'"),
    EXPR_BUILD_VARS("height", DDS_SQL_TK_INTEGER, "1",    0,0,0),
    EXPR_BUILD_VARS("height", DDS_SQL_TK_INTEGER, "0",    0,0,0),
    EXPR_BUILD_VARS("a.b.c",  DDS_SQL_TK_INTEGER, "1",    "b.a.c",  DDS_SQL_TK_INTEGER, "1"),
    EXPR_BUILD_VARS("height", DDS_SQL_TK_STRING,  "'A'",  0,0,0),
    EXPR_BUILD_VARS("a",      DDS_SQL_TK_INTEGER, "1",    "b",      DDS_SQL_TK_INTEGER, "0"),
    EXPR_BUILD_VARS("height", DDS_SQL_TK_INTEGER, "0",    0,0,0),
    EXPR_BUILD_VARS("a",      DDS_SQL_TK_INTEGER, "1",    "b",      DDS_SQL_TK_INTEGER, "0"),
    EXPR_BUILD_VARS("length", DDS_SQL_TK_INTEGER, "0",    0,0,0),
    EXPR_BUILD_VARS("height.c", DDS_SQL_TK_INTEGER, "10",   "length", DDS_SQL_TK_INTEGER, "10"),
  ),
  CU_DataPoints(char *,
    "-5",
    "-5.900000",
    "0",
    "0.000000",
    "2",
    "4",
    "2",
    "1",
    "1",
    "0",
    "0",
    "0",
    "1",
  )
};

#define VAR_APPLY(exp,var) \
  do { \
    int token; \
    if ((token = var.t) != 0) \
    { \
      uintptr_t hash = (uintptr_t)(void *)var.n; \
      if (token == DDS_SQL_TK_INTEGER || token == DDS_SQL_TK_FLOAT) { \
        union{ int64_t i; double  r; } b; \
        char *ss= var.v; \
        ret = dds_sql_get_numeric((void **)&b, (const unsigned char **)&ss, &token, (int)strlen(ss)); \
        CU_ASSERT(ret == 0); \
        if (token == DDS_SQL_TK_INTEGER) \
          ret = dds_sql_expr_bind_integer(exp, hash, b.i); \
        else \
          ret = dds_sql_expr_bind_real(exp, hash, b.r); \
        /* CU_ASSERT(ret == DDS_RETCODE_OK); \ */ \
      } else if (token == DDS_SQL_TK_STRING || token == DDS_SQL_TK_BLOB) { \
        char *b = NULL; \
        char *ss = var.v; \
        ret = dds_sql_get_string((void **)&b, (const unsigned char **)&ss, &token, (int)strlen(ss)); \
        size_t ss_s = strlen(ss); \
        if (token == DDS_SQL_TK_STRING) \
          CU_ASSERT(ret == (int)(ss_s - 1)); \
        else \
          CU_ASSERT(ret == (int)(ss_s - 3)/2); \
        if (token == DDS_SQL_TK_STRING) \
          ret = dds_sql_expr_bind_string(exp, hash, b); \
        else \
          ret = dds_sql_expr_bind_blob(exp, hash, (unsigned char *)b, (uint32_t)ret); \
        /* CU_ASSERT(ret == DDS_RETCODE_OK); \ */ \
        free (b); \
      } \
    }  \
  } \
  while (0);

CU_Theory((char *s, expr_build_param_t p, expr_build_var_t v, char *e), ddsc_sql, expr_eval)
{
  struct dds_sql_expr *exp = NULL;
  dds_return_t ret = dds_sql_expr_init(&exp, DDS_SQL_EXPR_KIND_PARAMETER);
  CU_ASSERT(ret == DDS_RETCODE_OK);
  ret = dds_sql_expr_parse((const unsigned char *)s, &exp);
  CU_ASSERT(ret == DDS_RETCODE_OK);

  fprintf (stderr, "\t\t%-80s\n", s);

  for (uint32_t i = 0; i < 2; i++) { PARAM_APPLY(exp, p.params[i]); }

  struct dds_sql_expr *exo = NULL;
  ret = dds_sql_expr_init(&exo, DDS_SQL_EXPR_KIND_VARIABLE);
  CU_ASSERT(ret == DDS_RETCODE_OK);
  ret = dds_sql_expr_build(exp, &exo);
  CU_ASSERT(ret == DDS_RETCODE_OK);
  {
    char res[EXPBUF_MAX_LEN] = {'\0'};
    size_t pos = 0;
    expr_node_explore(exo->node, 0, (char *)res, &pos);
    fprintf (stderr, "\t\t%-80s\n", res);
  }
  for (uint32_t i = 0; i < 2; i++) { VAR_APPLY(exo, v.vars[i]); }

  struct dds_sql_token *r = NULL;

  clock_t begin = clock();
  ret = dds_sql_expr_eval(exo, &r);
  clock_t end = clock();
  double time_spent = (double)(end - begin) / CLOCKS_PER_SEC;
  fprintf (stderr, "\t======= %f\n", time_spent);

  CU_ASSERT(ret == DDS_RETCODE_OK);
  struct dds_sql_expr_node tmp = {0};
  tmp.token = r;

  char res[EXPBUF_MAX_LEN] = {'\0'};
  size_t pos = 0;
  expr_node_explore(&tmp, 0, (char *)res, &pos);
  CU_ASSERT(pos == strlen(e));
  bool result = false;
  if (!(result = !memcmp(res, e, pos)))
    fprintf (stderr, "[FAILED]:\t%s\nexpected:\t%s (%ld)\nactual:\t%s (%ld)\n", s, e, strlen(e), res, pos);
  CU_ASSERT(result);

  if (r->s) free (r->s);
  free (r);
  dds_sql_expr_fini(exo);
  dds_sql_expr_fini(exp);
}
