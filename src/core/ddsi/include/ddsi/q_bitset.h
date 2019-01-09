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
#ifndef NN_BITSET_H
#define NN_BITSET_H

#include <assert.h>
#include <string.h>

#include "ddsi/q_unused.h"

inline int nn_bitset_isset (unsigned numbits, const unsigned *bits, unsigned idx)
{
  return idx < numbits && (bits[idx/32] & (1u << (31 - (idx%32))));
}

inline void nn_bitset_set (UNUSED_ARG_NDEBUG (unsigned numbits), unsigned *bits, unsigned idx)
{
  assert (idx < numbits);
  bits[idx/32] |= 1u << (31 - (idx%32));
}

inline void nn_bitset_clear (UNUSED_ARG_NDEBUG (unsigned numbits), unsigned *bits, unsigned idx)
{
  assert (idx < numbits);
  bits[idx/32] &= ~(1u << (31 - (idx%32)));
}

inline void nn_bitset_zero (unsigned numbits, unsigned *bits)
{
  memset (bits, 0, 4 * ((numbits + 31) / 32));
}

inline void nn_bitset_one (unsigned numbits, unsigned *bits)
{
  memset (bits, 0xff, 4 * ((numbits + 31) / 32));

  /* clear bits "accidentally" set */
  {
    const unsigned k = numbits / 32;
    const unsigned n = numbits % 32;
    bits[k] &= ~(~0u >> n);
  }
}

#endif /* NN_BITSET_H */
