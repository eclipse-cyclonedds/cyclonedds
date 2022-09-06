/*
 * Copyright(c) 2022 ZettaScale Technology and others
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
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/attributes.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_config_impl.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_tran.h"
#include "dds/ddsi/ddsi_nwpart.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/q_misc.h"

#ifdef DDS_HAS_NETWORK_PARTITIONS
struct nwpart_iter {
  struct ddsi_domaingv *gv;
  const char *msgtag;
  struct ddsi_config_networkpartition_listelem *next_nwp;
  bool ok;
  struct ddsi_networkpartition_address **nextp_uc;
  struct ddsi_networkpartition_address **nextp_asm;
#ifdef DDS_HAS_SSM
  struct ddsi_networkpartition_address **nextp_ssm;
#endif
};

static int wildcard_wildcard_match (const char * p1, char * p2) ddsrt_nonnull_all;
static char *get_partition_search_pattern (const char *partition, const char *topic) ddsrt_nonnull_all;
static void free_config_networkpartition_addresses_one (struct ddsi_config_networkpartition_listelem *np) ddsrt_nonnull_all;
static void nwpart_iter_init (struct nwpart_iter *it, struct ddsi_domaingv *gv)
  ddsrt_nonnull_all;
static void nwpart_iter_error (struct nwpart_iter *it, const char *tok, const char *msg)
  ddsrt_nonnull_all;
static struct ddsi_config_networkpartition_listelem *nwpart_iter_next (struct nwpart_iter *it)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;
static bool nwpart_iter_ok (const struct nwpart_iter *it) ddsrt_nonnull_all;
static void nwpart_iter_append_address (struct nwpart_iter *it, const char *tok, const ddsi_locator_t *loc, uint32_t port)
  ddsrt_nonnull_all;
static int convert_network_partition_addresses (struct ddsi_domaingv *gv, uint32_t port_data_uc)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;
static int convert_network_partition_interfaces (struct ddsi_domaingv *gv, uint32_t port_data_uc)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

static int wildcard_wildcard_match (const char * p1, char * p2)
{
  /* both patterns are empty or contain wildcards => overlap */
  if ((strcmp(p1, "") == 0 || strcmp(p1, "*") == 0) &&
      (strcmp(p2, "") == 0 || strcmp(p2, "*") == 0))
    return 1;
  /* Either pattern is empty (but the other is not empty or wildcard only) => no overlap */
  if (strcmp (p1, "") == 0 || strcmp(p2, "") == 0)
    return 0;
  if ((p1[0] == '*' || p2[0] == '*') && (wildcard_wildcard_match (p1,p2+1) || wildcard_wildcard_match (p1+1,p2)))
    return 1;
  if ((p1[0] == '?' || p2[0] == '?' || p1[0] == p2[0]) && wildcard_wildcard_match (p1+1,p2+1))
    return 1;
  /* else, no match, return false */
  return 0;
}

static char *get_partition_search_pattern (const char *partition, const char *topic)
{
  size_t sz = strlen (partition) + strlen (topic) + 2;
  char *pt = ddsrt_malloc (sz);
  (void) snprintf (pt, sz, "%s.%s", partition, topic);
  return pt;
}

const struct ddsi_config_partitionmapping_listelem *find_partitionmapping (const struct ddsi_config *cfg, const char *partition, const char *topic)
{
  char *pt = get_partition_search_pattern (partition, topic);
  struct ddsi_config_partitionmapping_listelem *pm;
  for (pm = cfg->partitionMappings; pm; pm = pm->next)
    if (wildcard_wildcard_match (pt, pm->DCPSPartitionTopic))
      break;
  ddsrt_free (pt);
  return pm;
}

static int ddsi_is_ignored_partition_one (const struct ddsi_config *cfg, const char *partition, const char *topic)
{
  char *pt = get_partition_search_pattern (partition, topic);
  struct ddsi_config_ignoredpartition_listelem *ip;
  for (ip = cfg->ignoredPartitions; ip; ip = ip->next)
    if (wildcard_wildcard_match (pt, ip->DCPSPartitionTopic))
      break;
  ddsrt_free (pt);
  return ip != NULL;
}

static void get_partition_set_from_xqos (char const * const * *ps, uint32_t *nps, const struct dds_qos *xqos)
{
  static const char *ps_def = "";
  if ((xqos->present & QP_PARTITION) && xqos->partition.n > 0) {
    *ps = (char const * const *) xqos->partition.strs;
    *nps = xqos->partition.n;
  } else {
    *ps = &ps_def;
    *nps = 1;
  }
}

