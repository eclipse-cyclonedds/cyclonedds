// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef NN_BSWAP_H
#define NN_BSWAP_H

#include <stdint.h>

#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/misc.h"
#include "ddsi__protocol.h"

#if defined (__cplusplus)
extern "C" {
#endif

/** @component ddsi_byteswap */
inline void ddsi_bswap_sequence_number (ddsi_sequence_number_t *sn)
{
  sn->high = ddsrt_bswap4 (sn->high);
  sn->low = ddsrt_bswap4u (sn->low);
}

/** @component ddsi_byteswap */
void ddsi_bswap_sequence_number_set_hdr (ddsi_sequence_number_set_header_t *snset);

/** @component ddsi_byteswap */
void ddsi_bswap_sequence_number_set_bitmap (ddsi_sequence_number_set_header_t *snset, uint32_t *bits);

/** @component ddsi_byteswap */
void ddsi_bswap_fragment_number_set_hdr (ddsi_fragment_number_set_header_t *fnset);

/** @component ddsi_byteswap */
void ddsi_bswap_fragment_number_set_bitmap (ddsi_fragment_number_set_header_t *fnset, uint32_t *bits);

#if defined (__cplusplus)
}
#endif

#endif /* NN_BSWAP_H */
