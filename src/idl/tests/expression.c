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
#include <stdint.h>
#include <inttypes.h>

#include "idl/processor.h"
#include "idl/retcode.h"
#include "idl/tree.h"
#include "expression.h"

#include "CUnit/Test.h"

#define NODE(_kind) \
  (idl_node_t){ _kind, 0, 0, { {NULL, 0, 0}, {NULL, 0, 0} }, 0, 0 }

#define MINUS() \
  (idl_unary_expr_t){ NODE(IDL_MINUS_EXPR) }
#define PLUS() \
  (idl_unary_expr_t){ NODE(IDL_PLUS_EXPR) }
#define NOT() \
  (idl_unary_expr_t){ NODE(IDL_NOT_EXPR) }
#define UINT32(_uint) \
  (idl_literal_t){ NODE(IDL_INTEGER_LITERAL), .value = { .ullng = _uint } }

static void
test_unary_expr(
  idl_unary_expr_t *expr, idl_retcode_t ret, idl_intval_t *var)
{
  idl_retcode_t _ret;
  idl_intval_t _var;

  memset(&_var, 0, sizeof(_var));
  _ret = idl_eval_int_expr((idl_processor_t *)1, &_var, (idl_const_expr_t *)expr, IDL_LONG);
  CU_ASSERT_EQUAL_FATAL(_ret, ret);
  CU_ASSERT_EQUAL(_var.negative, var->negative);
  fprintf(stderr, "retcode: %d\n", _ret);
  fprintf(stderr, "negative: %s, value: ", _var.negative ? "true" : "false");
  if (_var.negative)
    fprintf(stderr, "%"PRId64"\n", _var.value.llng);
  else
    fprintf(stderr, "%"PRIu64"\n", _var.value.ullng);
}

CU_Test(idl_expression, unary_plus)
{
  idl_intval_t var = { .negative = false, .value = { .ullng = 1 } };
  idl_unary_expr_t expr = PLUS();
  idl_literal_t lit = UINT32(1);

  expr.right = (idl_const_expr_t *)&lit;
  test_unary_expr(&expr, IDL_RETCODE_OK, &var);
}

CU_Test(idl_expression, unary_minus)
{
  idl_intval_t var = { .negative = true, .value = { .llng = -1 } };
  idl_unary_expr_t expr = MINUS();
  idl_literal_t lit = UINT32(1);

  expr.right = (idl_const_expr_t *)&lit;
  test_unary_expr(&expr, IDL_RETCODE_OK, &var);
}

CU_Test(idl_expression, unary_minus_minus)
{
  idl_intval_t var = { .negative = false, .value = { .ullng = 1 } };
  idl_unary_expr_t expr1 = MINUS();
  idl_unary_expr_t expr2 = MINUS();
  idl_literal_t lit = UINT32(1);

  expr1.right = (idl_const_expr_t *)&expr2;
  expr2.right = (idl_const_expr_t *)&lit;
  test_unary_expr(&expr1, IDL_RETCODE_OK, &var);
}

CU_Test(idl_expression, unary_not)
{
  idl_intval_t var = { .negative = false, .value = { .ullng = UINT32_MAX - 1 } };
  idl_unary_expr_t expr = NOT();
  idl_literal_t lit = UINT32(1);

  expr.right = (idl_const_expr_t *)&lit;
  test_unary_expr(&expr, IDL_RETCODE_OK, &var);
}

CU_Test(idl_expression, unary_not_minus)
{
  idl_intval_t var = { .negative = false, .value = { .ullng = 0 } };
  idl_unary_expr_t expr1 = NOT();
  idl_unary_expr_t expr2 = MINUS();
  idl_literal_t lit = UINT32(1);

  expr1.right = (idl_const_expr_t *)&expr2;
  expr2.right = (idl_const_expr_t *)&lit;
  test_unary_expr(&expr1, IDL_RETCODE_OK, &var);
}

CU_Test(idl_expression, const_char)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_const_t *c;
  idl_constval_t *v;

  const char idl[] = "const char c = 'f';";

  ret = idl_parse_string(idl, 0u, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(tree);
  c = (idl_const_t *)tree->root;
  CU_ASSERT_FATAL(idl_is_const(c));
  v = (idl_constval_t *)c->const_expr;
  CU_ASSERT_FATAL(idl_is_masked(v, IDL_CONST|IDL_CHAR));
  CU_ASSERT_EQUAL(v->value.chr, 'f');
  idl_delete_tree(tree);
}

CU_Test(idl_expression, const_string)
{
  idl_retcode_t ret;
  idl_tree_t *tree = NULL;
  idl_const_t *c;
  idl_constval_t *v;

  const char idl[] = "const string<12> c = \"foo\" \"bar\";";

  ret = idl_parse_string(idl, 0u, &tree);
  CU_ASSERT_EQUAL_FATAL(ret, IDL_RETCODE_OK);
  CU_ASSERT_PTR_NOT_NULL_FATAL(tree);
  c = (idl_const_t *)tree->root;
  CU_ASSERT_FATAL(idl_is_const(c));
  v = (idl_constval_t *)c->const_expr;
  CU_ASSERT_FATAL(idl_is_masked(v, IDL_CONST|IDL_STRING));
  CU_ASSERT_PTR_NOT_NULL(v->value.str);
  CU_ASSERT_STRING_EQUAL(v->value.str, "foobar");
  idl_delete_tree(tree);
}
