// Copyright(c) 2019 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Theory.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/static_assert.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/features.h"

#include "ddsi__plist.h"
#include "ddsi__udp.h"
#include "ddsi__tcp.h"
#include "ddsi__tran.h"
#include "ddsi__vendor.h"

#include "mem_ser.h"

CU_Test (ddsi_plist, unalias_copy_merge)
{
  /* one int, one string and one string sequence covers most cases */
  ddsi_plist_t p0, p0memcpy;
  char *p0strs[7];
  ddsi_plist_init_empty (&p0);
  p0.qos.present = DDSI_QP_PARTITION;
  p0.qos.aliased = DDSI_QP_PARTITION;
  p0.qos.partition.n = 3;
  p0.qos.partition.strs = ddsrt_malloc (p0.qos.partition.n * sizeof (*p0.qos.partition.strs));
  p0strs[0] = p0.qos.partition.strs[0] = "aap";
  p0strs[1] = p0.qos.partition.strs[1] = "noot";
  p0strs[2] = p0.qos.partition.strs[2] = "mies";
#ifdef DDS_HAS_SECURITY
  p0.present |= PP_IDENTITY_TOKEN;
  p0.aliased |= PP_IDENTITY_TOKEN;
  p0.identity_token.class_id = "class_id";
  p0.identity_token.properties.n = 2;
  p0.identity_token.properties.props = ddsrt_malloc (p0.identity_token.properties.n * sizeof (*p0.identity_token.properties.props));
  p0.identity_token.properties.props[0].propagate = false;
  p0strs[3] = p0.identity_token.properties.props[0].name = "name0";
  p0strs[4] = p0.identity_token.properties.props[0].value = "value0";
  p0.identity_token.properties.props[1].propagate = true;
  p0strs[5] = p0.identity_token.properties.props[1].name = "name1";
  p0strs[6] = p0.identity_token.properties.props[1].value = "value1";
  p0.identity_token.binary_properties.n = 0;
  p0.identity_token.binary_properties.props = NULL;
#endif
  memcpy (&p0memcpy, &p0, sizeof (p0));

  /* manually alias one, so we can free it*/
  ddsi_plist_t p0alias;
  memcpy (&p0alias, &p0, sizeof (p0));
  p0alias.qos.partition.strs = ddsrt_memdup (p0alias.qos.partition.strs, p0.qos.partition.n * sizeof (*p0.qos.partition.strs));
#ifdef DDS_HAS_SECURITY
  p0alias.identity_token.properties.props = ddsrt_memdup (p0alias.identity_token.properties.props,
                                              p0.identity_token.properties.n * sizeof (*p0.identity_token.properties.props));
#endif
  ddsi_plist_fini (&p0alias);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[0], p0strs[0]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[1], p0strs[1]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[2], p0strs[2]);
#ifdef DDS_HAS_SECURITY
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[0].name,  p0strs[3]);
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[0].value, p0strs[4]);
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[1].name,  p0strs[5]);
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[1].value, p0strs[6]);
#endif

  /* copy an aliased one; the original must be unchanged, the copy unaliased */
  ddsi_plist_t p1;
  ddsi_plist_init_empty (&p1);
  ddsi_plist_copy (&p1, &p0);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT (p1.present == p0.present);
  CU_ASSERT (p1.aliased == 0);
  CU_ASSERT (p1.qos.present == p0.qos.present);
  CU_ASSERT (p1.qos.aliased == 0);
  CU_ASSERT (p1.qos.partition.n == p0.qos.partition.n);
  CU_ASSERT (p1.qos.partition.strs != p0.qos.partition.strs);
  CU_ASSERT (p1.qos.partition.strs[0] != p0.qos.partition.strs[0]);
  CU_ASSERT (p1.qos.partition.strs[1] != p0.qos.partition.strs[1]);
  CU_ASSERT (p1.qos.partition.strs[2] != p0.qos.partition.strs[2]);
  CU_ASSERT_STRING_EQUAL (p1.qos.partition.strs[0], p0.qos.partition.strs[0]);
  CU_ASSERT_STRING_EQUAL (p1.qos.partition.strs[1], p0.qos.partition.strs[1]);
  CU_ASSERT_STRING_EQUAL (p1.qos.partition.strs[2], p0.qos.partition.strs[2]);
