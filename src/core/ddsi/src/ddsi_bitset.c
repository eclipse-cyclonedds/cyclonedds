// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "ddsi__bitset.h"

extern inline int ddsi_bitset_isset (uint32_t numbits, const uint32_t *bits, uint32_t idx);
extern inline void ddsi_bitset_set (uint32_t numbits, uint32_t *bits, uint32_t idx);
extern inline void ddsi_bitset_clear (uint32_t numbits, uint32_t *bits, uint32_t idx);
extern inline void ddsi_bitset_zero (uint32_t numbits, uint32_t *bits);
extern inline void ddsi_bitset_one (uint32_t numbits, uint32_t *bits);

