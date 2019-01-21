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
#include "ddsi/q_bswap.h"

extern inline uint16_t bswap2u (uint16_t x);
extern inline uint32_t bswap4u (uint32_t x);
extern inline uint64_t bswap8u (uint64_t x);
extern inline int16_t bswap2 (int16_t x);
extern inline int32_t bswap4 (int32_t x);
extern inline int64_t bswap8 (int64_t x);
extern inline void bswapSN (nn_sequence_number_t *sn);

