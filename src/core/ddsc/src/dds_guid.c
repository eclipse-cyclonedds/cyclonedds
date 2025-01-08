// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>

#include "dds/ddsrt/static_assert.h"
#include "dds__guid.h"

ddsi_guid_t dds_guid_to_ddsi_guid (dds_guid_t g)
{
  DDSRT_STATIC_ASSERT (sizeof (dds_guid_t) == sizeof (ddsi_guid_t));
  ddsi_guid_t gi;
  memcpy (&gi, &g, sizeof (gi));
  gi.prefix = ddsi_ntoh_guid_prefix (gi.prefix);
  gi.entityid = ddsi_ntoh_entityid (gi.entityid);
  return gi;
}

dds_guid_t dds_guid_from_ddsi_guid (ddsi_guid_t gi)
{
  DDSRT_STATIC_ASSERT (sizeof (dds_guid_t) == sizeof (ddsi_guid_t));
  gi.prefix = ddsi_hton_guid_prefix (gi.prefix);
  gi.entityid = ddsi_hton_entityid (gi.entityid);
  dds_guid_t g;
  memcpy (&g, &gi, sizeof (g));
  return g;
}
