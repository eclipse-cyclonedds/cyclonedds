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
/* -*- c -*- */

#if defined SUPPRESS_BSWAP_INLINES && defined VDDS_INLINE
#undef VDDS_INLINE
#define VDDS_INLINE
#endif

VDDS_INLINE uint16_t bswap2u (uint16_t x)
{
  return (unsigned short) ((x >> 8) | (x << 8));
}

VDDS_INLINE uint32_t bswap4u (uint32_t x)
{
  return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}

VDDS_INLINE uint64_t bswap8u (uint64_t x)
{
  const uint32_t newhi = bswap4u ((uint32_t) x);
  const uint32_t newlo = bswap4u ((uint32_t) (x >> 32));
  return ((uint64_t) newhi << 32) | (uint64_t) newlo;
}

VDDS_INLINE void bswapSN (nn_sequence_number_t *sn)
{
  sn->high = bswap4 (sn->high);
  sn->low = bswap4u (sn->low);
}