bool ddsi_is_ignored_partition (const struct ddsi_domaingv *gv, const struct dds_qos *xqos, const char *topic_name)
{
  char const * const *ps;
  uint32_t nps;
  get_partition_set_from_xqos (&ps, &nps, xqos);
  for (uint32_t i = 0; i < nps; i++)
    if (ddsi_is_ignored_partition_one (&gv->config, ps[i], topic_name))
      return true;
  return false;
}

static const struct ddsi_config_networkpartition_listelem *ddsi_get_partition_from_mapping_one (const struct ddsrt_log_cfg *logcfg, const struct ddsi_config *config, const char *partition, const char *topic)
{
  const struct ddsi_config_partitionmapping_listelem *pm;
  if ((pm = find_partitionmapping (config, partition, topic)) == NULL)
    return 0;
  else
  {
    DDS_CLOG (DDS_LC_DISCOVERY, logcfg, "matched writer for topic \"%s\" in partition \"%s\" to networkPartition \"%s\"\n", topic, partition, pm->networkPartition);
    return pm->partition;
  }
}

const struct ddsi_config_networkpartition_listelem *ddsi_get_partition_from_mapping (const struct ddsrt_log_cfg *logcfg, const struct ddsi_config *config, const struct dds_qos *xqos, const char *topic_name)
{
  char const * const *ps;
  uint32_t nps;
  get_partition_set_from_xqos (&ps, &nps, xqos);
  for (uint32_t i = 0; i < nps; i++)
  {
    const struct ddsi_config_networkpartition_listelem *nwp;
    if ((nwp = ddsi_get_partition_from_mapping_one (logcfg, config, ps[i], topic_name)) != NULL)
      return nwp;
  }
  return NULL;
}

static void free_config_networkpartition_addresses_one (struct ddsi_config_networkpartition_listelem *np)
{
  struct ddsi_networkpartition_address **ps[] = {
    &np->uc_addresses,
    &np->asm_addresses
#ifdef DDS_HAS_SSM
    , &np->ssm_addresses
#endif
  };
  for (size_t i = 0; i < sizeof (ps) / sizeof (ps[0]); i++)
  {
    while (*ps[i])
    {
      struct ddsi_networkpartition_address *x = *ps[i];
      *ps[i] = x->next;
      ddsrt_free (x);
    }
  }
}

void free_config_networkpartition_addresses (struct ddsi_domaingv *gv)
{
  for (struct ddsi_config_networkpartition_listelem *np = gv->config.networkPartitions; np; np = np->next)
    free_config_networkpartition_addresses_one (np);
}

static void nwpart_iter_init (struct nwpart_iter *it, struct ddsi_domaingv *gv)
{
  it->gv = gv;
  it->msgtag = NULL;
  it->next_nwp = gv->config.networkPartitions;
  it->ok = true;
}

static void nwpart_iter_error (struct nwpart_iter *it, const char *tok, const char *msg)
{
  struct ddsi_domaingv * const gv = it->gv;
  GVERROR ("network partition %s: %s%s%s\n", it->msgtag, tok, (*tok && *msg) ? ": " : "", msg);
  it->ok = false;
}

static struct ddsi_config_networkpartition_listelem *nwpart_iter_next (struct nwpart_iter *it)
{
  if (it->next_nwp == NULL)
    return NULL;
  it->msgtag = it->next_nwp->name;
  struct ddsi_config_networkpartition_listelem *nwp = it->next_nwp;
  it->next_nwp = nwp->next;
  it->nextp_uc = &nwp->uc_addresses;
  it->nextp_asm = &nwp->asm_addresses;
#ifdef DDS_HAS_SSM
  it->nextp_ssm = &nwp->ssm_addresses;
#endif
  return nwp;
}

static bool nwpart_iter_ok (const struct nwpart_iter *it)
{
  return it->ok;
}

static void nwpart_iter_append_address (struct nwpart_iter *it, const char *tok, const ddsi_locator_t *loc, uint32_t port)
{
  struct ddsi_networkpartition_address ***nextpp;
  ddsi_locator_t loc_to_use = *loc;
  if (ddsi_is_mcaddr (it->gv, loc))
  {
#ifdef DDS_HAS_SSM
    nextpp = ddsi_is_ssm_mcaddr (it->gv, loc) ? &it->nextp_ssm : &it->nextp_asm;
#else
    nextpp = &it->nextp_asm;
#endif
  }
  else
  {
    nextpp = &it->nextp_uc;
    size_t interf_idx;
    switch (ddsi_is_nearby_address (it->gv, loc, (size_t) it->gv->n_interfaces, it->gv->interfaces, &interf_idx))
    {
      case DNAR_SELF:
        // always advertise the configured external address
        loc_to_use = it->gv->interfaces[interf_idx].extloc;
        break;
      case DNAR_LOCAL:
      case DNAR_DISTANT:
      case DNAR_UNREACHABLE:
        nwpart_iter_error (it, tok, "address does not match a local interface");
        break;
    }
  }

  if (!nwpart_iter_ok (it))
    return;
  else if ((**nextpp = ddsrt_malloc (sizeof (***nextpp))) == NULL)
    nwpart_iter_error (it, tok, "out of memory");
  else
  {
    (**nextpp)->loc = loc_to_use;
    (**nextpp)->loc.port = port;
    (**nextpp)->next = NULL;
    DDSRT_WARNING_MSVC_OFF(6011);
    *nextpp = &(**nextpp)->next;
    DDSRT_WARNING_MSVC_ON(6011);
  }
}

