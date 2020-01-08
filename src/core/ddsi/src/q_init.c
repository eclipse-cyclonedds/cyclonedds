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

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sync.h"

#include "dds/ddsrt/avl.h"
#include "dds/ddsrt/thread_pool.h"

#include "dds/ddsi/q_protocol.h"
#include "dds/ddsi/q_rtps.h"
#include "dds/ddsi/q_misc.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsi/q_plist.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_lat_estim.h"
#include "dds/ddsi/q_bitset.h"
#include "dds/ddsi/q_xevent.h"
#include "dds/ddsi/q_addrset.h"
#include "dds/ddsi/q_ddsi_discovery.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/q_thread.h"
#include "dds/ddsi/q_ephash.h"
#include "dds/ddsi/q_lease.h"
#include "dds/ddsi/q_gc.h"
#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_nwif.h"
#include "dds/ddsi/q_globals.h"
#include "dds/ddsi/q_xmsg.h"
#include "dds/ddsi/q_receive.h"
#include "dds/ddsi/q_pcap.h"
#include "dds/ddsi/q_feature_check.h"
#include "dds/ddsi/q_debmon.h"
#include "dds/ddsi/q_init.h"
#include "dds/ddsi/ddsi_threadmon.h"

#include "dds/ddsi/ddsi_tran.h"
#include "dds/ddsi/ddsi_udp.h"
#include "dds/ddsi/ddsi_tcp.h"
#include "dds/ddsi/ddsi_raweth.h"
#include "dds/ddsi/ddsi_mcgroup.h"
#include "dds/ddsi/ddsi_serdata_default.h"

#include "dds/ddsi/ddsi_tkmap.h"
#include "dds__whc.h"
#include "dds/ddsi/ddsi_iid.h"

#include "dds/ddsi/ddsi_security_omg.h"

static void add_peer_addresses (const struct q_globals *gv, struct addrset *as, const struct config_peer_listelem *list)
{
  while (list)
  {
    add_addresses_to_addrset (gv, as, list->peer, -1, "add_peer_addresses", 0);
    list = list->next;
  }
}

enum make_uc_sockets_ret {
  MUSRET_SUCCESS,
  MUSRET_INVALID_PORTS,
  MUSRET_NOSOCKET
};

static enum make_uc_sockets_ret make_uc_sockets (struct q_globals *gv, uint32_t * pdisc, uint32_t * pdata, int ppid)
{
  if (gv->config.many_sockets_mode == MSM_NO_UNICAST)
  {
    assert (ppid == PARTICIPANT_INDEX_NONE);
    *pdata = *pdisc = ddsi_get_port (&gv->config, DDSI_PORT_MULTI_DISC, ppid);
    if (gv->config.allowMulticast)
    {
      /* FIXME: ugly hack - but we'll fix up after creating the multicast sockets */
      return MUSRET_SUCCESS;
    }
  }

  *pdisc = ddsi_get_port (&gv->config, DDSI_PORT_UNI_DISC, ppid);
  *pdata = ddsi_get_port (&gv->config, DDSI_PORT_UNI_DATA, ppid);

  if (!ddsi_is_valid_port (gv->m_factory, *pdisc) || !ddsi_is_valid_port (gv->m_factory, *pdata))
    return MUSRET_INVALID_PORTS;

  gv->disc_conn_uc = ddsi_factory_create_conn (gv->m_factory, *pdisc, NULL);
  if (gv->disc_conn_uc)
  {
    /* Check not configured to use same unicast port for data and discovery */

    if (*pdata != 0 && (*pdata != *pdisc))
    {
      gv->data_conn_uc = ddsi_factory_create_conn (gv->m_factory, *pdata, NULL);
    }
    else
    {
      gv->data_conn_uc = gv->disc_conn_uc;
    }
    if (gv->data_conn_uc == NULL)
    {
      ddsi_conn_free (gv->disc_conn_uc);
      gv->disc_conn_uc = NULL;
    }
    else
    {
      /* Set unicast locators */
      ddsi_conn_locator (gv->disc_conn_uc, &gv->loc_meta_uc);
      ddsi_conn_locator (gv->data_conn_uc, &gv->loc_default_uc);
    }
  }

  return gv->data_conn_uc ? MUSRET_SUCCESS : MUSRET_NOSOCKET;
}

static void make_builtin_endpoint_xqos (dds_qos_t *q, const dds_qos_t *template)
{
  nn_xqos_copy (q, template);
  q->reliability.kind = DDS_RELIABILITY_RELIABLE;
  q->reliability.max_blocking_time = 100 * T_MILLISECOND;
  q->durability.kind = DDS_DURABILITY_TRANSIENT_LOCAL;
}

#ifdef DDSI_INCLUDE_SECURITY
static void make_builtin_volatile_endpoint_xqos (dds_qos_t *q, const dds_qos_t *template)
{
  nn_xqos_copy (q, template);
  q->reliability.kind = DDS_RELIABILITY_RELIABLE;
  q->reliability.max_blocking_time = 100 * T_MILLISECOND;
  q->durability.kind = DDS_DURABILITY_VOLATILE;
  q->history.kind = DDS_HISTORY_KEEP_ALL;
}

static void add_property_to_xqos(dds_qos_t *q, const char *name, const char *value)
{
  assert(!(q->present & QP_PROPERTY_LIST));
  q->present |= QP_PROPERTY_LIST;
  q->property.value.n = 1;
  q->property.value.props = ddsrt_malloc(sizeof(dds_property_t));
  q->property.binary_value.n = 0;
  q->property.binary_value.props = NULL;
  q->property.value.props[0].name = ddsrt_strdup(name);
  q->property.value.props[0].value = ddsrt_strdup(value);
}
#endif

