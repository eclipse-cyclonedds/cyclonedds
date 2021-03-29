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
#include <string.h>

#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/heap.h"

#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_misc.h"

extern inline seqno_t fromSN (const nn_sequence_number_t sn);
extern inline nn_sequence_number_t toSN (seqno_t n);

#ifdef DDS_HAS_NETWORK_PARTITIONS
int WildcardOverlap(char * p1, char * p2)
{
  /* both patterns are empty or contain wildcards => overlap */
  if ((p1 == NULL || strcmp(p1,"") == 0 || strcmp(p1,"*") == 0) &&
      (p2 == NULL || strcmp(p2,"") == 0 || strcmp(p2,"*") == 0))
    return 1;

  /* Either pattern is empty (but the other is not empty or wildcard only) => no overlap */
  if (p1 == NULL || strcmp(p1,"") == 0 || p2 == NULL || strcmp(p2,"")==0)
    return 0;

  if ( (p1[0] == '*' || p2[0] == '*') && (WildcardOverlap(p1,p2+1)|| WildcardOverlap(p1+1,p2)))
    return 1;

  if ( (p1[0] == '?' || p2[0] == '?' || p1[0] == p2[0] ) && WildcardOverlap(p1+1,p2+1))
    return 1;

  /* else, no match, return false */
  return 0;
}
#endif

const ddsi_guid_t nullguid = { .prefix = { .u = { 0,0,0 } }, .entityid = { .u = 0 } };

bool guid_prefix_zero (const ddsi_guid_prefix_t *a)
{
  return a->u[0] == 0 && a->u[1] == 0 && a->u[2] == 0;
}

int guid_prefix_eq (const ddsi_guid_prefix_t *a, const ddsi_guid_prefix_t *b)
{
  return a->u[0] == b->u[0] && a->u[1] == b->u[1] && a->u[2] == b->u[2];
}

int guid_eq (const struct ddsi_guid *a, const struct ddsi_guid *b)
{
  return guid_prefix_eq(&a->prefix, &b->prefix) && (a->entityid.u == b->entityid.u);
}

int ddsi2_patmatch (const char *pat, const char *str)
{
  while (*pat)
  {
    if (*pat == '?')
    {
      /* any character will do */
      if (*str++ == 0)
      {
        return 0;
      }
      pat++;
    }
    else if (*pat == '*')
    {
      /* collapse a sequence of wildcards, requiring as many
       characters in str as there are ?s in the sequence */
      while (*pat == '*' || *pat == '?')
      {
        if (*pat == '?' && *str++ == 0)
        {
          return 0;
        }
        pat++;
      }
      /* try matching on all positions where str matches pat */
      while (*str)
      {
        if (*str == *pat && ddsi2_patmatch (pat+1, str+1))
        {
          return 1;
        }
        str++;
      }
      return *pat == 0;
    }
    else
    {
      /* only an exact match */
      if (*str++ != *pat++)
      {
        return 0;
      }
    }
  }
  return *str == 0;
}

#ifdef DDS_HAS_NETWORK_PARTITIONS
static char *get_partition_search_pattern (const char *partition, const char *topic)
{
  size_t sz = strlen (partition) + strlen (topic) + 2;
  char *pt = ddsrt_malloc (sz);
  (void) snprintf (pt, sz, "%s.%s", partition, topic);
  return pt;
}

struct ddsi_config_partitionmapping_listelem *find_partitionmapping (const struct ddsi_config *cfg, const char *partition, const char *topic)
{
  char *pt = get_partition_search_pattern (partition, topic);
  struct ddsi_config_partitionmapping_listelem *pm;
  for (pm = cfg->partitionMappings; pm; pm = pm->next)
    if (WildcardOverlap (pt, pm->DCPSPartitionTopic))
      break;
  ddsrt_free (pt);
  return pm;
}

int is_ignored_partition (const struct ddsi_config *cfg, const char *partition, const char *topic)
{
  char *pt = get_partition_search_pattern (partition, topic);
  struct ddsi_config_ignoredpartition_listelem *ip;
  for (ip = cfg->ignoredPartitions; ip; ip = ip->next)
    if (WildcardOverlap(pt, ip->DCPSPartitionTopic))
      break;
  ddsrt_free (pt);
  return ip != NULL;
}
#endif /* DDS_HAS_NETWORK_PARTITIONS */

#ifdef DDS_HAS_NETWORK_CHANNELS
struct ddsi_config_channel_listelem *find_channel (const struct config *cfg, nn_transport_priority_qospolicy_t transport_priority)
{
  struct ddsi_config_channel_listelem *c;
  /* Channel selection is to use the channel with the lowest priority
     not less than transport_priority, or else the one with the
     highest priority. */
  assert(cfg->channels != NULL);
  assert(cfg->max_channel != NULL);
  for (c = cfg->channels; c; c = c->next)
  {
    assert(c->next == NULL || c->next->priority > c->priority);
    if (transport_priority.value <= c->priority)
      return c;
  }
  return cfg->max_channel;
}
#endif /* DDS_HAS_NETWORK_CHANNELS */
