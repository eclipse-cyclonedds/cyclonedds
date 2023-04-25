// Copyright(c) 2021 to 2022 ZettaScale Technology and others
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <inttypes.h>

#include "idl/string.h"
#include "idl/processor.h"
#include "tree.h"
#include "expression.h"

static uint64_t uintmax(idl_type_t type);
static int64_t intmax(idl_type_t type);
static int64_t intmin(idl_type_t type);
static idl_intval_t intval(const idl_const_expr_t *expr);
static idl_floatval_t floatval(const idl_const_expr_t *expr);

#if _MSC_VER
/* suppress warning C4146: unary minus operator applied to unsigned type */
__pragma(warning(push))
__pragma(warning(disable: 4146))
#endif

idl_operator_t idl_operator(const void *node)
{
  idl_mask_t mask = idl_mask(node);

  mask &= ((((unsigned)IDL_BINARY_OPERATOR)<<1) - 1) |
          ((((unsigned)IDL_UNARY_OPERATOR)<<1) - 1);
  switch ((idl_operator_t)mask) {
    case IDL_MINUS:
    case IDL_PLUS:
    case IDL_NOT:
    case IDL_OR:
    case IDL_XOR:
    case IDL_AND:
    case IDL_LSHIFT:
    case IDL_RSHIFT:
    case IDL_ADD:
    case IDL_SUBTRACT:
    case IDL_MULTIPLY:
    case IDL_DIVIDE:
    case IDL_MODULO:
      return (idl_operator_t)mask;
    default:
      break;
  }

  return IDL_NOP;
}

static idl_retcode_t
eval_int_expr(
  idl_pstate_t *pstate,
  const idl_const_expr_t *expr,
  idl_type_t type,
  idl_intval_t *valp);

static unsigned greatest(unsigned a, unsigned b)
{
  return a > b ? a : b;
}

#define n(x) negative(x)
#define s(x) x->value.llng
#define t(x) x->type
#define u(x) x->value.ullng

static inline bool negative(idl_intval_t *a)
{
  switch (t(a)) {
    case IDL_LONG:
    case IDL_LLONG:
      return s(a) < 0;
    default:
      return false;
  }
}

static bool int_overflows(idl_intval_t *val, idl_type_t type)
{
  if (((unsigned)type & (unsigned)IDL_UNSIGNED))
    return val->value.ullng > uintmax(type);
  else
    return val->value.llng < intmin(type) || val->value.llng > intmax(type);
}

