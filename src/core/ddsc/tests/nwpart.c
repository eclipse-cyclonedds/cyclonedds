// Copyright(c) 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <stdlib.h>

#include "CUnit/Theory.h"

#include "dds/dds.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsi/ddsi_iid.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_init.h"
#include "ddsi__nwpart.h"
#include "ddsi__udp.h"
#include "ddsi__thread.h"
#include "ddsi__misc.h"
#include "ddsi__addrset.h"
#include "ddsi__discovery.h"
#include "ddsi__discovery_endpoint.h"
#include "ddsi__plist.h"
#include "ddsi__tran.h"

#include "test_util.h"

// CMake scripting to extract test cases is a bit simplistic and can't filter
// based on DDS_HAS_NETWORK_PARTITIONS, so we have to implement each case as
// PASS if they are not included in the build.
#ifdef DDS_HAS_NETWORK_PARTITIONS
static int errcount;

static void null_log_sink (void *vcount, const dds_log_data_t *msg)
{
  int *count = vcount;
  printf ("%s", msg->message);
  (*count)++;
}

static void intf_init (struct ddsi_network_interface *intf, int index, bool allow_mc, bool weird_extloc)
{
  memset (intf, 0, sizeof (*intf));
  intf->loc.kind = DDSI_LOCATOR_KIND_UDPv4;
  intf->loc.port = 0;
  // IPv4 locator address format: 12 leading 0s
  // - index 0 is loopback, indices > 0 are regular networks
  // - internal address: 127.0.0.1 / 192.168.(index-1).1
  // - configured external address if weird_extloc: 127.0.0.1 / 192.169.(index-1).1
  // netmask is 255.0.0.0 / 255.255.255.0
  // loopback and index > 1 are multicast-capable (if allow_mc)
  // index 1 is never multicast-capable
  if (index <= 0)
  {
    intf->loc.address[12] = 127;
    intf->loc.address[13] = 0;
    intf->loc.address[14] = 0;
    intf->loc.address[15] = 1;
    intf->netmask.address[12] = 255;
    intf->netmask.address[13] = 0;
    intf->netmask.address[14] = 0;
    intf->netmask.address[15] = 0;
    intf->extloc = intf->loc;
  }
  else
  {
    intf->loc.address[12] = (index == 1) ? 193 : 192;
    intf->loc.address[13] = 168;
    intf->loc.address[14] = (index >= 2) ? (unsigned char) (index - 2) : 0;
    intf->loc.address[15] = 1;
    intf->netmask.address[12] = 255;
    intf->netmask.address[13] = 255;
    intf->netmask.address[14] = 255;
    intf->netmask.address[15] = 0;
    intf->extloc = intf->loc;
    if (weird_extloc)
      intf->extloc.address[13]++;
  }
  intf->netmask.kind = DDSI_LOCATOR_KIND_UDPv4;
  // if_index is whatever index the operating system assigned to it, we just put
  // something in that would (probably) lead to disaster if used as an index in
  // gv->interfaces
  intf->if_index = 1000u + (uint32_t) index;
  intf->mc_capable = (!allow_mc || index == 1) ? 0 : 1;
  intf->mc_flaky = 0;
  intf->point_to_point = 0;
  intf->loopback = (index == 0) ? 1 : 0;
  intf->prefer_multicast = 0;
  intf->priority = 0;
  // naming them as lo, nomc, eth0, eth1, ... in an attempt at reducing headaches
  // in remembering what's what when writing test definitions
  switch (index)
  {
    case 0:  (void) ddsrt_asprintf (&intf->name, "lo"); break;
    case 1:  (void) ddsrt_asprintf (&intf->name, "nomc"); break;
    default: (void) ddsrt_asprintf (&intf->name, "eth%d", index - 2); break;
  }
}

