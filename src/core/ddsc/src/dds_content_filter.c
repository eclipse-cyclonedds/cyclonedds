// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"

#include "dds__content_filter.h"

dds_return_t dds_expression_filter_create(const char *expression, dds_expression_content_filter_t **filter)
{
  if (filter == NULL || expression == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  dds_expression_content_filter_t *ef = ddsrt_malloc(sizeof(*ef));
  ef->expression = ddsrt_strdup(expression);
  ef->nparam = dds_sql_expr_count_params(expression);
  ef->param = (ef->nparam > 0) ? ddsrt_malloc(sizeof(*ef->param)*ef->nparam): NULL;
  *filter = ef;
  return DDS_RETCODE_OK;
}

void dds_expression_filter_free(dds_expression_content_filter_t *filter)
{
  if (filter == NULL)
    return;
  dds_expression_content_filter_t *ef = filter;
  ddsrt_free(ef->expression);
  ddsrt_free(ef->param);
  ddsrt_free(ef);
}

static void filter_param_clean(struct dds_expression_filter_param *param)
{
  assert (param != NULL);
  param->n.i = 0;
  if      (param->t == DDS_EXPR_FILTER_PARAM_STRING) ddsrt_free(param->s.s);
  else if (param->t == DDS_EXPR_FILTER_PARAM_BLOB)   ddsrt_free(param->s.u);
  param->s.s = NULL;
  param->sz = 0;
}

static bool filter_param_copy(struct dds_expression_filter_param *from, struct dds_expression_filter_param *to)
{
  if (from == NULL || to == NULL)
    return false;
  filter_param_clean (to);
  to->t = from->t;
  to->n = from->n;
  if ((to->sz = from->sz) != 0) {
    if      (to->t == DDS_EXPR_FILTER_PARAM_STRING) to->s.s = ddsrt_strdup(from->s.s);
    else if (to->t == DDS_EXPR_FILTER_PARAM_BLOB)   to->s.u = ddsrt_memdup(from->s.u, to->sz);
    else return false;
  }
  return true;
}

static dds_return_t expression_filter_param_set (dds_expression_content_filter_t *filter, size_t id, struct dds_expression_filter_param p)
{
  dds_expression_content_filter_t *ef = filter;
  if (id > ef->nparam || ef->param == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  return filter_param_copy(&p, &ef->param[id-1]) ? DDS_RETCODE_OK: DDS_RETCODE_BAD_PARAMETER;
}

dds_return_t dds_expression_filter_bind_integer (dds_expression_content_filter_t *filter, size_t id, int64_t param)
{
  struct dds_expression_filter_param p = {.t = DDS_EXPR_FILTER_PARAM_INTEGER, .n.i = param};
  return expression_filter_param_set(filter, id, p);
}

dds_return_t dds_expression_filter_bind_real (dds_expression_content_filter_t *filter, size_t id, double param)
{
  struct dds_expression_filter_param p = { .t = DDS_EXPR_FILTER_PARAM_REAL, .n.d = param };
  return expression_filter_param_set(filter, id, p);
}

dds_return_t dds_expression_filter_bind_string (dds_expression_content_filter_t *filter, size_t id, char *param)
{
  struct dds_expression_filter_param p = { .t = DDS_EXPR_FILTER_PARAM_STRING, .s.s = param, .sz = strlen(param) };
  return expression_filter_param_set(filter, id, p);
}

dds_return_t dds_expression_filter_bind_blob (dds_expression_content_filter_t *filter, size_t id, unsigned char *param, size_t param_sz)
{
  struct dds_expression_filter_param p = { .t = DDS_EXPR_FILTER_PARAM_BLOB, .s.u = param, .sz = param_sz};
  return expression_filter_param_set(filter, id, p);
}

dds_return_t dds_function_filter_create(const dds_function_content_filter_mode_t mode, const dds_function_content_filter_fn_t fn, dds_function_content_filter_t **filter)
{
  if (filter == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  dds_function_content_filter_t *ff = ddsrt_malloc(sizeof(*ff));
  ff->mode = mode;
  ff->f = fn;
  ff->arg = NULL;
  *filter = ff;
  return DDS_RETCODE_OK;
}

void dds_function_filter_free(dds_function_content_filter_t *filter)
{
  if (filter == NULL)
    return;
  dds_function_content_filter_t *ff = filter;
  ddsrt_free(ff);
}

dds_return_t dds_function_filter_bind_arg (dds_function_content_filter_t *filter, void *arg)
{
  if (filter->mode == DDS_TOPIC_FILTER_NONE || filter->mode == DDS_TOPIC_FILTER_SAMPLE) {
    ; // do not break old api.
  } else {
    filter->arg = arg; // no responsibility on our side, since we don't own users arg.
  }
  return DDS_RETCODE_OK;
}

static bool expression_filter_copy(const dds_expression_content_filter_t *from, dds_expression_content_filter_t *to)
{
  if (from == NULL || to == NULL)
    return false;
  to->expression = ddsrt_strdup(from->expression);
  to->nparam = from->nparam;
  to->param = (struct dds_expression_filter_param *) ddsrt_malloc(sizeof(*to->param) * to->nparam);
  for (size_t i = 0; i < to->nparam; i++)
    filter_param_copy (&from->param[i], &to->param[i]);
  return true;
}

static bool function_filter_copy(const dds_function_content_filter_t *from, dds_function_content_filter_t *to)
{
  if (from == NULL || to == NULL)
    return false;
  to->f = from->f;
  to->mode = from->mode;
  to->arg = from->arg;
  return true;
}

bool dds_content_filter_valid (const struct dds_content_filter *filter)
{
  if (filter == NULL)
    return true;

  return filter->kind == DDS_CONTENT_FILTER_EXPRESSION? filter->filter.expr != NULL: filter->filter.func != NULL;
}

void dds_content_filter_free (struct dds_content_filter *filter)
{
  if (filter == NULL)
    return;

  switch (filter->kind)
  {
    case DDS_CONTENT_FILTER_EXPRESSION:
      dds_expression_filter_free (filter->filter.expr);
      break;
    case DDS_CONTENT_FILTER_FUNCTION:
      dds_function_filter_free (filter->filter.func);
      break;
  }

  ddsrt_free (filter);
}

struct dds_content_filter *dds_content_filter_dup (const struct dds_content_filter *filter)
{
  if (filter == NULL)
    return NULL;
  struct dds_content_filter *res = (struct dds_content_filter *) ddsrt_malloc(sizeof(*res));
  switch (filter->kind)
  {
    case DDS_CONTENT_FILTER_EXPRESSION:
      dds_expression_content_filter_t *expr = (dds_expression_content_filter_t *) ddsrt_malloc(sizeof(*expr));
      if (!expression_filter_copy(filter->filter.expr, expr)) {
        ddsrt_free (expr);
        goto err_copy;
      }
      res->filter.expr = expr;
      break;
    case DDS_CONTENT_FILTER_FUNCTION:
      dds_function_content_filter_t *func = (dds_function_content_filter_t *) ddsrt_malloc(sizeof(*func));
      if (!function_filter_copy (filter->filter.func, func)) {
        ddsrt_free (func);
        goto err_copy;
      }
      res->filter.func = func;
      break;
  }

  res->kind = filter->kind;
  return res;

err_copy:
  ddsrt_free (res);
  return NULL;
}
