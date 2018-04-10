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
#include "ddsi/q_align.h"
#include "ddsi/q_addrset.h"
#include "ddsi/q_ddsi_discovery.h"
#include "ddsi/q_radmin.h"
#include "ddsi/q_error.h"
#include "ddsi/q_thread.h"
#include "ddsi/q_ephash.h"
#include "ddsi/q_lease.h"
#include "ddsi/q_gc.h"
#include "ddsi/q_whc.h"
#include "ddsi/q_entity.h"
#include "ddsi/q_nwif.h"
#include "ddsi/q_globals.h"
#include "ddsi/q_xmsg.h"
#include "ddsi/q_receive.h"
#include "ddsi/q_pcap.h"
#include "ddsi/q_feature_check.h"
#include "ddsi/q_debmon.h"

#include "ddsi/sysdeps.h"

#include "ddsi/ddsi_ser.h"
#include "ddsi/ddsi_tran.h"
#include "ddsi/ddsi_udp.h"
#include "ddsi/ddsi_tcp.h"

#include "dds__tkmap.h"

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
  if (ppid >= 0)
  {
    /* FIXME: verify port numbers are in range instead of truncating them like this */
    int base = config.port_base + (config.port_dg * config.domainId) + (ppid * config.port_pg);
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
    NN_FATAL ("make_uc_sockets: invalid participant index %d\n", ppid);
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
        NN_WARNING ("DDSI2EService/General/MulticastRecvNetworkInterfaceAddresses: using 'preferred' instead of 'all' because of IPv6 link-local address\n");
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
        NN_ERROR ("DDSI2EService/General/MulticastRecvNetworkInterfaceAddresses: 'any' is unsupported in combination with an IPv6 link-local address\n");
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
        os_sockaddr_storage parsedaddr;
        if (!os_sockaddrStringToAddress (config.networkRecvAddressStrings[i], (os_sockaddr *) &parsedaddr, !config.useIpv6))
        {
          NN_ERROR ("%s: not a valid IP address\n", config.networkRecvAddressStrings[i]);
          return -1;
        }
        if (os_sockaddrIPAddressEqual ((os_sockaddr *) &gv.interfaces[gv.selected_interface].addr, (os_sockaddr *) &parsedaddr))
          have_selected = 1;
        else
          have_others = 1;
      }
      gv.recvips_mode = have_selected ? RECVIPS_MODE_PREFERRED : RECVIPS_MODE_NONE;
      if (have_others)
      {
        NN_WARNING ("DDSI2EService/General/MulticastRecvNetworkInterfaceAddresses: using 'preferred' because of IPv6 local address\n");
      }
    }