#ifdef DDS_HAS_SECURITY
  CU_ASSERT (p1.identity_token.class_id != p0.identity_token.class_id);
  CU_ASSERT_STRING_EQUAL (p1.identity_token.class_id, p0.identity_token.class_id);
  CU_ASSERT (p1.identity_token.properties.n == p0.identity_token.properties.n);
  CU_ASSERT (p1.identity_token.properties.props != p0.identity_token.properties.props);
  CU_ASSERT (p1.identity_token.properties.props[0].name != p0.identity_token.properties.props[0].name);
  CU_ASSERT (p1.identity_token.properties.props[0].value != p0.identity_token.properties.props[0].value);
  CU_ASSERT (p1.identity_token.properties.props[0].propagate == p0.identity_token.properties.props[0].propagate);
  CU_ASSERT (p1.identity_token.properties.props[1].name != p0.identity_token.properties.props[1].name);
  CU_ASSERT (p1.identity_token.properties.props[1].value != p0.identity_token.properties.props[1].value);
  CU_ASSERT (p1.identity_token.properties.props[1].propagate == p0.identity_token.properties.props[1].propagate);
  CU_ASSERT_STRING_EQUAL (p1.identity_token.properties.props[0].name, p0.identity_token.properties.props[0].name);
  CU_ASSERT_STRING_EQUAL (p1.identity_token.properties.props[0].value, p0.identity_token.properties.props[0].value);
  CU_ASSERT_STRING_EQUAL (p1.identity_token.properties.props[1].name, p0.identity_token.properties.props[1].name);
  CU_ASSERT_STRING_EQUAL (p1.identity_token.properties.props[1].value, p0.identity_token.properties.props[1].value);
  CU_ASSERT (p1.identity_token.binary_properties.n == 0);
  CU_ASSERT (p1.identity_token.binary_properties.props == NULL);
#endif

  /* merge-in missing ones from an aliased copy: original must remain unchanged;
     existing ones should stay without touching "aliased" only new ones are
     added as unaliased ones */
  ddsi_plist_t p2, p2memcpy;
  ddsi_plist_init_empty (&p2);
  memcpy (&p2memcpy, &p2, sizeof (p2));
  ddsi_plist_mergein_missing (&p2, &p0, p0.present, p0.qos.present);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT (p2.present == p0.present);
  CU_ASSERT (p2.aliased == p2memcpy.aliased);
  CU_ASSERT (p2.qos.present == p0.qos.present);
  CU_ASSERT (p2.qos.aliased == p2memcpy.qos.aliased);
  CU_ASSERT (p2.qos.partition.n == p0.qos.partition.n);
  CU_ASSERT (p2.qos.partition.strs != p0.qos.partition.strs);
  CU_ASSERT (p2.qos.partition.strs[0] != p0.qos.partition.strs[0]);
  CU_ASSERT (p2.qos.partition.strs[1] != p0.qos.partition.strs[1]);
  CU_ASSERT (p2.qos.partition.strs[2] != p0.qos.partition.strs[2]);
  CU_ASSERT_STRING_EQUAL (p2.qos.partition.strs[0], p0.qos.partition.strs[0]);
  CU_ASSERT_STRING_EQUAL (p2.qos.partition.strs[1], p0.qos.partition.strs[1]);
  CU_ASSERT_STRING_EQUAL (p2.qos.partition.strs[2], p0.qos.partition.strs[2]);
