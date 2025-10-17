// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Test.h"

#include "dds/dds.h"
#include "SerdataData.h"

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds__filter.h"
#include "dds__types.h"
#include "dds__sql_expr.h"

#include "test_util.h"

/* COMMON FILTER
 * */
/* EXPRESSION FILTER
 * */

static int get__token(const unsigned char **s, int *token)
{
  assert (token != NULL);
  *token = -1;
  const unsigned char *cur = *s;
  int i = 0;
  do {
    if (*cur == '\0')
      goto exit;
    cur += (i=dds_sql_get_token(cur, token));
  } while (*token == DDS_SQL_TK_SPACE);

exit:
  *s = cur;
  return i;
}

CU_Test(ddsc_expr_filter, init_fini)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct {
    char *expr;                   // expression to be used as a filter
    char *param;                  // parameters sep. by ' ' to be applied in
                                  // order
    dds_topic_descriptor_t tdesc; // base topic descriptor to be used
    struct {
      char *fields;               // keyed field on a result type
    } expec;
  } test[] = {
    { .expr = "a + b + c",        .param = "",    .tdesc = SerdataKeyOrder_desc,                    .expec = { .fields = "`a` `b` `c`" }},
    { .expr = "a + b OR ?1 * c",  .param = "0",   .tdesc = SerdataKeyOrderId_desc,                  .expec = { .fields = "`a` `b`" }},
    { .expr = "x AND y OR z.b",   .param = "",    .tdesc = SerdataKeyOrderFinalNestedMutable_desc,  .expec = { .fields = "`x` `y` `z.b`" }}
  };

  size_t ntest = sizeof(test) / sizeof(test[0]);
  dds_domainid_t domain_id = 0;
  for (size_t i = 0; i < ntest; i++)
  {
    dds_entity_t domain = dds_create_domain (domain_id, NULL);
    dds_entity_t participant = dds_create_participant (domain_id, NULL, NULL);

    struct dds_content_filter con_flt = { .kind = DDS_CONTENT_FILTER_EXPRESSION };
    ret = dds_expression_filter_create(test[i].expr, &con_flt.filter.expr);
    CU_ASSERT (ret == DDS_RETCODE_OK);

    /* parameters bind */
    int token = 0;
    int token_sz = 0;
    const unsigned char *cursor = (const unsigned char *) test[i].param;
    uint32_t id = 1;
    do {
      if ((token_sz = get__token(&cursor, &token)) > 0 && token > 0) {
        const unsigned char *param = cursor - token_sz;
        switch (token)
        {
          case DDS_SQL_TK_INTEGER:
          case DDS_SQL_TK_FLOAT:
          {
            union {int64_t i; double r;} b;
            ret = dds_sql_get_numeric((void **)&b, &param, &token, (int)token_sz);
            CU_ASSERT(ret == 0);
            if (token == DDS_SQL_TK_INTEGER)
              ret = dds_expression_filter_bind_integer(con_flt.filter.expr, id, b.i);
            else if (token == DDS_SQL_TK_FLOAT)
              ret = dds_expression_filter_bind_real(con_flt.filter.expr, id, b.r);
            break;
          }
          case DDS_SQL_TK_STRING:
          case DDS_SQL_TK_BLOB:
          {
            char *b = NULL;
            ret = dds_sql_get_string((void **)&b, &param, &token, (int)token_sz);
            CU_ASSERT(ret != 0);
            if (token == DDS_SQL_TK_STRING)
              ret = dds_expression_filter_bind_string(con_flt.filter.expr, id, b);
            else if (token == DDS_SQL_TK_BLOB)
              ret = dds_expression_filter_bind_blob(con_flt.filter.expr, id, (unsigned char *)b, (uint32_t)ret);
            free (b);
            break;
          }
          default:
            CU_ASSERT(false);
        }
        CU_ASSERT (ret == DDS_RETCODE_OK);
        id++;
      }
    } while (token > 0);

    char topic_name[100];
    create_unique_topic_name ("ddsc_expr_filter", topic_name, sizeof(topic_name));
    dds_entity_t tp = dds_create_topic (participant, &test[i].tdesc, topic_name, NULL, NULL);
    const struct ddsi_sertype *sertype;
    ret = dds_get_entity_sertype (tp, &sertype);
    CU_ASSERT (ret == DDS_RETCODE_OK);

    struct dds_filter *flt = NULL;
    ret = dds_filter_create(domain_id, &con_flt, sertype, &flt);
    CU_ASSERT (ret == DDS_RETCODE_OK);

    dds_expression_filter_free(con_flt.filter.expr);

    /* validate that result descriptor created correctly */
    const dds_topic_descriptor_t *new_desc = ((struct dds_expression_filter *)flt)->desc;
    CU_ASSERT (((struct dds_expression_filter *)flt)->bin_expr->nparams == new_desc->m_nkeys);

    token = 0;
    cursor = (const unsigned char *) test[i].expec.fields;
    do {
      if ((token_sz = get__token(&cursor, &token)) > 0 && token > 0) {
        char *field = ddsrt_strndup((const char *)(cursor - token_sz + 1U), (size_t)token_sz - 2U);

        bool found = false;
        for (size_t j = 0; j < new_desc->m_nkeys && !(found = !strcmp(field, new_desc->m_keys[j].m_name)); j++) {}
        CU_ASSERT (found);
        ddsrt_free (field);
      }
    } while (token > 0);

    dds_filter_free (flt);
    dds_delete (domain);
  }
}
/* FUNC FILTER
 * */
