// Copyright(c) 2025 ZettaScale Technology and others
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
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_rhc.h"
#include "dds/ddsc/dds_data_type_properties.h"
#include "ddsi__addrset.h"
#include "ddsi__participant.h"
#include "ddsi__proxy_participant.h"
#include "ddsi__endpoint.h"
#include "ddsi__proxy_endpoint.h"
#include "ddsi__plist.h"
#include "ddsi__radmin.h"
#include "ddsi__xmsg.h"
#include "ddsi__vendor.h"
#include "ddsi__receive.h"
#include "ddsi__tran.h"
#include "ddsi__protocol.h"
#include "ddsi__radmin.h"
#include "ddsi__wraddrset.h"
#include "ddsi__vnet.h"

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
  // with just one receive thread we don't need to send anything during shutdown and can remain deaf/mute
  gv.config.multiple_recv_threads = false;
  ddsi_init (&gv, NULL);
  rbufpool = ddsi_rbufpool_new (&gv.logconfig, 131072, 65536);
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

static void setup_and_start (void)
{
  setup ();
  // not very proper to do this here
  // abusing DDS_LC_CONTENT a bit
  dds_log_cfg_init (&gv.logconfig, gv.config.domainId, DDS_LC_CONTENT /* DDS_LC_ALL or DDS_LC_CONTENT */, stdout, stdout);

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
  // On shutdown there is an expectation that the thread was discovered dynamically.
  // We overrode it in the setup code, we undo it now.
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  thrst->state = DDSI_THREAD_STATE_LAZILY_CREATED;
  ddsi_stop (&gv);
  teardown ();
}

struct serdata {
  struct ddsi_serdata c;
  uint32_t size;
};

static void serdata_free (struct ddsi_serdata *sd) {
  ddsrt_free (sd);
}

static struct ddsi_serdata *serdata_from_ser (const struct ddsi_sertype *st, enum ddsi_serdata_kind kind, const struct ddsi_rdata *fragchain, size_t size)
{
  (void) fragchain;
  printf ("Yay! %zu bytes\n", size);
  struct serdata *sd = ddsrt_malloc (sizeof (*sd));
  ddsi_serdata_init (&sd->c, st, kind);
  sd->size = (uint32_t) size;
  sd->c.hash = st->serdata_basehash;
  return &sd->c;
}

static struct ddsi_serdata *serdata_to_untyped (const struct ddsi_serdata *d)
{
  // it doesn't matter all that much
  return ddsi_serdata_ref (d);
}

static bool serdata_eqkey (const struct ddsi_serdata *a, const struct ddsi_serdata *b)
{
  (void) a; (void) b;
  return true;
}

static uint32_t serdata_get_size (const struct ddsi_serdata *sd0)
{
  struct serdata *sd = (struct serdata *) sd0;
  return sd->size;
};

static const struct ddsi_serdata_ops serdata_ops = {
  .from_ser = serdata_from_ser,
  .free = serdata_free,
  .to_untyped = serdata_to_untyped,
  .eqkey = serdata_eqkey,
  .get_size = serdata_get_size
};

static void sertype_free (struct ddsi_sertype *st)
{
  ddsi_sertype_fini (st);
  ddsrt_free (st);
}

static uint32_t sertype_hash (const struct ddsi_sertype *st)
{
  (void) st;
  return 1;
}

static bool sertype_equal (const struct ddsi_sertype *a, const struct ddsi_sertype *b)
{
  (void) a; (void) b;
  return true;
}

// woefully incomplete set of operations, which we can get away with because
// we're not really doing anything anyway
static const struct ddsi_sertype_ops sertype_ops = {
  .version = ddsi_sertype_v0,
  .free = sertype_free,
  .hash = sertype_hash,
  .equal = sertype_equal
};

static struct ddsi_sertype *sertype_new (bool with_key)
{
  const size_t sizeof_type = 8u; // fictitious, we're not really doing anything
  struct ddsi_sertype *st = ddsrt_malloc (sizeof (*st));
  ddsi_sertype_init_props (st, "Q", &sertype_ops, &serdata_ops, sizeof_type, DDS_DATA_TYPE_IS_MEMCPY_SAFE | (with_key ? DDS_DATA_TYPE_CONTAINS_KEY : 0), DDS_DATA_REPRESENTATION_XCDR1 | DDS_DATA_REPRESENTATION_XCDR2, 0);
  return st;
}

struct rhc {
  struct ddsi_rhc c;
  bool stored;
};

static void rhc_free (struct ddsi_rhc *rhc) {
  (void) rhc;
}

