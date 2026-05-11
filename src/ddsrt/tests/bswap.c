// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdint.h>

#include "CUnit/Test.h"
#include "dds/ddsrt/bswap.h"

static void assert_uint128_eq (ddsrt_uint128_t x, uint64_t h, uint64_t l)
{
  CU_ASSERT_EQ (x.h, h);
  CU_ASSERT_EQ (x.l, l);
}

static void assert_int128_eq (ddsrt_int128_t x, int64_t h, uint64_t l)
{
  CU_ASSERT_EQ (x.h, h);
  CU_ASSERT_EQ (x.l, l);
}

CU_Test(ddsrt_bswap, integers)
{
  const ddsrt_uint128_t u128 = {
    .h = UINT64_C (0x0123456789abcdef),
    .l = UINT64_C (0xfedcba9876543210)
  };
  const ddsrt_int128_t i128 = {
    .h = (int64_t) UINT64_C (0x89abcdef01234567),
    .l = UINT64_C (0x0123456789abcdef)
  };

  CU_ASSERT_EQ (ddsrt_bswap2u (UINT16_C (0x1234)), UINT16_C (0x3412));
  CU_ASSERT_EQ ((uint16_t) ddsrt_bswap2 ((int16_t) UINT16_C (0x9234)), UINT16_C (0x3492));
  CU_ASSERT_EQ (ddsrt_bswap4u (UINT32_C (0x12345678)), UINT32_C (0x78563412));
  CU_ASSERT_EQ ((uint32_t) ddsrt_bswap4 ((int32_t) UINT32_C (0x92345678)), UINT32_C (0x78563492));
  CU_ASSERT_EQ (ddsrt_bswap8u (UINT64_C (0x0123456789abcdef)), UINT64_C (0xefcdab8967452301));
  CU_ASSERT_EQ ((uint64_t) ddsrt_bswap8 ((int64_t) UINT64_C (0x8123456789abcdef)), UINT64_C (0xefcdab8967452381));

  assert_uint128_eq (ddsrt_bswap16u (u128), UINT64_C (0x1032547698badcfe), UINT64_C (0xefcdab8967452301));
  assert_int128_eq (ddsrt_bswap16 (i128), (int64_t) UINT64_C (0xefcdab8967452301), UINT64_C (0x67452301efcdab89));
}

CU_Test(ddsrt_bswap, endian_macros)
{
  const uint32_t x = UINT32_C (0x12345678);
  const ddsrt_uint128_t u128 = {
    .h = UINT64_C (0x0123456789abcdef),
    .l = UINT64_C (0xfedcba9876543210)
  };

#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
  const ddsrt_uint128_t u128_be = {
    .h = UINT64_C (0x1032547698badcfe),
    .l = UINT64_C (0xefcdab8967452301)
  };

  CU_ASSERT_EQ (ddsrt_toBE2u (UINT16_C (0x1234)), UINT16_C (0x3412));
  CU_ASSERT_EQ (ddsrt_toBE4u (x), UINT32_C (0x78563412));
  CU_ASSERT_EQ (ddsrt_toBE8u (UINT64_C (0x0123456789abcdef)), UINT64_C (0xefcdab8967452301));
  assert_uint128_eq (ddsrt_toBE16u (u128), UINT64_C (0x1032547698badcfe), UINT64_C (0xefcdab8967452301));
  CU_ASSERT_EQ (ddsrt_toLE4u (x), x);
  CU_ASSERT_EQ (ddsrt_fromBE4u (UINT32_C (0x78563412)), x);
  assert_uint128_eq (ddsrt_fromBE16u (u128_be), UINT64_C (0x0123456789abcdef), UINT64_C (0xfedcba9876543210));
  CU_ASSERT_EQ (ddsrt_toBO4u (DDSRT_BOSEL_NATIVE, x), x);
  CU_ASSERT_EQ (ddsrt_toBO4u (DDSRT_BOSEL_LE, x), x);
  CU_ASSERT_EQ (ddsrt_toBO4u (DDSRT_BOSEL_BE, x), UINT32_C (0x78563412));
#else
  CU_ASSERT_EQ (ddsrt_toBE2u (UINT16_C (0x1234)), UINT16_C (0x1234));
  CU_ASSERT_EQ (ddsrt_toBE4u (x), x);
  CU_ASSERT_EQ (ddsrt_toBE8u (UINT64_C (0x0123456789abcdef)), UINT64_C (0x0123456789abcdef));
  assert_uint128_eq (ddsrt_toBE16u (u128), UINT64_C (0x0123456789abcdef), UINT64_C (0xfedcba9876543210));
  CU_ASSERT_EQ (ddsrt_toLE4u (x), UINT32_C (0x78563412));
  CU_ASSERT_EQ (ddsrt_fromBE4u (x), x);
  assert_uint128_eq (ddsrt_fromBE16u (u128), UINT64_C (0x0123456789abcdef), UINT64_C (0xfedcba9876543210));
  CU_ASSERT_EQ (ddsrt_toBO4u (DDSRT_BOSEL_NATIVE, x), x);
  CU_ASSERT_EQ (ddsrt_toBO4u (DDSRT_BOSEL_BE, x), x);
  CU_ASSERT_EQ (ddsrt_toBO4u (DDSRT_BOSEL_LE, x), UINT32_C (0x78563412));
#endif

  CU_ASSERT_EQ ((uint16_t) ddsrt_toBE2 ((int16_t) UINT16_C (0x1234)), ddsrt_toBE2u (UINT16_C (0x1234)));
  CU_ASSERT_EQ ((uint32_t) ddsrt_toBE4 ((int32_t) x), ddsrt_toBE4u (x));
  CU_ASSERT_EQ ((uint64_t) ddsrt_toBE8 ((int64_t) UINT64_C (0x0123456789abcdef)), ddsrt_toBE8u (UINT64_C (0x0123456789abcdef)));
}