static idl_retcode_t
int_or(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  t(r) = greatest(t(a), t(b));
  u(r) = u(a) | u(b);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_xor(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  t(r) = greatest(t(a), t(b));
  u(r) = u(a) ^ u(b);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_and(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  t(r) = greatest(t(a), t(b));
  u(r) = u(a) & u(b);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_lshift(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  idl_type_t gt = greatest(t(a), t(b));
  if (u(b) >= (uintmax(gt) == UINT64_MAX ? 64 : 32))
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  t(r) = gt;
  if (n(a))
    s(r) = s(a) << u(b);
  else
    u(r) = u(a) << u(b);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_rshift(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  idl_type_t gt = greatest(t(a), t(b));
  if (u(b) >= ((gt & IDL_LLONG) == IDL_LLONG ? 64 : 32))
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  t(r) = gt;
  if (n(a))
    s(r) = s(a) >> u(b);
  else
    u(r) = u(a) >> u(b);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_add(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  idl_type_t gt = greatest(t(a), t(b));

  switch ((n(a) ? 1:0) + (n(b) ? 2:0)) {
    case 0:
      if (u(a) > uintmax(gt) - u(b))
        return IDL_RETCODE_OUT_OF_RANGE;
      u(r) = u(a) + u(b);
      t(r) = gt | (u(r) > (uint64_t)intmax(gt));
      break;
    case 1:
      if (-u(a) < u(b)) {
        u(r) = u(a) + u(b);
        t(r) = gt | (u(r) > (uint64_t)intmax(gt));
      } else {
        u(r) = u(a) + u(b);
        t(r) = gt;
      }
      break;
    case 2:
      if (-u(b) >= u(a)) {
        u(r) = u(a) + u(b);
        t(r) = gt | (u(r) > (uint64_t)intmax(gt));
      } else {
        u(r) = u(a) + u(b);
        t(r) = gt;
      }
      break;
    case 3:
      if (s(b) < (intmin(gt) + -s(a)))
        return IDL_RETCODE_OUT_OF_RANGE;
      s(r) = s(a) + s(b);
      t(r) = gt & ~1u;
      break;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_subtract(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  idl_type_t gt = greatest(t(a), t(b));

  switch ((n(a) ? 1:0) + (n(b) ? 2:0)) {
    case 0:
      if (u(a) < u(b) && u(a) - u(b) > -(uint64_t)intmin(gt))
        return IDL_RETCODE_OUT_OF_RANGE;
      u(r) = u(a) - u(b);
      t(r) = gt & ~1u;
      break;
    case 1:
      if (u(b) > -(uint64_t)intmin(gt) || -u(a) > -(uint64_t)intmin(gt) - u(b))
        return -1;
      u(r) = u(a) - u(b);
      t(r) = gt & ~1u;
      break;
    case 2:
      if (-u(b) > uintmax(gt) - u(a))
        return IDL_RETCODE_OUT_OF_RANGE;
      u(r) = u(a) - u(b);
      t(r) = gt | (-u(b) > u(a));
      break;
    case 3:
      s(r) = s(a) - s(b);
      t(r) = gt;
      break;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_multiply(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  idl_type_t gt = greatest(t(a), t(b));

  switch ((n(a) ? 1:0) + (n(b) ? 2:0)) {
    case 0:
      if (u(b) && u(a) > uintmax(gt) / u(b))
        return IDL_RETCODE_OUT_OF_RANGE;
      u(r) = u(a) * u(b);
      t(r) = gt;
      break;
    case 1:
      if (u(b) > (uint64_t)(intmin(gt) / s(a)))
        return IDL_RETCODE_OUT_OF_RANGE;
      u(r) = u(a) * u(b);
      t(r) = gt & ~1u;
      break;
    case 2:
      if (u(a) && u(a) > (uint64_t)(intmin(gt) / s(b)))
        return IDL_RETCODE_OUT_OF_RANGE;
      u(r) = u(a) * u(b);
      t(r) = gt & ~1u;
      break;
    case 3:
      if (-u(a) > uintmax(gt) / -u(b))
        return IDL_RETCODE_OUT_OF_RANGE;
      u(r) = u(a) * u(b);
      t(r) = gt | (u(r) > (uint32_t)intmax(gt));
      break;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_divide(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  idl_type_t gt = greatest(t(a), t(b));

  if (u(b) == 0)
    return IDL_RETCODE_ILLEGAL_EXPRESSION;

  switch ((n(a) ? 1:0) + (n(b) ? 2:0)) {
    case 0:
      u(r) = u(a) / u(b);
      t(r) = gt;
      break;
    case 1:
      u(r) = u(a) / u(b);
      t(r) = gt;
      break;
    case 2:
      if (-(uint64_t)intmin(gt) < u(a) && s(b) == -1)
        return IDL_RETCODE_OUT_OF_RANGE;
      u(r) = u(a) / u(b);
      t(r) = gt;
      break;
    case 3:
      s(r) = s(a) / s(b);
      t(r) = gt;
      break;
  }

  return IDL_RETCODE_OK;
}

static idl_retcode_t
int_modulo(idl_intval_t *a, idl_intval_t *b, idl_intval_t *r)
{
  idl_type_t gt = greatest(t(a), t(b));

  if (u(b) == 0)
    return IDL_RETCODE_ILLEGAL_EXPRESSION;

  switch ((n(a) ? 1:0) + (n(b) ? 2:0)) {
    case 0:
      u(r) = u(a) % u(b);
      t(r) = gt;
      break;
    case 1:
      u(r) = -(-u(a) % u(b));
      t(r) = gt;
      break;
    case 2:
      u(r) = u(a) % -u(b);
      t(r) = gt;
      break;
    case 3:
      u(r) = u(a) % u(b);
      t(r) = gt;
      break;
  }

  return IDL_RETCODE_OK;
}

#ifndef NDEBUG
static bool is_arith_return_type (idl_type_t t)
{
  return (t == IDL_LONG || t == IDL_ULONG || t == IDL_LLONG || t == IDL_ULLONG);
}
#endif

static idl_retcode_t
eval_binary_int_expr(
  idl_pstate_t *pstate,
  const idl_binary_expr_t *expr,
  idl_type_t type,
  idl_intval_t *valp)
{
  idl_retcode_t ret;
  idl_intval_t val, lhs, rhs;

  assert (is_arith_return_type (type));

  if ((ret = eval_int_expr(pstate, expr->left, type, &lhs)))
    return ret;
  if ((ret = eval_int_expr(pstate, expr->right, type, &rhs)))
    return ret;

  switch (idl_operator(expr)) {
    case IDL_OR:       ret = int_or(&lhs, &rhs, &val);       break;
    case IDL_XOR:      ret = int_xor(&lhs, &rhs, &val);      break;
    case IDL_AND:      ret = int_and(&lhs, &rhs, &val);      break;
    case IDL_LSHIFT:   ret = int_lshift(&lhs, &rhs, &val);   break;
    case IDL_RSHIFT:   ret = int_rshift(&lhs, &rhs, &val);   break;
    case IDL_ADD:      ret = int_add(&lhs, &rhs, &val);      break;
    case IDL_SUBTRACT: ret = int_subtract(&lhs, &rhs, &val); break;
    case IDL_MULTIPLY: ret = int_multiply(&lhs, &rhs, &val); break;
    case IDL_DIVIDE:   ret = int_divide(&lhs, &rhs, &val);   break;
    case IDL_MODULO:   ret = int_modulo(&lhs, &rhs, &val);   break;
    default:
      idl_error(pstate, idl_location(expr),
        "Cannot evaluate %s as integer expression", idl_construct(expr));
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  if (ret == IDL_RETCODE_ILLEGAL_EXPRESSION) {
    idl_error(pstate, idl_location(expr), "Invalid integer expression");
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  } else if (ret == IDL_RETCODE_OUT_OF_RANGE || int_overflows(&val, type)) {
    idl_error(pstate, idl_location(expr), "Integer expression overflows");
    return IDL_RETCODE_OUT_OF_RANGE;
  }

  *valp = val;
  return IDL_RETCODE_OK;
}

static int int_minus(idl_intval_t *a, idl_intval_t *r)
{
  t(r) = t(a);
  u(r) = -u(a);
  return IDL_RETCODE_OK;
}

static int int_plus(idl_intval_t *a, idl_intval_t *r)
{
  *r = *a;
  return IDL_RETCODE_OK;
}

static int int_not(idl_intval_t *a, idl_intval_t *r)
{
  t(r) = t(a);
  u(r) = ~u(a);
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_unary_int_expr(
  idl_pstate_t *pstate,
  const idl_unary_expr_t *expr,
  idl_type_t type,
  idl_intval_t *valp)
{
  idl_retcode_t ret;
  idl_intval_t val, rhs;

  assert (is_arith_return_type (type));

  if ((ret = eval_int_expr(pstate, expr->right, type, &rhs)))
    return ret;

  switch (idl_operator(expr)) {
    case IDL_MINUS: ret = int_minus(&rhs, &val); break;
    case IDL_PLUS:  ret = int_plus(&rhs, &val);  break;
    case IDL_NOT:   ret = int_not(&rhs, &val);   break;
    default:
      idl_error(pstate, idl_location(expr),
        "Cannot evaluate %s as integer expression", idl_construct(expr));
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  if (ret == IDL_RETCODE_ILLEGAL_EXPRESSION) {
    idl_error(pstate, idl_location(expr), "Invalid integer expression");
    return IDL_RETCODE_ILLEGAL_EXPRESSION;
  } else if (ret == IDL_RETCODE_OUT_OF_RANGE || int_overflows(&val, type)) {
    idl_error(pstate, idl_location(expr), "Integer expression overflows");
    return IDL_RETCODE_OUT_OF_RANGE;
  }

  *valp = val;
  return IDL_RETCODE_OK;
}

static idl_intval_t bitval(const idl_const_expr_t *const_expr)
{
  assert(idl_is_bit_value(const_expr));

  const idl_bit_value_t *val = (idl_bit_value_t *)const_expr;

  return (idl_intval_t){.type = IDL_ULLONG, .value = {.ullng = (uint64_t) (0x1ull << val->position.value)} };
}

#undef u
#undef s
#undef t
#undef n

static idl_retcode_t
eval_int_expr(
  idl_pstate_t *pstate,
  const idl_const_expr_t *const_expr,
  idl_type_t type,
  idl_intval_t *valp)
{
  idl_mask_t mask;
  if (idl_mask(const_expr) & IDL_CONST)
    const_expr = ((idl_const_t *)const_expr)->const_expr;

  mask = idl_mask(const_expr);
  if (mask & IDL_BINARY_OPERATOR) {
    return eval_binary_int_expr(pstate, const_expr, type, valp);
  } else if (mask & IDL_UNARY_OPERATOR) {
    return eval_unary_int_expr(pstate, const_expr, type, valp);
  } else if ((mask & (IDL_LITERAL|IDL_OCTET)) == (IDL_LITERAL|IDL_OCTET) ||
             (mask & (IDL_LITERAL|IDL_INTEGER_TYPE)) == (IDL_LITERAL|IDL_INTEGER_TYPE))
  {
    *valp = intval(const_expr);
    return IDL_RETCODE_OK;
  } else if (mask & IDL_BIT_VALUE) {
    *valp = bitval(const_expr);
    return IDL_RETCODE_OK;
  }

  idl_error(pstate, idl_location(const_expr),
    "Cannot evaluate %s as integer expression", idl_construct(const_expr));
  return IDL_RETCODE_ILLEGAL_EXPRESSION;
}

static idl_retcode_t
eval_bitmask(
  idl_pstate_t *pstate,
  idl_const_expr_t *expr,
  idl_type_t type,
  void *nodep)
{
  idl_retcode_t ret;
  idl_intval_t val;
  idl_literal_t literal;
  idl_type_t as = IDL_ULLONG;

  memset(&literal, 0, sizeof(literal));

  if ((ret = eval_int_expr(pstate, expr, as, &val)) ||
      (ret = idl_create_literal(pstate, idl_location(expr), type, nodep)))
    return ret;

  literal.value.uint64 = val.value.ullng;

  (*((idl_literal_t **)nodep))->value = literal.value;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_int(
  idl_pstate_t *pstate,
  idl_const_expr_t *expr,
  idl_type_t type,
  void *nodep)
{
  idl_retcode_t ret;
  idl_intval_t val;
  idl_literal_t literal;
  idl_type_t as = IDL_LONG;

  memset(&literal, 0, sizeof(literal));

  if (((unsigned)type & (unsigned)IDL_LLONG) == IDL_LLONG)
    as = IDL_LLONG;
  if ((ret = eval_int_expr(pstate, expr, as, &val)))
    return ret;
  if (type == IDL_ANY)
    type = val.type;
  if (int_overflows(&val, type))
    goto overflow;

  if ((ret = idl_create_literal(pstate, idl_location(expr), type, nodep)))
    return ret;

  switch (type) {
    case IDL_INT8:   literal.value.int8 = (int8_t)val.value.llng;      break;
    case IDL_OCTET:
    case IDL_UINT8:  literal.value.uint8 = (uint8_t)val.value.ullng;   break;
    case IDL_SHORT:
    case IDL_INT16:  literal.value.int16 = (int16_t)val.value.llng;    break;
    case IDL_USHORT:
    case IDL_UINT16: literal.value.uint16 = (uint16_t)val.value.ullng; break;
    case IDL_LONG:
    case IDL_INT32:  literal.value.int32 = (int32_t)val.value.llng;    break;
    case IDL_ULONG:
    case IDL_UINT32: literal.value.uint32 = (uint32_t)val.value.ullng; break;
    case IDL_LLONG:
    case IDL_INT64:  literal.value.int64 = (int64_t)val.value.llng;    break;
    case IDL_ULLONG:
    case IDL_UINT64: literal.value.uint64 = val.value.ullng;           break;
    default:
      break;
  }

  (*((idl_literal_t **)nodep))->value = literal.value;
  return IDL_RETCODE_OK;
overflow:
  idl_error(pstate, idl_location(expr), "Integer expression overflows");
  return IDL_RETCODE_OUT_OF_RANGE;
}

static idl_retcode_t
eval_float_expr(
  idl_pstate_t *pstate,
  const idl_const_expr_t *expr,
  idl_type_t type,
  idl_floatval_t *valp);

#if defined __MINGW32__
_Pragma("GCC diagnostic push")
_Pragma("GCC diagnostic ignored \"-Wfloat-conversion\"")
#endif
static bool
float_overflows(long double ldbl, idl_type_t type)
{
  if (type == IDL_FLOAT)
    return isnan((float)ldbl) || isinf((float)ldbl);
  else if (type == IDL_DOUBLE)
    return isnan((double)ldbl) || isinf((double)ldbl);
  else if (type == IDL_LDOUBLE)
    return isnan(ldbl) || isinf(ldbl);
  abort();
}
#if defined __MINGW32__
_Pragma("GCC diagnostic pop")
#endif

static idl_retcode_t
eval_binary_float_expr(
  idl_pstate_t *pstate,
  const idl_binary_expr_t *expr,
  idl_type_t type,
  idl_floatval_t *valp)
{
  idl_retcode_t ret;
  idl_floatval_t val, lhs, rhs;

  if ((ret = eval_float_expr(pstate, expr->left, type, &lhs)))
    return ret;
  if ((ret = eval_float_expr(pstate, expr->right, type, &rhs)))
    return ret;

  switch (idl_operator(expr)) {
    case IDL_ADD:      val = lhs+rhs; break;
    case IDL_SUBTRACT: val = lhs-rhs; break;
    case IDL_MULTIPLY: val = lhs*rhs; break;
    case IDL_DIVIDE:
      if (rhs == 0.0l) {
        idl_error(pstate, idl_location(expr),
          "Division by zero in floating point expression");
        return IDL_RETCODE_ILLEGAL_EXPRESSION;
      }
      val = lhs/rhs;
      break;
    default:
      idl_error(pstate, idl_location(expr),
        "Invalid floating point expression");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  if (float_overflows(val, type)) {
    idl_error(pstate, idl_location(expr),
      "Floating point expression overflows");
    return IDL_RETCODE_OUT_OF_RANGE;
  }

  *valp = val;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_unary_float_expr(
  idl_pstate_t *pstate,
  const idl_unary_expr_t *expr,
  idl_type_t type,
  idl_floatval_t *valp)
{
  idl_retcode_t ret;
  idl_floatval_t val, rhs;

  if ((ret = eval_float_expr(pstate, expr->right, type, &rhs)))
    return ret;

  switch (idl_operator(expr)) {
    case IDL_PLUS:  val = +rhs; break;
    case IDL_MINUS: val = -rhs; break;
    default:
      idl_error(pstate, idl_location(expr),
        "Invalid floating point expression");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
  }

  if (float_overflows(val, type)) {
    idl_error(pstate, idl_location(expr),
      "Floating point expression overflows");
    return IDL_RETCODE_OUT_OF_RANGE;
  }

  *valp = val;
  return IDL_RETCODE_OK;
}

static idl_retcode_t
eval_float_expr(
  idl_pstate_t *pstate,
  const idl_const_expr_t *expr,
  idl_type_t type,
  idl_floatval_t *valp)
{
  if (idl_mask(expr) & IDL_CONST)
    expr = ((const idl_const_t *)expr)->const_expr;

  if (idl_mask(expr) & IDL_BINARY_OPERATOR) {
    return eval_binary_float_expr(pstate, expr, type, valp);
  } else if (idl_mask(expr) & IDL_UNARY_OPERATOR) {
    return eval_unary_float_expr(pstate, expr, type, valp);
  } else if (idl_mask(expr) & (IDL_LITERAL|IDL_FLOATING_PT_TYPE)) {
    *valp = floatval(expr);
    return IDL_RETCODE_OK;
  }

  idl_error(pstate, idl_location(expr),
    "Cannot evaluate %s as floating point expression", idl_construct(expr));
  return IDL_RETCODE_ILLEGAL_EXPRESSION;
}

static idl_retcode_t
eval_float(
  idl_pstate_t *pstate,
  idl_const_expr_t *expr,
  idl_type_t type,
  void *nodep)
{
  idl_retcode_t ret;
  idl_floatval_t val;
  idl_literal_t *literal = NULL;
  idl_type_t as = (type == IDL_LDOUBLE) ? IDL_LDOUBLE : IDL_DOUBLE;

  if ((ret = eval_float_expr(pstate, expr, as, &val)))
    return ret;
  if (type == IDL_ANY)
    type = as;
  if (float_overflows(val, type))
    goto overflow;
  if ((ret = idl_create_literal(pstate, idl_location(expr), type, &literal)))
    return ret;

  switch (type) {
    case IDL_FLOAT:   literal->value.flt = (float)val;  break;
    case IDL_DOUBLE:  literal->value.dbl = (double)val; break;
    case IDL_LDOUBLE: literal->value.ldbl = val;        break;
    default:
      break;
  }

  *((idl_literal_t **)nodep) = literal;
  return IDL_RETCODE_OK;
overflow:
  idl_error(pstate, idl_location(expr),
    "Floating point expression overflows");
  return IDL_RETCODE_OUT_OF_RANGE;
}

static idl_type_t figure_type(idl_const_expr_t *const_expr)
{
  if (idl_mask(const_expr) & IDL_CONST)
    const_expr = ((const idl_const_t *)const_expr)->const_expr;

  if (idl_mask(const_expr) & IDL_BINARY_OPERATOR)
    return figure_type(((const idl_binary_expr_t *)const_expr)->left);
  else if (idl_mask(const_expr) & IDL_UNARY_OPERATOR)
    return figure_type(((const idl_unary_expr_t *)const_expr)->right);
  else
    return idl_type(const_expr);
}

idl_retcode_t
idl_evaluate(
  idl_pstate_t *pstate,
  idl_const_expr_t *const_expr,
  idl_type_t type,
  void *nodep)
{
  idl_retcode_t ret;
  idl_literal_t temporary, *literal = NULL;
  idl_type_t implicit;
  static const char fmt[] = "Cannot evaluate %s as %s expression";

  implicit = (type == IDL_ANY) ? figure_type(const_expr) : type;

  /* enumerators are referenced */
  if (implicit == IDL_ENUM) {
    const char *constr = idl_construct(const_expr);
    if (!(idl_mask(const_expr) & IDL_ENUMERATOR)) {
      idl_error(pstate, idl_location(const_expr), fmt, constr, "enumerator");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
    }
    *((idl_enumerator_t **)nodep) = const_expr;
    return IDL_RETCODE_OK;
  } else if (implicit == IDL_BITMASK) {
    if ((ret = eval_bitmask(pstate, const_expr, type, nodep)))
      return ret;
    goto done;
  } else if (implicit == IDL_OCTET || (implicit & IDL_INTEGER_TYPE)) {
    if ((ret = eval_int(pstate, const_expr, type, nodep)))
      return ret;
    goto done;
  } else if (implicit & IDL_FLOATING_PT_TYPE) {
    if ((ret = eval_float(pstate, const_expr, type, nodep)))
      return ret;
    goto done;
  }

  if (idl_is_const(const_expr))
    literal = ((idl_const_t *)const_expr)->const_expr;
  else if (idl_is_literal(const_expr))
    literal = const_expr;
  assert(literal);

  memset(&temporary, 0, sizeof(temporary));
  if (implicit == IDL_CHAR) {
    if (idl_type(literal) == IDL_CHAR) {
      temporary.value.chr = literal->value.chr;
    } else {
      const char *constr = idl_construct(const_expr);
      idl_error(pstate, idl_location(const_expr), fmt, constr, "character");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
    }
  } else if (implicit == IDL_BOOL) {
    if (idl_type(literal) == IDL_BOOL) {
      temporary.value.bln = literal->value.bln;
    } else {
      const char *constr = idl_construct(const_expr);
      idl_error(pstate, idl_location(const_expr), fmt, constr, "boolean");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
    }
  } else if (implicit == IDL_STRING) {
    if (idl_type(literal) == IDL_STRING) {
      if (!(temporary.value.str = idl_strdup(literal->value.str)))
        return IDL_RETCODE_NO_MEMORY;
    } else {
      const char *constr = idl_construct(const_expr);
      idl_error(pstate, idl_location(const_expr), fmt, constr, "string");
      return IDL_RETCODE_ILLEGAL_EXPRESSION;
    }
  }

  if ((ret = idl_create_literal(pstate, idl_location(const_expr), implicit, nodep)))
    return ret;
  (*((idl_literal_t **)nodep))->value = temporary.value;
done:
  idl_unreference_node(const_expr);
  return IDL_RETCODE_OK;
}

static uint64_t uintmax(idl_type_t type)
{
  switch (type) {
    case IDL_INT8:
    case IDL_UINT8:
    case IDL_OCTET:  return UINT8_MAX;
    case IDL_INT16:
    case IDL_SHORT:
    case IDL_UINT16:
    case IDL_USHORT: return UINT16_MAX;
    case IDL_INT32:
    case IDL_LONG:
    case IDL_UINT32:
    case IDL_ULONG:  return UINT32_MAX;
    case IDL_INT64:
    case IDL_LLONG:
    case IDL_UINT64:
    case IDL_ULLONG: return UINT64_MAX;
    default:
      break;
  }

  return 0llu;
}

static int64_t intmax(idl_type_t type)
{
  switch (type) {
    case IDL_INT8:
    case IDL_UINT8:
    case IDL_OCTET:  return INT8_MAX;
    case IDL_INT16:
    case IDL_SHORT:
    case IDL_UINT16:
    case IDL_USHORT: return INT16_MAX;
    case IDL_INT32:
    case IDL_LONG:
    case IDL_UINT32:
    case IDL_ULONG:  return INT32_MAX;
    case IDL_INT64:
    case IDL_LLONG:
    case IDL_UINT64:
    case IDL_ULLONG: return INT64_MAX;
    default:
      break;
  }

  return 0ll;
}

static int64_t intmin(idl_type_t type)
{
  switch (type) {
    case IDL_INT8:
    case IDL_UINT8:
    case IDL_OCTET:  return INT8_MIN;
    case IDL_INT16:
    case IDL_SHORT:
    case IDL_UINT16:
    case IDL_USHORT: return INT16_MIN;
    case IDL_INT32:
    case IDL_LONG:
    case IDL_UINT32:
    case IDL_ULONG:  return INT32_MIN;
    case IDL_INT64:
    case IDL_LLONG:
    case IDL_UINT64:
    case IDL_ULLONG: return INT64_MIN;
    default:
      break;
  }

  return 0ll;
}

static idl_intval_t intval(const idl_const_expr_t *const_expr)
{
  return idl_intval(const_expr);
}

idl_intval_t idl_intval(const idl_const_expr_t *const_expr)
{
  idl_type_t type = idl_type(const_expr);
  const idl_literal_t *val = (idl_literal_t *)const_expr;

#define SIGNED(t,v) (idl_intval_t){ .type = (t), .value = { .llng = (v) } }
#define UNSIGNED(t,v) (idl_intval_t){ .type = (t), .value = { .ullng = (v) } }

  assert(idl_is_literal(const_expr));

  switch (type) {
    case IDL_BITMASK: return bitval(const_expr);
    case IDL_INT8:   return SIGNED(IDL_LONG, val->value.int8);
    case IDL_UINT8:
    case IDL_OCTET:  return UNSIGNED(IDL_ULONG, val->value.uint8);
    case IDL_INT16:
    case IDL_SHORT:  return SIGNED(IDL_LONG, val->value.int16);
    case IDL_UINT16:
    case IDL_USHORT: return UNSIGNED(IDL_ULONG, val->value.uint16);
    case IDL_INT32:
    case IDL_LONG:   return SIGNED(IDL_LONG, val->value.int32);
    case IDL_UINT32:
    case IDL_ULONG:  return UNSIGNED(IDL_ULONG, val->value.uint32);
    case IDL_INT64:
    case IDL_LLONG:  return SIGNED(IDL_LLONG, val->value.int64);
    case IDL_UINT64:
    case IDL_ULLONG:
    default:
      assert(type == IDL_ULLONG);
      return UNSIGNED(IDL_ULLONG, val->value.uint64);
  }

#undef UNSIGNED
#undef SIGNED
}

static idl_equality_t
compare_int(const idl_const_expr_t *lhs, const idl_const_expr_t *rhs)
{
  idl_intval_t lval = intval(lhs), rval = intval(rhs);

  switch ((negative(&lval) ? 1:0) + (negative(&rval) ? 2:0)) {
    case 0:
      if (lval.value.ullng < rval.value.ullng)
        return IDL_LESS;
      if (lval.value.ullng > rval.value.ullng)
        return IDL_GREATER;
      return IDL_EQUAL;
    case 1:
      return IDL_GREATER;
    case 2:
      return IDL_LESS;
    case 3:
    default:
      if (lval.value.llng < rval.value.llng)
        return IDL_LESS;
      if (lval.value.llng > rval.value.llng)
        return IDL_GREATER;
      return IDL_EQUAL;
  }
}

idl_floatval_t
idl_floatval(const idl_const_expr_t *const_expr) {
  return floatval(const_expr);
}

static idl_floatval_t floatval(const idl_const_expr_t *const_expr)
{
  idl_type_t type = idl_type(const_expr);
  const idl_literal_t *val = (idl_literal_t *)const_expr;

  assert(idl_is_literal(const_expr));

  switch (type) {
    case IDL_INT8:
      return (idl_floatval_t)val->value.int8;
    case IDL_OCTET:
    case IDL_UINT8:
      return (idl_floatval_t)val->value.uint8;
    case IDL_SHORT:
    case IDL_INT16:
      return (idl_floatval_t)val->value.int16;
    case IDL_USHORT:
    case IDL_UINT16:
      return (idl_floatval_t)val->value.uint16;
    case IDL_LONG:
    case IDL_INT32:
      return (idl_floatval_t)val->value.int32;
    case IDL_ULONG:
    case IDL_UINT32:
      return (idl_floatval_t)val->value.uint32;
    case IDL_LLONG:
    case IDL_INT64:
      return (idl_floatval_t)val->value.int64;
    case IDL_ULLONG:
    case IDL_UINT64:
      return (idl_floatval_t)val->value.uint64;
    case IDL_FLOAT:
      return (idl_floatval_t)val->value.flt;
    case IDL_DOUBLE:
      return (idl_floatval_t)val->value.dbl;
    case IDL_LDOUBLE:
    default:
      assert(type == IDL_LDOUBLE);
      return val->value.ldbl;
  }
}

static idl_equality_t
compare_float(const idl_const_expr_t *lhs, const idl_const_expr_t *rhs)
{
  idl_floatval_t lval = floatval(lhs), rval = floatval(rhs);
  return (lval < rval) ? IDL_LESS : (lval > rval ? IDL_GREATER : IDL_EQUAL);
}

static char charval(const idl_const_expr_t *const_expr)
{
  assert(idl_type(const_expr) == IDL_CHAR);
  assert(idl_is_literal(const_expr));
  return ((const idl_literal_t *)const_expr)->value.chr;
}

static idl_equality_t
compare_char(const idl_const_expr_t *lhs, const idl_const_expr_t *rhs)
{
  char lval = charval(lhs), rval = charval(rhs);
  return (lval == rval ? IDL_EQUAL : (lval < rval ? IDL_LESS : IDL_GREATER));
}

static bool boolval(const idl_const_expr_t *const_expr)
{
  assert(idl_type(const_expr) == IDL_BOOL);
  assert(idl_is_literal(const_expr));
  return ((const idl_literal_t *)const_expr)->value.bln;
}

static idl_equality_t
compare_bool(const idl_const_expr_t *lhs, const idl_const_expr_t *rhs)
{
  bool lval = boolval(lhs), rval = boolval(rhs);
  return (lval == rval ? IDL_EQUAL : (!lval ? IDL_LESS : IDL_GREATER));
}

static idl_equality_t
compare_enum(const idl_const_expr_t *lhs, const idl_const_expr_t *rhs)
{
  const idl_enumerator_t *lval = lhs, *rval = rhs;
  assert(idl_is_enumerator(lval));
  assert(idl_is_enumerator(rval));
  if (lval->node.parent != rval->node.parent)
    return IDL_MISMATCH; /* incompatible enums */
  if (lval->value.value < rval->value.value)
    return IDL_LESS;
  if (lval->value.value > rval->value.value)
    return IDL_GREATER;
  return IDL_EQUAL;
}

static const char *stringval(const idl_const_expr_t *const_expr)
{
  assert(idl_type(const_expr) == IDL_STRING);
  assert(idl_is_literal(const_expr));
  return ((const idl_literal_t *)const_expr)->value.str;
}

static idl_equality_t
compare_string(const idl_const_expr_t *lhs, const idl_const_expr_t *rhs)
{
  int cmp;
  const char *lval = stringval(lhs), *rval = stringval(rhs);
  if (!lval || !rval)
    return (rval ? IDL_LESS : (lval ? IDL_GREATER : IDL_EQUAL));
  cmp = strcmp(lval, rval);
  return (cmp < 0 ? IDL_LESS : (cmp > 0 ? IDL_GREATER : IDL_EQUAL));
}

#if defined _MSC_VER
__pragma(warning(pop))
#endif

idl_equality_t
idl_compare(const void *lhs, const void *rhs)
{
  idl_type_t ltype, rtype;

  ltype = idl_type(lhs);
  rtype = idl_type(rhs);

  if ((ltype == IDL_OCTET || (ltype & IDL_INTEGER_TYPE)) &&
      (rtype == IDL_OCTET || (rtype & IDL_INTEGER_TYPE)))
    return compare_int(lhs, rhs);
  else if ((ltype & IDL_FLOATING_PT_TYPE) &&
           (rtype & IDL_FLOATING_PT_TYPE))
    return compare_float(lhs, rhs);
  else if (!ltype || !rtype)
    return IDL_INVALID; /* non-type */
  else if (ltype != rtype)
    return IDL_MISMATCH; /* incompatible types */
  else if (ltype == IDL_CHAR)
    return compare_char(lhs, rhs);
  else if (ltype == IDL_BOOL)
    return compare_bool(lhs, rhs);
  else if (ltype == IDL_ENUM)
    return compare_enum(lhs, rhs);
  else if (ltype == IDL_STRING)
    return compare_string(lhs, rhs);

  return IDL_INVALID; /* non-comparable type */
}
