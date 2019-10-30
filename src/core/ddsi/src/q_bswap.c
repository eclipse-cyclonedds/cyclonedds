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
#include "dds/ddsi/q_bswap.h"

extern inline void bswapSN (nn_sequence_number_t *sn);

ddsi_guid_prefix_t nn_hton_guid_prefix (ddsi_guid_prefix_t p)
{
  int i;
  for (i = 0; i < 3; i++)
    p.u[i] = ddsrt_toBE4u (p.u[i]);
  return p;
}

ddsi_guid_prefix_t nn_ntoh_guid_prefix (ddsi_guid_prefix_t p)
{
  int i;
  for (i = 0; i < 3; i++)
    p.u[i] = ddsrt_fromBE4u (p.u[i]);
  return p;
}

ddsi_entityid_t nn_hton_entityid (ddsi_entityid_t e)
{
  e.u = ddsrt_toBE4u (e.u);
  return e;
}

ddsi_entityid_t nn_ntoh_entityid (ddsi_entityid_t e)
{
  e.u = ddsrt_fromBE4u (e.u);
  return e;
}

ddsi_guid_t nn_hton_guid (ddsi_guid_t g)
{
  g.prefix = nn_hton_guid_prefix (g.prefix);
  g.entityid = nn_hton_entityid (g.entityid);
  return g;
}

ddsi_guid_t nn_ntoh_guid (ddsi_guid_t g)
{
  g.prefix = nn_ntoh_guid_prefix (g.prefix);
  g.entityid = nn_ntoh_entityid (g.entityid);
  return g;
}

void bswap_sequence_number_set_hdr (nn_sequence_number_set_header_t *snset)
{
  bswapSN (&snset->bitmap_base);
  snset->numbits = ddsrt_bswap4u (snset->numbits);
}

void bswap_sequence_number_set_bitmap (nn_sequence_number_set_header_t *snset, uint32_t *bits)
{
  const uint32_t n = (snset->numbits + 31) / 32;
  for (uint32_t i = 0; i < n; i++)
    bits[i] = ddsrt_bswap4u (bits[i]);
}

void bswap_fragment_number_set_hdr (nn_fragment_number_set_header_t *fnset)
{
  fnset->bitmap_base = ddsrt_bswap4u (fnset->bitmap_base);
  fnset->numbits = ddsrt_bswap4u (fnset->numbits);
}

void bswap_fragment_number_set_bitmap (nn_fragment_number_set_header_t *fnset, uint32_t *bits)
{
  const uint32_t n = (fnset->numbits + 31) / 32;
  for (uint32_t i = 0; i < n; i++)
    bits[i] = ddsrt_bswap4u (bits[i]);
}