static int set_recvips (struct q_globals *gv)
{
  gv->recvips = NULL;

  if (gv->config.networkRecvAddressStrings)
  {
    if (ddsrt_strcasecmp (gv->config.networkRecvAddressStrings[0], "all") == 0)
    {
#if DDSRT_HAVE_IPV6
      if (gv->ipv6_link_local)
      {
        GVWARNING ("DDSI2EService/General/MulticastRecvNetworkInterfaceAddresses: using 'preferred' instead of 'all' because of IPv6 link-local address\n");
        gv->recvips_mode = RECVIPS_MODE_PREFERRED;
      }
      else
#endif
      {
        gv->recvips_mode = RECVIPS_MODE_ALL;
      }
    }
    else if (ddsrt_strcasecmp (gv->config.networkRecvAddressStrings[0], "any") == 0)
    {
#if DDSRT_HAVE_IPV6
      if (gv->ipv6_link_local)
      {
        GVERROR ("DDSI2EService/General/MulticastRecvNetworkInterfaceAddresses: 'any' is unsupported in combination with an IPv6 link-local address\n");
        return -1;
      }
#endif
      gv->recvips_mode = RECVIPS_MODE_ANY;
    }
    else if (ddsrt_strcasecmp (gv->config.networkRecvAddressStrings[0], "preferred") == 0)
    {
      gv->recvips_mode = RECVIPS_MODE_PREFERRED;
    }
    else if (ddsrt_strcasecmp (gv->config.networkRecvAddressStrings[0], "none") == 0)
    {
      gv->recvips_mode = RECVIPS_MODE_NONE;
    }
#if DDSRT_HAVE_IPV6
    else if (gv->ipv6_link_local)
    {
      /* If the configuration explicitly includes the selected
       interface, treat it as "preferred", else as "none"; warn if
       interfaces other than the selected one are included. */
      int i, have_selected = 0, have_others = 0;
      for (i = 0; gv->config.networkRecvAddressStrings[i] != NULL; i++)
      {
        nn_locator_t loc;
        if (ddsi_locator_from_string(gv, &loc, gv->config.networkRecvAddressStrings[i], gv->m_factory) != AFSR_OK)
        {
          GVERROR ("%s: not a valid address in DDSI2EService/General/MulticastRecvNetworkInterfaceAddresses\n", gv->config.networkRecvAddressStrings[i]);
          return -1;
        }
        if (compare_locators(&loc, &gv->interfaces[gv->selected_interface].loc) == 0)
          have_selected = 1;
        else
          have_others = 1;
      }
      gv->recvips_mode = have_selected ? RECVIPS_MODE_PREFERRED : RECVIPS_MODE_NONE;
      if (have_others)
      {
        GVWARNING ("DDSI2EService/General/MulticastRecvNetworkInterfaceAddresses: using 'preferred' because of IPv6 local address\n");
      }
    }
#endif
    else
    {
      struct config_in_addr_node **recvnode = &gv->recvips;
      int i, j;
      gv->recvips_mode = RECVIPS_MODE_SOME;
      for (i = 0; gv->config.networkRecvAddressStrings[i] != NULL; i++)
      {
        nn_locator_t loc;
        if (ddsi_locator_from_string(gv, &loc, gv->config.networkRecvAddressStrings[i], gv->m_factory) != AFSR_OK)
        {
          GVERROR ("%s: not a valid address in DDSI2EService/General/MulticastRecvNetworkInterfaceAddresses\n", gv->config.networkRecvAddressStrings[i]);
          return -1;
        }
        for (j = 0; j < gv->n_interfaces; j++)
        {
          if (compare_locators(&loc, &gv->interfaces[j].loc) == 0)
            break;
        }
        if (j == gv->n_interfaces)
        {
          GVERROR ("No interface bound to requested address '%s'\n", gv->config.networkRecvAddressStrings[i]);
          return -1;
        }
        *recvnode = ddsrt_malloc (sizeof (struct config_in_addr_node));
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
static int string_to_default_locator (const struct q_globals *gv, nn_locator_t *loc, const char *string, uint32_t port, int mc, const char *tag)
{
  if (strspn (string, " \t") == strlen (string))
  {
    /* string consisting of just spaces and/or tabs (that includes the empty string) is ignored */
    return 0;
  }
  switch (ddsi_locator_from_string(gv, loc, string, gv->m_factory))
  {
    case AFSR_OK:
      break;
    case AFSR_INVALID:
      GVERROR ("%s: not a valid address (%s)\n", string, tag);
      return -1;
    case AFSR_UNKNOWN:
      GVERROR ("%s: address name resolution failure (%s)\n", string, tag);
      return -1;
    case AFSR_MISMATCH:
      GVERROR ("%s: invalid address kind (%s)\n", string, tag);
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
    const int ismc = is_unspec_locator (loc) || ddsi_is_mcaddr (gv, loc);
    if (mc != ismc)
    {
      GVERROR ("%s: %s %s be the unspecified address or a multicast address\n", string, tag, rel);
      return -1;
    }
  }
  return 1;
}

static int set_spdp_address (struct q_globals *gv)
{
  const uint32_t port = ddsi_get_port (&gv->config, DDSI_PORT_MULTI_DISC, 0);
  int rc = 0;
  /* FIXME: FIXME: FIXME: */
  gv->loc_spdp_mc.kind = NN_LOCATOR_KIND_INVALID;
  if (strcmp (gv->config.spdpMulticastAddressString, "239.255.0.1") != 0)
  {
    if ((rc = string_to_default_locator (gv, &gv->loc_spdp_mc, gv->config.spdpMulticastAddressString, port, 1, "SPDP address")) < 0)
      return rc;
  }
  if (rc == 0 && gv->m_factory->m_connless) /* FIXME: connless the right one? */
  {
    /* There isn't a standard IPv6 multicast group for DDSI. For
       some reason, node-local multicast addresses seem to be
       unsupported (ff01::... would be a node-local one I think), so
       instead do link-local. I suppose we could use the hop limit
       to make it node-local.  If other hosts reach us in some way,
       we'll of course respond. */
    rc = string_to_default_locator (gv, &gv->loc_spdp_mc, gv->m_factory->m_default_spdp_address, port, 1, "SPDP address");
    assert (rc > 0);
  }
#ifdef DDSI_INCLUDE_SSM
  if (gv->loc_spdp_mc.kind != NN_LOCATOR_KIND_INVALID && ddsi_is_ssm_mcaddr (gv, &gv->loc_spdp_mc))
  {
    GVERROR ("%s: SPDP address may not be an SSM address\n", gv->config.spdpMulticastAddressString);
    return -1;
  }
#endif
  return 0;
}

static int set_default_mc_address (struct q_globals *gv)
{
  const uint32_t port = ddsi_get_port (&gv->config, DDSI_PORT_MULTI_DATA, 0);
  int rc;
  if (!gv->config.defaultMulticastAddressString)
    gv->loc_default_mc = gv->loc_spdp_mc;
  else if ((rc = string_to_default_locator (gv, &gv->loc_default_mc, gv->config.defaultMulticastAddressString, port, 1, "default multicast address")) < 0)
    return rc;
  else if (rc == 0)
    gv->loc_default_mc = gv->loc_spdp_mc;
  gv->loc_meta_mc = gv->loc_default_mc;
  return 0;
}

static int set_ext_address_and_mask (struct q_globals *gv)
{
  nn_locator_t loc;
  int rc;

  if (!gv->config.externalAddressString)
    gv->extloc = gv->ownloc;
  else if ((rc = string_to_default_locator (gv, &loc, gv->config.externalAddressString, 0, 0, "external address")) < 0)
    return rc;
  else if (rc == 0) {
    GVWARNING ("Ignoring ExternalNetworkAddress %s\n", gv->config.externalAddressString);
    gv->extloc = gv->ownloc;
  } else {
    gv->extloc = loc;
  }

  if (!gv->config.externalMaskString || strcmp (gv->config.externalMaskString, "0.0.0.0") == 0)
  {
    memset(&gv->extmask.address, 0, sizeof(gv->extmask.address));
    gv->extmask.kind = NN_LOCATOR_KIND_INVALID;
    gv->extmask.port = NN_LOCATOR_PORT_INVALID;
  }
  else if (gv->config.transport_selector != TRANS_UDP)
  {
    GVERROR ("external network masks only supported in IPv4 mode\n");
    return -1;
  }
  else
  {
    if ((rc = string_to_default_locator (gv, &gv->extmask, gv->config.externalMaskString, 0, -1, "external mask")) < 0)
      return rc;
  }
  return 0;
}

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
static int known_channel_p (const struct q_globals *gv, const char *name)
{
  const struct config_channel_listelem *c;
  for (c = gv->config.channels; c; c = c->next)
    if (strcmp (name, c->name) == 0)
      return 1;
  return 0;
}
#endif

static int check_thread_properties (const struct q_globals *gv)
{
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  static const char *fixed[] = { "recv", "tev", "gc", "lease", "dq.builtins", "debmon", NULL };
  static const char *chanprefix[] = { "xmit.", "tev.","dq.",NULL };
#else
  static const char *fixed[] = { "recv", "tev", "gc", "lease", "dq.builtins", "xmit.user", "dq.user", "debmon", NULL };
#endif
  const struct config_thread_properties_listelem *e;
  int ok = 1, i;
  for (e = gv->config.thread_properties; e; e = e->next)
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
        if (strncmp (chanprefix[i], e->name, n) == 0 && known_channel_p (gv, e->name + n))
          break;
      }
      if (chanprefix[i] == NULL)
      {
        DDS_ILOG (DDS_LC_ERROR, gv->config.domainId, "config: DDSI2Service/Threads/Thread[@name=\"%s\"]: unknown thread\n", e->name);
        ok = 0;
      }
#else
      DDS_ILOG (DDS_LC_ERROR, gv->config.domainId, "config: DDSI2Service/Threads/Thread[@name=\"%s\"]: unknown thread\n", e->name);
      ok = 0;
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */
    }
  }
  return ok;
}

int rtps_config_open_trace (struct q_globals *gv)
{
  DDSRT_WARNING_MSVC_OFF(4996);
  int status;

  if (gv->config.tracefile == NULL || *gv->config.tracefile == 0 || gv->config.tracemask == 0)
  {
    gv->config.tracemask = 0;
    gv->config.tracefp = NULL;
    status = 1;
  }
  else if (ddsrt_strcasecmp (gv->config.tracefile, "stdout") == 0)
  {
    gv->config.tracefp = stdout;
    status = 1;
  }
  else if (ddsrt_strcasecmp (gv->config.tracefile, "stderr") == 0)
  {
    gv->config.tracefp = stderr;
    status = 1;
  }
  else if ((gv->config.tracefp = fopen (gv->config.tracefile, gv->config.tracingAppendToFile ? "a" : "w")) == NULL)
  {
    DDS_ILOG (DDS_LC_ERROR, gv->config.domainId, "%s: cannot open for writing\n", gv->config.tracefile);
    status = 0;
  }
  else
  {
    status = 1;
  }

  dds_log_cfg_init (&gv->logconfig, gv->config.domainId, gv->config.tracemask, stderr, gv->config.tracefp);
  return status;
  DDSRT_WARNING_MSVC_ON(4996);
}

int rtps_config_prep (struct q_globals *gv, struct cfgst *cfgst)
{
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  unsigned num_channels = 0;
  unsigned num_channel_threads = 0;
#endif

  /* advertised domain id defaults to the real domain id; clear "isdefault" so the config
     dump includes the actually used value rather than "default" */
  if (gv->config.extDomainId.isdefault)
  {
    gv->config.extDomainId.value = gv->config.domainId;
    gv->config.extDomainId.isdefault = 0;
  }

  {
    char message[256];
    int32_t ppidx;
    if (gv->config.participantIndex >= 0 || gv->config.participantIndex == PARTICIPANT_INDEX_NONE)
      ppidx = gv->config.participantIndex;
    else if (gv->config.participantIndex == PARTICIPANT_INDEX_AUTO)
      ppidx = gv->config.maxAutoParticipantIndex;
    else
    {
      assert (0);
      ppidx = 0;
    }
    if (!ddsi_valid_portmapping (&gv->config, ppidx, message, sizeof (message)))
    {
      DDS_ILOG (DDS_LC_ERROR, gv->config.domainId, "Invalid port mapping: %s\n", message);
      goto err_config_late_error;
    }
  }

  /* retry_on_reject_duration default is dependent on late_ack_mode and responsiveness timeout, so fix up */
  if (gv->config.whc_init_highwater_mark.isdefault)
    gv->config.whc_init_highwater_mark.value = gv->config.whc_lowwater_mark;
  if (gv->config.whc_highwater_mark < gv->config.whc_lowwater_mark ||
      gv->config.whc_init_highwater_mark.value < gv->config.whc_lowwater_mark ||
      gv->config.whc_init_highwater_mark.value > gv->config.whc_highwater_mark)
  {
    DDS_ILOG (DDS_LC_ERROR, gv->config.domainId, "Invalid watermark settings\n");
    goto err_config_late_error;
  }

  if (gv->config.besmode == BESMODE_MINIMAL && gv->config.many_sockets_mode == MSM_MANY_UNICAST)
  {
    /* These two are incompatible because minimal bes mode can result
       in implicitly creating proxy participants inheriting the
       address set from the ddsi2 participant (which is then typically
       inherited by readers/writers), but in many sockets mode each
       participant has its own socket, and therefore unique address
       set */
    DDS_ILOG (DDS_LC_ERROR, gv->config.domainId, "Minimal built-in endpoint set mode and ManySocketsMode are incompatible\n");
    goto err_config_late_error;
  }

  /* Dependencies between default values is not handled
   automatically by the gv->config processing (yet) */
  if (gv->config.many_sockets_mode == MSM_MANY_UNICAST)
  {
    if (gv->config.max_participants == 0)
      gv->config.max_participants = 100;
  }
  if (gv->config.max_queued_rexmit_bytes == 0)
  {
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
    if (gv->config.auxiliary_bandwidth_limit == 0)
      gv->config.max_queued_rexmit_bytes = 2147483647u;
    else
    {
      double max = (double) gv->config.auxiliary_bandwidth_limit * ((double) gv->config.nack_delay / 1e9);
      if (max < 0)
      {
        DDS_ILOG (DDS_LC_ERROR, gv->config.domainId.value, "AuxiliaryBandwidthLimit * NackDelay = %g bytes is insane\n", max);
        goto err_config_late_error;
      }
      gv->config.max_queued_rexmit_bytes = max > 2147483647.0 ? 2147483647u : (unsigned) max;
    }
#else
    gv->config.max_queued_rexmit_bytes = 2147483647u;
#endif /* DDSI_INCLUDE_BANDWIDTH_LIMITING */
  }

  /* Verify thread properties refer to defined threads */
  if (!check_thread_properties (gv))
  {
    goto err_config_late_error;
  }

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  {
    /* Determine number of configured channels to be able to
     determine the correct number of threads.  Also fix fields if
     at default, and check for some known IPv4/IPv6
     "compatibility" issues */
    struct config_channel_listelem *chptr = gv->config.channels;
    int error = 0;

    while (chptr)
    {
      size_t slen = strlen (chptr->name) + 5;
      char *thread_name = ddsrt_malloc (slen);
      (void) snprintf (thread_name, slen, "tev.%s", chptr->name);

      num_channels++;
      num_channel_threads += 2; /* xmit and dqueue */

      if (gv->config.transport_selector != TRANS_UDP && chptr->diffserv_field != 0)
      {
        DDS_ILOG (DDS_LC_ERROR, gv->config.domainId.value, "channel %s specifies IPv4 DiffServ settings which is incompatible with IPv6 use\n", chptr->name);
        error = 1;
      }

      if (
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
          chptr->auxiliary_bandwidth_limit > 0 ||
#endif
          lookup_thread_properties (thread_name))
        num_channel_threads++;

      ddsrt_free (thread_name);
      chptr = chptr->next;
    }
    if (error)
      goto err_config_late_error;
  }
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */

  /* Open tracing file after all possible config errors have been printed */
  if (! rtps_config_open_trace (gv))
  {
    goto err_config_late_error;
  }

  /* Thread admin: need max threads, which is currently (2 or 3) for each
     configured channel plus 9: main, recv (up to 3x), dqueue.builtin,
     lease, gc, debmon; once thread state admin has been inited, upgrade the
     main thread one participating in the thread tracking stuff as
     if it had been created using create_thread(). */
#if 0 /* FIXME: threads are per-process, not per-domain */
  {
  /* Temporary: thread states for each application thread is managed using thread_states structure
  */
#define USER_MAX_THREADS 50

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
    const unsigned max_threads = 9 + USER_MAX_THREADS + num_channel_threads + gv->config.ddsi2direct_max_threads;
#else
    const unsigned max_threads = 11 + USER_MAX_THREADS + gv->config.ddsi2direct_max_threads;
#endif
    thread_states_init (max_threads);
  }
#endif

  /* Now the per-thread-log-buffers are set up, so print the configuration.  After this there
     is no value to the source information for the various configuration elements, so free those. */
  config_print_cfgst (cfgst, &gv->logconfig);
  config_free_source_info (cfgst);
  return 0;

err_config_late_error:
  return -1;
}

struct joinleave_spdp_defmcip_helper_arg {
  struct q_globals *gv;
  int errcount;
  int dojoin;
};

static void joinleave_spdp_defmcip_helper (const nn_locator_t *loc, void *varg)
{
  struct joinleave_spdp_defmcip_helper_arg *arg = varg;
  if (!ddsi_is_mcaddr (arg->gv, loc))
    return;
#ifdef DDSI_INCLUDE_SSM
  /* Can't join SSM until we actually have a source */
  if (ddsi_is_ssm_mcaddr (arg->gv, loc))
    return;
#endif
  if (arg->dojoin) {
    if (ddsi_join_mc (arg->gv, arg->gv->mship, arg->gv->disc_conn_mc, NULL, loc) < 0 ||
        ddsi_join_mc (arg->gv, arg->gv->mship, arg->gv->data_conn_mc, NULL, loc) < 0)
      arg->errcount++;
  } else {
    if (ddsi_leave_mc (arg->gv, arg->gv->mship, arg->gv->disc_conn_mc, NULL, loc) < 0 ||
        ddsi_leave_mc (arg->gv, arg->gv->mship, arg->gv->data_conn_mc, NULL, loc) < 0)
      arg->errcount++;
  }
}

int joinleave_spdp_defmcip (struct q_globals *gv, int dojoin)
{
  /* Addrset provides an easy way to filter out duplicates */
  struct joinleave_spdp_defmcip_helper_arg arg;
  struct addrset *as = new_addrset ();
  arg.gv = gv;
  arg.errcount = 0;
  arg.dojoin = dojoin;
  if (gv->config.allowMulticast & AMC_SPDP)
    add_to_addrset (gv, as, &gv->loc_spdp_mc);
  if (gv->config.allowMulticast & ~AMC_SPDP)
    add_to_addrset (gv, as, &gv->loc_default_mc);
  addrset_forall (as, joinleave_spdp_defmcip_helper, &arg);
  unref_addrset (as);
  if (arg.errcount)
  {
    GVERROR ("rtps_init: failed to join multicast groups for domain %"PRIu32" participant %d\n", gv->config.domainId, gv->config.participantIndex);
    return -1;
  }
  return 0;
}

int create_multicast_sockets (struct q_globals *gv)
{
  ddsi_tran_qos_t qos = ddsi_tran_create_qos ();
  ddsi_tran_conn_t disc, data;
  uint32_t port;
  qos->m_multicast = 1;

  port = ddsi_get_port (&gv->config, DDSI_PORT_MULTI_DISC, 0);
  if (!ddsi_is_valid_port (gv->m_factory, port))
  {
    GVERROR ("Failed to create discovery multicast socket for domain %"PRIu32": resulting port number (%"PRIu32") is out of range\n",
             gv->config.extDomainId.value, port);
    goto err_disc;
  }
  if ((disc = ddsi_factory_create_conn (gv->m_factory, port, qos)) == NULL)
    goto err_disc;
  if (gv->config.many_sockets_mode == MSM_NO_UNICAST)
  {
    /* FIXME: not quite logical to tie this to "no unicast" */
    data = disc;
  }
  else
  {
    port = ddsi_get_port (&gv->config, DDSI_PORT_MULTI_DATA, 0);
    if (!ddsi_is_valid_port (gv->m_factory, port))
    {
      GVERROR ("Failed to create data multicast socket for domain %"PRIu32": resulting port number (%"PRIu32") is out of range\n",
               gv->config.extDomainId.value, port);
      goto err_disc;
    }
    if ((data = ddsi_factory_create_conn (gv->m_factory, port, qos)) == NULL)
    {
      goto err_data;
    }
  }
  ddsi_tran_free_qos (qos);

  gv->disc_conn_mc = disc;
  gv->data_conn_mc = data;
  GVLOG (DDS_LC_CONFIG, "Multicast Ports: discovery %"PRIu32" data %"PRIu32" \n",
         ddsi_conn_port (gv->disc_conn_mc), ddsi_conn_port (gv->data_conn_mc));
  return 1;

err_data:
  ddsi_conn_free (disc);
err_disc:
  ddsi_tran_free_qos (qos);
  return 0;
}

static void rtps_term_prep (struct q_globals *gv)
{
  /* Stop all I/O */
  ddsrt_mutex_lock (&gv->lock);
  if (ddsrt_atomic_ld32 (&gv->rtps_keepgoing))
  {
    ddsrt_atomic_st32 (&gv->rtps_keepgoing, 0); /* so threads will stop once they get round to checking */
    ddsrt_atomic_fence ();
    /* can't wake up throttle_writer, currently, but it'll check every few seconds */
    trigger_recv_threads (gv);
  }
  ddsrt_mutex_unlock (&gv->lock);
}

struct wait_for_receive_threads_helper_arg {
  struct q_globals *gv;
  unsigned count;
};

static void wait_for_receive_threads_helper (struct xevent *xev, void *varg, nn_mtime_t tnow)
{
  struct wait_for_receive_threads_helper_arg * const arg = varg;
  if (arg->count++ == arg->gv->config.recv_thread_stop_maxretries)
    abort ();
  trigger_recv_threads (arg->gv);
  (void) resched_xevent_if_earlier (xev, add_duration_to_mtime (tnow, T_SECOND));
}

static void wait_for_receive_threads (struct q_globals *gv)
{
  struct xevent *trigev;
  struct wait_for_receive_threads_helper_arg cbarg;
  cbarg.gv = gv;
  cbarg.count = 0;
  if ((trigev = qxev_callback (gv->xevents, add_duration_to_mtime (now_mt (), T_SECOND), wait_for_receive_threads_helper, &cbarg)) == NULL)
  {
    /* retrying is to deal a packet geting lost because the socket buffer is full or because the
       macOS firewall (and perhaps others) likes to ask if the process is allowed to receive data,
       dropping the packets until the user approves. */
    GVWARNING ("wait_for_receive_threads: failed to schedule periodic triggering of the receive threads to deal with packet loss\n");
  }
  for (uint32_t i = 0; i < gv->n_recv_threads; i++)
  {
    if (gv->recv_threads[i].ts)
    {
      join_thread (gv->recv_threads[i].ts);
      /* setting .ts to NULL helps in sanity checking */
      gv->recv_threads[i].ts = NULL;
    }
  }
  if (trigev)
  {
    delete_xevent (trigev);
  }
}

static struct ddsi_sertopic *make_special_topic (struct serdatapool *serpool, uint16_t enc_id, const struct ddsi_serdata_ops *ops)
{
  /* FIXME: two things (at least)
     - it claims there is a key, but the underlying type description is missing
       that only works as long as it ends up comparing the keyhash field ...
       the keyhash field should be eliminated; but this can simply be moved over to an alternate
       topic class, it need not use the "default" one, that's mere expediency
     - initialising/freeing them here, in this manner, is not very clean
       it should be moved to somewhere in the topic implementation
       (kinda natural if they stop being "default" ones) */
  struct ddsi_sertopic_default *st = ddsrt_malloc (sizeof (*st));
  memset (st, 0, sizeof (*st));
  ddsi_sertopic_init_anon (&st->c, &ddsi_sertopic_ops_default, ops, false);
  st->native_encoding_identifier = enc_id;
  st->serpool = serpool;
  st->nkeys = 1;
  return (struct ddsi_sertopic *) st;
}

static void make_special_topics (struct q_globals *gv)
{
  gv->plist_topic = make_special_topic (gv->serpool, DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? PL_CDR_LE : PL_CDR_BE, &ddsi_serdata_ops_plist);
  gv->rawcdr_topic = make_special_topic (gv->serpool, DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? CDR_LE : CDR_BE, &ddsi_serdata_ops_rawcdr);
}

static void free_special_topics (struct q_globals *gv)
{
  ddsi_sertopic_unref (gv->plist_topic);
  ddsi_sertopic_unref (gv->rawcdr_topic);
}

static int setup_and_start_recv_threads (struct q_globals *gv)
{
  for (uint32_t i = 0; i < MAX_RECV_THREADS; i++)
  {
    gv->recv_threads[i].ts = NULL;
    gv->recv_threads[i].arg.mode = RTM_SINGLE;
    gv->recv_threads[i].arg.rbpool = NULL;
    gv->recv_threads[i].arg.gv = gv;
    gv->recv_threads[i].arg.u.single.loc = NULL;
    gv->recv_threads[i].arg.u.single.conn = NULL;
  }

  /* First thread always uses a waitset and gobbles up all sockets not handled by dedicated threads - FIXME: MSM_NO_UNICAST mode with UDP probably doesn't even need this one to use a waitset */
  gv->n_recv_threads = 1;
  gv->recv_threads[0].name = "recv";
  gv->recv_threads[0].arg.mode = RTM_MANY;
  if (gv->m_factory->m_connless && gv->config.many_sockets_mode != MSM_NO_UNICAST && gv->config.multiple_recv_threads)
  {
    if (ddsi_is_mcaddr (gv, &gv->loc_default_mc) && !ddsi_is_ssm_mcaddr (gv, &gv->loc_default_mc) && (gv->config.allowMulticast & AMC_ASM))
    {
      /* Multicast enabled, but it isn't an SSM address => handle data multicasts on a separate thread (the trouble with SSM addresses is that we only join matching writers, which our own sockets typically would not be) */
      gv->recv_threads[gv->n_recv_threads].name = "recvMC";
      gv->recv_threads[gv->n_recv_threads].arg.mode = RTM_SINGLE;
      gv->recv_threads[gv->n_recv_threads].arg.u.single.conn = gv->data_conn_mc;
      gv->recv_threads[gv->n_recv_threads].arg.u.single.loc = &gv->loc_default_mc;
      ddsi_conn_disable_multiplexing (gv->data_conn_mc);
      gv->n_recv_threads++;
    }
    if (gv->config.many_sockets_mode == MSM_SINGLE_UNICAST)
    {
      /* No per-participant sockets => handle data unicasts on a separate thread as well */
      gv->recv_threads[gv->n_recv_threads].name = "recvUC";
      gv->recv_threads[gv->n_recv_threads].arg.mode = RTM_SINGLE;
      gv->recv_threads[gv->n_recv_threads].arg.u.single.conn = gv->data_conn_uc;
      gv->recv_threads[gv->n_recv_threads].arg.u.single.loc = &gv->loc_default_uc;
      ddsi_conn_disable_multiplexing (gv->data_conn_uc);
      gv->n_recv_threads++;
    }
  }
  assert (gv->n_recv_threads <= MAX_RECV_THREADS);

  /* For each thread, create rbufpool and waitset if needed, then start it */
  for (uint32_t i = 0; i < gv->n_recv_threads; i++)
  {
    /* We create the rbufpool for the receive thread, and so we'll
       become the initial owner thread. The receive thread will change
       it before it does anything with it. */
    if ((gv->recv_threads[i].arg.rbpool = nn_rbufpool_new (&gv->logconfig, gv->config.rbuf_size, gv->config.rmsg_chunk_size)) == NULL)
    {
      GVERROR ("rtps_init: can't allocate receive buffer pool for thread %s\n", gv->recv_threads[i].name);
      goto fail;
    }
    if (gv->recv_threads[i].arg.mode == RTM_MANY)
    {
      if ((gv->recv_threads[i].arg.u.many.ws = os_sockWaitsetNew ()) == NULL)
      {
        GVERROR ("rtps_init: can't allocate sock waitset for thread %s\n", gv->recv_threads[i].name);
        goto fail;
      }
    }
    if (create_thread (&gv->recv_threads[i].ts, gv, gv->recv_threads[i].name, recv_thread, &gv->recv_threads[i].arg) != DDS_RETCODE_OK)
    {
      GVERROR ("rtps_init: failed to start thread %s\n", gv->recv_threads[i].name);
      goto fail;
    }
  }
  return 0;

fail:
  /* to trigger any threads we already started to stop - xevent thread has already been started */
  rtps_term_prep (gv);
  wait_for_receive_threads (gv);
  for (uint32_t i = 0; i < gv->n_recv_threads; i++)
  {
    if (gv->recv_threads[i].arg.mode == RTM_MANY && gv->recv_threads[i].arg.u.many.ws)
      os_sockWaitsetFree (gv->recv_threads[i].arg.u.many.ws);
    if (gv->recv_threads[i].arg.rbpool)
      nn_rbufpool_free (gv->recv_threads[i].arg.rbpool);
  }
  return -1;
}

int rtps_init (struct q_globals *gv)
{
  uint32_t port_disc_uc = 0;
  uint32_t port_data_uc = 0;
  bool mc_available = true;

  gv->tstart = now ();    /* wall clock time, used in logs */

  nn_plist_init_tables ();

  gv->disc_conn_uc = NULL;
  gv->data_conn_uc = NULL;
  gv->disc_conn_mc = NULL;
  gv->data_conn_mc = NULL;
  gv->tev_conn = NULL;
  gv->listener = NULL;
  gv->thread_pool = NULL;
  gv->debmon = NULL;

  /* Print start time for referencing relative times in the remainder of the DDS_LOG. */
  {
    int sec = (int) (gv->tstart.v / 1000000000);
    int usec = (int) (gv->tstart.v % 1000000000) / 1000;
    char str[DDSRT_RFC3339STRLEN+1];
    ddsrt_ctime(gv->tstart.v, str, sizeof(str));
    GVLOG (DDS_LC_CONFIG, "started at %d.06%d -- %s\n", sec, usec, str);
  }

  /* Initialize thread pool */
  if (gv->config.tp_enable)
  {
    gv->thread_pool = ddsrt_thread_pool_new (gv->config.tp_threads, gv->config.tp_max_threads, 0, NULL);
  }

  /* Initialize UDP or TCP transport and resolve factory */
  switch (gv->config.transport_selector)
  {
    case TRANS_DEFAULT:
      assert(0);
    case TRANS_UDP:
    case TRANS_UDP6:
      gv->config.publish_uc_locators = 1;
      gv->config.enable_uc_locators = 1;
      if (ddsi_udp_init (gv) < 0)
        goto err_udp_tcp_init;
      gv->m_factory = ddsi_factory_find (gv, gv->config.transport_selector == TRANS_UDP ? "udp" : "udp6");
      break;
    case TRANS_TCP:
    case TRANS_TCP6:
      gv->config.publish_uc_locators = (gv->config.tcp_port != -1);
      gv->config.enable_uc_locators = 1;
      /* TCP affects what features are supported/required */
      gv->config.many_sockets_mode = MSM_SINGLE_UNICAST;
      gv->config.allowMulticast = AMC_FALSE;
      if (ddsi_tcp_init (gv) < 0)
        goto err_udp_tcp_init;
      gv->m_factory = ddsi_factory_find (gv, gv->config.transport_selector == TRANS_TCP ? "tcp" : "tcp6");
      break;
    case TRANS_RAWETH:
      gv->config.publish_uc_locators = 1;
      gv->config.enable_uc_locators = 0;
      gv->config.participantIndex = PARTICIPANT_INDEX_NONE;
      gv->config.many_sockets_mode = MSM_NO_UNICAST;
      if (ddsi_raweth_init (gv) < 0)
        goto err_udp_tcp_init;
      gv->m_factory = ddsi_factory_find (gv, "raweth");
      break;
  }

  if (!find_own_ip (gv, gv->config.networkAddressString))
  {
    /* find_own_ip already logs a more informative error message */
    GVLOG (DDS_LC_CONFIG, "No network interface selected\n");
    goto err_find_own_ip;
  }
  if (gv->config.allowMulticast)
  {
    if (!gv->interfaces[gv->selected_interface].mc_capable)
    {
      GVWARNING ("selected interface is not multicast-capable: disabling multicast\n");
      gv->config.allowMulticast = AMC_FALSE;
      /* ensure discovery can work: firstly, that the process will be reachable on a "well-known" port
         number, and secondly, that the local interface's IP address gets added to the discovery
         address set */
      gv->config.participantIndex = PARTICIPANT_INDEX_AUTO;
      mc_available = false;
    }
    else if (gv->config.allowMulticast & AMC_DEFAULT)
    {
      /* default is dependent on network interface type: if multicast is believed to be flaky,
         use multicast only for SPDP packets */
      assert ((gv->config.allowMulticast & ~AMC_DEFAULT) == 0);
      if (gv->interfaces[gv->selected_interface].mc_flaky)
      {
        gv->config.allowMulticast = AMC_SPDP;
        GVLOG (DDS_LC_CONFIG, "presumed flaky multicast, use for SPDP only\n");
      }
      else
      {
        GVLOG (DDS_LC_CONFIG, "presumed robust multicast support, use for everything\n");
        gv->config.allowMulticast = AMC_TRUE;
      }
    }
  }

  assert ((gv->config.allowMulticast & AMC_DEFAULT) == 0);
  if (set_recvips (gv) < 0)
    goto err_set_recvips;
  if (set_spdp_address (gv) < 0)
    goto err_set_ext_address;
  if (set_default_mc_address (gv) < 0)
    goto err_set_ext_address;
  if (set_ext_address_and_mask (gv) < 0)
    goto err_set_ext_address;

  {
    char buf[DDSI_LOCSTRLEN];
    /* the "ownip", "extip" labels in the trace have been there for so long, that it seems worthwhile to retain them even though they need not be IP any longer */
    GVLOG (DDS_LC_CONFIG, "ownip: %s\n", ddsi_locator_to_string_no_port (gv, buf, sizeof(buf), &gv->ownloc));
    GVLOG (DDS_LC_CONFIG, "extip: %s\n", ddsi_locator_to_string_no_port (gv, buf, sizeof(buf), &gv->extloc));
    GVLOG (DDS_LC_CONFIG, "extmask: %s%s\n", ddsi_locator_to_string_no_port (gv, buf, sizeof(buf), &gv->extmask), gv->m_factory->m_kind != NN_LOCATOR_KIND_UDPv4 ? " (not applicable)" : "");
    GVLOG (DDS_LC_CONFIG, "networkid: 0x%lx\n", (unsigned long) gv->myNetworkId);
    GVLOG (DDS_LC_CONFIG, "SPDP MC: %s\n", ddsi_locator_to_string_no_port (gv, buf, sizeof(buf), &gv->loc_spdp_mc));
    GVLOG (DDS_LC_CONFIG, "default MC: %s\n", ddsi_locator_to_string_no_port (gv, buf, sizeof(buf), &gv->loc_default_mc));
#ifdef DDSI_INCLUDE_SSM
    GVLOG (DDS_LC_CONFIG, "SSM support included\n");
#endif
  }

  if (gv->ownloc.kind != gv->extloc.kind)
    DDS_FATAL ("mismatch between network address kinds\n");

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  /* Convert address sets in partition mappings from string to address sets */
  {
    const uint32_t port = ddsi_get_port (&gv->config, DDSI_PORT_MULTI_DATA, 0);
    struct config_networkpartition_listelem *np;
    for (np = gv->config.networkPartitions; np; np = np->next)
    {
      static const char msgtag_fixed[] = ": partition address";
      size_t slen = strlen (np->name) + sizeof (msgtag_fixed);
      char * msgtag = ddsrt_malloc (slen);
      int rc;
      snprintf (msgtag, slen, "%s%s", np->name, msgtag_fixed);
      np->as = new_addrset ();
      rc = add_addresses_to_addrset (gv, np->as, np->address_string, (int) port, msgtag, 1);
      ddsrt_free (msgtag);
      if (rc < 0)
        goto err_network_partition_addrset;
    }
  }
#endif

  gv->xmsgpool = nn_xmsgpool_new ();
  gv->serpool = ddsi_serdatapool_new ();

  nn_plist_init_default_participant (&gv->default_plist_pp);
  nn_plist_init_default_participant (&gv->default_local_plist_pp);
  nn_xqos_init_default_reader (&gv->default_xqos_rd);
  nn_xqos_init_default_writer (&gv->default_xqos_wr);
  nn_xqos_init_default_writer_noautodispose (&gv->default_xqos_wr_nad);
  nn_xqos_init_default_topic (&gv->default_xqos_tp);
  nn_xqos_init_default_subscriber (&gv->default_xqos_sub);
  nn_xqos_init_default_publisher (&gv->default_xqos_pub);
  nn_xqos_copy (&gv->spdp_endpoint_xqos, &gv->default_xqos_rd);
  gv->spdp_endpoint_xqos.durability.kind = DDS_DURABILITY_TRANSIENT_LOCAL;
  make_builtin_endpoint_xqos (&gv->builtin_endpoint_xqos_rd, &gv->default_xqos_rd);
  make_builtin_endpoint_xqos (&gv->builtin_endpoint_xqos_wr, &gv->default_xqos_wr);
#ifdef DDSI_INCLUDE_SECURITY
  make_builtin_volatile_endpoint_xqos(&gv->builtin_volatile_xqos_rd, &gv->default_xqos_rd);
  make_builtin_volatile_endpoint_xqos(&gv->builtin_volatile_xqos_wr, &gv->default_xqos_wr);
  nn_xqos_copy (&gv->builtin_stateless_xqos_rd, &gv->default_xqos_rd);
  nn_xqos_copy (&gv->builtin_stateless_xqos_wr, &gv->default_xqos_wr);
  gv->builtin_stateless_xqos_wr.reliability.kind = DDS_RELIABILITY_BEST_EFFORT;
  gv->builtin_stateless_xqos_wr.durability.kind = DDS_DURABILITY_VOLATILE;

  /* Setting these properties allows the CryptoKeyFactory to recognize
   * the entities (see DDS Security spec chapter 8.8.8.1). */
  add_property_to_xqos(&gv->builtin_volatile_xqos_rd, "dds.sec.builtin_endpoint_name", "BuiltinParticipantVolatileMessageSecureReader");
  add_property_to_xqos(&gv->builtin_volatile_xqos_wr, "dds.sec.builtin_endpoint_name", "BuiltinParticipantVolatileMessageSecureWriter");
  
  q_omg_security_init( &gv->security_context );
#endif

  make_special_topics (gv);

  ddsrt_mutex_init (&gv->participant_set_lock);
  ddsrt_cond_init (&gv->participant_set_cond);
  lease_management_init (gv);
  gv->deleted_participants = deleted_participants_admin_new (&gv->logconfig, gv->config.prune_deleted_ppant.delay);
  gv->guid_hash = ephash_new (gv);

  ddsrt_mutex_init (&gv->privileged_pp_lock);
  gv->privileged_pp = NULL;

  /* Base participant GUID.  IID initialisation should be from a really good random
     generator and yield almost-unique numbers, and with a fallback of using process
     id, timestamp and a counter, so incorporating that should do a lot to construct
     a pseudo-random ID.  (The assumption here is that feeding pseudo-random data in
     MD5 will not change the randomness ...)  Mix in the network configuration to
     make machines with very reproducible boot sequences and low-resolution clocks
     distinguishable.

     This base is kept constant, prefix.u[1] and prefix.u[2] are then treated as a
     64-bit unsigned integer to which we add IIDs to generate a hopping sequence
     that won't repeat in the lifetime of the process.  Seems like it ought to work
     to keep the risks of collisions low. */
  {
    uint64_t iid = ddsrt_toBE8u (ddsi_iid_gen ());
    ddsrt_md5_state_t st;
    ddsrt_md5_byte_t digest[16];
    ddsrt_md5_init (&st);
    ddsrt_md5_append (&st, (const ddsrt_md5_byte_t *) &iid, sizeof (iid));
    for (int i = 0; i < gv->n_interfaces; i++)
    {
      const struct nn_interface *intf = &gv->interfaces[i];
      ddsrt_md5_append (&st, (const ddsrt_md5_byte_t *) &intf->loc.kind, sizeof (intf->loc.kind));
      ddsrt_md5_append (&st, (const ddsrt_md5_byte_t *) intf->loc.address, sizeof (intf->loc.address));
    }
    ddsrt_md5_finish (&st, digest);
    /* DDSI 2.2 requires the first two bytes of the GUID to be set to the vendor
       code -- a terrible waste of entropy ... */
    gv->ppguid_base.prefix.s[0] = NN_VENDORID_ECLIPSE.id[0];
    gv->ppguid_base.prefix.s[1] = NN_VENDORID_ECLIPSE.id[1];
    DDSRT_STATIC_ASSERT (sizeof (gv->ppguid_base.prefix.s) > 2 && sizeof (gv->ppguid_base.prefix.s) - 2 <= sizeof (digest));
    memcpy (&gv->ppguid_base.prefix.s[2], digest, sizeof (gv->ppguid_base.prefix.s) - 2);
    gv->ppguid_base.entityid.u = NN_ENTITYID_PARTICIPANT;
  }

  ddsrt_mutex_init (&gv->lock);
  ddsrt_mutex_init (&gv->spdp_lock);
  gv->spdp_defrag = nn_defrag_new (&gv->logconfig, NN_DEFRAG_DROP_OLDEST, gv->config.defrag_unreliable_maxsamples);
  gv->spdp_reorder = nn_reorder_new (&gv->logconfig, NN_REORDER_MODE_ALWAYS_DELIVER, gv->config.primary_reorder_maxsamples, false);

  gv->m_tkmap = ddsi_tkmap_new (gv);

  if (gv->m_factory->m_connless)
  {
    if (gv->config.participantIndex >= 0 || gv->config.participantIndex == PARTICIPANT_INDEX_NONE)
    {
      enum make_uc_sockets_ret musret = make_uc_sockets (gv, &port_disc_uc, &port_data_uc, gv->config.participantIndex);
      switch (musret)
      {
        case MUSRET_SUCCESS:
          break;
        case MUSRET_INVALID_PORTS:
          GVERROR ("Failed to create unicast sockets for domain %"PRIu32" participant index %d: resulting port numbers (%"PRIu32", %"PRIu32") are out of range\n",
                   gv->config.extDomainId.value, gv->config.participantIndex, port_disc_uc, port_data_uc);
          goto err_unicast_sockets;
        case MUSRET_NOSOCKET:
          GVERROR ("rtps_init: failed to create unicast sockets for domain %"PRId32" participant index %d (ports %"PRIu32", %"PRIu32")\n", gv->config.extDomainId.value, gv->config.participantIndex, port_disc_uc, port_data_uc);
          goto err_unicast_sockets;
      }
    }
    else if (gv->config.participantIndex == PARTICIPANT_INDEX_AUTO)
    {
      /* try to find a free one, and update gv->config.participantIndex */
      enum make_uc_sockets_ret musret = MUSRET_NOSOCKET;
      int ppid;
      GVLOG (DDS_LC_CONFIG, "rtps_init: trying to find a free participant index\n");
      for (ppid = 0; ppid <= gv->config.maxAutoParticipantIndex && musret == MUSRET_NOSOCKET; ppid++)
      {
        musret = make_uc_sockets (gv, &port_disc_uc, &port_data_uc, ppid);
        switch (musret)
        {
          case MUSRET_SUCCESS:
            break;
          case MUSRET_INVALID_PORTS:
            GVERROR ("Failed to create unicast sockets for domain %"PRIu32" participant index %d: resulting port numbers (%"PRIu32", %"PRIu32") are out of range\n",
                     gv->config.extDomainId.value, ppid, port_disc_uc, port_data_uc);
            goto err_unicast_sockets;
          case MUSRET_NOSOCKET: /* Try next one */
            break;
        }
      }
      if (ppid > gv->config.maxAutoParticipantIndex)
      {
        GVERROR ("Failed to find a free participant index for domain %"PRIu32"\n", gv->config.extDomainId.value);
        goto err_unicast_sockets;
      }
      gv->config.participantIndex = ppid;
    }
    else
    {
      assert(0);
    }
    GVLOG (DDS_LC_CONFIG, "rtps_init: uc ports: disc %"PRIu32" data %"PRIu32"\n", port_disc_uc, port_data_uc);
  }
  GVLOG (DDS_LC_CONFIG, "rtps_init: domainid %"PRIu32" participantid %d\n", gv->config.domainId, gv->config.participantIndex);

  if (gv->config.pcap_file && *gv->config.pcap_file)
  {
    gv->pcap_fp = new_pcap_file (&gv->logconfig, gv->config.pcap_file);
    if (gv->pcap_fp)
    {
      ddsrt_mutex_init (&gv->pcap_lock);
    }
  }
  else
  {
    gv->pcap_fp = NULL;
  }

  gv->mship = new_group_membership();

  if (gv->m_factory->m_connless)
  {
    if (!(gv->config.many_sockets_mode == MSM_NO_UNICAST && gv->config.allowMulticast))
      GVLOG (DDS_LC_CONFIG, "Unicast Ports: discovery %"PRIu32" data %"PRIu32"\n", ddsi_conn_port (gv->disc_conn_uc), ddsi_conn_port (gv->data_conn_uc));

    if (gv->config.allowMulticast)
    {
      if (!create_multicast_sockets (gv))
        goto err_mc_conn;

      if (gv->config.many_sockets_mode == MSM_NO_UNICAST)
      {
        gv->data_conn_uc = gv->data_conn_mc;
        gv->disc_conn_uc = gv->disc_conn_mc;
      }

      /* Set multicast locators */
      if (!is_unspec_locator(&gv->loc_spdp_mc))
        gv->loc_spdp_mc.port = ddsi_conn_port (gv->disc_conn_mc);
      if (!is_unspec_locator(&gv->loc_meta_mc))
        gv->loc_meta_mc.port = ddsi_conn_port (gv->disc_conn_mc);
      if (!is_unspec_locator(&gv->loc_default_mc))
        gv->loc_default_mc.port = ddsi_conn_port (gv->data_conn_mc);

      if (joinleave_spdp_defmcip (gv, 1) < 0)
        goto err_mc_conn;
    }
  }
  else
  {
    /* Must have a data_conn_uc/tev_conn/transmit_conn */
    gv->data_conn_uc = ddsi_factory_create_conn (gv->m_factory, 0, NULL);

    if (gv->config.tcp_port == -1)
      ; /* nop */
    else if (!ddsi_is_valid_port (gv->m_factory, (uint32_t) gv->config.tcp_port))
    {
      GVERROR ("Listener port %d is out of range for transport %s\n", gv->config.tcp_port, gv->m_factory->m_typename);
    }
    else
    {
      gv->listener = ddsi_factory_create_listener (gv->m_factory, (uint32_t) gv->config.tcp_port, NULL);
      if (gv->listener == NULL || ddsi_listener_listen (gv->listener) != 0)
      {
        GVERROR ("Failed to create %s listener\n", gv->m_factory->m_typename);
        if (gv->listener)
          ddsi_listener_free(gv->listener);
        goto err_mc_conn;
      }

      /* Set unicast locators from listener */
      set_unspec_locator (&gv->loc_spdp_mc);
      set_unspec_locator (&gv->loc_meta_mc);
      set_unspec_locator (&gv->loc_default_mc);

      ddsi_listener_locator (gv->listener, &gv->loc_meta_uc);
      ddsi_listener_locator (gv->listener, &gv->loc_default_uc);
    }
  }

  /* Create shared transmit connection */

  gv->tev_conn = gv->data_conn_uc;
  GVLOG (DDS_LC_CONFIG, "Timed event transmit port: %d\n", (int) ddsi_conn_port (gv->tev_conn));

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  {
    struct config_channel_listelem *chptr = gv->config.channels;
    while (chptr)
    {
      size_t slen = strlen (chptr->name) + 5;
      char * tname = ddsrt_malloc (slen);
      (void) snprintf (tname, slen, "tev.%s", chptr->name);

      /* Only actually create new connection if diffserv set */

      if (chptr->diffserv_field)
      {
        ddsi_tran_qos_t qos = ddsi_tran_create_qos ();
        qos->m_diffserv = chptr->diffserv_field;
        chptr->transmit_conn = ddsi_factory_create_conn (gv->m_factory, 0, qos);
        ddsi_tran_free_qos (qos);
        if (chptr->transmit_conn == NULL)
        {
          DDS_FATAL ("failed to create transmit connection for channel %s\n", chptr->name);
        }
      }
      else
      {
        chptr->transmit_conn = gv->data_conn_uc;
      }
      GVLOG (DDS_LC_CONFIG, "channel %s: transmit port %d\n", chptr->name, (int) ddsi_tran_port (chptr->transmit_conn));

#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
      if (chptr->auxiliary_bandwidth_limit > 0 || lookup_thread_properties (tname))
      {
        chptr->evq = xeventq_new
        (
          chptr->transmit_conn,
          gv->config.max_queued_rexmit_bytes,
          gv->config.max_queued_rexmit_msgs,
          chptr->auxiliary_bandwidth_limit
        );
      }
#else
      if (lookup_thread_properties (tname))
      {
        chptr->evq = xeventq_new
        (
          chptr->transmit_conn,
          gv->config.max_queued_rexmit_bytes,
          gv->config.max_queued_rexmit_msgs,
          0
        );
      }
#endif
      ddsrt_free (tname);
      chptr = chptr->next;
    }
  }
#endif /* DDSI_INCLUDE_NETWORK_CHANNELS */

  /* Create event queues */

  gv->xevents = xeventq_new
  (
    gv->tev_conn,
    gv->config.max_queued_rexmit_bytes,
    gv->config.max_queued_rexmit_msgs,
#ifdef DDSI_INCLUDE_BANDWIDTH_LIMITING
    gv->config.auxiliary_bandwidth_limit
#else
    0
#endif
  );

  gv->as_disc = new_addrset ();
  if (gv->config.allowMulticast & AMC_SPDP)
    add_to_addrset (gv, gv->as_disc, &gv->loc_spdp_mc);
  /* If multicast was enabled but not available, always add the local interface to the discovery address set.
     Conversion via string and add_peer_addresses has the benefit that the port number expansion happens
     automatically. */
  if (!mc_available)
  {
    struct config_peer_listelem peer_local;
    char local_addr[DDSI_LOCSTRLEN];
    ddsi_locator_to_string_no_port (gv, local_addr, sizeof (local_addr), &gv->interfaces[gv->selected_interface].loc);
    peer_local.next = NULL;
    peer_local.peer = local_addr;
    add_peer_addresses (gv, gv->as_disc, &peer_local);
  }
  if (gv->config.peers)
  {
    add_peer_addresses (gv, gv->as_disc, gv->config.peers);
  }
  if (gv->config.peers_group)
  {
    gv->as_disc_group = new_addrset ();
    add_peer_addresses (gv, gv->as_disc_group, gv->config.peers_group);
  }
  else
  {
    gv->as_disc_group = NULL;
  }

  gv->gcreq_queue = gcreq_queue_new (gv);

  ddsrt_atomic_st32 (&gv->rtps_keepgoing, 1);

  if (gv->config.xpack_send_async)
  {
    nn_xpack_sendq_init (gv);
    nn_xpack_sendq_start (gv);
  }

  gv->builtins_dqueue = nn_dqueue_new ("builtins", gv, gv->config.delivery_queue_maxsamples, builtins_dqueue_handler, NULL);
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  for (struct config_channel_listelem *chptr = gv->config.channels; chptr; chptr = chptr->next)
    chptr->dqueue = nn_dqueue_new (chptr->name, &gv->config, gv->config.delivery_queue_maxsamples, user_dqueue_handler, NULL);
#else
  gv->user_dqueue = nn_dqueue_new ("user", gv, gv->config.delivery_queue_maxsamples, user_dqueue_handler, NULL);
#endif

  return 0;

err_mc_conn:
  if (gv->disc_conn_mc)
    ddsi_conn_free (gv->disc_conn_mc);
  if (gv->data_conn_mc && gv->data_conn_mc != gv->disc_conn_mc)
    ddsi_conn_free (gv->data_conn_mc);
  if (gv->pcap_fp)
    ddsrt_mutex_destroy (&gv->pcap_lock);
  if (gv->disc_conn_uc != gv->disc_conn_mc)
    ddsi_conn_free (gv->disc_conn_uc);
  if (gv->data_conn_uc != gv->disc_conn_uc)
    ddsi_conn_free (gv->data_conn_uc);
  free_group_membership (gv->mship);
err_unicast_sockets:
  ddsi_tkmap_free (gv->m_tkmap);
  nn_reorder_free (gv->spdp_reorder);
  nn_defrag_free (gv->spdp_defrag);
  ddsrt_mutex_destroy (&gv->spdp_lock);
  ddsrt_mutex_destroy (&gv->lock);
  ddsrt_mutex_destroy (&gv->privileged_pp_lock);
  ephash_free (gv->guid_hash);
  gv->guid_hash = NULL;
  deleted_participants_admin_free (gv->deleted_participants);
  lease_management_term (gv);
  ddsrt_cond_destroy (&gv->participant_set_cond);
  ddsrt_mutex_destroy (&gv->participant_set_lock);
  free_special_topics (gv);
#ifdef DDSI_INCLUDE_SECURITY
  nn_xqos_fini (&gv->builtin_stateless_xqos_wr);
  nn_xqos_fini (&gv->builtin_stateless_xqos_rd);
  nn_xqos_fini (&gv->builtin_volatile_xqos_wr);
  nn_xqos_fini (&gv->builtin_volatile_xqos_rd);
  
  q_omg_security_deinit( &gv->security_context );
#endif
  nn_xqos_fini (&gv->builtin_endpoint_xqos_wr);
  nn_xqos_fini (&gv->builtin_endpoint_xqos_rd);
  nn_xqos_fini (&gv->spdp_endpoint_xqos);
  nn_xqos_fini (&gv->default_xqos_pub);
  nn_xqos_fini (&gv->default_xqos_sub);
  nn_xqos_fini (&gv->default_xqos_tp);
  nn_xqos_fini (&gv->default_xqos_wr_nad);
  nn_xqos_fini (&gv->default_xqos_wr);
  nn_xqos_fini (&gv->default_xqos_rd);
  nn_plist_fini (&gv->default_local_plist_pp);
  nn_plist_fini (&gv->default_plist_pp);
  ddsi_serdatapool_free (gv->serpool);
  nn_xmsgpool_free (gv->xmsgpool);
#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
err_network_partition_addrset:
  for (struct config_networkpartition_listelem *np = gv->config.networkPartitions; np; np = np->next)
    unref_addrset (np->as);
#endif
err_set_ext_address:
  while (gv->recvips)
  {
    struct config_in_addr_node *n = gv->recvips;
    gv->recvips = n->next;
    ddsrt_free (n);
  }
err_set_recvips:
err_find_own_ip:
  for (int i = 0; i < gv->n_interfaces; i++)
    ddsrt_free (gv->interfaces[i].name);
  ddsi_tran_factories_fini (gv);
err_udp_tcp_init:
  if (gv->config.tp_enable)
    ddsrt_thread_pool_free (gv->thread_pool);
  return -1;
}

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
static void stop_all_xeventq_upto (struct config_channel_listelem *chptr)
{
  for (struct config_channel_listelem *chptr1 = gv->config.channels; chptr1 != chptr; chptr1 = chptr1->next)
    if (chptr1->evq)
      xeventq_stop (chptr1->evq);
}
#endif

int rtps_start (struct q_globals *gv)
{
  if (xeventq_start (gv->xevents, NULL) < 0)
    return -1;
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  for (struct config_channel_listelem *chptr = gv->config.channels; chptr; chptr = chptr->next)
  {
    if (chptr->evq)
    {
      if (xeventq_start (chptr->evq, chptr->name) < 0)
      {
        stop_all_xeventq_upto (chptr);
        xeventq_stop (gv->xevents);
        return -1;
      }
    }
  }
#endif

  if (setup_and_start_recv_threads (gv) < 0)
  {
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
    stop_all_xeventq_upto (NULL);
#endif
    xeventq_stop (gv->xevents);
    return -1;
  }
  if (gv->listener)
  {
    if (create_thread (&gv->listen_ts, gv, "listen", (uint32_t (*) (void *)) listen_thread, gv->listener) != DDS_RETCODE_OK)
    {
      GVERROR ("failed to create TCP listener thread\n");
      ddsi_listener_free (gv->listener);
      gv->listener = NULL;
      rtps_stop (gv);
      return -1;
    }
  }
  if (gv->config.monitor_port >= 0)
  {
    if ((gv->debmon = new_debug_monitor (gv, gv->config.monitor_port)) == NULL)
    {
      GVERROR ("failed to create debug monitor thread\n");
      rtps_stop (gv);
      return -1;
    }
  }

  return 0;
}

struct dq_builtins_ready_arg {
  ddsrt_mutex_t lock;
  ddsrt_cond_t cond;
  int ready;
};

static void builtins_dqueue_ready_cb (void *varg)
{
  struct dq_builtins_ready_arg *arg = varg;
  ddsrt_mutex_lock (&arg->lock);
  arg->ready = 1;
  ddsrt_cond_broadcast (&arg->cond);
  ddsrt_mutex_unlock (&arg->lock);
}

void rtps_stop (struct q_globals *gv)
{
  struct thread_state1 * const ts1 = lookup_thread_state ();

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  struct config_channel_listelem * chptr;
#endif

  if (gv->debmon)
  {
    free_debug_monitor (gv->debmon);
    gv->debmon = NULL;
  }

  /* Stop all I/O */
  rtps_term_prep (gv);
  wait_for_receive_threads (gv);

  if (gv->listener)
  {
    ddsi_listener_unblock(gv->listener);
    join_thread (gv->listen_ts);
    ddsi_listener_free(gv->listener);
  }

  xeventq_stop (gv->xevents);
#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  for (chptr = gv->config.channels; chptr; chptr = chptr->next)
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
    ddsrt_mutex_init (&arg.lock);
    ddsrt_cond_init (&arg.cond);
    arg.ready = 0;
    nn_dqueue_enqueue_callback(gv->builtins_dqueue, builtins_dqueue_ready_cb, &arg);
    ddsrt_mutex_lock (&arg.lock);
    while (!arg.ready)
      ddsrt_cond_wait (&arg.cond, &arg.lock);
    ddsrt_mutex_unlock (&arg.lock);
    ddsrt_cond_destroy (&arg.cond);
    ddsrt_mutex_destroy (&arg.lock);
  }

  /* Once the receive threads have stopped, defragmentation and
     reorder state can't change anymore, and can be freed safely. */
  nn_reorder_free (gv->spdp_reorder);
  nn_defrag_free (gv->spdp_defrag);
  ddsrt_mutex_destroy (&gv->spdp_lock);

  {
    struct ephash_enum_proxy_participant est;
    struct proxy_participant *proxypp;
    const nn_wctime_t tnow = now();
    /* Clean up proxy readers, proxy writers and proxy
       participants. Deleting a proxy participants deletes all its
       readers and writers automatically */
    thread_state_awake (ts1, gv);
    ephash_enum_proxy_participant_init (&est, gv->guid_hash);
    while ((proxypp = ephash_enum_proxy_participant_next (&est)) != NULL)
    {
      delete_proxy_participant_by_guid (gv, &proxypp->e.guid, tnow, 1);
    }
    ephash_enum_proxy_participant_fini (&est);
    thread_state_asleep (ts1);
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
    thread_state_awake (ts1, gv);
    ephash_enum_writer_init (&est_wr, gv->guid_hash);
    while ((wr = ephash_enum_writer_next (&est_wr)) != NULL)
    {
      if (!is_builtin_entityid (wr->e.guid.entityid, NN_VENDORID_ECLIPSE))
        delete_writer_nolinger (gv, &wr->e.guid);
    }
    ephash_enum_writer_fini (&est_wr);
    thread_state_awake_to_awake_no_nest (ts1);
    ephash_enum_reader_init (&est_rd, gv->guid_hash);
    while ((rd = ephash_enum_reader_next (&est_rd)) != NULL)
    {
      if (!is_builtin_entityid (rd->e.guid.entityid, NN_VENDORID_ECLIPSE))
        delete_reader (gv, &rd->e.guid);
    }
    ephash_enum_reader_fini (&est_rd);
    thread_state_awake_to_awake_no_nest (ts1);
    ephash_enum_participant_init (&est_pp, gv->guid_hash);
    while ((pp = ephash_enum_participant_next (&est_pp)) != NULL)
    {
      delete_participant (gv, &pp->e.guid);
    }
    ephash_enum_participant_fini (&est_pp);
    thread_state_asleep (ts1);
  }

  /* Wait until all participants are really gone => by then we can be
     certain that no new GC requests will be added, short of what we
     do here */
  ddsrt_mutex_lock (&gv->participant_set_lock);
  while (gv->nparticipants > 0)
    ddsrt_cond_wait (&gv->participant_set_cond, &gv->participant_set_lock);
  ddsrt_mutex_unlock (&gv->participant_set_lock);

  /* Wait until no more GC requests are outstanding -- not really
     necessary, but it allows us to claim the stack is quiescent
     at this point */
  gcreq_queue_drain (gv->gcreq_queue);

  /* Clean up privileged_pp -- it must be NULL now (all participants
     are gone), but the lock still needs to be destroyed */
  assert (gv->privileged_pp == NULL);
  ddsrt_mutex_destroy (&gv->privileged_pp_lock);
}

