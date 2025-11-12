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
#include "Space.h"

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds__serdata_default.h"
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

struct expr_filter_test_base {
  const char *expr;         // expression to be used as a filter.
  const char *param;        // parameters separated by ' ' in order.
  const dds_topic_descriptor_t tdesc; // target topic descriptor.
};

static dds_return_t init_dds_expression_filter (const dds_entity_t pp, const struct expr_filter_test_base *base, struct dds_filter **flt)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_content_filter con_flt = { .kind = DDS_CONTENT_FILTER_EXPRESSION };
  ret = dds_expression_filter_create(base->expr, &con_flt.filter.expr);
  CU_ASSERT (ret == DDS_RETCODE_OK);

  /* parameters bind */
  int token = 0;
  int token_sz = 0;
  const unsigned char *cursor = (const unsigned char *) base->param;
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
  dds_entity_t tp = dds_create_topic (pp, &base->tdesc, topic_name, NULL, NULL);
  const struct ddsi_sertype *sertype;
  ret = dds_get_entity_sertype (tp, &sertype);
  CU_ASSERT (ret == DDS_RETCODE_OK);

  dds_domainid_t domain_id;
  ret = dds_get_domainid(pp, &domain_id);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);
  ret = dds_filter_create(domain_id, &con_flt, sertype, flt);
  CU_ASSERT_FATAL (ret == DDS_RETCODE_OK);

  dds_expression_filter_free(con_flt.filter.expr);

  return ret;
}

CU_Test(ddsc_expr_filter, accept)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct {
    struct expr_filter_test_base b;
    const void *sample;
    const bool result;
  } test[] = {
  {{ .expr="`e1`=0",               .param="",    .tdesc=Space_invalid_data_desc                 }, .sample=&(struct Space_invalid_data){.e1= Space_IDE0},                         .result = true },
  {{ .expr="`bm1`=(1 << 0)",       .param="",    .tdesc=Space_invalid_data_desc                 }, .sample=&(struct Space_invalid_data){.bm1=Space_IDB0},                         .result = true },
  {{ .expr="b == \'abc\'",         .param="",    .tdesc=SerdataKeyStringBounded_desc            }, .sample=&(struct SerdataKeyStringBounded){.a=1U,.b="abc"},                     .result = true  },
  {{ .expr="b == \'abcd\'",        .param="",    .tdesc=SerdataKeyString_desc                   }, .sample=&(struct SerdataKeyString){.a=1U,.b="abcd"},                           .result = true  },
/* FIXME: unsupported case because of "union" keys limitation.
 * {{ .expr="`um_sub1.m_int32`=0",  .param="",    .tdesc=dunion_desc                             }, .sample=&(struct dunion){._d=16,._u.um_sub1.m_int32=0},                        .result = false }, */
#ifdef DDS_HAS_TYPELIB
  {{ .expr="e.z.a = d.z.a",        .param="",    .tdesc=SerdataKeyNestedFinalImplicit_desc      }, .sample=&(struct SerdataKeyNestedFinalImplicit){.e.z.a=1U,.d.z.a=0U},          .result = false },
  {{ .expr="b = by OR by = bx.ny", .param="",    .tdesc=SerdataKeyInheritMutable_desc           }, .sample=&(struct SerdataKeyInheritMutable){.b=1U,.parent={.by=0U,.bx.ny=0U}},  .result = true  },
  {{ .expr="a + b",                .param="",    .tdesc=SerdataKeyOrder_desc                    }, .sample=&(struct SerdataKeyOrder){.a=1U,.b=1U,.c=0U},                          .result = true  },
  {{ .expr="a + b OR ?1 * c",      .param="0",   .tdesc=SerdataKeyOrderId_desc                  }, .sample=&(struct SerdataKeyOrderId){.a=1U,.b=0U,.c=0U},                        .result = true  },
  {{ .expr="x AND y OR z.b",       .param="",    .tdesc=SerdataKeyOrderFinalNestedMutable_desc  }, .sample=&(struct SerdataKeyOrderFinalNestedMutable){.x=0U,.y=0U,.z.b=1U},      .result = true  },
  {{ .expr="d.x AND d.z.c OR e.x", .param="",    .tdesc=SerdataKeyNestedFinalImplicit_desc      }, .sample=&(struct SerdataKeyNestedFinalImplicit){.d.x=1U,.d.z.c=0U,.e.x=0U},    .result = false },
#else
  {{ .expr="d.z.a = d.z.c",        .param="",    .tdesc=SerdataKeyNestedFinalImplicit_desc      }, .sample=&(struct SerdataKeyNestedFinalImplicit){.d.z.a=1U,.d.z.c=0U},              .result = false },
  {{ .expr="bx.nz=bz OR bz=bx.nx", .param="",    .tdesc=SerdataKeyInheritMutable_desc           }, .sample=&(struct SerdataKeyInheritMutable){.parent={.bz=1U,.bx.nz=1U,.bx.nx=1U}},  .result = true  },
  {{ .expr="a + c",                .param="",    .tdesc=SerdataKeyOrder_desc                    }, .sample=&(struct SerdataKeyOrder){.a=1U,.c=0U},                                    .result = true  },
  {{ .expr="a + c OR ?1 * c",      .param="0",   .tdesc=SerdataKeyOrderId_desc                  }, .sample=&(struct SerdataKeyOrderId){.a=1U,.c=0U},                                  .result = true  },
  {{ .expr="x AND z.c OR z.a",     .param="",    .tdesc=SerdataKeyOrderFinalNestedMutable_desc  }, .sample=&(struct SerdataKeyOrderFinalNestedMutable){.x=0U,.z.c=0U,.z.b=1U},        .result = false },
  {{ .expr="d.x AND d.z.c OR f",   .param="",    .tdesc=SerdataKeyNestedFinalImplicit_desc      }, .sample=&(struct SerdataKeyNestedFinalImplicit){.d.x=1U,.d.z.c=0U,.f=0U},          .result = false },
#endif
  };

  size_t ntest = sizeof(test) / sizeof(test[0]);
  dds_domainid_t domain_id = 0;
  for (size_t i = 0; i < ntest; i++)
  {
    dds_entity_t domain = dds_create_domain (domain_id, NULL);
    dds_entity_t participant = dds_create_participant (domain_id, NULL, NULL);

    struct dds_filter *flt = NULL;
    ret = init_dds_expression_filter (participant, &test[i].b, &flt);
    CU_ASSERT (ret == DDS_RETCODE_OK);

#ifdef DDS_HAS_TYPELIB
    /* validate that result descriptor created correctly */
    const struct dds_cdrstream_desc new_desc = ((struct dds_sertype_default *)(((struct dds_expression_filter *)flt)->st))->type;
    CU_ASSERT (((struct dds_expression_filter *)flt)->bin_expr->nparams == new_desc.keys.nkeys);
#endif

    bool res = dds_filter_writer_accept (flt, NULL, test[i].sample);
    CU_ASSERT (res == test[i].result);

    dds_filter_free (flt);
    dds_delete (domain);
  }
}

