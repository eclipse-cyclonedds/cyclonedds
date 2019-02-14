/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <ctype.h>
#include <stddef.h>

#include "os/os.h"

#include "util/ut_avl.h"
#include "util/ut_thread_pool.h"

#include "ddsi/q_md5.h"
#include "ddsi/q_protocol.h"
#include "ddsi/q_rtps.h"
#include "ddsi/q_misc.h"
#include "ddsi/q_config.h"
#include "ddsi/q_log.h"
#include "ddsi/q_plist.h"
#include "ddsi/q_unused.h"
#include "ddsi/q_bswap.h"
#include "ddsi/q_lat_estim.h"
#include "ddsi/q_bitset.h"
#include "ddsi/q_xevent.h"
#include "ddsi/q_addrset.h"
#include "ddsi/q_ddsi_discovery.h"
#include "ddsi/q_radmin.h"
#include "ddsi/q_error.h"
#include "ddsi/q_thread.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_lease.h"
#include "ddsi/q_gc.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_nwif.h"
#include "ddsi/q_globals.h"
#include "ddsi/q_xmsg.h"
#include "ddsi/q_receive.h"
#include "ddsi/q_pcap.h"
#include "ddsi/q_feature_check.h"
#include "ddsi/q_debmon.h"
#include "ddsi/q_init.h"

#include "ddsi/ddsi_tran.h"
#include "ddsi/ddsi_udp.h"
#include "ddsi/ddsi_tcp.h"
#include "ddsi/ddsi_raweth.h"
#include "ddsi/ddsi_mcgroup.h"
#include "ddsi/ddsi_serdata_default.h"

#include "ddsi/ddsi_tkmap.h"
#include "dds__whc.h"
#include "ddsi/ddsi_iid.h"

static void add_peer_addresses (struct addrset *as, const struct config_peer_listelem *list)
{
  while (list)
  {
    add_addresses_to_addrset (as, list->peer, -1, "add_peer_addresses", 0);
    list = list->next;
  }
}

static int make_uc_sockets (uint32_t * pdisc, uint32_t * pdata, int ppid)
{
  if (config.many_sockets_mode == MSM_NO_UNICAST)
  {
    assert (ppid == PARTICIPANT_INDEX_NONE);
    *pdata = *pdisc = (uint32_t) (config.port_base + config.port_dg * config.domainId.value);
    if (config.allowMulticast)
    {
      /* FIXME: ugly hack - but we'll fix up after creating the multicast sockets */
      return 0;
    }
  }

  if (ppid >= 0)
  {
    /* FIXME: verify port numbers are in range instead of truncating them like this */
    int base = config.port_base + (config.port_dg * config.domainId.value) + (ppid * config.port_pg);
    *pdisc = (uint32_t) (base + config.port_d1);
    *pdata = (uint32_t) (base + config.port_d3);
  }
  else if (ppid == PARTICIPANT_INDEX_NONE)
  {
    *pdata = 0;
    *pdisc = 0;
  }
  else
  {
    DDS_FATAL("make_uc_sockets: invalid participant index %d\n", ppid);
    return -1;
  }

  gv.disc_conn_uc = ddsi_factory_create_conn (gv.m_factory, *pdisc, NULL);
  if (gv.disc_conn_uc)
  {
    /* Check not configured to use same unicast port for data and discovery */

    if (*pdata != 0 && (*pdata != *pdisc))
    {
      gv.data_conn_uc = ddsi_factory_create_conn (gv.m_factory, *pdata, NULL);
    }
    else
    {
      gv.data_conn_uc = gv.disc_conn_uc;
    }
    if (gv.data_conn_uc == NULL)
    {
      ddsi_conn_free (gv.disc_conn_uc);
      gv.disc_conn_uc = NULL;
    }
    else
    {
      /* Set unicast locators */

      ddsi_conn_locator (gv.disc_conn_uc, &gv.loc_meta_uc);
      ddsi_conn_locator (gv.data_conn_uc, &gv.loc_default_uc);
    }
  }

  return gv.data_conn_uc ? 0 : -1;
}

static void make_builtin_endpoint_xqos (nn_xqos_t *q, const nn_xqos_t *template)
{
  nn_xqos_copy (q, template);
  q->reliability.kind = NN_RELIABLE_RELIABILITY_QOS;
  q->reliability.max_blocking_time = nn_to_ddsi_duration (100 * T_MILLISECOND);
  q->durability.kind = NN_TRANSIENT_LOCAL_DURABILITY_QOS;
}

static int set_recvips (void)
{
  gv.recvips = NULL;

  if (config.networkRecvAddressStrings)
  {
    if (os_strcasecmp (config.networkRecvAddressStrings[0], "all") == 0)
    {
#if OS_SOCKET_HAS_IPV6
      if (gv.ipv6_link_local)
      {
        DDS_WARNING("DDSI2EService/General/MulticastRecvNetworkInterfaceAddresses: using 'preferred' instead of 'all' because of IPv6 link-local address\n");
        gv.recvips_mode = RECVIPS_MODE_PREFERRED;
      }
      else
#endif
      {
        gv.recvips_mode = RECVIPS_MODE_ALL;
      }
    }
    else if (os_strcasecmp (config.networkRecvAddressStrings[0], "any") == 0)
    {
#if OS_SOCKET_HAS_IPV6
      if (gv.ipv6_link_local)
      {
        DDS_ERROR("DDSI2EService/General/MulticastRecvNetworkInterfaceAddresses: 'any' is unsupported in combination with an IPv6 link-local address\n");
        return -1;
      }
#endif
      gv.recvips_mode = RECVIPS_MODE_ANY;
    }
    else if (os_strcasecmp (config.networkRecvAddressStrings[0], "preferred") == 0)
    {
      gv.recvips_mode = RECVIPS_MODE_PREFERRED;
    }
    else if (os_strcasecmp (config.networkRecvAddressStrings[0], "none") == 0)
    {
      gv.recvips_mode = RECVIPS_MODE_NONE;
    }
#if OS_SOCKET_HAS_IPV6
    else if (gv.ipv6_link_local)
    {
      /* If the configuration explicitly includes the selected
       interface, treat it as "preferred", else as "none"; warn if
       interfaces other than the selected one are included. */
      int i, have_selected = 0, have_others = 0;
      for (i = 0; config.networkRecvAddressStrings[i] != NULL; i++)
      {
        nn_locator_t loc;
        if (ddsi_locator_from_string(&loc, config.networkRecvAddressStrings[i]) != AFSR_OK)
        {
          DDS_ERROR("%s: not a valid address in DDSI2EService/General/MulticastRecvNetworkInterfaceAddresses\n", config.networkRecvAddressStrings[i]);
          return -1;
        }
        if (compare_locators(&loc, &gv.interfaces[gv.selected_interface].loc) == 0)
          have_selected = 1;
        else
          have_others = 1;
      }
      gv.recvips_mode = have_selected ? RECVIPS_MODE_PREFERRED : RECVIPS_MODE_NONE;
      if (have_others)
      {
        DDS_WARNING("DDSI2EService/General/MulticastRecvNetworkInterfaceAddresses: using 'preferred' because of IPv6 local address\n");
      }
    }
#endif
    else
    {
      struct config_in_addr_node **recvnode = &gv.recvips;
      int i, j;
      gv.recvips_mode = RECVIPS_MODE_SOME;
      for (i = 0; config.networkRecvAddressStrings[i] != NULL; i++)
      {
        nn_locator_t loc;
        if (ddsi_locator_from_string(&loc, config.networkRecvAddressStrings[i]) != AFSR_OK)
        {
          DDS_ERROR("%s: not a valid address in DDSI2EService/General/MulticastRecvNetworkInterfaceAddresses\n", config.networkRecvAddressStrings[i]);
          return -1;
        }
        for (j = 0; j < gv.n_interfaces; j++)
        {
          if (compare_locators(&loc, &gv.interfaces[j].loc) == 0)
            break;
        }
        if (j == gv.n_interfaces)
        {
          DDS_ERROR("No interface bound to requested address '%s'\n", config.networkRecvAddressStrings[i]);
          return -1;
        }
        *recvnode = os_malloc (sizeof (struct config_in_addr_node));
        (*recvnode)->loc = loc;
        recvnode = &(*recvnode)->next;
        *recvnode = NULL;
      }
    }
  }
  return 0;
}

