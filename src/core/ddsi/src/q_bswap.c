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
#include "ddsi/q_bswap.h"

nn_guid_prefix_t nn_hton_guid_prefix (nn_guid_prefix_t p)
{
  int i;
  for (i = 0; i < 3; i++)
    p.u[i] = toBE4u (p.u[i]);
  return p;
}

nn_guid_prefix_t nn_ntoh_guid_prefix (nn_guid_prefix_t p)
{
  int i;
  for (i = 0; i < 3; i++)
    p.u[i] = fromBE4u (p.u[i]);
  return p;
}

nn_entityid_t nn_hton_entityid (nn_entityid_t e)
{
  e.u = toBE4u (e.u);
  return e;
}

nn_entityid_t nn_ntoh_entityid (nn_entityid_t e)
{
  e.u = fromBE4u (e.u);
  return e;
}

nn_guid_t nn_hton_guid (nn_guid_t g)
{
  g.prefix = nn_hton_guid_prefix (g.prefix);
  g.entityid = nn_hton_entityid (g.entityid);
  return g;
}

nn_guid_t nn_ntoh_guid (nn_guid_t g)
{
  g.prefix = nn_ntoh_guid_prefix (g.prefix);
  g.entityid = nn_ntoh_entityid (g.entityid);
  return g;
}

void bswap_sequence_number_set_hdr (nn_sequence_number_set_t *snset)
{
  bswapSN (&snset->bitmap_base);
  snset->numbits = bswap4u (snset->numbits);
}

void bswap_sequence_number_set_bitmap (nn_sequence_number_set_t *snset)
{
  unsigned i, n = (snset->numbits + 31) / 32;
  for (i = 0; i < n; i++)
    snset->bits[i] = bswap4u (snset->bits[i]);
}

void bswap_fragment_number_set_hdr (nn_fragment_number_set_t *fnset)
{
  fnset->bitmap_base = bswap4u (fnset->bitmap_base);
  fnset->numbits = bswap4u (fnset->numbits);
}

void bswap_fragment_number_set_bitmap (nn_fragment_number_set_t *fnset)
{
  unsigned i, n = (fnset->numbits + 31) / 32;
  for (i = 0; i < n; i++)
    fnset->bits[i] = bswap4u (fnset->bits[i]);
}
