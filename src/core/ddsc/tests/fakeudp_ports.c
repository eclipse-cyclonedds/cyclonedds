// Copyright(c) 2026 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "CUnit/Theory.h"

#include "dds/dds.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/sockets.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_config.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_participant.h"
#include "dds/ddsi/ddsi_thread.h"
#include "ddsi__entity_index.h"
#include "ddsi__fakenet.h"
#include "ddsi__tran.h"
#include "dds__entity.h"
#include "test_util.h"

struct conn_expect {
  const char *addr;
  uint32_t port;
  const char *bind_addr;
  uint32_t bind_port;
};

struct domain_case {
  const char *name;
  const char *interfaces;
  int n_interfaces;
  const char *allow_multicast;
  const char *participant_index;
  const char *many_sockets_mode;
  int32_t exp_participant_index;
  struct conn_expect disc_uc[2];
  struct conn_expect data_uc[2];
  bool expect_mc;
  uint32_t disc_mc_port;
  uint32_t data_mc_port;
};

struct participant_case {
  const char *name;
  const struct domain_case *domain;
  bool expect_private_conns;
  struct conn_expect participant[2];
};

static const char intf_fake0[] =
  "<Interfaces>"
  "  <NetworkInterface name=\"fake0\"/>"
  "</Interfaces>";

static const char intf_fake0_fake1[] =
  "<Interfaces>"
  "  <NetworkInterface name=\"fake0\"/>"
  "  <NetworkInterface name=\"fake1\"/>"
  "</Interfaces>";

static const char intf_mixed_multicast[] =
  "<Interfaces>"
  "  <NetworkInterface name=\"fake0\" allow_multicast=\"true\"/>"
  "  <NetworkInterface name=\"fake1\" allow_multicast=\"false\"/>"
  "</Interfaces>";

static const struct domain_case one_default_single_mc = {
  .name = "one-default-single-mc",
  .interfaces = intf_fake0,
  .n_interfaces = 1,
  .allow_multicast = "true",
  .participant_index = "default",
  .many_sockets_mode = "single",
  .exp_participant_index = DDSI_PARTICIPANT_INDEX_NONE,
  .disc_uc = { { "192.0.2.1", 49152, "0.0.0.0", 49152 } },
  .data_uc = { { "192.0.2.1", 49152, "0.0.0.0", 49152 } },
  .expect_mc = true,
  .disc_mc_port = 7400,
  .data_mc_port = 7401
};

static const struct domain_case one_pi0_single_mc = {
  .name = "one-pi0-single-mc",
  .interfaces = intf_fake0,
  .n_interfaces = 1,
  .allow_multicast = "true",
  .participant_index = "0",
  .many_sockets_mode = "single",
  .exp_participant_index = 0,
  .disc_uc = { { "192.0.2.1", 7410, "0.0.0.0", 7410 } },
  .data_uc = { { "192.0.2.1", 7411, "0.0.0.0", 7411 } },
  .expect_mc = true,
  .disc_mc_port = 7400,
  .data_mc_port = 7401
};

static const struct domain_case one_default_single_no_mc = {
  .name = "one-default-single-no-mc",
  .interfaces = intf_fake0,
  .n_interfaces = 1,
  .allow_multicast = "false",
  .participant_index = "default",
  .many_sockets_mode = "single",
  .exp_participant_index = 0,
  .disc_uc = { { "192.0.2.1", 7410, "0.0.0.0", 7410 } },
  .data_uc = { { "192.0.2.1", 7411, "0.0.0.0", 7411 } },
  .expect_mc = false
};

static const struct domain_case two_pi0_single_mc = {
  .name = "two-pi0-single-mc",
  .interfaces = intf_fake0_fake1,
  .n_interfaces = 2,
  .allow_multicast = "true",
  .participant_index = "0",
  .many_sockets_mode = "single",
  .exp_participant_index = 0,
  .disc_uc = {
    { "192.0.2.1", 7410, "192.0.2.1", 7410 },
    { "198.51.100.1", 7410, "198.51.100.1", 7410 }
  },
  .data_uc = {
    { "192.0.2.1", 7411, "192.0.2.1", 7411 },
    { "198.51.100.1", 7411, "198.51.100.1", 7411 }
  },
  .expect_mc = true,
  .disc_mc_port = 7400,
  .data_mc_port = 7401
};

