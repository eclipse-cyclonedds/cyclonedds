// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>

#include "dds/ddsi/ddsi_typebuilder.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds/ddsi/ddsi_typelib.h"

#include "dds__domain.h"
#include "dds__filter.h"
#include "dds__content_filter.h"
#include "dds__serdata_default.h"

#define DDS_EXPR_VAR_SET_REAL(e,so,i,t) \
  do { \
    t *v = (t *) (so); \
    ret = dds_sql_expr_bind_real (e, i, *v); \
    assert (ret == DDS_RETCODE_OK); \
  } while (0);

#define DDS_EXPR_VAR_SET_INTEGER(f,e,so,i,t) \
  do { \
    if (f & DDS_OP_FLAG_SGN) { \
      t *v = (t *) (so); \
      ret = dds_sql_expr_bind_integer (e, i ,*v); \
    } else { \
      u##t *v = (u##t *) (so); \
      ret = dds_sql_expr_bind_integer (e, i, *v); \
    } \
    assert (ret == DDS_RETCODE_OK); \
  } while (0);

static dds_return_t topic_expr_filter_vars_apply(const struct dds_expression_filter *filter, const void *sample)
{
  dds_return_t ret = DDS_RETCODE_OK;
  const dds_topic_descriptor_t *desc = filter->desc;

  for (size_t i = 0; i < desc->m_nkeys; i++)
  {
    dds_key_descriptor_t msg_field = desc->m_keys[i];
    uintptr_t id = (uintptr_t) msg_field.m_name;
    const uint32_t *op = desc->m_ops + msg_field.m_offset;
    const uint32_t op_type = DDS_OP_TYPE((desc->m_ops + op[1])[0]);
    /* const uint32_t op_sub = DDS_OP_SUBTYPE((desc->m_ops + op[1])[0]); */
    const uint32_t op_flags = DDS_OP_FLAGS((desc->m_ops + op[1])[0]);
    const uint32_t op_offs = (desc->m_ops + op[1])[1];

    switch (op_type)
    {
      case DDS_OP_TYPE_1BY:
      case DDS_OP_TYPE_2BY:
      case DDS_OP_TYPE_4BY:
      case DDS_OP_TYPE_8BY:
      case DDS_OP_TYPE_BLN:
      // TODO:
      // currently not supported by sql
      // DDS_OP_TYPE_ENU:
      // DDS_OP_TYPE_BMK:
      {
        if (op_flags & DDS_OP_FLAG_FP) {
          switch (op_type)
          {
            case DDS_OP_TYPE_4BY: {
              DDS_EXPR_VAR_SET_REAL(filter->bin_expr, (sample + op_offs), id, float);
              break;
            }
            case DDS_OP_TYPE_8BY: {
              DDS_EXPR_VAR_SET_REAL(filter->bin_expr, (sample + op_offs), id, double);
              break;
            }
            default:
              abort();
          }
          assert (ret == DDS_RETCODE_OK);
        } else {
          switch (op_type)
          {
            case DDS_OP_TYPE_BLN:
            case DDS_OP_TYPE_1BY: {
              DDS_EXPR_VAR_SET_INTEGER(op_flags, filter->bin_expr, (sample + op_offs), id, int8_t);
              break;
            }
            case DDS_OP_TYPE_2BY: {
              DDS_EXPR_VAR_SET_INTEGER(op_flags, filter->bin_expr, (sample + op_offs), id, int16_t);
              break;
            }
            case DDS_OP_TYPE_4BY: {
              DDS_EXPR_VAR_SET_INTEGER(op_flags, filter->bin_expr, (sample + op_offs), id, int32_t);
              break;
            }
            case DDS_OP_TYPE_8BY: {
              if (op_flags & DDS_OP_FLAG_SGN) {
                int64_t *i = (int64_t *)(sample + op_offs);
                ret = dds_sql_expr_bind_integer (filter->bin_expr, id, *i);
              } else {
              /* FIXME:
               * current implementation doesn't support 64 bit unsigned, since sql
               * expression evaluator have nothing to handle that type. */
                assert (false);
              }
              assert (ret == DDS_RETCODE_OK);
              break;
            }
            default: assert (false);
          }
        }
        break;
      }
      case DDS_OP_TYPE_STR:
      case DDS_OP_TYPE_BST:
      /* FIXME:
       * Q: how to get size? */

      // TODO:
      // currently not supported by sql
      // case DDS_OP_TYPE_WSTR:
      // case DDS_OP_TYPE_WCHAR:
        assert (false);
        break;
      case DDS_OP_TYPE_SEQ: {
        /* FIXME:
         * Q: can we determine octet sequence as a SQL_BLOB? */
        /* if (op_sub & DDS_OP_SUBTYPE_1BY) */

        assert (false);
        break;
      }
      default:
        abort();
    }
  }

  return ret;
}