static void setup (struct ddsi_domaingv *gv, const struct ddsi_config *config, bool allow_mc, bool weird_extloc)
{
  ddsi_iid_init ();
  ddsi_thread_states_init ();

  // register the main thread, then claim it as spawned by Cyclone because the
  // internal processing has various asserts that it isn't an application thread
  // doing the dirty work
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  // coverity[missing_lock:FALSE]
  assert (thrst->state == DDSI_THREAD_STATE_LAZILY_CREATED);
  thrst->state = DDSI_THREAD_STATE_ALIVE;
  ddsrt_atomic_stvoidp (&thrst->gv, &gv);

  memset (gv, 0, sizeof (*gv));
  gv->config = *config;

  // UDP means:
  gv->config.publish_uc_locators = 1;
  gv->config.enable_uc_locators = 1;
  (void) ddsi_udp_init (gv);
  gv->m_factory = ddsi_factory_find (gv, "udp");
  assert (gv->m_factory != NULL);

  DDSRT_STATIC_ASSERT (4 <= MAX_XMIT_CONNS);
  gv->extmask.kind = DDSI_LOCATOR_KIND_INVALID;
  gv->n_interfaces = 4;
  for (int i = 0; i < gv->n_interfaces; i++)
  {
    intf_init (&gv->interfaces[i], i, allow_mc, weird_extloc);
    // minimalistic inialisation of xmit conns just so we can use xlocators and addrsets
    struct ddsi_tran_conn *fakeconn = ddsrt_malloc (sizeof (*fakeconn));
    fakeconn->m_factory = gv->m_factory;
    fakeconn->m_base.gv = gv;
    fakeconn->m_interf = &gv->interfaces[i];
    gv->xmit_conns[i] = fakeconn;
  }

  ddsi_config_prep (gv, NULL);
  dds_set_log_sink (null_log_sink, &errcount);
  dds_set_trace_sink (null_log_sink, &errcount);
  gv->logconfig.c.tracemask = gv->logconfig.c.mask = UINT32_MAX;
}

static void teardown (struct ddsi_domaingv *gv)
{
  // for some reason, GCC 12's analyzer thinks some of this is uninitialised
#if __GNUC__ >= 12
  DDSRT_WARNING_GNUC_OFF(analyzer-use-of-uninitialized-value)
#endif
  for (int i = 0; i < gv->n_interfaces; i++)
  {
    ddsrt_free (gv->xmit_conns[i]);
    ddsrt_free (gv->interfaces[i].name);
  }
  while (gv->ddsi_tran_factories)
  {
    struct ddsi_tran_factory *f = gv->ddsi_tran_factories;
    gv->ddsi_tran_factories = f->m_factory;
    ddsi_factory_free (f);
  }
#if __GNUC__ >= 12
  DDSRT_WARNING_GNUC_ON(analyzer-use-of-uninitialized-value)
#endif

  // On shutdown, there is an expectation that the thread was discovered dynamically.
  // We overrode it in the setup code, we undo it now.
  // coverity[missing_lock:FALSE]
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();
  thrst->state = DDSI_THREAD_STATE_LAZILY_CREATED;
  ddsi_thread_states_fini ();
  ddsi_iid_fini ();
}

static bool check_address_list (const char *expected, struct ddsi_networkpartition_address *as)
{
  char *copy = ddsrt_strdup (expected), *cursor = copy, *tok;
  while ((tok = ddsrt_strsep (&cursor, ",")) != NULL)
  {
    if (as == NULL) {
      printf ("check_address_list: too few addresses\n");
      goto err;
    }
    char buf[DDSI_LOCSTRLEN];
    ddsi_locator_to_string (buf, sizeof (buf), &as->loc);
    if (strcmp (tok, buf) != 0) {
      printf ("check_address_list: expected %s, got %s\n", tok, buf);
      goto err;
    }
    as = as->next;
  }
  if (as != NULL) {
    printf ("check_address_list: too many addresses\n");
    goto err;
  }
  ddsrt_free (copy);
  return true;
err:
  ddsrt_free (copy);
  return false;
}

