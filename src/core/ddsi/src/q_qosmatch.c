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
#include <assert.h>

#include "ddsi/q_time.h"
#include "ddsi/q_xqos.h"
#include "ddsi/q_misc.h"
#include "ddsi/q_qosmatch.h"

int is_wildcard_partition (const char *str)
{
  return strchr (str, '*') || strchr (str, '?');
}

static int partition_patmatch_p (const char *pat, const char *name)
{
  /* pat may be a wildcard expression, name must not be */
  if (!is_wildcard_partition (pat))
    /* no wildcard in pat => must equal name */
    return (strcmp (pat, name) == 0);
  else if (is_wildcard_partition (name))
    /* (we know: wildcard in pat) => wildcard in name => no match */
    return 0;
  else
    return ddsi2_patmatch (pat, name);
}

static int partitions_match_default (const nn_xqos_t *x)
{
  unsigned i;
  if (!(x->present & QP_PARTITION) || x->partition.n == 0)
    return 1;
  for (i = 0; i < x->partition.n; i++)
    if (partition_patmatch_p (x->partition.strs[i], ""))
      return 1;
  return 0;
}

int partitions_match_p (const nn_xqos_t *a, const nn_xqos_t *b)
{
  if (!(a->present & QP_PARTITION) || a->partition.n == 0)
    return partitions_match_default (b);
  else if (!(b->present & QP_PARTITION) || b->partition.n == 0)
    return partitions_match_default (a);
  else
  {
    unsigned i, j;
    for (i = 0; i < a->partition.n; i++)
      for (j = 0; j < b->partition.n; j++)
      {
        if (partition_patmatch_p (a->partition.strs[i], b->partition.strs[j]) ||
            partition_patmatch_p (b->partition.strs[j], a->partition.strs[i]))
          return 1;
      }
    return 0;
  }
}

int partition_match_based_on_wildcard_in_left_operand (const nn_xqos_t *a, const nn_xqos_t *b, const char **realname)
{
  assert (partitions_match_p (a, b));
  if (!(a->present & QP_PARTITION) || a->partition.n == 0)
  {
    return 0;
  }
  else if (!(b->present & QP_PARTITION) || b->partition.n == 0)
  {
    /* Either A explicitly includes the default partition, or it is a
       wildcard that matches it */
    unsigned i;
    for (i = 0; i < a->partition.n; i++)
      if (strcmp (a->partition.strs[i], "") == 0)
        return 0;
    *realname = "";
    return 1;
  }
  else
  {
    unsigned i, j;
    int maybe_yes = 0;
    for (i = 0; i < a->partition.n; i++)
      for (j = 0; j < b->partition.n; j++)
      {
        if (partition_patmatch_p (a->partition.strs[i], b->partition.strs[j]))
        {
          if (!is_wildcard_partition (a->partition.strs[i]))
            return 0;
          else
          {
            *realname = b->partition.strs[j];
            maybe_yes = 1;
          }
        }
      }
    return maybe_yes;
  }
}

static int ddsi_duration_is_lt (nn_duration_t a0, nn_duration_t b0)
{
  /* inf counts as <= inf */
  const int64_t a = nn_from_ddsi_duration (a0);
  const int64_t b = nn_from_ddsi_duration (b0);
  if (a == T_NEVER)
    return 0;
  else if (b == T_NEVER)
    return 1;
  else
    return a < b;
}

/* Duplicates of DDS policy ids to avoid inclusion of actual definitions */

