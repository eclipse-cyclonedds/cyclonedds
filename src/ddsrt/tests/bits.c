// Copyright(c) 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include "CUnit/Test.h"

#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/bits.h"
#include "dds/ddsrt/random.h"

CU_Init(ddsrt_bits)
{
  ddsrt_init();
  return 0;
}

CU_Clean(ddsrt_bits)
{
  ddsrt_fini();
  return 0;
}

CU_Test(ddsrt_bits, ffs32u)
{
  // trivial cases: 0 and just 1 bit set in each possible position
  CU_ASSERT (ddsrt_ffs32u (0) == 0);
  int onebit_ok = 0;
  for (uint32_t i = 0; i < 32; i++)
    onebit_ok += ddsrt_ffs32u ((uint32_t)1 << i) == i + 1;
  CU_ASSERT (onebit_ok == 32);

  // all combinations of two bits
  int twobit_ok = 0;
  for (uint32_t i = 0; i < 31; i++)
    for (uint32_t j = i+1; j < 32; j++)
      twobit_ok += ddsrt_ffs32u (((uint32_t)1 << i) | ((uint32_t)1 << j)) == i + 1;
  CU_ASSERT (twobit_ok == 32*31/2);

  // random junk above the least significant bit set
  int junk_ok = 0;
  for (uint32_t i = 0; i < 31; i++)
    for (uint32_t j = 0; j < 1000; j++)
      junk_ok += ddsrt_ffs32u (((uint32_t)1 << i) | (ddsrt_random () << (i+1))) == i + 1;
  CU_ASSERT (junk_ok == 31 * 1000);
}
