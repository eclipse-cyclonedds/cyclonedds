// Copyright(c) 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef IDL_EXPRESSION_H
#define IDL_EXPRESSION_H

#include "idl/tree.h"

typedef enum idl_equality idl_equality_t;
enum idl_equality {
  IDL_INVALID = -3,
  IDL_MISMATCH = -2, /**< type mismatch */
  IDL_LESS = -1,
  IDL_EQUAL,
  IDL_GREATER,
};

IDL_EXPORT idl_equality_t
idl_compare(
  const idl_const_expr_t *lhs,
  const idl_const_expr_t *rhs);

#endif /* IDL_EXPRESSION_H */