#endif
    else
    {
      struct ospl_in_addr_node **recvnode = &gv.recvips;
      int i, j;
      gv.recvips_mode = RECVIPS_MODE_SOME;
      for (i = 0; config.networkRecvAddressStrings[i] != NULL; i++)
      {
        os_sockaddr_storage parsedaddr;
        if (!os_sockaddrStringToAddress (config.networkRecvAddressStrings[i], (os_sockaddr *) &parsedaddr, !config.useIpv6))
        {
          NN_ERROR ("%s: not a valid IP address\n", config.networkRecvAddressStrings[i]);
          return -1;
        }
        for (j = 0; j < gv.n_interfaces; j++)
        {
          if (os_sockaddrIPAddressEqual ((os_sockaddr *) &gv.interfaces[j].addr, (os_sockaddr *) &parsedaddr))
            break;
        }
        if (j == gv.n_interfaces)
        {
          NN_ERROR ("No interface bound to requested address '%s'\n",
                     config.networkRecvAddressStrings[i]);
          return -1;
        }
        *recvnode = os_malloc (sizeof (struct ospl_in_addr_node));
        (*recvnode)->addr = parsedaddr;
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
  os_sockaddr_storage addr;
  if (strspn (string, " \t") == strlen (string))
  {
    /* string consisting of just spaces and/or tabs (that includes the empty string) is ignored */
    return 0;
  }
  else if (!os_sockaddrStringToAddress (string, (os_sockaddr *) &addr, !config.useIpv6))
  {
    NN_ERROR ("%s: not a valid IP address (%s)\n", string, tag);
    return -1;
  }
  else if (!config.useIpv6 && addr.ss_family != AF_INET)
  {
    NN_ERROR ("%s: not a valid IPv4 address (%s)\n", string, tag);
    return -1;
  }
#if OS_SOCKET_HAS_IPV6
  else if (config.useIpv6 && addr.ss_family != AF_INET6)
  {
    NN_ERROR ("%s: not a valid IPv6 address (%s)\n", string, tag);
    return -1;
  }
#endif
  else
  {
    nn_address_to_loc (loc, &addr, config.useIpv6 ? NN_LOCATOR_KIND_UDPv6 : NN_LOCATOR_KIND_UDPv4);
    if (port != 0 && !is_unspec_locator(loc))
      loc->port = port;
    assert (mc == -1 || mc == 0 || mc == 1);
    if (mc >= 0)
    {
      const char *unspecstr = config.useIpv6 ? "the IPv6 unspecified address (::0)" : "IPv4 ANY (0.0.0.0)";
      const char *rel = mc ? "must" : "may not";
      const int ismc = is_unspec_locator (loc) || is_mcaddr (loc);
      if (mc != ismc)
      {
        NN_ERROR ("%s: %s %s be %s or a multicast address\n", string, tag, rel, unspecstr);
        return -1;
      }
    }

    return 1;
  }
}

static int set_spdp_address (void)
{
  const uint32_t port = (uint32_t) (config.port_base + config.port_dg * config.domainId + config.port_d0);
  int rc = 0;
  if (strcmp (config.spdpMulticastAddressString, "239.255.0.1") != 0)
  {
    if ((rc = string_to_default_locator (&gv.loc_spdp_mc, config.spdpMulticastAddressString, port, 1, "SPDP address")) < 0)
      return rc;
  }
  if (rc == 0)
  {
    /* There isn't a standard IPv6 multicast group for DDSI. For
       some reason, node-local multicast addresses seem to be
       unsupported (ff01::... would be a node-local one I think), so
       instead do link-local. I suppose we could use the hop limit
       to make it node-local.  If other hosts reach us in some way,
       we'll of course respond. */
    const char *def = config.useIpv6 ? "ff02::ffff:239.255.0.1" : "239.255.0.1";
    rc = string_to_default_locator (&gv.loc_spdp_mc, def, port, 1, "SPDP address");
    assert (rc > 0);
  }
#ifdef DDSI_INCLUDE_SSM
  if (is_ssm_mcaddr (&gv.loc_spdp_mc))
  {
    NN_ERROR ("%s: SPDP address may not be an SSM address\n", config.spdpMulticastAddressString);
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
  const uint32_t port = (uint32_t) (config.port_base + config.port_dg * config.domainId + config.port_d2);
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
    gv.extip = gv.ownip;
  else if ((rc = string_to_default_locator (&loc, config.externalAddressString, 0, 0, "external address")) < 0)
    return rc;
  else if (rc == 0) {
    NN_WARNING ("Ignoring ExternalNetworkAddress %s\n", config.externalAddressString);
    gv.extip = gv.ownip;
  } else {
    nn_loc_to_address (&gv.extip, &loc);
  }

  if (!config.externalMaskString || strcmp (config.externalMaskString, "0.0.0.0") == 0)
    gv.extmask.s_addr = 0;
  else if (config.useIpv6)
  {
    NN_ERROR ("external network masks only supported in IPv4 mode\n");
    return -1;
  }
  else
  {
    os_sockaddr_storage addr;
    if ((rc = string_to_default_locator (&loc, config.externalMaskString, 0, -1, "external mask")) < 0)
      return rc;
    nn_loc_to_address(&addr, &loc);
    assert (addr.ss_family == AF_INET);
    gv.extmask = ((const os_sockaddr_in *) &addr)->sin_addr;
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
        NN_ERROR ("config: DDSI2Service/Threads/Thread[@name=\"%s\"]: unknown thread\n", e->name);
        ok = 0;
      }
#else
      NN_ERROR ("config: DDSI2Service/Threads/Thread[@name=\"%s\"]: unknown thread\n", e->name);
      ok = 0;
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */
    }
  }
  return ok;
}

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
        NN_ERROR ("%s: cannot open for writing\n", config.tracingOutputFileName);
        status = 0;
    }
    else
    {
        status = 1;
    }

    return status;
}