#define Q_INVALID_QOS_POLICY_ID 0
#define Q_USERDATA_QOS_POLICY_ID 1
#define Q_DURABILITY_QOS_POLICY_ID 2
#define Q_PRESENTATION_QOS_POLICY_ID 3
#define Q_DEADLINE_QOS_POLICY_ID 4
#define Q_LATENCYBUDGET_QOS_POLICY_ID 5
#define Q_OWNERSHIP_QOS_POLICY_ID 6
#define Q_OWNERSHIPSTRENGTH_QOS_POLICY_ID 7
#define Q_LIVELINESS_QOS_POLICY_ID 8
#define Q_TIMEBASEDFILTER_QOS_POLICY_ID 9
#define Q_PARTITION_QOS_POLICY_ID 10
#define Q_RELIABILITY_QOS_POLICY_ID 11
#define Q_DESTINATIONORDER_QOS_POLICY_ID 12
#define Q_HISTORY_QOS_POLICY_ID 13
#define Q_RESOURCELIMITS_QOS_POLICY_ID 14
#define Q_ENTITYFACTORY_QOS_POLICY_ID 15
#define Q_WRITERDATALIFECYCLE_QOS_POLICY_ID 16
#define Q_READERDATALIFECYCLE_QOS_POLICY_ID 17
#define Q_TOPICDATA_QOS_POLICY_ID 18
#define Q_GROUPDATA_QOS_POLICY_ID 19
#define Q_TRANSPORTPRIORITY_QOS_POLICY_ID 20
#define Q_LIFESPAN_QOS_POLICY_ID 21
#define Q_DURABILITYSERVICE_QOS_POLICY_ID 22

int32_t qos_match_p (const nn_xqos_t *rd, const nn_xqos_t *wr)
{
#ifndef NDEBUG
  unsigned musthave = (QP_RXO_MASK | QP_PARTITION | QP_TOPIC_NAME | QP_TYPE_NAME);
  assert ((rd->present & musthave) == musthave);
  assert ((wr->present & musthave) == musthave);
#endif
  if (strcmp (rd->topic_name, wr->topic_name) != 0)
  {
    return Q_INVALID_QOS_POLICY_ID;
  }
  if (strcmp (rd->type_name, wr->type_name) != 0)
  {
    return Q_INVALID_QOS_POLICY_ID;
  }
  if (rd->relaxed_qos_matching.value || wr->relaxed_qos_matching.value)
  {
    if (rd->reliability.kind != wr->reliability.kind)
    {
      return Q_RELIABILITY_QOS_POLICY_ID;
    }
  }
  else
  {
    if (rd->reliability.kind > wr->reliability.kind)
    {
      return Q_RELIABILITY_QOS_POLICY_ID;
    }
    if (rd->durability.kind > wr->durability.kind)
    {
      return Q_DURABILITY_QOS_POLICY_ID;
    }
    if (rd->presentation.access_scope > wr->presentation.access_scope)
    {
      return Q_PRESENTATION_QOS_POLICY_ID;
    }
    if (rd->presentation.coherent_access > wr->presentation.coherent_access)
    {
      return Q_PRESENTATION_QOS_POLICY_ID;
    }
    if (rd->presentation.ordered_access > wr->presentation.ordered_access)
    {
      return Q_PRESENTATION_QOS_POLICY_ID;
    }
    if (ddsi_duration_is_lt (rd->deadline.deadline, wr->deadline.deadline))
    {
      return Q_DEADLINE_QOS_POLICY_ID;
    }
    if (ddsi_duration_is_lt (rd->latency_budget.duration, wr->latency_budget.duration))
    {
      return Q_LATENCYBUDGET_QOS_POLICY_ID;
    }
    if (rd->ownership.kind != wr->ownership.kind)
    {
      return Q_OWNERSHIP_QOS_POLICY_ID;
    }
    if (rd->liveliness.kind > wr->liveliness.kind)
    {
      return Q_LIVELINESS_QOS_POLICY_ID;
    }
    if (ddsi_duration_is_lt (rd->liveliness.lease_duration, wr->liveliness.lease_duration))
    {
      return Q_LIVELINESS_QOS_POLICY_ID;
    }
    if (rd->destination_order.kind > wr->destination_order.kind)
    {
      return Q_DESTINATIONORDER_QOS_POLICY_ID;
    }
  }
  if (!partitions_match_p (rd, wr))
  {
    return Q_PARTITION_QOS_POLICY_ID;
  }
  return -1;
}