static bool rhc_store (struct ddsi_rhc *rhc, const struct ddsi_writer_info *wrinfo, struct ddsi_serdata *sample, struct ddsi_tkmap_instance *tk)
{
  (void) wrinfo; (void) sample; (void) tk;
  struct rhc *x = (struct rhc *) rhc;
  x->stored = true;
  return true;
}

static void rhc_unregister_wr (struct ddsi_rhc *rhc, const struct ddsi_writer_info *wrinfo)
{
  (void) rhc; (void) wrinfo;
}

static void rhc_relinquish_ownership (struct ddsi_rhc *rhc, const uint64_t wr_iid)
{
  (void) rhc; (void) wr_iid;
}

static void rhc_set_qos (struct ddsi_rhc *rhc, const struct dds_qos *qos)
{
  (void) rhc; (void) qos;
}

static struct rhc rhc = {
  .c = {
    .ops = &(struct ddsi_rhc_ops){
      .store = rhc_store,
      .unregister_wr = rhc_unregister_wr,
      .relinquish_ownership = rhc_relinquish_ownership,
      .set_qos = rhc_set_qos,
      .free = rhc_free
    }
  },
  .stored = false
};

static const ddsi_guid_t nullguid = { .prefix = { .u = { 0,0,0 } }, .entityid = { .u = 0 } };

static bool isnullguid (const ddsi_guid_t *guid)
{
  return guid->prefix.u[0] == 0 && guid->prefix.u[1] == 0 && guid->prefix.u[2] == 0 && guid->entityid.u == 0;
}