int rtps_config_prep (struct cfgst *cfgst)
{
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  unsigned num_channels = 0;
  unsigned num_channel_threads = 0;
#endif

  /* if the discovery domain id was explicitly set, override the default here */
  if (!config.discoveryDomainId.isdefault)
  {
    config.domainId = config.discoveryDomainId.value;
  }

  /* retry_on_reject_duration default is dependent on late_ack_mode and responsiveness timeout, so fix up */


  if (config.whc_init_highwater_mark.isdefault)
    config.whc_init_highwater_mark.value = config.whc_lowwater_mark;
  if (config.whc_highwater_mark < config.whc_lowwater_mark ||
      config.whc_init_highwater_mark.value < config.whc_lowwater_mark ||
      config.whc_init_highwater_mark.value > config.whc_highwater_mark)
  {
    NN_ERROR ("Invalid watermark settings\n");
    goto err_config_late_error;
  }

  if (config.besmode == BESMODE_MINIMAL && config.many_sockets_mode)
  {
    /* These two are incompatible because minimal bes mode can result
       in implicitly creating proxy participants inheriting the
       address set from the ddsi2 participant (which is then typically
       inherited by readers/writers), but in many sockets mode each
       participant has its own socket, and therefore unique address
       set */
    NN_ERROR ("Minimal built-in endpoint set mode and ManySocketsMode are incompatible\n");
    goto err_config_late_error;
  }

  /* Dependencies between default values is not handled
   automatically by the config processing (yet) */
  if (config.many_sockets_mode)
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
        NN_FATAL ("AuxiliaryBandwidthLimit * NackDelay = %g bytes is insane\n", max);
      if (max > 2147483647.0)
        config.max_queued_rexmit_bytes = 2147483647u;
      else
        config.max_queued_rexmit_bytes = (unsigned) max;
    }
#else
    config.max_queued_rexmit_bytes = 2147483647u;
#endif /* DDSI_INCLUDE_BANDWIDTH_LIMITING */
  }

  /* Verify thread properties refer to defined threads */
  if (!check_thread_properties ())
  {
    NN_ERROR ("Could not initialise configuration\n");
    goto err_config_late_error;
  }

#if ! OS_SOCKET_HAS_IPV6
  /* If the platform doesn't support IPv6, guarantee useIpv6 is
   false. There are two ways of going about it, one is to do it
   silently, the other to let the user fix his config. Clearly, we
   have chosen the latter. */
  if (config.useIpv6)
  {
    NN_ERROR ("IPv6 addressing requested but not supported on this platform\n");
    goto err_config_late_error;
  }
#endif

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

      if (config.useIpv6 && chptr->diffserv_field != 0)
      {
        NN_ERROR ("channel %s specifies IPv4 DiffServ settings which is incompatible with IPv6 use\n",
                   chptr->name);
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
    NN_ERROR ("Could not initialise configuration\n");
    goto err_config_late_error;
  }

  /* Thread admin: need max threads, which is currently (2 or 3) for each
   configured channel plus 7: main, recv, dqueue.builtin,
   lease, gc, debmon; once thread state admin has been inited, upgrade the
   main thread one participating in the thread tracking stuff as
   if it had been created using create_thread(). */

  {
  /* For Lite - Temporary
    Thread states for each application thread is managed using thread_states structure
  */
#define USER_MAX_THREADS 50

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
    const unsigned max_threads = 7 + USER_MAX_THREADS + num_channel_threads + config.ddsi2direct_max_threads;
#else
    const unsigned max_threads = 9 + USER_MAX_THREADS + config.ddsi2direct_max_threads;
#endif
    thread_states_init (max_threads);
  }

  /* Now the per-thread-log-buffers are set up, so print the configuration */
  config_print_cfgst (cfgst);

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  /* Convert address sets in partition mappings from string to address sets */
  {
    const int port = config.port_base + config.port_dg * config.domainId + config.port_d2;
    struct config_networkpartition_listelem *np;
    for (np = config.networkPartitions; np; np = np->next)
    {
      static const char msgtag_fixed[] = ": partition address";
      size_t slen = strlen (np->name) + sizeof (msgtag_fixed);
      char * msgtag = os_malloc (slen);
      int rc;
      (void) snprintf (msgtag, slen, "%s%s", np->name, msgtag_fixed);
      np->as = new_addrset ();
      rc = add_addresses_to_addrset (np->as, np->address_string, port, msgtag, 1);
      os_free (msgtag);
      if (rc < 0)
        goto err_config_late_error;
    }
  }
