/*
 * Copyright(c) 2026 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#ifndef DYNTYPELIB_FLOAT128_IO_H
#define DYNTYPELIB_FLOAT128_IO_H

#include <stdbool.h>
#include <stddef.h>

#include "dds/ddsrt/retcode.h"

#if defined (__cplusplus)
extern "C" {
#endif

#define DTL_FLOAT128_STRING_BUFSZ 64

typedef struct dtl_float128 {
  unsigned char x[16];
} dtl_float128_t;

bool dtl_float128_from_string (const char *str, dtl_float128_t *out);
bool dtl_float32_from_string (const char *str, float *out);
bool dtl_float64_from_string (const char *str, double *out);
void dtl_float128_from_float32 (dtl_float128_t *out, float v);
void dtl_float128_from_float64 (dtl_float128_t *out, double v);
dds_return_t dtl_float32_to_string (char *buf, size_t bufsz, float v);
dds_return_t dtl_float64_to_string (char *buf, size_t bufsz, double v);
dds_return_t dtl_float128_to_string (char *buf, size_t bufsz, const dtl_float128_t *in);

#if defined (__cplusplus)
}
#endif

#endif /* DYNTYPELIB_FLOAT128_IO_H */