static const struct domain_case two_none_single_mc = {
  .name = "two-none-single-mc",
  .interfaces = intf_fake0_fake1,
  .n_interfaces = 2,
  .allow_multicast = "true",
  .participant_index = "none",
  .many_sockets_mode = "single",
  .exp_participant_index = DDSI_PARTICIPANT_INDEX_NONE,
  .disc_uc = {
    { "192.0.2.1", 49152, "192.0.2.1", 49152 },
    { "198.51.100.1", 49152, "198.51.100.1", 49152 }
  },
  .data_uc = {
    { "192.0.2.1", 49152, "192.0.2.1", 49152 },
    { "198.51.100.1", 49152, "198.51.100.1", 49152 }
  },
  .expect_mc = true,
  .disc_mc_port = 7400,
  .data_mc_port = 7401
};

static const struct domain_case two_auto_single_no_mc = {
  .name = "two-auto-single-no-mc",
  .interfaces = intf_fake0_fake1,
  .n_interfaces = 2,
  .allow_multicast = "false",
  .participant_index = "auto",
  .many_sockets_mode = "single",
  .exp_participant_index = 0,
  .disc_uc = {
    { "192.0.2.1", 7410, "192.0.2.1", 7410 },
    { "198.51.100.1", 7410, "198.51.100.1", 7410 }
  },
  .data_uc = {
    { "192.0.2.1", 7411, "192.0.2.1", 7411 },
    { "198.51.100.1", 7411, "198.51.100.1", 7411 }
  },
  .expect_mc = false
};

static const struct domain_case one_none_mode_mc = {
  .name = "one-none-mode-mc",
  .interfaces = intf_fake0,
  .n_interfaces = 1,
  .allow_multicast = "true",
  .participant_index = "default",
  .many_sockets_mode = "none",
  .exp_participant_index = DDSI_PARTICIPANT_INDEX_NONE,
  .disc_uc = { { "192.0.2.1", 7400, "0.0.0.0", 7400 } },
  .data_uc = { { "192.0.2.1", 7400, "0.0.0.0", 7400 } },
  .expect_mc = true,
  .disc_mc_port = 7400,
  .data_mc_port = 7400
};

static const struct domain_case one_none_mode_no_mc = {
  .name = "one-none-mode-no-mc",
  .interfaces = intf_fake0,
  .n_interfaces = 1,
  .allow_multicast = "false",
  .participant_index = "none",
  .many_sockets_mode = "none",
  .exp_participant_index = DDSI_PARTICIPANT_INDEX_NONE,
  .disc_uc = { { "192.0.2.1", 49152, "0.0.0.0", 49152 } },
  .data_uc = { { "192.0.2.1", 49152, "0.0.0.0", 49152 } },
  .expect_mc = false
};

static const struct domain_case one_pi0_many_mc = {
  .name = "one-pi0-many-mc",
  .interfaces = intf_fake0,
  .n_interfaces = 1,
  .allow_multicast = "true",
  .participant_index = "0",
  .many_sockets_mode = "many",
  .exp_participant_index = 0,
  .disc_uc = { { "192.0.2.1", 7410, "0.0.0.0", 7410 } },
  .data_uc = { { "192.0.2.1", 7411, "0.0.0.0", 7411 } },
  .expect_mc = true,
  .disc_mc_port = 7400,
  .data_mc_port = 7401
};

static const struct domain_case two_pi0_many_mc = {
  .name = "two-pi0-many-mc",
  .interfaces = intf_fake0_fake1,
  .n_interfaces = 2,
  .allow_multicast = "true",
  .participant_index = "0",
  .many_sockets_mode = "many",
  .exp_participant_index = 0,
  .disc_uc = {
    { "192.0.2.1", 7410, "192.0.2.1", 7410 },
    { "198.51.100.1", 7410, "198.51.100.1", 7410 }
  },
  .data_uc = {
    { "192.0.2.1", 7411, "192.0.2.1", 7411 },
    { "198.51.100.1", 7411, "198.51.100.1", 7411 }
  },
  .expect_mc = true,
  .disc_mc_port = 7400,
  .data_mc_port = 7401
};