#endif

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
  if (!is_mcaddr (loc))
    return;
#ifdef DDSI_INCLUDE_SSM
  /* Can't join SSM until we actually have a source */
  if (is_ssm_mcaddr (loc))
    return;
#endif
  if (arg->dojoin) {
    if (ddsi_conn_join_mc (gv.disc_conn_mc, NULL, loc) < 0 || ddsi_conn_join_mc (gv.data_conn_mc, NULL, loc) < 0)
      arg->errcount++;
  } else {
    if (ddsi_conn_leave_mc (gv.disc_conn_mc, NULL, loc) < 0 || ddsi_conn_leave_mc (gv.data_conn_mc, NULL, loc) < 0)
      arg->errcount++;
  }
}

static int joinleave_spdp_defmcip (int dojoin)
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
    NN_ERROR ("rtps_init: failed to join multicast groups for domain %d participant %d\n", config.domainId, config.participantIndex);
    return -1;
  }
  return 0;
}

int rtps_init (void)
{
  uint32_t port_disc_uc = 0;
  uint32_t port_data_uc = 0;

  /* Initialize implementation (Lite or OSPL) */

  ddsi_plugin_init ();

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
   of the nn_log. */
  {
    int sec = (int) (gv.tstart.v / 1000000000);
    int usec = (int) (gv.tstart.v % 1000000000) / 1000;
    os_time tv;
    char str[OS_CTIME_R_BUFSIZE];
    tv.tv_sec = sec;
    tv.tv_nsec = usec * 1000;
    os_ctime_r (&tv, str, sizeof(str));
    nn_log (LC_INFO | LC_CONFIG, "started at %d.06%d -- %s\n", sec, usec, str);
  }

  /* Initialize thread pool */

  if (config.tp_enable)
  {
    gv.thread_pool = ut_thread_pool_new
      (config.tp_threads, config.tp_max_threads, 0, NULL);
  }

  /* Initialize UDP or TCP transport and resolve factory */

  if (!config.tcp_enable)
  {
    config.publish_uc_locators = true;
    if (ddsi_udp_init () < 0)
      goto err_udp_tcp_init;
    gv.m_factory = ddsi_factory_find ("udp");
  }
  else
  {
    config.publish_uc_locators = (config.tcp_port == -1) ? false : true;
    /* TCP affects what features are supported/required */
    config.suppress_spdp_multicast = true;
    config.many_sockets_mode = false;
    config.allowMulticast = AMC_FALSE;

    if (ddsi_tcp_init () < 0)
      goto err_udp_tcp_init;
    gv.m_factory = ddsi_factory_find ("tcp");
  }

  if (!find_own_ip (config.networkAddressString))
  {
    NN_ERROR ("No network interface selected\n");
    goto err_find_own_ip;
  }
  if (config.allowMulticast)
  {
    int i;
    for (i = 0; i < gv.n_interfaces; i++)
    {
      if (gv.interfaces[i].mc_capable)
        break;
    }
    if (i == gv.n_interfaces)
    {
      NN_WARNING ("No multicast capable interfaces: disabling multicast\n");
      config.suppress_spdp_multicast = 1;
      config.allowMulticast = AMC_FALSE;
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
    char buf[INET6_ADDRSTRLEN_EXTENDED];
    nn_log (LC_CONFIG, "ownip: %s\n", sockaddr_to_string_no_port (buf, &gv.ownip));
    nn_log (LC_CONFIG, "extip: %s\n", sockaddr_to_string_no_port (buf, &gv.extip));
    (void)inet_ntop(AF_INET, &gv.extmask, buf, sizeof(buf));
    nn_log (LC_CONFIG, "extmask: %s%s\n", buf, config.useIpv6 ? " (not applicable)" : "");
    nn_log (LC_CONFIG, "networkid: 0x%lx\n", (unsigned long) gv.myNetworkId);
    nn_log (LC_CONFIG, "SPDP MC: %s\n", locator_to_string_no_port (buf, &gv.loc_spdp_mc));
    nn_log (LC_CONFIG, "default MC: %s\n", locator_to_string_no_port (buf, &gv.loc_default_mc));
#ifdef DDSI_INCLUDE_SSM
    nn_log (LC_CONFIG, "SSM support included\n");
#endif
  }

  if (gv.ownip.ss_family != gv.extip.ss_family)
    NN_FATAL ("mismatch between network address kinds\n");

  gv.startup_mode = (config.startup_mode_duration > 0) ? 1 : 0;
  nn_log (LC_CONFIG, "startup-mode: %s\n", gv.startup_mode ? "enabled" : "disabled");

  (ddsi_plugin.init_fn) ();

  gv.xmsgpool = nn_xmsgpool_new ();
  gv.serpool = ddsi_serstatepool_new ();

#ifdef DDSI_INCLUDE_ENCRYPTION
  if (q_security_plugin.new_decoder)
  {
    gv.recvSecurityCodec = (q_security_plugin.new_decoder) ();
    nn_log (LC_CONFIG, "decoderset created\n");
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

  os_mutexInit (&gv.participant_set_lock);
  os_condInit (&gv.participant_set_cond, &gv.participant_set_lock);
  lease_management_init ();
  deleted_participants_admin_init ();
  gv.guid_hash = ephash_new ();

  os_mutexInit (&gv.privileged_pp_lock);
  gv.privileged_pp = NULL;

  /* Template PP guid -- protected by privileged_pp_lock for simplicity */
  gv.next_ppguid.prefix.u[0] = sockaddr_to_hopefully_unique_uint32 (&gv.ownip);
  gv.next_ppguid.prefix.u[1] = (unsigned) os_procIdSelf ();
  gv.next_ppguid.prefix.u[2] = 1;
  gv.next_ppguid.entityid.u = NN_ENTITYID_PARTICIPANT;

  os_mutexInit (&gv.lock);
  os_mutexInit (&gv.spdp_lock);
  gv.spdp_defrag = nn_defrag_new (NN_DEFRAG_DROP_OLDEST, config.defrag_unreliable_maxsamples);
  gv.spdp_reorder = nn_reorder_new (NN_REORDER_MODE_ALWAYS_DELIVER, config.primary_reorder_maxsamples);

  gv.m_tkmap = dds_tkmap_new ();

  if (gv.m_factory->m_connless)
  {
    if (config.participantIndex >= 0 || config.participantIndex == PARTICIPANT_INDEX_NONE)
    {
      if (make_uc_sockets (&port_disc_uc, &port_data_uc, config.participantIndex) < 0)
      {
        NN_ERROR ("rtps_init: failed to create unicast sockets for domain %d participant %d\n", config.domainId, config.participantIndex);
        goto err_unicast_sockets;
      }
    }
    else if (config.participantIndex == PARTICIPANT_INDEX_AUTO)
    {
      /* try to find a free one, and update config.participantIndex */
      int ppid;
      nn_log (LC_CONFIG, "rtps_init: trying to find a free participant index\n");
      for (ppid = 0; ppid <= config.maxAutoParticipantIndex; ppid++)
      {
        int r = make_uc_sockets (&port_disc_uc, &port_data_uc, ppid);
        if (r == 0) /* Success! */
          break;
        else if (r == -1) /* Try next one */
          continue;
        else /* Oops! */
        {
          NN_ERROR ("rtps_init: failed to create unicast sockets for domain %d participant %d\n", config.domainId, ppid);
          goto err_unicast_sockets;
        }
      }
      if (ppid > config.maxAutoParticipantIndex)
      {
        NN_ERROR ("rtps_init: failed to find a free participant index for domain %d\n", config.domainId);
        goto err_unicast_sockets;
      }
      config.participantIndex = ppid;
    }
    else
    {
      assert(0);
    }
    nn_log (LC_CONFIG, "rtps_init: uc ports: disc %u data %u\n", port_disc_uc, port_data_uc);
  }
  nn_log (LC_CONFIG, "rtps_init: domainid %d participantid %d\n", config.domainId, config.participantIndex);

  gv.waitset = os_sockWaitsetNew ();

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

  if (gv.m_factory->m_connless)
  {
    uint32_t port;

    TRACE (("Unicast Ports: discovery %u data %u \n",
      ddsi_tran_port (gv.disc_conn_uc), ddsi_tran_port (gv.data_conn_uc)));

    if (config.allowMulticast)
    {
      ddsi_tran_qos_t qos = ddsi_tran_create_qos ();
      qos->m_multicast = true;

      /* FIXME: should check for overflow */
      port = (uint32_t) (config.port_base + config.port_dg * config.domainId + config.port_d0);
      gv.disc_conn_mc = ddsi_factory_create_conn (gv.m_factory, port, qos);

      port = (uint32_t) (config.port_base + config.port_dg * config.domainId + config.port_d2);
      gv.data_conn_mc = ddsi_factory_create_conn (gv.m_factory, port, qos);

      ddsi_tran_free_qos (qos);

      if (gv.disc_conn_mc == NULL || gv.data_conn_mc == NULL)
        goto err_mc_conn;

      TRACE (("Multicast Ports: discovery %u data %u \n",
        ddsi_tran_port (gv.disc_conn_mc), ddsi_tran_port (gv.data_conn_mc)));

      /* Set multicast locators */
      if (!is_unspec_locator(&gv.loc_spdp_mc))
        gv.loc_spdp_mc.port = ddsi_tran_port (gv.disc_conn_mc);
      if (!is_unspec_locator(&gv.loc_meta_mc))
        gv.loc_meta_mc.port = ddsi_tran_port (gv.disc_conn_mc);
      if (!is_unspec_locator(&gv.loc_default_mc))
        gv.loc_default_mc.port = ddsi_tran_port (gv.data_conn_mc);

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
        NN_ERROR ("Failed to create %s listener\n", gv.m_factory->m_typename);
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
  TRACE (("Timed event transmit port: %d\n", (int) ddsi_tran_port (gv.tev_conn)));

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
          NN_FATAL ("failed to create transmit connection for channel %s\n", chptr->name);
        }
      }
      else
      {
        chptr->transmit_conn = gv.data_conn_uc;
      }
      TRACE (("channel %s: transmit port %d\n", chptr->name, (int) ddsi_tran_port (chptr->transmit_conn)));

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
  if (config.peers)
    add_peer_addresses (gv.as_disc, config.peers);
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

  /* We create the rbufpool for the receive thread, and so we'll
     become the initial owner thread. The receive thread will change
     it before it does anything with it. */
  if ((gv.rbufpool = nn_rbufpool_new (config.rbuf_size, config.rmsg_chunk_size)) == NULL)
  {
    NN_FATAL ("rtps_init: can't allocate receive buffer pool\n");
  }

  gv.rtps_keepgoing = 1;
  os_rwlockInit (&gv.qoslock);

  {
    int r;
    gv.builtins_dqueue = nn_dqueue_new ("builtins", config.delivery_queue_maxsamples, builtins_dqueue_handler, NULL);
    if ((r = xeventq_start (gv.xevents, NULL)) < 0)
    {
      NN_FATAL ("failed to start global event processing thread (%d)\n", r);
    }
  }

  nn_xpack_sendq_init();
  nn_xpack_sendq_start();

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
          NN_FATAL ("failed to start event processing thread for channel '%s' (%d)\n", chptr->name, r);
      }
      chptr = chptr->next;
    }
  }