void rtps_fini (struct q_globals *gv)
{
  /* Shut down the GC system -- no new requests will be added */
  gcreq_queue_free (gv->gcreq_queue);

  /* No new data gets added to any admin, all synchronous processing
     has ended, so now we can drain the delivery queues to end up with
     the expected reference counts all over the radmin thingummies. */
  nn_dqueue_free (gv->builtins_dqueue);

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  chptr = gv->config.channels;
  while (chptr)
  {
    nn_dqueue_free (chptr->dqueue);
    chptr = chptr->next;
  }
#else
  nn_dqueue_free (gv->user_dqueue);
#endif

  xeventq_free (gv->xevents);

  if (gv->config.xpack_send_async)
  {
    nn_xpack_sendq_stop (gv);
    nn_xpack_sendq_fini (gv);
  }

#ifdef DDSI_INCLUDE_NETWORK_CHANNELS
  chptr = gv->config.channels;
  while (chptr)
  {
    if (chptr->evq)
    {
      xeventq_free (chptr->evq);
    }
    if (chptr->transmit_conn != gv->data_conn_uc)
    {
      ddsi_conn_free (chptr->transmit_conn);
    }
    chptr = chptr->next;
  }
#endif

  ddsrt_thread_pool_free (gv->thread_pool);

  (void) joinleave_spdp_defmcip (gv, 0);

  ddsi_conn_free (gv->disc_conn_mc);
  if (gv->data_conn_mc != gv->disc_conn_mc)
    ddsi_conn_free (gv->data_conn_mc);
  if (gv->disc_conn_uc != gv->disc_conn_mc)
    ddsi_conn_free (gv->disc_conn_uc);
  if (gv->data_conn_uc != gv->disc_conn_uc)
    ddsi_conn_free (gv->data_conn_uc);

  /* Not freeing gv->tev_conn: it aliases data_conn_uc */

  free_group_membership(gv->mship);
  ddsi_tran_factories_fini (gv);

  if (gv->pcap_fp)
  {
    ddsrt_mutex_destroy (&gv->pcap_lock);
    fclose (gv->pcap_fp);
  }

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
  for (struct config_networkpartition_listelem *np = gv->config.networkPartitions; np; np = np->next)
    unref_addrset (np->as);
#endif
  unref_addrset (gv->as_disc);
  unref_addrset (gv->as_disc_group);

  /* Must delay freeing of rbufpools until after *all* references have
     been dropped, which only happens once all receive threads have
     stopped, defrags and reorders have been freed, and all delivery
     queues been drained.  I.e., until very late in the game. */
  for (uint32_t i = 0; i < gv->n_recv_threads; i++)
  {
    if (gv->recv_threads[i].arg.mode == RTM_MANY)
      os_sockWaitsetFree (gv->recv_threads[i].arg.u.many.ws);
    nn_rbufpool_free (gv->recv_threads[i].arg.rbpool);
  }

  ddsi_tkmap_free (gv->m_tkmap);

  ephash_free (gv->guid_hash);
  gv->guid_hash = NULL;
  deleted_participants_admin_free (gv->deleted_participants);
  lease_management_term (gv);
  ddsrt_mutex_destroy (&gv->participant_set_lock);
  ddsrt_cond_destroy (&gv->participant_set_cond);
  free_special_topics (gv);

#ifdef DDSI_INCLUDE_SECURITY
  nn_xqos_fini (&gv->builtin_stateless_xqos_wr);
  nn_xqos_fini (&gv->builtin_stateless_xqos_rd);
  nn_xqos_fini (&gv->builtin_volatile_xqos_wr);
  nn_xqos_fini (&gv->builtin_volatile_xqos_rd);
  
  q_omg_security_deinit( &gv->security_context);
#endif
  nn_xqos_fini (&gv->builtin_endpoint_xqos_wr);
  nn_xqos_fini (&gv->builtin_endpoint_xqos_rd);
  nn_xqos_fini (&gv->spdp_endpoint_xqos);
  nn_xqos_fini (&gv->default_xqos_pub);
  nn_xqos_fini (&gv->default_xqos_sub);
  nn_xqos_fini (&gv->default_xqos_tp);
  nn_xqos_fini (&gv->default_xqos_wr_nad);
  nn_xqos_fini (&gv->default_xqos_wr);
  nn_xqos_fini (&gv->default_xqos_rd);
  nn_plist_fini (&gv->default_local_plist_pp);
  nn_plist_fini (&gv->default_plist_pp);

  ddsrt_mutex_destroy (&gv->lock);

  while (gv->recvips)
  {
    struct config_in_addr_node *n = gv->recvips;
    /* The compiler doesn't realize that n->next is always initialized. */
    DDSRT_WARNING_MSVC_OFF(6001);
    gv->recvips = n->next;
    DDSRT_WARNING_MSVC_ON(6001);
    ddsrt_free (n);
  }

  for (int i = 0; i < (int) gv->n_interfaces; i++)
    ddsrt_free (gv->interfaces[i].name);

  ddsi_serdatapool_free (gv->serpool);
  nn_xmsgpool_free (gv->xmsgpool);
  GVLOG (DDS_LC_CONFIG, "Finis.\n");
}
