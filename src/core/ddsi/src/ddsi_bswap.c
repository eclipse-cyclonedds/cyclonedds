// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsi/ddsi_protocol.h"
#include "ddsi__bswap.h"

extern inline void ddsi_bswap_sequence_number (ddsi_sequence_number_t *sn);

void ddsi_bswap_sequence_number_set_hdr (ddsi_sequence_number_set_header_t *snset)
{
  ddsi_bswap_sequence_number (&snset->bitmap_base);
  snset->numbits = ddsrt_bswap4u (snset->numbits);
}

void ddsi_bswap_sequence_number_set_bitmap (ddsi_sequence_number_set_header_t *snset, uint32_t *bits)
{
  const uint32_t n = (snset->numbits + 31) / 32;
  for (uint32_t i = 0; i < n; i++)
    bits[i] = ddsrt_bswap4u (bits[i]);
}

void ddsi_bswap_fragment_number_set_hdr (ddsi_fragment_number_set_header_t *fnset)
{
  fnset->bitmap_base = ddsrt_bswap4u (fnset->bitmap_base);
  fnset->numbits = ddsrt_bswap4u (fnset->numbits);
}

void ddsi_bswap_fragment_number_set_bitmap (ddsi_fragment_number_set_header_t *fnset, uint32_t *bits)
{
  const uint32_t n = (fnset->numbits + 31) / 32;
  for (uint32_t i = 0; i < n; i++)
    bits[i] = ddsrt_bswap4u (bits[i]);
}

