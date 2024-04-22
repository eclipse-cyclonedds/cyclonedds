// Copyright(c) 2023 ZettaScale Technology and others
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
#include "ddsi__radmin.h"
#include "mem_ser.h"

#define HDR(id, len) SER32BE(((uint32_t)(id) << 16) | (uint32_t)(len))
#define SENTINEL     HDR(DDSI_PID_SENTINEL, 0)

#define UDPLOCATOR(a,b,c,d,port) \
  SER32BE (DDSI_LOCATOR_KIND_UDPv4), \
  SER32BE(port), \
  SER32BE(0),SER32BE(0),SER32BE(0), \
  (a),(b),(c),(d)

#define TEST_GUIDPREFIX_BYTES 7,7,3,4, 5,6,7,8, 9,10,11,12

static struct ddsi_cfgst *cfgst;
static struct ddsi_domaingv gv;
static struct ddsi_rbufpool *rbufpool;
static ddsi_guid_t ppguid;

struct logger_arg {
  ddsrt_atomic_uint32_t match;
};

static struct logger_arg logger_arg = {
  DDSRT_ATOMIC_UINT32_INIT (0)
};

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
  ddsi_init (&gv, NULL);
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

static void logger (void *ptr, const dds_log_data_t *data)
{
  struct logger_arg *arg = ptr;
  printf ("%s", data->message);
  fflush (stdout);
  // We know the GUID; 707 is simply how the beginnning of
  // TEST_GUIDPREFIX_BYTES gets printed, and as the first
  // two bytes are vendor code and not Cyclone DDS, this
  // suffices
  if (strstr (data->message, "PMD ST0 pp 707"))
    ddsrt_atomic_inc32 (&arg->match);
}