static void receive_packet_init (ddsi_guid_t *rdguid, ddsi_guid_t *wrguid, bool with_key, ddsi_seqno_t next_seqno)
{
#define UNILOC(k) { .kind = DDSI_LOCATOR_KIND_UDPv4, .address = {0,0,0,0, 0,0,0,0, 0,0,0,0, 192,16,1,k+1}, .port = 7410 }
  const ddsi_locator_t ucloc[2] = { UNILOC(0), UNILOC(1) };
  const ddsi_locator_t mcloc = {
    .kind = DDSI_LOCATOR_KIND_UDPv4, .address = {0,0,0,0, 0,0,0,0, 0,0,0,0, 239,255,0,1}, .port = 7400
  };
#define UNILOC1(k) { .next = NULL, .loc = ucloc[k] }
  const struct ddsi_locators_one ucloc1[2] = { UNILOC1(0), UNILOC1(1) };
  const struct ddsi_locators_one mcloc1 = { .next = NULL, .loc = mcloc };
#define UNILOCS(k) { .n = 1, .first = (struct ddsi_locators_one *) &ucloc1[k], .last = (struct ddsi_locators_one *) &ucloc1[k] }
  const ddsi_locators_t uclocs[2] = { UNILOCS(0), UNILOCS(1) };
  const ddsi_locators_t mclocs = { .n = 1, .first = (struct ddsi_locators_one *) &mcloc1, .last = (struct ddsi_locators_one *) &mcloc1 };
#define PLIST_PP(k) { \
    .present = PP_UNICAST_LOCATOR | PP_MULTICAST_LOCATOR | PP_DEFAULT_UNICAST_LOCATOR | PP_DEFAULT_MULTICAST_LOCATOR, \
    .unicast_locators = uclocs[k], \
    .multicast_locators = mclocs, \
    .default_unicast_locators = uclocs[k], \
    .default_multicast_locators = mclocs, \
    .metatraffic_unicast_locators = uclocs[k], \
    .metatraffic_multicast_locators = mclocs, \
    .qos = { \
      .present = DDSI_QP_LIVELINESS, \
      .liveliness = { .kind = DDS_LIVELINESS_AUTOMATIC, .lease_duration = DDS_INFINITY } \
    } \
  }
  const ddsi_plist_t plist_pp[2] = { PLIST_PP(0), PLIST_PP(1) };
  ddsi_guid_t rdppguid, wrppguid;

  setup_and_start ();
  ddsi_thread_state_awake (ddsi_lookup_thread_state(), &gv);
  if (isnullguid (rdguid))
    ddsi_generate_participant_guid (&rdppguid, &gv);
  else
  {
    rdppguid.prefix = rdguid->prefix;
    rdppguid.entityid.u = DDSI_ENTITYID_PARTICIPANT;
  }
  ddsi_new_participant (&rdppguid, &gv, 0, &plist_pp[0]);
  struct ddsi_participant * const pp = ddsi_entidx_lookup_participant_guid (gv.entity_index, &rdppguid);

  // construct sertype & register it
  struct ddsi_sertype * const st = sertype_new (with_key);
  ddsrt_mutex_lock (&gv.sertypes_lock);
  assert (ddsi_sertype_lookup_locked (&gv, st) == NULL);
  ddsi_sertype_register_locked (&gv, st);
  ddsrt_mutex_unlock (&gv.sertypes_lock);
  // drop initial reference: we don't need it anymore because it is registered now
  // (must be done outside sertypes_lock)
  ddsi_sertype_unref (st);

  struct ddsi_reader *rd;
  if (isnullguid (rdguid))
  {
    dds_return_t ret = ddsi_generate_reader_guid (rdguid, pp, st);
    assert (ret == DDS_RETCODE_OK);
    (void) ret;
  }
  ddsi_new_reader (&rd, rdguid, NULL, pp, "Q", st, &ddsi_default_qos_reader, &rhc.c, NULL, NULL, NULL);
  assert (ddsi_entidx_lookup_reader_guid (gv.entity_index, rdguid));
  // reader keeps sertype alive, so we can safely drop a reference here
  // (akin to deleting the topic after creating the reader in the API)
  ddsi_sertype_unref (st);

  // real guids never have the first 32-bits 0, so these are guaranteed unique
  if (isnullguid (wrguid))
  {
    wrppguid = (ddsi_guid_t){
      .prefix = { .u = { 0, 1, 1 } },
      .entityid = { .u = DDSI_ENTITYID_PARTICIPANT }
    };
  }
  else
  {
    wrppguid.prefix = wrguid->prefix;
    wrppguid.entityid.u = DDSI_ENTITYID_PARTICIPANT;
  }
  struct ddsi_addrset *proxypp_as = ddsi_new_addrset ();
  struct ddsi_proxy_participant *proxy_participant;
  ddsi_locator_t loc = ucloc[1]; loc.port = 1000;
  ddsi_add_locator_to_addrset (&gv, proxypp_as, &loc);
  ddsi_add_locator_to_addrset (&gv, proxypp_as, &mcloc);
  ddsi_new_proxy_participant (&proxy_participant, &gv, &wrppguid, 0, proxypp_as, ddsi_ref_addrset (proxypp_as), &plist_pp[1], DDS_INFINITY, DDSI_VENDORID_ECLIPSE, ddsrt_time_wallclock (), next_seqno);
  assert (proxy_participant != NULL);

  if (isnullguid (wrguid))
  {
    wrguid->prefix = wrppguid.prefix;
    wrguid->entityid.u = DDSI_ENTITYID_ALLOCSTEP | DDSI_ENTITYID_SOURCE_USER;
    if (with_key)
      wrguid->entityid.u |= DDSI_ENTITYID_KIND_WRITER_WITH_KEY;
    else
      wrguid->entityid.u |= DDSI_ENTITYID_KIND_WRITER_NO_KEY;
  };
  ddsi_plist_t plist_wr = { .present = 0, .qos = ddsi_default_qos_writer };
  plist_wr.qos.present |= DDSI_QP_TOPIC_NAME | DDSI_QP_TYPE_NAME;
  plist_wr.qos.reliability.kind = DDS_RELIABILITY_RELIABLE;
  plist_wr.qos.topic_name = "Q";
  plist_wr.qos.type_name = "Q";
  struct ddsi_addrset *wr_as = ddsi_new_addrset ();
  ddsi_add_locator_to_addrset (&gv, wr_as, &loc);
  ddsi_add_locator_to_addrset (&gv, wr_as, &mcloc);
  struct ddsi_proxy_writer *proxy_writer;
  //int ddsi_new_proxy_writer (struct ddsi_proxy_writer **proxy_writer, struct ddsi_domaingv *gv, const struct ddsi_guid *ppguid, const struct ddsi_guid *guid, struct ddsi_addrset *as, const ddsi_plist_t *plist, struct ddsi_dqueue *dqueue, struct ddsi_xeventq *evq, ddsrt_wctime_t timestamp, ddsi_seqno_t seq)
  ddsi_new_proxy_writer (&proxy_writer, &gv, &wrppguid, wrguid, wr_as, &plist_wr, gv.user_dqueue, gv.xevents, ddsrt_time_wallclock (), 1);
  assert (proxy_writer);
  ddsi_unref_addrset (wr_as);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
}

static void receive_packet_fini (void)
{
  stop_and_teardown ();
}

static ddsi_guid_t ddsi_guid_from_octets (const unsigned char x[16])
{
  ddsi_guid_t y;
  memcpy (&y, x, sizeof (y));
  return ddsi_ntoh_guid (y);
}

