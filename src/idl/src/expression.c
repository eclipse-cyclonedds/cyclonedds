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
#include <stdint.h>

#include "idl/processor.h"
#include "idl/tree.h"
#include "table.h"
#include "expression.h"

static idl_retcode_t
eval_int_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_const_expr_t *expr,
  idl_mask_t type);

static idl_retcode_t
eval_int_or_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_binary_expr_t *expr,
  idl_mask_t type)
{
  idl_retcode_t ret;
  idl_intval_t lrval, rrval;
  uint64_t int_max;

  assert(type == IDL_LONG || type == IDL_LLONG);
  if ((ret = eval_int_expr(proc, &lrval, expr->left, type)) != 0)
    return ret;
  if ((ret = eval_int_expr(proc, &rrval, expr->right, type)) != 0)
    return ret;

  int_max = type == IDL_LLONG ? INT64_MAX : INT32_MAX;
  switch ((lrval.negative ? 1 : 0) + (rrval.negative ? 2 : 0)) {
    case 0:
      *val = lrval;
      val->value.ullng |= rrval.value.ullng;
      break;
    case 1:
      assert(lrval.value.llng < 0);
      if (rrval.value.ullng > int_max) {
        *val = rrval;
        val->value.ullng |= lrval.value.ullng;
      } else {
        *val = lrval;
        val->value.llng |= rrval.value.llng;
      }
      break;
    case 2:
      assert(rrval.value.llng < 0);
      if (lrval.value.ullng > int_max) {
        *val = lrval;
        val->value.ullng |= rrval.value.ullng;
      } else {
        *val = rrval;
        val->value.llng |= lrval.value.llng;
      }
      break;
    case 3:
      assert(lrval.value.llng < 0);
      assert(rrval.value.llng < 0);
      *val = lrval;
      val->value.llng |= rrval.value.llng;
      break;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_int_xor_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_binary_expr_t *expr,
  idl_mask_t type)
{
  idl_retcode_t ret;
  idl_intval_t lrval, rrval;
  uint64_t int_max;

  assert(type == IDL_LONG || type == IDL_LLONG);
  if ((ret = eval_int_expr(proc, &lrval, expr->left, type)) != 0)
    return ret;
  if ((ret = eval_int_expr(proc, &rrval, expr->right, type)) != 0)
    return ret;

  int_max = type == IDL_LLONG ? INT64_MAX : INT32_MAX;
  switch ((lrval.negative ? 1 : 0) + (rrval.negative ? 2 : 0)) {
    case 0:
      *val = lrval;
      val->value.ullng |= rrval.value.ullng;
      break;
    case 1:
      assert(lrval.value.llng < 0);
      if (rrval.value.ullng > int_max) {
        *val = rrval;
        val->value.ullng |= lrval.value.ullng;
      } else {
        *val = lrval;
        val->value.llng |= rrval.value.llng;
      }
      break;
    case 2:
      assert(rrval.value.llng < 0);
      if (lrval.value.ullng > int_max) {
        *val = lrval;
        val->value.ullng |= rrval.value.ullng;
      } else {
        *val = rrval;
        val->value.llng |= lrval.value.llng;
      }
      break;
    case 3:
      assert(lrval.value.llng < 0);
      assert(rrval.value.llng < 0);
      *val = lrval;
      val->value.llng |= rrval.value.llng;
      break;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_int_minus_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_unary_expr_t *expr,
  idl_mask_t type)
{
  idl_retcode_t ret;
  idl_intval_t rval;

  assert(type == IDL_LONG || type == IDL_LLONG);
  if ((ret = eval_int_expr(proc, &rval, expr->right, type)) != IDL_RETCODE_OK)
    return ret;

  if (rval.negative) {
    assert(rval.value.llng < 0);
    val->negative = false;
    val->value.ullng = (uint64_t)-rval.value.llng;
  } else {
    const char *label;
    uint64_t int_max;

    if (type == IDL_LLONG) {
      int_max = INT64_MAX;
      label = "long long";
    } else {
      int_max = INT32_MAX;
      label = "long";
    }

    if (rval.value.ullng > int_max) {
      idl_error(proc, idl_location(expr),
        "value exceeds maximum for %s", label);
      return IDL_RETCODE_OUT_OF_RANGE;
    }

    val->negative = true;
    val->value.llng = -(int64_t)rval.value.ullng;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_int_plus_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_unary_expr_t *expr,
  idl_mask_t type)
{
  assert(type == IDL_LONG || type == IDL_LLONG);
  return eval_int_expr(proc, val, expr->right, type);
}

static idl_retcode_t
eval_int_not_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_unary_expr_t *expr,
  idl_mask_t type)
{
  idl_retcode_t ret;
  idl_intval_t rval;
  uint64_t uint_max;

  assert(type == IDL_LONG || type == IDL_LLONG);
  if ((ret = eval_int_expr(proc, &rval, expr->right, type)) != IDL_RETCODE_OK)
    return ret;

  if (rval.negative) {
    assert(rval.value.llng < 0);
    val->negative = false;
    val->value.llng = ~rval.value.llng;
  } else {
    uint_max = type == IDL_LLONG ? UINT64_MAX : UINT32_MAX;
    val->value.ullng = uint_max & ~rval.value.ullng;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_int_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_const_expr_t *expr,
  idl_mask_t type)
{
  const idl_node_t *node = expr;
  char *label;
  uint64_t uint_max;

  assert(proc);
  assert(val);
  assert(expr);
  assert(type == IDL_LONG || type == IDL_LLONG);

  switch (node->mask & ~(IDL_EXPR|IDL_LITERAL)) {
    case IDL_OR_EXPR:
      return eval_int_or_expr(proc, val, (idl_binary_expr_t *)expr, type);
    case IDL_XOR_EXPR:
      return eval_int_xor_expr(proc, val, (idl_binary_expr_t *)expr, type);
    case IDL_MINUS_EXPR:
      return eval_int_minus_expr(proc, val, (idl_unary_expr_t *)expr, type);
    case IDL_PLUS_EXPR:
      return eval_int_plus_expr(proc, val, (idl_unary_expr_t *)expr, type);
    case IDL_NOT_EXPR:
      return eval_int_not_expr(proc, val, (idl_unary_expr_t *)expr, type);
    case IDL_ULONG:
    case IDL_ULLONG:
      if (type == IDL_LLONG) {
        uint_max = UINT64_MAX;
        label = "unsigned long long";
      } else {
        uint_max = UINT32_MAX;
        label = "unsigned long";
      }

      if (((idl_literal_t *)node)->value.ullng > uint_max) {
        idl_error(proc, idl_location(expr),
          "value too large for %s", label);
        return IDL_RETCODE_OUT_OF_RANGE;
      }

      val->negative = false;
      val->value.ullng = ((idl_literal_t *)expr)->value.ullng;
      break;
    default:
      idl_error(proc, idl_location(expr),
        "cannot evaluate %s as integer expression", idl_label(expr));
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_eval_int_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_const_expr_t *expr,
  idl_mask_t type)
{
  assert(proc);
  assert(val);
  assert(expr);
  if (type == IDL_ULONG)
    type = IDL_LONG;
  else if (type == IDL_ULLONG)
    type = IDL_LLONG;
  assert(type == IDL_LONG || type == IDL_LLONG);
  return eval_int_expr(proc, val, expr, type);
}
