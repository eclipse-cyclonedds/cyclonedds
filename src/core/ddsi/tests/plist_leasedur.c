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
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsi/ddsi_init.h"
#include "ddsi__participant.h"
#include "ddsi__plist.h"
#include "ddsi__radmin.h"
#include "ddsi__xmsg.h"
#include "ddsi__vendor.h"
#include "ddsi__receive.h"
#include "ddsi__tran.h"
#include "ddsi__protocol.h"
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
  struct plist_valid spdp, others;
  unsigned char cdr[44];
};
static const struct plist_cdr plists[] = {
  { .spdp = { true, true, DDS_LIVELINESS_AUTOMATIC, 3071111111 },
    .others = { true, false, (dds_liveliness_kind_t)0, 0 },
    .cdr = { LD(3,0x12345679), SENTINEL } },
  { .spdp = { true, false, (dds_liveliness_kind_t)0, 0 },
    .others = { true, true, DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, 2041944443 },
    .cdr = { LL(1, 2,0x0abcdefb), SENTINEL } },
  { .spdp = { true, true, DDS_LIVELINESS_AUTOMATIC, 3071111111 },
    .others = { true, true, DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, 2041944443 },
    .cdr = { LD(3,0x12345679), LL(1, 2,0x0abcdefb), SENTINEL } },
  { .spdp = { true, true, DDS_LIVELINESS_AUTOMATIC, 3071111111 },
    .others = { true, true, DDS_LIVELINESS_MANUAL_BY_PARTICIPANT, 2041944443 },
    .cdr = { LL(1, 2,0x0abcdefb), LD(3,0x12345679), SENTINEL } },
  { // invalid for SPDP because there are two copies of lease duration,
    // SEDP ignores those and so accepts it
    .spdp = { false, false, (dds_liveliness_kind_t)0, 0 },
    .others = { true,  false, (dds_liveliness_kind_t)0, 0 },
    .cdr = { LD(3,0x12345679), LD(4,0x12345679), SENTINEL } },
  { // invalid for SEDP because there are two copies of liveliness,
    // SPDP ignores those and so accepts it
    .spdp = { true, false, (dds_liveliness_kind_t)0, 0 },
    .others = { false, false, (dds_liveliness_kind_t)0, 0 },
    .cdr = { LL(1, 2,0x0abcdefb), LL(1, 5,0x0abcdefb), SENTINEL } },
};

// context table order must match use in plists[i].valid
static const enum ddsi_plist_context_kind contexts[] = {
  DDSI_PLIST_CONTEXT_PARTICIPANT,
  DDSI_PLIST_CONTEXT_ENDPOINT,
  DDSI_PLIST_CONTEXT_TOPIC,
  DDSI_PLIST_CONTEXT_INLINE_QOS
};

static struct ddsi_cfgst *cfgst;
static struct ddsi_domaingv gv;
static struct ddsi_rbufpool *rbufpool;

static void setup (void)
{
  ddsrt_init ();
  ddsi_iid_init ();
  ddsi_thread_states_init ();
  const char *config = "";
  (void) ddsrt_getenv ("CYCLONEDDS_URI", &config);
  cfgst = ddsi_config_init (config, &gv.config, 0);
  assert (cfgst != NULL);
  ddsi_config_prep (&gv, cfgst);
  rbufpool = ddsi_rbufpool_new (&gv.logconfig, 131072, 65536);
  ddsi_init (&gv);
}