static bool topic_expr_filter_reader_accept(const dds_reader *rd, const struct dds_filter *filter, const struct ddsi_serdata *sample, const struct dds_sample_info *si)
{
  (void) rd; (void) si;
  bool res = false;
  const struct dds_expression_filter *ef = (const struct dds_expression_filter *) filter;
  struct ddsi_serdata *sd = ddsi_serdata_ref_as_type (ef->st, (struct ddsi_serdata *)sample);
  struct ddsi_serdata *sd_un = ddsi_serdata_to_untyped(sd);
  void *s = ddsrt_malloc(sd->type->sizeof_type);
  res = ddsi_serdata_untyped_to_sample(ef->st, sd_un, s, NULL, 0);
  assert (res == true);

  dds_return_t ret = topic_expr_filter_vars_apply(ef, s);
  assert (ret == DDS_RETCODE_OK);

  struct dds_sql_token *r = NULL;
  ret = dds_sql_expr_eval (ef->bin_expr, &r);
  assert (ret == DDS_RETCODE_OK);

  /* actually we always expect numeric result, since we support boolean
   * expressions only.
   * FIXME: but there is still not validation for that! */

  res = (r->aff >= DDS_SQL_AFFINITY_NUMERIC) && r->n.i;
  free (r);
  return res;
}

static bool topic_expr_filter_writer_accept(const dds_writer *wr, const struct dds_filter *filter, const void *sample)
{
  (void) wr;
  bool res = true;
  const struct dds_expression_filter *ef = (const struct dds_expression_filter *) filter;
  dds_return_t ret = topic_expr_filter_vars_apply(ef, sample);
  assert (ret == DDS_RETCODE_OK);

  struct dds_sql_token *r = NULL;
  ret = dds_sql_expr_eval (ef->bin_expr, &r);
  assert (ret == DDS_RETCODE_OK);

  ret = (r->aff >= DDS_SQL_AFFINITY_NUMERIC) && r->n.i;
  free (r);
  return res;
}

static void topic_expr_filter_fini(struct dds_filter *filter)
{
  struct dds_expression_filter *ef = (struct dds_expression_filter *)filter;
  ddsrt_free(ef->expression);
  if (ef->bin_expr != NULL) dds_sql_expr_fini(ef->bin_expr);
  if (ef->expr != NULL) dds_sql_expr_fini(ef->expr);
  if (ef->desc != NULL) ddsi_topic_descriptor_fini(ef->desc);
  if (ef->st != NULL) ddsi_sertype_unref(ef->st);
  ddsrt_free(ef);
}

