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
#include "ddsi__bswap.h"

CU_Test(ddsi_bswap, sequence_number)
{
  ddsi_sequence_number_t sn = {
    .high = (int32_t) UINT32_C (0x81234567),
    .low = UINT32_C (0x89abcdef)
  };

  ddsi_bswap_sequence_number (&sn);
  CU_ASSERT_EQ ((uint32_t) sn.high, UINT32_C (0x67452381));
  CU_ASSERT_EQ (sn.low, UINT32_C (0xefcdab89));
}

CU_Test(ddsi_bswap, sequence_number_set)
{
  ddsi_sequence_number_set_header_t snset = {
    .bitmap_base = {
      .high = (int32_t) UINT32_C (0x81234567),
      .low = UINT32_C (0x89abcdef)
    },
    .numbits = UINT32_C (65)
  };
  uint32_t bits[] = {
    UINT32_C (0x01234567),
    UINT32_C (0x89abcdef),
    UINT32_C (0xfedcba98)
  };

  ddsi_bswap_sequence_number_set_hdr (&snset);
  CU_ASSERT_EQ ((uint32_t) snset.bitmap_base.high, UINT32_C (0x67452381));
  CU_ASSERT_EQ (snset.bitmap_base.low, UINT32_C (0xefcdab89));
  CU_ASSERT_EQ (snset.numbits, UINT32_C (0x41000000));

  snset.numbits = 65;
  ddsi_bswap_sequence_number_set_bitmap (&snset, bits);
  CU_ASSERT_EQ (bits[0], UINT32_C (0x67452301));
  CU_ASSERT_EQ (bits[1], UINT32_C (0xefcdab89));
  CU_ASSERT_EQ (bits[2], UINT32_C (0x98badcfe));
}

CU_Test(ddsi_bswap, fragment_number_set)
{
  ddsi_fragment_number_set_header_t fnset = {
    .bitmap_base = UINT32_C (0x01234567),
    .numbits = UINT32_C (33)
  };
  uint32_t bits[] = {
    UINT32_C (0x89abcdef),
    UINT32_C (0xfedcba98)
  };

  ddsi_bswap_fragment_number_set_hdr (&fnset);
  CU_ASSERT_EQ (fnset.bitmap_base, UINT32_C (0x67452301));
  CU_ASSERT_EQ (fnset.numbits, UINT32_C (0x21000000));

  fnset.numbits = 33;
  ddsi_bswap_fragment_number_set_bitmap (&fnset, bits);
  CU_ASSERT_EQ (bits[0], UINT32_C (0xefcdab89));
  CU_ASSERT_EQ (bits[1], UINT32_C (0x98badcfe));
}