static const struct participant_case participant_cases[] = {
  {
    .name = "one-pi0-single-mc",
    .domain = &one_pi0_single_mc,
    .expect_private_conns = false,
    .participant = {
      { "192.0.2.1", 7411, "0.0.0.0", 7411 },
      { "192.0.2.1", 7411, "0.0.0.0", 7411 }
    }
  },
  {
    .name = "two-pi0-single-mc",
    .domain = &two_pi0_single_mc,
    .expect_private_conns = false,
    .participant = {
      { "192.0.2.1", 7411, "192.0.2.1", 7411 },
      { "192.0.2.1", 7411, "192.0.2.1", 7411 }
    }
  },
  {
    .name = "one-pi0-many-mc",
    .domain = &one_pi0_many_mc,
    .expect_private_conns = true,
    .participant = {
      { "192.0.2.1", 49152, "0.0.0.0", 49152 },
      { "192.0.2.1", 49153, "0.0.0.0", 49153 }
    }
  },
  {
    .name = "two-pi0-many-mc",
    .domain = &two_pi0_many_mc,
    .expect_private_conns = true,
    .participant = {
      { "192.0.2.1", 49152, "0.0.0.0", 49152 },
      { "192.0.2.1", 49153, "0.0.0.0", 49153 }
    }
  }
};

static void addr_to_bytes (const char *addr, unsigned char bytes[4])
{
  unsigned a, b, c, d;
  CU_ASSERT_EQ_FATAL (sscanf (addr, "%u.%u.%u.%u", &a, &b, &c, &d), 4);
  CU_ASSERT_FATAL (a <= 255 && b <= 255 && c <= 255 && d <= 255);
  bytes[0] = (unsigned char) a;
  bytes[1] = (unsigned char) b;
  bytes[2] = (unsigned char) c;
  bytes[3] = (unsigned char) d;
}

static void assert_locator (const char *what, const ddsi_locator_t *loc, const char *addr, uint32_t port)
{
  unsigned char bytes[4];
  addr_to_bytes (addr, bytes);
  tprintf ("%s = %u.%u.%u.%u:%"PRIu32"\n", what,
           loc->address[12], loc->address[13], loc->address[14], loc->address[15], loc->port);
  CU_ASSERT_EQ_FATAL (loc->kind, DDSI_LOCATOR_KIND_UDPv4);
  CU_ASSERT_EQ_FATAL (loc->port, port);
  CU_ASSERT_MEMEQ (loc->address + 12, sizeof (bytes), bytes, sizeof (bytes));
}