/* Apart from returning errors with the 'string' content is not valid, this
 * function can also return errors when a mismatch in multicast/unicast is
 * detected.
 *   return -1 : ddsi is unicast, but 'mc' indicates it expects multicast
 *   return  0 : ddsi is multicast, but 'mc' indicates it expects unicast
 * The return 0 means that the possible changes in 'loc' can be ignored. */
static int string_to_default_locator (nn_locator_t *loc, const char *string, uint32_t port, int mc, const char *tag)
{
  if (strspn (string, " \t") == strlen (string))
  {
    /* string consisting of just spaces and/or tabs (that includes the empty string) is ignored */
    return 0;
  }
  switch (ddsi_locator_from_string(loc, string))
  {
    case AFSR_OK:
      break;
    case AFSR_INVALID:
      DDS_ERROR("%s: not a valid address (%s)\n", string, tag);
      return -1;
    case AFSR_UNKNOWN:
      DDS_ERROR("%s: address name resolution failure (%s)\n", string, tag);
      return -1;
    case AFSR_MISMATCH:
      DDS_ERROR("%s: invalid address kind (%s)\n", string, tag);
      return -1;
  }
  if (port != 0 && !is_unspec_locator(loc))
    loc->port = port;
  else
    loc->port = NN_LOCATOR_PORT_INVALID;
  assert (mc == -1 || mc == 0 || mc == 1);
  if (mc >= 0)
  {
    const char *rel = mc ? "must" : "may not";
    const int ismc = is_unspec_locator (loc) || ddsi_is_mcaddr (loc);
    if (mc != ismc)
    {
      DDS_ERROR("%s: %s %s be the unspecified address or a multicast address\n", string, tag, rel);
      return -1;
    }
  }
  return 1;
}

static int set_spdp_address (void)
{
  const uint32_t port = (uint32_t) (config.port_base + config.port_dg * config.domainId.value + config.port_d0);
  int rc = 0;
  /* FIXME: FIXME: FIXME: */
  gv.loc_spdp_mc.kind = NN_LOCATOR_KIND_INVALID;
  if (strcmp (config.spdpMulticastAddressString, "239.255.0.1") != 0)
  {
    if ((rc = string_to_default_locator (&gv.loc_spdp_mc, config.spdpMulticastAddressString, port, 1, "SPDP address")) < 0)
      return rc;
  }
  if (rc == 0 && gv.m_factory->m_connless) /* FIXME: connless the right one? */
  {
    /* There isn't a standard IPv6 multicast group for DDSI. For
       some reason, node-local multicast addresses seem to be
       unsupported (ff01::... would be a node-local one I think), so
       instead do link-local. I suppose we could use the hop limit
       to make it node-local.  If other hosts reach us in some way,
       we'll of course respond. */
    rc = string_to_default_locator (&gv.loc_spdp_mc, gv.m_factory->m_default_spdp_address, port, 1, "SPDP address");
    assert (rc > 0);
  }
#ifdef DDSI_INCLUDE_SSM
  if (gv.loc_spdp_mc.kind != NN_LOCATOR_KIND_INVALID && ddsi_is_ssm_mcaddr (&gv.loc_spdp_mc))
  {
    DDS_ERROR("%s: SPDP address may not be an SSM address\n", config.spdpMulticastAddressString);
    return -1;
  }
#endif
  if (!(config.allowMulticast & AMC_SPDP) || config.suppress_spdp_multicast)
  {
    /* Explicitly disabling SPDP multicasting is always possible */
    set_unspec_locator (&gv.loc_spdp_mc);
  }
  return 0;
}

static int set_default_mc_address (void)
{
  const uint32_t port = (uint32_t) (config.port_base + config.port_dg * config.domainId.value + config.port_d2);
  int rc;
  if (!config.defaultMulticastAddressString)
    gv.loc_default_mc = gv.loc_spdp_mc;
  else if ((rc = string_to_default_locator (&gv.loc_default_mc, config.defaultMulticastAddressString, port, 1, "default multicast address")) < 0)
    return rc;
  else if (rc == 0)
    gv.loc_default_mc = gv.loc_spdp_mc;
  if (!(config.allowMulticast & ~AMC_SPDP))
  {
    /* no multicasting beyond SPDP */
    set_unspec_locator (&gv.loc_default_mc);
  }
  gv.loc_meta_mc = gv.loc_default_mc;
  return 0;
}

static int set_ext_address_and_mask (void)
{
  nn_locator_t loc;
  int rc;

  if (!config.externalAddressString)
    gv.extloc = gv.ownloc;
  else if ((rc = string_to_default_locator (&loc, config.externalAddressString, 0, 0, "external address")) < 0)
    return rc;
  else if (rc == 0) {
    DDS_WARNING("Ignoring ExternalNetworkAddress %s\n", config.externalAddressString);
    gv.extloc = gv.ownloc;
  } else {
    gv.extloc = loc;
  }

  if (!config.externalMaskString || strcmp (config.externalMaskString, "0.0.0.0") == 0)
  {
    memset(&gv.extmask.address, 0, sizeof(gv.extmask.address));
    gv.extmask.kind = NN_LOCATOR_KIND_INVALID;
    gv.extmask.port = NN_LOCATOR_PORT_INVALID;
  }
  else if (config.transport_selector != TRANS_UDP)
  {
    DDS_ERROR("external network masks only supported in IPv4 mode\n");
    return -1;
  }
  else
  {
    if ((rc = string_to_default_locator (&gv.extmask, config.externalMaskString, 0, -1, "external mask")) < 0)
      return rc;
  }
  return 0;
}

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
static int known_channel_p (const char *name)
{
  const struct config_channel_listelem *c;
  for (c = config.channels; c; c = c->next)
    if (strcmp (name, c->name) == 0)
      return 1;
  return 0;
}
#endif

static int check_thread_properties (void)
{
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  static const char *fixed[] = { "recv", "tev", "gc", "lease", "dq.builtins", "debmon", NULL };
  static const char *chanprefix[] = { "xmit.", "tev.","dq.",NULL };
#else
  static const char *fixed[] = { "recv", "tev", "gc", "lease", "dq.builtins", "xmit.user", "dq.user", "debmon", NULL };
#endif
  const struct config_thread_properties_listelem *e;
  int ok = 1, i;
  for (e = config.thread_properties; e; e = e->next)
  {
    for (i = 0; fixed[i]; i++)
      if (strcmp (fixed[i], e->name) == 0)
        break;
    if (fixed[i] == NULL)
    {
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
      /* Some threads are named after the channel, with names of the form PREFIX.CHAN */

      for (i = 0; chanprefix[i]; i++)
      {
        size_t n = strlen (chanprefix[i]);
        if (strncmp (chanprefix[i], e->name, n) == 0 && known_channel_p (e->name + n))
          break;
      }
      if (chanprefix[i] == NULL)
      {
        DDS_ERROR("config: DDSI2Service/Threads/Thread[@name=\"%s\"]: unknown thread\n", e->name);
        ok = 0;
      }
#else
      DDS_ERROR("config: DDSI2Service/Threads/Thread[@name=\"%s\"]: unknown thread\n", e->name);
      ok = 0;
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */
    }
  }
  return ok;
}

