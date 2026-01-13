// Copyright(c) 2025 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdio.h>
#include <ctype.h>

#include "dds/ddsi/ddsi_protocol.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsrt/time.h"
#include "dds__guid.h"
#include "ddsi__tran.h"
#include "test_common.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "dds/ddsi/ddsi_proxy_participant.h"
#include "dds/ddsi/ddsi_addrset.h"

#include "dds/dds.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/heap.h"
#include "ddsi__addrset.h"

static ddsrt_atomic_uint32_t framing_errors;

static void scan_for_framing_error (void *ptr, const dds_log_data_t *data)
{
  (void) ptr;
  //tprintf ("[%"PRIu32"] %s", data->domid, data->message);
  if (strstr (data->message, "framing error, dropping connection") != NULL)
    ddsrt_atomic_or32 (&framing_errors, 1u << data->domid);
}

static void do_tcp (void (*action) (dds_entity_t rd, dds_entity_t wr, void *arg), void *arg)
{
  const char *config_fmt =
    "<General>"
    "  <Interfaces>"
    "    <NetworkInterface address=\"127.0.0.1\"/>"
    "  </Interfaces>"
    "  <Transport>tcp</Transport>"
    "</General>"
    "<Discovery>"
    "  <ExternalDomainId>0</ExternalDomainId>"
    "  <Tag>${CYCLONEDDS_PID}</Tag>"
    "  <Peers>"
    "    %s"
    "  </Peers>"
    "</Discovery>"
    "<TCP>"
    "  <Port>%d</Port>"
    "</TCP>"
    "<Tracing><Category>trace,tcp</Category><OutputFile>stdout</OutputFile></Tracing>";

  ddsrt_atomic_st32 (&framing_errors, 0);
  dds_set_log_sink (scan_for_framing_error, NULL);
  dds_set_trace_sink (scan_for_framing_error, NULL);

  // Start up a new domain listening to an ephemeral TCP port
  char *configs = NULL;
  (void) ddsrt_asprintf (&configs, config_fmt, "", 0);
  const dds_entity_t doms = dds_create_domain (0, configs);
  ddsrt_free (configs);

  CU_ASSERT_GT_FATAL (doms, 0);
  struct ddsi_domaingv * const gvs = get_domaingv (doms);
  CU_ASSERT_NEQ_FATAL (gvs, NULL);

  // Create a second domain instance without a server socket and with the
  // first domain's ephemeral port as the peer
  char peerlocbuf[DDSI_LOCSTRLEN];
  char *peerstr = NULL;
  (void) ddsrt_asprintf (&peerstr, "<Peer address=\"%s\"/>", ddsi_locator_to_string (peerlocbuf, sizeof (peerlocbuf), &gvs->loc_meta_uc));
  char *configc = NULL;
  (void) ddsrt_asprintf (&configc, config_fmt, peerstr, -1);
  const dds_entity_t domc = dds_create_domain (1, configc);
  CU_ASSERT_GT_FATAL (domc, 0);
  ddsrt_free (configc);
  ddsrt_free (peerstr);

  const dds_entity_t pp_pub = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_GT_FATAL (pp_pub, 0);
  const dds_entity_t pp_sub = dds_create_participant (1, NULL, NULL);
  CU_ASSERT_GT_FATAL (pp_sub, 0);
  char topicname[100];
  create_unique_topic_name ("tcp_basic", topicname, sizeof (topicname));
  dds_qos_t *qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  const dds_entity_t tp_pub = dds_create_topic (pp_pub, &Space_Type1_desc, topicname, qos, NULL);
  const dds_entity_t tp_sub = dds_create_topic (pp_sub, &Space_Type1_desc, topicname, qos, NULL);
  dds_delete_qos (qos);

  const dds_entity_t wr = dds_create_writer (pp_pub, tp_pub, NULL, NULL);
  const dds_entity_t rd = dds_create_reader (pp_sub, tp_sub, NULL, NULL);
  sync_reader_writer (pp_sub, rd, pp_pub, wr);

  action (rd, wr, arg);

  dds_return_t rc;
  rc = dds_delete (domc);
  CU_ASSERT_EQ_FATAL (rc, 0);
  rc = dds_delete (doms);
  CU_ASSERT_EQ_FATAL (rc, 0);

  dds_set_log_sink (scan_for_framing_error, NULL);
  dds_set_trace_sink (scan_for_framing_error, NULL);
}

