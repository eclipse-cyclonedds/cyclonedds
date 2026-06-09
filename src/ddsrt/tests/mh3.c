// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>

#include "CUnit/Test.h"
#include "dds/ddsrt/mh3.h"

CU_Test(ddsrt_mh3, unaligned)
{
  union {
    uint32_t words[4];
    unsigned char bytes[16];
  } aligned = {
    .words = { 0x00112233u, 0x44556677u, 0x8899aabbu, 0xccddeeffu }
  };
  unsigned char unaligned[17];

  memcpy (unaligned + 1, aligned.bytes, sizeof (aligned.bytes));

  CU_ASSERT_EQ_FATAL (
    ddsrt_mh3 (aligned.bytes, sizeof (aligned.bytes), 0x12345678u),
    ddsrt_mh3 (unaligned + 1, sizeof (aligned.bytes), 0x12345678u));
}