#ifdef DDS_HAS_SECURITY
  CU_ASSERT (p2.identity_token.class_id != p0.identity_token.class_id);
  CU_ASSERT_STRING_EQUAL (p2.identity_token.class_id, p0.identity_token.class_id);
  CU_ASSERT (p2.identity_token.properties.n == p0.identity_token.properties.n);
  CU_ASSERT (p2.identity_token.properties.props != p0.identity_token.properties.props);
  CU_ASSERT (p2.identity_token.properties.props[0].name != p0.identity_token.properties.props[0].name);
  CU_ASSERT (p2.identity_token.properties.props[0].value != p0.identity_token.properties.props[0].value);
  CU_ASSERT (p2.identity_token.properties.props[0].propagate == p0.identity_token.properties.props[0].propagate);
  CU_ASSERT (p2.identity_token.properties.props[1].name != p0.identity_token.properties.props[1].name);
  CU_ASSERT (p2.identity_token.properties.props[1].value != p0.identity_token.properties.props[1].value);
  CU_ASSERT (p2.identity_token.properties.props[1].propagate == p0.identity_token.properties.props[1].propagate);
  CU_ASSERT_STRING_EQUAL (p2.identity_token.properties.props[0].name, p0.identity_token.properties.props[0].name);
  CU_ASSERT_STRING_EQUAL (p2.identity_token.properties.props[0].value, p0.identity_token.properties.props[0].value);
  CU_ASSERT_STRING_EQUAL (p2.identity_token.properties.props[1].name, p0.identity_token.properties.props[1].name);
  CU_ASSERT_STRING_EQUAL (p2.identity_token.properties.props[1].value, p0.identity_token.properties.props[1].value);
  CU_ASSERT (p2.identity_token.binary_properties.n == 0);
  CU_ASSERT (p2.identity_token.binary_properties.props == NULL);
#endif

  /* unalias of p0, partition.strs mustn't change, because it, unlike its elements, wasn't aliased */
  ddsi_plist_unalias (&p0);
  CU_ASSERT (p0.present == p0memcpy.present);
  CU_ASSERT (p0.aliased == 0);
  CU_ASSERT (p0.qos.present == p0memcpy.qos.present);
  CU_ASSERT (p0.qos.aliased == 0);
  CU_ASSERT (p0.qos.partition.n == p0memcpy.qos.partition.n);
  CU_ASSERT (p0.qos.partition.strs == p0memcpy.qos.partition.strs);
  CU_ASSERT (p0.qos.partition.strs[0] != p0strs[0]);
  CU_ASSERT (p0.qos.partition.strs[1] != p0strs[1]);
  CU_ASSERT (p0.qos.partition.strs[2] != p0strs[2]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[0], p0strs[0]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[1], p0strs[1]);
  CU_ASSERT_STRING_EQUAL (p0.qos.partition.strs[2], p0strs[2]);
#ifdef DDS_HAS_SECURITY
  CU_ASSERT (p0.identity_token.properties.props[0].name  != p0strs[3]);
  CU_ASSERT (p0.identity_token.properties.props[0].value != p0strs[4]);
  CU_ASSERT (p0.identity_token.properties.props[1].name  != p0strs[5]);
  CU_ASSERT (p0.identity_token.properties.props[1].value != p0strs[6]);
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[0].name,  p0strs[3]);
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[0].value, p0strs[4]);
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[1].name,  p0strs[5]);
  CU_ASSERT_STRING_EQUAL (p0.identity_token.properties.props[1].value, p0strs[6]);
#endif

  memcpy (&p0memcpy, &p0, sizeof (p0));

  /* copy an aliased one; the original must be unchanged, the copy unaliased */
  ddsi_plist_t p3;
  ddsi_plist_init_empty (&p3);
  ddsi_plist_copy (&p3, &p0);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT (p3.present == p0.present);
  CU_ASSERT (p3.aliased == 0);
  CU_ASSERT (p3.qos.present == p0.qos.present);
  CU_ASSERT (p3.qos.aliased == 0);
  CU_ASSERT (p3.qos.partition.n == p0.qos.partition.n);
  CU_ASSERT (p3.qos.partition.strs != p0.qos.partition.strs);
  CU_ASSERT (p3.qos.partition.strs[0] != p0.qos.partition.strs[0]);
  CU_ASSERT (p3.qos.partition.strs[1] != p0.qos.partition.strs[1]);
  CU_ASSERT (p3.qos.partition.strs[2] != p0.qos.partition.strs[2]);
  CU_ASSERT_STRING_EQUAL (p3.qos.partition.strs[0], p0.qos.partition.strs[0]);
  CU_ASSERT_STRING_EQUAL (p3.qos.partition.strs[1], p0.qos.partition.strs[1]);
  CU_ASSERT_STRING_EQUAL (p3.qos.partition.strs[2], p0.qos.partition.strs[2]);
