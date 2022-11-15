/*
 * Copyright(c) 2019 to 2020 ZettaScale Technology and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */

#include "CUnit/Theory.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/endian.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsi/ddsi_init.h"
#include "ddsi__plist.h"
#include "ddsi__radmin.h"
#include "ddsi__xmsg.h"
#include "ddsi__vendor.h"
#include "mem_ser.h"

/* We already used "liveliness" for participant lease durations in the API
   (including the built-in topics), and now dropped the "participant lease
   duration" everywhere *except* in the (de)serialisation code.

   That means the discovery (de)serializer is now aware of whether it is
   processing SPDP or SEDP data, and now needs to ignore DDSI_PI_LIVELINESS
   for SPDP and ignore DDSI_PI_PARTICIPANT_LEASE_DURATION for SEDP, because
   they both map to the liveliness QoS setting.

   - Use big-endian for convenience, that way the parameter header is:

     (id << 16) | length

   and this test isn't concerned with byte-swapping anyway

   - lease duration is seconds, fraction
   - liveliness is kind, seconds, fraction */
#define HDR(id, len) SER32BE(((uint32_t)(id) << 16) | (uint32_t)(len))
#define SENTINEL     HDR(DDSI_PID_SENTINEL, 0)
#define RLD(s, f)    SER32BE((uint32_t)s), SER32BE((uint32_t)f)
#define RLL(k, s, f) SER32BE((uint32_t)(k)), RLD(s, f)
#define LD(s, f)     HDR(DDSI_PID_PARTICIPANT_LEASE_DURATION, 8), RLD(s, f)
#define LL(k, s, f)  HDR(DDSI_PID_LIVELINESS, 12), RLL(k, s, f)

struct plist_valid {
  bool valid;
  bool present;
  dds_liveliness_kind_t kind;
  dds_duration_t lease_duration;
};
struct plist_cdr {
  struct plist_valid exp[2]; // SPDP, SEDP
  unsigned char cdr[44];
};
static const struct plist_cdr plists[] = {
  { { { true, true, DDS_LIVELINESS_AUTOMATIC, 3071111111 },
      { true, false, (dds_liveliness_kind_t)0, 0 } },
    { LD(3,0x12345679), SENTINEL } },
  { { { true, false, (dds_liveliness_kind_t)0, 0 },
      { true, true, DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, 2041944443 } },
    { LL(1, 2,0x0abcdefb), SENTINEL } },
  { { { true, true, DDS_LIVELINESS_AUTOMATIC, 3071111111 },
      { true, true, DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, 2041944443 } },
    { LD(3,0x12345679), LL(1, 2,0x0abcdefb), SENTINEL } },
  { { { true, true, DDS_LIVELINESS_AUTOMATIC, 3071111111 },
      { true, true, DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, 2041944443 } },
    { LL(1, 2,0x0abcdefb), LD(3,0x12345679), SENTINEL } },
  { // invalid for SPDP because there are two copies of lease duration,
    // SEDP ignores those and so accepts it
    { { false, false, (dds_liveliness_kind_t)0, 0 },
      { true,  false, (dds_liveliness_kind_t)0, 0 } },
    { LD(3,0x12345679), LD(4,0x12345679), SENTINEL } },
  { // invalid for SEDP because there are two copies of liveliness,
    // SPDP ignores those and so accepts it
    { { true, false, (dds_liveliness_kind_t)0, 0 },
      { false, false, (dds_liveliness_kind_t)0, 0 } },
    { LL(1, 2,0x0abcdefb), LL(1, 5,0x0abcdefb), SENTINEL } },
};

// context table order must match use in plists[i].valid
static const enum ddsi_plist_context_kind contexts[] = {
  DDSI_PLIST_CONTEXT_PARTICIPANT,
  DDSI_PLIST_CONTEXT_ENDPOINT
};

static struct ddsi_domaingv gv;

static void setup (void)
{
  ddsi_thread_states_init ();
  ddsi_config_init_default (&gv.config);
  ddsi_config_prep (&gv, NULL);
  ddsi_init (&gv);
}

static void teardown (void)
{
  ddsi_fini (&gv);
  ddsi_thread_states_fini ();
}

