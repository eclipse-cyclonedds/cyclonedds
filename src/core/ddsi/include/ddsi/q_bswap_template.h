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

VDDS_INLINE unsigned short bswap2u (unsigned short x)
{
  return (unsigned short) ((x >> 8) | (x << 8));
}

VDDS_INLINE unsigned bswap4u (unsigned x)
{
  return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}

VDDS_INLINE unsigned long long bswap8u (unsigned long long x)
{
  const unsigned newhi = bswap4u ((unsigned) x);
  const unsigned newlo = bswap4u ((unsigned) (x >> 32));
  return ((unsigned long long) newhi << 32) | (unsigned long long) newlo;
}

VDDS_INLINE void bswapSN (nn_sequence_number_t *sn)
{
  sn->high = bswap4 (sn->high);
  sn->low = bswap4u (sn->low);
}