CU_TheoryDataPoints(ddsc_nwpart, definition) = {
  CU_DataPoints(struct ddsi_config_networkpartition_listelem,
    { .name = "p0", .address_string = "192.168.1.1", .interface_names = "eth1" },  // both unicast & interface
    { .name = "p1", .address_string = "", .interface_names = "not_an_interface" }, // non-existent interface
    { .name = "p2", .address_string = "239.255.0.8", .interface_names = "nomc" },  // multicast address on non-mc intf
    { .name = "p3", .address_string = "239.255.0.8,193.168.0.1", .interface_names = "" }, // multicast address on non-mc intf
    { .name = "p4", .address_string = "239.255.0.8", .interface_names = "" },      // multicast address, no mc-capable intf
    { .name = "p5", .address_string = "7.7.7.7", .interface_names = "" },          // no matching interface
    { .name = "p6", .address_string = "192.168.1.7", .interface_names = "" },      // network part of address, but host part non-zero
    //
    { .name = "p7", .address_string = "192.168.1.1", .interface_names = "" },
    { .name = "p8", .address_string = "", .interface_names = "lo,eth0" },
    { .name = "p9", .address_string = "239.255.0.8", .interface_names = "eth1,lo,eth0" },
    { .name = "pA", .address_string = "192.169.1.1", .interface_names = "" },     // matching should also work on external address
    { .name = "pB", .address_string = "192.168.1.0", .interface_names = "" },     // only network part of address
    { .name = "pC", .address_string = "239.255.0.4,239.255.0.5", .interface_names = "" }),
  CU_DataPoints(bool, // allow_mc
    true, true, true, true, false, true, true,
    true, true, true, true, true, true),
  CU_DataPoints(const char *, // uc addresses
    // expecting an error
    NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    // note: output is expected to be "external" address
    // input order is maintained in lists of addresses in network partition
    "udp/192.169.1.1:31415",
    "udp/127.0.0.1:31415,udp/192.169.0.1:31415",
    "udp/192.169.1.1:31415,udp/127.0.0.1:31415,udp/192.169.0.1:31415",
    "udp/192.169.1.1:31415",
    "udp/192.169.1.1:31415",
    ""),
  CU_DataPoints(const char *, // mc addresses
    // expecting an error
    NULL, NULL, NULL, NULL, NULL, NULL, NULL,
    // note: output is expected to be "external" address
    "",
    "",
    "udp/239.255.0.8:7401",
    "",
    "",
    "udp/239.255.0.4:7401,udp/239.255.0.5:7401")
};
#endif
CU_Theory ((struct ddsi_config_networkpartition_listelem ps, bool allow_mc, const char *uc, const char *mc), ddsc_nwpart, definition)
{
#ifndef DDS_HAS_NETWORK_PARTITIONS
  CU_PASS ("no network partitions in build");
#else
  // Low-level trickery so we can control the addresses/network interfaces without
  // having any system dependency
  assert ((uc == NULL) == (mc == NULL));
  printf ("test: %s\n", ps.name);
  struct ddsi_domaingv gv;
  struct ddsi_config config;
  ddsi_config_init_default (&config);
  config.networkPartitions = &ps;
  errcount = 0;
  setup (&gv, &config, allow_mc, true);
  int rc = ddsi_convert_nwpart_config (&gv, 31415);
  if (uc == NULL) {
    CU_ASSERT_FATAL (rc < 0);
    CU_ASSERT_FATAL (errcount > 0);
  } else {
    CU_ASSERT_FATAL (rc == 0);
    CU_ASSERT_FATAL (errcount == 0);
    CU_ASSERT_FATAL (check_address_list (uc, ps.uc_addresses));
    CU_ASSERT_FATAL (check_address_list (mc, ps.asm_addresses));
  }
  ddsi_free_config_nwpart_addresses (&gv);
  teardown (&gv);
#endif
}

CU_Test (ddsc_nwpart, duplicate)
{
#ifndef DDS_HAS_NETWORK_PARTITIONS
  CU_PASS ("no network partitions in build");
#else
  // network partition names are case-insensitive, it appears,
  // so make the duplicate name use a different case
  const char *config =
    "<Partitioning>"
    "  <NetworkPartitions>"
    "    <NetworkPartition name=\"p2\" address=\"239.255.0.13\"/>"
    "    <NetworkPartition name=\"p1\" address=\"239.255.0.12\"/>"
    "    <NetworkPartition name=\"P2\" address=\"239.255.0.11\"/>"
    "  </NetworkPartitions>"
    "</Partitioning>";
  dds_entity_t eh = dds_create_domain (0, config);
  CU_ASSERT_FATAL (eh < 0);
#endif
}

