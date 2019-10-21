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

#include "cyclonedds/ddsi/q_time.h"
#include "cyclonedds/ddsi/q_xqos.h"
#include "cyclonedds/ddsi/q_misc.h"
#include "cyclonedds/ddsi/q_qosmatch.h"

static int is_wildcard_partition (const char *str)
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

static int partitions_match_default (const dds_qos_t *x)
{
  if (!(x->present & QP_PARTITION) || x->partition.n == 0)
    return 1;
  for (uint32_t i = 0; i < x->partition.n; i++)
    if (partition_patmatch_p (x->partition.strs[i], ""))
      return 1;
  return 0;
}

int partitions_match_p (const dds_qos_t *a, const dds_qos_t *b)
{
  if (!(a->present & QP_PARTITION) || a->partition.n == 0)
    return partitions_match_default (b);
  else if (!(b->present & QP_PARTITION) || b->partition.n == 0)
    return partitions_match_default (a);
  else
  {
    for (uint32_t i = 0; i < a->partition.n; i++)
      for (uint32_t j = 0; j < b->partition.n; j++)
      {
        if (partition_patmatch_p (a->partition.strs[i], b->partition.strs[j]) ||
            partition_patmatch_p (b->partition.strs[j], a->partition.strs[i]))
          return 1;
      }
    return 0;
  }
}

bool qos_match_mask_p (const dds_qos_t *rd, const dds_qos_t *wr, uint64_t mask, dds_qos_policy_id_t *reason)
{
#ifndef NDEBUG
  unsigned musthave = (QP_RXO_MASK | QP_PARTITION | QP_TOPIC_NAME | QP_TYPE_NAME) & mask;
  assert ((rd->present & musthave) == musthave);
  assert ((wr->present & musthave) == musthave);
#endif
  mask &= rd->present & wr->present;
  *reason = DDS_INVALID_QOS_POLICY_ID;
  if ((mask & QP_TOPIC_NAME) && strcmp (rd->topic_name, wr->topic_name) != 0)
    return false;
  if ((mask & QP_TYPE_NAME) && strcmp (rd->type_name, wr->type_name) != 0)
    return false;

  if ((mask & QP_RELIABILITY) && rd->reliability.kind > wr->reliability.kind) {
    *reason = DDS_RELIABILITY_QOS_POLICY_ID;
    return false;
  }
  if ((mask & QP_DURABILITY) && rd->durability.kind > wr->durability.kind) {
    *reason = DDS_DURABILITY_QOS_POLICY_ID;
    return false;
  }
  if ((mask & QP_PRESENTATION) && rd->presentation.access_scope > wr->presentation.access_scope) {
    *reason = DDS_PRESENTATION_QOS_POLICY_ID;
    return false;
  }
  if ((mask & QP_PRESENTATION) && rd->presentation.coherent_access > wr->presentation.coherent_access) {
    *reason = DDS_PRESENTATION_QOS_POLICY_ID;
    return false;
  }
  if ((mask & QP_PRESENTATION) && rd->presentation.ordered_access > wr->presentation.ordered_access) {
    *reason = DDS_PRESENTATION_QOS_POLICY_ID;
    return false;
  }
  if ((mask & QP_DEADLINE) && rd->deadline.deadline < wr->deadline.deadline) {
    *reason = DDS_DEADLINE_QOS_POLICY_ID;
    return false;
  }
  if ((mask & QP_LATENCY_BUDGET) && rd->latency_budget.duration < wr->latency_budget.duration) {
    *reason = DDS_LATENCYBUDGET_QOS_POLICY_ID;
    return false;
  }
  if ((mask & QP_OWNERSHIP) && rd->ownership.kind != wr->ownership.kind) {
    *reason = DDS_OWNERSHIP_QOS_POLICY_ID;
    return false;
  }
  if ((mask & QP_LIVELINESS) && rd->liveliness.kind > wr->liveliness.kind) {
    *reason = DDS_LIVELINESS_QOS_POLICY_ID;
    return false;
  }
  if ((mask & QP_LIVELINESS) && rd->liveliness.lease_duration < wr->liveliness.lease_duration) {
    *reason = DDS_LIVELINESS_QOS_POLICY_ID;
    return false;
  }
  if ((mask & QP_DESTINATION_ORDER) && rd->destination_order.kind > wr->destination_order.kind) {
    *reason = DDS_DESTINATIONORDER_QOS_POLICY_ID;
    return false;
  }
  if ((mask & QP_PARTITION) && !partitions_match_p (rd, wr)) {
    *reason = DDS_PARTITION_QOS_POLICY_ID;
    return false;
  }
  return true;
}

bool qos_match_p (const dds_qos_t *rd, const dds_qos_t *wr, dds_qos_policy_id_t *reason)
{
  dds_qos_policy_id_t dummy;
  return qos_match_mask_p (rd, wr, ~(uint64_t)0, reason ? reason : &dummy);
}
