// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>

#include "CUnit/Theory.h"
#include "dds/ddsrt/cdtors.h"
#include "dds/ddsrt/ifaddrs.h"
#include "dds/ddsi/ddsi_config.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_nwinterfaces.h"
#include "ddsi__fakeudp.h"
#include "ddsi__fakenet.h"
#include "ddsi__tran.h"

static const char fakeudp_topology[] =
  "<FakeNetwork>"
  "  <Host name=\"a\">"
  "    <Interface name=\"a0\" index=\"10\" address=\"10.0.0.1\" netmask=\"255.255.255.0\" flags=\"up,multicast\" type=\"wired\"/>"
  "    <Interface name=\"a1\" index=\"11\" address=\"10.0.1.1\" netmask=\"255.255.255.0\" flags=\"up,multicast\" type=\"wired\"/>"
  "  </Host>"
  "  <Host name=\"b\">"
  "    <Interface name=\"b0\" index=\"20\" address=\"10.0.0.2\" netmask=\"255.255.255.0\" flags=\"up,multicast\" type=\"wired\"/>"
  "  </Host>"
  "</FakeNetwork>";

static void set_ipv4_locator (ddsi_locator_t *loc, unsigned a, unsigned b, unsigned c, unsigned d)
{
  memset (loc, 0, sizeof (*loc));
  loc->kind = DDSI_LOCATOR_KIND_UDPv4;
  loc->port = DDSI_LOCATOR_PORT_INVALID;
  loc->address[12] = (unsigned char) a;
  loc->address[13] = (unsigned char) b;
  loc->address[14] = (unsigned char) c;
  loc->address[15] = (unsigned char) d;
}

static void init_gv (struct ddsi_domaingv *gv)
{
  memset (gv, 0, sizeof (*gv));
  ddsi_config_init_default (&gv->config);
  gv->config.transport_selector = DDSI_TRANS_FAKEUDP;
  gv->config.extended_packet_info = DDSI_BOOLDEF_FALSE;
  CU_ASSERT_EQ_FATAL (ddsi_fakeudp_init (gv), 0);
  gv->m_factory = ddsi_factory_find (gv, "udp");
  CU_ASSERT_FATAL (gv->m_factory != NULL);
}

static void fini_gv (struct ddsi_domaingv *gv)
{
  ddsi_tran_factories_fini (gv);
}

CU_Test (ddsi_fakeudp, selector_accepts_topology_modes)
{
  ddsrt_init ();
  struct ddsi_config default_cfg;
  struct ddsi_cfgst *cfgst = ddsi_config_init ("<General><Transport>fakeudp</Transport></General>", &default_cfg, 0);
  CU_ASSERT_NEQ_FATAL (cfgst, NULL);
  CU_ASSERT_EQ (default_cfg.transport_selector, DDSI_TRANS_FAKEUDP);
  CU_ASSERT_EQ (default_cfg.fake_network_topology_kind, DDSI_FAKENET_TOPOLOGY_BUILTIN);
  CU_ASSERT_EQ (default_cfg.fake_network_topology_file, NULL);
  ddsi_config_fini (cfgst);

  struct ddsi_config real_cfg;
  cfgst = ddsi_config_init ("<General><Transport>fakeudp:real</Transport></General>", &real_cfg, 0);
  CU_ASSERT_NEQ_FATAL (cfgst, NULL);
  CU_ASSERT_EQ (real_cfg.transport_selector, DDSI_TRANS_FAKEUDP);
  CU_ASSERT_EQ (real_cfg.fake_network_topology_kind, DDSI_FAKENET_TOPOLOGY_REAL);
  CU_ASSERT_EQ (real_cfg.fake_network_topology_file, NULL);
  ddsi_config_fini (cfgst);

  struct ddsi_config cfg;
  cfgst = ddsi_config_init ("<General><Transport>fakeudp:/tmp/fakenet.xml</Transport></General>", &cfg, 0);
  CU_ASSERT_NEQ_FATAL (cfgst, NULL);
  CU_ASSERT_EQ (cfg.transport_selector, DDSI_TRANS_FAKEUDP);
  CU_ASSERT_EQ (cfg.fake_network_topology_kind, DDSI_FAKENET_TOPOLOGY_FILE);
  CU_ASSERT_NEQ_FATAL (cfg.fake_network_topology_file, NULL);
  CU_ASSERT_STREQ (cfg.fake_network_topology_file, "/tmp/fakenet.xml");
  ddsi_config_fini (cfgst);

  struct ddsi_config bad_cfg;
  CU_ASSERT_EQ (ddsi_config_init ("<General><Transport>fakeudp:</Transport></General>", &bad_cfg, 0), NULL);
  ddsrt_fini ();
}