static void assert_conn (const char *what, struct ddsi_tran_conn *conn, const struct conn_expect *exp)
{
  CU_ASSERT_NEQ_FATAL (conn, NULL);

  ddsi_locator_t loc;
  CU_ASSERT_EQ_FATAL (ddsi_conn_locator (conn, &loc), 0);
  assert_locator (what, &loc, exp->addr, exp->port);
  CU_ASSERT_EQ_FATAL (ddsi_conn_port (conn), exp->port);

  struct sockaddr_storage ss;
  memset (&ss, 0, sizeof (ss));
  socklen_t sslen = sizeof (ss);
  CU_ASSERT_EQ_FATAL (ddsi_fakenet_getsockname (ddsi_conn_handle (conn), (struct sockaddr *) &ss, &sslen), DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (((struct sockaddr *) &ss)->sa_family, AF_INET);

  unsigned char bytes[4];
  addr_to_bytes (exp->bind_addr, bytes);
  const struct sockaddr_in *sin = (const struct sockaddr_in *) &ss;
  CU_ASSERT_EQ_FATAL (ddsrt_sockaddr_get_port ((const struct sockaddr *) &ss), exp->bind_port);
  CU_ASSERT_MEMEQ (&sin->sin_addr.s_addr, sizeof (bytes), bytes, sizeof (bytes));
}

static char *make_config (const struct domain_case *tc, int ext_domain_id)
{
  char ext_domain[80];
  if (ext_domain_id >= 0)
    (void) snprintf (ext_domain, sizeof (ext_domain), "<ExternalDomainId>%d</ExternalDomainId>", ext_domain_id);
  else
    ext_domain[0] = 0;

  char *config = NULL;
  (void) ddsrt_asprintf (&config,
    "<General>"
    "  <Transport>fakeudp</Transport>"
    "  %s"
    "  <AllowMulticast>%s</AllowMulticast>"
    "</General>"
    "<Discovery>"
    "  %s"
    "  <ParticipantIndex>%s</ParticipantIndex>"
    "  <MaxAutoParticipantIndex>1</MaxAutoParticipantIndex>"
    "</Discovery>"
    "<Compatibility>"
    "  <ManySocketsMode>%s</ManySocketsMode>"
    "</Compatibility>",
    tc->interfaces, tc->allow_multicast, ext_domain, tc->participant_index, tc->many_sockets_mode);
  return config;
}

static dds_entity_t create_domain (dds_domainid_t domain_id, const struct domain_case *tc, int ext_domain_id)
{
  char *config = make_config (tc, ext_domain_id);
  tprintf ("fakeudp port case %s\n", tc->name);
  dds_entity_t domain = dds_create_domain (domain_id, config);
  ddsrt_free (config);
  return domain;
}

static void assert_domain_ports (const struct ddsi_domaingv *gv, const struct domain_case *tc)
{
  CU_ASSERT_EQ_FATAL (gv->config.participantIndex, tc->exp_participant_index);
  CU_ASSERT_EQ_FATAL (gv->n_interfaces, tc->n_interfaces);

  for (int i = 0; i < tc->n_interfaces; i++)
  {
    char what[64];
    (void) snprintf (what, sizeof (what), "%s disc_uc[%d]", tc->name, i);
    assert_conn (what, gv->disc_conn_uc[i], &tc->disc_uc[i]);
    (void) snprintf (what, sizeof (what), "%s data_uc[%d]", tc->name, i);
    assert_conn (what, gv->data_conn_uc[i], &tc->data_uc[i]);
    CU_ASSERT_EQ_FATAL (gv->xmit_conns_meta[i], gv->disc_conn_uc[i]);
    CU_ASSERT_EQ_FATAL (gv->xmit_conns_data[i], gv->data_conn_uc[i]);
  }

  assert_locator ("loc_meta_uc", &gv->loc_meta_uc, tc->disc_uc[0].addr, tc->disc_uc[0].port);
  assert_locator ("loc_default_uc", &gv->loc_default_uc, tc->data_uc[0].addr, tc->data_uc[0].port);

  if (tc->expect_mc)
  {
    struct conn_expect disc_mc = { "192.0.2.1", tc->disc_mc_port, "0.0.0.0", tc->disc_mc_port };
    struct conn_expect data_mc = { "192.0.2.1", tc->data_mc_port, "0.0.0.0", tc->data_mc_port };
    assert_conn ("disc_mc", gv->disc_conn_mc, &disc_mc);
    assert_conn ("data_mc", gv->data_conn_mc, &data_mc);
    assert_locator ("loc_spdp_mc", &gv->loc_spdp_mc, "239.255.0.1", tc->disc_mc_port);
    assert_locator ("loc_default_mc", &gv->loc_default_mc, "239.255.0.1", tc->data_mc_port);
  }
  else
  {
    CU_ASSERT_EQ_FATAL (gv->disc_conn_mc, NULL);
    CU_ASSERT_EQ_FATAL (gv->data_conn_mc, NULL);
  }
}

CU_TheoryDataPoints(ddsc_fakeudp_ports, domain_ports) = {
  CU_DataPoints(const struct domain_case *,
    &one_default_single_mc,
    &one_pi0_single_mc,
    &one_default_single_no_mc,
    &two_pi0_single_mc,
    &two_none_single_mc,
    &two_auto_single_no_mc,
    &one_none_mode_mc,
    &one_none_mode_no_mc)
};

CU_Theory((const struct domain_case *tc), ddsc_fakeudp_ports, domain_ports)
{
  ddsi_fakenet_clear ();
  dds_entity_t domain = create_domain (0, tc, -1);
  CU_ASSERT_GT_FATAL (domain, 0);

  const struct ddsi_domaingv *gv = get_domaingv (domain);
  CU_ASSERT_NEQ_FATAL (gv, NULL);
  assert_domain_ports (gv, tc);

  CU_ASSERT_EQ_FATAL (dds_delete (domain), DDS_RETCODE_OK);
  ddsi_fakenet_clear ();
}

static struct ddsi_participant *get_ddsi_participant (dds_entity_t ppent, const struct ddsi_domaingv *gv)
{
  dds_entity *e;
  CU_ASSERT_EQ_FATAL (dds_entity_pin (ppent, &e), DDS_RETCODE_OK);
  ddsi_thread_state_awake (ddsi_lookup_thread_state (), &e->m_domain->gv);
  struct ddsi_participant *pp = ddsi_entidx_lookup_participant_guid (gv->entity_index, &e->m_guid);
  ddsi_thread_state_asleep (ddsi_lookup_thread_state ());
  dds_entity_unpin (e);
  CU_ASSERT_NEQ_FATAL (pp, NULL);
  return pp;
}

CU_TheoryDataPoints(ddsc_fakeudp_ports, participant_ports) = {
  CU_DataPoints(const struct participant_case *,
    &participant_cases[0],
    &participant_cases[1],
    &participant_cases[2],
    &participant_cases[3])
};

CU_Theory((const struct participant_case *tc), ddsc_fakeudp_ports, participant_ports)
{
  ddsi_fakenet_clear ();
  dds_entity_t domain = create_domain (0, tc->domain, -1);
  CU_ASSERT_GT_FATAL (domain, 0);

  const struct ddsi_domaingv *gv = get_domaingv (domain);
  CU_ASSERT_NEQ_FATAL (gv, NULL);
  assert_domain_ports (gv, tc->domain);

  struct ddsi_tran_conn *ppconn[2];
  for (int i = 0; i < 2; i++)
  {
    dds_entity_t ppent = dds_create_participant (0, NULL, NULL);
    CU_ASSERT_GT_FATAL (ppent, 0);
    struct ddsi_participant *pp = get_ddsi_participant (ppent, gv);

    char what[64];
    (void) snprintf (what, sizeof (what), "%s participant[%d]", tc->name, i);
    if (tc->expect_private_conns)
    {
      CU_ASSERT_NEQ_FATAL (pp->m_conn, NULL);
      ppconn[i] = pp->m_conn;
      assert_locator (what, &pp->m_locator, tc->participant[i].addr, tc->participant[i].port);
    }
    else
    {
      CU_ASSERT_EQ_FATAL (pp->m_conn, NULL);
      ppconn[i] = gv->data_conn_uc[0];
    }
    assert_conn (what, ppconn[i], &tc->participant[i]);
  }

  if (tc->expect_private_conns)
  {
    CU_ASSERT_NEQ_FATAL (ppconn[0], gv->data_conn_uc[0]);
    CU_ASSERT_NEQ_FATAL (ppconn[1], gv->data_conn_uc[0]);
    CU_ASSERT_NEQ_FATAL (ppconn[0], ppconn[1]);
  }
  else
  {
    CU_ASSERT_EQ_FATAL (ppconn[0], gv->data_conn_uc[0]);
    CU_ASSERT_EQ_FATAL (ppconn[1], gv->data_conn_uc[0]);
    CU_ASSERT_EQ_FATAL (ppconn[0], ppconn[1]);
  }

  CU_ASSERT_EQ_FATAL (dds_delete (domain), DDS_RETCODE_OK);
  ddsi_fakenet_clear ();
}

CU_Test(ddsc_fakeudp_ports, auto_participant_index_rewinds_all_interfaces)
{
  ddsi_fakenet_clear ();
  dds_entity_t first = create_domain (0, &two_pi0_single_mc, -1);
  CU_ASSERT_GT_FATAL (first, 0);

  dds_entity_t second = create_domain (1, &two_auto_single_no_mc, 0);
  CU_ASSERT_GT_FATAL (second, 0);
  const struct ddsi_domaingv *gv = get_domaingv (second);
  CU_ASSERT_NEQ_FATAL (gv, NULL);

  struct domain_case expected = two_auto_single_no_mc;
  expected.exp_participant_index = 1;
  expected.disc_uc[0].port = expected.disc_uc[0].bind_port = 7412;
  expected.disc_uc[1].port = expected.disc_uc[1].bind_port = 7412;
  expected.data_uc[0].port = expected.data_uc[0].bind_port = 7413;
  expected.data_uc[1].port = expected.data_uc[1].bind_port = 7413;
  assert_domain_ports (gv, &expected);

  CU_ASSERT_EQ_FATAL (dds_delete (second), DDS_RETCODE_OK);
  CU_ASSERT_EQ_FATAL (dds_delete (first), DDS_RETCODE_OK);
  ddsi_fakenet_clear ();
}

CU_Test(ddsc_fakeudp_ports, many_sockets_none_rejects_mixed_multicast)
{
  struct domain_case mixed = one_none_mode_mc;
  mixed.name = "mixed-multicast-none-mode";
  mixed.interfaces = intf_mixed_multicast;
  mixed.n_interfaces = 2;

  ddsi_fakenet_clear ();
  dds_entity_t domain = create_domain (0, &mixed, -1);
  CU_ASSERT_LT_FATAL (domain, 0);
  ddsi_fakenet_clear ();
}