CU_Test (ddsc_nwpart, mapping_undefined)
{
#ifndef DDS_HAS_NETWORK_PARTITIONS
  CU_PASS ("no network partitions in build");
#else
  // Network partition names are case-insensitive, it appears,
  // so make the duplicate name use a different case.
  //
  // Test depends on multicast support, but most regular machines
  // and the CI all support it, so that is not an issue in practice.
  const char *config =
    "${CYCLONEDDS_URI},<Partitioning>"
    "  <NetworkPartitions>"
    "    <NetworkPartition name=\"p2\" address=\"239.255.0.13\"/>"
    "    <NetworkPartition name=\"p1\" address=\"239.255.0.12\"/>"
    "    <NetworkPartition name=\"p0\" address=\"239.255.0.11\"/>"
    "  </NetworkPartitions>"
    "  <PartitionMappings>"
    "    <PartitionMapping DCPSPartitionTopic=\"a.b\" networkpartition=\"pX\"/>"
    "  </PartitionMappings>"
    "</Partitioning>";
  char *config1 = ddsrt_expand_envvars (config, 0);
  dds_entity_t eh = dds_create_domain (0, config1);
  ddsrt_free (config1);
  CU_ASSERT_FATAL (eh < 0);
#endif
}

CU_Test (ddsc_nwpart, mapping_multiple)
{
#ifndef DDS_HAS_NETWORK_PARTITIONS
  CU_PASS ("no network partitions in build");
#else
  // Network partition names are case-insensitive, it appears,
  // so make the mapping use a different case.
  //
  // Test depends on multicast support, but most regular machines
  // and the CI all support it, so that is not an issue in practice.
  const char *config =
    "${CYCLONEDDS_URI},<Partitioning>"
    "  <NetworkPartitions>"
    "    <NetworkPartition name=\"p2\" address=\"239.255.0.13\"/>"
    "    <NetworkPartition name=\"p1\" address=\"239.255.0.12\"/>"
    "    <NetworkPartition name=\"p0\" address=\"239.255.0.11\"/>"
    "  </NetworkPartitions>"
    "  <PartitionMappings>"
    "    <PartitionMapping DCPSPartitionTopic=\"a.b\" networkpartition=\"P0\"/>"
    "    <PartitionMapping DCPSPartitionTopic=\"c.d\" networkpartition=\"p2\"/>"
    "  </PartitionMappings>"
    "</Partitioning>";
  char *config1 = ddsrt_expand_envvars (config, 0);
  dds_entity_t eh = dds_create_domain (0, config1);
  ddsrt_free (config1);
  CU_ASSERT_FATAL (eh > 0);
  struct ddsi_domaingv * const gv = get_domaingv (eh);
  // all of this is order preserving; check that the entries meet that expectation
  struct ddsi_config_partitionmapping_listelem *m0, *m1;
  m0 = gv->config.partitionMappings;
  m1 = m0->next;
  CU_ASSERT_FATAL (strcmp (m0->networkPartition, "P0") == 0);
  CU_ASSERT_FATAL (strcmp (m1->networkPartition, "p2") == 0);
  CU_ASSERT_FATAL (m1->next == NULL);
  struct ddsi_config_networkpartition_listelem *p0, *p1, *p2;
  p2 = gv->config.networkPartitions; // this order matches the names
  p1 = p2->next;
  p0 = p1->next;
  CU_ASSERT_FATAL (strcmp (p2->name, "p2") == 0);
  CU_ASSERT_FATAL (strcmp (p1->name, "p1") == 0);
  CU_ASSERT_FATAL (strcmp (p0->name, "p0") == 0);
  CU_ASSERT_FATAL (p0->next == NULL);
  // given that:
  CU_ASSERT_FATAL (m0->partition == p0);
  CU_ASSERT_FATAL (m1->partition == p2);
  dds_delete (eh);
#endif
}

struct check_address_present_arg {
  const char **expected;
  bool ok;
};

static void check_address_present (const ddsi_xlocator_t *loc, void *varg)
{
  struct check_address_present_arg *arg = varg;
  char buf[DDSI_LOCSTRLEN];
  ddsi_xlocator_to_string (buf, sizeof (buf), loc);
  printf (" %s", buf);
  int i = 0;
  while (arg->expected[i] && strcmp (arg->expected[i], buf) != 0)
    i++;
  if (arg->expected[i] == NULL)
    arg->ok = false;
}