static void setup_and_start (void)
{
  setup ();
  dds_set_log_sink (&logger, &logger_arg);
  dds_set_trace_sink (&logger, &logger_arg);
  // not very proper to do this here
  dds_log_cfg_init (&gv.logconfig, gv.config.domainId, DDS_LC_TRACE, stderr, NULL);

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
  dds_set_log_sink (0, 0);
  dds_set_trace_sink (0, 0);

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

struct wait_for_dqueue_helper_arg {
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  int ready;
};

static void wait_for_dqueue_helper_cb (void *varg)
{
  struct wait_for_dqueue_helper_arg *arg = varg;
  ddsrt_mutex_lock (&arg->lock);
  arg->ready = 1;
  ddsrt_cond_broadcast (&arg->cond);
  ddsrt_mutex_unlock (&arg->lock);
}

static void wait_for_dqueue (void)
{
  struct wait_for_dqueue_helper_arg arg;
  ddsrt_mutex_init (&arg.lock);
  ddsrt_cond_init (&arg.cond);
  arg.ready = 0;
  ddsi_dqueue_enqueue_callback(gv.builtins_dqueue, wait_for_dqueue_helper_cb, &arg);
  ddsrt_mutex_lock (&arg.lock);
  while (!arg.ready)
    ddsrt_cond_wait (&arg.cond, &arg.lock);
  ddsrt_mutex_unlock (&arg.lock);
  ddsrt_cond_destroy (&arg.cond);
  ddsrt_mutex_destroy (&arg.lock);
}

static void create_fake_proxy_participant (void)
{
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  const uint32_t port = gv.loc_meta_uc.port;

  // not static nor const: we need to patch in the port number
  unsigned char spdp_pkt[] = {
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
    HDR (DDSI_PID_BUILTIN_ENDPOINT_SET, 4),
    SER32BE (DDSI_DISC_BUILTIN_ENDPOINT_SUBSCRIPTION_ANNOUNCER | DDSI_BUILTIN_ENDPOINT_PARTICIPANT_MESSAGE_DATA_WRITER),
    HDR (DDSI_PID_PROTOCOL_VERSION, 4),             DDSI_RTPS_MAJOR, DDSI_RTPS_MINOR, 0,0,
    HDR (DDSI_PID_VENDORID, 4),                     1, DDSI_VENDORID_MINOR_ECLIPSE, 0,0,
    HDR (DDSI_PID_DEFAULT_UNICAST_LOCATOR, 24),     UDPLOCATOR (127,0,0,1, port),
    HDR (DDSI_PID_METATRAFFIC_UNICAST_LOCATOR, 24), UDPLOCATOR (127,0,0,1, port),
    HDR (DDSI_PID_PARTICIPANT_LEASE_DURATION, 8),   SER32BE (100), SER32BE (0),
    SENTINEL
  };
  struct ddsi_network_packet_info pktinfo;
  ddsi_conn_locator (gv.xmit_conns[0], &pktinfo.src);
  pktinfo.dst.kind = DDSI_LOCATOR_KIND_INVALID;
  pktinfo.if_index = 0;
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
  memcpy (buf, spdp_pkt, sizeof (spdp_pkt));
  size += sizeof (spdp_pkt);
  ddsi_rmsg_setsize (rmsg, (uint32_t) size);
  ddsi_handle_rtps_message (thrst, &gv, gv.data_conn_uc, NULL, rbufpool, rmsg, size, buf, &pktinfo);
  ddsi_rmsg_commit (rmsg);
  // wait until SPDP message has been processed
  wait_for_dqueue ();

  // Discovery data processing is done by the dq.builtin thread, so we can't be
  // sure the SPDP message gets processed immediately.  Polling seems reasonable
  struct ddsi_proxy_participant *proxypp;
  ddsi_thread_state_awake (thrst, &gv);
  proxypp = ddsi_entidx_lookup_proxy_participant_guid (gv.entity_index, &proxypp_guid);
  CU_ASSERT_FATAL (proxypp != NULL);
  ddsi_thread_state_asleep (thrst);

  // No risk of a GUID collision: the fake proxy participant uses a different
  // vendor code
  ddsi_plist_t plist;
  ddsi_plist_init_empty (&plist);
  ddsi_xqos_mergein_missing (&plist.qos, &gv.default_local_xqos_pp, ~(uint64_t)0);
  ddsi_thread_state_awake (thrst, &gv);
  dds_return_t ret = ddsi_new_participant (&ppguid, &gv, RTPS_PF_IS_DDSI2_PP | RTPS_PF_PRIVILEGED_PP, &plist);
  ddsi_thread_state_asleep (thrst);
  ddsi_plist_fini (&plist);
  CU_ASSERT_FATAL (ret == 0);
}

static void send_pmd_message (uint32_t seqlo, uint16_t encoding, uint16_t options, uint32_t kind, uint32_t seq_length, uint32_t act_payload_size, bool msg_is_valid)
{
  // actual sequence length must be in range of our message bytes following the
  // CDR encoding+options, we don't want an out-of-bounds read
  assert (act_payload_size <= 24);

  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();

  // not static nor const: we need to patch in the port number
  unsigned char pmd_pkt[] = {
    'R', 'T', 'P', 'S', DDSI_RTPS_MAJOR, DDSI_RTPS_MINOR,
    // vendor id: major 1 is a given
    1, DDSI_VENDORID_MINOR_ECLIPSE,
    // GUID prefix: first two bytes ordinarily have vendor id, so 7,7 is
    // guaranteed to not be used locally
    TEST_GUIDPREFIX_BYTES,
    // INFO_DST or it won't accept the heartbeat as a handshake one
    DDSI_RTPS_SMID_INFO_DST, 0, 0,12, // flags, octets-to-next-header
    SER32BE (ppguid.prefix.u[0]), SER32BE (ppguid.prefix.u[1]), SER32BE (ppguid.prefix.u[2]),
    // HEARTBEAT or it won't accept the PMD message (no handshake completed)
    DDSI_RTPS_SMID_HEARTBEAT, 0, 0,28, // flags, octets-to-next-header
    SER32BE (DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_READER),
    SER32BE (DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER),
    SER32BE (0), SER32BE (seqlo),
    SER32BE (0), SER32BE (seqlo),
    SER32BE (seqlo),
    // DATA: flags (4 = dataflag + big-endian); octets-to-next-header = 0
    // means it continues until the end
    DDSI_RTPS_SMID_DATA, 4, 0,0,
    0,0, // extra flags
    0,16, // octets to inline QoS (no inline qos here, so: to payload)
    SER32BE (DDSI_ENTITYID_UNKNOWN),
    SER32BE (DDSI_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_WRITER),
    SER32BE (0), SER32BE (seqlo),
    (unsigned char) (encoding >> 8), (unsigned char) (encoding & 0xff),
    (unsigned char) (options >> 8), (unsigned char) (options & 0xff),
    // PMD message payload:
    TEST_GUIDPREFIX_BYTES,
    SER32BE (kind),
    SER32BE (seq_length),
    SER32BE (0)
  };

  ddsrt_atomic_st32 (&logger_arg.match, 0);

  // Process the packet we so carefully constructed above as if it was received
  // over the network.  Stack is deaf (and mute), so there is no risk that the
  // message gets dropped because some buffer is full
  struct ddsi_network_packet_info pktinfo;
  ddsi_conn_locator (gv.xmit_conns[0], &pktinfo.src);
  pktinfo.dst.kind = DDSI_LOCATOR_KIND_INVALID;
  pktinfo.if_index = 0;
  struct ddsi_rmsg *rmsg = ddsi_rmsg_new (rbufpool);
  unsigned char *buf = (unsigned char *) DDSI_RMSG_PAYLOAD (rmsg);
  size_t size = 0;
  memcpy (buf, pmd_pkt, sizeof (pmd_pkt));
  size += sizeof (pmd_pkt) - 24 + act_payload_size;
  ddsi_rmsg_setsize (rmsg, (uint32_t) size);
  ddsi_handle_rtps_message (thrst, &gv, gv.data_conn_uc, NULL, rbufpool, rmsg, size, buf, &pktinfo);
  ddsi_rmsg_commit (rmsg);
  // wait until PMD message has been processed
  wait_for_dqueue ();

  CU_ASSERT_FATAL (msg_is_valid == (ddsrt_atomic_ld32 (&logger_arg.match) == 1));
}

CU_Test (ddsi_pmd_message, valid, .init = setup_and_start, .fini = stop_and_teardown)
{
  create_fake_proxy_participant ();
  send_pmd_message (1, DDSI_RTPS_CDR_BE, 0, 0, 0, 20, true); // auto
  send_pmd_message (2, DDSI_RTPS_CDR_BE, 0, 1, 0, 20, true); // manual
  send_pmd_message (3, DDSI_RTPS_CDR_BE, 0, 2, 0, 20, true); // meaningless, ignored (log line is still output)
  send_pmd_message (4, DDSI_RTPS_CDR_BE, 3, 0, 1, 24, true); // 3 padding bytes
  send_pmd_message (5, DDSI_RTPS_CDR_BE, 0, 0, 4, 24, true);
}

CU_Test (ddsi_pmd_message, invalid_sequence, .init = setup_and_start, .fini = stop_and_teardown)
{
  create_fake_proxy_participant ();
  send_pmd_message (1, DDSI_RTPS_CDR_BE, 0, 0, 8, 24, false); // only have up to 4 bytes for octet sequence
  send_pmd_message (2, DDSI_RTPS_CDR_BE, 3, 0, 4, 24, true); // not valid but XTypes' padding-at-end field currently ignored
}

CU_Test (ddsi_pmd_message, bogus_header, .init = setup_and_start, .fini = stop_and_teardown)
{
  create_fake_proxy_participant ();
  send_pmd_message (1, DDSI_RTPS_CDR_BE, 0xa481, 0, 0, 20, true); // options may be anything, XTypes' padding-at-end field currently ignored
  send_pmd_message (2, DDSI_RTPS_CDR_BE, 0xa481, 0, 0, 16, false); // short
  send_pmd_message (3, DDSI_RTPS_CDR_BE, 0xa481, 0, 0, 0, false); // nothing at all -> used to trigger an assert
  send_pmd_message (4, 0xa481, 0, 0, 0, 0, false);
}
