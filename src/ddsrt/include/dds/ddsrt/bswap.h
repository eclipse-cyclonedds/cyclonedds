// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSRT_BSWAP_H
#define DDSRT_BSWAP_H

/** @file bswap.h
  This header implements byteswapping (between big endian and little endian) for different integer types.
*/

#include <stdint.h>
#include <stdlib.h>

#include "dds/export.h"
#include "dds/ddsrt/endian.h"

#if defined (__cplusplus)
extern "C" {
#endif

/**
 * @brief Defines the byte order options: native, big endian, and little endian
 */
enum ddsrt_byte_order_selector {
  DDSRT_BOSEL_NATIVE,
  DDSRT_BOSEL_BE,
  DDSRT_BOSEL_LE,
};

/**
 * @brief Byteswap uint16_t
 * 
 * @param[in] x the uint16_t to byteswap
 * @return the byteswapped uint16_t
 */
DDS_INLINE_EXPORT inline uint16_t ddsrt_bswap2u (uint16_t x)
{
  return (uint16_t) ((x >> 8) | (x << 8));
}

/**
 * @brief Byteswap int16_t
 * 
 * @param[in] x the int16_t to byteswap
 * @return the byteswapped int16_t
 */
DDS_INLINE_EXPORT inline int16_t ddsrt_bswap2 (int16_t x)
{
  return (int16_t) ddsrt_bswap2u ((uint16_t) x);
}

/**
 * @brief Byteswap uint32_t
 * 
 * @param[in] x the uint32_t to byteswap
 * @return the byteswapped uint32_t
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_bswap4u (uint32_t x)
{
  return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}

/**
 * @brief Byteswap int32_t
 * 
 * @param[in] x the int32_t to byteswap
 * @return the byteswapped int32_t
 */
DDS_INLINE_EXPORT inline int32_t ddsrt_bswap4 (int32_t x)
{
  return (int32_t) ddsrt_bswap4u ((uint32_t) x);
}

/**
 * @brief Byteswap uint64_t
 * 
 * @param[in] x the uint64_t to byteswap
 * @return the byteswapped uint64_t
 */
DDS_INLINE_EXPORT inline uint64_t ddsrt_bswap8u (uint64_t x)
{
  const uint32_t newhi = ddsrt_bswap4u ((uint32_t) x);
  const uint32_t newlo = ddsrt_bswap4u ((uint32_t) (x >> 32));
  return ((uint64_t) newhi << 32) | (uint64_t) newlo;
}

/**
 * @brief Byteswap int64_t
 * 
 * @param[in] x the int64_t to byteswap
 * @return the byteswapped int64_t
 */
DDS_INLINE_EXPORT inline int64_t ddsrt_bswap8 (int64_t x)
{
  return (int64_t) ddsrt_bswap8u ((uint64_t) x);
}

/**
 * @brief Macros for byteswapping
 * 
 * These macros are for converting integer types from and to big endian or little endian.
 * They abstract from the endianness of your system by providing a different implementation based on the value of DDSRT_ENDIAN
 * (for example if you want to convert to big endian, it converts if your system is little endian,
 * but if your system is already big endian, the conversion is skipped).
 * Postfix numbers indicate the number of bytes of the integer (2,4,8 for 16-bit, 32-bit, 64-bit respectively),
 * whilst an 'u' postfix indicates an unsigned integer type.
 * 
 * - ddsrt_toBE: convert from native to big endian
 * - ddsrt_toLE: convert from native to little endian
 * - ddsrt_toBO: convert from native to byte order 'bo', @see ddsrt_byte_order_selector
 * - ddsrt_fromBE: convert from big endian to native endianness (DDSRT_ENDIAN)
 */
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
#define ddsrt_toBE2(x) ddsrt_bswap2 (x)
#define ddsrt_toBE2u(x) ddsrt_bswap2u (x)
#define ddsrt_toBE4(x) ddsrt_bswap4 (x)
#define ddsrt_toBE4u(x) ddsrt_bswap4u (x)
#define ddsrt_toBE8(x) ddsrt_bswap8 (x)
#define ddsrt_toBE8u(x) ddsrt_bswap8u (x)
#define ddsrt_toLE2(x) (x)
#define ddsrt_toLE2u(x) (x)
#define ddsrt_toLE4(x) (x)
#define ddsrt_toLE4u(x) (x)
#define ddsrt_toLE8(x) (x)
#define ddsrt_toLE8u(x) (x)
#define ddsrt_toBO2(bo, x) ((bo) == DDSRT_BOSEL_BE ? ddsrt_bswap2 (x) : (x))
#define ddsrt_toBO2u(bo, x) ((bo) == DDSRT_BOSEL_BE ? ddsrt_bswap2u (x) : (x))
#define ddsrt_toBO4(bo, x) ((bo) == DDSRT_BOSEL_BE ? ddsrt_bswap4 (x) : (x))
#define ddsrt_toBO4u(bo, x) ((bo) == DDSRT_BOSEL_BE ? ddsrt_bswap4u (x) : (x))
#define ddsrt_toBO8(bo, x) ((bo) == DDSRT_BOSEL_BE ? ddsrt_bswap8 (x) : (x))
#define ddsrt_toBO8u(bo, x) ((bo) == DDSRT_BOSEL_BE ? ddsrt_bswap8u (x) : (x))
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
#define ddsrt_toLE2(x) ddsrt_bswap2 (x)
#define ddsrt_toLE2u(x) ddsrt_bswap2u (x)
#define ddsrt_toLE4(x) ddsrt_bswap4 (x)
#define ddsrt_toLE4u(x) ddsrt_bswap4u (x)
#define ddsrt_toLE8(x) ddsrt_bswap8 (x)
#define ddsrt_toLE8u(x) ddsrt_bswap8u (x)
#define ddsrt_toBO2(bo, x) ((bo) == DDSRT_BOSEL_LE ? ddsrt_bswap2 (x) : (x))
#define ddsrt_toBO2u(bo, x) ((bo) == DDSRT_BOSEL_LE ? ddsrt_bswap2u (x) : (x))
#define ddsrt_toBO4(bo, x) ((bo) == DDSRT_BOSEL_LE ? ddsrt_bswap4 (x) : (x))
#define ddsrt_toBO4u(bo, x) ((bo) == DDSRT_BOSEL_LE ? ddsrt_bswap4u (x) : (x))
#define ddsrt_toBO8(bo, x) ((bo) == DDSRT_BOSEL_LE ? ddsrt_bswap8 (x) : (x))
#define ddsrt_toBO8u(bo, x) ((bo) == DDSRT_BOSEL_LE ? ddsrt_bswap8u (x) : (x))
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