#ifdef DDS_HAS_SECURITY
  CU_ASSERT (p3.identity_token.class_id != p0.identity_token.class_id);
  CU_ASSERT_STRING_EQUAL (p3.identity_token.class_id, p0.identity_token.class_id);
  CU_ASSERT (p3.identity_token.properties.n == p0.identity_token.properties.n);
  CU_ASSERT (p3.identity_token.properties.props != p0.identity_token.properties.props);
  CU_ASSERT (p3.identity_token.properties.props[0].name != p0.identity_token.properties.props[0].name);
  CU_ASSERT (p3.identity_token.properties.props[0].value != p0.identity_token.properties.props[0].value);
  CU_ASSERT (p3.identity_token.properties.props[0].propagate == p0.identity_token.properties.props[0].propagate);
  CU_ASSERT (p3.identity_token.properties.props[1].name != p0.identity_token.properties.props[1].name);
  CU_ASSERT (p3.identity_token.properties.props[1].value != p0.identity_token.properties.props[1].value);
  CU_ASSERT (p3.identity_token.properties.props[1].propagate == p0.identity_token.properties.props[1].propagate);
  CU_ASSERT_STRING_EQUAL (p3.identity_token.properties.props[0].name, p0.identity_token.properties.props[0].name);
  CU_ASSERT_STRING_EQUAL (p3.identity_token.properties.props[0].value, p0.identity_token.properties.props[0].value);
  CU_ASSERT_STRING_EQUAL (p3.identity_token.properties.props[1].name, p0.identity_token.properties.props[1].name);
  CU_ASSERT_STRING_EQUAL (p3.identity_token.properties.props[1].value, p0.identity_token.properties.props[1].value);
  CU_ASSERT (p3.identity_token.binary_properties.n == 0);
  CU_ASSERT (p3.identity_token.binary_properties.props == NULL);
#endif

  /* merge-in missing ones from an aliased copy: original must remain unchanged;
     existing ones should stay without touching "aliased" only new ones are
     added as unaliased ones */
  ddsi_plist_t p4, p4memcpy;
  ddsi_plist_init_empty (&p4);
  memcpy (&p4memcpy, &p4, sizeof (p4));
  ddsi_plist_mergein_missing (&p4, &p0, p0.present, p0.qos.present);
  CU_ASSERT (memcmp (&p0, &p0memcpy, sizeof (p0)) == 0);
  CU_ASSERT (p4.present == p0.present);
  CU_ASSERT (p4.aliased == p4memcpy.aliased);
  CU_ASSERT (p4.qos.present == p0.qos.present);
  CU_ASSERT (p4.qos.aliased == p4memcpy.qos.aliased);
  CU_ASSERT (p4.qos.partition.n == p0.qos.partition.n);
  CU_ASSERT (p4.qos.partition.strs != p0.qos.partition.strs);
  CU_ASSERT (p4.qos.partition.strs[0] != p0.qos.partition.strs[0]);
  CU_ASSERT (p4.qos.partition.strs[1] != p0.qos.partition.strs[1]);
  CU_ASSERT (p4.qos.partition.strs[2] != p0.qos.partition.strs[2]);
  CU_ASSERT_STRING_EQUAL (p4.qos.partition.strs[0], p0.qos.partition.strs[0]);
  CU_ASSERT_STRING_EQUAL (p4.qos.partition.strs[1], p0.qos.partition.strs[1]);
  CU_ASSERT_STRING_EQUAL (p4.qos.partition.strs[2], p0.qos.partition.strs[2]);