OS_WARNING_MSVC_OFF(4996);
int rtps_config_open (void)
{
    int status;

    if (config.tracingOutputFileName == NULL || *config.tracingOutputFileName == 0 || config.enabled_logcats == 0)
    {
        config.enabled_logcats = 0;
        config.tracingOutputFile = NULL;
        status = 1;
    }
    else if (os_strcasecmp (config.tracingOutputFileName, "stdout") == 0)
    {
        config.tracingOutputFile = stdout;
        status = 1;
    }
    else if (os_strcasecmp (config.tracingOutputFileName, "stderr") == 0)
    {
        config.tracingOutputFile = stderr;
        status = 1;
    }
    else if ((config.tracingOutputFile = fopen (config.tracingOutputFileName, config.tracingAppendToFile ? "a" : "w")) == NULL)
    {
        DDS_ERROR("%s: cannot open for writing\n", config.tracingOutputFileName);
        status = 0;
    }
    else
    {
        status = 1;
    }

    dds_set_log_mask(config.enabled_logcats);
    dds_set_trace_file(config.tracingOutputFile);

    return status;
}
OS_WARNING_MSVC_ON(4996);

int rtps_config_prep (struct cfgst *cfgst)
{
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  unsigned num_channels = 0;
  unsigned num_channel_threads = 0;
#endif

  /* retry_on_reject_duration default is dependent on late_ack_mode and responsiveness timeout, so fix up */
  if (config.whc_init_highwater_mark.isdefault)
    config.whc_init_highwater_mark.value = config.whc_lowwater_mark;
  if (config.whc_highwater_mark < config.whc_lowwater_mark ||
      config.whc_init_highwater_mark.value < config.whc_lowwater_mark ||
      config.whc_init_highwater_mark.value > config.whc_highwater_mark)
  {
    DDS_ERROR("Invalid watermark settings\n");
    goto err_config_late_error;
  }

  if (config.besmode == BESMODE_MINIMAL && config.many_sockets_mode == MSM_MANY_UNICAST)
  {
    /* These two are incompatible because minimal bes mode can result
       in implicitly creating proxy participants inheriting the
       address set from the ddsi2 participant (which is then typically
       inherited by readers/writers), but in many sockets mode each
       participant has its own socket, and therefore unique address
       set */
    DDS_ERROR ("Minimal built-in endpoint set mode and ManySocketsMode are incompatible\n");
    goto err_config_late_error;
  }

  /* Dependencies between default values is not handled
   automatically by the config processing (yet) */
  if (config.many_sockets_mode == MSM_MANY_UNICAST)
  {
    if (config.max_participants == 0)
      config.max_participants = 100;
  }
  if (NN_STRICT_P)
  {
    /* Should not be sending invalid messages when strict */
    config.respond_to_rti_init_zero_ack_with_invalid_heartbeat = 0;
    config.acknack_numbits_emptyset = 1;
  }
  if (config.max_queued_rexmit_bytes == 0)
  {
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
    if (config.auxiliary_bandwidth_limit == 0)
      config.max_queued_rexmit_bytes = 2147483647u;
    else
    {
      double max = (double) config.auxiliary_bandwidth_limit * ((double) config.nack_delay / 1e9);
      if (max < 0)
      {
        DDS_ERROR ("AuxiliaryBandwidthLimit * NackDelay = %g bytes is insane\n", max);
        goto err_config_late_error;
      }
      config.max_queued_rexmit_bytes = max > 2147483647.0 ? 2147483647u : (unsigned) max;
    }
#else
    config.max_queued_rexmit_bytes = 2147483647u;
#endif /* DDSI_INCLUDE_BANDWIDTH_LIMITING */
  }

  /* Verify thread properties refer to defined threads */
  if (!check_thread_properties ())
  {
    DDS_TRACE ("Could not initialise configuration\n");
    goto err_config_late_error;
  }

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  {
    /* Determine number of configured channels to be able to
     determine the correct number of threads.  Also fix fields if
     at default, and check for some known IPv4/IPv6
     "compatibility" issues */
    struct config_channel_listelem *chptr = config.channels;
    int error = 0;

    while (chptr)
    {
      size_t slen = strlen (chptr->name) + 5;
      char *thread_name = os_malloc (slen);
      (void) snprintf (thread_name, slen, "tev.%s", chptr->name);

      num_channels++;
      num_channel_threads += 2; /* xmit and dqueue */

      if (config.transport_selector != TRANS_UDP && chptr->diffserv_field != 0)
      {
        DDS_ERROR ("channel %s specifies IPv4 DiffServ settings which is incompatible with IPv6 use\n", chptr->name);
        error = 1;
      }

      if (
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
          chptr->auxiliary_bandwidth_limit > 0 ||
#endif
          lookup_thread_properties (thread_name))
        num_channel_threads++;

      os_free (thread_name);
      chptr = chptr->next;
    }
    if (error)
      goto err_config_late_error;
  }
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */

  /* Open tracing file after all possible config errors have been
   printed */
  if (! rtps_config_open ())
  {
    DDS_TRACE ("Could not initialise configuration\n");
    goto err_config_late_error;
  }

  /* Thread admin: need max threads, which is currently (2 or 3) for each
     configured channel plus 9: main, recv (up to 3x), dqueue.builtin,
     lease, gc, debmon; once thread state admin has been inited, upgrade the
     main thread one participating in the thread tracking stuff as
     if it had been created using create_thread(). */

  {
  /* Temporary: thread states for each application thread is managed using thread_states structure
  */
#define USER_MAX_THREADS 50

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
    const unsigned max_threads = 9 + USER_MAX_THREADS + num_channel_threads + config.ddsi2direct_max_threads;
#else
    const unsigned max_threads = 11 + USER_MAX_THREADS + config.ddsi2direct_max_threads;
#endif
    thread_states_init (max_threads);
  }

  /* Now the per-thread-log-buffers are set up, so print the configuration */
  config_print_cfgst (cfgst);
  return 0;

err_config_late_error:
  return -1;
}

struct joinleave_spdp_defmcip_helper_arg {
  int errcount;
  int dojoin;
};

static void joinleave_spdp_defmcip_helper (const nn_locator_t *loc, void *varg)
{
  struct joinleave_spdp_defmcip_helper_arg *arg = varg;
  if (!ddsi_is_mcaddr (loc))
    return;
#ifdef DDSI_INCLUDE_SSM
  /* Can't join SSM until we actually have a source */
  if (ddsi_is_ssm_mcaddr (loc))
    return;
#endif
  if (arg->dojoin) {
    if (ddsi_join_mc (gv.disc_conn_mc, NULL, loc) < 0 || ddsi_join_mc (gv.data_conn_mc, NULL, loc) < 0)
      arg->errcount++;
  } else {
    if (ddsi_leave_mc (gv.disc_conn_mc, NULL, loc) < 0 || ddsi_leave_mc (gv.data_conn_mc, NULL, loc) < 0)
      arg->errcount++;
  }
}

int joinleave_spdp_defmcip (int dojoin)
{
  /* Addrset provides an easy way to filter out duplicates */
  struct joinleave_spdp_defmcip_helper_arg arg;
  struct addrset *as = new_addrset ();
  arg.errcount = 0;
  arg.dojoin = dojoin;
  if (config.allowMulticast & AMC_SPDP)
    add_to_addrset (as, &gv.loc_spdp_mc);
  if (config.allowMulticast & ~AMC_SPDP)
    add_to_addrset (as, &gv.loc_default_mc);
  addrset_forall (as, joinleave_spdp_defmcip_helper, &arg);
  unref_addrset (as);
  if (arg.errcount)
  {
    DDS_ERROR("rtps_init: failed to join multicast groups for domain %d participant %d\n", config.domainId.value, config.participantIndex);
    return -1;
  }
  return 0;
}

int create_multicast_sockets(void)
{
  ddsi_tran_qos_t qos = ddsi_tran_create_qos ();
  ddsi_tran_conn_t disc, data;
  uint32_t port;
  qos->m_multicast = 1;

  /* FIXME: should check for overflow */
  port = (uint32_t) (config.port_base + config.port_dg * config.domainId.value + config.port_d0);
  if ((disc = ddsi_factory_create_conn (gv.m_factory, port, qos)) == NULL)
    goto err_disc;
  if (config.many_sockets_mode == MSM_NO_UNICAST)
  {
    /* FIXME: not quite logical to tie this to "no unicast" */
    data = disc;
  }
  else
  {
    port = (uint32_t) (config.port_base + config.port_dg * config.domainId.value + config.port_d2);
    if ((data = ddsi_factory_create_conn (gv.m_factory, port, qos)) == NULL)
      goto err_data;
  }
  ddsi_tran_free_qos (qos);

  gv.disc_conn_mc = disc;
  gv.data_conn_mc = data;
  DDS_TRACE("Multicast Ports: discovery %d data %d \n",
          ddsi_conn_port (gv.disc_conn_mc), ddsi_conn_port (gv.data_conn_mc));
  return 1;

err_data:
  ddsi_conn_free (disc);
err_disc:
  ddsi_tran_free_qos (qos);
  return 0;
}

