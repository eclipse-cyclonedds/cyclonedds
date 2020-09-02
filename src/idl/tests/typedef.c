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
#include <stdlib.h>
#include <stdbool.h>

#include "idl/processor.h"

#include "CUnit/Test.h"

CU_Test(idl_typedef, bogus_type)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;

  const char str[] = "typedef foo bar;";
  ret = idl_parse_string(str, 0u, &tree);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_SEMANTIC_ERROR);
  CU_ASSERT_PTR_NULL(tree);
  idl_delete_tree(tree);
}

CU_Test(idl_typedef, simple_declarator)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_typedef_t *t;
  idl_declarator_t *d;

  const char str[] = "typedef char foo;";
  ret = idl_parse_string(str, 0u, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(tree);
  t = (idl_typedef_t *)tree->root;
  CU_ASSERT_PTR_NOT_NULL_FATAL(t);
  CU_ASSERT_FATAL(idl_is_typedef(t));
  CU_ASSERT_PTR_NULL(idl_previous(t));
  CU_ASSERT_PTR_NULL(idl_next(t));
  CU_ASSERT_PTR_NULL(idl_parent(t));
  CU_ASSERT_PTR_NOT_NULL(t->type_spec);
  CU_ASSERT(idl_is_type_spec(t->type_spec, IDL_CHAR));
  d = t->declarators;
  CU_ASSERT_PTR_NOT_NULL_FATAL(d);
  CU_ASSERT_FATAL(idl_is_declarator(d));
  CU_ASSERT_PTR_NULL(idl_previous(d));
  CU_ASSERT_PTR_NULL(idl_next(d));
  CU_ASSERT_PTR_EQUAL(idl_parent(d), t);
  CU_ASSERT_STRING_EQUAL(d->identifier, "foo");
  CU_ASSERT_PTR_NULL(d->const_expr);
  idl_delete_tree(tree);
}

CU_Test(idl_typedef, simple_declarators)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_typedef_t *t;
  idl_declarator_t *d;

  const char str[] = "typedef char foo, bar, baz;";
  ret = idl_parse_string(str, 0u, &tree);
  CU_ASSERT_EQUAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(tree);
  t = (idl_typedef_t *)tree->root;
  CU_ASSERT_PTR_NOT_NULL_FATAL(t);
  CU_ASSERT_FATAL(idl_is_typedef(t));
  CU_ASSERT_PTR_NOT_NULL(t->type_spec);
  CU_ASSERT(idl_is_type_spec(t->type_spec, IDL_CHAR));
  d = t->declarators;
  CU_ASSERT_PTR_NOT_NULL_FATAL(d);
  CU_ASSERT_FATAL(idl_is_declarator(d));
  CU_ASSERT_PTR_NULL(idl_previous(d));
  CU_ASSERT_PTR_EQUAL(idl_parent(d), t);
  CU_ASSERT_PTR_NOT_NULL_FATAL(d->identifier);
  CU_ASSERT_STRING_EQUAL(d->identifier, "foo");
  CU_ASSERT_PTR_NULL(d->const_expr);
  d = idl_next(d);
  CU_ASSERT_PTR_NOT_NULL_FATAL(d);
  CU_ASSERT_FATAL(idl_is_declarator(d));
  CU_ASSERT_PTR_EQUAL(idl_parent(d), t);
  CU_ASSERT_PTR_NOT_NULL_FATAL(d->identifier);
  CU_ASSERT_STRING_EQUAL(d->identifier, "bar");
  CU_ASSERT_PTR_NULL(d->const_expr);
  d = idl_next(d);
  CU_ASSERT_PTR_NOT_NULL_FATAL(d);
  CU_ASSERT_FATAL(idl_is_declarator(d));
  CU_ASSERT_PTR_EQUAL(idl_parent(d), t);
  CU_ASSERT_PTR_NOT_NULL_FATAL(d->identifier);
  CU_ASSERT_STRING_EQUAL(d->identifier, "baz");
  CU_ASSERT_PTR_NULL(d->const_expr);
  CU_ASSERT_PTR_NULL(idl_next(d));
  idl_delete_tree(tree);
}

// x. typedef with complex declarator
// x. typedef with more than one complex declarator
// x. typedef to typedef