#ifdef DDS_HAS_SECURITY
  CU_ASSERT (p4.identity_token.class_id != p0.identity_token.class_id);
  CU_ASSERT_STRING_EQUAL (p4.identity_token.class_id, p0.identity_token.class_id);
  CU_ASSERT (p4.identity_token.properties.n == p0.identity_token.properties.n);
  CU_ASSERT (p4.identity_token.properties.props != p0.identity_token.properties.props);
  CU_ASSERT (p4.identity_token.properties.props[0].name != p0.identity_token.properties.props[0].name);
  CU_ASSERT (p4.identity_token.properties.props[0].value != p0.identity_token.properties.props[0].value);
  CU_ASSERT (p4.identity_token.properties.props[0].propagate == p0.identity_token.properties.props[0].propagate);
  CU_ASSERT (p4.identity_token.properties.props[1].name != p0.identity_token.properties.props[1].name);
  CU_ASSERT (p4.identity_token.properties.props[1].value != p0.identity_token.properties.props[1].value);
  CU_ASSERT (p4.identity_token.properties.props[1].propagate == p0.identity_token.properties.props[1].propagate);
  CU_ASSERT_STRING_EQUAL (p4.identity_token.properties.props[0].name, p0.identity_token.properties.props[0].name);
  CU_ASSERT_STRING_EQUAL (p4.identity_token.properties.props[0].value, p0.identity_token.properties.props[0].value);
  CU_ASSERT_STRING_EQUAL (p4.identity_token.properties.props[1].name, p0.identity_token.properties.props[1].name);
  CU_ASSERT_STRING_EQUAL (p4.identity_token.properties.props[1].value, p0.identity_token.properties.props[1].value);
  CU_ASSERT (p4.identity_token.binary_properties.n == 0);
  CU_ASSERT (p4.identity_token.binary_properties.props == NULL);
#endif

  ddsi_plist_fini (&p0);
  ddsi_plist_fini (&p1);
  ddsi_plist_fini (&p2);
  ddsi_plist_fini (&p3);
  ddsi_plist_fini (&p4);
}

#define UNKNOWN_KIND 0x33221133
DDSRT_STATIC_ASSERT(
  DDSI_LOCATOR_KIND_UDPv4 != UNKNOWN_KIND &&
  DDSI_LOCATOR_KIND_TCPv4 != UNKNOWN_KIND &&
  DDSI_LOCATOR_KIND_UDPv6 != UNKNOWN_KIND &&
  DDSI_LOCATOR_KIND_TCPv6 != UNKNOWN_KIND &&
  DDSI_LOCATOR_KIND_PSMX  != UNKNOWN_KIND &&
  DDSI_LOCATOR_KIND_UDPv4MCGEN != UNKNOWN_KIND &&
  DDSI_LOCATOR_KIND_RAWETH != UNKNOWN_KIND &&
  DDSI_LOCATOR_KIND_INVALID != UNKNOWN_KIND);
DDSRT_STATIC_ASSERT((DDSI_LOCATOR_KIND_UDPv4 & DDSI_LOCATOR_KIND_TCPv4) == 0);

#define HDR(id, len) SER32BE(((uint32_t)(id) << 16) | (uint32_t)(len))

static void setup (struct ddsi_domaingv *gv, uint32_t factories)
{
  memset (gv, 0, sizeof (*gv));
  dds_log_cfg_init (&gv->logconfig, 0, DDS_LC_TRACE | DDS_LC_PLIST, stdout, stdout);
  if (factories & DDSI_LOCATOR_KIND_UDPv4)
  {
    (void) ddsi_udp_init (gv);
    struct ddsi_tran_factory * const udp = ddsi_factory_find (gv, "udp");
    CU_ASSERT_FATAL (udp != NULL);
    udp->m_enable = true;
  }
  if (factories & DDSI_LOCATOR_KIND_TCPv4)
  {
    (void) ddsi_tcp_init (gv);
    struct ddsi_tran_factory * const tcp = ddsi_factory_find (gv, "tcp");
    CU_ASSERT_FATAL (tcp != NULL);
    tcp->m_enable = true;
  }
}

