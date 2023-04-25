// Copyright(c) 2019 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Theory.h"
#include "dds/ddsi/ddsi_ownip.h"
#include "ddsi__ipaddr.h"
#include "ddsi__tran.h"

CU_Test (ddsi_ipaddr, is_nearby_address)
{
  struct ddsi_network_interface ifs[3] = {
    { .loc      = { .kind = DDSI_LOCATOR_KIND_UDPv4, .port = 0, .address = { [12] = 192, [13] = 168, [14] = 1, [15] = 70 } },
      .netmask  = { .kind = DDSI_LOCATOR_KIND_UDPv4, .port = 0, .address = { [12] = 255, [13] = 255, [14] = 0, [15] = 0 } } },
    { .loc      = { .kind = DDSI_LOCATOR_KIND_UDPv4, .port = 0, .address = { [12] = 192, [13] = 168, [14] = 2, [15] = 70 } },
      .netmask  = { .kind = DDSI_LOCATOR_KIND_UDPv4, .port = 0, .address = { [12] = 255, [13] = 255, [14] = 255, [15] = 0 } } },
  };
  for (size_t i = 0; i < 2; i++)
    ifs[i].extloc = ifs[i].loc;
  ifs[2] = ifs[0]; // so we can easily check that the result is independent of interface ordering
  struct { enum ddsi_nearby_address_result res; size_t interf_index; struct ddsi_locator loc; } locs[] = {
    { DNAR_SELF, 0, { .kind = DDSI_LOCATOR_KIND_UDPv4, .port = 0, .address = { [12] = 192, [13] = 168, [14] = 1, [15] = 70 } } },
    { DNAR_SELF, 1, { .kind = DDSI_LOCATOR_KIND_UDPv4, .port = 0, .address = { [12] = 192, [13] = 168, [14] = 2, [15] = 70 } } },
    { DNAR_LOCAL, 0, { .kind = DDSI_LOCATOR_KIND_UDPv4, .port = 0, .address = { [12] = 192, [13] = 168, [14] = 1, [15] = 80 } } },
    { DNAR_LOCAL, 1, { .kind = DDSI_LOCATOR_KIND_UDPv4, .port = 0, .address = { [12] = 192, [13] = 168, [14] = 2, [15] = 80 } } },
    { DNAR_DISTANT, 0, { .kind = DDSI_LOCATOR_KIND_UDPv4, .port = 0, .address = { [12] = 10, [13] = 10, [14] = 10, [15] = 10 } } },
    { DNAR_UNREACHABLE, 1, { .kind = DDSI_LOCATOR_KIND_UDPv6 } }
  };
  for (int ifs_offset = 0; ifs_offset < 2; ifs_offset++)
  {
    for (size_t i = 0; i < sizeof (locs) / sizeof (locs[0]); i++)
    {
      bool ok = true;
      size_t interf_index;
      enum ddsi_nearby_address_result res;
      res = ddsi_ipaddr_is_nearby_address(&locs[i].loc, 2, ifs + ifs_offset, &interf_index);
      printf ("off %d i %zu res %d", ifs_offset, i, (int) res);
      if (res != locs[i].res)
        ok = false;
      CU_ASSERT (res == locs[i].res);
      if (res == DNAR_SELF || res == DNAR_LOCAL)
      {
        printf (" index %zu", interf_index);
        if (ifs_offset == 1) // adjust for reversed interface order
          locs[i].interf_index = 1 - locs[i].interf_index;
        if (interf_index != locs[i].interf_index)
          ok = false;
        CU_ASSERT (interf_index == locs[i].interf_index);
      }
      printf (" (%s)\n", ok ? "ok" : "nok");
    }
  }
}
