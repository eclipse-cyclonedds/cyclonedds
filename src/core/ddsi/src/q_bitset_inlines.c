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
#include "dds/ddsi/q_bitset.h"

DDS_EXPORT extern inline int nn_bitset_isset (uint32_t numbits, const uint32_t *bits, uint32_t idx);
DDS_EXPORT extern inline void nn_bitset_set (uint32_t numbits, uint32_t *bits, uint32_t idx);
DDS_EXPORT extern inline void nn_bitset_clear (uint32_t numbits, uint32_t *bits, uint32_t idx);
DDS_EXPORT extern inline void nn_bitset_zero (uint32_t numbits, uint32_t *bits);
DDS_EXPORT extern inline void nn_bitset_one (uint32_t numbits, uint32_t *bits);