CU_Test (ddsi_fakeudp, enumerate_interfaces_from_builtin_default)
{
  ddsrt_init ();
  CU_ASSERT_EQ_FATAL (ddsi_fakenet_load_default (), 0);

  struct ddsi_domaingv gv;
  init_gv (&gv);
  ddsrt_ifaddrs_t *ifs = NULL;
  CU_ASSERT_EQ_FATAL (ddsi_enumerate_interfaces (gv.m_factory, gv.config.transport_selector, &ifs), 0);
  CU_ASSERT_FATAL (ifs != NULL);
  CU_ASSERT_FATAL (strcmp (ifs->name, "lo") == 0);
  CU_ASSERT_FATAL (ifs->next != NULL);
  CU_ASSERT_FATAL (strcmp (ifs->next->name, "fake0") == 0);
  CU_ASSERT_FATAL (ifs->next->next != NULL);
  CU_ASSERT_FATAL (strcmp (ifs->next->next->name, "fake1") == 0);
  CU_ASSERT_FATAL (ifs->next->next->next == NULL);
  ddsrt_freeifaddrs (ifs);
  fini_gv (&gv);

  ddsi_fakenet_clear ();
  ddsrt_fini ();
}

CU_Test (ddsi_fakeudp, enumerate_interfaces_from_xml)
{
  ddsrt_init ();
  CU_ASSERT_EQ_FATAL (ddsi_fakenet_load_xml_string (fakeudp_topology), 0);
  CU_ASSERT_EQ_FATAL (ddsi_fakenet_set_host ("a"), 0);

  struct ddsi_domaingv gv;
  init_gv (&gv);
  ddsrt_ifaddrs_t *ifs = NULL;
  CU_ASSERT_EQ_FATAL (ddsi_enumerate_interfaces (gv.m_factory, gv.config.transport_selector, &ifs), 0);
  CU_ASSERT_FATAL (ifs != NULL);
  CU_ASSERT_FATAL (strcmp (ifs->name, "a0") == 0);
  CU_ASSERT_FATAL (ifs->next != NULL);
  CU_ASSERT_FATAL (strcmp (ifs->next->name, "a1") == 0);
  CU_ASSERT_FATAL (ifs->next->next == NULL);
  ddsrt_freeifaddrs (ifs);
  fini_gv (&gv);

  ddsi_fakenet_clear ();
  ddsrt_fini ();
}