static dds_return_t topic_expr_filter_param_rebind (struct dds_filter *a, const struct dds_content_filter *b, const struct ddsi_sertype *st)
{
  assert (b->kind == DDS_CONTENT_FILTER_EXPRESSION);
  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_expression_filter *eflt = (struct dds_expression_filter *) a;
  struct dds_sql_expr *expr = eflt->expr;
  for (uint32_t i = 0; i < b->filter.expr->nparam; i++)
  {
    struct dds_expression_filter_param param = b->filter.expr->param[i];
    uint32_t id = i + 1U;
    switch (param.t)
    {
      case DDS_EXPR_FILTER_PARAM_INTEGER:
        ret = dds_sql_expr_bind_integer(expr, id, param.n.i);
        break;
      case DDS_EXPR_FILTER_PARAM_REAL:
        ret = dds_sql_expr_bind_real(expr, id, param.n.d);
        break;
      case DDS_EXPR_FILTER_PARAM_STRING:
        ret = dds_sql_expr_bind_string(expr, id, param.s.s);
        break;
      case DDS_EXPR_FILTER_PARAM_BLOB:
        ret = dds_sql_expr_bind_blob(expr, id, param.s.u, param.sz);
        break;
      default:
        abort();
    }
    if (ret != DDS_RETCODE_OK)
      goto err;
  }
  /* after get new parameters set we are free to rebuild operational
   * expression. */
  struct dds_sql_expr *exp = NULL;
  ret = dds_sql_expr_init(&exp, DDS_SQL_EXPR_KIND_VARIABLE);
  assert (ret == DDS_RETCODE_OK);
  if ((ret = dds_sql_expr_build(eflt->expr, &exp)) != DDS_RETCODE_OK) {
    dds_sql_expr_fini(exp);
    goto err;
  }
  if (eflt->bin_expr != NULL)
    dds_sql_expr_fini(eflt->bin_expr);
  eflt->bin_expr = exp;

  const ddsi_typeid_t *id = ddsi_sertype_typeid(st, DDSI_TYPEID_KIND_COMPLETE);
  struct ddsi_domaingv *gv = ddsrt_atomic_ldvoidp (&st->gv);
  struct ddsi_type *type = ddsi_type_lookup(gv, id);

  const char **fields = ddsrt_malloc(sizeof(eflt->bin_expr->nparams));
  size_t nfields = eflt->bin_expr->nparams;

  size_t i = 0;
  struct ddsrt_hh_iter it;
  for (dds_sql_param_t *param = ddsrt_hh_iter_first(eflt->bin_expr->param_tokens, &it); param != NULL; param = ddsrt_hh_iter_next(&it))
  {
    fields[i] = param->token.s;
    i++;
  }
  assert (i == nfields);

  struct ddsi_type *res_type = ddsi_type_dup_with_keys(type, fields, nfields);
  if (eflt->desc != NULL)
    ddsi_topic_descriptor_fini(eflt->desc);
  eflt->desc = ddsrt_malloc(sizeof(*eflt->desc));
  ret = ddsi_topic_descriptor_from_type (gv, eflt->desc, res_type);
  assert (ret == DDS_RETCODE_OK);


  ddsrt_mutex_lock (&dds_global.m_mutex);
  struct dds_domain *dom = dds_domain_find_locked(eflt->tf.domain_id);
  struct dds_sertype_default *st_def = ddsrt_malloc(sizeof(*st_def));
  uint16_t min_xcdrv = eflt->desc->m_flagset & DDS_DATA_REPRESENTATION_FLAG_XCDR1? DDSI_RTPS_CDR_ENC_VERSION_1: DDSI_RTPS_CDR_ENC_VERSION_2;
  dds_data_representation_id_t data_representation = ((struct dds_sertype_default *)st)->write_encoding_version == DDSI_RTPS_CDR_ENC_VERSION_1? DDS_DATA_REPRESENTATION_XCDR1: DDS_DATA_REPRESENTATION_XCDR2;
  ret = dds_sertype_default_init (dom, st_def, eflt->desc, min_xcdrv, data_representation);
  assert (ret == DDS_RETCODE_OK);
  ddsrt_mutex_unlock (&dds_global.m_mutex);

  eflt->st = (struct ddsi_sertype *)st_def;

  /* TODO: to be implemented */
  /* ddsi_type_unref (gv, res_type); */

err:
  return ret;
}

static dds_return_t topic_expr_filter_init(dds_domainid_t domain_id, const struct dds_content_filter *con_filter, const struct ddsi_sertype *st, struct dds_filter **filter)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_expression_content_filter *cflt = con_filter->filter.expr;
  struct dds_expression_filter *eflt = (struct dds_expression_filter *) ddsrt_malloc(sizeof(*eflt));
  eflt->tf.domain_id = domain_id;
  eflt->st = NULL;
  eflt->desc = NULL;
  eflt->expression = ddsrt_strdup(cflt->expression);
  eflt->expr = NULL;
  eflt->bin_expr = NULL;
  (void) dds_sql_expr_init(&eflt->expr, DDS_SQL_EXPR_KIND_PARAMETER);
  (void) dds_sql_expr_parse((const unsigned char *)eflt->expression, &eflt->expr);
  if ((cflt->nparam != 0 && cflt->param != NULL)
      || (cflt->nparam == 0))
  {
    if ((ret = topic_expr_filter_param_rebind((struct dds_filter *)eflt, con_filter, st)) != DDS_RETCODE_OK)
    {
      topic_expr_filter_fini((struct dds_filter *)eflt);
      goto err;
    }
  } else {
   /* FIXME:
    * potentially can be a error.
    * */
  }

  *filter = (struct dds_filter *)eflt;
err:
  return ret;
}

