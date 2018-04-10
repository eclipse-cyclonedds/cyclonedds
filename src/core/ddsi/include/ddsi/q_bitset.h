/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifndef NN_BITSET_H
#define NN_BITSET_H

#include <assert.h>
#include <string.h>

#include "ddsi/q_inline.h"

#if NN_HAVE_C99_INLINE && !defined SUPPRESS_BITSET_INLINES
#include "q_bitset_template.h"
#else
#if defined (__cplusplus)
extern "C" {
#endif
int nn_bitset_isset (unsigned numbits, const unsigned *bits, unsigned idx);
void nn_bitset_set (unsigned numbits, unsigned *bits, unsigned idx);
void nn_bitset_clear (unsigned numbits, unsigned *bits, unsigned idx);
void nn_bitset_zero (unsigned numbits, unsigned *bits);
void nn_bitset_one (unsigned numbits, unsigned *bits);
#if defined (__cplusplus)
}
#endif
#endif

#endif /* NN_BITSET_H */