#else
  gv.user_dqueue = nn_dqueue_new ("user", config.delivery_queue_maxsamples, user_dqueue_handler, NULL);
#endif

  gv.recv_ts = create_thread ("recv", (uint32_t (*) (void *)) recv_thread, gv.rbufpool);
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
  if (gv.data_conn_mc)
    ddsi_conn_free (gv.data_conn_mc);
  if (gv.pcap_fp)
    os_mutexDestroy (&gv.pcap_lock);
  os_sockWaitsetFree (gv.waitset);
  if (gv.disc_conn_uc == gv.data_conn_uc)
    ddsi_conn_free (gv.data_conn_uc);
  else
  {
    ddsi_conn_free (gv.data_conn_uc);
    ddsi_conn_free (gv.disc_conn_uc);
  }
err_unicast_sockets:
  dds_tkmap_free (gv.m_tkmap);
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
  ddsi_serstatepool_free (gv.serpool);
  nn_xmsgpool_free (gv.xmsgpool);
  (ddsi_plugin.fini_fn) ();
err_set_ext_address:
  while (gv.recvips)
  {
    struct ospl_in_addr_node *n = gv.recvips;
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

void rtps_term_prep (void)
{
  /* Stop all I/O */
  os_mutexLock (&gv.lock);
  if (gv.rtps_keepgoing)
  {
    gv.rtps_keepgoing = 0; /* so threads will stop once they get round to checking */
    os_atomic_fence ();
    /* can't wake up throttle_writer, currently, but it'll check every few seconds */
    os_sockWaitsetTrigger (gv.waitset);
  }
  os_mutexUnlock (&gv.lock);
}

void rtps_term (void)
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
  join_thread (gv.recv_ts);

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
    const nn_vendorid_t ownvendorid = MY_VENDOR_ID;
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
      if (!is_builtin_entityid (wr->e.guid.entityid, ownvendorid))
        delete_writer_nolinger (&wr->e.guid);
    }
    ephash_enum_writer_fini (&est_wr);
    thread_state_awake (self);
    ephash_enum_reader_init (&est_rd);
    while ((rd = ephash_enum_reader_next (&est_rd)) != NULL)
    {
      if (!is_builtin_entityid (rd->e.guid.entityid, ownvendorid))
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
     certain that no new GC requests will be added */
  os_mutexLock (&gv.participant_set_lock);
  while (gv.nparticipants > 0)
    os_condWait (&gv.participant_set_cond, &gv.participant_set_lock);
  os_mutexUnlock (&gv.participant_set_lock);

  /* Clean up privileged_pp -- it must be NULL now (all participants
     are gone), but the lock still needs to be destroyed */
  assert (gv.privileged_pp == NULL);
  os_mutexDestroy (&gv.privileged_pp_lock);

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

  nn_xpack_sendq_stop();
  nn_xpack_sendq_fini();

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

  os_sockWaitsetFree (gv.waitset);

  (void) joinleave_spdp_defmcip (0);

  ddsi_conn_free (gv.disc_conn_mc);
  ddsi_conn_free (gv.data_conn_mc);
  if (gv.disc_conn_uc == gv.data_conn_uc)
  {
    ddsi_conn_free (gv.data_conn_uc);
  }
  else
  {
    ddsi_conn_free (gv.data_conn_uc);
    ddsi_conn_free (gv.disc_conn_uc);
  }

  /* Not freeing gv.tev_conn: it aliases data_conn_uc */

  ddsi_tran_factories_fini ();

  if (gv.pcap_fp)
  {
    os_mutexDestroy (&gv.pcap_lock);
    fclose (gv.pcap_fp);
  }

  unref_addrset (gv.as_disc);
  unref_addrset (gv.as_disc_group);

  /* Must delay freeing of rbufpools until after *all* references have
     been dropped, which only happens once all receive threads have
     stopped, defrags and reorders have been freed, and all delivery
     queues been drained.  I.e., until very late in the game. */
  nn_rbufpool_free (gv.rbufpool);
  dds_tkmap_free (gv.m_tkmap);

  ephash_free (gv.guid_hash);
  gv.guid_hash = NULL;
  deleted_participants_admin_fini ();
  lease_management_term ();
  os_mutexDestroy (&gv.participant_set_lock);
  os_condDestroy (&gv.participant_set_cond);

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
    struct ospl_in_addr_node *n = gv.recvips;
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

  ddsi_serstatepool_free (gv.serpool);
  nn_xmsgpool_free (gv.xmsgpool);
  (ddsi_plugin.fini_fn) ();
}
