// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>

#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/heap.h"

#include "ddsi__misc.h"

extern inline ddsi_seqno_t ddsi_from_seqno (const ddsi_sequence_number_t sn);
extern inline bool ddsi_validating_from_seqno (const ddsi_sequence_number_t sn, ddsi_seqno_t *res);
extern inline ddsi_sequence_number_t ddsi_to_seqno (ddsi_seqno_t n);

const ddsi_guid_t ddsi_nullguid = { .prefix = { .u = { 0,0,0 } }, .entityid = { .u = 0 } };

bool ddsi_guid_prefix_zero (const ddsi_guid_prefix_t *a)
{
  return a->u[0] == 0 && a->u[1] == 0 && a->u[2] == 0;
}

int ddsi_guid_prefix_eq (const ddsi_guid_prefix_t *a, const ddsi_guid_prefix_t *b)
{
  return a->u[0] == b->u[0] && a->u[1] == b->u[1] && a->u[2] == b->u[2];
}

int ddsi_guid_eq (const struct ddsi_guid *a, const struct ddsi_guid *b)
{
  return ddsi_guid_prefix_eq(&a->prefix, &b->prefix) && (a->entityid.u == b->entityid.u);
}

int ddsi_patmatch (const char *pat, const char *str)
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
        if (*str == *pat && ddsi_patmatch (pat+1, str+1))
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