static bool nwpart_iter_fini (struct nwpart_iter *it)
{
  return it->ok;
}

static void convert_network_partition_addresses_one (struct nwpart_iter *npit, struct ddsi_config_networkpartition_listelem *np, uint32_t port_mc, uint32_t port_data_uc, const char *tok)
{
  // FIXME: it'd be nice if the one could specify a port and additional sockets would be created
  ddsi_locator_t loc;
  switch (ddsi_locator_from_string (npit->gv, &loc, tok, npit->gv->m_factory))
  {
    case AFSR_OK:       break;
    case AFSR_INVALID:  nwpart_iter_error (npit, tok, "not a valid address"); return;
    case AFSR_UNKNOWN:  nwpart_iter_error (npit, tok, "unknown address"); return;
    case AFSR_MISMATCH: nwpart_iter_error (npit, tok, "address family mismatch"); return;
  }
  if (loc.port != 0)
    nwpart_iter_error (npit, tok, "no port number expected");
  else if (ddsi_is_mcaddr (npit->gv, &loc))
    nwpart_iter_append_address (npit, tok, &loc, port_mc);
  else if (strspn (np->interface_names, ", \t") != strlen (np->interface_names))
    nwpart_iter_error (npit, tok, "unicast addresses not allowed when interfaces are also specified");
  else
    nwpart_iter_append_address (npit, tok, &loc, port_data_uc);
}

static int convert_network_partition_addresses (struct ddsi_domaingv *gv, uint32_t port_data_uc)
{
  const uint32_t port_mc = ddsi_get_port (&gv->config, DDSI_PORT_MULTI_DATA, 0);
  struct nwpart_iter npit;
  nwpart_iter_init (&npit, gv);
  struct ddsi_config_networkpartition_listelem *np;
  while ((np = nwpart_iter_next (&npit)) != NULL)
  {
    char *copy = ddsrt_strdup (np->address_string), *cursor = copy, *tok;
    while ((tok = ddsrt_strsep (&cursor, ",")) != NULL)
      convert_network_partition_addresses_one (&npit, np, port_mc, port_data_uc, tok);
    ddsrt_free (copy);
  }
  return nwpart_iter_fini (&npit) ? 0 : -1;
}

static int convert_network_partition_interfaces (struct ddsi_domaingv *gv, uint32_t port_data_uc)
{
  struct nwpart_iter npit;
  nwpart_iter_init (&npit, gv);
  struct ddsi_config_networkpartition_listelem *np;
  while ((np = nwpart_iter_next (&npit)) != NULL)
  {
    char *copy = ddsrt_strdup (np->interface_names), *cursor = copy, *tok;
    while ((tok = ddsrt_strsep (&cursor, ",")) != NULL)
    {
      int i;
      for (i = 0; i < gv->n_interfaces; i++)
        if (strcmp (tok, gv->interfaces[i].name) == 0)
          break;
      if (i == gv->n_interfaces)
        nwpart_iter_error (&npit, tok, "network partition references non-existent/configured interface");
      else
        nwpart_iter_append_address (&npit, tok, &gv->interfaces[i].loc, port_data_uc);
    }
    ddsrt_free (copy);
  }
  return nwpart_iter_fini (&npit) ? 0 : -1;
}

int convert_network_partition_config (struct ddsi_domaingv *gv, uint32_t port_data_uc)
{
  int rc;
  if ((rc = convert_network_partition_addresses (gv, port_data_uc)) < 0)
    return rc;
  if ((rc = convert_network_partition_interfaces (gv, port_data_uc)) < 0)
    return rc;
  return 0;
}

#else

void free_config_networkpartition_addresses (struct ddsi_domaingv *gv) {
  (void)gv;
}
bool ddsi_is_ignored_partition (const struct ddsi_domaingv *gv, const struct dds_qos *xqos, const char *topic_name) {
  (void)gv; (void)xqos; (void)topic_name;
  return false;
}
int convert_network_partition_config (struct ddsi_domaingv *gv, uint32_t port_data_uc) {
  (void)gv; (void)port_data_uc;
  return 0;
}

#endif // DDS_HAS_NETWORK_PARTITIONS
