// Copyright(c) 2023 ZettaScale Technology and others
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

#include "CUnit/Theory.h"
#include "Space.h"
#include "test_util.h"

#include "dds/dds.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/heap.h"

#include "dds__entity.h"
#include "ddsi__addrset.h"
#include "ddsi__misc.h"
#include "ddsi__proxy_endpoint.h"
#include "ddsi__endpoint.h"

enum logger_state {
  LST_INACTIVE,
  LST_WRITE,
  LST_ACKNACK
};

struct logger_arg {
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  bool enabled;
  bool data_seen;
  bool acknack_seen;
  bool mc_for_data;
  const char *guidstr;
  enum logger_state state[2][2];
};

static void check_destination_addresses (const char *message, bool multicast)
{
  const char *as;
  if ((as = strrchr (message, '[')) == NULL)
    return;
  // skip until first address (we assume IPv4 and no spaces in the address representation)
  as++;
  while (*as && isspace ((unsigned char) *as))
    as++;
  int naddrs = 0;
  while (*as != ']')
  {
    char const * const astart = as;
    while (*as != ']' && !isspace ((unsigned char) *as))
      as++;
    // all default addresses, so 239.* is multicast and anything else is unicast
    const bool ismc = (strncmp (astart, "udp/239.", 8) == 0);
    CU_ASSERT_FATAL (ismc == multicast);
    naddrs++;
    while (*as && isspace ((unsigned char) *as))
      as++;
  }
  CU_ASSERT_FATAL (naddrs == 2);
}

static void logger (void *ptr, const dds_log_data_t *data)
{
  struct logger_arg * const arg = ptr;
  const char *msg = data->message - data->hdrsize;
  ddsrt_mutex_lock (&arg->lock);
  printf ("%s ", arg->enabled ? "+" : " ");
  fputs (msg, stdout);
  if (data->domid == DDS_DOMAIN_DEFAULT)
    goto skip;
  assert (data->domid <= 1);
  const char *thrend;
  if ((thrend = strchr (msg, ':')) == NULL || thrend == msg)
    goto skip;
  const char *thrname = thrend;
  while (thrname > msg && thrname[-1] != ' ' && thrname[-1] != ']')
    thrname--;
  int thridx;
  if (strncmp (thrname, "tev:", 4) == 0)
    thridx = 0;
  else
    thridx = 1;
  if (arg->enabled)
  {
    switch (arg->state[data->domid][thridx])
    {
      case LST_INACTIVE:
        if (thridx == 1 && ddsi_patmatch ("*write_sample*", msg) && ddsi_patmatch (arg->guidstr, msg))
          arg->state[data->domid][thridx] = LST_WRITE;
        else if (thridx == 0 && ddsi_patmatch ("*acknack*", msg) && ddsi_patmatch (arg->guidstr, msg))
          arg->state[data->domid][thridx] = LST_ACKNACK;
        break;
      case LST_WRITE:
        if (ddsi_patmatch ("*xpack_send*", msg))
        {
          check_destination_addresses (msg, arg->mc_for_data);
          arg->state[data->domid][thridx] = LST_INACTIVE;
          arg->data_seen = true;
          ddsrt_cond_broadcast (&arg->cond);
        }
        break;
      case LST_ACKNACK:
        if (ddsi_patmatch ("*xpack_send*", msg))
        {
          check_destination_addresses (msg, false);
          arg->state[data->domid][thridx] = LST_INACTIVE;
          arg->acknack_seen = true;
          ddsrt_cond_broadcast (&arg->cond);
        }
        break;
    }
  }
skip:
  ddsrt_mutex_unlock (&arg->lock);
}