CU_Test (ddsi_plist_leasedur, deser, .init = setup, .fini = teardown)
{
  for (size_t j = 0; j < sizeof (contexts) / sizeof (contexts[0]); j++)
  {
    for (size_t i = 0; i < sizeof (plists) / sizeof (plists[0]); i++)
    {
      ddsi_plist_src_t src = {
        .protocol_version = {2, 1},
        .vendorid = DDSI_VENDORID_ECLIPSE,
        .encoding = DDSI_RTPS_PL_CDR_BE,
        .buf = plists[i].cdr,
        .bufsz = sizeof (plists[i].cdr),
        .strict = true
      };
      ddsi_plist_t plist;
      dds_return_t ret;

      struct plist_valid const * const exp = &plists[i].exp[j];
      ret = ddsi_plist_init_frommsg (&plist, NULL, ~(uint64_t)0, ~(uint64_t)0, &src, &gv, contexts[j]);
      CU_ASSERT_FATAL ((ret == 0) == exp->valid);
      if (exp->valid)
      {
        CU_ASSERT_FATAL (plist.present == 0 && plist.aliased == 0);
        CU_ASSERT_FATAL (((plist.qos.present & DDSI_QP_LIVELINESS) != 0) == exp->present);
        CU_ASSERT_FATAL (plist.qos.aliased == 0);
        if (exp->present)
        {
          CU_ASSERT_FATAL (plist.qos.liveliness.kind == exp->kind);
          CU_ASSERT_FATAL (plist.qos.liveliness.lease_duration == exp->lease_duration);
        }
        ddsi_plist_fini (&plist);
      }
    }
  }
}

CU_Test (ddsi_plist_leasedur, ser_spdp, .init = setup, .fini = teardown)
{
  ddsi_guid_t guid;
  memset (&guid, 0, sizeof (guid));
  struct ddsi_xmsg *m = ddsi_xmsg_new (gv.xmsgpool, &guid, NULL, 64, DDSI_XMSG_KIND_DATA);
  CU_ASSERT_FATAL (m != NULL);
  struct ddsi_xmsg_marker marker;
  (void) ddsi_xmsg_append (m, &marker, 0);

  ddsi_plist_t plist;
  ddsi_plist_init_empty (&plist);
  plist.qos.present |= DDSI_QP_LIVELINESS;
  plist.qos.liveliness.kind = DDS_LIVELINESS_AUTOMATIC;
  plist.qos.liveliness.lease_duration = 3071111111;
  ddsi_plist_addtomsg_bo (m, &plist, 0, DDSI_QP_LIVELINESS, DDSRT_BOSEL_BE, DDSI_PLIST_CONTEXT_PARTICIPANT);
  ddsi_xmsg_addpar_sentinel_bo (m, DDSRT_BOSEL_BE);

  const uint8_t expected[] = { LD(3,0x12345679), SENTINEL };
  const unsigned char *cdr = ddsi_xmsg_submsg_from_marker (m, marker);
  CU_ASSERT (memcmp (expected, cdr, sizeof (expected)) == 0);

  ddsi_plist_fini (&plist);
  ddsi_xmsg_free (m);
}

CU_Test (ddsi_plist_leasedur, ser_sedp, .init = setup, .fini = teardown)
{
  ddsi_guid_t guid;
  memset (&guid, 0, sizeof (guid));
  struct ddsi_xmsg *m = ddsi_xmsg_new (gv.xmsgpool, &guid, NULL, 64, DDSI_XMSG_KIND_DATA);
  CU_ASSERT_FATAL (m != NULL);
  struct ddsi_xmsg_marker marker;
  (void) ddsi_xmsg_append (m, &marker, 0);

  ddsi_plist_t plist;
  ddsi_plist_init_empty (&plist);
  plist.qos.present |= DDSI_QP_LIVELINESS;
  plist.qos.liveliness.kind = DDS_LIVELINESS_MANUAL_BY_PARTICIPANT;
  plist.qos.liveliness.lease_duration = 2041944443;
  ddsi_plist_addtomsg_bo (m, &plist, 0, DDSI_QP_LIVELINESS, DDSRT_BOSEL_BE, DDSI_PLIST_CONTEXT_ENDPOINT);
  ddsi_xmsg_addpar_sentinel_bo (m, DDSRT_BOSEL_BE);

  const uint8_t expected[] = { LL(1, 2,0x0abcdefb), SENTINEL };
  const unsigned char *cdr = ddsi_xmsg_submsg_from_marker (m, marker);
  CU_ASSERT (memcmp (expected, cdr, sizeof (expected)) == 0);

  ddsi_plist_fini (&plist);
  ddsi_xmsg_free (m);
}