static void rtps_term_prep (void)
{
  /* Stop all I/O */
  os_mutexLock (&gv.lock);
  if (gv.rtps_keepgoing)
  {
    gv.rtps_keepgoing = 0; /* so threads will stop once they get round to checking */
    os_atomic_fence ();
    /* can't wake up throttle_writer, currently, but it'll check every few seconds */
    trigger_recv_threads ();
  }
  os_mutexUnlock (&gv.lock);
}

struct wait_for_receive_threads_helper_arg {
  unsigned count;
};

static void wait_for_receive_threads_helper (struct xevent *xev, void *varg, nn_mtime_t tnow)
{
  struct wait_for_receive_threads_helper_arg * const arg = varg;
  if (arg->count++ == config.recv_thread_stop_maxretries)
    abort ();
  trigger_recv_threads ();
  resched_xevent_if_earlier (xev, add_duration_to_mtime (tnow, T_SECOND));
}

static void wait_for_receive_threads (void)
{
  struct xevent *trigev;
  unsigned i;
  struct wait_for_receive_threads_helper_arg cbarg;
  cbarg.count = 0;
  if ((trigev = qxev_callback (add_duration_to_mtime (now_mt (), T_SECOND), wait_for_receive_threads_helper, &cbarg)) == NULL)
  {
    /* retrying is to deal a packet geting lost because the socket buffer is full or because the
       macOS firewall (and perhaps others) likes to ask if the process is allowed to receive data,
       dropping the packets until the user approves. */
    DDS_WARNING("wait_for_receive_threads: failed to schedule periodic triggering of the receive threads to deal with packet loss\n");
  }
  for (i = 0; i < gv.n_recv_threads; i++)
  {
    if (gv.recv_threads[i].ts)
    {
      join_thread (gv.recv_threads[i].ts);
      /* setting .ts to NULL helps in sanity checking */
      gv.recv_threads[i].ts = NULL;
    }
  }
  if (trigev)
  {
    delete_xevent (trigev);
  }
}

static struct ddsi_sertopic *make_special_topic (uint16_t enc_id, const struct ddsi_serdata_ops *ops)
{
  /* FIXME: two things (at least)
     - it claims there is a key, but the underlying type description is missing
       that only works as long as it ends up comparing the keyhash field ...
       the keyhash field should be eliminated; but this can simply be moved over to an alternate
       topic class, it need not use the "default" one, that's mere expediency
     - initialising/freeing them here, in this manner, is not very clean
       it should be moved to somewhere in the topic implementation
       (kinda natural if they stop being "default" ones) */
  struct ddsi_sertopic_default *st = os_malloc (sizeof (*st));
  memset (st, 0, sizeof (*st));
  os_atomic_st32 (&st->c.refc, 1);
  st->c.ops = &ddsi_sertopic_ops_default;
  st->c.serdata_ops = ops;
  st->c.serdata_basehash = ddsi_sertopic_compute_serdata_basehash (st->c.serdata_ops);
  st->c.iid = ddsi_iid_gen ();
  st->native_encoding_identifier = enc_id;
  st->nkeys = 1;
  return (struct ddsi_sertopic *)st;
}

static void make_special_topics (void)
{
  gv.plist_topic = make_special_topic (PLATFORM_IS_LITTLE_ENDIAN ? PL_CDR_LE : PL_CDR_BE, &ddsi_serdata_ops_plist);
  gv.rawcdr_topic = make_special_topic (PLATFORM_IS_LITTLE_ENDIAN ? CDR_LE : CDR_BE, &ddsi_serdata_ops_rawcdr);
}

static void free_special_topics (void)
{
  ddsi_sertopic_unref (gv.plist_topic);
  ddsi_sertopic_unref (gv.rawcdr_topic);
}

static int setup_and_start_recv_threads (void)
{
  unsigned i;
  for (i = 0; i < MAX_RECV_THREADS; i++)
  {
    gv.recv_threads[i].ts = NULL;
    gv.recv_threads[i].arg.mode = RTM_SINGLE;
    gv.recv_threads[i].arg.rbpool = NULL;
    gv.recv_threads[i].arg.u.single.loc = NULL;
    gv.recv_threads[i].arg.u.single.conn = NULL;
  }

  /* First thread always uses a waitset and gobbles up all sockets not handled by dedicated threads - FIXME: MSM_NO_UNICAST mode with UDP probably doesn't even need this one to use a waitset */
  gv.n_recv_threads = 1;
  gv.recv_threads[0].name = "recv";
  gv.recv_threads[0].arg.mode = RTM_MANY;
  if (gv.m_factory->m_connless && config.many_sockets_mode != MSM_NO_UNICAST && config.multiple_recv_threads)
  {
    if (ddsi_is_mcaddr (&gv.loc_default_mc) && !ddsi_is_ssm_mcaddr (&gv.loc_default_mc))
    {
      /* Multicast enabled, but it isn't an SSM address => handle data multicasts on a separate thread (the trouble with SSM addresses is that we only join matching writers, which our own sockets typically would not be) */
      gv.recv_threads[gv.n_recv_threads].name = "recvMC";
      gv.recv_threads[gv.n_recv_threads].arg.mode = RTM_SINGLE;
      gv.recv_threads[gv.n_recv_threads].arg.u.single.conn = gv.data_conn_mc;
      gv.recv_threads[gv.n_recv_threads].arg.u.single.loc = &gv.loc_default_mc;
      ddsi_conn_disable_multiplexing (gv.data_conn_mc);
      gv.n_recv_threads++;
    }
    if (config.many_sockets_mode == MSM_SINGLE_UNICAST)
    {
      /* No per-participant sockets => handle data unicasts on a separate thread as well */
      gv.recv_threads[gv.n_recv_threads].name = "recvUC";
      gv.recv_threads[gv.n_recv_threads].arg.mode = RTM_SINGLE;
      gv.recv_threads[gv.n_recv_threads].arg.u.single.conn = gv.data_conn_uc;
      gv.recv_threads[gv.n_recv_threads].arg.u.single.loc = &gv.loc_default_uc;
      ddsi_conn_disable_multiplexing (gv.data_conn_uc);
      gv.n_recv_threads++;
    }
  }
  assert (gv.n_recv_threads <= MAX_RECV_THREADS);

  /* For each thread, create rbufpool and waitset if needed, then start it */
  for (i = 0; i < gv.n_recv_threads; i++)
  {
    /* We create the rbufpool for the receive thread, and so we'll
       become the initial owner thread. The receive thread will change
       it before it does anything with it. */
    if ((gv.recv_threads[i].arg.rbpool = nn_rbufpool_new (config.rbuf_size, config.rmsg_chunk_size)) == NULL)
    {
      DDS_ERROR("rtps_init: can't allocate receive buffer pool for thread %s\n", gv.recv_threads[i].name);
      goto fail;
    }
    if (gv.recv_threads[i].arg.mode == RTM_MANY)
    {
      if ((gv.recv_threads[i].arg.u.many.ws = os_sockWaitsetNew ()) == NULL)
      {
        DDS_ERROR("rtps_init: can't allocate sock waitset for thread %s\n", gv.recv_threads[i].name);
        goto fail;
      }
    }
    if ((gv.recv_threads[i].ts = create_thread (gv.recv_threads[i].name, recv_thread, &gv.recv_threads[i].arg)) == NULL)
    {
      DDS_ERROR("rtps_init: failed to start thread %s\n", gv.recv_threads[i].name);
      goto fail;
    }
  }
  return 0;

fail:
  /* to trigger any threads we already started to stop - xevent thread has already been started */
  rtps_term_prep ();
  wait_for_receive_threads ();
  for (i = 0; i < gv.n_recv_threads; i++)
  {
    if (gv.recv_threads[i].arg.mode == RTM_MANY && gv.recv_threads[i].arg.u.many.ws)
      os_sockWaitsetFree (gv.recv_threads[i].arg.u.many.ws);
    if (gv.recv_threads[i].arg.rbpool)
      nn_rbufpool_free (gv.recv_threads[i].arg.rbpool);
  }
  return -1;
}