CU_Test (ddsi_fakeudp, unicast_packets_flow_between_fake_hosts)
{
  ddsrt_init ();
  CU_ASSERT_EQ_FATAL (ddsi_fakenet_load_xml_string (fakeudp_topology), 0);

  struct ddsi_domaingv gv;
  init_gv (&gv);
  struct ddsi_network_interface intf_a = { .if_index = 10, .name = "a0" };
  struct ddsi_network_interface intf_b = { .if_index = 20, .name = "b0" };
  set_ipv4_locator (&intf_a.loc, 10, 0, 0, 1);
  set_ipv4_locator (&intf_a.extloc, 10, 0, 0, 1);
  set_ipv4_locator (&intf_a.netmask, 255, 255, 255, 0);
  set_ipv4_locator (&intf_b.loc, 10, 0, 0, 2);
  set_ipv4_locator (&intf_b.extloc, 10, 0, 0, 2);
  set_ipv4_locator (&intf_b.netmask, 255, 255, 255, 0);

  struct ddsi_tran_conn *recv_conn = NULL;
  CU_ASSERT_EQ_FATAL (ddsi_fakenet_set_host ("b"), 0);
  const struct ddsi_tran_qos recv_qos = {
    .m_purpose = DDSI_TRAN_QOS_RECVXMIT_UC,
    .m_diffserv = 0,
    .m_interface = &intf_b
  };
  CU_ASSERT_EQ_FATAL (ddsi_factory_create_conn (&recv_conn, gv.m_factory, 7400, &recv_qos), DDS_RETCODE_OK);

  struct ddsi_tran_conn *send_conn = NULL;
  CU_ASSERT_EQ_FATAL (ddsi_fakenet_set_host ("a"), 0);
  const struct ddsi_tran_qos send_qos = {
    .m_purpose = DDSI_TRAN_QOS_XMIT_UC,
    .m_diffserv = 0,
    .m_interface = &intf_a
  };
  CU_ASSERT_EQ_FATAL (ddsi_factory_create_conn (&send_conn, gv.m_factory, 0, &send_qos), DDS_RETCODE_OK);

  unsigned char payload[] = { 1, 2, 3, 4, 5 };
  DDSI_DECL_CONST_TRAN_WRITE_MSGFRAGS_PTR (msgfrags, ((ddsrt_iovec_t){ .iov_base = payload, .iov_len = sizeof (payload) }));
  ddsi_locator_t dst = intf_b.loc;
  dst.port = 7400;
  size_t written = 0;
  CU_ASSERT_EQ_FATAL (ddsi_conn_write (send_conn, &dst, msgfrags, 0, &written), DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (written, sizeof (payload));

  unsigned char buf[16] = { 0 };
  size_t nread = 0;
  struct ddsi_network_packet_info pktinfo;
  CU_ASSERT_EQ_FATAL (ddsi_conn_read (recv_conn, buf, sizeof (buf), false, &pktinfo, &nread), DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (nread, sizeof (payload));
  CU_ASSERT_MEMEQ (buf, sizeof (payload), payload, sizeof (payload));
  CU_ASSERT_EQ (pktinfo.src.kind, DDSI_LOCATOR_KIND_UDPv4);
  CU_ASSERT_EQ (pktinfo.src.address[12], 10);
  CU_ASSERT_EQ (pktinfo.src.address[15], 1);

  ddsi_conn_free (send_conn);
  ddsi_conn_free (recv_conn);
  fini_gv (&gv);
  ddsi_fakenet_clear ();
  ddsrt_fini ();
}

CU_Test (ddsi_fakeudp, multicast_packets_flow_to_joined_interfaces)
{
  ddsrt_init ();
  CU_ASSERT_EQ_FATAL (ddsi_fakenet_load_xml_string (fakeudp_topology), 0);

  struct ddsi_domaingv gv;
  init_gv (&gv);
#if defined (IP_PKTINFO) && defined (CMSG_SPACE)
  gv.config.extended_packet_info = DDSI_BOOLDEF_TRUE;
#endif
  struct ddsi_network_interface intf_a = { .if_index = 10, .name = "a0", .allow_multicast = DDSI_AMC_TRUE };
  struct ddsi_network_interface intf_b = { .if_index = 20, .name = "b0", .allow_multicast = DDSI_AMC_TRUE };
  set_ipv4_locator (&intf_a.loc, 10, 0, 0, 1);
  set_ipv4_locator (&intf_a.extloc, 10, 0, 0, 1);
  set_ipv4_locator (&intf_a.netmask, 255, 255, 255, 0);
  set_ipv4_locator (&intf_b.loc, 10, 0, 0, 2);
  set_ipv4_locator (&intf_b.extloc, 10, 0, 0, 2);
  set_ipv4_locator (&intf_b.netmask, 255, 255, 255, 0);
  gv.n_interfaces = 1;
  gv.interfaces[0] = intf_b;

  ddsi_locator_t group;
  set_ipv4_locator (&group, 239, 255, 0, 1);
  group.port = 7400;

  struct ddsi_tran_conn *recv_conn = NULL;
  CU_ASSERT_EQ_FATAL (ddsi_fakenet_set_host ("b"), 0);
  const struct ddsi_tran_qos recv_qos = {
    .m_purpose = DDSI_TRAN_QOS_RECV_MC,
    .m_diffserv = 0,
    .m_bind_to_any = true
  };
  CU_ASSERT_EQ_FATAL (ddsi_factory_create_conn (&recv_conn, gv.m_factory, 7400, &recv_qos), DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (ddsi_conn_join_mc (recv_conn, NULL, &group, &gv.interfaces[0]), 0);

  struct ddsi_tran_conn *send_conn = NULL;
  CU_ASSERT_EQ_FATAL (ddsi_fakenet_set_host ("a"), 0);
  const struct ddsi_tran_qos send_qos = {
    .m_purpose = DDSI_TRAN_QOS_XMIT_MC,
    .m_diffserv = 0,
    .m_interface = &intf_a
  };
  CU_ASSERT_EQ_FATAL (ddsi_factory_create_conn (&send_conn, gv.m_factory, 0, &send_qos), DDS_RETCODE_OK);

  unsigned char payload[] = { 9, 8, 7 };
  DDSI_DECL_CONST_TRAN_WRITE_MSGFRAGS_PTR (msgfrags, ((ddsrt_iovec_t){ .iov_base = payload, .iov_len = sizeof (payload) }));
  size_t written = 0;
  CU_ASSERT_EQ_FATAL (ddsi_conn_write (send_conn, &group, msgfrags, 0, &written), DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (written, sizeof (payload));

  unsigned char buf[16] = { 0 };
  size_t nread = 0;
  struct ddsi_network_packet_info pktinfo;
  CU_ASSERT_EQ_FATAL (ddsi_conn_read (recv_conn, buf, sizeof (buf), false, &pktinfo, &nread), DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (nread, sizeof (payload));
  CU_ASSERT_MEMEQ (buf, sizeof (payload), payload, sizeof (payload));
  CU_ASSERT_EQ (pktinfo.src.kind, DDSI_LOCATOR_KIND_UDPv4);
  CU_ASSERT_EQ (pktinfo.src.address[12], 10);
  CU_ASSERT_EQ (pktinfo.src.address[15], 1);
#if defined (IP_PKTINFO) && defined (CMSG_SPACE)
  CU_ASSERT_EQ (pktinfo.dst.kind, DDSI_LOCATOR_KIND_UDPv4);
  CU_ASSERT_EQ (pktinfo.dst.address[12], 239);
  CU_ASSERT_EQ (pktinfo.dst.address[13], 255);
  CU_ASSERT_EQ (pktinfo.dst.address[15], 1);
  CU_ASSERT_EQ (pktinfo.if_index, 20);
#endif

  CU_ASSERT_EQ_FATAL (ddsi_conn_leave_mc (recv_conn, NULL, &group, &gv.interfaces[0]), 0);
  CU_ASSERT_EQ_FATAL (ddsi_conn_write (send_conn, &group, msgfrags, 0, &written), DDS_RETCODE_OK);
  CU_ASSERT_EQ (ddsi_conn_read (recv_conn, buf, sizeof (buf), true, &pktinfo, &nread), DDS_RETCODE_TRY_AGAIN);

  ddsi_conn_free (send_conn);
  ddsi_conn_free (recv_conn);
  fini_gv (&gv);
  ddsi_fakenet_clear ();
  ddsrt_fini ();
}
