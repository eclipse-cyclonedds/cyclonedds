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
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_serdata.h"
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
  // for this test it should be ok to do this a bit late
  ddsi_vnet_init (&gv, "psmx", DDSI_LOCATOR_KIND_PSMX);
}

static void teardown (void)
{
  ddsi_fini (&gv);
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

static void sertype_free (struct ddsi_sertype *st) { (void) st; }
static void whc_free (struct ddsi_whc *whc) { (void) whc; }
static void whc_get_state (const struct ddsi_whc *whc, struct ddsi_whc_state *st)
{
  (void) whc;
  st->max_seq = st->min_seq = st->unacked_bytes = 0;
}
static uint32_t whc_remove_acked_messages (struct ddsi_whc *whc, ddsi_seqno_t max_drop_seq, struct ddsi_whc_state *whcst, struct ddsi_whc_node **deferred_free_list)
{
  (void) whc; (void) max_drop_seq; (void) whcst; (void) deferred_free_list;
  return 0;
}
static void whc_free_deferred_free_list (struct ddsi_whc *whc, struct ddsi_whc_node *deferred_free_list)
{
  (void) whc; (void) deferred_free_list;
}

static void ddsi_wraddrset_some_cases (int casenumber, int cost, bool wr_psmx, const int nrds[4])
{
#define UNILOC(k) { .kind = DDSI_LOCATOR_KIND_UDPv4, .address = {0,0,0,0, 0,0,0,0, 0,0,0,0, 192,16,1,k+1}, .port = 7410 }
#define PSMXLOC(k) { .kind = DDSI_LOCATOR_KIND_PSMX, .address = {k+1,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0}, .port = 123 }
  const ddsi_locator_t ucloc[4] = { UNILOC(0), UNILOC(1), UNILOC(2), UNILOC(3) };
  const ddsi_locator_t psmxloc[4] = { PSMXLOC(0), PSMXLOC(1), PSMXLOC(2), PSMXLOC(3) };
  const ddsi_locator_t mcloc = {
    .kind = DDSI_LOCATOR_KIND_UDPv4, .address = {0,0,0,0, 0,0,0,0, 0,0,0,0, 239,255,0,1}, .port = 7400
  };
#define UNILOC1(k) { .next = NULL, .loc = ucloc[k] }
  const struct ddsi_locators_one ucloc1[4] = { UNILOC1(0), UNILOC1(1), UNILOC1(2), UNILOC1(3) };
  const struct ddsi_locators_one mcloc1 = { .next = NULL, .loc = mcloc };
#define UNILOCS(k) { .n = 1, .first = (struct ddsi_locators_one *) &ucloc1[k], .last = (struct ddsi_locators_one *) &ucloc1[k] }
  const ddsi_locators_t uclocs[4] = { UNILOCS(0), UNILOCS(1), UNILOCS(2), UNILOCS(3) };
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
  const ddsi_plist_t plist_pp[4] = { PLIST_PP(0), PLIST_PP(1), PLIST_PP(2), PLIST_PP(3) };
  ddsi_guid_t wrppguid, rdppguid[4][3];

  setup_and_start ();
  ddsi_thread_state_awake (ddsi_lookup_thread_state(), &gv);
  ddsi_new_participant (&wrppguid, &gv, RTPS_PF_PRIVILEGED_PP | RTPS_PF_IS_DDSI2_PP, &plist_pp[0]);

  const struct ddsi_sertype st = {
    .ops = &(struct ddsi_sertype_ops){ .free = sertype_free },
    .serdata_ops = &(struct ddsi_serdata_ops){ NULL },
    .serdata_basehash = 0,
    .has_key = 0,
    .request_keyhash = 0,
    .is_memcpy_safe = 1,
    .allowed_data_representation = DDS_DATA_REPRESENTATION_RESTRICT_DEFAULT,
    .type_name = "Q",
    .gv = DDSRT_ATOMIC_VOIDP_INIT (&gv),
    .flags_refc = DDSRT_ATOMIC_UINT32_INIT (0),
    .base_sertype = NULL,
    .sizeof_type = 8,
    .data_type_props = DDS_DATA_TYPE_IS_MEMCPY_SAFE
  };

  struct ddsi_participant *pp = ddsi_entidx_lookup_participant_guid (gv.entity_index, &wrppguid);
  struct ddsi_writer *wr;
  ddsi_guid_t wrguid;
  struct ddsi_psmx_locators_set psmx_locs = { .length = 1, .locators = (struct ddsi_locator *) &psmxloc[0] };
  struct ddsi_whc whc = {
    .ops = &(struct ddsi_whc_ops){
      .get_state = whc_get_state,
      .remove_acked_messages = whc_remove_acked_messages,
      .free_deferred_free_list = whc_free_deferred_free_list,
      .free = whc_free
    }
  };
  ddsi_new_writer (&wr, &wrguid, NULL, pp, "Q", &st, &ddsi_default_qos_writer, &whc, NULL, NULL, wr_psmx ? &psmx_locs : NULL);
  assert (ddsi_entidx_lookup_writer_guid (gv.entity_index, &wrguid));

  struct ddsi_tran_conn fake_conn = {
    .m_factory = ddsi_factory_find_supported_kind (&gv, DDSI_LOCATOR_KIND_PSMX),
    .m_interf = &(struct ddsi_network_interface){
      .prefer_multicast = false,
      .priority = 1000000
    }
  };
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < nrds[i]; j++)
    {
      // real guids never have the first 32-bits 0, so these are guaranteed unique
      rdppguid[i][j] = (ddsi_guid_t){
        .prefix = { .u = { 0, (unsigned) i, (unsigned) j } },
        .entityid = { .u = DDSI_ENTITYID_PARTICIPANT }
      };
      struct ddsi_addrset *proxypp_as = ddsi_new_addrset ();
      ddsi_locator_t loc = ucloc[i]; loc.port = 1000 + (unsigned) j;
      ddsi_add_locator_to_addrset (&gv, proxypp_as, &loc);
      ddsi_add_locator_to_addrset (&gv, proxypp_as, &mcloc);
      ddsi_new_proxy_participant (&gv, &rdppguid[i][j], 0, NULL, proxypp_as, ddsi_ref_addrset (proxypp_as), &plist_pp[i], DDS_INFINITY, DDSI_VENDORID_ECLIPSE, 0, ddsrt_time_wallclock (), 1);
      assert (ddsi_entidx_lookup_proxy_participant_guid (gv.entity_index, &rdppguid[i][j]));

      const ddsi_guid_t rdguid = {
        .prefix = rdppguid[i][j].prefix,
        .entityid = { .u = DDSI_ENTITYID_ALLOCSTEP | DDSI_ENTITYID_SOURCE_USER | DDSI_ENTITYID_KIND_READER_NO_KEY }
      };
      ddsi_plist_t plist_rd = { .present = 0, .qos = ddsi_default_qos_reader };
      plist_rd.qos.present |= DDSI_QP_TOPIC_NAME | DDSI_QP_TYPE_NAME;
      plist_rd.qos.reliability.kind = DDS_RELIABILITY_RELIABLE;
      plist_rd.qos.topic_name = "Q";
      plist_rd.qos.type_name = "Q";
      struct ddsi_addrset *rd_as = ddsi_new_addrset ();
      ddsi_add_locator_to_addrset (&gv, rd_as, &loc);
      ddsi_add_locator_to_addrset (&gv, rd_as, &mcloc);
      if (i == 0)
      {
        // We haven't configured a fake interface for PSMX, which means add_locator can't map it
        // and ignores it.  We *know* we're not doing anything, so we can just fake-map it to
        // something else
        ddsi_add_xlocator_to_addrset (&gv, rd_as, &(ddsi_xlocator_t){ .conn = &fake_conn, .c = psmxloc[i] });
      }
#if DDS_HAS_SSM
      ddsi_new_proxy_reader (&gv, &rdppguid[i][j], &rdguid, rd_as, &plist_rd, ddsrt_time_wallclock (), 1, false);
#else
      ddsi_new_proxy_reader (&gv, &rdppguid[i][j], &rdguid, rd_as, &plist_rd, ddsrt_time_wallclock (), 1);
#endif
      assert (ddsi_entidx_lookup_proxy_reader_guid (gv.entity_index, &rdguid));
      ddsi_unref_addrset (rd_as);
    }
  }

  if (casenumber == 0)
    DDS_CLOG (DDS_LC_CONTENT, &gv.logconfig, "    #rd/host  cost: addresses\n");
  DDS_CLOG (DDS_LC_CONTENT, &gv.logconfig, "%2d  ", casenumber+1);
  for (int i = 0; i < 4; i++)
    DDS_CLOG (DDS_LC_CONTENT, &gv.logconfig, " %d", nrds[i]);
  DDS_CLOG (DDS_LC_CONTENT, &gv.logconfig, "  %2dms", cost);
  ddsi_log_addrset (&gv, DDS_LC_CONTENT, ": {", wr->as);
  DDS_CLOG (DDS_LC_CONTENT, &gv.logconfig, " }");
  if (wr->c.psmx_locators.length)
    DDS_CLOG (DDS_LC_CONTENT, &gv.logconfig, " + psmx");
  DDS_CLOG (DDS_LC_CONTENT, &gv.logconfig, "\n");

  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  stop_and_teardown ();
}

CU_Test (ddsi_wraddrset, some_cases)
{
  const struct {
    int nrds[4];
    int cost;
  } cases[] = {
    {{0, 0, 0, 0}, 7},
    {{1, 0, 0, 0}, 7},
    {{2, 0, 0, 0}, 7},
    {{0, 1, 1, 0}, 7},
    {{1, 1, 0, 0}, 8},
    {{1, 1, 1, 0}, 8},
    {{2, 1, 0, 0}, 12},
    {{2, 1, 1, 0}, 16},
    {{3, 1, 1, 0}, 16},
    {{2, 1, 1, 1}, 20}
  };
  for (size_t k = 0; k < sizeof (cases) / sizeof (cases[0]); k++)
  {
    ddsi_wraddrset_some_cases ((int) k, cases[k].cost, true, cases[k].nrds);
  }
  CU_PASS ("I want to keep this code, but I don't know yet what the test expectation should be ...");
}