static void teardown (void)
{
  ddsi_fini (&gv);
  ddsi_rbufpool_free (rbufpool);
  ddsi_config_fini (cfgst);
  ddsi_iid_fini ();
  ddsi_thread_states_fini ();
  ddsrt_fini ();
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

      struct plist_valid const * const exp =
        (contexts[j] == DDSI_PLIST_CONTEXT_PARTICIPANT) ? &plists[i].spdp : &plists[i].others;
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

CU_Test (ddsi_plist_leasedur, ser_others, .init = setup, .fini = teardown)
{
  for (size_t j = 0; j < sizeof (contexts) / sizeof (contexts[0]); j++)
  {
    // SPDP is handled separately
    if (contexts[j] == DDSI_PLIST_CONTEXT_PARTICIPANT)
      continue;

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
}

#define UDPLOCATOR(a,b,c,d,port) \
  SER32BE (DDSI_LOCATOR_KIND_UDPv4), \
  SER32BE(port), \
  SER32BE(0),SER32BE(0),SER32BE(0), \
  (a),(b),(c),(d)

#define TEST_GUIDPREFIX_BYTES 7,7,3,4, 5,6,7,8, 9,10,11,12

static void setup_and_start (void)
{
  setup ();
  ddsi_set_deafmute (&gv, true, true, DDS_INFINITY);
  ddsi_start (&gv);
  // Register the main thread, then claim it as spawned by Cyclone because the
  // internal processing has various asserts that it isn't an application thread
  // doing the dirty work
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  assert (thrst->state == DDSI_THREAD_STATE_LAZILY_CREATED);
  thrst->state = DDSI_THREAD_STATE_ALIVE;
  ddsrt_atomic_stvoidp (&thrst->gv, &gv);
}

static void stop_and_teardown (void)
{
  // Shutdown currently relies on sending packets to shutdown receiver threads
  // handling individual sockets (this sometime causes issues with firewalls, too)
  ddsi_set_deafmute (&gv, false, false, DDS_INFINITY);
  // On shutdown there is an expectation that the thread was discovered dynamically.
  // We overrode it in the setup code, we undo it now.
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  thrst->state = DDSI_THREAD_STATE_LAZILY_CREATED;
  ddsi_stop (&gv);
  teardown ();
}

static void ddsi_plist_leasedur_new_proxypp_impl (bool include_lease_duration)
{
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  const uint32_t port = gv.loc_meta_uc.port;

  // not static nor const: we need to patch in the port number
  unsigned char pkt_header[] = {
    'R', 'T', 'P', 'S', DDSI_RTPS_MAJOR, DDSI_RTPS_MINOR,
    // vendor id: major 1 is a given
    1, DDSI_VENDORID_MINOR_ECLIPSE,
    // GUID prefix: first two bytes ordinarily have vendor id, so 7,7 is
    // guaranteed to not be used locally
    TEST_GUIDPREFIX_BYTES,
    // DATA: flags (4 = dataflag + big-endian); octets-to-next-header = 0
    // means it continues until the end
    DDSI_RTPS_SMID_DATA, 4, 0,0,
    0,0, // extra flags
    0,16, // octets to inline QoS (no inline qos here, so: to payload)
    SER32BE (DDSI_ENTITYID_UNKNOWN),
    SER32BE (DDSI_ENTITYID_SPDP_BUILTIN_PARTICIPANT_WRITER),
    SER32BE (0), SER32BE (1), // seq number 1
    0,2, // PL_CDR_BE
    0,0, // options = 0
    HDR (DDSI_PID_PARTICIPANT_GUID, 16),
      TEST_GUIDPREFIX_BYTES, SER32BE (DDSI_ENTITYID_PARTICIPANT),
    HDR (DDSI_PID_BUILTIN_ENDPOINT_SET, 4),         SER32BE (DDSI_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER),
    HDR (DDSI_PID_PROTOCOL_VERSION, 4),             DDSI_RTPS_MAJOR, DDSI_RTPS_MINOR, 0,0,
    HDR (DDSI_PID_VENDORID, 4),                     1, DDSI_VENDORID_MINOR_ECLIPSE, 0,0,
    HDR (DDSI_PID_DEFAULT_UNICAST_LOCATOR, 24),     UDPLOCATOR (127,0,0,1, port),
    HDR (DDSI_PID_METATRAFFIC_UNICAST_LOCATOR, 24), UDPLOCATOR (127,0,0,1, port)
  };
  unsigned char pkt_leasedur[] = {
    HDR (DDSI_PID_PARTICIPANT_LEASE_DURATION, 8),   SER32BE (3), SER32BE (0x12345679),
  };
  unsigned char pkt_trailer[] = {
    SENTINEL
  };
  ddsi_locator_t srcloc;
  ddsi_conn_locator (gv.xmit_conns[0], &srcloc);
  const ddsi_guid_t proxypp_guid = {
    .prefix = ddsi_ntoh_guid_prefix ((ddsi_guid_prefix_t){ .s = { TEST_GUIDPREFIX_BYTES } }),
    .entityid = { .u = DDSI_ENTITYID_PARTICIPANT }
  };

  // Process the packet we so carefully constructed above as if it was received
  // over the network.  Stack is deaf (and mute), so there is no risk that the
  // message gets dropped because some buffer is full
  struct ddsi_rmsg *rmsg = ddsi_rmsg_new (rbufpool);
  unsigned char *buf = (unsigned char *) DDSI_RMSG_PAYLOAD (rmsg);
  size_t size = 0;
  memcpy (buf, pkt_header, sizeof (pkt_header));
  size += sizeof (pkt_header);
  if (include_lease_duration)
  {
    memcpy (buf + size, pkt_leasedur, sizeof (pkt_leasedur));
    size += sizeof (pkt_leasedur);
  }
  memcpy (buf + size, pkt_trailer, sizeof (pkt_trailer));
  size += sizeof (pkt_trailer);
  ddsi_rmsg_setsize (rmsg, (uint32_t) size);
  ddsi_handle_rtps_message (thrst, &gv, gv.data_conn_uc, NULL, rbufpool, rmsg, size, buf, &srcloc);
  ddsi_rmsg_commit (rmsg);

  // Discovery data processing is done by the dq.builtin thread, so we can't be
  // sure the SPDP message gets processed immediately.  Polling seems reasonable
  const dds_time_t tend = dds_time () + DDS_SECS (10);
  struct ddsi_proxy_participant *proxypp = NULL;
  ddsi_thread_state_awake (thrst, &gv);
  while (proxypp == NULL && dds_time () < tend)
  {
    ddsi_thread_state_asleep (thrst);
    dds_sleepfor (DDS_MSECS (10));
    ddsi_thread_state_awake (thrst, &gv);
    proxypp = ddsi_entidx_lookup_proxy_participant_guid (gv.entity_index, &proxypp_guid);
  }

  // After waiting for a reasonable amount of time, the (fake) proxy participant
  // should exist and have picked up the lease duration from the message
  CU_ASSERT_FATAL (proxypp != NULL);
  assert (proxypp); // for gcc/clang static analyzer
  CU_ASSERT_FATAL (proxypp->plist->qos.present & DDSI_QP_LIVELINESS);
  CU_ASSERT_FATAL (proxypp->plist->qos.liveliness.kind == DDS_LIVELINESS_AUTOMATIC);
  if (include_lease_duration) {
    CU_ASSERT_FATAL (proxypp->plist->qos.liveliness.lease_duration == 3071111111);
  } else {
    CU_ASSERT_FATAL (proxypp->plist->qos.liveliness.lease_duration == DDS_SECS (100));
  }
  CU_ASSERT_FATAL (proxypp->lease->tdur == proxypp->plist->qos.liveliness.lease_duration);
  ddsi_thread_state_asleep (thrst);
}

CU_Test (ddsi_plist_leasedur, new_proxypp, .init = setup_and_start, .fini = stop_and_teardown)
{
  ddsi_plist_leasedur_new_proxypp_impl (true);
}

CU_Test (ddsi_plist_leasedur, new_proxypp_def, .init = setup_and_start, .fini = stop_and_teardown)
{
  ddsi_plist_leasedur_new_proxypp_impl (false);
}

static void ddsi_plist_leasedur_new_proxyrd_impl (bool include_lease_duration)
{
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();

  ddsi_guid_t ppguid;
  ddsi_plist_t plist;
  ddsi_plist_init_empty (&plist);
  plist.qos.present |= DDSI_QP_LIVELINESS;
  plist.qos.liveliness.kind = DDS_LIVELINESS_AUTOMATIC;
  plist.qos.liveliness.lease_duration = DDS_SECS (10);
  ddsi_thread_state_awake (thrst, &gv);
  dds_return_t ret = ddsi_new_participant (&ppguid, &gv, RTPS_PF_PRIVILEGED_PP | RTPS_PF_IS_DDSI2_PP, &plist);
  ddsi_thread_state_asleep (thrst);
  CU_ASSERT_FATAL (ret >= 0);
  ddsi_plist_fini (&plist);

  // not static nor const: we need to patch in the port number
  const unsigned char pkt_p0[] = {
    'R', 'T', 'P', 'S', DDSI_RTPS_MAJOR, DDSI_RTPS_MINOR,
    // vendor id: major 1 is a given
    1, DDSI_VENDORID_MINOR_ECLIPSE,
    // GUID prefix: first two bytes ordinarily have vendor id, so 7,7 is
    // guaranteed to not be used locally
    TEST_GUIDPREFIX_BYTES,
    // INFO_DST: flags (0 = big-endian); octets-to-next-header = 12
    DDSI_RTPS_SMID_INFO_DST, 0, 0,12
    // guid prefix of local node (= ppguidprefix_base) comes here
  };
  const unsigned char pkt_p1[] = {
    // HEARTBEAT; flags (2 = final+big-endian); octets-to-next-header = 28
    DDSI_RTPS_SMID_HEARTBEAT, 2, 0,28,
    SER32BE (DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_READER),
    SER32BE (DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER),
    SER32BE (0), SER32BE (1), // min seq number 1 \_ empty WHC
    SER32BE (0), SER32BE (0), // max seq number 0 /
    SER32BE (1) // count
  };
  const unsigned char pkt_p2[] = {
    // DATA: flags (4 = dataflag + big-endian); octets-to-next-header = 0
    // means it continues until the end
    DDSI_RTPS_SMID_DATA, 4, 0,0,
    0,0, // extra flags
    0,16, // octets to inline QoS (no inline qos here, so: to payload)
    SER32BE (DDSI_ENTITYID_UNKNOWN),
    SER32BE (DDSI_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER),
    SER32BE (0), SER32BE (1), // seq number 1
    0,2, // PL_CDR_BE
    0,0, // options = 0
    HDR (DDSI_PID_ENDPOINT_GUID, 16),
      TEST_GUIDPREFIX_BYTES, SER32BE (0x107), // reader-with-key
    HDR (DDSI_PID_TOPIC_NAME, 12), SER32BE (6), 't','o','p','i','c',0, 0,0,
    HDR (DDSI_PID_TYPE_NAME, 12),  SER32BE (5), 't','y','p','e',0,   0,0,0,
  };
  unsigned char pkt_p3[] = {
    HDR (DDSI_PID_LIVELINESS, 12),
      SER32BE (DDS_LIVELINESS_MANUAL_BY_PARTICIPANT),
      SER32BE (2), SER32BE (0x0abcdefb),
  };
  unsigned char pkt_p4[] = {
    SENTINEL
  };
  ddsi_locator_t srcloc;
  ddsi_conn_locator (gv.xmit_conns[0], &srcloc);
  const ddsi_guid_t prd_guid = {
    .prefix = ddsi_ntoh_guid_prefix ((ddsi_guid_prefix_t){ .s = { TEST_GUIDPREFIX_BYTES } }),
    .entityid = { .u = 0x107 }
  };

  // Process the packet we so carefully constructed above as if it was received
  // over the network.  Stack is deaf (and mute), so there is no risk that the
  // message gets dropped because some buffer is full
  struct ddsi_rmsg *rmsg = ddsi_rmsg_new (rbufpool);
  unsigned char *buf = (unsigned char *) DDSI_RMSG_PAYLOAD (rmsg);
  size_t size = 0;
  memcpy (buf + size, pkt_p0, sizeof (pkt_p0));
  size += sizeof (pkt_p0);
  ddsi_guid_prefix_t ppguidprefix = ddsi_hton_guid_prefix (ppguid.prefix);
  memcpy (buf + size, &ppguidprefix, sizeof (ppguidprefix));
  size += sizeof (ppguidprefix);
  memcpy (buf + size, pkt_p1, sizeof (pkt_p1));
  size += sizeof (pkt_p1);
  memcpy (buf + size, pkt_p2, sizeof (pkt_p2));
  size += sizeof (pkt_p2);
  if (include_lease_duration)
  {
    memcpy (buf + size, pkt_p3, sizeof (pkt_p3));
    size += sizeof (pkt_p3);
  }
  memcpy (buf + size, pkt_p4, sizeof (pkt_p4));
  size += sizeof (pkt_p4);
  ddsi_rmsg_setsize (rmsg, (uint32_t) size);
  ddsi_handle_rtps_message (thrst, &gv, gv.data_conn_uc, NULL, rbufpool, rmsg, size, buf, &srcloc);
  ddsi_rmsg_commit (rmsg);

  // Discovery data processing is done by the dq.builtin thread, so we can't be
  // sure the SEDP message gets processed immediately.  Polling seems reasonable
  const dds_time_t tend = dds_time () + DDS_SECS (10);
  struct ddsi_proxy_reader *prd = NULL;
  ddsi_thread_state_awake (thrst, &gv);
  while (prd == NULL && dds_time () < tend)
  {
    ddsi_thread_state_asleep (thrst);
    dds_sleepfor (DDS_MSECS (10));
    ddsi_thread_state_awake (thrst, &gv);
    prd = ddsi_entidx_lookup_proxy_reader_guid (gv.entity_index, &prd_guid);
  }

  // After waiting for a reasonable amount of time, the (fake) proxy participant
  // should exist and have picked up the lease duration from the message
  CU_ASSERT_FATAL (prd != NULL);
  assert (prd); // for gcc/clang static analyzer
  CU_ASSERT_FATAL (prd->c.xqos->present & DDSI_QP_LIVELINESS);
  if (include_lease_duration) {
    CU_ASSERT (prd->c.xqos->liveliness.kind == DDS_LIVELINESS_MANUAL_BY_PARTICIPANT);
    CU_ASSERT (prd->c.xqos->liveliness.lease_duration == 2041944443);
  } else {
    CU_ASSERT (prd->c.xqos->liveliness.kind == DDS_LIVELINESS_AUTOMATIC);
    CU_ASSERT (prd->c.xqos->liveliness.lease_duration == DDS_INFINITY);
  }
  ddsi_thread_state_asleep (thrst);
}

CU_Test (ddsi_plist_leasedur, new_proxyrd, .init = setup_and_start, .fini = stop_and_teardown)
{
  ddsi_plist_leasedur_new_proxypp_impl (false);
  ddsi_plist_leasedur_new_proxyrd_impl (true);
}

CU_Test (ddsi_plist_leasedur, new_proxyrd_def, .init = setup_and_start, .fini = stop_and_teardown)
{
  ddsi_plist_leasedur_new_proxypp_impl (false);
  ddsi_plist_leasedur_new_proxyrd_impl (false);
}
