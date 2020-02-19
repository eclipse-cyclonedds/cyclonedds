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

int64_t pseudo_random_delay (const ddsi_guid_t *x, const ddsi_guid_t *y, nn_mtime_t tnow, int64_t max_ms)
{
  /* You know, an ordinary random generator would be even better, but
     the C library doesn't have a reentrant one and I don't feel like
     integrating, say, the Mersenne Twister right now. */
  static const uint64_t cs[] = {
    UINT64_C (15385148050874689571),
    UINT64_C (17503036526311582379),
    UINT64_C (11075621958654396447),
    UINT64_C ( 9748227842331024047),
    UINT64_C (14689485562394710107),
    UINT64_C (17256284993973210745),
    UINT64_C ( 9288286355086959209),
    UINT64_C (17718429552426935775),
    UINT64_C (10054290541876311021),
    UINT64_C (13417933704571658407)
  };
  uint32_t a = x->prefix.u[0], b = x->prefix.u[1], c = x->prefix.u[2], d = x->entityid.u;
  uint32_t e = y->prefix.u[0], f = y->prefix.u[1], g = y->prefix.u[2], h = y->entityid.u;
  uint32_t i = (uint32_t) ((uint64_t) tnow.v >> 32), j = (uint32_t) tnow.v;
  uint64_t m = 0;
  m += (a + cs[0]) * (b + cs[1]);
  m += (c + cs[2]) * (d + cs[3]);
  m += (e + cs[4]) * (f + cs[5]);
  m += (g + cs[6]) * (h + cs[7]);
  m += (i + cs[8]) * (j + cs[9]);
  return (int64_t) (m >> 32) * max_ms / 4295;
}
