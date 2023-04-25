// Copyright(c) 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef DDSRT_BITS_H
#define DDSRT_BITS_H

#include <stdint.h>
#include <stdlib.h>

#include "dds/export.h"
#include "dds/ddsrt/static_assert.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** \brief Find first set (POSIX style): returns index of least significant bit set in input

    @param[in] x input, may be 0
    @return position of least significant bit set in x (LSB is 1, MSB is 32), returns 0 if x == 0
 */
DDS_INLINE_EXPORT inline uint32_t ddsrt_ffs32u (uint32_t x)
{
  // Compiler support checks based on https://en.wikipedia.org/wiki/Find_first_set
#if defined __clang__ && __clang_major__ >= 5
  DDSRT_STATIC_ASSERT (sizeof (int) == sizeof (uint32_t));
  return (uint32_t) __builtin_ffs ((int) x);
#elif defined __GNUC__ && (__GNUC__ > 3 || __GNUC__ == 3 && __GNUC_MINOR__ >= 4)
  DDSRT_STATIC_ASSERT (sizeof (int) == sizeof (uint32_t));
  return (uint32_t) __builtin_ffs ((int) x);
#elif defined _MSC_VER && _MSC_VER >= 1400 // Visual Studio 2005
  DDSRT_STATIC_ASSERT (sizeof (unsigned long) == sizeof (uint32_t));
  unsigned long index;
  return _BitScanForward (&index, x) ? (index + 1) : 0;
#else
  if (x == 0)
    return 0;
  uint32_t n = 1;
  if ((x & 0x0000FFFF) == 0) { n += 16; x >>= 16; };
  if ((x & 0x000000FF) == 0) { n +=  8; x >>=  8; };
  if ((x & 0x0000000F) == 0) { n +=  4; x >>=  4; };
  if ((x & 0x00000003) == 0) { n +=  2; x >>=  2; };
  if ((x & 0x00000001) == 0) { n +=  1; };
  return n;
#endif
}

#if defined (__cplusplus)
}
#endif

#endif /* DDSRT_BITS_H */