int rtps_init (void)
{
  uint32_t port_disc_uc = 0;
  uint32_t port_data_uc = 0;
  bool mc_available = true;

  ddsi_plugin_init ();
  ddsi_iid_init ();

  gv.tstart = now ();    /* wall clock time, used in logs */

  gv.disc_conn_uc = NULL;
  gv.data_conn_uc = NULL;
  gv.disc_conn_mc = NULL;
  gv.data_conn_mc = NULL;
  gv.tev_conn = NULL;
  gv.listener = NULL;
  gv.thread_pool = NULL;
  gv.debmon = NULL;

  /* Print start time for referencing relative times in the remainder
   of the DDS_LOG. */
  {
    int sec = (int) (gv.tstart.v / 1000000000);
    int usec = (int) (gv.tstart.v % 1000000000) / 1000;
    os_time tv;
    char str[OS_CTIME_R_BUFSIZE];
    tv.tv_sec = sec;
    tv.tv_nsec = usec * 1000;
    os_ctime_r (&tv, str, sizeof(str));
    DDS_LOG(DDS_LC_CONFIG, "started at %d.06%d -- %s\n", sec, usec, str);
  }

  /* Initialize thread pool */

  if (config.tp_enable)
  {
    gv.thread_pool = ut_thread_pool_new
      (config.tp_threads, config.tp_max_threads, 0, NULL);
  }

  /* Initialize UDP or TCP transport and resolve factory */
  switch (config.transport_selector)
  {
    case TRANS_DEFAULT:
      assert(0);
    case TRANS_UDP:
    case TRANS_UDP6:
      config.publish_uc_locators = 1;
      config.enable_uc_locators = 1;
      if (ddsi_udp_init () < 0)
        goto err_udp_tcp_init;
      gv.m_factory = ddsi_factory_find (config.transport_selector == TRANS_UDP ? "udp" : "udp6");
      break;
    case TRANS_TCP:
    case TRANS_TCP6:
      config.publish_uc_locators = (config.tcp_port != -1);
      config.enable_uc_locators = 1;
      /* TCP affects what features are supported/required */
      config.suppress_spdp_multicast = 1;
      config.many_sockets_mode = MSM_SINGLE_UNICAST;
      config.allowMulticast = AMC_FALSE;
      if (ddsi_tcp_init () < 0)
        goto err_udp_tcp_init;
      gv.m_factory = ddsi_factory_find (config.transport_selector == TRANS_TCP ? "tcp" : "tcp6");
      break;
    case TRANS_RAWETH:
      config.publish_uc_locators = 1;
      config.enable_uc_locators = 0;
      config.participantIndex = PARTICIPANT_INDEX_NONE;
      config.many_sockets_mode = MSM_NO_UNICAST;
      if (ddsi_raweth_init () < 0)
        goto err_udp_tcp_init;
      gv.m_factory = ddsi_factory_find ("raweth");
      break;
  }

  if (!find_own_ip (config.networkAddressString))
  {
    /* find_own_ip already logs a more informative error message */
    DDS_LOG(DDS_LC_CONFIG, "No network interface selected\n");
    goto err_find_own_ip;
  }
  if (config.allowMulticast)
  {
    if (!gv.interfaces[gv.selected_interface].mc_capable)
    {
      DDS_WARNING("selected interface is not multicast-capable: disabling multicast\n");
      config.suppress_spdp_multicast = 1;
      config.allowMulticast = AMC_FALSE;
      /* ensure discovery can work: firstly, that the process will be reachable on a "well-known" port
         number, and secondly, that the local interface's IP address gets added to the discovery
         address set */
      config.participantIndex = PARTICIPANT_INDEX_AUTO;
      mc_available = false;
    }
  }
  if (set_recvips () < 0)
    goto err_set_recvips;
  if (set_spdp_address () < 0)
    goto err_set_ext_address;
  if (set_default_mc_address () < 0)
    goto err_set_ext_address;
  if (set_ext_address_and_mask () < 0)
    goto err_set_ext_address;

  {
    char buf[DDSI_LOCSTRLEN];
    /* the "ownip", "extip" labels in the trace have been there for so long, that it seems worthwhile to retain them even though they need not be IP any longer */
    DDS_LOG(DDS_LC_CONFIG, "ownip: %s\n", ddsi_locator_to_string_no_port (buf, sizeof(buf), &gv.ownloc));
    DDS_LOG(DDS_LC_CONFIG, "extip: %s\n", ddsi_locator_to_string_no_port (buf, sizeof(buf), &gv.extloc));
    DDS_LOG(DDS_LC_CONFIG, "extmask: %s%s\n", ddsi_locator_to_string_no_port (buf, sizeof(buf), &gv.extmask), gv.m_factory->m_kind != NN_LOCATOR_KIND_UDPv4 ? " (not applicable)" : "");
    DDS_LOG(DDS_LC_CONFIG, "networkid: 0x%lx\n", (unsigned long) gv.myNetworkId);
    DDS_LOG(DDS_LC_CONFIG, "SPDP MC: %s\n", ddsi_locator_to_string_no_port (buf, sizeof(buf), &gv.loc_spdp_mc));
    DDS_LOG(DDS_LC_CONFIG, "default MC: %s\n", ddsi_locator_to_string_no_port (buf, sizeof(buf), &gv.loc_default_mc));
#ifdef DDSI_INCLUDE_SSM
    DDS_LOG(DDS_LC_CONFIG, "SSM support included\n");
#endif
  }

  if (gv.ownloc.kind != gv.extloc.kind)
    DDS_FATAL("mismatch between network address kinds\n");

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  /* Convert address sets in partition mappings from string to address sets */
  {
    const int port = config.port_base + config.port_dg * config.domainId.value + config.port_d2;
    struct config_networkpartition_listelem *np;
    for (np = config.networkPartitions; np; np = np->next)
    {
      static const char msgtag_fixed[] = ": partition address";
      size_t slen = strlen (np->name) + sizeof (msgtag_fixed);
      char * msgtag = os_malloc (slen);
      int rc;
      snprintf (msgtag, slen, "%s%s", np->name, msgtag_fixed);
      np->as = new_addrset ();
      rc = add_addresses_to_addrset (np->as, np->address_string, port, msgtag, 1);
      os_free (msgtag);
      if (rc < 0)
        goto err_network_partition_addrset;
    }
  }
#endif

  gv.startup_mode = (config.startup_mode_duration > 0) ? 1 : 0;
  DDS_LOG(DDS_LC_CONFIG, "startup-mode: %s\n", gv.startup_mode ? "enabled" : "disabled");

  (ddsi_plugin.init_fn) ();

  gv.xmsgpool = nn_xmsgpool_new ();
  gv.serpool = ddsi_serdatapool_new ();

#ifdef DDSI_INCLUDE_ENCRYPTION
  if (q_security_plugin.new_decoder)
  {
    gv.recvSecurityCodec = (q_security_plugin.new_decoder) ();
    DDS_LOG(DDS_LC_CONFIG, "decoderset created\n");
  }
#endif

  nn_plist_init_default_participant (&gv.default_plist_pp);
  nn_xqos_init_default_reader (&gv.default_xqos_rd);
  nn_xqos_init_default_writer (&gv.default_xqos_wr);
  nn_xqos_init_default_writer_noautodispose (&gv.default_xqos_wr_nad);
  nn_xqos_init_default_topic (&gv.default_xqos_tp);
  nn_xqos_init_default_subscriber (&gv.default_xqos_sub);
  nn_xqos_init_default_publisher (&gv.default_xqos_pub);
  nn_xqos_copy (&gv.spdp_endpoint_xqos, &gv.default_xqos_rd);
  gv.spdp_endpoint_xqos.durability.kind = NN_TRANSIENT_LOCAL_DURABILITY_QOS;
  make_builtin_endpoint_xqos (&gv.builtin_endpoint_xqos_rd, &gv.default_xqos_rd);
  make_builtin_endpoint_xqos (&gv.builtin_endpoint_xqos_wr, &gv.default_xqos_wr);

  make_special_topics ();

  os_mutexInit (&gv.participant_set_lock);
  os_condInit (&gv.participant_set_cond, &gv.participant_set_lock);
  lease_management_init ();
  deleted_participants_admin_init ();
  gv.guid_hash = ephash_new ();

  os_mutexInit (&gv.privileged_pp_lock);
  gv.privileged_pp = NULL;

  /* Template PP guid -- protected by privileged_pp_lock for simplicity */
  gv.next_ppguid.prefix.u[0] = locator_to_hopefully_unique_uint32 (&gv.ownloc);
  gv.next_ppguid.prefix.u[1] = (unsigned) os_getpid ();
  gv.next_ppguid.prefix.u[2] = 1;
  gv.next_ppguid.entityid.u = NN_ENTITYID_PARTICIPANT;

  os_mutexInit (&gv.lock);
  os_mutexInit (&gv.spdp_lock);
  gv.spdp_defrag = nn_defrag_new (NN_DEFRAG_DROP_OLDEST, config.defrag_unreliable_maxsamples);
  gv.spdp_reorder = nn_reorder_new (NN_REORDER_MODE_ALWAYS_DELIVER, config.primary_reorder_maxsamples);

  gv.m_tkmap = ddsi_tkmap_new ();

  if (gv.m_factory->m_connless)
  {
    if (config.participantIndex >= 0 || config.participantIndex == PARTICIPANT_INDEX_NONE)
    {
      if (make_uc_sockets (&port_disc_uc, &port_data_uc, config.participantIndex) < 0)
      {
        DDS_ERROR("rtps_init: failed to create unicast sockets for domain %d participant %d\n", config.domainId.value, config.participantIndex);
        goto err_unicast_sockets;
      }
    }
    else if (config.participantIndex == PARTICIPANT_INDEX_AUTO)
    {
      /* try to find a free one, and update config.participantIndex */
      int ppid;
      DDS_LOG(DDS_LC_CONFIG, "rtps_init: trying to find a free participant index\n");
      for (ppid = 0; ppid <= config.maxAutoParticipantIndex; ppid++)
      {
        int r = make_uc_sockets (&port_disc_uc, &port_data_uc, ppid);
        if (r == 0) /* Success! */
          break;
        else if (r == -1) /* Try next one */
          continue;
        else /* Oops! */
        {
          DDS_ERROR("rtps_init: failed to create unicast sockets for domain %d participant %d\n", config.domainId.value, ppid);
          goto err_unicast_sockets;
        }
      }
      if (ppid > config.maxAutoParticipantIndex)
      {
        DDS_ERROR("rtps_init: failed to find a free participant index for domain %d\n", config.domainId.value);
        goto err_unicast_sockets;
      }
      config.participantIndex = ppid;
    }
    else
    {
      assert(0);
    }
    DDS_LOG(DDS_LC_CONFIG, "rtps_init: uc ports: disc %u data %u\n", port_disc_uc, port_data_uc);
  }
  DDS_LOG(DDS_LC_CONFIG, "rtps_init: domainid %d participantid %d\n", config.domainId.value, config.participantIndex);

  if (config.pcap_file && *config.pcap_file)
  {
    gv.pcap_fp = new_pcap_file (config.pcap_file);
    if (gv.pcap_fp)
    {
      os_mutexInit (&gv.pcap_lock);
    }
  }
  else
  {
    gv.pcap_fp = NULL;
  }

  gv.mship = new_group_membership();

  if (gv.m_factory->m_connless)
  {
    if (!(config.many_sockets_mode == MSM_NO_UNICAST && config.allowMulticast))
      DDS_TRACE("Unicast Ports: discovery %d data %d\n", ddsi_conn_port (gv.disc_conn_uc), ddsi_conn_port (gv.data_conn_uc));

    if (config.allowMulticast)
    {
      if (!create_multicast_sockets())
        goto err_mc_conn;

      if (config.many_sockets_mode == MSM_NO_UNICAST)
      {
        gv.data_conn_uc = gv.data_conn_mc;
        gv.disc_conn_uc = gv.disc_conn_mc;
      }

      /* Set multicast locators */
      if (!is_unspec_locator(&gv.loc_spdp_mc))
        gv.loc_spdp_mc.port = ddsi_conn_port (gv.disc_conn_mc);
      if (!is_unspec_locator(&gv.loc_meta_mc))
        gv.loc_meta_mc.port = ddsi_conn_port (gv.disc_conn_mc);
      if (!is_unspec_locator(&gv.loc_default_mc))
        gv.loc_default_mc.port = ddsi_conn_port (gv.data_conn_mc);

      if (joinleave_spdp_defmcip (1) < 0)
        goto err_mc_conn;
    }
  }
  else
  {
    /* Must have a data_conn_uc/tev_conn/transmit_conn */
    gv.data_conn_uc = ddsi_factory_create_conn (gv.m_factory, 0, NULL);

    if (config.tcp_port != -1)
    {
      gv.listener = ddsi_factory_create_listener (gv.m_factory, config.tcp_port, NULL);
      if (gv.listener == NULL || ddsi_listener_listen (gv.listener) != 0)
      {
        DDS_ERROR("Failed to create %s listener\n", gv.m_factory->m_typename);
        if (gv.listener)
          ddsi_listener_free(gv.listener);
        goto err_mc_conn;
      }

      /* Set unicast locators from listener */
      set_unspec_locator (&gv.loc_spdp_mc);
      set_unspec_locator (&gv.loc_meta_mc);
      set_unspec_locator (&gv.loc_default_mc);

      ddsi_listener_locator (gv.listener, &gv.loc_meta_uc);
      ddsi_listener_locator (gv.listener, &gv.loc_default_uc);
    }
  }

  /* Create shared transmit connection */

  gv.tev_conn = gv.data_conn_uc;
  DDS_TRACE("Timed event transmit port: %d\n", (int) ddsi_conn_port (gv.tev_conn));

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  {
    struct config_channel_listelem *chptr = config.channels;
    while (chptr)
    {
      size_t slen = strlen (chptr->name) + 5;
      char * tname = os_malloc (slen);
      (void) snprintf (tname, slen, "tev.%s", chptr->name);

      /* Only actually create new connection if diffserv set */

      if (chptr->diffserv_field)
      {
        ddsi_tran_qos_t qos = ddsi_tran_create_qos ();
        qos->m_diffserv = chptr->diffserv_field;
        chptr->transmit_conn = ddsi_factory_create_conn (gv.m_factory, 0, qos);
        ddsi_tran_free_qos (qos);
        if (chptr->transmit_conn == NULL)
        {
          DDS_FATAL("failed to create transmit connection for channel %s\n", chptr->name);
        }
      }
      else
      {
        chptr->transmit_conn = gv.data_conn_uc;
      }
      DDS_TRACE("channel %s: transmit port %d\n", chptr->name, (int) ddsi_tran_port (chptr->transmit_conn));

#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
      if (chptr->auxiliary_bandwidth_limit > 0 || lookup_thread_properties (tname))
      {
        chptr->evq = xeventq_new
        (
          chptr->transmit_conn,
          config.max_queued_rexmit_bytes,
          config.max_queued_rexmit_msgs,
          chptr->auxiliary_bandwidth_limit
        );
      }
#else
      if (lookup_thread_properties (tname))
      {
        chptr->evq = xeventq_new
        (
          chptr->transmit_conn,
          config.max_queued_rexmit_bytes,
          config.max_queued_rexmit_msgs,
          0
        );
      }
#endif
      os_free (tname);
      chptr = chptr->next;
    }
  }
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */

  /* Create event queues */

  gv.xevents = xeventq_new
  (
    gv.tev_conn,
    config.max_queued_rexmit_bytes,
    config.max_queued_rexmit_msgs,
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
    config.auxiliary_bandwidth_limit
#else
    0
#endif
  );

  gv.as_disc = new_addrset ();
  add_to_addrset (gv.as_disc, &gv.loc_spdp_mc);
  /* If multicast was enabled but not available, always add the local interface to the discovery address set.
     Conversion via string and add_peer_addresses has the benefit that the port number expansion happens
     automatically. */
  if (!mc_available)
  {
    struct config_peer_listelem peer_local;
    char local_addr[DDSI_LOCSTRLEN];
    ddsi_locator_to_string_no_port (local_addr, sizeof (local_addr), &gv.interfaces[gv.selected_interface].loc);
    peer_local.next = NULL;
    peer_local.peer = local_addr;
    add_peer_addresses (gv.as_disc, &peer_local);
  }
  if (config.peers)
  {
    add_peer_addresses (gv.as_disc, config.peers);
  }
  if (config.peers_group)
  {
    gv.as_disc_group = new_addrset ();
    add_peer_addresses (gv.as_disc_group, config.peers_group);
  }
  else
  {
    gv.as_disc_group = NULL;
  }

  gv.gcreq_queue = gcreq_queue_new ();

  gv.rtps_keepgoing = 1;
  os_rwlockInit (&gv.qoslock);

  {
    int r;
    gv.builtins_dqueue = nn_dqueue_new ("builtins", config.delivery_queue_maxsamples, builtins_dqueue_handler, NULL);
    if ((r = xeventq_start (gv.xevents, NULL)) < 0)
    {
      DDS_FATAL("failed to start global event processing thread (%d)\n", r);
    }
  }

  if (config.xpack_send_async)
  {
    nn_xpack_sendq_init();
    nn_xpack_sendq_start();
  }

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  /* Create a delivery queue and start tev for each channel */
  {
    struct config_channel_listelem * chptr = config.channels;
    while (chptr)
    {
      chptr->dqueue = nn_dqueue_new (chptr->name, config.delivery_queue_maxsamples, user_dqueue_handler, NULL);
      if (chptr->evq)
      {
        int r;
        if ((r = xeventq_start (chptr->evq, chptr->name)) < 0)
          DDS_FATAL("failed to start event processing thread for channel '%s' (%d)\n", chptr->name, r);
      }
      chptr = chptr->next;
    }
  }
#else
  gv.user_dqueue = nn_dqueue_new ("user", config.delivery_queue_maxsamples, user_dqueue_handler, NULL);
#endif

  if (setup_and_start_recv_threads () < 0)
  {
    DDS_FATAL("failed to start receive threads\n");
  }

  if (gv.listener)
  {
    gv.listen_ts = create_thread ("listen", (uint32_t (*) (void *)) listen_thread, gv.listener);
  }

  if (gv.startup_mode)
  {
    qxev_end_startup_mode (add_duration_to_mtime (now_mt (), config.startup_mode_duration));
  }

  if (config.monitor_port >= 0)
  {
    gv.debmon = new_debug_monitor (config.monitor_port);
  }

  return 0;

err_mc_conn:
  if (gv.disc_conn_mc)
    ddsi_conn_free (gv.disc_conn_mc);
  if (gv.data_conn_mc && gv.data_conn_mc != gv.disc_conn_mc)
    ddsi_conn_free (gv.data_conn_mc);
  if (gv.pcap_fp)
    os_mutexDestroy (&gv.pcap_lock);
  if (gv.disc_conn_uc != gv.disc_conn_mc)
    ddsi_conn_free (gv.disc_conn_uc);
  if (gv.data_conn_uc != gv.disc_conn_uc)
    ddsi_conn_free (gv.data_conn_uc);
  free_group_membership(gv.mship);
err_unicast_sockets:
  ddsi_tkmap_free (gv.m_tkmap);
  nn_reorder_free (gv.spdp_reorder);
  nn_defrag_free (gv.spdp_defrag);
  os_mutexDestroy (&gv.spdp_lock);
  os_mutexDestroy (&gv.lock);
  os_mutexDestroy (&gv.privileged_pp_lock);
  ephash_free (gv.guid_hash);
  gv.guid_hash = NULL;
  deleted_participants_admin_fini ();
  lease_management_term ();
  os_condDestroy (&gv.participant_set_cond);
  os_mutexDestroy (&gv.participant_set_lock);
  free_special_topics ();
#ifdef DDSI_INCLUDE_ENCRYPTION
  if (q_security_plugin.free_decoder)
    q_security_plugin.free_decoder (gv.recvSecurityCodec);
#endif
  nn_xqos_fini (&gv.builtin_endpoint_xqos_wr);
  nn_xqos_fini (&gv.builtin_endpoint_xqos_rd);
  nn_xqos_fini (&gv.spdp_endpoint_xqos);
  nn_xqos_fini (&gv.default_xqos_pub);
  nn_xqos_fini (&gv.default_xqos_sub);
  nn_xqos_fini (&gv.default_xqos_tp);
  nn_xqos_fini (&gv.default_xqos_wr_nad);
  nn_xqos_fini (&gv.default_xqos_wr);
  nn_xqos_fini (&gv.default_xqos_rd);
  nn_plist_fini (&gv.default_plist_pp);
  ddsi_serdatapool_free (gv.serpool);
  nn_xmsgpool_free (gv.xmsgpool);
  ddsi_iid_fini ();
  (ddsi_plugin.fini_fn) ();
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
err_network_partition_addrset:
  for (struct config_networkpartition_listelem *np = config.networkPartitions; np; np = np->next)
    unref_addrset (np->as);
#endif
err_set_ext_address:
  while (gv.recvips)
  {
    struct config_in_addr_node *n = gv.recvips;
    gv.recvips = n->next;
    os_free (n);
  }
err_set_recvips:
  {
    int i;
    for (i = 0; i < gv.n_interfaces; i++)
      os_free (gv.interfaces[i].name);
  }
err_find_own_ip:
    ddsi_tran_factories_fini ();
err_udp_tcp_init:
  if (config.tp_enable)
    ut_thread_pool_free (gv.thread_pool);
  return -1;
}