CU_TheoryDataPoints(ddsc_nwpart, selected_addrs) = {
  CU_DataPoints(bool, // same machine = loopback applies
    true,  true,  false, false,
    true,  true,  false, false,
    true,  true,  false, false,
    true,  true,  false, false,
    true,  true,  false, false),
  CU_DataPoints(bool, // as_default has multicast address (see also # mc addrs)
    true,  false, true,  false,
    true,  false, true,  false,
    true,  false, true,  false,
    true,  false, true,  false,
    true,  false, true,  false),
  CU_DataPoints(int, // # uc addrs in SEDP: if 0, use as_default UC addrs for interfaces
    0,     0,     0,     0,
    2,     2,     2,     2,
    3,     3,     3,     3,
    3,     3,     3,     3,
    3,     3,     3,     3),
  CU_DataPoints(int, // # mc addrs in SEDP: if mc ok, then if 0 use as_default MC addr else use 239.255.0.2 (+.3)
    0,     1,     0,     1,
    1,     0,     1,     0,
    0,     0,     0,     0,
    1,     1,     1,     1,
    2,     2,     2,     2),
  CU_DataPoints(const char **, // expected result
    (const char *[]){ // same host,  mc in as_default: yes, SEDP 0 uc, 0 mc
      "udp/127.0.0.1:31416@1000", "udp/193.168.0.1:31416@1001", "udp/192.168.0.1:31416@1002", "udp/192.168.1.1:31416@1003",
      "udp/239.255.0.1:7401@1000", "udp/239.255.0.1:7401@1002", "udp/239.255.0.1:7401@1003", NULL },
    (const char *[]){ // same host,  mc in as_default: no,  SEDP 0 uc, 1 mc
      "udp/127.0.0.1:31416@1000", "udp/193.168.0.1:31416@1001", "udp/192.168.0.1:31416@1002", "udp/192.168.1.1:31416@1003",
      "udp/239.255.0.2:7401@1000", "udp/239.255.0.2:7401@1002", "udp/239.255.0.2:7401@1003", NULL },
    (const char *[]){ // other host, mc in as_default: yes, SEDP 0 uc, 0 mc
      "udp/193.168.0.2:31416@1001", "udp/192.168.0.2:31416@1002", "udp/192.168.1.2:31416@1003",
      "udp/239.255.0.1:7401@1002", "udp/239.255.0.1:7401@1003", NULL },
    (const char *[]){ // other host, mc in as_default: no,  SEDP 0 uc, 1 mc
      "udp/193.168.0.2:31416@1001", "udp/192.168.0.2:31416@1002", "udp/192.168.1.2:31416@1003",
      "udp/239.255.0.2:7401@1002", "udp/239.255.0.2:7401@1003", NULL },
    //
    (const char *[]){ // same host,  mc in as_default: yes, SEDP 2 uc, 1 mc
      "udp/127.0.0.1:31416@1000", "udp/193.168.0.1:31416@1001",
      "udp/239.255.0.2:7401@1000", NULL },
    (const char *[]){ // same host,  mc in as_default: no,  SEDP 2 uc, 0 mc
      "udp/127.0.0.1:31416@1000", "udp/193.168.0.1:31416@1001", NULL },
    (const char *[]){ // other host, mc in as_default: yes, SEDP 2 uc, 1 mc
      "udp/193.168.0.2:31416@1001", NULL },
    (const char *[]){ // other host, mc in as_default: no,  SEDP 2 uc, 0 mc
      "udp/193.168.0.2:31416@1001", NULL },
    //
    // Expected behaviour: if SEDP gives:
    // - no addresses, use ppant uni- and multicast addresses
    // - only multicast, use those for multicast and use ppant address for unicast
    // - only unicast, use only those (i.e., disable multicast for this reader, cos
    //   that's a useful feature but it cannot be configured otherwise given the
    //   information available in the protocol)
    // - both, use only those
    // So here we're not expecting multicast in the resulting addrset even though the
    // the proxy participant does allow it and one could be forgiven for thinking that
    // it would be inherited.
    (const char *[]){ // same host,  mc in as_default: yes, SEDP 3 uc, 0 mc
      "udp/127.0.0.1:31416@1000", "udp/193.168.0.1:31416@1001", "udp/192.168.0.1:31416@1002", NULL },
    (const char *[]){ // same host,  mc in as_default: no,  SEDP 3 uc, 0 mc
      "udp/127.0.0.1:31416@1000", "udp/193.168.0.1:31416@1001", "udp/192.168.0.1:31416@1002", NULL },
    (const char *[]){ // other host, mc in as_default: yes, SEDP 3 uc, 0 mc
      "udp/193.168.0.2:31416@1001", "udp/192.168.0.2:31416@1002", NULL },
    (const char *[]){ // other host, mc in as_default: no,  SEDP 3 uc, 0 mc
      "udp/193.168.0.2:31416@1001", "udp/192.168.0.2:31416@1002", NULL },
    //
    (const char *[]){ // same host,  mc in as_default: yes, SEDP 3 uc, 1 mc
      "udp/127.0.0.1:31416@1000", "udp/193.168.0.1:31416@1001", "udp/192.168.0.1:31416@1002",
      "udp/239.255.0.2:7401@1000", "udp/239.255.0.2:7401@1002", NULL },
    (const char *[]){ // same host,  mc in as_default: no,  SEDP 3 uc, 1 mc
      "udp/127.0.0.1:31416@1000", "udp/193.168.0.1:31416@1001", "udp/192.168.0.1:31416@1002",
      "udp/239.255.0.2:7401@1000", "udp/239.255.0.2:7401@1002", NULL },
    (const char *[]){ // other host, mc in as_default: yes, SEDP 3 uc, 1 mc
      "udp/193.168.0.2:31416@1001", "udp/192.168.0.2:31416@1002",
      "udp/239.255.0.2:7401@1002", NULL },
    (const char *[]){ // other host, mc in as_default: no,  SEDP 3 uc, 1 mc
      "udp/193.168.0.2:31416@1001", "udp/192.168.0.2:31416@1002",
      "udp/239.255.0.2:7401@1002", NULL },
    //
    (const char *[]){ // same host,  mc in as_default: yes, SEDP 3 uc, 2 mc
      "udp/127.0.0.1:31416@1000", "udp/193.168.0.1:31416@1001", "udp/192.168.0.1:31416@1002",
      "udp/239.255.0.2:7401@1000", "udp/239.255.0.2:7401@1002",
      "udp/239.255.0.3:7401@1000", "udp/239.255.0.3:7401@1002", NULL },
    (const char *[]){ // same host,  mc in as_default: no,  SEDP 3 uc, 2 mc
      "udp/127.0.0.1:31416@1000", "udp/193.168.0.1:31416@1001", "udp/192.168.0.1:31416@1002",
      "udp/239.255.0.2:7401@1000", "udp/239.255.0.2:7401@1002",
      "udp/239.255.0.3:7401@1000", "udp/239.255.0.3:7401@1002", NULL },
    (const char *[]){ // other host, mc in as_default: yes, SEDP 3 uc, 2 mc
      "udp/193.168.0.2:31416@1001", "udp/192.168.0.2:31416@1002",
      "udp/239.255.0.2:7401@1002",
      "udp/239.255.0.3:7401@1002", NULL },
    (const char *[]){ // other host, mc in as_default: no,  SEDP 3 uc, 2 mc
      "udp/193.168.0.2:31416@1001", "udp/192.168.0.2:31416@1002",
      "udp/239.255.0.2:7401@1002",
      "udp/239.255.0.3:7401@1002", NULL })
};