static void do_basic_write (dds_entity_t rd, dds_entity_t wr, void *varg)
{
  (void) varg;

  dds_return_t rc;
  rc = dds_write (wr, &(Space_Type1){345,678,101});
  CU_ASSERT_EQ_FATAL (rc, 0);

  const dds_entity_t ws = dds_create_waitset (dds_get_participant (rd));
  CU_ASSERT_GT_FATAL (ws, 0);
  rc = dds_set_status_mask (rd, DDS_DATA_AVAILABLE_STATUS);
  CU_ASSERT_EQ_FATAL (rc, 0);
  rc = dds_waitset_attach (ws, rd, 0);
  CU_ASSERT_EQ_FATAL (rc, 0);
  rc = dds_waitset_wait (ws, NULL, 0, DDS_MSECS (5000));
  CU_ASSERT_EQ (rc, 1);

  Space_Type1 sample;
  void *rawsample = &sample;
  dds_sample_info_t si;
  rc = dds_take (rd, &rawsample, &si, 1, 1);
  CU_ASSERT_EQ_FATAL (rc, 1);
  CU_ASSERT_FATAL(si.valid_data);
  CU_ASSERT_EQ_FATAL (sample.long_1, 345);
  CU_ASSERT_EQ_FATAL (sample.long_2, 678);
  CU_ASSERT_EQ_FATAL (sample.long_3, 101);
}

CU_Test (ddsc_tcp, basic)
{
  do_tcp (do_basic_write, NULL);
}

struct inject_arg {
  ddsrt_iovec_t data;
};

static void do_inject (dds_entity_t rd, dds_entity_t wr, void *varg)
{
  const struct inject_arg *arg = varg;
  struct ddsi_domaingv * const gvwr = get_domaingv (wr);

  // Find reader's proxy participant in writer's domain
  dds_guid_t rdguid;
  dds_get_guid (rd, &rdguid);
  ddsi_guid_t rdpp_guid = dds_guid_to_ddsi_guid (rdguid);
  rdpp_guid.entityid.u = DDSI_ENTITYID_PARTICIPANT;
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), gvwr);
  struct ddsi_proxy_participant *rdproxypp = ddsi_entidx_lookup_proxy_participant_guid (gvwr->entity_index, &rdpp_guid);
  CU_ASSERT_NEQ_FATAL (rdproxypp, NULL);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  // ... because we need the address
  ddsi_xlocator_t xloc;
  ddsi_addrset_any_uc (rdproxypp->as_default, &xloc);

  // Make writer's domain mute so we can inject garbage in the TCP connection without
  // interfering with RTPS messages being sent in the background
  dds_domain_set_deafmute (wr, false, true, DDS_INFINITY);

  DDSI_DECL_TRAN_WRITE_MSGFRAGS_PTR(mf, 1);
  mf->niov = 1;
  mf->iov[0] = arg->data;
  ddsi_conn_write (xloc.conn, &xloc.c, mf, 0);

  // logger should
  dds_domainid_t rd_domain_id;
  dds_return_t rc;
  rc = dds_get_domainid (rd, &rd_domain_id);
  CU_ASSERT_EQ_FATAL (rc, 0);
  while (ddsrt_atomic_ld32 (&framing_errors) == 0)
    dds_sleepfor (DDS_MSECS (10));
  CU_ASSERT_EQ_FATAL (ddsrt_atomic_ld32 (&framing_errors), (1u << rd_domain_id));
}

CU_Test (ddsc_tcp, inject_wrong_smid)
{
  unsigned char msg[] = {
    'R', 'T', 'P', 'S', /* version (don't care): */ 2,5,
    /* vendor id (don't care, 1.16 = Cyclone */ 1,16,
    /* GUID prefix (don't care) */ 1,16,3,4, 5,6,7,8, 9,10,11,12,
    /* SMID_ADLINK_MSG_LEN, big-endian, 4 octets-to-next-header */ 130,0,0,4,
    /* fake length */ 0x0,0x0,0x0,0x20
  };
  struct inject_arg arg = {
    .data = {
      .iov_len = 28,
      .iov_base = msg
    }
  };
  do_tcp (do_inject, &arg);
}

CU_Test (ddsc_tcp, inject_short_length)
{
  unsigned char msg[] = {
    'R', 'T', 'P', 'S', /* version (don't care): */ 2,5,
    /* vendor id (don't care, 1.16 = Cyclone */ 1,16,
    /* GUID prefix (don't care) */ 1,16,3,4, 5,6,7,8, 9,10,11,12,
    /* SMID_ADLINK_MSG_LEN, big-endian, 4 octets-to-next-header */ 129,0,0,4,
    /* fake length */ 0x0,0x0,0x0,0x3
  };
  struct inject_arg arg = {
    .data = {
      .iov_len = 28,
      .iov_base = msg
    }
  };
  do_tcp (do_inject, &arg);
}

CU_Test (ddsc_tcp, inject_oversize_length)
{
  unsigned char msg[] = {
    'R', 'T', 'P', 'S', /* version (don't care): */ 2,5,
    /* vendor id (don't care, 1.16 = Cyclone */ 1,16,
    /* GUID prefix (don't care) */ 1,16,3,4, 5,6,7,8, 9,10,11,12,
    /* SMID_ADLINK_MSG_LEN, big-endian, 4 octets-to-next-header */ 129,0,0,4,
    /* fake length */ 0x33,0x44,0x55,0x66
  };
  struct inject_arg arg = {
    .data = {
      .iov_len = 28,
      .iov_base = msg
    }
  };
  do_tcp (do_inject, &arg);
}
