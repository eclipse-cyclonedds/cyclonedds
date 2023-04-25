// Copyright(c) 2021 ZettaScale Technology and others
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

#include "idl/processor.h"

#include "CUnit/Theory.h"

static idl_retcode_t parse_string(const char *str, idl_pstate_t **pstatep)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  if ((ret = idl_create_pstate(IDL_FLAG_EXTENDED_DATA_TYPES, NULL, &pstate)))
    return ret;
  if ((ret = idl_parse_string(pstate, str)))
    idl_delete_pstate(pstate);
  else
    *pstatep = pstate;
  return ret;
}

CU_Test(idl_forward, struct_union_maybe_enum)
{
  static const struct {
    idl_retcode_t retcode;
    uint32_t type;
    const char *idl;
  } tests[] = {
    { IDL_RETCODE_OK, IDL_STRUCT, "struct a; struct a { long b; };" },
    { IDL_RETCODE_OK, IDL_STRUCT, "struct a; struct a; struct a { long b; };" },
    { IDL_RETCODE_OK, IDL_UNION, "union a; union a switch (short) { case 1: long b; };" },
    { IDL_RETCODE_OK, IDL_UNION, "union a; union a; union a switch (short) { case 1: long b; };" },
    { IDL_RETCODE_SEMANTIC_ERROR, 0u, "struct a; union a; struct a { long b; };" },
    { IDL_RETCODE_SEMANTIC_ERROR, 0u, "union a; struct a; struct a { long b; };" },
    { IDL_RETCODE_SEMANTIC_ERROR, 0u, "union a; struct a; union a switch (short) { case 1: long b; };" },
    { IDL_RETCODE_SYNTAX_ERROR, 0u, "enum a; enum a { b, c };" }
  };

  for (size_t i = 0; i < sizeof (tests) / sizeof (tests[0]); i++) {
    idl_retcode_t ret;
    idl_pstate_t *pstate = NULL;
    printf("test idl: %s\n", tests[i].idl);
    ret = parse_string(tests[i].idl, &pstate);
    CU_ASSERT_EQUAL_FATAL(ret, tests[i].retcode);
    if (ret == IDL_RETCODE_OK) {
      const idl_forward_t *forward = NULL;
      const idl_type_spec_t *node;
      CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
      CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
      assert(pstate);
      for (node = pstate->root; idl_is_forward(node); node = idl_next(node))
        forward = node;
      CU_ASSERT_FATAL(idl_is_forward(forward));
      assert(forward);
      CU_ASSERT_EQUAL(idl_type(node), tests[i].type);
      CU_ASSERT_PTR_EQUAL(forward->type_spec, node);
    }
    idl_delete_pstate(pstate);
  }
}

CU_Test(idl_forward, aliases)
{
  static const struct {
    size_t forwards;
    const char *idl;
    idl_retcode_t retcode;
    idl_type_t type;
  } tests[] = {
    { 1, "struct a; typedef a b; struct a { long b; };", IDL_RETCODE_OK, IDL_STRUCT },
    { 2, "struct a; struct a; typedef a b; struct a { long b; };", IDL_RETCODE_OK, IDL_STRUCT },
    { 1, "union a; typedef a b; union a switch(short) { case 1: long b; };", IDL_RETCODE_OK, IDL_UNION },
    { 2, "union a; union a; typedef a b; union a switch(short) { case 1: long b; };", IDL_RETCODE_OK, IDL_UNION }
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    idl_retcode_t ret;
    idl_pstate_t *pstate = NULL;
    printf("test idl: %s\n", tests[i].idl);
    ret = parse_string(tests[i].idl, &pstate);
    CU_ASSERT_EQUAL(ret, tests[i].retcode);
    if (ret == IDL_RETCODE_OK) {
      idl_forward_t *forward[2];
      idl_typedef_t *alias;
      idl_type_spec_t *type_spec;
      size_t n = tests[i].forwards - 1;
      for (size_t j = 0; j < tests[i].forwards; j++) {
        forward[j] = j ? idl_next(forward[j - 1]) : (idl_forward_t *)pstate->root;
        CU_ASSERT_FATAL(idl_is_forward(forward[j]));
      }
      alias = idl_next(forward[n]);
      CU_ASSERT_FATAL(idl_is_typedef(alias));
      type_spec = idl_next(alias);
      CU_ASSERT_EQUAL_FATAL(idl_type(type_spec), tests[i].type);
      CU_ASSERT_PTR_EQUAL(forward[n]->type_spec, type_spec);
      CU_ASSERT_PTR_EQUAL(alias->type_spec, forward[n]);
    }
    idl_delete_pstate(pstate);
  }
}