static bool topic_expr_filter_compare (const struct dds_filter *a, const struct dds_content_filter *b)
{
  bool res = true;
  assert (a->type == DDS_EXPR_FILTER);
  if (b->kind != DDS_CONTENT_FILTER_EXPRESSION) {
    res = false;
  } else {
    struct dds_expression_filter *ef = (struct dds_expression_filter *) a;
    if (strcmp(ef->expression, b->filter.expr->expression))
      res = false;
  }
  return res;
}

static struct dds_filter_ops dds_topic_expr_filter_ops = {
  .init = topic_expr_filter_init,
  .fini = topic_expr_filter_fini,
  .reader_accept = topic_expr_filter_reader_accept,
  .writer_accept = topic_expr_filter_writer_accept,
};

static bool topic_func_filter_reader_accept(const dds_reader *rd, const struct dds_filter *filter, const struct ddsi_serdata *sample, const struct dds_sample_info *si)
{
  bool res = true;
  const struct dds_function_filter *ff = (const struct dds_function_filter *)filter;
  switch (ff->mode)
  {
    case DDS_TOPIC_FILTER_SAMPLEINFO_ARG: {
      res = ff->f.sampleinfo_arg (si, ff->arg);
      break;
    }
    case DDS_TOPIC_FILTER_SAMPLE:
    case DDS_TOPIC_FILTER_SAMPLE_ARG:
    case DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG: {
      void *s;
      s = ddsi_sertype_alloc_sample (rd->m_topic->m_stype);
      res = ddsi_serdata_to_sample (sample, s, NULL, NULL);
      assert (res == true);
      switch (ff->mode)
      {
        case DDS_TOPIC_FILTER_SAMPLE: {
          res = ff->f.sample (s);
          break;
        }
        case DDS_TOPIC_FILTER_SAMPLE_ARG: {
          res = ff->f.sample_arg (s, ff->arg);
          break;
        }
        case DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG: {
          res = ff->f.sample_sampleinfo_arg (si, s, ff->arg);
          break;
        }
        default:
          abort();
      }
      ddsi_sertype_free_sample (rd->m_topic->m_stype, s, DDS_FREE_ALL);
      break;
    }
    default:
      abort();
  }

  return res;
}

static bool topic_func_filter_writer_accept(const dds_writer *wr, const struct dds_filter *filter, const void *sample)
{
  (void) wr;
  bool res = true;
  const struct dds_function_filter *ff = (const struct dds_function_filter *) filter;
  switch (ff->mode)
  {
    case DDS_TOPIC_FILTER_SAMPLEINFO_ARG:
      break;
    case DDS_TOPIC_FILTER_SAMPLE_SAMPLEINFO_ARG: {
      struct dds_sample_info si;
      (void) memset (&si, 0U, sizeof(si));
      res = ff->f.sample_sampleinfo_arg (&si, sample, ff->arg);
      break;
    }
    case DDS_TOPIC_FILTER_SAMPLE: {
      res = ff->f.sample (sample);
      break;
    }
    case DDS_TOPIC_FILTER_SAMPLE_ARG: {
      res = ff->f.sample_arg (sample, ff->arg);
      break;
    }
    default:
      abort();
  }

  return res;
}

static void topic_func_filter_fini(struct dds_filter *filter)
{
  struct dds_function_filter *ff = (struct dds_function_filter *)filter;
  ddsrt_free(ff);
}

static dds_return_t topic_func_filter_param_rebind(struct dds_filter *a, const struct dds_content_filter *b, const struct ddsi_sertype *st)
{
  /* FIXME:
   * unused in context of function filter, but could be. for example of
   * filtering on keys only, or in case of interest in specific filed of data.
   * which sound nice, because allows us to not deserialize whole sample if
   * user don't need one. */
  (void) st;
  assert (b->kind == DDS_CONTENT_FILTER_FUNCTION);
  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_function_filter *fflt = (struct dds_function_filter *) a;
  fflt->arg = b->filter.func->arg;
  return ret;
}

static dds_return_t topic_func_filter_init(dds_domainid_t domain_id, const struct dds_content_filter *con_filter, const struct ddsi_sertype *st, struct dds_filter **filter)
{
  dds_return_t ret = DDS_RETCODE_OK;
  dds_function_content_filter_t *cflt = con_filter->filter.func;
  struct dds_function_filter *fflt = (struct dds_function_filter *) ddsrt_malloc(sizeof(*fflt));
  fflt->tf.domain_id = domain_id;
  fflt->f = cflt->f;
  fflt->mode = cflt->mode;
  if ((ret = topic_func_filter_param_rebind((struct dds_filter *) fflt, con_filter, st)) != DDS_RETCODE_OK)
  {
    topic_func_filter_fini((struct dds_filter *)fflt);
    goto err;
  }
  *filter = (struct dds_filter *) fflt;
err:
  return ret;
}

