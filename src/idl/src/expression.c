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
#include <stdlib.h>
#include <math.h>

#include "idl/processor.h"
#include "tree.h"
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
eval_int_and_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_binary_expr_t *expr,
  idl_mask_t type)
{ (void)proc; (void)val; (void)expr; (void)type; return IDL_RETCODE_NO_MEMORY; }

static idl_retcode_t
eval_int_lshift_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_binary_expr_t *expr,
  idl_mask_t type)
{ (void)proc; (void)val; (void)expr; (void)type; return IDL_RETCODE_NO_MEMORY; }

static idl_retcode_t
eval_int_rshift_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_binary_expr_t *expr,
  idl_mask_t type)
{ (void)proc; (void)val; (void)expr; (void)type; return IDL_RETCODE_NO_MEMORY; }

static idl_retcode_t
eval_int_add_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_binary_expr_t *expr,
  idl_mask_t type)
{ (void)proc; (void)val; (void)expr; (void)type; return IDL_RETCODE_NO_MEMORY; }

static idl_retcode_t
eval_int_sub_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_binary_expr_t *expr,
  idl_mask_t type)
{ (void)proc; (void)val; (void)expr; (void)type; return IDL_RETCODE_NO_MEMORY; }

static idl_retcode_t
eval_int_mult_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_binary_expr_t *expr,
  idl_mask_t type)
{ (void)proc; (void)val; (void)expr; (void)type; return IDL_RETCODE_NO_MEMORY; }

static idl_retcode_t
eval_int_div_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_binary_expr_t *expr,
  idl_mask_t type)
{ (void)proc; (void)val; (void)expr; (void)type; return IDL_RETCODE_NO_MEMORY; }

static idl_retcode_t
eval_int_mod_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_binary_expr_t *expr,
  idl_mask_t type)
{ (void)proc; (void)val; (void)expr; (void)type; return IDL_RETCODE_NO_MEMORY; }

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