CU_Theory ((bool same_machine, bool proxypp_has_defmc, int n_ep_uc, int n_ep_mc, const char **expected), ddsc_nwpart, selected_addrs)
{
  // This arguably should be part of tests for interpreting discovery data in
  // some other file, but it also makes some sense to have a small sanity check
  // here as in Cyclone this all really integrates with the network partitions
  // and the rather complicated setup procedure with fake network interfaces
  // is needed in both cases because those play in validating/interpreting the
  // network partitions and in interpreting the lists of discovery addresses.
  //
  // So as long as this is the only test for this, it might as well be here.
  printf ("---------------\n");
  printf ("same_machine %d proxypp_has_defmc %d n_ep_uc %d n_ep_mc %d\n", same_machine, proxypp_has_defmc, n_ep_uc, n_ep_mc);
  struct ddsi_domaingv gv;
  struct ddsi_config config;
  ddsi_config_init_default (&config);
  config.transport_selector = DDSI_TRANS_UDP;
  config.allowMulticast = DDSI_AMC_TRUE;
  errcount = 0;
  memset (&gv, 0, sizeof (gv)); // solves a spurious gcc-12 "uninitialized" warning
  setup (&gv, &config, true, false);
  printf ("interfaces =\n");
  for (int i = 0; i < gv.n_interfaces; i++)
  {
    char buf[DDSI_LOCSTRLEN];
    printf ("  %s", ddsi_locator_to_string_no_port (buf, sizeof (buf), &gv.interfaces[i].loc));
    printf (" extern: %s", ddsi_locator_to_string_no_port (buf, sizeof (buf), &gv.interfaces[i].extloc));
    printf (" (%smc)\n", gv.interfaces[i].mc_capable ? "" : "no-");
  }

  // pretend the remote one is on another machine but on the same networks
  printf ("as_default =\n");
  struct ddsi_addrset *as_default = ddsi_new_addrset ();
  for (int i = (same_machine ? 0 : 1); i < gv.n_interfaces; i++)
  {
    ddsi_xlocator_t xloc = {
      .conn = gv.xmit_conns[i],
      .c = gv.interfaces[i].extloc
    };
    xloc.c.port = 31416;
    if (!same_machine && i > 0) // i = 0 => loopback => no change
      xloc.c.address[15]++;
    char buf[DDSI_LOCSTRLEN];
    printf ("  %s\n", ddsi_xlocator_to_string (buf, sizeof (buf), &xloc));
    ddsi_add_xlocator_to_addrset (&gv, as_default, &xloc);
  }

  const ddsi_locator_t defmcloc = {
    .kind = DDSI_LOCATOR_KIND_UDPv4,
    .port = 7401,
    .address = { 0,0,0,0, 0,0,0,0, 0,0,0,0, 239,255,0,1 }
  };
  if (proxypp_has_defmc)
  {
    for (int i = (same_machine ? 0 : 1); i < gv.n_interfaces; i++)
    {
      if (gv.interfaces[i].mc_capable)
      {
        ddsi_xlocator_t xloc = { .conn = gv.xmit_conns[i], .c = defmcloc };
        char buf[DDSI_LOCSTRLEN];
        printf ("  %s\n", ddsi_xlocator_to_string (buf, sizeof (buf), &xloc));
        ddsi_add_xlocator_to_addrset (&gv, as_default, &xloc);
      }
    }
  }

  // remote endpoint: advertise the first few interfaces in unicast addresses
  // (interfaces are: lo, nomc, eth0, eth1)
  ddsi_plist_t plist;
  ddsi_plist_init_empty (&plist);
  struct ddsi_locators_one uc[MAX_XMIT_CONNS];
  assert (n_ep_uc <= gv.n_interfaces);
  for (int i = 0; i < n_ep_uc; i++)
  {
    uc[i].next = &uc[i+1];
    uc[i].loc = gv.interfaces[i].extloc;
    uc[i].loc.port = 31416;
    if (!same_machine && i > 0) // i = 0 => loopback => no change
      uc[i].loc.address[15]++; // see as_default above
  }
  if (n_ep_uc > 0)
  {
    uc[n_ep_uc-1].next = NULL;
    plist.unicast_locators = (ddsi_locators_t){ .n = (uint32_t)n_ep_uc, .first = &uc[0], .last = &uc[n_ep_uc-1] };
    plist.present |= PP_UNICAST_LOCATOR;
  }

  struct ddsi_locators_one mc[2] = {
    { .next = &mc[1],
      .loc = {
        .kind = DDSI_LOCATOR_KIND_UDPv4,
        .port = 7401,
        .address = { 0,0,0,0, 0,0,0,0, 0,0,0,0, 239,255,0,2 }
      }
    },
    { .next = NULL,
      .loc = {
        .kind = DDSI_LOCATOR_KIND_UDPv4,
        .port = 7401,
        .address = { 0,0,0,0, 0,0,0,0, 0,0,0,0, 239,255,0,3 }
      }
    }
  };
  assert (n_ep_mc <= (int) (sizeof (mc) / sizeof (mc[0])));
  if (n_ep_mc > 0)
  {
    mc[n_ep_mc-1].next = NULL;
    plist.multicast_locators = (ddsi_locators_t){ .n = (uint32_t)n_ep_mc, .first = &mc[0], .last = &mc[n_ep_mc-1] };
    plist.present |= PP_MULTICAST_LOCATOR;
  }

  {
    char buf[1024];
    ddsi_plist_print (buf, sizeof (buf), &plist);
    printf ("advertised plist: %s\n", buf);
  }

  struct ddsi_addrset *as = ddsi_get_endpoint_addrset (&gv, &plist, as_default, NULL);

  int n = 0;
  while (expected[n])
    n++;
  struct check_address_present_arg arg = {
    .expected = expected,
    .ok = (ddsi_addrset_count (as) == (size_t) n)
  };
  printf ("addrset =");
  ddsi_addrset_forall (as, check_address_present, &arg);
  if (arg.ok)
    printf ("\nOK\n");
  else
  {
    printf ("\nexpected =");
    for (int i = 0; expected[i]; i++)
      printf (" %s", expected[i]);
    printf ("\n(in any order)\n");
  }
  CU_ASSERT (arg.ok);
  ddsi_unref_addrset (as);
  // not calling plist_fini: we didn't allocate anything
  ddsi_unref_addrset (as_default);
  teardown (&gv);
}

