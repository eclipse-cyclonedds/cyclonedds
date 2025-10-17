// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDS__CONTENT_FILTER_H
#define DDS__CONTENT_FILTER_H

#include "dds/dds.h"
#include "dds__sql_expr.h"

#if defined (__cplusplus)
extern "C" {
#endif

/* #define DDS_EXPR_FILTER_PARAM_UINTEGER  DDS_SLQ_TK_UINT */
#define DDS_EXPR_FILTER_PARAM_INTEGER   DDS_SQL_TK_INTEGER
#define DDS_EXPR_FILTER_PARAM_REAL      DDS_SQL_TK_FLOAT
#define DDS_EXPR_FILTER_PARAM_STRING    DDS_SQL_TK_STRING
#define DDS_EXPR_FILTER_PARAM_BLOB      DDS_SQL_TK_BLOB

struct dds_expression_filter_param
{
  int t;
  union {
    int64_t i; double d;;
  } n;
  union {
    char *s;
    unsigned char *u;
  } s;
  size_t sz;
};

struct dds_expression_content_filter
{
  char *expression;
  size_t nparam;
  struct dds_expression_filter_param *param;
};

struct dds_content_filter *dds_content_filter_dup (const struct dds_content_filter *filter);
void dds_content_filter_free (struct dds_content_filter *filter);
bool dds_content_filter_valid (const struct dds_content_filter *filter);

#if defined (__cplusplus)
}
#endif

#endif // DDS__CONTENT_FILTER_H