idl_retcode_t
eval_int_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_const_expr_t *expr,
  idl_mask_t type)
{
  const idl_node_t *node = expr;
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
    case IDL_AND_EXPR:
      return eval_int_and_expr(proc, val, (idl_binary_expr_t *)expr, type);
    case IDL_LSHIFT_EXPR:
      return eval_int_lshift_expr(proc, val, (idl_binary_expr_t *)expr, type);
    case IDL_RSHIFT_EXPR:
      return eval_int_rshift_expr(proc, val, (idl_binary_expr_t *)expr, type);
    case IDL_ADD_EXPR:
      return eval_int_add_expr(proc, val, (idl_binary_expr_t *)expr, type);
    case IDL_SUB_EXPR:
      return eval_int_sub_expr(proc, val, (idl_binary_expr_t *)expr, type);
    case IDL_MULT_EXPR:
      return eval_int_mult_expr(proc, val, (idl_binary_expr_t *)expr, type);
    case IDL_DIV_EXPR:
      return eval_int_div_expr(proc, val, (idl_binary_expr_t *)expr, type);
    case IDL_MOD_EXPR:
      return eval_int_mod_expr(proc, val, (idl_binary_expr_t *)expr, type);
    case IDL_MINUS_EXPR:
      return eval_int_minus_expr(proc, val, (idl_unary_expr_t *)expr, type);
    case IDL_PLUS_EXPR:
      return eval_int_plus_expr(proc, val, (idl_unary_expr_t *)expr, type);
    case IDL_NOT_EXPR:
      return eval_int_not_expr(proc, val, (idl_unary_expr_t *)expr, type);
    case IDL_ULONG:
    case IDL_ULLONG:
      uint_max = type == IDL_LLONG ? UINT64_MAX : UINT32_MAX;

      if (((idl_literal_t *)node)->value.ullng > uint_max) {
        idl_error(proc, idl_location(expr),
          "value too large for %s", "<type>");
        return IDL_RETCODE_OUT_OF_RANGE;
      }

      val->negative = false;
      val->value.ullng = ((idl_literal_t *)expr)->value.ullng;
      break;
    default:
      idl_error(proc, idl_location(expr),
        "cannot evaluate %s as integer expression", "<expression>");
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
{ return eval_int_expr(proc, val, expr, type); }

static idl_retcode_t
eval_float_expr(
  idl_processor_t *proc,
  long double *val,
  const idl_const_expr_t *expr,
  idl_mask_t type);

static bool
float_overflows(long double ldbl, idl_mask_t type)
{
  if (type == IDL_FLOAT)
    return isnan((float)ldbl) || isinf((float)ldbl);
  else if (type == IDL_DOUBLE)
    return isnan((double)ldbl) || isinf((double)ldbl);
  else if (type == IDL_LDOUBLE)
    return isnan(ldbl) || isinf(ldbl);
  abort();
}

static idl_retcode_t
eval_float_add_expr(
  idl_processor_t *proc,
  long double *val,
  const idl_binary_expr_t *expr,
  idl_mask_t type)
{
  idl_retcode_t ret;
  long double lval, rval;

  if ((ret = eval_float_expr(proc, &lval, expr->left, type)))
    return ret;
  if ((ret = eval_float_expr(proc, &rval, expr->right, type)))
    return ret;

  if (float_overflows(lval+rval, type)) {
    idl_error(proc, idl_location(expr),
      "Floating point expression overflows");
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  *val = lval+rval;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_float_sub_expr(
  idl_processor_t *proc,
  long double *val,
  const idl_binary_expr_t *expr,
  idl_mask_t type)
{
  idl_retcode_t ret;
  long double lval, rval;

  if ((ret = eval_float_expr(proc, &lval, expr->left, type)))
    return ret;
  if ((ret = eval_float_expr(proc, &rval, expr->right, type)))
    return ret;

  if (float_overflows(lval-rval, type)) {
    idl_error(proc, idl_location(expr),
      "Floating point expression overflows");
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  *val = lval-rval;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_float_mult_expr(
  idl_processor_t *proc,
  long double *val,
  const idl_binary_expr_t *expr,
  idl_mask_t type)
{
  idl_retcode_t ret;
  long double lval, rval;

  if ((ret = eval_float_expr(proc, &lval, expr->left, type)))
    return ret;
  if ((ret = eval_float_expr(proc, &rval, expr->right, type)))
    return ret;

  if (float_overflows(lval*rval, type)) {
    idl_error(proc, idl_location(expr),
      "Floating point expression overflows");
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  *val = lval*rval;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_float_div_expr(
  idl_processor_t *proc,
  long double *val,
  const idl_binary_expr_t *expr,
  idl_mask_t type)
{
  idl_retcode_t ret;
  long double lval, rval;

  if ((ret = eval_float_expr(proc, &lval, expr->left, type)))
    return ret;
  if ((ret = eval_float_expr(proc, &rval, expr->right, type)))
    return ret;

  if (rval == 0.0l) {
    idl_error(proc, idl_location(expr),
      "Division by zero in floating point expression");
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  } else if (float_overflows(lval/rval, type)) {
    idl_error(proc, idl_location(expr),
      "Floating point expression overflows");
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  *val = lval/rval;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_float_plus_expr(
  idl_processor_t *proc,
  long double *val,
  const idl_unary_expr_t *expr,
  idl_mask_t type)
{
  idl_retcode_t ret;
  long double rval;

  if ((ret = eval_float_expr(proc, &rval, expr->right, type)))
    return ret;

  *val = +rval;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_float_minus_expr(
  idl_processor_t *proc,
  long double *val,
  const idl_unary_expr_t *expr,
  idl_mask_t type)
{
  idl_retcode_t ret;
  long double rval;

  if ((ret = eval_float_expr(proc, &rval, expr->right, type)))
    return ret;

  if (float_overflows(-rval, type)) {
    idl_error(proc, idl_location(expr),
      "Floating point expression overflows");
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  *val = -rval;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_float_expr(
  idl_processor_t *proc,
  long double *val,
  const idl_const_expr_t *expr,
  idl_mask_t type)
{
  assert(type == IDL_DOUBLE || type == IDL_LDOUBLE);

  switch (expr->mask & ~(IDL_EXPR|IDL_LITERAL)) {
    case IDL_ADD_EXPR:
      return eval_float_add_expr(proc, val, (idl_binary_expr_t *)expr, type);
    case IDL_SUB_EXPR:
      return eval_float_sub_expr(proc, val, (idl_binary_expr_t *)expr, type);
    case IDL_MULT_EXPR:
      return eval_float_mult_expr(proc, val, (idl_binary_expr_t *)expr, type);
    case IDL_DIV_EXPR:
      return eval_float_div_expr(proc, val, (idl_binary_expr_t *)expr, type);
    case IDL_MINUS_EXPR:
      return eval_float_minus_expr(proc, val, (idl_unary_expr_t *)expr, type);
    case IDL_PLUS_EXPR:
      return eval_float_plus_expr(proc, val, (idl_unary_expr_t *)expr, type);
    case IDL_DOUBLE:
    case IDL_LDOUBLE:
      if (float_overflows(((idl_literal_t *)expr)->value.ldbl, type)) {
        idl_error(proc, idl_location(expr),
          "Floating point expression overflows");
        return IDL_RETCODE_ILLEGAL_EXPRESSION;
      }
      *val = ((idl_literal_t *)expr)->value.ldbl;
      break;
    default:
      idl_error(proc, idl_location(expr),
        "Cannot evaluate %s as floating point expression", "<expression>");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_float(
  idl_processor_t *proc,
  idl_node_t **nodeptr,
  idl_const_expr_t *const_expr,
  idl_mask_t type)
{
  idl_retcode_t ret;
  long double val;
  idl_constval_t *constval;

  ret = eval_float_expr(
    proc, &val, const_expr, type == IDL_LDOUBLE ? IDL_LDOUBLE : IDL_DOUBLE);
  if (ret != IDL_RETCODE_OK)
    return ret;
  ret = idl_create_constval(proc, &constval, idl_location(const_expr), type);
  if (ret != IDL_RETCODE_OK)
    return ret;

  switch (type) {
    case IDL_FLOAT:
      constval->value.flt = (float)val;
      break;
    case IDL_DOUBLE:
      constval->value.dbl = (double)val;
      break;
    case IDL_LDOUBLE:
      constval->value.ldbl = val;
      break;
    default:
      abort();
      break;
  }

  *nodeptr = (idl_node_t*)constval;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_int(
  idl_processor_t *proc,
  idl_node_t **nodeptr,
  idl_const_expr_t *expr,
  idl_mask_t type)
{
  idl_retcode_t ret;
  idl_intval_t intval;
  idl_constval_t *constval;

  ret = eval_int_expr(
    proc, &intval, expr, (type & IDL_LLONG) ? IDL_LLONG : IDL_LONG);
  if (ret != IDL_RETCODE_OK)
    return ret;
  ret = idl_create_constval(proc, &constval, idl_location(expr), type);
  if (ret != IDL_RETCODE_OK)
    return ret;

  if (intval.negative) {
    int64_t min;
    switch (type) {
      case IDL_INT8:
        min = INT8_MIN;
        constval->value.int8 = (int8_t)intval.value.llng;
        break;
      case IDL_SHORT:
      //case IDL_INT16:
        min = INT16_MIN;
        constval->value.int16 = (int16_t)intval.value.llng;
        break;
      case IDL_LONG:
      //case IDL_INT32:
        min = INT32_MIN;
        constval->value.int32 = (int32_t)intval.value.llng;
        break;
      case IDL_LLONG:
      //case IDL_INT64:
        min = INT64_MIN;
        constval->value.int64 = (int64_t)intval.value.llng;
        break;
      default:
        abort();
    }
    if (intval.value.llng < min) {
      idl_delete_node(constval);
      idl_error(proc, idl_location(expr),
        "Value is too small for '%s'", "");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
  } else {
    uint64_t max = 0;
    switch (type) {
      case IDL_INT8:
        max = INT8_MAX;
        constval->value.int8 = (int8_t)intval.value.ullng;
        break;
      case IDL_OCTET:
      case IDL_UINT8:
        max = UINT8_MAX;
        constval->value.uint8 = (uint8_t)intval.value.ullng;
        break;
      case IDL_SHORT:
      //case IDL_INT16:
        max = INT16_MAX;
        constval->value.int16 = (int16_t)intval.value.ullng;
        break;
      case IDL_USHORT:
      //case IDL_UINT16:
        max = UINT16_MAX;
        constval->value.uint16 = (uint16_t)intval.value.ullng;
        break;
      case IDL_LONG:
      //case IDL_INT32:
        max = INT32_MAX;
        constval->value.int32 = (int32_t)intval.value.ullng;
        break;
      case IDL_ULONG:
      //case IDL_UINT32:
        max = UINT32_MAX;
        constval->value.uint32 = (uint32_t)intval.value.ullng;
        break;
      case IDL_LLONG:
      //case IDL_INT64:
        max = INT64_MAX;
        constval->value.int64 = (int64_t)intval.value.ullng;
        break;
      case IDL_ULLONG:
      //case IDL_UINT64:
        max = UINT64_MAX;
        constval->value.uint64 = intval.value.ullng;
        break;
      default:
        abort();
    }
    if (intval.value.ullng > max) {
      idl_delete_node(constval);
      idl_error(proc, idl_location(expr),
        "Value is too large for '%s'", "<type>");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
  }

  *nodeptr = (idl_node_t*)constval;
  return IDL_RETCODE_OK;
}

idl_retcode_t
idl_evaluate(
  idl_processor_t *proc,
  idl_node_t **nodeptr,
  idl_const_expr_t *expr,
  idl_mask_t type)
{
  idl_retcode_t ret;
  idl_constval_t *constval;

  assert(type & (IDL_BASE_TYPE|IDL_ENUMERATOR));
  type &= (IDL_ENUMERATOR|IDL_BASE_TYPE | (IDL_BASE_TYPE - 1));

  /* enumerators are referenced */
  if (type == IDL_ENUMERATOR) {
    expr = idl_unalias(expr);
    if (!idl_is_masked(expr, IDL_ENUMERATOR)) {
      idl_error(proc, idl_location(expr),
        "Cannot evaluate '%s' as enumerator", "<foobar>");
      return IDL_RETCODE_SEMANTIC_ERROR;
    }
    *((idl_node_t **)nodeptr) = expr;
    return IDL_RETCODE_OK;
  } else if (type == IDL_OCTET || (type & IDL_INTEGER_TYPE) == IDL_INTEGER_TYPE) {
    if ((ret = eval_int(proc, nodeptr, expr, type)) == IDL_RETCODE_OK)
      idl_delete_node(expr);
    return ret;
  } else if ((type & IDL_FLOATING_PT_TYPE) == IDL_FLOATING_PT_TYPE) {
    if ((ret = eval_float(proc, nodeptr, expr, type)) == IDL_RETCODE_OK)
      idl_delete_node(expr);
    return ret;
  }

  ret = idl_create_constval(proc, &constval, idl_location(expr), type);
  if (ret != IDL_RETCODE_OK)
    goto err_alloc;

  if (type == IDL_CHAR) {
    if (expr->mask != IDL_CHAR_LITERAL) {
      idl_error(proc, idl_location(expr),
        "Cannot evaluate '%s' as character expression", "<foobar>");
      goto err_eval;
    }
    constval->value.chr = 'a';//((idl_literal_t *)expr)->value.chr;
  } else if (type == IDL_BOOL) {
    if (expr->mask != (IDL_BOOLEAN_LITERAL | IDL_EXPR)) {
      idl_error(proc, idl_location(expr),
        "Cannot evaluate '%s' as boolean expression", "<foobar>");
      goto err_eval;
    }
    constval->value.bln = ((idl_literal_t *) expr)->value.bln;
  } else {
    assert(0);
  }

  idl_delete_node(expr);
  *nodeptr = (idl_node_t *)constval;
  return IDL_RETCODE_OK;
err_eval:
  free(constval);
err_alloc:
  return ret;
}