static void teardown (struct ddsi_domaingv *gv)
{
  while (gv->ddsi_tran_factories)
  {
    struct ddsi_tran_factory *f = gv->ddsi_tran_factories;
    gv->ddsi_tran_factories = f->m_factory;
    ddsi_factory_free (f);
  }
}

CU_Test (ddsi_plist, locator_lists_reject)
{
  static const uint32_t enabled_kinds[] = {
    0,
    DDSI_LOCATOR_KIND_UDPv4,
    DDSI_LOCATOR_KIND_TCPv4,
    DDSI_LOCATOR_KIND_UDPv4 | DDSI_LOCATOR_KIND_TCPv4
  };
  uint32_t subtest = 0; // particularly useful for setting an ignore count on a breakpoint
  for (size_t enabled_kinds_idx = 0; enabled_kinds_idx < sizeof (enabled_kinds) / sizeof (enabled_kinds[0]); enabled_kinds_idx++)
  {
    const uint32_t factories = enabled_kinds[enabled_kinds_idx];
    struct ddsi_domaingv gv;
    setup (&gv, factories);
    // Do this for each of the "normal" locator parameters - they are handled by the
    // same code and should behave the same but check anyway
    static const uint32_t kinds[] = {
      DDSI_LOCATOR_KIND_UDPv4,
      DDSI_LOCATOR_KIND_TCPv4,
      UNKNOWN_KIND
    };
    for (size_t kindidx = 0; kindidx < sizeof (kinds) / sizeof (kinds[0]); kindidx++)
    {
      const uint32_t kind = kinds[kindidx];
      // plist to be patched (this one is valid)
      // @0...1: pid; @2...3: length; @4...7: kind, @8...11: port, @12...27: address
      enum reject_case { REJ_SHORT_20, REJ_SHORT_23, REJ_PORT_0, REJ_PORT_64k, REJ_GARBAGE } reject_case = REJ_SHORT_20;
      do {
        printf ("subtest %"PRIu32": enabled_kinds = %"PRIx32" kind = %"PRIx32" reject_case = %d\n",
                ++subtest, factories, kind, (int) reject_case);
        unsigned char plist_rej[] = {
          HDR(DDSI_PID_UNICAST_LOCATOR, 24), SER32BE(kind), SER32BE(1), 0,0,0,0, 0,0,0,0, 0,0,0,0, 127,0,0,1,
          HDR(DDSI_PID_SENTINEL, 0)
        };
        switch (reject_case)
        {
          case REJ_SHORT_20: plist_rej[3] = 20; break;
          case REJ_SHORT_23: plist_rej[3] = 23; break;
          case REJ_PORT_0:   plist_rej[11] = 0; break;
          case REJ_PORT_64k: plist_rej[9] = 1; plist_rej[10] = plist_rej[11] = 0; break;
          case REJ_GARBAGE:  plist_rej[12] = 1; break;
        }
        const ddsi_plist_src_t src = {
          .protocol_version = { DDSI_RTPS_MAJOR, DDSI_RTPS_MINOR },
          .vendorid = DDSI_VENDORID_ECLIPSE,
          .encoding = DDSI_RTPS_PL_CDR_BE,
          .buf = plist_rej,
          .bufsz = sizeof (plist_rej),
          .strict = false
        };
        char *nextafter = NULL;
        ddsi_plist_t plist;
        dds_return_t rc = ddsi_plist_init_frommsg (&plist, &nextafter, ~(uint64_t)0, ~(uint64_t)0, &src, &gv, DDSI_PLIST_CONTEXT_PARTICIPANT);
        if (reject_case <= REJ_SHORT_23) {
          // short/invalid lengths => always reject entire plist
          CU_ASSERT (rc == DDS_RETCODE_BAD_PARAMETER);
        } else if (kind != UNKNOWN_KIND && (factories & kind)) {
          // invalid content: only reject for known & enabled transports
          CU_ASSERT (rc == DDS_RETCODE_BAD_PARAMETER);
        } else {
          // ignored: locator kind unknown/not enabled, so plist should be accepted but the
          // result should be empty
          CU_ASSERT (rc == 0);
          CU_ASSERT ((unsigned char *) nextafter == plist_rej + sizeof (plist_rej));
          CU_ASSERT (plist.present == 0);
          ddsi_plist_fini (&plist);
        }
      } while (++reject_case <= REJ_GARBAGE);
    }
    teardown (&gv);
  }
}