CU_Test(ddsc_expr_filter, init_fini)
{
  dds_return_t ret = DDS_RETCODE_OK;
  struct {
    struct expr_filter_test_base b;
    struct {
      char *fields;               // keyed field on a result type sep. by ' '
    } expec;
  } test[] = {
#ifdef DDS_HAS_TYPELIB
  {{ .expr="a + b + c",           .param="",    .tdesc=SerdataKeyOrder_desc                    },  .expec={"`a` `b` `c`" }   },
  {{ .expr="a + b OR ?1 * c",     .param="0",   .tdesc=SerdataKeyOrderId_desc                  },  .expec={"`a` `b`" }       },
  {{ .expr="x AND y OR z.b",      .param="",    .tdesc=SerdataKeyOrderFinalNestedMutable_desc  },  .expec={"`x` `y` `z.b`" } }
#else
  {{ .expr="a",                   .param="",    .tdesc=SerdataKeyOrder_desc                    },  .expec={"`a` `c`" }         },
  {{ .expr="a OR ?1 * c",         .param="0",   .tdesc=SerdataKeyOrderId_desc                  },  .expec={"`a` `c`" }         },
  {{ .expr="x OR z.a",            .param="",    .tdesc=SerdataKeyOrderFinalNestedMutable_desc  },  .expec={"`x` `z.a` `z.c`" } }
#endif
  };

  size_t ntest = sizeof(test) / sizeof(test[0]);
  dds_domainid_t domain_id = 0;
  for (size_t i = 0; i < ntest; i++)
  {
    dds_entity_t domain = dds_create_domain (domain_id, NULL);
    dds_entity_t participant = dds_create_participant (domain_id, NULL, NULL);

    struct dds_filter *flt = NULL;
    ret = init_dds_expression_filter (participant, &test[i].b, &flt);
    CU_ASSERT (ret == DDS_RETCODE_OK);

    /* validate that result descriptor created correctly */
    const struct dds_cdrstream_desc new_desc = ((struct dds_sertype_default *)(((struct dds_expression_filter *)flt)->st))->type;
#ifdef DDS_HAS_TYPELIB
    CU_ASSERT (((struct dds_expression_filter *)flt)->bin_expr->nparams == new_desc.keys.nkeys);
#endif

    int token = 0;
    int token_sz = 0;
    const unsigned char *cursor = (const unsigned char *) test[i].expec.fields;
    do {
      if ((token_sz = get__token(&cursor, &token)) > 0 && token > 0) {
        char *field = ddsrt_strndup((const char *)(cursor - token_sz + 1U), (size_t)token_sz - 2U);

        bool found = false;
        for (size_t j = 0; j < new_desc.keys.nkeys && !(found = !strcmp(field, new_desc.keys.keys[j].name)); j++) {}
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
