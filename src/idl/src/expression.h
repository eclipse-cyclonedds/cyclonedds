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
#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <stdbool.h>
#include <stdint.h>

#include "idl/export.h"

/** @private */
typedef struct idl_intval idl_intval_t;
struct idl_intval {
  bool negative;
  union {
    int64_t llng;
    uint64_t ullng;
  } value;
};

/** @private */
IDL_EXPORT idl_retcode_t
idl_eval_int_expr(
  idl_processor_t *proc,
  idl_intval_t *val,
  const idl_const_expr_t *expr,
  idl_mask_t type);

#endif /* IDL_EXPRESSION_H */
