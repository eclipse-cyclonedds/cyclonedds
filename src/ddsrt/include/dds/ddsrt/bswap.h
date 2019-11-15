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
#ifndef DDSRT_BSWAP_H
#define DDSRT_BSWAP_H

#include <stdint.h>
#include <stdlib.h>

#include "dds/ddsrt/endian.h"

#if defined (__cplusplus)
extern "C" {
#endif

inline uint16_t ddsrt_bswap2u (uint16_t x)
{
  return (uint16_t) ((x >> 8) | (x << 8));
}

inline int16_t ddsrt_bswap2 (int16_t x)
{
  return (int16_t) ddsrt_bswap2u ((uint16_t) x);
}

inline uint32_t ddsrt_bswap4u (uint32_t x)
{
  return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}

inline int32_t ddsrt_bswap4 (int32_t x)
{
  return (int32_t) ddsrt_bswap4u ((uint32_t) x);
}

inline uint64_t ddsrt_bswap8u (uint64_t x)
{
  const uint32_t newhi = ddsrt_bswap4u ((uint32_t) x);
  const uint32_t newlo = ddsrt_bswap4u ((uint32_t) (x >> 32));
  return ((uint64_t) newhi << 32) | (uint64_t) newlo;
}

inline int64_t ddsrt_bswap8 (int64_t x)
{
  return (int64_t) ddsrt_bswap8u ((uint64_t) x);
}

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
#define ddsrt_toBE2(x) ddsrt_bswap2 (x)
#define ddsrt_toBE2u(x) ddsrt_bswap2u (x)
#define ddsrt_toBE4(x) ddsrt_bswap4 (x)
#define ddsrt_toBE4u(x) ddsrt_bswap4u (x)
#define ddsrt_toBE8(x) ddsrt_bswap8 (x)
#define ddsrt_toBE8u(x) ddsrt_bswap8u (x)
#define ddsrt_fromBE2(x) ddsrt_bswap2 (x)
#define ddsrt_fromBE2u(x) ddsrt_bswap2u (x)
#define ddsrt_fromBE4(x) ddsrt_bswap4 (x)
#define ddsrt_fromBE4u(x) ddsrt_bswap4u (x)
#define ddsrt_fromBE8(x) ddsrt_bswap8 (x)
#define ddsrt_fromBE8u(x) ddsrt_bswap8u (x)
#else
#define ddsrt_toBE2u(x) (x)
#define ddsrt_toBE4(x) (x)
#define ddsrt_toBE4u(x) (x)
#define ddsrt_toBE8(x) (x)
#define ddsrt_toBE8u(x) (x)
#define ddsrt_fromBE2(x) (x)
#define ddsrt_fromBE2u(x) (x)
#define ddsrt_fromBE4(x) (x)
#define ddsrt_fromBE4u(x) (x)
#define ddsrt_fromBE8(x) (x)
#define ddsrt_fromBE8u(x) (x)
#endif

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_BSWAP_H */