struct dq_builtins_ready_arg {
  os_mutex lock;
  os_cond cond;
  int ready;
};

static void builtins_dqueue_ready_cb (void *varg)
{
  struct dq_builtins_ready_arg *arg = varg;
  os_mutexLock (&arg->lock);
  arg->ready = 1;
  os_condBroadcast (&arg->cond);
  os_mutexUnlock (&arg->lock);
}

void rtps_stop (void)
{
  struct thread_state1 *self = lookup_thread_state ();
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  struct config_channel_listelem * chptr;
#endif

  if (gv.debmon)
  {
    free_debug_monitor (gv.debmon);
    gv.debmon = NULL;
  }

  /* Stop all I/O */
  rtps_term_prep ();
  wait_for_receive_threads ();

  if (gv.listener)
  {
    ddsi_listener_unblock(gv.listener);
    join_thread (gv.listen_ts);
    ddsi_listener_free(gv.listener);
  }

  xeventq_stop (gv.xevents);
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  for (chptr = config.channels; chptr; chptr = chptr->next)
  {
    if (chptr->evq)
      xeventq_stop (chptr->evq);
  }
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */

  /* Send a bubble through the delivery queue for built-ins, so that any
     pending proxy participant discovery is finished before we start
     deleting them */
  {
    struct dq_builtins_ready_arg arg;
    os_mutexInit (&arg.lock);
    os_condInit (&arg.cond, &arg.lock);
    arg.ready = 0;
    nn_dqueue_enqueue_callback(gv.builtins_dqueue, builtins_dqueue_ready_cb, &arg);
    os_mutexLock (&arg.lock);
    while (!arg.ready)
      os_condWait (&arg.cond, &arg.lock);
    os_mutexUnlock (&arg.lock);
    os_condDestroy (&arg.cond);
    os_mutexDestroy (&arg.lock);
  }

  /* Once the receive threads have stopped, defragmentation and
     reorder state can't change anymore, and can be freed safely. */
  nn_reorder_free (gv.spdp_reorder);
  nn_defrag_free (gv.spdp_defrag);
  os_mutexDestroy (&gv.spdp_lock);
#ifdef DDSI_INCLUDE_ENCRYPTION
  if (q_security_plugin.free_decoder)
  {
    (q_security_plugin.free_decoder) (gv.recvSecurityCodec);
  }
#endif /* DDSI_INCLUDE_ENCRYPTION */

  {
    struct ephash_enum_proxy_participant est;
    struct proxy_participant *proxypp;
    const nn_wctime_t tnow = now();
    /* Clean up proxy readers, proxy writers and proxy
       participants. Deleting a proxy participants deletes all its
       readers and writers automatically */
    thread_state_awake (self);
    ephash_enum_proxy_participant_init (&est);
    while ((proxypp = ephash_enum_proxy_participant_next (&est)) != NULL)
    {
      delete_proxy_participant_by_guid(&proxypp->e.guid, tnow, 1);
    }
    ephash_enum_proxy_participant_fini (&est);
    thread_state_asleep (self);
  }

  {
    struct ephash_enum_writer est_wr;
    struct ephash_enum_reader est_rd;
    struct ephash_enum_participant est_pp;
    struct participant *pp;
    struct writer *wr;
    struct reader *rd;
    /* Delete readers, writers and participants, relying on
       delete_participant to schedule the deletion of the built-in
       rwriters to get all SEDP and SPDP dispose+unregister messages
       out. FIXME: need to keep xevent thread alive for a while
       longer. */
    thread_state_awake (self);
    ephash_enum_writer_init (&est_wr);
    while ((wr = ephash_enum_writer_next (&est_wr)) != NULL)
    {
      if (!is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE))
        delete_writer_nolinger (&wr->e.guid);
    }
    ephash_enum_writer_fini (&est_wr);
    thread_state_awake (self);
    ephash_enum_reader_init (&est_rd);
    while ((rd = ephash_enum_reader_next (&est_rd)) != NULL)
    {
      if (!is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE))
        (void)delete_reader (&rd->e.guid);
    }
    ephash_enum_reader_fini (&est_rd);
    thread_state_awake (self);
    ephash_enum_participant_init (&est_pp);
    while ((pp = ephash_enum_participant_next (&est_pp)) != NULL)
    {
      delete_participant (&pp->e.guid);
    }
    ephash_enum_participant_fini (&est_pp);
    thread_state_asleep (self);
  }

  /* Wait until all participants are really gone => by then we can be
     certain that no new GC requests will be added, short of what we
     do here */
  os_mutexLock (&gv.participant_set_lock);
  while (gv.nparticipants > 0)
    os_condWait (&gv.participant_set_cond, &gv.participant_set_lock);
  os_mutexUnlock (&gv.participant_set_lock);

  /* Wait until no more GC requests are outstanding -- not really
     necessary, but it allows us to claim the stack is quiescent
     at this point */
  gcreq_queue_drain (gv.gcreq_queue);

  /* Clean up privileged_pp -- it must be NULL now (all participants
     are gone), but the lock still needs to be destroyed */
  assert (gv.privileged_pp == NULL);
  os_mutexDestroy (&gv.privileged_pp_lock);
}

