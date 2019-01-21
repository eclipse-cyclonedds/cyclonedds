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
#ifndef NN_BSWAP_H
#define NN_BSWAP_H

#include "os/os.h"

#include "ddsi/q_rtps.h" /* for nn_guid_t, nn_guid_prefix_t */
#include "ddsi/q_protocol.h" /* for nn_sequence_number_t */

inline uint16_t bswap2u (uint16_t x)
{
  return (uint16_t) ((x >> 8) | (x << 8));
}

inline int16_t bswap2 (int16_t x)
{
  return (int16_t) bswap2u ((uint16_t) x);
}

inline uint32_t bswap4u (uint32_t x)
{
  return (x >> 24) | ((x >> 8) & 0xff00) | ((x << 8) & 0xff0000) | (x << 24);
}

inline int32_t bswap4 (int32_t x)
{
  return (int32_t) bswap4u ((uint32_t) x);
}

inline uint64_t bswap8u (uint64_t x)
{
  const uint32_t newhi = bswap4u ((uint32_t) x);
  const uint32_t newlo = bswap4u ((uint32_t) (x >> 32));
  return ((uint64_t) newhi << 32) | (uint64_t) newlo;
}

inline int64_t bswap8 (int64_t x)
{
  return (int64_t) bswap8u ((uint64_t) x);
}

inline void bswapSN (nn_sequence_number_t *sn)
{
  sn->high = bswap4 (sn->high);
  sn->low = bswap4u (sn->low);
}

#if OS_ENDIANNESS == OS_LITTLE_ENDIAN
#define toBE2(x) bswap2 (x)
#define toBE2u(x) bswap2u (x)
#define toBE4(x) bswap4 (x)
#define toBE4u(x) bswap4u (x)
#define toBE8(x) bswap8 (x)
#define toBE8u(x) bswap8u (x)
#define fromBE2(x) bswap2 (x)
#define fromBE2u(x) bswap2u (x)
#define fromBE4(x) bswap4 (x)
#define fromBE4u(x) bswap4u (x)
#define fromBE8(x) bswap8 (x)
#define fromBE8u(x) bswap8u (x)
#else
#define toBE2u(x) (x)
#define toBE4(x) (x)
#define toBE4u(x) (x)
#define toBE8(x) (x)
#define toBE8u(x) (x)
#define fromBE2(x) (x)
#define fromBE2u(x) (x)
#define fromBE4(x) (x)
#define fromBE4u(x) (x)
#define fromBE8(x) (x)
#define fromBE8u(x) (x)
#endif

#if defined (__cplusplus)
extern "C" {
#endif

nn_guid_prefix_t nn_hton_guid_prefix (nn_guid_prefix_t p);
nn_guid_prefix_t nn_ntoh_guid_prefix (nn_guid_prefix_t p);
nn_entityid_t nn_hton_entityid (nn_entityid_t e);
nn_entityid_t nn_ntoh_entityid (nn_entityid_t e);
nn_guid_t nn_hton_guid (nn_guid_t g);
nn_guid_t nn_ntoh_guid (nn_guid_t g);

void bswap_sequence_number_set_hdr (nn_sequence_number_set_t *snset);
void bswap_sequence_number_set_bitmap (nn_sequence_number_set_t *snset);
void bswap_fragment_number_set_hdr (nn_fragment_number_set_t *fnset);
void bswap_fragment_number_set_bitmap (nn_fragment_number_set_t *fnset);

#if defined (__cplusplus)
}
#endif

#endif /* NN_BSWAP_H */