// ParticipantIndex none/auto and ManySocketsMode none/single take very different paths
// when making sockets and selecting port numbers.  I have been bitten by that difference,
// so better run through the set of combinations (MSM = none requires PI = none).
CU_TheoryDataPoints(ddsc_nwpart, full_stack_init) = {
  CU_DataPoints(const char *, "none", "auto", "none"),    // participant index
  CU_DataPoints(const char *, "single", "single", "none") // many sockets mode
};
CU_Theory ((const char *pistr, const char *msmstr), ddsc_nwpart, full_stack_init)
{
#ifndef DDS_HAS_NETWORK_PARTITIONS
  CU_PASS ("no network partitions in build");
#else
  dds_return_t rc;
  // start up domain with default config to discover the interface name
  // use a high value for "max auto participant index" to avoid spurious
  // failures caused by running several tests in parallel (using a unique
  // domain id would help, too, but where to find a unique id?)
  dds_entity_t eh = dds_create_domain (0, NULL);
  CU_ASSERT_FATAL (eh > 0);
  const struct ddsi_domaingv *gv = get_domaingv (eh);
  CU_ASSERT_FATAL (gv != NULL);
  assert (gv != NULL);
  // construct a configuration using this interface
  char *config = NULL;
  (void) ddsrt_asprintf (&config,
    "<General>"
    "  <Interfaces>"
    "    <NetworkInterface name=\"%s\"/>"
    "  </Interfaces>"
    "</General>"
    "<Discovery>"
    "  <MaxAutoParticipantIndex>100</MaxAutoParticipantIndex>"
    "  <ParticipantIndex>%s</ParticipantIndex>"
    "</Discovery>"
    "<Compatibility>"
    "  <ManySocketsMode>%s</ManySocketsMode>"
    "</Compatibility>"
    "<Partitioning>"
    "  <NetworkPartitions>"
    "    <NetworkPartition name=\"part\" address=\"239.255.0.13\" interface=\"%s\"/>"
    "  </NetworkPartitions>"
    "</Partitioning>",
    gv->interfaces[0].name,
    pistr,
    msmstr,
    gv->interfaces[0].name);
  rc = dds_delete (eh);
  CU_ASSERT_FATAL (rc == 0);
  // start up a new domain with this new configuration
  eh = dds_create_domain (0, config);
  ddsrt_free (config);
  CU_ASSERT_FATAL (eh > 0);
  gv = get_domaingv (eh);
  CU_ASSERT_FATAL (gv != NULL);
  assert (gv != NULL);
  // verify that the unicast address and port number in the network partition
  // are correct (this is slightly different from the other tests: those mock
  // most of the code, this uses the actual code)
  struct ddsi_config_networkpartition_listelem const * const np = gv->config.networkPartitions;
  struct ddsi_locator const * const nploc = &np->uc_addresses->loc;
  CU_ASSERT (memcmp (gv->interfaces[0].loc.address, nploc->address, sizeof (nploc->address)) == 0);
  CU_ASSERT (memcmp (gv->loc_default_uc.address, nploc->address, sizeof (nploc->address)) == 0);
  CU_ASSERT (gv->loc_default_uc.port == nploc->port);
  rc = dds_delete (eh);
  CU_ASSERT_FATAL (rc == 0);
#endif
}
