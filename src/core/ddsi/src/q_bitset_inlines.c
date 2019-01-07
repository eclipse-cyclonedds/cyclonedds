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
#include "ddsi/q_bitset.h"

extern inline int nn_bitset_isset (unsigned numbits, const unsigned *bits, unsigned idx);
extern inline void nn_bitset_set (unsigned numbits, unsigned *bits, unsigned idx);
extern inline void nn_bitset_clear (unsigned numbits, unsigned *bits, unsigned idx);
extern inline void nn_bitset_zero (unsigned numbits, unsigned *bits);
extern inline void nn_bitset_one (unsigned numbits, unsigned *bits);

