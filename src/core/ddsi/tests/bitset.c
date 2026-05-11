// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Test.h"
#include "ddsi__bitset.h"

CU_Test(ddsi_bitset, zero)
{
  uint32_t bits[] = {
    UINT32_C (0xffffffff),
    UINT32_C (0xffffffff),
    UINT32_C (0xaaaaaaaa)
  };

  ddsi_bitset_zero (33, bits);
  CU_ASSERT_EQ (bits[0], UINT32_C (0));
  CU_ASSERT_EQ (bits[1], UINT32_C (0));
  CU_ASSERT_EQ (bits[2], UINT32_C (0xaaaaaaaa));
}

CU_Test(ddsi_bitset, one)
{
  uint32_t bits[] = {
    UINT32_C (0),
    UINT32_C (0),
    UINT32_C (0),
    UINT32_C (0xaaaaaaaa)
  };

  ddsi_bitset_one (35, bits);
  CU_ASSERT_EQ (bits[0], UINT32_C (0xffffffff));
  CU_ASSERT_EQ (bits[1], UINT32_C (0xe0000000));
  CU_ASSERT_EQ (bits[2], UINT32_C (0));
  CU_ASSERT_EQ (bits[3], UINT32_C (0xaaaaaaaa));

  for (uint32_t i = 0; i < 35; i++)
    CU_ASSERT (ddsi_bitset_isset (35, bits, i));
  for (uint32_t i = 35; i < 64; i++)
    CU_ASSERT (!ddsi_bitset_isset (35, bits, i));
}

CU_Test(ddsi_bitset, set_clear_isset)
{
  uint32_t bits[] = {
    UINT32_C (0),
    UINT32_C (0),
    UINT32_C (0),
    UINT32_C (0xaaaaaaaa)
  };

  ddsi_bitset_set (65, bits, 0);
  ddsi_bitset_set (65, bits, 31);
  ddsi_bitset_set (65, bits, 32);
  ddsi_bitset_set (65, bits, 64);

  CU_ASSERT_EQ (bits[0], UINT32_C (0x80000001));
  CU_ASSERT_EQ (bits[1], UINT32_C (0x80000000));
  CU_ASSERT_EQ (bits[2], UINT32_C (0x80000000));
  CU_ASSERT_EQ (bits[3], UINT32_C (0xaaaaaaaa));
  CU_ASSERT (ddsi_bitset_isset (65, bits, 0));
  CU_ASSERT (ddsi_bitset_isset (65, bits, 31));
  CU_ASSERT (ddsi_bitset_isset (65, bits, 32));
  CU_ASSERT (ddsi_bitset_isset (65, bits, 64));
  CU_ASSERT (!ddsi_bitset_isset (65, bits, 1));
  CU_ASSERT (!ddsi_bitset_isset (65, bits, 63));
  CU_ASSERT (!ddsi_bitset_isset (65, bits, 65));

  ddsi_bitset_clear (65, bits, 31);
  ddsi_bitset_clear (65, bits, 64);

  CU_ASSERT_EQ (bits[0], UINT32_C (0x80000000));
  CU_ASSERT_EQ (bits[1], UINT32_C (0x80000000));
  CU_ASSERT_EQ (bits[2], UINT32_C (0));
  CU_ASSERT_EQ (bits[3], UINT32_C (0xaaaaaaaa));
  CU_ASSERT (ddsi_bitset_isset (65, bits, 0));
  CU_ASSERT (!ddsi_bitset_isset (65, bits, 31));
  CU_ASSERT (ddsi_bitset_isset (65, bits, 32));
  CU_ASSERT (!ddsi_bitset_isset (65, bits, 64));
}

CU_Test(ddsi_bitset, empty)
{
  uint32_t bits[] = {
    UINT32_C (0xaaaaaaaa)
  };

  ddsi_bitset_zero (0, bits);
  CU_ASSERT_EQ (bits[0], UINT32_C (0xaaaaaaaa));
  ddsi_bitset_one (0, bits);
  CU_ASSERT_EQ (bits[0], UINT32_C (0xaaaaaaaa));
  CU_ASSERT (!ddsi_bitset_isset (0, bits, 0));
}