#define UNKLOCATOR SER32BE(UNKNOWN_KIND), SER32BE(1), SER32BE(0), SER32BE(0), SER32BE(0), SER32BE(0)
#define UDPLOCATOR(a,b,c,d,port) \
  SER32BE(DDSI_LOCATOR_KIND_UDPv4), \
  SER32BE(port), \
  SER32BE(0),SER32BE(0),SER32BE(0), \
  (a),(b),(c),(d)
#define TCPLOCATOR(a,b,c,d,port) \
  SER32BE(DDSI_LOCATOR_KIND_TCPv4), \
  SER32BE(port), \
  SER32BE(0),SER32BE(0),SER32BE(0), \
  (a),(b),(c),(d)

CU_Test (ddsi_plist, locator_lists_accept)
{
  static const uint32_t enabled_kinds[] = {
    0,
    DDSI_LOCATOR_KIND_UDPv4,
    DDSI_LOCATOR_KIND_TCPv4,
    DDSI_LOCATOR_KIND_UDPv4 | DDSI_LOCATOR_KIND_TCPv4
  };
  for (size_t enabled_kinds_idx = 0; enabled_kinds_idx < sizeof (enabled_kinds) / sizeof (enabled_kinds[0]); enabled_kinds_idx++)
  {
    const uint32_t factories = enabled_kinds[enabled_kinds_idx];
    struct ddsi_domaingv gv;
    setup (&gv, factories);
    static const struct {
      uint16_t pid;
      uint64_t pp_flag;
      size_t pp_offset;
    } pids[] = {
      { DDSI_PID_UNICAST_LOCATOR, PP_UNICAST_LOCATOR, offsetof (ddsi_plist_t, unicast_locators) },
      { DDSI_PID_MULTICAST_LOCATOR, PP_MULTICAST_LOCATOR, offsetof (ddsi_plist_t, multicast_locators) },
      { DDSI_PID_DEFAULT_UNICAST_LOCATOR, PP_DEFAULT_UNICAST_LOCATOR, offsetof (ddsi_plist_t, default_unicast_locators) },
      { DDSI_PID_DEFAULT_MULTICAST_LOCATOR, PP_DEFAULT_MULTICAST_LOCATOR, offsetof (ddsi_plist_t, default_multicast_locators) },
      { DDSI_PID_METATRAFFIC_UNICAST_LOCATOR, PP_METATRAFFIC_UNICAST_LOCATOR, offsetof (ddsi_plist_t, metatraffic_unicast_locators) },
      { DDSI_PID_METATRAFFIC_MULTICAST_LOCATOR, PP_METATRAFFIC_MULTICAST_LOCATOR, offsetof (ddsi_plist_t, metatraffic_multicast_locators) }
    };
    for (size_t pididx = 0; pididx < sizeof (pids) / sizeof (pids[0]); pididx++)
    {
      const size_t pid1idx = (pididx == (sizeof (pids) / sizeof (pids[0]) - 1)) ? 0 : pididx + 1;
      const uint16_t pid = pids[pididx].pid;
      const uint16_t pid1 = pids[pid1idx].pid;
      const unsigned char plist_ok[] = {
        HDR (pid,  24), UDPLOCATOR (127,0,0, 1, 1), // accept if UDP enabled
        HDR (pid1, 24), UDPLOCATOR (127,0,0, 2, 1),
        HDR (pid,  24), TCPLOCATOR (127,0,0, 3, 1), // accept if TCP enabled
        HDR (pid1, 24), TCPLOCATOR (127,0,0, 4, 1),
        HDR (pid,  24), UDPLOCATOR (127,0,0, 5, 1), // accept if UDP enabled (for list construction)
        HDR (pid1, 24), UDPLOCATOR (127,0,0, 6, 1),
        HDR (pid,  24), TCPLOCATOR (127,0,0, 7, 1), // accept if TCP enabled (for list construction)
        HDR (pid1, 24), TCPLOCATOR (127,0,0, 8, 1),
        HDR (pid,  24), UNKLOCATOR,                 // ignore
        HDR (pid1, 24), UNKLOCATOR,
        HDR (pid,  24), UDPLOCATOR (127,0,0, 9, 1), // accept if UDP enabled (three is when it gets interesting)
        HDR (pid1, 24), UDPLOCATOR (127,0,0,10, 1),
        HDR (pid,  24), TCPLOCATOR (127,0,0,11, 1), // accept if TCP enabled (three is when it gets interesting)
        HDR (pid1, 24), TCPLOCATOR (127,0,0,12, 1),
        HDR(DDSI_PID_SENTINEL, 0)
      };
      const ddsi_plist_src_t src = {
        .protocol_version = { DDSI_RTPS_MAJOR, DDSI_RTPS_MINOR },
        .vendorid = DDSI_VENDORID_ECLIPSE,
        .encoding = DDSI_RTPS_PL_CDR_BE,
        .buf = plist_ok,
        .bufsz = sizeof (plist_ok),
        .strict = false
      };
      char *nextafter = NULL;
      ddsi_plist_t plist;
      dds_return_t rc = ddsi_plist_init_frommsg (&plist, &nextafter, ~(uint64_t)0, ~(uint64_t)0, &src, &gv, DDSI_PLIST_CONTEXT_PARTICIPANT);
      CU_ASSERT (rc == 0);
      CU_ASSERT ((unsigned char *) nextafter == plist_ok + sizeof (plist_ok));
      if (factories == 0)
      {
        // nothing enabled, nothing present
        CU_ASSERT (plist.present == 0);
      }
      else
      {
        // should have both lists present
        CU_ASSERT (plist.present & pids[pididx].pp_flag);
        CU_ASSERT (plist.present & pids[pid1idx].pp_flag);
        const struct ddsi_locators *lpid = (const struct ddsi_locators *) ((const char *) &plist + pids[pididx].pp_offset);
        const struct ddsi_locators *lpid1 = (const struct ddsi_locators *) ((const char *) &plist + pids[pid1idx].pp_offset);
        const struct ddsi_locators_one *l = lpid->first;
        const struct ddsi_locators_one *l1 = lpid1->first;
        for (size_t pi = 0; pi < sizeof (plist_ok) - 4; pi += 28)
        {
          uint16_t pi_pid;
          uint32_t pi_kind, pi_port;
          memcpy (&pi_pid, plist_ok + pi, sizeof (pi_pid));
          memcpy (&pi_kind, plist_ok + pi + 4, sizeof (pi_kind));
          memcpy (&pi_port, plist_ok + pi + 8, sizeof (pi_port));
          pi_pid = ddsrt_fromBE2u (pi_pid);
          pi_kind = ddsrt_fromBE4u (pi_kind);
          pi_port = ddsrt_fromBE4u (pi_port);

          if (pi_kind == UNKNOWN_KIND || !(factories & pi_kind))
            ; // skip ignored ones
          else
          {
            const struct ddsi_locators_one *lcmp;
            if (pi_pid == pid) {
              lcmp = l; l = l->next;
            } else {
              lcmp = l1; l1 = l1->next;
            }
            CU_ASSERT_FATAL (lcmp != NULL);
            CU_ASSERT ((uint32_t) lcmp->loc.kind == pi_kind);
            CU_ASSERT (lcmp->loc.port == pi_port);
            CU_ASSERT (memcmp (lcmp->loc.address, plist_ok + pi + 12, sizeof (lcmp->loc.address)) == 0);
          }
        }
        CU_ASSERT (l == NULL);
        CU_ASSERT (l1 == NULL);
      }
      ddsi_plist_fini (&plist);
    }
    teardown (&gv);
  }
}
