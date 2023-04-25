// Copyright(c) 2006 to 2019 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsrt/bswap.h"
#include "dds/ddsi/ddsi_guid.h"
#include "dds/ddsi/ddsi_entity.h"

ddsi_guid_prefix_t ddsi_hton_guid_prefix (ddsi_guid_prefix_t p)
{
  int i;
  for (i = 0; i < 3; i++)
    p.u[i] = ddsrt_toBE4u (p.u[i]);
  return p;
}

ddsi_guid_prefix_t ddsi_ntoh_guid_prefix (ddsi_guid_prefix_t p)
{
  int i;
  for (i = 0; i < 3; i++)
    p.u[i] = ddsrt_fromBE4u (p.u[i]);
  return p;
}

ddsi_guid_t ddsi_hton_guid (ddsi_guid_t g)
{
  g.prefix = ddsi_hton_guid_prefix (g.prefix);
  g.entityid = ddsi_hton_entityid (g.entityid);
  return g;
}

ddsi_guid_t ddsi_ntoh_guid (ddsi_guid_t g)
{
  g.prefix = ddsi_ntoh_guid_prefix (g.prefix);
  g.entityid = ddsi_ntoh_entityid (g.entityid);
  return g;
}

