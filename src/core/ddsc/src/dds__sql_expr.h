// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#ifndef DDS__SQL_EXPR_H
#define DDS__SQL_EXPR_H

#include "dds/ddsrt/retcode.h"
#include <stdint.h>
#include <stdio.h>

#if defined (__cplusplus)
extern "C" {
#endif

#define DDS_SQL_TK_NOT                             19
#define DDS_SQL_TK_LP                              22
#define DDS_SQL_TK_RP                              23
#define DDS_SQL_TK_COMMA                           25
#define DDS_SQL_TK_CAST                            36
#define DDS_SQL_TK_OR                              43
#define DDS_SQL_TK_AND                             44
#define DDS_SQL_TK_LIKE_KW                         48
#define DDS_SQL_TK_BETWEEN                         49
#define DDS_SQL_TK_NE                              53
#define DDS_SQL_TK_EQ                              54
#define DDS_SQL_TK_GT                              55
#define DDS_SQL_TK_LE                              56
#define DDS_SQL_TK_LT                              57
#define DDS_SQL_TK_GE                              58
#define DDS_SQL_TK_ESCAPE                          59
#define DDS_SQL_TK_ID                              60
#define DDS_SQL_TK_BITAND                         103
#define DDS_SQL_TK_BITOR                          104
#define DDS_SQL_TK_LSHIFT                         105
#define DDS_SQL_TK_RSHIFT                         106
#define DDS_SQL_TK_PLUS                           107
#define DDS_SQL_TK_MINUS                          108
#define DDS_SQL_TK_STAR                           109
#define DDS_SQL_TK_SLASH                          110
#define DDS_SQL_TK_REM                            111
#define DDS_SQL_TK_CONCAT                         112
#define DDS_SQL_TK_COLLATE                        114
#define DDS_SQL_TK_BITNOT                         115
#define DDS_SQL_TK_STRING                         118
#define DDS_SQL_TK_DOT                            142
#define DDS_SQL_TK_FLOAT                          154
#define DDS_SQL_TK_BLOB                           155
#define DDS_SQL_TK_INTEGER                        156
#define DDS_SQL_TK_VARIABLE                       157
#define DDS_SQL_TK_UPLUS                          173
#define DDS_SQL_TK_UMINUS                         174
#define DDS_SQL_TK_QNUMBER                        183
#define DDS_SQL_TK_SPACE                          184
#define DDS_SQL_TK_COMMENT                        185
#define DDS_SQL_TK_ILLEGAL                        186

#define DDS_SQL_AFFINITY_NONE       0x40    /* '@' */
#define DDS_SQL_AFFINITY_BLOB       0x41    /* 'A' */
#define DDS_SQL_AFFINITY_TEXT       0x42    /* 'B' */
#define DDS_SQL_AFFINITY_NUMERIC    0x43    /* 'C' */
#define DDS_SQL_AFFINITY_INTEGER    0x44    /* 'D' */
#define DDS_SQL_AFFINITY_REAL       0x45    /* 'E' */

#define DDS_SQL_ASSOC_LEFT          0x01
#define DDS_SQL_ASSOC_RIGHT         0x02
#define DDS_SQL_ASSOC_NONE          0x03

struct dds_sql_token {
  /* ------- base ------- */
  int     tok;    // DDS_SQL_TK_*
  int     aff;    // DDS_SQL_AFFINITY_*

  /* ----- operator ----- */
  int    prc;    // [0...10]
  int    asc;    // DDS_SQL_ASSOC_*

  /* ----- constant ----- */
  union {
    double  r;    // DDS_SQL_TK_FLOAT
    int64_t i;    // DDS_SQL_TK_INTEGER
    /* FIXME:
    uint64_t u;   // DDS_SQL_TK_UNSIGNED
    */
  } n;
  char *s;        // DDS_SQL_TK_BLOB/DDS_SQL_TK_STRING
};


int dds_sql_get_token(const unsigned char *s, int *token);
int dds_sql_get_numeric(void *blob, const unsigned char **c, int *tk, int tk_sz);
int dds_sql_get_string(void **blob, const unsigned char **c, const int *tk, int tk_sz);
int dds_sql_apply_affinity(struct dds_sql_token *st, const int aff);


typedef struct dds_sql_token dds_sql_token_t;
typedef struct dds_sql_expr_node {

  /* FIXME:
   * Possible way of additional optimization is "sibling" field.
   *    ?1 + 'var1 + 5 + 2 + ?2 + 3
   * expression above easily transforms into:
   *    ?1 + 'var + 9 + ?2
   * the problem is that it presented in b-tree form, that lose information
   * about "sibling" like relationships between nodes:
   *    +(?1, +('var, +(9, ?2)))
   * it's become more clear after parameters apply during "expr_build"
   * operation, since we additionaly try to optimize what we can:
   *    ?1=1;?2=2;
   *    +(1, +('var, +(9, 2)))
   *    +(1, +('var, 11))                         [IMPROVE]
   *    ^--- (ridiculous! why not just eval. it too)
   *    ---------------------------------------------------
   *    +(12, 'var)                               [PREFECT]
   * */

  size_t height;
  dds_sql_token_t *token;
  struct dds_sql_expr_node *p;
  struct dds_sql_expr_node *l;
  struct dds_sql_expr_node *r;
} dds_sql_expr_node_t;

typedef struct dds_sql_param {
  struct dds_sql_token token;
  union {
    uint64_t index;
    char *str;
  } id;
  int tok;                    // DDS_SQL_TK_ID/DDS_SQL_TK_VARIABLE
} dds_sql_param_t;

enum dds_sql_expr_param_kind
{
  DDS_SQL_EXPR_KIND_PARAMETER = DDS_SQL_TK_VARIABLE,
  DDS_SQL_EXPR_KIND_VARIABLE  = DDS_SQL_TK_ID
};

typedef struct dds_sql_expr {
  struct dds_sql_expr_node *node;
  enum dds_sql_expr_param_kind param_kind;
  uint32_t nparams;
  struct ddsrt_hh *param_tokens;
  /*
   * expression may be wrong on a different stages, for that purpouse it's not
   * so bad to have proper error handling. at leats to notify user about his
   * mistakes and ofcourse to save some resources on evolve incorrect
   * statements.
   * */
} dds_sql_expr_t;

/* FIXME: to be implemented to validate expression string before parse. */
dds_return_t dds_sql_expr_validate(const char *s);
/*
 * calculate the amount of parameters presented in expression. which is usefull
 * to do before event start parsing process.
 * */
size_t dds_sql_expr_count_params(const char *s);

dds_return_t dds_sql_expr_init(struct dds_sql_expr **expr, enum dds_sql_expr_param_kind kind);
void dds_sql_expr_fini(struct dds_sql_expr *expr);

dds_return_t dds_sql_expr_parse(const unsigned char *s, struct dds_sql_expr **ex);

dds_return_t dds_sql_expr_bind_integer(const struct dds_sql_expr *ex, uintptr_t i, int64_t p);
dds_return_t dds_sql_expr_bind_real   (const struct dds_sql_expr *ex, uintptr_t i, double p);
dds_return_t dds_sql_expr_bind_string (const struct dds_sql_expr *ex, uintptr_t i, char s[]);
dds_return_t dds_sql_expr_bind_blob   (const struct dds_sql_expr *ex, uintptr_t i, unsigned char b[], size_t s);

dds_return_t dds_sql_expr_build(const struct dds_sql_expr *ex, struct dds_sql_expr **out);
dds_return_t dds_sql_expr_eval(const struct dds_sql_expr *ex, struct dds_sql_token **out);

#if defined (__cplusplus)
}
#endif

#endif // DDS__SQL_EXPR_H