CU_Test(idl_forward, backwards_aliases)
{
  static const struct {
    size_t forwards;
    const char *idl;
    idl_retcode_t retcode;
    idl_type_t type;
  } tests[] = {
    { 1, "struct a { long b; }; struct a; typedef a b;", IDL_RETCODE_OK, IDL_STRUCT },
    { 2, "struct a { long b; }; struct a; struct a; typedef a b;", IDL_RETCODE_OK, IDL_STRUCT },
    { 1, "union a switch(short) { case 1: long b; }; union a; typedef a b;", IDL_RETCODE_OK, IDL_UNION },
    { 2, "union a switch(short) { case 1: long b; }; union a; union a; typedef a b;", IDL_RETCODE_OK, IDL_UNION }
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    idl_retcode_t ret;
    idl_pstate_t *pstate = NULL;
    printf("test idl: %s\n", tests[i].idl);
    ret = parse_string(tests[i].idl, &pstate);
    CU_ASSERT_EQUAL(ret, tests[i].retcode);
    if (ret == IDL_RETCODE_OK) {
      idl_forward_t *forward[2];
      idl_typedef_t *alias;
      idl_type_spec_t *type_spec;
      size_t n = tests[i].forwards - 1;
      type_spec = (idl_type_spec_t *)pstate->root;
      CU_ASSERT_EQUAL_FATAL(idl_type(type_spec), tests[i].type);
      for (size_t j = 0; j < tests[i].forwards; j++) {
        forward[j] = j ? idl_next(forward[j - 1]) : idl_next(type_spec);
        CU_ASSERT_FATAL(idl_is_forward(forward[j]));
      }
      alias = idl_next(forward[n]);
      CU_ASSERT_FATAL(idl_is_typedef(alias));
      CU_ASSERT_PTR_EQUAL(forward[n]->type_spec, type_spec);
      CU_ASSERT_PTR_EQUAL(alias->type_spec, type_spec);
    }
    idl_delete_pstate(pstate);
  }
}

CU_Test(idl_forward, incomplete_type)
{
  static const struct {
    const char *idl;
    idl_retcode_t retcode;
  } tests[] = {
    { "struct a; struct b { a f1; }; struct a { long a1; };", IDL_RETCODE_SEMANTIC_ERROR },
    { "struct a; typedef a c; struct b { c f1; }; struct a { long a1; };", IDL_RETCODE_SEMANTIC_ERROR },
    { "struct a; struct b { @external a f1; }; struct a { long a1; };", IDL_RETCODE_OK },
    { "struct a; typedef a c; struct b { @external c f1; }; struct a { long a1; };", IDL_RETCODE_OK },
    { "struct a; struct b { sequence<a> f1; }; struct a { long a1; };", IDL_RETCODE_OK },
    { "struct a; struct b { a f1[2]; }; struct a { long a1; };", IDL_RETCODE_SEMANTIC_ERROR },
    { "struct a; typedef a b[2]; struct a { long a1; };", IDL_RETCODE_SEMANTIC_ERROR },
    { "union a; struct b { a f1; }; union a switch(long) { case 1: long a1; };", IDL_RETCODE_SEMANTIC_ERROR },
    { "union a; struct b { @external a f1; }; union a switch(long) { case 1: long a1; };", IDL_RETCODE_OK },
    { "struct a; union b switch(long) { case 1: a f1; }; struct a { long a1; };", IDL_RETCODE_SEMANTIC_ERROR },
    { "struct a; union b switch(long) { case 1: @external a f1; }; struct a { long a1; };", IDL_RETCODE_OK },
  };

  for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
    idl_pstate_t *pstate = NULL;
    idl_retcode_t ret = idl_create_pstate(IDL_FLAG_ANNOTATIONS, NULL, &pstate);
    CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
    CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
    if (!pstate)
      return;
    pstate->config.default_extensibility = IDL_FINAL;
    ret = idl_parse_string(pstate, tests[i].idl);
    CU_ASSERT_EQUAL(ret, tests[i].retcode);
    printf("test idl: %s - %s\n", tests[i].idl, ret == tests[i].retcode ? "OK" : "FAIL");
    idl_delete_pstate(pstate);
  }
}


// x. provide definition in reopened module
