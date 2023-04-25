// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSI__BITSET_H
#define DDSI__BITSET_H

#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "dds/export.h"
#include "dds/ddsi/ddsi_unused.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** @component bitset */
inline int ddsi_bitset_isset (uint32_t numbits, const uint32_t *bits, uint32_t idx)
{
  return idx < numbits && (bits[idx/32] & (UINT32_C(1) << (31 - (idx%32))));
}

/** @component bitset */
inline void ddsi_bitset_set (UNUSED_ARG_NDEBUG (uint32_t numbits), uint32_t *bits, uint32_t idx)
{
  assert (idx < numbits);
  bits[idx/32] |= UINT32_C(1) << (31 - (idx%32));
}

/** @component bitset */
inline void ddsi_bitset_clear (UNUSED_ARG_NDEBUG (uint32_t numbits), uint32_t *bits, uint32_t idx)
{
  assert (idx < numbits);
  bits[idx/32] &= ~(UINT32_C(1) << (31 - (idx%32)));
}

/** @component bitset */
inline void ddsi_bitset_zero (uint32_t numbits, uint32_t *bits)
{
  memset (bits, 0, 4 * ((numbits + 31) / 32));
}

/** @component bitset */
inline void ddsi_bitset_one (uint32_t numbits, uint32_t *bits)
{
  memset (bits, 0xff, 4 * ((numbits + 31) / 32));

  /* clear bits "accidentally" set */
  if ((numbits % 32) != 0)
  {
    const uint32_t k = numbits / 32;
    const uint32_t n = numbits % 32;
    bits[k] &= ~(~UINT32_C(0) >> n);
  }
}

#if defined (__cplusplus)
}
#endif

#endif /* DDSI__BITSET_H */