static bool topic_func_filter_compare(const struct dds_filter *a, const struct dds_content_filter *b)
{
  bool res = true;
  assert (a->type == DDS_FUNC_FILTER);
  if (b->kind != DDS_CONTENT_FILTER_FUNCTION) {
    res = false;
  } else {
    struct dds_function_filter *ff = (struct dds_function_filter *) a;
    if (ff->mode != b->filter.func->mode)
      res = false;
    else if (memcmp(&ff->f, &b->filter.func->f, sizeof(ff->f)))
      res = false;
  }
  return res;
}

static struct dds_filter_ops dds_topic_func_filter_ops = {
  .init = topic_func_filter_init,
  .fini = topic_func_filter_fini,
  .reader_accept = topic_func_filter_reader_accept,
  .writer_accept = topic_func_filter_writer_accept,
};

dds_return_t dds_filter_init(dds_domainid_t domain_id, const struct dds_content_filter *filter, const struct ddsi_sertype *st, struct dds_filter **out)
{
  assert (filter);
  struct dds_filter_ops flt_ops;
  enum dds_filter_type flt_type;
  switch (filter->kind)
  {
    case DDS_CONTENT_FILTER_EXPRESSION:
    {
      flt_type = DDS_EXPR_FILTER;
      flt_ops = dds_topic_expr_filter_ops;
      break;
    }
    case DDS_CONTENT_FILTER_FUNCTION:
    {
      flt_type = DDS_FUNC_FILTER;
      flt_ops = dds_topic_func_filter_ops;
      break;
    }
    default:
      abort();
  }

  dds_return_t ret = flt_ops.init(domain_id, filter, st, out);
  (*out)->type = flt_type;
  (*out)->ops = flt_ops;
  return ret;
}

void dds_filter_fini( struct dds_filter *filter)
{
  if (filter)
    filter->ops.fini(filter);
}

static bool dds_filter_compare(const struct dds_filter *a, const struct dds_content_filter *b)
{
  assert (a != NULL && b != NULL);
  bool res = false;
  switch (a->type)
  {
    case DDS_EXPR_FILTER:
      res = topic_expr_filter_compare (a, b);
      break;
    case DDS_FUNC_FILTER:
      res = topic_func_filter_compare (a, b);
      break;
    default:
      abort();
  }
  return res;
}

static dds_return_t dds_filter_param_rebind(struct dds_filter *a, const struct dds_content_filter *b, const struct ddsi_sertype *st)
{
  assert (a != NULL && b != NULL);
  dds_return_t res = DDS_RETCODE_BAD_PARAMETER;
  switch (a->type)
  {
    case DDS_EXPR_FILTER:
      res = topic_expr_filter_param_rebind (a, b, st);
      break;
    case DDS_FUNC_FILTER:
      res = topic_func_filter_param_rebind (a, b, st);
      break;
    default:
      abort();
  }
  return res;
}

dds_return_t dds_filter_update(const struct dds_content_filter *con_filter, const struct ddsi_sertype *st, struct dds_filter **out)
{
  dds_return_t ret = DDS_RETCODE_OK;

  if (out == NULL || *out == NULL)
    return DDS_RETCODE_BAD_PARAMETER;

  struct dds_filter *flt = *out;

  /* why waste time on potentially heavy init process, just rebind parameters
   * and go on to filter apply phase. */
  if (dds_filter_compare(flt, con_filter)) {
    if ((ret = dds_filter_param_rebind(flt, con_filter, st)) != DDS_RETCODE_OK)
      goto err;
  } else {
    dds_domainid_t domain_id = flt->domain_id;
    dds_filter_fini(flt);
    if ((ret = dds_filter_init(domain_id, con_filter, st, &flt)) != DDS_RETCODE_OK)
      goto err;
  }

  *out = flt;
err:
  return ret;
}

bool dds_filter_reader_accept (const struct dds_filter *filter, const struct dds_reader *rd, const struct ddsi_serdata *sd, const struct dds_sample_info *si)
{
  return filter == NULL || filter->ops.reader_accept (rd, filter, sd, si);
}

bool dds_filter_writer_accept(const struct dds_filter *filter, const struct dds_writer *wr, const void *sample)
{
  return filter == NULL || filter->ops.writer_accept (wr, filter, sample);
}
