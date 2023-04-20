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
#include <stdlib.h>
#include <string.h>

#include "idl/processor.h"

#include "CUnit/Test.h"

CU_Test(idl_enum, no_enumerator)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  const char str[] = "enum foo { };";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(pstate);
  assert(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_SYNTAX_ERROR);
  CU_ASSERT_PTR_NULL(pstate->root);
  idl_delete_pstate(pstate);
}

CU_Test(idl_enum, duplicate_enumerators)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  const char str[] = "enum foo { bar, bar };";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(pstate);
  assert(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_SEMANTIC_ERROR);
  CU_ASSERT_PTR_NULL(pstate->root);
  idl_delete_pstate(pstate);
}

CU_Test(idl_enum, enumerator_matches_enum)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;

  const char str[] = "enum foo { foo };";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL(pstate);
  assert(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_SEMANTIC_ERROR);
  CU_ASSERT_PTR_NULL(pstate->root);
  idl_delete_pstate(pstate);
}

#if 0
CU _ Test(idl_enum, enumerator_used_name)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;

  const char str[] = "struct s { char c; }; enum e { e1 };";
  ret = idl_parse_string(str, 0u, &tree);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_SEMANTIC_ERROR);
  CU_ASSERT_PTR_NULL(tree);
  idl_delete_tree(tree);
}
#endif

CU_Test(idl_enum, single_enumerator)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_enum_t *e;
  idl_enumerator_t *er;

  const char str[] = "enum foo { bar };";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
  e = (idl_enum_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_enum(e));
  er = (idl_enumerator_t *)e->enumerators;
  CU_ASSERT_FATAL(idl_is_enumerator(er));
  CU_ASSERT_PTR_NOT_NULL_FATAL(idl_identifier(er));
  CU_ASSERT_STRING_EQUAL(idl_identifier(er), "bar");
  CU_ASSERT_PTR_EQUAL(idl_parent(er), e);
  idl_delete_pstate(pstate);
}

CU_Test(idl_enum, multiple_enumerators)
{
  idl_retcode_t ret;
  idl_pstate_t *pstate = NULL;
  idl_enum_t *e;
  idl_enumerator_t *er;

  const char str[] = "enum foo { bar, baz };";
  ret = idl_create_pstate(0u, NULL, &pstate);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(pstate);
  assert(pstate);
  ret = idl_parse_string(pstate, str);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
  e = (idl_enum_t *)pstate->root;
  CU_ASSERT_FATAL(idl_is_enum(e));
  er = (idl_enumerator_t *)e->enumerators;
  CU_ASSERT_FATAL(idl_is_enumerator(er));
  CU_ASSERT_PTR_NOT_NULL_FATAL(idl_identifier(er));
  CU_ASSERT_STRING_EQUAL(idl_identifier(er), "bar");
  CU_ASSERT_PTR_EQUAL(idl_parent(er), e);
  er = idl_next(er);
  CU_ASSERT_FATAL(idl_is_enumerator(er));
  CU_ASSERT_PTR_NOT_NULL_FATAL(idl_identifier(er));
  CU_ASSERT_STRING_EQUAL(idl_identifier(er), "baz");
  CU_ASSERT_PTR_EQUAL(idl_parent(er), e);
  idl_delete_pstate(pstate);
}