CU_Test (ddsc_redundant_networking, uc_data_on_all_intfs)
{
  dds_return_t rc;
  struct logger_arg larg = {
    .enabled = false,
    .mc_for_data = false,
    .guidstr = NULL,
    .data_seen = false,
    .acknack_seen = false,
    .state = { { LST_INACTIVE, LST_INACTIVE }, {LST_INACTIVE, LST_INACTIVE } }
  };
  ddsrt_mutex_init (&larg.lock);
  ddsrt_cond_init (&larg.cond);
  dds_set_log_mask (DDS_LC_TRACE);
  dds_set_log_sink (&logger, &larg);
  dds_set_trace_sink (&logger, &larg);

  // start up domain with default config to discover the interface name
  // use a high value for "max auto participant index" to avoid spurious
  // failures caused by running several tests in parallel (using a unique
  // domain id would help, too, but where to find a unique id?)
  dds_entity_t dom_pub = dds_create_domain (0, "<General/>");
  CU_ASSERT_FATAL (dom_pub > 0);
  struct ddsi_domaingv *gv_pub = get_domaingv (dom_pub);
  CU_ASSERT_FATAL (gv_pub != NULL);
  // construct a configuration using this interface and the loopback
  // interface (we assume that the loopback interface exists and uses
  // 127.0.0.1)
  if (gv_pub->interfaces[0].loopback)
  {
    CU_PASS ("need two interfaces to test redundant networking");
    rc = dds_delete (dom_pub);
    CU_ASSERT_FATAL (rc == 0);
    dds_set_log_sink (NULL, NULL);
    dds_set_trace_sink (NULL, NULL);
    ddsrt_cond_destroy (&larg.cond);
    ddsrt_mutex_destroy (&larg.lock);
    return;
  }
  char *config = NULL;
  (void) ddsrt_asprintf (&config,
    "<General>"
    "  <Interfaces>"
    "    <NetworkInterface name=\"%s\"/>"
    "    <NetworkInterface address=\"127.0.0.1\"/>"
    "  </Interfaces>"
    "  <RedundantNetworking>true</RedundantNetworking>"
    "</General>"
    "<Discovery>"
    "  <ExternalDomainId>0</ExternalDomainId>"
    "  <Tag>${CYCLONEDDS_PID}</Tag>"
    "</Discovery>"
    "<Tracing><Category>trace</Category></Tracing>",
    gv_pub->interfaces[0].name);
  rc = dds_delete (dom_pub);
  CU_ASSERT_FATAL (rc == 0);

  // Start up a new domain with this new configuration, get gv pointer (if only
  // to avoid a dangling pointer)
  dom_pub = dds_create_domain (0, config);
  CU_ASSERT_FATAL (dom_pub > 0);
  gv_pub = get_domaingv (dom_pub);
  CU_ASSERT_FATAL (gv_pub != NULL);
  const dds_entity_t dom_sub = dds_create_domain (1, config);
  CU_ASSERT_FATAL (dom_sub > 0);
  struct ddsi_domaingv * const gv_sub = get_domaingv (dom_sub);
  CU_ASSERT_FATAL (gv_sub != NULL);
  ddsrt_free (config);
  
  // Redundant logic networking treats loopback specially because that one is
  // not subject to the types of failure that redundancy is used for.  Here
  // that is a problem, because it means we require two real interfaces to test
  // things and we can't count on having two.
  //
  // Overriding the "loopback" flag in the interface will force it treat it
  // as a real network instead. We know we are running on a single machine and
  // not communicating with any other processes (thanks to the domain tag), so
  // this should not cause problems.
  for (int i = 0; i < gv_pub->n_interfaces; i++)
    gv_pub->interfaces[i].loopback = 0;
  for (int i = 0; i < gv_sub->n_interfaces; i++)
    gv_sub->interfaces[i].loopback = 0;

  const dds_entity_t pp_pub = dds_create_participant (0, NULL, NULL);
  CU_ASSERT_FATAL (pp_pub > 0);
  const dds_entity_t pp_sub = dds_create_participant (1, NULL, NULL);
  CU_ASSERT_FATAL (pp_sub > 0);
  char topicname[100];
  create_unique_topic_name ("redundant_networking", topicname, sizeof (topicname));
  dds_qos_t *qos = dds_create_qos ();
  dds_qset_reliability (qos, DDS_RELIABILITY_RELIABLE, DDS_INFINITY);
  const dds_entity_t tp_pub = dds_create_topic (pp_pub, &Space_Type1_desc, topicname, qos, NULL);
  const dds_entity_t tp_sub = dds_create_topic (pp_sub, &Space_Type1_desc, topicname, qos, NULL);
  dds_delete_qos (qos);

  const dds_entity_t wr = dds_create_writer (pp_pub, tp_pub, NULL, NULL);
  const dds_entity_t rd = dds_create_reader (pp_sub, tp_sub, NULL, NULL);
  sync_reader_writer (pp_sub, rd, pp_pub, wr);

  struct dds_entity *xent;
  rc = dds_entity_pin (wr, &xent);
  CU_ASSERT_FATAL (rc == 0 && dds_entity_kind (xent) == DDS_KIND_WRITER);
  struct dds_writer * const xwr = (struct dds_writer *) xent;
  // We expect exactly two unicast orelse two multicast addresses
  // (which ones we get depends on whether the network interface
  // supports multicast and on decisions in wraddrset)
  CU_ASSERT_FATAL ((ddsi_addrset_count_uc (xwr->m_wr->as) == 2 && ddsi_addrset_count_mc (xwr->m_wr->as) == 0) ||
                   (ddsi_addrset_count_uc (xwr->m_wr->as) == 0 && ddsi_addrset_count_mc (xwr->m_wr->as) == 2));
  const bool data_uses_mc = (ddsi_addrset_count_mc (xwr->m_wr->as) > 0);
  char guidstr[1 + 4 * 8 + 3 * 1 + 2];
  snprintf (guidstr, sizeof (guidstr), "*"PGUIDFMT"*", PGUID (xwr->m_entity.m_guid));
  dds_entity_unpin (xent);

  ddsrt_mutex_lock (&larg.lock);
  larg.enabled = true;
  larg.mc_for_data = data_uses_mc;
  larg.guidstr = guidstr;
  ddsrt_mutex_unlock (&larg.lock);

  rc = dds_write (wr, &(Space_Type1){0,0,0});
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_wait_for_acks (wr, DDS_INFINITY);
  CU_ASSERT_FATAL (rc == 0);

  // The ACK can be processed before the "xpack_send" line is output by the sending tev thread
  // this gives a bit of extra time
  dds_time_t waituntil = dds_time () + DDS_SECS (1);
  ddsrt_mutex_lock (&larg.lock);
  while (!larg.acknack_seen)
    ddsrt_cond_waituntil (&larg.cond, &larg.lock, waituntil);
  ddsrt_mutex_unlock (&larg.lock);

  dds_set_log_sink (NULL, NULL);
  dds_set_trace_sink (NULL, NULL);
  ddsrt_cond_destroy (&larg.cond);
  ddsrt_mutex_destroy (&larg.lock);

  CU_ASSERT_FATAL (larg.data_seen && larg.acknack_seen);

  rc = dds_delete (dom_sub);
  CU_ASSERT_FATAL (rc == 0);
  rc = dds_delete (dom_pub);
  CU_ASSERT_FATAL (rc == 0);
}