CU_Test (ddsi_receive_packet, rti_dispose_with_key)
{
  ddsi_guid_t rdguid = nullguid;
  ddsi_guid_t wrguid = ddsi_guid_from_octets (((unsigned char[]){
    0x1, 0x1, 0x5c, 0x45, 0x1f, 0x8c, 0x14, 0xb5, 0xdf, 0x19, 0xb3, 0xbd,
    0x80, 0x0, 0x0, 0x2}));
  //   Real-Time Publish-Subscribe Wire Protocol
  //   Magic: RTPS
  //   Protocol version: 2.5
  //   vendorId: 01.01 (Real-Time Innovations, Inc. - Connext DDS)
  //   guidPrefix: 01015c451f8c14b5df19b3bd
  //   Default port mapping (Based on calculated domainId. Might not be accurate): MULTICAST_USERTRAFFIC, domainId=0
  //   submessageId: INFO_TS (0x09)
  //   submessageId: DATA (0x15)
  //       Flags: 0x0b, Serialized Key, Inline QoS, Endianness
  //       octetsToNextHeader: 88
  //       0000 0000 0000 0000 = Extra flags: 0x0000
  //       Octets to inline QoS: 16
  //       readerEntityId: ENTITYID_UNKNOWN (0x00000000)
  //       writerEntityId: 0x80000002 (Application-defined writer (with key): 0x800000)
  //       writerSeqNumber: 32
  //       inlineQos:
  //           PID_KEY_HASH
  //               parameterId: PID_KEY_HASH (0x0070)
  //               parameterLength: 16
  //               guid: d41c500b:dd937e4b:5a01e791:2e41a06d
  //           PID_STATUS_INFO
  //               parameterId: PID_STATUS_INFO (0x0071)
  //               parameterLength: 4
  //               Flags: 0x00000001, Disposed
  //           PID_SENTINEL
  //       serializedKey
  //           encapsulation kind: PL_CDR_LE (0x0003)
  //           encapsulation options: 0x0000
  //           serializedData
  //               Member (id = 79104952, len = 16)
  //                   Member ID: 79104952
  //                   Member length: 16
  //                   Member value: 56464455664256b4d3229be867453e22
  const unsigned char rtps_message[] = {
    0x52, 0x54, 0x50, 0x53, 0x2, 0x5, 0x1, 0x1, 0x1, 0x1, 0x5c, 0x45, 0x1f, 0x8c, 0x14,
    0xb5, 0xdf, 0x19, 0xb3, 0xbd, 0x9, 0x1, 0x8, 0x0, 0x75, 0x36, 0x2b, 0x68, 0xa9, 0xca,
    0x85, 0x9e, 0x15, 0xb, 0x58, 0x0, 0x0, 0x0, 0x10, 0x0, 0x0, 0x0, 0x0, 0x0, 0x80, 0x0,
    0x0, 0x2, 0x0, 0x0, 0x0, 0x0, 0x20, 0x0, 0x0, 0x0, 0x70, 0x0, 0x10, 0x0, 0xd4, 0x1c,
    0x50, 0xb, 0xdd, 0x93, 0x7e, 0x4b, 0x5a, 0x1, 0xe7, 0x91, 0x2e, 0x41, 0xa0, 0x6d, 0x71,
    0x0, 0x4, 0x0, 0x0, 0x0, 0x0, 0x1, 0x1, 0x0, 0x0, 0x0, 0x0, 0x3, 0x0, 0x0, 0x1, 0x3f,
    0x8, 0x0, 0xb8, 0xb, 0xb7, 0x4, 0x10, 0x0, 0x0, 0x0, 0x56, 0x46, 0x44, 0x55, 0x66,
    0x42, 0x56, 0xb4, 0xd3, 0x22, 0x9b, 0xe8, 0x67, 0x45, 0x3e, 0x22, 0x2, 0x7f, 0x0, 0x0
  };

  receive_packet_init (&rdguid, &wrguid, true, 32);

  struct ddsi_network_packet_info pktinfo;
  ddsi_conn_locator (gv.xmit_conns[0], &pktinfo.src);
  pktinfo.dst.kind = DDSI_LOCATOR_KIND_INVALID;
  pktinfo.if_index = 0;

  // Process the packet we so carefully constructed above as if it was received
  // over the network.  Stack is deaf (and mute), so there is no risk that the
  // message gets dropped because some buffer is full
  struct ddsi_rmsg *rmsg = ddsi_rmsg_new (rbufpool);
  unsigned char *buf = (unsigned char *) DDSI_RMSG_PAYLOAD (rmsg);
  memcpy (buf, rtps_message, sizeof (rtps_message));
  ddsi_rmsg_setsize (rmsg, (uint32_t) sizeof (rtps_message));
  
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  ddsi_handle_rtps_message (thrst, &gv, gv.data_conn_uc, NULL, rbufpool, rmsg, (uint32_t) sizeof (rtps_message), buf, &pktinfo);
  ddsi_rmsg_commit (rmsg);

  receive_packet_fini ();
  CU_ASSERT_NEQ (rhc.stored, 0);
}
