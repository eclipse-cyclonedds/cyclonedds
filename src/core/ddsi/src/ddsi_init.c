// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <ctype.h>
#include <stddef.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/time.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/avl.h"

#include "dds/ddsi/ddsi_init.h"
#include "dds/ddsi/ddsi_log.h"
#include "dds/ddsi/ddsi_unused.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_feature_check.h"
#include "dds/ddsi/ddsi_threadmon.h"
#include "dds/ddsi/ddsi_tkmap.h"
#include "dds/ddsi/ddsi_iid.h"
#include "ddsi__protocol.h"
#include "ddsi__misc.h"
#include "ddsi__config_impl.h"
#include "ddsi__bitset.h"
#include "ddsi__xevent.h"
#include "ddsi__addrset.h"
#include "ddsi__discovery.h"
#include "ddsi__radmin.h"
#include "ddsi__thread.h"
#include "ddsi__entity_index.h"
#include "ddsi__lease.h"
#include "ddsi__entity.h"
#include "ddsi__participant.h"
#include "ddsi__ownip.h"
#include "ddsi__xmsg.h"
#include "ddsi__receive.h"
#include "ddsi__pcap.h"
#include "ddsi__debmon.h"
#include "ddsi__pmd.h"
#include "ddsi__typelookup.h"
#include "ddsi__tran.h"
#include "ddsi__udp.h"
#include "ddsi__tcp.h"
#include "ddsi__raweth.h"
#include "ddsi__vnet.h"
#include "ddsi__mcgroup.h"
#include "ddsi__nwpart.h"
#include "ddsi__serdata_cdr.h"
#include "ddsi__serdata_pserop.h"
#include "ddsi__serdata_plist.h"
#include "ddsi__security_omg.h"
#include "ddsi__security_msg.h"
#include "ddsi__endpoint.h"
#include "ddsi__gc.h"
#include "ddsi__plist.h"
#include "ddsi__portmapping.h"
#include "ddsi__proxy_participant.h"
#include "ddsi__topic.h"
#include "ddsi__typelib.h"
#include "ddsi__vendor.h"
#include "ddsi__sockwaitset.h"

#include "dds__whc.h"
#include "dds/cdr/dds_cdrstream.h"

#ifdef DDS_HAS_SHM
#include "dds/ddsi/ddsi_shm_transport.h"
#include "dds/ddsrt/io.h"
#include "iceoryx_binding_c/runtime.h"
#endif

static void add_peer_addresses (const struct ddsi_domaingv *gv, struct ddsi_addrset *as, const struct ddsi_config_peer_listelem *list)
{
  while (list)
  {
    ddsi_add_addresses_to_addrset (gv, as, list->peer, -1, "add_peer_addresses", 0);
    list = list->next;
  }
}

enum make_uc_sockets_ret {
  MUSRET_SUCCESS,       /* unicast socket(s) created */
  MUSRET_INVALID_PORTS, /* specified port numbers are invalid */
  MUSRET_PORTS_IN_USE,  /* ports were in use, keep trying */
  MUSRET_ERROR          /* generic error, no use continuing */
};

static enum make_uc_sockets_ret make_uc_sockets (struct ddsi_domaingv *gv, uint32_t * pdisc, uint32_t * pdata, int ppid)
{
  dds_return_t rc;