void rtps_fini (void)
{
  /* Shut down the GC system -- no new requests will be added */
  gcreq_queue_free (gv.gcreq_queue);

  /* No new data gets added to any admin, all synchronous processing
     has ended, so now we can drain the delivery queues to end up with
     the expected reference counts all over the radmin thingummies. */
  nn_dqueue_free (gv.builtins_dqueue);

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  chptr = config.channels;
  while (chptr)
  {
    nn_dqueue_free (chptr->dqueue);
    chptr = chptr->next;
  }
#else
  nn_dqueue_free (gv.user_dqueue);
#endif

  xeventq_free (gv.xevents);

  if (config.xpack_send_async)
  {
    nn_xpack_sendq_stop();
    nn_xpack_sendq_fini();
  }

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  chptr = config.channels;
  while (chptr)
  {
    if (chptr->evq)
    {
      xeventq_free (chptr->evq);
    }
    if (chptr->transmit_conn != gv.data_conn_uc)
    {
      ddsi_conn_free (chptr->transmit_conn);
    }
    chptr = chptr->next;
  }
#endif

  ut_thread_pool_free (gv.thread_pool);

  (void) joinleave_spdp_defmcip (0);

  ddsi_conn_free (gv.disc_conn_mc);
  if (gv.data_conn_mc != gv.disc_conn_mc)
    ddsi_conn_free (gv.data_conn_mc);
  if (gv.disc_conn_uc != gv.disc_conn_mc)
    ddsi_conn_free (gv.disc_conn_uc);
  if (gv.data_conn_uc != gv.disc_conn_uc)
    ddsi_conn_free (gv.data_conn_uc);

  /* Not freeing gv.tev_conn: it aliases data_conn_uc */

  free_group_membership(gv.mship);
  ddsi_tran_factories_fini ();

  if (gv.pcap_fp)
  {
    os_mutexDestroy (&gv.pcap_lock);
    fclose (gv.pcap_fp);
  }

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  for (struct config_networkpartition_listelem *np = config.networkPartitions; np; np = np->next)
    unref_addrset (np->as);
#endif
  unref_addrset (gv.as_disc);
  unref_addrset (gv.as_disc_group);

  /* Must delay freeing of rbufpools until after *all* references have
     been dropped, which only happens once all receive threads have
     stopped, defrags and reorders have been freed, and all delivery
     queues been drained.  I.e., until very late in the game. */
  {
    unsigned i;
    for (i = 0; i < gv.n_recv_threads; i++)
    {
      if (gv.recv_threads[i].arg.mode == RTM_MANY)
        os_sockWaitsetFree (gv.recv_threads[i].arg.u.many.ws);
      nn_rbufpool_free (gv.recv_threads[i].arg.rbpool);
    }
  }

  ddsi_tkmap_free (gv.m_tkmap);

  ephash_free (gv.guid_hash);
  gv.guid_hash = NULL;
  deleted_participants_admin_fini ();
  lease_management_term ();
  os_mutexDestroy (&gv.participant_set_lock);
  os_condDestroy (&gv.participant_set_cond);
  free_special_topics ();

  nn_xqos_fini (&gv.builtin_endpoint_xqos_wr);
  nn_xqos_fini (&gv.builtin_endpoint_xqos_rd);
  nn_xqos_fini (&gv.spdp_endpoint_xqos);
  nn_xqos_fini (&gv.default_xqos_pub);
  nn_xqos_fini (&gv.default_xqos_sub);
  nn_xqos_fini (&gv.default_xqos_tp);
  nn_xqos_fini (&gv.default_xqos_wr_nad);
  nn_xqos_fini (&gv.default_xqos_wr);
  nn_xqos_fini (&gv.default_xqos_rd);
  nn_plist_fini (&gv.default_plist_pp);

  os_mutexDestroy (&gv.lock);
  os_rwlockDestroy (&gv.qoslock);

  while (gv.recvips)
  {
    struct config_in_addr_node *n = gv.recvips;
/* The compiler doesn't realize that n->next is always initialized. */
OS_WARNING_MSVC_OFF(6001);
    gv.recvips = n->next;
OS_WARNING_MSVC_ON(6001);
    os_free (n);
  }

  {
    int i;
    for (i = 0; i < (int) gv.n_interfaces; i++)
      os_free (gv.interfaces[i].name);
  }

  ddsi_serdatapool_free (gv.serpool);
  nn_xmsgpool_free (gv.xmsgpool);
  ddsi_iid_fini ();
  (ddsi_plugin.fini_fn) ();
  DDS_LOG(DDS_LC_CONFIG, "Finis.\n");
}
