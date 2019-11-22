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

#include "dds/ddsi/q_misc.h"

#include "dds/ddsrt/md5.h"
#include "dds/ddsi/q_bswap.h"

extern inline seqno_t fromSN (const nn_sequence_number_t sn);
extern inline nn_sequence_number_t toSN (seqno_t n);

#ifdef DDSI_INCLUDE_NETWORK_PARTITIONS
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