  if (gv->config.many_sockets_mode == DDSI_MSM_NO_UNICAST)
  {
    assert (ppid == DDSI_PARTICIPANT_INDEX_NONE);
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

  const struct ddsi_tran_qos qos = { .m_purpose = DDSI_TRAN_QOS_RECV_UC, .m_diffserv = 0, .m_interface = NULL };
  rc = ddsi_factory_create_conn (&gv->disc_conn_uc, gv->m_factory, *pdisc, &qos);
  if (rc != DDS_RETCODE_OK)
    goto fail_disc;

  if (*pdata == 0 || *pdata == *pdisc)
    gv->data_conn_uc = gv->disc_conn_uc;
  else
  {
    rc = ddsi_factory_create_conn (&gv->data_conn_uc, gv->m_factory, *pdata, &qos);
    if (rc != DDS_RETCODE_OK)
      goto fail_data;
  }
  ddsi_conn_locator (gv->disc_conn_uc, &gv->loc_meta_uc);
  ddsi_conn_locator (gv->data_conn_uc, &gv->loc_default_uc);
  *pdisc = gv->loc_meta_uc.port;
  *pdata = gv->loc_default_uc.port;
  return MUSRET_SUCCESS;

fail_data:
  ddsi_conn_free (gv->disc_conn_uc);
  gv->disc_conn_uc = NULL;
fail_disc:
  if (rc == DDS_RETCODE_PRECONDITION_NOT_MET)
    return MUSRET_PORTS_IN_USE;
  return MUSRET_ERROR;
}

static void make_builtin_endpoint_xqos (dds_qos_t *q, const dds_qos_t *template)
{
  ddsi_xqos_copy (q, template);
  q->reliability.kind = DDS_RELIABILITY_RELIABLE;
  q->reliability.max_blocking_time = DDS_MSECS (100);
  q->durability.kind = DDS_DURABILITY_TRANSIENT_LOCAL;
}

#if defined (DDS_HAS_TYPE_DISCOVERY) || defined (DDS_HAS_SECURITY)
static void make_builtin_volatile_endpoint_xqos (dds_qos_t *q, const dds_qos_t *template)
{
  ddsi_xqos_copy (q, template);
  q->reliability.kind = DDS_RELIABILITY_RELIABLE;
  q->reliability.max_blocking_time = DDS_MSECS (100);
  q->durability.kind = DDS_DURABILITY_VOLATILE;
  q->history.kind = DDS_HISTORY_KEEP_ALL;
}
#endif

static int set_recvips (struct ddsi_domaingv *gv)
{
  gv->recvips = NULL;

  if (gv->config.networkRecvAddressStrings)
  {
    if (ddsrt_strcasecmp (gv->config.networkRecvAddressStrings[0], "all") == 0)
    {
      if (gv->using_link_local_intf)
      {
        GVWARNING ("CycloneDDS/Domain/General/MulticastRecvNetworkInterfaceAddresses: using 'preferred' instead of 'all' because of link-local address\n");
        gv->recvips_mode = DDSI_RECVIPS_MODE_PREFERRED;
      }
      else
      {
        gv->recvips_mode = DDSI_RECVIPS_MODE_ALL;
      }
    }
    else if (ddsrt_strcasecmp (gv->config.networkRecvAddressStrings[0], "any") == 0)
    {
      if (gv->using_link_local_intf)
      {
        GVERROR ("CycloneDDS/Domain/General/MulticastRecvNetworkInterfaceAddresses: 'any' is unsupported in combination with a link-local address\n");
        return -1;
      }
      gv->recvips_mode = DDSI_RECVIPS_MODE_ANY;
    }
    else if (ddsrt_strcasecmp (gv->config.networkRecvAddressStrings[0], "preferred") == 0)
    {
      gv->recvips_mode = DDSI_RECVIPS_MODE_PREFERRED;
    }
    else if (ddsrt_strcasecmp (gv->config.networkRecvAddressStrings[0], "none") == 0)
    {
      gv->recvips_mode = DDSI_RECVIPS_MODE_NONE;
    }
    else if (gv->using_link_local_intf)
    {
      /* If the configuration explicitly includes the selected
       interface, treat it as "preferred", else as "none"; warn if
       interfaces other than the selected one are included. */
      int i, have_selected = 0, have_others = 0;
      assert (gv->n_interfaces == 1);
      for (i = 0; gv->config.networkRecvAddressStrings[i] != NULL; i++)
      {
        ddsi_locator_t loc;
        if (ddsi_locator_from_string(gv, &loc, gv->config.networkRecvAddressStrings[i], gv->m_factory) != AFSR_OK)
        {
          GVERROR ("%s: not a valid address in CycloneDDS/Domain/General/MulticastRecvNetworkInterfaceAddresses\n", gv->config.networkRecvAddressStrings[i]);
          return -1;
        }
        if (ddsi_compare_locators(&loc, &gv->interfaces[0].loc) == 0)
          have_selected = 1;
        else
          have_others = 1;
      }
      gv->recvips_mode = have_selected ? DDSI_RECVIPS_MODE_PREFERRED : DDSI_RECVIPS_MODE_NONE;
      if (have_others)
      {
        GVWARNING ("CycloneDDS/Domain/General/MulticastRecvNetworkInterfaceAddresses: using 'preferred' because of IPv6 local address\n");
      }
    }
    else
    {
      struct ddsi_config_in_addr_node **recvnode = &gv->recvips;
      int i, j;
      gv->recvips_mode = DDSI_RECVIPS_MODE_SOME;
      for (i = 0; gv->config.networkRecvAddressStrings[i] != NULL; i++)
      {
        ddsi_locator_t loc;
        if (ddsi_locator_from_string(gv, &loc, gv->config.networkRecvAddressStrings[i], gv->m_factory) != AFSR_OK)
        {
          GVERROR ("%s: not a valid address in CycloneDDS/Domain/General/MulticastRecvNetworkInterfaceAddresses\n", gv->config.networkRecvAddressStrings[i]);
          return -1;
        }
        for (j = 0; j < gv->n_interfaces; j++)
        {
          if (ddsi_compare_locators(&loc, &gv->interfaces[j].loc) == 0)
            break;
        }
        if (j == gv->n_interfaces)
        {
          GVERROR ("No interface bound to requested address '%s'\n", gv->config.networkRecvAddressStrings[i]);
          return -1;
        }
        *recvnode = ddsrt_malloc (sizeof (struct ddsi_config_in_addr_node));
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
static int string_to_default_locator (const struct ddsi_domaingv *gv, ddsi_locator_t *loc, const char *string, uint32_t port, int mc, const char *tag)
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
  if (port != 0 && !ddsi_is_unspec_locator(loc))
    loc->port = port;
  else
    loc->port = DDSI_LOCATOR_PORT_INVALID;
  assert (mc == -1 || mc == 0 || mc == 1);
  if (mc >= 0)
  {
    const char *rel = mc ? "must" : "may not";
    const int ismc = ddsi_is_unspec_locator (loc) || ddsi_is_mcaddr (gv, loc);
    if (mc != ismc)
    {
      GVERROR ("%s: %s %s be the unspecified address or a multicast address\n", string, tag, rel);
      return -1;
    }
  }
  return 1;
}

static int set_spdp_address (struct ddsi_domaingv *gv)
{
  const uint32_t port = ddsi_get_port (&gv->config, DDSI_PORT_MULTI_DISC, 0);
  int rc = 0;
  /* FIXME: FIXME: FIXME: */
  gv->loc_spdp_mc.kind = DDSI_LOCATOR_KIND_INVALID;
  if (strcmp (gv->config.spdpMulticastAddressString, "239.255.0.1") != 0)
  {
    if ((rc = string_to_default_locator (gv, &gv->loc_spdp_mc, gv->config.spdpMulticastAddressString, port, 1, "SPDP address")) < 0)
      return rc;
  }
  if (rc == 0 && gv->m_factory->m_default_spdp_address)
  {
    rc = string_to_default_locator (gv, &gv->loc_spdp_mc, gv->m_factory->m_default_spdp_address, port, 1, "SPDP address");
    assert (rc > 0);
  }
#ifdef DDS_HAS_SSM
  if (gv->loc_spdp_mc.kind != DDSI_LOCATOR_KIND_INVALID && ddsi_is_ssm_mcaddr (gv, &gv->loc_spdp_mc))
  {
    GVERROR ("%s: SPDP address may not be an SSM address\n", gv->config.spdpMulticastAddressString);
    return -1;
  }
#endif
  return 0;
}

static int set_default_mc_address (struct ddsi_domaingv *gv)
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

static int set_ext_address_and_mask (struct ddsi_domaingv *gv)
{
  ddsi_locator_t loc;
  int rc;

  if (!gv->config.externalAddressString)
    ddsi_set_unspec_locator (&loc);
  else if ((rc = string_to_default_locator (gv, &loc, gv->config.externalAddressString, 0, 0, "external address")) < 0)
    return rc;
  else if (rc == 0)
  {
    GVWARNING ("Ignoring ExternalNetworkAddress %s\n", gv->config.externalAddressString);
    ddsi_set_unspec_locator (&loc);
  }

  if (!gv->config.externalMaskString || strcmp (gv->config.externalMaskString, "0.0.0.0") == 0)
  {
    memset(&gv->extmask.address, 0, sizeof(gv->extmask.address));
    gv->extmask.kind = DDSI_LOCATOR_KIND_INVALID;
    gv->extmask.port = DDSI_LOCATOR_PORT_INVALID;
  }
  else if (gv->config.transport_selector != DDSI_TRANS_UDP)
  {
    GVERROR ("external network masks only supported in IPv4 mode\n");
    return -1;
  }
  else if (gv->n_interfaces != 1)
  {
    GVERROR ("external network masks only supported with a single interface\n");
    return -1;
  }
  else
  {
    if ((rc = string_to_default_locator (gv, &gv->extmask, gv->config.externalMaskString, 0, -1, "external mask")) < 0)
      return rc;
  }

  if (!ddsi_is_unspec_locator (&loc))
  {
    int non_loopback_intf = -1;
    for (int i = 0; i < gv->n_interfaces; i++)
    {
      if (gv->interfaces[i].loopback || gv->interfaces[i].link_local)
        continue;
      else if (non_loopback_intf == -1)
        non_loopback_intf = i;
      else
      {
        non_loopback_intf = -1;
        break;
      }
    }
    if (non_loopback_intf == -1)
    {
      GVERROR ("external network address specification only supported if there is a unique non-loopback interface\n");
      return -1;
    }
    if (gv->interfaces[non_loopback_intf].loc.kind != loc.kind)
    {
      GVERROR ("external network address kind does not match unique non-loopback interface\n");
      return -1;
    }
    gv->interfaces[non_loopback_intf].extloc = loc;
  }
  return 0;
}

static int check_thread_properties (const struct ddsi_domaingv *gv)
{
  static const char *fixed[] = { "recv", "recvUC", "recvMC", "tev", "gc", "lease", "dq.builtins", "xmit.user", "dq.user", "debmon", "fsm", NULL };
  const struct ddsi_config_thread_properties_listelem *e;
  int ok = 1, i;
  for (e = gv->config.thread_properties; e; e = e->next)
  {
    for (i = 0; fixed[i]; i++)
      if (strcmp (fixed[i], e->name) == 0)
        break;
    if (fixed[i] == NULL)
    {
      DDS_ILOG (DDS_LC_ERROR, gv->config.domainId, "config: CycloneDDS/Domain/Threads/Thread[@name=\"%s\"]: unknown thread\n", e->name);
      ok = 0;
    }
  }
  return ok;
}

static int ddsi_config_open_trace (struct ddsi_domaingv *gv)
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

int ddsi_config_prep (struct ddsi_domaingv *gv, struct ddsi_cfgst *cfgst)
{
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
    if (gv->config.participantIndex >= 0 || gv->config.participantIndex == DDSI_PARTICIPANT_INDEX_NONE)
      ppidx = gv->config.participantIndex;
    else if (gv->config.participantIndex == DDSI_PARTICIPANT_INDEX_AUTO)
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

  if (gv->config.besmode == DDSI_BESMODE_MINIMAL && gv->config.many_sockets_mode == DDSI_MSM_MANY_UNICAST)
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
  if (gv->config.many_sockets_mode == DDSI_MSM_MANY_UNICAST)
  {
    if (gv->config.max_participants == 0)
      gv->config.max_participants = 100;
  }
  else if (gv->config.many_sockets_mode == DDSI_MSM_NO_UNICAST)
  {
    if (gv->config.participantIndex != DDSI_PARTICIPANT_INDEX_NONE)
    {
      DDS_ILOG (DDS_LC_ERROR, gv->config.domainId, "ParticipantIndex and ManySocketsMode are incompatible\n");
      goto err_config_late_error;
    }
  }
  if (gv->config.max_queued_rexmit_bytes == 0)
  {
    gv->config.max_queued_rexmit_bytes = 2147483647u;
  }

  /* Verify thread properties refer to defined threads */
  if (!check_thread_properties (gv))
  {
    goto err_config_late_error;
  }

  /* Open tracing file after all possible config errors have been printed */
  if (!ddsi_config_open_trace (gv))
  {
    goto err_config_late_error;
  }

  /* Now the per-thread-log-buffers are set up, so print the configuration.  Note that configurations
     passed in as initializers don't have associated parsing state and source information.

     After this there is no value to the source information for the various configuration elements, so
     free those. */
  if (cfgst != NULL)
  {
    ddsi_config_print_cfgst (cfgst, &gv->logconfig);
    ddsi_config_free_source_info (cfgst);
  }
  else
  {
    ddsi_config_print_rawconfig (&gv->config, &gv->logconfig);
  }
  return 0;

err_config_late_error:
  return -1;
}

struct joinleave_spdp_defmcip_helper_arg {
  struct ddsi_domaingv *gv;
  int errcount;
  int dojoin;
};

static void joinleave_spdp_defmcip_helper (const ddsi_xlocator_t *loc, void *varg)
{
  struct joinleave_spdp_defmcip_helper_arg *arg = varg;
  if (!ddsi_is_mcaddr (arg->gv, &loc->c))
    return;
#ifdef DDS_HAS_SSM
  /* Can't join SSM until we actually have a source */
  if (ddsi_is_ssm_mcaddr (arg->gv, &loc->c))
    return;
#endif
  if (arg->dojoin) {
    if (ddsi_join_mc (arg->gv, arg->gv->mship, arg->gv->disc_conn_mc, NULL, &loc->c) < 0 ||
        ddsi_join_mc (arg->gv, arg->gv->mship, arg->gv->data_conn_mc, NULL, &loc->c) < 0)
      arg->errcount++;
  } else {
    if (ddsi_leave_mc (arg->gv, arg->gv->mship, arg->gv->disc_conn_mc, NULL, &loc->c) < 0 ||
        ddsi_leave_mc (arg->gv, arg->gv->mship, arg->gv->data_conn_mc, NULL, &loc->c) < 0)
      arg->errcount++;
  }
}

static int joinleave_spdp_defmcip (struct ddsi_domaingv *gv, int dojoin)
{
  /* Addrset provides an easy way to filter out duplicates */
  struct joinleave_spdp_defmcip_helper_arg arg;
  struct ddsi_addrset *as = ddsi_new_addrset ();
  arg.gv = gv;
  arg.errcount = 0;
  arg.dojoin = dojoin;
  if (gv->config.allowMulticast & DDSI_AMC_SPDP)
    ddsi_add_locator_to_addrset (gv, as, &gv->loc_spdp_mc);
  if (gv->config.allowMulticast & ~DDSI_AMC_SPDP)
    ddsi_add_locator_to_addrset (gv, as, &gv->loc_default_mc);
  ddsi_addrset_forall (as, joinleave_spdp_defmcip_helper, &arg);
  ddsi_unref_addrset (as);
  if (arg.errcount)
  {
    GVERROR ("rtps_init: failed to join multicast groups for domain %"PRIu32" participant %d\n", gv->config.domainId, gv->config.participantIndex);
    return -1;
  }
  return 0;
}

static int create_multicast_sockets (struct ddsi_domaingv *gv)
{
  const struct ddsi_tran_qos qos = { .m_purpose = DDSI_TRAN_QOS_RECV_MC, .m_diffserv = 0, .m_interface = NULL };
  struct ddsi_tran_conn * disc, * data;
  uint32_t port;

  port = ddsi_get_port (&gv->config, DDSI_PORT_MULTI_DISC, 0);
  if (!ddsi_is_valid_port (gv->m_factory, port))
  {
    GVERROR ("Failed to create discovery multicast socket for domain %"PRIu32": resulting port number (%"PRIu32") is out of range\n",
             gv->config.extDomainId.value, port);
    goto err_disc;
  }
  if (ddsi_factory_create_conn (&disc, gv->m_factory, port, &qos) != DDS_RETCODE_OK)
    goto err_disc;
  if (gv->config.many_sockets_mode == DDSI_MSM_NO_UNICAST)
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
    if (ddsi_factory_create_conn (&data, gv->m_factory, port, &qos) != DDS_RETCODE_OK)
      goto err_data;
  }

  gv->disc_conn_mc = disc;
  gv->data_conn_mc = data;
  GVLOG (DDS_LC_CONFIG, "Multicast Ports: discovery %"PRIu32" data %"PRIu32" \n",
         ddsi_conn_port (gv->disc_conn_mc), ddsi_conn_port (gv->data_conn_mc));
  return 1;

err_data:
  ddsi_conn_free (disc);
err_disc:
  return 0;
}

static void ddsi_term_prep (struct ddsi_domaingv *gv)
{
  /* Stop all I/O */
  ddsrt_mutex_lock (&gv->lock);
  if (ddsrt_atomic_ld32 (&gv->rtps_keepgoing))
  {
    ddsrt_atomic_st32 (&gv->rtps_keepgoing, 0); /* so threads will stop once they get round to checking */
    ddsrt_atomic_fence ();
    /* can't wake up throttle_writer, currently, but it'll check every few seconds */
    ddsi_trigger_recv_threads (gv);
  }
  ddsrt_mutex_unlock (&gv->lock);
}

struct wait_for_receive_threads_helper_arg {
  unsigned count;
};

static void wait_for_receive_threads_helper (struct ddsi_domaingv *gv, struct ddsi_xevent *xev, struct ddsi_xpack *xp, void *varg, ddsrt_mtime_t tnow)
{
  struct wait_for_receive_threads_helper_arg * const arg = varg;
  (void) xp;
  if (arg->count++ == gv->config.recv_thread_stop_maxretries)
    abort ();
  ddsi_trigger_recv_threads (gv);
  (void) ddsi_resched_xevent_if_earlier (xev, ddsrt_mtime_add_duration (tnow, DDS_SECS (1)));
}

static void wait_for_receive_threads (struct ddsi_domaingv *gv)
{
  struct ddsi_xevent *trigev;
  struct wait_for_receive_threads_helper_arg cbarg = { .count = 0 };
  if ((trigev = ddsi_qxev_callback (gv->xevents, ddsrt_mtime_add_duration (ddsrt_time_monotonic (), DDS_SECS (1)), wait_for_receive_threads_helper, &cbarg, sizeof (cbarg), true)) == NULL)
  {
    /* retrying is to deal a packet geting lost because the socket buffer is full or because the
       macOS firewall (and perhaps others) likes to ask if the process is allowed to receive data,
       dropping the packets until the user approves. */
    GVWARNING ("wait_for_receive_threads: failed to schedule periodic triggering of the receive threads to deal with packet loss\n");
  }
  for (uint32_t i = 0; i < gv->n_recv_threads; i++)
  {
    if (gv->recv_threads[i].thrst)
    {
      ddsi_join_thread (gv->recv_threads[i].thrst);
      /* setting .thrst to NULL helps in sanity checking */
      gv->recv_threads[i].thrst = NULL;
    }
  }
  if (trigev)
  {
    ddsi_delete_xevent (trigev);
  }
}

static struct ddsi_sertype *make_special_type_pserop (const char *typename, size_t memsize, size_t nops, const enum ddsi_pserop *ops, size_t nops_key, const enum ddsi_pserop *ops_key)
{
  struct ddsi_sertype_pserop *st = ddsrt_malloc (sizeof (*st));
  memset (st, 0, sizeof (*st));
  ddsi_sertype_init (&st->c, typename, &ddsi_sertype_ops_pserop, &ddsi_serdata_ops_pserop, nops_key == 0);
  st->encoding_format = DDSI_RTPS_CDR_ENC_FORMAT_PLAIN;
  st->memsize = memsize;
  st->nops = nops;
  st->ops = ops;
  st->nops_key = nops_key;
  st->ops_key = ops_key;
  return (struct ddsi_sertype *) st;
}

static struct ddsi_sertype *make_special_type_plist (const char *typename, ddsi_parameterid_t keyparam)
{
  struct ddsi_sertype_plist *st = ddsrt_malloc (sizeof (*st));
  memset (st, 0, sizeof (*st));
  ddsi_sertype_init (&st->c, typename, &ddsi_sertype_ops_plist, &ddsi_serdata_ops_plist, false);
  st->encoding_format = DDSI_RTPS_CDR_ENC_FORMAT_PL;
  st->keyparam = keyparam;
  return (struct ddsi_sertype *) st;
}

#ifdef DDS_HAS_TYPE_DISCOVERY
/* Creates a sertype that is used for built-in type lookup readers and writers, which are using XCDR2
   because the request/response messages contain mutable and appendable types. */
static struct ddsi_sertype *make_special_type_cdrstream (const struct ddsi_domaingv *gv, const dds_topic_descriptor_t *desc)
{
  struct ddsi_sertype_cdr *st = ddsrt_malloc (sizeof (*st));
  dds_return_t ret = ddsi_sertype_cdr_init (gv, st, desc);
  assert (ret == DDS_RETCODE_OK);
  (void) ret;
  return (struct ddsi_sertype *) st;
}
#endif


static void free_special_types (struct ddsi_domaingv *gv)
{
#ifdef DDS_HAS_SECURITY
  ddsi_sertype_unref (gv->pgm_volatile_type);
  ddsi_sertype_unref (gv->pgm_stateless_type);
  ddsi_sertype_unref (gv->pmd_secure_type);
  ddsi_sertype_unref (gv->spdp_secure_type);
  ddsi_sertype_unref (gv->sedp_reader_secure_type);
  ddsi_sertype_unref (gv->sedp_writer_secure_type);
#endif
#ifdef DDS_HAS_TOPIC_DISCOVERY
  if (gv->config.enable_topic_discovery_endpoints)
    ddsi_sertype_unref (gv->sedp_topic_type);
#endif
#ifdef DDS_HAS_TYPE_DISCOVERY
  ddsi_sertype_unref (gv->tl_svc_request_type);
  ddsi_sertype_unref (gv->tl_svc_reply_type);
#endif
  ddsi_sertype_unref (gv->pmd_type);
  ddsi_sertype_unref (gv->spdp_type);
  ddsi_sertype_unref (gv->sedp_reader_type);
  ddsi_sertype_unref (gv->sedp_writer_type);
}

static void make_special_types (struct ddsi_domaingv *gv)
{
  gv->spdp_type = make_special_type_plist ("ParticipantBuiltinTopicData", DDSI_PID_PARTICIPANT_GUID);
  gv->sedp_reader_type = make_special_type_plist ("SubscriptionBuiltinTopicData", DDSI_PID_ENDPOINT_GUID);
  gv->sedp_writer_type = make_special_type_plist ("PublicationBuiltinTopicData", DDSI_PID_ENDPOINT_GUID);
  gv->pmd_type = make_special_type_pserop ("ParticipantMessageData", sizeof (ddsi_participant_message_data_t), ddsi_participant_message_data_nops, ddsi_participant_message_data_ops, ddsi_participant_message_data_nops_key, ddsi_participant_message_data_ops_key);
#ifdef DDS_HAS_TYPE_DISCOVERY
  gv->tl_svc_request_type = make_special_type_cdrstream (gv, &DDS_Builtin_TypeLookup_Request_desc);
  gv->tl_svc_reply_type = make_special_type_cdrstream (gv, &DDS_Builtin_TypeLookup_Reply_desc);
#endif
#ifdef DDS_HAS_TOPIC_DISCOVERY
  if (gv->config.enable_topic_discovery_endpoints)
    gv->sedp_topic_type = make_special_type_plist ("TopicBuiltinTopicData", DDSI_PID_CYCLONE_TOPIC_GUID);
#endif
#ifdef DDS_HAS_SECURITY
  gv->spdp_secure_type = make_special_type_plist ("ParticipantBuiltinTopicDataSecure", DDSI_PID_PARTICIPANT_GUID);
  gv->sedp_reader_secure_type = make_special_type_plist ("SubscriptionBuiltinTopicDataSecure", DDSI_PID_ENDPOINT_GUID);
  gv->sedp_writer_secure_type = make_special_type_plist ("PublicationBuiltinTopicDataSecure", DDSI_PID_ENDPOINT_GUID);
  gv->pmd_secure_type = make_special_type_pserop ("ParticipantMessageDataSecure", sizeof (ddsi_participant_message_data_t), ddsi_participant_message_data_nops, ddsi_participant_message_data_ops, ddsi_participant_message_data_nops_key, ddsi_participant_message_data_ops_key);
  gv->pgm_stateless_type = make_special_type_pserop ("ParticipantStatelessMessage", sizeof (ddsi_participant_generic_message_t), ddsi_pserop_participant_generic_message_nops, ddsi_pserop_participant_generic_message, 0, NULL);
  gv->pgm_volatile_type = make_special_type_pserop ("ParticipantVolatileMessageSecure", sizeof (ddsi_participant_generic_message_t), ddsi_pserop_participant_generic_message_nops, ddsi_pserop_participant_generic_message, 0, NULL);
#endif

  ddsrt_mutex_lock (&gv->sertypes_lock);
  ddsi_sertype_register_locked (gv, gv->spdp_type);
  ddsi_sertype_register_locked (gv, gv->sedp_reader_type);
  ddsi_sertype_register_locked (gv, gv->sedp_writer_type);
  ddsi_sertype_register_locked (gv, gv->pmd_type);
#ifdef DDS_HAS_TYPE_DISCOVERY
  ddsi_sertype_register_locked (gv, gv->tl_svc_request_type);
  ddsi_sertype_register_locked (gv, gv->tl_svc_reply_type);
#endif
#ifdef DDS_HAS_TOPIC_DISCOVERY
  if (gv->config.enable_topic_discovery_endpoints)
    ddsi_sertype_register_locked (gv, gv->sedp_topic_type);
#endif
#ifdef DDS_HAS_SECURITY
  ddsi_sertype_register_locked (gv, gv->spdp_secure_type);
  ddsi_sertype_register_locked (gv, gv->sedp_reader_secure_type);
  ddsi_sertype_register_locked (gv, gv->sedp_writer_secure_type);
  ddsi_sertype_register_locked (gv, gv->pmd_secure_type);
  ddsi_sertype_register_locked (gv, gv->pgm_stateless_type);
  ddsi_sertype_register_locked (gv, gv->pgm_volatile_type);
#endif
  ddsrt_mutex_unlock (&gv->sertypes_lock);

  /* register increments refcount (which is reasonable), but at some point
     one needs to get rid of that reference */
  free_special_types (gv);
}

static bool use_multiple_receive_threads (const struct ddsi_config *cfg)
{
  /* Under some unknown circumstances Windows (at least Windows 10) exhibits
     the interesting behaviour of losing its ability to let us send packets
     to our own sockets. When that happens, dedicated receive threads can no
     longer be stopped and Cyclone hangs in shutdown.  So until someone
     figures out why this happens, it is probably best have a different
     default on Windows. */
#if _WIN32
  const bool def = false;
#else
  const bool def = true;
#endif
  switch (cfg->multiple_recv_threads)
  {
    case DDSI_BOOLDEF_FALSE:
      return false;
    case DDSI_BOOLDEF_TRUE:
      return true;
    case DDSI_BOOLDEF_DEFAULT:
      return def;
  }
  assert (0);
  return false;
}

static int setup_and_start_recv_threads (struct ddsi_domaingv *gv)
{
  const bool multi_recv_thr = use_multiple_receive_threads (&gv->config);

  for (uint32_t i = 0; i < MAX_RECV_THREADS; i++)
  {
    gv->recv_threads[i].thrst = NULL;
    gv->recv_threads[i].arg.mode = DDSI_RTM_SINGLE;
    gv->recv_threads[i].arg.rbpool = NULL;
    gv->recv_threads[i].arg.gv = gv;
    gv->recv_threads[i].arg.u.single.loc = NULL;
    gv->recv_threads[i].arg.u.single.conn = NULL;
  }

  /* First thread always uses a waitset and gobbles up all sockets not handled by dedicated threads - FIXME: DDSI_MSM_NO_UNICAST mode with UDP probably doesn't even need this one to use a waitset */
  gv->n_recv_threads = 1;
  gv->recv_threads[0].name = "recv";
  gv->recv_threads[0].arg.mode = DDSI_RTM_MANY;
  if (gv->m_factory->m_connless && gv->config.many_sockets_mode != DDSI_MSM_NO_UNICAST && multi_recv_thr)
  {
    if (ddsi_is_mcaddr (gv, &gv->loc_default_mc) && !ddsi_is_ssm_mcaddr (gv, &gv->loc_default_mc) && (gv->config.allowMulticast & DDSI_AMC_ASM))
    {
      /* Multicast enabled, but it isn't an SSM address => handle data multicasts on a separate thread (the trouble with SSM addresses is that we only join matching writers, which our own sockets typically would not be) */
      gv->recv_threads[gv->n_recv_threads].name = "recvMC";
      gv->recv_threads[gv->n_recv_threads].arg.mode = DDSI_RTM_SINGLE;
      gv->recv_threads[gv->n_recv_threads].arg.u.single.conn = gv->data_conn_mc;
      gv->recv_threads[gv->n_recv_threads].arg.u.single.loc = &gv->loc_default_mc;
      ddsi_conn_disable_multiplexing (gv->data_conn_mc);
      gv->n_recv_threads++;
    }
    if (gv->config.many_sockets_mode == DDSI_MSM_SINGLE_UNICAST)
    {
      /* No per-participant sockets => handle data unicasts on a separate thread as well */
      gv->recv_threads[gv->n_recv_threads].name = "recvUC";
      gv->recv_threads[gv->n_recv_threads].arg.mode = DDSI_RTM_SINGLE;
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
    if ((gv->recv_threads[i].arg.rbpool = ddsi_rbufpool_new (&gv->logconfig, gv->config.rbuf_size, gv->config.rmsg_chunk_size)) == NULL)
    {
      GVERROR ("rtps_init: can't allocate receive buffer pool for thread %s\n", gv->recv_threads[i].name);
      goto fail;
    }
    if (gv->recv_threads[i].arg.mode == DDSI_RTM_MANY)
    {
      if ((gv->recv_threads[i].arg.u.many.ws = ddsi_sock_waitset_new ()) == NULL)
      {
        GVERROR ("rtps_init: can't allocate sock waitset for thread %s\n", gv->recv_threads[i].name);
        goto fail;
      }
    }
    if (ddsi_create_thread (&gv->recv_threads[i].thrst, gv, gv->recv_threads[i].name, ddsi_recv_thread, &gv->recv_threads[i].arg) != DDS_RETCODE_OK)
    {
      GVERROR ("rtps_init: failed to start thread %s\n", gv->recv_threads[i].name);
      goto fail;
    }
  }
  return 0;

fail:
  /* to trigger any threads we already started to stop - xevent thread has already been started */
  ddsi_term_prep (gv);
  wait_for_receive_threads (gv);
  for (uint32_t i = 0; i < gv->n_recv_threads; i++)
  {
    if (gv->recv_threads[i].arg.mode == DDSI_RTM_MANY && gv->recv_threads[i].arg.u.many.ws)
      ddsi_sock_waitset_free (gv->recv_threads[i].arg.u.many.ws);
    if (gv->recv_threads[i].arg.rbpool)
      ddsi_rbufpool_free (gv->recv_threads[i].arg.rbpool);
  }
  return -1;
}

static int ddsi_sertype_equal_wrap (const void *a, const void *b)
{
  return ddsi_sertype_equal (a, b);
}

static uint32_t ddsi_sertype_hash_wrap (const void *tp)
{
  return ddsi_sertype_hash (tp);
}

#ifdef DDS_HAS_TOPIC_DISCOVERY
static int topic_definition_equal_wrap (const void *tpd_a, const void *tpd_b)
{
  return ddsi_topic_definition_equal (tpd_a, tpd_b);
}
static uint32_t topic_definition_hash_wrap (const void *tpd)
{
  return ddsi_topic_definition_hash (tpd);
}
#endif /* DDS_HAS_TYPE_DISCOVERY */

static void reset_deaf_mute (struct ddsi_domaingv *gv, struct ddsi_xevent *xev, UNUSED_ARG (struct ddsi_xpack *xp), UNUSED_ARG (void *varg), UNUSED_ARG (ddsrt_mtime_t tnow))
{
  gv->deaf = 0;
  gv->mute = 0;
  GVLOGDISC ("DEAFMUTE auto-reset to [deaf, mute]=[%d, %d]\n", gv->deaf, gv->mute);
  ddsi_delete_xevent (xev);
}

void ddsi_set_deafmute (struct ddsi_domaingv *gv, bool deaf, bool mute, int64_t reset_after)
{
  gv->deaf = deaf;
  gv->mute = mute;
  GVLOGDISC (" DEAFMUTE set [deaf, mute]=[%d, %d]", gv->deaf, gv->mute);
  if (reset_after < DDS_INFINITY)
  {
    ddsrt_mtime_t when = ddsrt_mtime_add_duration (ddsrt_time_monotonic (), reset_after);
    GVTRACE (" reset after %"PRId64".%09u ns", reset_after / DDS_NSECS_IN_SEC, (unsigned) (reset_after % DDS_NSECS_IN_SEC));
    ddsi_qxev_callback (gv->xevents, when, reset_deaf_mute, NULL, 0, true);
  }
  GVLOGDISC ("\n");
}

static void free_conns (struct ddsi_domaingv *gv)
{
  // Depending on settings, various "conn"s can alias others, this makes sure we free each one only once
  // FIXME: perhaps store them in a table instead?
  struct ddsi_tran_conn * cs[4 + MAX_XMIT_CONNS] = { gv->disc_conn_mc, gv->data_conn_mc, gv->disc_conn_uc, gv->data_conn_uc };
  for (size_t i = 0; i < MAX_XMIT_CONNS; i++)
    cs[4 + i] = gv->xmit_conns[i];
  for (size_t i = 0; i < sizeof (cs) / sizeof (cs[0]); i++)
  {
    if (cs[i] == NULL)
      continue;
    for (size_t j = i + 1; j < sizeof (cs) / sizeof (cs[0]); j++)
      if (cs[i] == cs[j])
        cs[j] = NULL;
    ddsi_conn_free (cs[i]);
  }
}

#ifdef DDS_HAS_SHM
static int iceoryx_init (struct ddsi_domaingv *gv)
{
  shm_set_loglevel(gv->config.shm_log_lvl);

  char *sptr;
  ddsrt_asprintf (&sptr, "iceoryx_rt_%"PRIdPID"_%"PRId64, ddsrt_getpid (), gv->tstart.v);
  GVLOG (DDS_LC_SHM, "Current process name for iceoryx is %s\n", sptr);
  iox_runtime_init (sptr);
  free(sptr);

  // FIXME: this can be done more elegantly when properly supporting multiple transports
  if (ddsi_vnet_init (gv, "iceoryx", DDSI_LOCATOR_KIND_SHEM) < 0)
    return -1;
  ddsi_factory_find (gv, "iceoryx")->m_enable = true;

  if (gv->config.shm_locator && *gv->config.shm_locator)
  {
    enum ddsi_locator_from_string_result res;
    res = ddsi_locator_from_string (gv, &gv->loc_iceoryx_addr, gv->config.shm_locator, ddsi_factory_find (gv, "iceoryx"));
    switch (res)
    {
      case AFSR_OK:
        if (gv->loc_iceoryx_addr.kind != DDSI_LOCATOR_KIND_SHEM)
        {
          GVERROR ("invalid or unsupported SharedMemory/Locator: %s\n", gv->config.shm_locator);
          return -1;
        }
        DDSRT_STATIC_ASSERT (DDSI_LOCATOR_PORT_INVALID == 0);
        assert (gv->loc_iceoryx_addr.port == DDSI_LOCATOR_PORT_INVALID);
        break;
      case AFSR_INVALID:
      case AFSR_UNKNOWN:
      case AFSR_MISMATCH:
        GVERROR ("invalid or unsupported SharedMemory/Locator: %s\n", gv->config.shm_locator);
        return -1;
    }
  }
  else
  {
    int if_index;

    // Try to avoid loopback interfaces for getting a MAC address, but if all
    // we have are loopback interfaces, then it really doesn't matter.
    for (if_index = 0; if_index < gv->n_interfaces; if_index++)
    {
      if (!gv->interfaces[if_index].loopback)
        break;
    }
    if (if_index == gv->n_interfaces)
    {
      if_index = 0;
    }

    memset (gv->loc_iceoryx_addr.address, 0, sizeof (gv->loc_iceoryx_addr.address));
    if (ddsrt_eth_get_mac_addr (gv->interfaces[if_index].name, gv->loc_iceoryx_addr.address))
    {
      GVERROR ("Unable to get MAC address for iceoryx\n");
      return -1;
    }
    gv->loc_iceoryx_addr.kind = DDSI_LOCATOR_KIND_SHEM;
    gv->loc_iceoryx_addr.port = 0;
  }

  {
    char buf[DDSI_LOCSTRLEN];
    GVLOG (DDS_LC_CONFIG | DDS_LC_SHM, "My iceoryx address: %s\n", ddsi_locator_to_string (buf, sizeof (buf), &gv->loc_iceoryx_addr));
  }

  if (gv->n_interfaces == MAX_XMIT_CONNS)
  {
    GVERROR ("maximum number of interfaces reached, can't add virtual one for iceoryx\n");
    return -1;
  }
  struct ddsi_network_interface *intf = &gv->interfaces[gv->n_interfaces];
  // Pick a (ideally unique, but it isn't actually used) interface index
  // Unix machines tend to use small integers, so this should be easy to recognize
  intf->if_index = 1000;
  for (int i = 0; i < gv->n_interfaces; i++)
    if (gv->interfaces[i].if_index >= intf->if_index)
      intf->if_index = gv->interfaces[i].if_index + 1;
  intf->link_local = true; // Makes it so that non-local addresses are ignored
  intf->loc = gv->loc_iceoryx_addr;
  intf->extloc = intf->loc;
  intf->loopback = false;
  intf->mc_capable = true; // FIXME: matters most for discovery, this avoids auto-lack-of-multicast-mitigation
  intf->mc_flaky = false;
  intf->name = ddsrt_strdup ("iceoryx");
  intf->point_to_point = false;
  intf->netmask.kind = DDSI_LOCATOR_KIND_INVALID;
  intf->netmask.port = DDSI_LOCATOR_PORT_INVALID;
  memset (intf->netmask.address, 0, sizeof (intf->netmask.address) - 6);
  gv->n_interfaces++;
  return 0;
}
#endif

int ddsi_init (struct ddsi_domaingv *gv)
{
  uint32_t port_disc_uc = 0;
  uint32_t port_data_uc = 0;
  ddsrt_mtime_t reset_deaf_mute_time = DDSRT_MTIME_NEVER;

  gv->tstart = ddsrt_time_wallclock ();    /* wall clock time, used in logs */

  ddsi_plist_init_tables ();

  gv->disc_conn_uc = NULL;
  gv->data_conn_uc = NULL;
  gv->disc_conn_mc = NULL;
  gv->data_conn_mc = NULL;
  for (size_t i = 0; i < MAX_XMIT_CONNS; i++)
    gv->xmit_conns[i] = NULL;
  gv->listener = NULL;
  gv->debmon = NULL;
  gv->n_recv_threads = 0;
  gv->ddsi_tran_factories = NULL;

  /* Print start time for referencing relative times in the remainder of the DDS_LOG. */
  {
    int sec = (int) (gv->tstart.v / 1000000000);
    int usec = (int) (gv->tstart.v % 1000000000) / 1000;
    char str[DDSRT_RFC3339STRLEN+1];
    ddsrt_ctime(gv->tstart.v, str, sizeof(str));
    GVLOG (DDS_LC_CONFIG, "started at %d.06%d -- %s\n", sec, usec, str);
  }

  /* Allow configuration to set "deaf_mute" in case we want to start out that way */
  gv->deaf = gv->config.initial_deaf;
  gv->mute = gv->config.initial_mute;
  if (gv->deaf || gv->mute)
  {
    GVLOG (DDS_LC_CONFIG | DDS_LC_DISCOVERY, "DEAFMUTE initial deaf=%d mute=%d reset after %"PRId64"d ns\n", gv->deaf, gv->mute, gv->config.initial_deaf_mute_reset);
    reset_deaf_mute_time = ddsrt_mtime_add_duration (ddsrt_time_monotonic (), gv->config.initial_deaf_mute_reset);
  }

  /* Initialize UDP or TCP transport and resolve factory */
  switch (gv->config.transport_selector)
  {
    case DDSI_TRANS_DEFAULT:
      assert(0);
    case DDSI_TRANS_UDP:
    case DDSI_TRANS_UDP6:
      gv->config.publish_uc_locators = 1;
      gv->config.enable_uc_locators = 1;
      if (ddsi_udp_init (gv) < 0)
        goto err_udp_tcp_init;
      gv->m_factory = ddsi_factory_find (gv, gv->config.transport_selector == DDSI_TRANS_UDP ? "udp" : "udp6");
      break;
    case DDSI_TRANS_TCP:
    case DDSI_TRANS_TCP6:
      gv->config.publish_uc_locators = (gv->config.tcp_port != -1);
      gv->config.enable_uc_locators = 1;
      /* TCP affects what features are supported/required */
      gv->config.many_sockets_mode = DDSI_MSM_SINGLE_UNICAST;
      gv->config.allowMulticast = DDSI_AMC_FALSE;
      if (ddsi_tcp_init (gv) < 0)
        goto err_udp_tcp_init;
      gv->m_factory = ddsi_factory_find (gv, gv->config.transport_selector == DDSI_TRANS_TCP ? "tcp" : "tcp6");
      break;
    case DDSI_TRANS_RAWETH:
      gv->config.publish_uc_locators = 1;
      gv->config.enable_uc_locators = 0;
      gv->config.participantIndex = DDSI_PARTICIPANT_INDEX_NONE;
      gv->config.many_sockets_mode = DDSI_MSM_NO_UNICAST;
      if (ddsi_raweth_init (gv) < 0)
        goto err_udp_tcp_init;
      gv->m_factory = ddsi_factory_find (gv, "raweth");
      break;
    case DDSI_TRANS_NONE:
      gv->config.publish_uc_locators = 0;
      gv->config.enable_uc_locators = 0;
      gv->config.participantIndex = DDSI_PARTICIPANT_INDEX_NONE;
      gv->config.many_sockets_mode = DDSI_MSM_NO_UNICAST;
      if (ddsi_vnet_init (gv, "dummy", INT32_MAX) < 0)
        goto err_udp_tcp_init;
      gv->m_factory = ddsi_factory_find (gv, "dummy");
      break;
  }
  gv->m_factory->m_enable = true;

  if (!ddsi_find_own_ip (gv))
  {
    /* ddsi_find_own_ip already logs a more informative error message */
    GVLOG (DDS_LC_CONFIG, "No network interface selected\n");
    goto err_ddsi_find_own_ip;
  }

#ifdef DDS_HAS_SHM
  if (gv->config.enable_shm)
  {
    if (iceoryx_init (gv) < 0)
      goto err_iceoryx;
  }
#endif

  if (gv->config.allowMulticast)
  {
    for (int i = 0; i < gv->n_interfaces; i++)
    {
      if (!gv->interfaces[i].mc_capable)
      {
        GVWARNING ("selected interface \"%s\" is not multicast-capable: disabling multicast\n", gv->interfaces[i].name);
        gv->config.allowMulticast = DDSI_AMC_FALSE;
        /* ensure discovery can work: firstly, that the process will be reachable on a "well-known" port
         number, and secondly, that the local interface's IP address gets added to the discovery
         address set */
        gv->config.participantIndex = DDSI_PARTICIPANT_INDEX_AUTO;
      }
      else if (gv->config.allowMulticast & DDSI_AMC_DEFAULT)
      {
        /* default is dependent on network interface type: if multicast is believed to be flaky,
         use multicast only for SPDP packets */
        assert ((gv->config.allowMulticast & ~DDSI_AMC_DEFAULT) == 0);
        if (gv->interfaces[i].mc_flaky)
        {
          gv->config.allowMulticast = DDSI_AMC_SPDP;
          GVLOG (DDS_LC_CONFIG, "presumed flaky multicast, use for SPDP only\n");
        }
        else
        {
          GVLOG (DDS_LC_CONFIG, "presumed robust multicast support, use for everything\n");
          gv->config.allowMulticast = DDSI_AMC_TRUE;
        }
      }
    }
  }

  assert ((gv->config.allowMulticast & DDSI_AMC_DEFAULT) == 0);
  if (set_recvips (gv) < 0)
    goto err_set_recvips;
  if (set_spdp_address (gv) < 0)
    goto err_set_ext_address;
  if (set_default_mc_address (gv) < 0)
    goto err_set_ext_address;
  if (set_ext_address_and_mask (gv) < 0)
    goto err_set_ext_address;

  {
    char buf[DDSI_LOCSTRLEN], buf2[DDSI_LOCSTRLEN];
    /* the "ownip", "extip" labels in the trace have been there for so long, that it seems worthwhile to retain them even though they need not be IP any longer */
    GVLOG (DDS_LC_CONFIG, "ownip: ");
    for (int i = 0; i < gv->n_interfaces; i++)
    {
      if (ddsi_compare_locators(&gv->interfaces[i].loc, &gv->interfaces[i].extloc) == 0)
        GVLOG (DDS_LC_CONFIG, "%s%s", (i == 0) ? "" : ", ", ddsi_locator_to_string_no_port (buf, sizeof(buf), &gv->interfaces[i].loc));
      else
        GVLOG (DDS_LC_CONFIG, "%s%s (ext: %s)", (i == 0) ? "" : ", ", ddsi_locator_to_string_no_port (buf, sizeof(buf), &gv->interfaces[i].loc), ddsi_locator_to_string_no_port (buf2, sizeof(buf2), &gv->interfaces[i].extloc));
    }
    GVLOG (DDS_LC_CONFIG, "\n");
    GVLOG (DDS_LC_CONFIG, "extmask: %s%s\n", ddsi_locator_to_string_no_port (buf, sizeof(buf), &gv->extmask), gv->extmask.kind != DDSI_LOCATOR_KIND_UDPv4 ? " (not applicable)" : "");
    GVLOG (DDS_LC_CONFIG, "SPDP MC: %s\n", ddsi_locator_to_string_no_port (buf, sizeof(buf), &gv->loc_spdp_mc));
    GVLOG (DDS_LC_CONFIG, "default MC: %s\n", ddsi_locator_to_string_no_port (buf, sizeof(buf), &gv->loc_default_mc));
#ifdef DDS_HAS_SSM
    GVLOG (DDS_LC_CONFIG, "SSM support included\n");
#endif
  }

  gv->xmsgpool = ddsi_xmsgpool_new ();

  // copy default participant plist into one that is used for this domain's participants
  // a plain copy is safe because it doesn't alias anything
  gv->default_local_xqos_pp = ddsi_default_qos_participant;
  assert (gv->default_local_xqos_pp.present & DDSI_QP_LIVELINESS);
  assert (gv->default_local_xqos_pp.liveliness.kind == DDS_LIVELINESS_AUTOMATIC);
  gv->default_local_xqos_pp.liveliness.lease_duration = gv->config.lease_duration;

  ddsi_xqos_copy (&gv->spdp_endpoint_xqos, &ddsi_default_qos_reader);
  ddsi_xqos_mergein_missing (&gv->spdp_endpoint_xqos, &ddsi_default_qos_writer, ~(uint64_t)0);
  gv->spdp_endpoint_xqos.durability.kind = DDS_DURABILITY_TRANSIENT_LOCAL;
  assert (gv->spdp_endpoint_xqos.reliability.kind == DDS_RELIABILITY_BEST_EFFORT);
  make_builtin_endpoint_xqos (&gv->builtin_endpoint_xqos_rd, &ddsi_default_qos_reader);
  make_builtin_endpoint_xqos (&gv->builtin_endpoint_xqos_wr, &ddsi_default_qos_writer);
#ifdef DDS_HAS_TYPE_DISCOVERY
  make_builtin_volatile_endpoint_xqos(&gv->builtin_volatile_xqos_rd, &ddsi_default_qos_reader);
  make_builtin_volatile_endpoint_xqos(&gv->builtin_volatile_xqos_wr, &ddsi_default_qos_writer);
#endif
#ifdef DDS_HAS_SECURITY
  make_builtin_volatile_endpoint_xqos(&gv->builtin_secure_volatile_xqos_rd, &ddsi_default_qos_reader);
  make_builtin_volatile_endpoint_xqos(&gv->builtin_secure_volatile_xqos_wr, &ddsi_default_qos_writer);
  ddsi_xqos_copy (&gv->builtin_stateless_xqos_rd, &ddsi_default_qos_reader);
  ddsi_xqos_copy (&gv->builtin_stateless_xqos_wr, &ddsi_default_qos_writer);
  gv->builtin_stateless_xqos_wr.reliability.kind = DDS_RELIABILITY_BEST_EFFORT;
  gv->builtin_stateless_xqos_wr.durability.kind = DDS_DURABILITY_VOLATILE;

  /* Setting these properties allows the CryptoKeyFactory to recognize
   * the entities (see DDS Security spec chapter 8.8.8.1). */
  ddsi_xqos_add_property_if_unset(&gv->builtin_secure_volatile_xqos_rd, false, DDS_SEC_PROP_BUILTIN_ENDPOINT_NAME, "BuiltinParticipantVolatileMessageSecureReader");
  ddsi_xqos_add_property_if_unset(&gv->builtin_secure_volatile_xqos_wr, false, DDS_SEC_PROP_BUILTIN_ENDPOINT_NAME, "BuiltinParticipantVolatileMessageSecureWriter");
#endif

  /* participant location properties */
  {
    char * procname = ddsrt_getprocessname();
    char namebuf[256];

    if (procname) {
      ddsi_xqos_add_property_if_unset(&gv->default_local_xqos_pp, true, DDS_BUILTIN_TOPIC_PARTICIPANT_PROPERTY_PROCESS_NAME, procname);
      ddsrt_free(procname);
    }

    snprintf(namebuf, sizeof(namebuf), "%" PRIdPID, ddsrt_getpid());
    ddsi_xqos_add_property_if_unset(&gv->default_local_xqos_pp, true, DDS_BUILTIN_TOPIC_PARTICIPANT_PROPERTY_PID, namebuf);

#if DDSRT_HAVE_GETHOSTNAME
    if (ddsrt_gethostname(namebuf, sizeof(namebuf)) == DDS_RETCODE_OK) {
      ddsi_xqos_add_property_if_unset(&gv->default_local_xqos_pp, true, DDS_BUILTIN_TOPIC_PARTICIPANT_PROPERTY_HOSTNAME, namebuf);
    }
#endif
  }

  ddsrt_mutex_init (&gv->sertypes_lock);
  gv->sertypes = ddsrt_hh_new (1, ddsi_sertype_hash_wrap, ddsi_sertype_equal_wrap);

#ifdef DDS_HAS_TYPE_DISCOVERY
  ddsrt_mutex_init (&gv->typelib_lock);
  ddsrt_cond_init (&gv->typelib_resolved_cond);
  ddsrt_avl_init (&ddsi_typelib_treedef, &gv->typelib);
  ddsrt_avl_init (&ddsi_typedeps_treedef, &gv->typedeps);
  ddsrt_avl_init (&ddsi_typedeps_reverse_treedef, &gv->typedeps_reverse);
#endif
  ddsrt_mutex_init (&gv->new_topic_lock);
  ddsrt_cond_init (&gv->new_topic_cond);
  gv->new_topic_version = 0;
#ifdef DDS_HAS_TOPIC_DISCOVERY
  ddsrt_mutex_init (&gv->topic_defs_lock);
  gv->topic_defs = ddsrt_hh_new (1, topic_definition_hash_wrap, topic_definition_equal_wrap);
#endif
  make_special_types (gv);

  ddsrt_mutex_init (&gv->participant_set_lock);
  ddsrt_cond_init (&gv->participant_set_cond);
  ddsi_lease_management_init (gv);
  gv->deleted_participants = ddsi_deleted_participants_admin_new (&gv->logconfig, gv->config.prune_deleted_ppant.delay);
  gv->entity_index = ddsi_entity_index_new (gv);

  ddsrt_mutex_init (&gv->privileged_pp_lock);
  gv->privileged_pp = NULL;

  ddsrt_mutex_init(&gv->naming_lock);
  ddsrt_prng_init(&gv->naming_rng, &gv->config.entity_naming_seed);

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
      const struct ddsi_network_interface *intf = &gv->interfaces[i];
      ddsrt_md5_append (&st, (const ddsrt_md5_byte_t *) &intf->loc.kind, sizeof (intf->loc.kind));
      ddsrt_md5_append (&st, (const ddsrt_md5_byte_t *) intf->loc.address, sizeof (intf->loc.address));
    }
    ddsrt_md5_finish (&st, digest);
    /* DDSI 2.2 requires the first two bytes of the GUID to be set to the vendor
       code -- a terrible waste of entropy ... */
    gv->ppguid_base.prefix.s[0] = DDSI_VENDORID_ECLIPSE.id[0];
    gv->ppguid_base.prefix.s[1] = DDSI_VENDORID_ECLIPSE.id[1];
    DDSRT_STATIC_ASSERT (sizeof (gv->ppguid_base.prefix.s) > 2 && sizeof (gv->ppguid_base.prefix.s) - 2 <= sizeof (digest));
    memcpy (&gv->ppguid_base.prefix.s[2], digest, sizeof (gv->ppguid_base.prefix.s) - 2);
    gv->ppguid_base.prefix = ddsi_ntoh_guid_prefix (gv->ppguid_base.prefix);
    gv->ppguid_base.entityid.u = DDSI_ENTITYID_PARTICIPANT;
  }

  ddsrt_mutex_init (&gv->lock);
  ddsrt_mutex_init (&gv->spdp_lock);
  gv->spdp_defrag = ddsi_defrag_new (&gv->logconfig, DDSI_DEFRAG_DROP_OLDEST, gv->config.defrag_unreliable_maxsamples);
  gv->spdp_reorder = ddsi_reorder_new (&gv->logconfig, DDSI_REORDER_MODE_ALWAYS_DELIVER, gv->config.primary_reorder_maxsamples, false);

  gv->m_tkmap = ddsi_tkmap_new (gv);

  if (gv->m_factory->m_connless)
  {
    if (gv->config.participantIndex >= 0 || gv->config.participantIndex == DDSI_PARTICIPANT_INDEX_NONE)
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
        case MUSRET_PORTS_IN_USE:
          GVERROR ("rtps_init: failed to create unicast sockets for domain %"PRId32" participant index %d (ports %"PRIu32", %"PRIu32")\n", gv->config.extDomainId.value, gv->config.participantIndex, port_disc_uc, port_data_uc);
          goto err_unicast_sockets;
        case MUSRET_ERROR:
          /* something bad happened; assume make_uc_sockets logged the error */
          goto err_unicast_sockets;
      }
    }
    else if (gv->config.participantIndex == DDSI_PARTICIPANT_INDEX_AUTO)
    {
      /* try to find a free one, and update gv->config.participantIndex */
      enum make_uc_sockets_ret musret = MUSRET_PORTS_IN_USE;
      int ppid;
      GVLOG (DDS_LC_CONFIG, "rtps_init: trying to find a free participant index\n");
      for (ppid = 0; ppid <= gv->config.maxAutoParticipantIndex && musret == MUSRET_PORTS_IN_USE; ppid++)
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
          case MUSRET_PORTS_IN_USE: /* Try next one */
            break;
          case MUSRET_ERROR:
            /* something bad happened; assume make_uc_sockets logged the error */
            goto err_unicast_sockets;
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
    gv->pcap_fp = ddsi_new_pcap_file (gv, gv->config.pcap_file);
    if (gv->pcap_fp)
    {
      ddsrt_mutex_init (&gv->pcap_lock);
    }
  }
  else
  {
    gv->pcap_fp = NULL;
  }

  gv->mship = ddsi_new_mcgroup_membership();
  if (gv->m_factory->m_connless)
  {
    if (!(gv->config.many_sockets_mode == DDSI_MSM_NO_UNICAST && gv->config.allowMulticast))
      GVLOG (DDS_LC_CONFIG, "Unicast Ports: discovery %"PRIu32" data %"PRIu32"\n", ddsi_conn_port (gv->disc_conn_uc), ddsi_conn_port (gv->data_conn_uc));

    if (gv->config.allowMulticast)
    {
      if (!create_multicast_sockets (gv))
        goto err_mc_conn;

      if (gv->config.many_sockets_mode == DDSI_MSM_NO_UNICAST)
      {
        gv->data_conn_uc = gv->data_conn_mc;
        gv->disc_conn_uc = gv->disc_conn_mc;
        // FIXME: uc locators get set by make_uc_sockets for all cases but MSM_NO_UNICAST but we need them
        ddsi_conn_locator (gv->disc_conn_uc, &gv->loc_meta_uc);
        ddsi_conn_locator (gv->data_conn_uc, &gv->loc_default_uc);
      }

      /* Set multicast locators */
      if (!ddsi_is_unspec_locator(&gv->loc_spdp_mc))
        gv->loc_spdp_mc.port = ddsi_conn_port (gv->disc_conn_mc);
      if (!ddsi_is_unspec_locator(&gv->loc_meta_mc))
        gv->loc_meta_mc.port = ddsi_conn_port (gv->disc_conn_mc);
      if (!ddsi_is_unspec_locator(&gv->loc_default_mc))
        gv->loc_default_mc.port = ddsi_conn_port (gv->data_conn_mc);
    }
  }
  else
  {
    if (gv->config.tcp_port == -1)
      ; /* nop */
    else if (!ddsi_is_valid_port (gv->m_factory, (uint32_t) gv->config.tcp_port))
    {
      GVERROR ("Listener port %d is out of range for transport %s\n", gv->config.tcp_port, gv->m_factory->m_typename);
    }
    else
    {
      dds_return_t rc;
      rc = ddsi_factory_create_listener (&gv->listener, gv->m_factory, (uint32_t) gv->config.tcp_port, NULL);
      if (rc != DDS_RETCODE_OK || ddsi_listener_listen (gv->listener) != 0)
      {
        GVERROR ("Failed to create %s listener\n", gv->m_factory->m_typename);
        if (gv->listener)
          ddsi_listener_free(gv->listener);
        goto err_mc_conn;
      }

      /* Set unicast locators from listener */
      ddsi_set_unspec_locator (&gv->loc_spdp_mc);
      ddsi_set_unspec_locator (&gv->loc_meta_mc);
      ddsi_set_unspec_locator (&gv->loc_default_mc);

      ddsi_listener_locator (gv->listener, &gv->loc_meta_uc);
      ddsi_listener_locator (gv->listener, &gv->loc_default_uc);
    }
  }

  /* Create transmit connections */
  for (size_t i = 0; i < MAX_XMIT_CONNS; i++)
    gv->xmit_conns[i] = NULL;
  if (gv->config.many_sockets_mode == DDSI_MSM_NO_UNICAST)
  {
    gv->xmit_conns[0] = gv->data_conn_uc;
  }
  else
  {
    dds_return_t rc;
    for (int i = 0; i < gv->n_interfaces; i++)
    {
      const struct ddsi_tran_qos qos = {
        .m_purpose = (gv->config.allowMulticast ? DDSI_TRAN_QOS_XMIT_MC : DDSI_TRAN_QOS_XMIT_UC),
        .m_diffserv = 0,
        .m_interface = &gv->interfaces[i]
      };
      // FIXME: looking up the factory here is a hack to support iceoryx in addition to (e.g.) UDP
      struct ddsi_tran_factory * fact = ddsi_factory_find_supported_kind (gv, gv->interfaces[i].loc.kind);
      rc = ddsi_factory_create_conn (&gv->xmit_conns[i], fact, 0, &qos);
      if (rc != DDS_RETCODE_OK)
        goto err_mc_conn;
    }
  }
  for (int i = 0; i < gv->n_interfaces; i++)
  {
    GVLOG (DDS_LC_CONFIG, "interface %s: transmit port %d\n", gv->interfaces[i].name, (int) ddsi_conn_port (gv->xmit_conns[i]));
    gv->intf_xlocators[i].conn = gv->xmit_conns[i];
    gv->intf_xlocators[i].c = gv->interfaces[i].loc;
  }

  // Now that we know the interfaces and xmit_conns, we can convert the strings in the
  // network partition configuration to something useful.  Addresses must go first to
  // satisfy some assertions
  if (ddsi_convert_nwpart_config (gv, port_data_uc) < 0)
    goto err_network_partition_config;

  // Join SPDP, default multicast addresses if enabled
  if (gv->m_factory->m_connless && gv->config.allowMulticast)
  {
    if (joinleave_spdp_defmcip (gv, 1) < 0)
      goto err_joinleave_spdp;
  }

  /* Create event queues */
  gv->xevents = ddsi_xeventq_new (gv, gv->config.max_queued_rexmit_bytes, gv->config.max_queued_rexmit_msgs);

#ifdef DDS_HAS_SECURITY
  ddsi_omg_security_init (gv);
#endif

  gv->as_disc = ddsi_new_addrset ();
  if (gv->config.allowMulticast & DDSI_AMC_SPDP)
    ddsi_add_locator_to_addrset (gv, gv->as_disc, &gv->loc_spdp_mc);
  /* If multicast was enabled but not available, always add the local interface to the discovery address set.
     Conversion via string and add_peer_addresses has the benefit that the port number expansion happens
     automatically. */
  for (int i = 0; i < gv->n_interfaces; i++)
  {
    if (!gv->interfaces[i].mc_capable && gv->config.peers == NULL)
    {
      struct ddsi_config_peer_listelem peer_local;
      char local_addr[DDSI_LOCSTRLEN];
      ddsi_locator_to_string_no_port (local_addr, sizeof (local_addr), &gv->interfaces[i].loc);
      peer_local.next = NULL;
      peer_local.peer = local_addr;
      add_peer_addresses (gv, gv->as_disc, &peer_local);
    }
  }
  if (gv->config.peers)
  {
    add_peer_addresses (gv, gv->as_disc, gv->config.peers);
  }

  gv->gcreq_queue = ddsi_gcreq_queue_new (gv);

  ddsrt_atomic_st32 (&gv->rtps_keepgoing, 1);

  // sendq thread is started if a DW is created with non-zero latency
  gv->sendq_running = false;
  ddsrt_mutex_init (&gv->sendq_running_lock);

  gv->builtins_dqueue = ddsi_dqueue_new ("builtins", gv, gv->config.delivery_queue_maxsamples, ddsi_builtins_dqueue_handler, NULL);
  gv->user_dqueue = ddsi_dqueue_new ("user", gv, gv->config.delivery_queue_maxsamples, ddsi_user_dqueue_handler, NULL);

  if (reset_deaf_mute_time.v < DDS_NEVER)
    ddsi_qxev_callback (gv->xevents, reset_deaf_mute_time, reset_deaf_mute, NULL, 0, true);
  return 0;

#if 0
#ifdef DDS_HAS_SECURITY
err_post_omg_security_init:
  ddsi_omg_security_stop (gv); // should be a no-op as it starts lazily
  ddsi_omg_security_deinit (gv->security_context);
  ddsi_omg_security_free (gv);
#endif
#endif
err_joinleave_spdp:
  ddsi_free_config_nwpart_addresses (gv);
err_network_partition_config:
err_mc_conn:
  for (int i = 0; i < gv->n_interfaces; i++)
    gv->intf_xlocators[i].conn = NULL;
  free_conns (gv);
  if (gv->pcap_fp)
    ddsrt_mutex_destroy (&gv->pcap_lock);
  ddsi_free_mcgroup_membership (gv->mship);
err_unicast_sockets:
  ddsi_tkmap_free (gv->m_tkmap);
  ddsi_reorder_free (gv->spdp_reorder);
  ddsi_defrag_free (gv->spdp_defrag);
  ddsrt_mutex_destroy (&gv->spdp_lock);
  ddsrt_mutex_destroy (&gv->lock);
  ddsrt_mutex_destroy (&gv->privileged_pp_lock);
  ddsrt_mutex_destroy (&gv->naming_lock);

  ddsi_entity_index_free (gv->entity_index);
  gv->entity_index = NULL;
  ddsi_deleted_participants_admin_free (gv->deleted_participants);
  ddsi_lease_management_term (gv);
  ddsrt_cond_destroy (&gv->participant_set_cond);
  ddsrt_mutex_destroy (&gv->participant_set_lock);
  free_special_types (gv);
#ifndef NDEBUG
  {
    struct ddsrt_hh_iter it;
    assert (ddsrt_hh_iter_first (gv->sertypes, &it) == NULL);
  }
#endif
  ddsrt_hh_free (gv->sertypes);
  ddsrt_mutex_destroy (&gv->sertypes_lock);
#ifdef DDS_HAS_TOPIC_DISCOVERY
  ddsrt_hh_free (gv->topic_defs);
  ddsrt_mutex_destroy (&gv->topic_defs_lock);
#endif
  ddsrt_mutex_destroy (&gv->new_topic_lock);
  ddsrt_cond_destroy (&gv->new_topic_cond);
#ifdef DDS_HAS_TYPE_DISCOVERY
  ddsrt_avl_free (&ddsi_typelib_treedef, &gv->typelib, 0);
  ddsrt_avl_free (&ddsi_typedeps_treedef, &gv->typedeps, 0);
  ddsrt_avl_free (&ddsi_typedeps_reverse_treedef, &gv->typedeps_reverse, 0);
  ddsrt_mutex_destroy (&gv->typelib_lock);
  ddsrt_cond_destroy (&gv->typelib_resolved_cond);
#endif
#ifdef DDS_HAS_SECURITY
  ddsi_xqos_fini (&gv->builtin_stateless_xqos_wr);
  ddsi_xqos_fini (&gv->builtin_stateless_xqos_rd);
  ddsi_xqos_fini (&gv->builtin_secure_volatile_xqos_wr);
  ddsi_xqos_fini (&gv->builtin_secure_volatile_xqos_rd);
#endif
#ifdef DDS_HAS_TYPE_DISCOVERY
  ddsi_xqos_fini (&gv->builtin_volatile_xqos_wr);
  ddsi_xqos_fini (&gv->builtin_volatile_xqos_rd);
#endif
  ddsi_xqos_fini (&gv->builtin_endpoint_xqos_wr);
  ddsi_xqos_fini (&gv->builtin_endpoint_xqos_rd);
  ddsi_xqos_fini (&gv->spdp_endpoint_xqos);
  ddsi_xqos_fini (&gv->default_local_xqos_pp);

  ddsi_xmsgpool_free (gv->xmsgpool);
err_set_ext_address:
  while (gv->recvips)
  {
    struct ddsi_config_in_addr_node *n = gv->recvips;
    gv->recvips = n->next;
    ddsrt_free (n);
  }
err_set_recvips:
#ifdef DDS_HAS_SHM
err_iceoryx:
#endif
err_ddsi_find_own_ip:
  for (int i = 0; i < gv->n_interfaces; i++)
    ddsrt_free (gv->interfaces[i].name);
  ddsi_tran_factories_fini (gv);
err_udp_tcp_init:
  return -1;
}

int ddsi_start (struct ddsi_domaingv *gv)
{
  ddsi_gcreq_queue_start (gv->gcreq_queue);

  ddsi_dqueue_start (gv->builtins_dqueue);
  ddsi_dqueue_start (gv->user_dqueue);

  if (ddsi_xeventq_start (gv->xevents, NULL) < 0)
    return -1;

  if (gv->config.transport_selector != DDSI_TRANS_NONE && setup_and_start_recv_threads (gv) < 0)
  {
    ddsi_xeventq_stop (gv->xevents);
    return -1;
  }
  if (gv->listener)
  {
    if (ddsi_create_thread (&gv->listen_ts, gv, "listen", (uint32_t (*) (void *)) ddsi_listen_thread, gv->listener) != DDS_RETCODE_OK)
    {
      GVERROR ("failed to create TCP listener thread\n");
      ddsi_listener_free (gv->listener);
      gv->listener = NULL;
      ddsi_stop (gv);
      return -1;
    }
  }
  if (gv->config.monitor_port >= 0)
  {
    if ((gv->debmon = ddsi_new_debug_monitor (gv, gv->config.monitor_port)) == NULL)
    {
      GVERROR ("failed to create debug monitor thread\n");
      ddsi_stop (gv);
      return -1;
    }
    else {
      ddsi_locator_t loc;
      char buf[DDSI_LOCSTRLEN];

      if (ddsi_get_debug_monitor_locator(gv->debmon, &loc)) {
        ddsi_xqos_add_property_if_unset(&gv->default_local_xqos_pp, true, DDS_BUILTIN_TOPIC_PARTICIPANT_DEBUG_MONITOR,
          ddsi_locator_to_string (buf, sizeof(buf), &loc));
      }
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

void ddsi_stop (struct ddsi_domaingv *gv)
{
  struct ddsi_thread_state * const thrst = ddsi_lookup_thread_state ();

  if (gv->debmon)
  {
    ddsi_free_debug_monitor (gv->debmon);
    gv->debmon = NULL;
  }

  /* Stop all I/O */
  ddsi_term_prep (gv);
  if (gv->config.transport_selector != DDSI_TRANS_NONE)
    wait_for_receive_threads (gv);

  if (gv->listener)
  {
    ddsi_listener_unblock(gv->listener);
    ddsi_join_thread (gv->listen_ts);
    ddsi_listener_free(gv->listener);
  }

  ddsi_xeventq_stop (gv->xevents);

  /* Send a bubble through the delivery queue for built-ins, so that any
     pending proxy participant discovery is finished before we start
     deleting them */
  {
    struct dq_builtins_ready_arg arg;
    ddsrt_mutex_init (&arg.lock);
    ddsrt_cond_init (&arg.cond);
    arg.ready = 0;
    ddsi_dqueue_enqueue_callback(gv->builtins_dqueue, builtins_dqueue_ready_cb, &arg);
    ddsrt_mutex_lock (&arg.lock);
    while (!arg.ready)
      ddsrt_cond_wait (&arg.cond, &arg.lock);
    ddsrt_mutex_unlock (&arg.lock);
    ddsrt_cond_destroy (&arg.cond);
    ddsrt_mutex_destroy (&arg.lock);
  }

  /* Once the receive threads have stopped, defragmentation and
     reorder state can't change anymore, and can be freed safely.
     We don't do that here because it means rtps_init/rtps_fini
     allow will leak it. */

  {
    struct ddsi_entity_enum_proxy_participant est;
    struct ddsi_proxy_participant *proxypp;
    const ddsrt_wctime_t tnow = ddsrt_time_wallclock();
    /* Clean up proxy readers, proxy writers and proxy
       participants. Deleting a proxy participants deletes all its
       readers and writers automatically */
    ddsi_thread_state_awake (thrst, gv);
    ddsi_entidx_enum_proxy_participant_init (&est, gv->entity_index);
    while ((proxypp = ddsi_entidx_enum_proxy_participant_next (&est)) != NULL)
    {
      ddsi_delete_proxy_participant_by_guid (gv, &proxypp->e.guid, tnow, 1);
    }
    ddsi_entidx_enum_proxy_participant_fini (&est);
    ddsi_thread_state_asleep (thrst);
  }

  {
    struct ddsi_entity_enum_writer est_wr;
    struct ddsi_entity_enum_reader est_rd;
    struct ddsi_entity_enum_participant est_pp;
    struct ddsi_participant *pp;
    struct ddsi_writer *wr;
    struct ddsi_reader *rd;
    /* Delete readers, writers and participants, relying on
       delete_participant to schedule the deletion of the built-in
       rwriters to get all SEDP and SPDP dispose+unregister messages
       out. FIXME: need to keep xevent thread alive for a while
       longer. */
    ddsi_thread_state_awake (thrst, gv);
    ddsi_entidx_enum_writer_init (&est_wr, gv->entity_index);
    while ((wr = ddsi_entidx_enum_writer_next (&est_wr)) != NULL)
    {
      if (!ddsi_is_builtin_entityid (wr->e.guid.entityid, DDSI_VENDORID_ECLIPSE))
        ddsi_delete_writer_nolinger (gv, &wr->e.guid);
    }
    ddsi_entidx_enum_writer_fini (&est_wr);
    ddsi_thread_state_awake_to_awake_no_nest (thrst);
    ddsi_entidx_enum_reader_init (&est_rd, gv->entity_index);
    while ((rd = ddsi_entidx_enum_reader_next (&est_rd)) != NULL)
    {
      if (!ddsi_is_builtin_entityid (rd->e.guid.entityid, DDSI_VENDORID_ECLIPSE))
        ddsi_delete_reader (gv, &rd->e.guid);
    }
    ddsi_entidx_enum_reader_fini (&est_rd);
    ddsi_thread_state_awake_to_awake_no_nest (thrst);
#ifdef DDS_HAS_TOPIC_DISCOVERY
    struct ddsi_entity_enum_topic est_tp;
    struct ddsi_topic *tp;
    ddsi_entidx_enum_topic_init (&est_tp, gv->entity_index);
    while ((tp = ddsi_entidx_enum_topic_next (&est_tp)) != NULL)
      ddsi_delete_topic (gv, &tp->e.guid);
    ddsi_entidx_enum_topic_fini (&est_tp);
    ddsi_thread_state_awake_to_awake_no_nest (thrst);
#endif
    ddsi_entidx_enum_participant_init (&est_pp, gv->entity_index);
    while ((pp = ddsi_entidx_enum_participant_next (&est_pp)) != NULL)
    {
      ddsi_delete_participant (gv, &pp->e.guid);
    }
    ddsi_entidx_enum_participant_fini (&est_pp);
    ddsi_thread_state_asleep (thrst);
  }

  /* Stop background (handshake) processing in security implementation,
     do this only once we know no new events will be coming in. */
#if DDS_HAS_SECURITY
  ddsi_omg_security_stop (gv);
#endif

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
  ddsi_gcreq_queue_drain (gv->gcreq_queue);

  /* Clean up privileged_pp -- it must be NULL now (all participants
     are gone), but the lock still needs to be destroyed */
  assert (gv->privileged_pp == NULL);
  ddsrt_mutex_destroy (&gv->privileged_pp_lock);
}

void ddsi_fini (struct ddsi_domaingv *gv)
{
  /* The receive threads have already been stopped, therefore
     defragmentation and reorder state can't change anymore and
     can be freed. */
  ddsi_reorder_free (gv->spdp_reorder);
  ddsi_defrag_free (gv->spdp_defrag);
  ddsrt_mutex_destroy (&gv->spdp_lock);

  /* Shut down the GC system -- no new requests will be added */
  ddsi_gcreq_queue_free (gv->gcreq_queue);

  /* No new data gets added to any admin, all synchronous processing
     has ended, so now we can drain the delivery queues to end up with
     the expected reference counts all over the radmin thingummies. */
  ddsi_dqueue_free (gv->builtins_dqueue);
  ddsi_dqueue_free (gv->user_dqueue);

#ifdef DDS_HAS_SECURITY
  ddsi_omg_security_deinit (gv->security_context);
#endif

  ddsi_xeventq_free (gv->xevents);

  // if sendq thread is started
  ddsrt_mutex_lock (&gv->sendq_running_lock);
  if (gv->sendq_running)
  {
    ddsi_xpack_sendq_stop (gv);
    ddsi_xpack_sendq_fini (gv);
  }
  ddsrt_mutex_unlock (&gv->sendq_running_lock);

  (void) joinleave_spdp_defmcip (gv, 0);
  for (int i = 0; i < gv->n_interfaces; i++)
    gv->intf_xlocators[i].conn = NULL;
  free_conns (gv);
  ddsi_free_mcgroup_membership(gv->mship);
  ddsi_tran_factories_fini (gv);

  if (gv->pcap_fp)
  {
    ddsrt_mutex_destroy (&gv->pcap_lock);
    fclose (gv->pcap_fp);
  }

  ddsi_free_config_nwpart_addresses (gv);
  ddsi_unref_addrset (gv->as_disc);

  /* Must delay freeing of rbufpools until after *all* references have
     been dropped, which only happens once all receive threads have
     stopped, defrags and reorders have been freed, and all delivery
     queues been drained.  I.e., until very late in the game. */
  for (uint32_t i = 0; i < gv->n_recv_threads; i++)
  {
    if (gv->recv_threads[i].arg.mode == DDSI_RTM_MANY)
      ddsi_sock_waitset_free (gv->recv_threads[i].arg.u.many.ws);
    ddsi_rbufpool_free (gv->recv_threads[i].arg.rbpool);
  }

  ddsi_tkmap_free (gv->m_tkmap);
  ddsi_entity_index_free (gv->entity_index);
  gv->entity_index = NULL;
  ddsi_deleted_participants_admin_free (gv->deleted_participants);
  ddsi_lease_management_term (gv);
  ddsrt_mutex_destroy (&gv->participant_set_lock);
  ddsrt_cond_destroy (&gv->participant_set_cond);
  free_special_types (gv);

  ddsrt_mutex_destroy(&gv->naming_lock);

#ifdef DDS_HAS_TOPIC_DISCOVERY
#ifndef NDEBUG
  {
    struct ddsrt_hh_iter it;
    assert (ddsrt_hh_iter_first (gv->topic_defs, &it) == NULL);
  }
#endif
  ddsrt_hh_free (gv->topic_defs);
  ddsrt_mutex_destroy (&gv->topic_defs_lock);
#endif /* DDS_HAS_TOPIC_DISCOVERY */
#ifdef DDS_HAS_TYPE_DISCOVERY
#ifndef NDEBUG
  {
    assert(ddsrt_avl_is_empty(&gv->typelib));
    assert(ddsrt_avl_is_empty(&gv->typedeps));
    assert(ddsrt_avl_is_empty(&gv->typedeps_reverse));
  }
#endif
  ddsrt_avl_free (&ddsi_typelib_treedef, &gv->typelib, 0);
  ddsrt_avl_free (&ddsi_typedeps_treedef, &gv->typedeps, 0);
  ddsrt_avl_free (&ddsi_typedeps_reverse_treedef, &gv->typedeps_reverse, 0);
  ddsrt_mutex_destroy (&gv->typelib_lock);
#endif /* DDS_HAS_TYPE_DISCOVERY */
#ifndef NDEBUG
  {
    struct ddsrt_hh_iter it;
    assert (ddsrt_hh_iter_first (gv->sertypes, &it) == NULL);
  }
#endif
  ddsrt_hh_free (gv->sertypes);
  ddsrt_mutex_destroy (&gv->sertypes_lock);
#ifdef DDS_HAS_SECURITY
  ddsi_omg_security_free (gv);
  ddsi_xqos_fini (&gv->builtin_stateless_xqos_wr);
  ddsi_xqos_fini (&gv->builtin_stateless_xqos_rd);
  ddsi_xqos_fini (&gv->builtin_secure_volatile_xqos_wr);
  ddsi_xqos_fini (&gv->builtin_secure_volatile_xqos_rd);
#endif
#ifdef DDS_HAS_TYPE_DISCOVERY
  ddsi_xqos_fini (&gv->builtin_volatile_xqos_wr);
  ddsi_xqos_fini (&gv->builtin_volatile_xqos_rd);
#endif
  ddsi_xqos_fini (&gv->builtin_endpoint_xqos_wr);
  ddsi_xqos_fini (&gv->builtin_endpoint_xqos_rd);
  ddsi_xqos_fini (&gv->spdp_endpoint_xqos);
  ddsi_xqos_fini (&gv->default_local_xqos_pp);

  ddsrt_mutex_destroy (&gv->lock);

  while (gv->recvips)
  {
    struct ddsi_config_in_addr_node *n = gv->recvips;
    /* The compiler doesn't realize that n->next is always initialized. */
    DDSRT_WARNING_MSVC_OFF(6001);
    gv->recvips = n->next;
    DDSRT_WARNING_MSVC_ON(6001);
    ddsrt_free (n);
  }

  for (int i = 0; i < (int) gv->n_interfaces; i++)
    ddsrt_free (gv->interfaces[i].name);

  ddsi_xmsgpool_free (gv->xmsgpool);
  GVLOG (DDS_LC_CONFIG, "Finis.\n");
}
