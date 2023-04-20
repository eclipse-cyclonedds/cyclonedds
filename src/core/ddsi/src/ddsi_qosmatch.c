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
#include <assert.h>

#include "dds/features.h"
#include "dds/ddsi/ddsi_xqos.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_entity.h"
#include "dds/ddsi/ddsi_qosmatch.h"
#include "ddsi__typelookup.h"
#include "ddsi__misc.h"
#include "ddsi__typelib.h"
#include "dds/dds.h"

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
    return ddsi_patmatch (pat, name);
}

static int partitions_match_default (const dds_qos_t *x)
{
  if (!(x->present & DDSI_QP_PARTITION) || x->partition.n == 0)
    return 1;
  for (uint32_t i = 0; i < x->partition.n; i++)
    if (partition_patmatch_p (x->partition.strs[i], ""))
      return 1;
  return 0;
}

static int partitions_match_p (const dds_qos_t *a, const dds_qos_t *b)
{
  if (!(a->present & DDSI_QP_PARTITION) || a->partition.n == 0)
    return partitions_match_default (b);
  else if (!(b->present & DDSI_QP_PARTITION) || b->partition.n == 0)
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

#ifdef DDS_HAS_TYPE_DISCOVERY

static uint32_t is_endpoint_type_resolved (struct ddsi_domaingv *gv, char *type_name, const ddsi_type_pair_t *type_pair, bool *req_lookup, const char *entity)
  ddsrt_nonnull((1, 2, 3));

static uint32_t is_endpoint_type_resolved (struct ddsi_domaingv *gv, char *type_name, const ddsi_type_pair_t *type_pair, bool *req_lookup, const char *entity)
{
  assert (type_pair);
  ddsrt_mutex_lock (&gv->typelib_lock);
  bool min_resolved = ddsi_type_resolved_locked (gv, type_pair->minimal, DDSI_TYPE_INCLUDE_DEPS),
    compl_resolved = ddsi_type_resolved_locked (gv, type_pair->complete, DDSI_TYPE_INCLUDE_DEPS);
  if (!min_resolved && !compl_resolved)
  {
    struct ddsi_typeid_str str;
    const ddsi_typeid_t *tid_m = ddsi_type_pair_minimal_id (type_pair),
      *tid_c = ddsi_type_pair_complete_id (type_pair);
    GVTRACE ("unresolved %s type %s ", entity, type_name);
    if (tid_m)
      GVTRACE ("min %s", ddsi_make_typeid_str (&str, tid_m));
    if (tid_c)
      GVTRACE ("compl %s", ddsi_make_typeid_str (&str, tid_c));
    GVTRACE ("\n");
    /* defer requesting unresolved type until after the endpoint qos lock
       has been released, so just set a bool value indicating that a type
       lookup is required */
    if (req_lookup != NULL)
      *req_lookup = true;
    ddsrt_mutex_unlock (&gv->typelib_lock);
    return DDS_XTypes_TK_NONE;
  }
  ddsrt_mutex_unlock (&gv->typelib_lock);

  if (min_resolved && compl_resolved)
    return DDS_XTypes_EK_BOTH;
  return compl_resolved ? DDS_XTypes_EK_COMPLETE : DDS_XTypes_EK_MINIMAL;
}

#endif /* DDS_HAS_TYPE_DISCOVERY */

static int data_representation_match_p (const dds_qos_t *rd_qos, const dds_qos_t *wr_qos)
{
  assert (rd_qos->present & DDSI_QP_DATA_REPRESENTATION);
  assert (rd_qos->data_representation.value.n > 0);
  assert (wr_qos->present & DDSI_QP_DATA_REPRESENTATION);
  assert (wr_qos->data_representation.value.n > 0);

  /* For the writer only use the first representation identifier and ignore 1..n (spec 7.6.3.1.1) */
  for (uint32_t i = 0; i < rd_qos->data_representation.value.n; i++)
    if (rd_qos->data_representation.value.ids[i] == wr_qos->data_representation.value.ids[0])
      return 1;
  return 0;
}

#ifdef DDS_HAS_TYPE_DISCOVERY
static bool type_pair_has_id (const ddsi_type_pair_t *pair)
{
  return pair && (pair->minimal || pair->complete);
}
#endif

bool ddsi_qos_match_mask_p (
    struct ddsi_domaingv *gv,
    const dds_qos_t *rd_qos,
    const dds_qos_t *wr_qos,
    uint64_t mask,
    dds_qos_policy_id_t *reason
#ifdef DDS_HAS_TYPE_DISCOVERY
    , const struct ddsi_type_pair *rd_type_pair
    , const struct ddsi_type_pair *wr_type_pair
    , bool *rd_typeid_req_lookup
    , bool *wr_typeid_req_lookup
#endif
)
{
  DDSRT_UNUSED_ARG (gv);
#ifndef NDEBUG
  uint64_t musthave = (DDSI_QP_RXO_MASK | DDSI_QP_PARTITION | DDSI_QP_TOPIC_NAME | DDSI_QP_TYPE_NAME | DDSI_QP_DATA_REPRESENTATION) & mask;
  assert ((rd_qos->present & musthave) == musthave);
  assert ((wr_qos->present & musthave) == musthave);
#endif

#ifdef DDS_HAS_TYPE_DISCOVERY
  if (rd_typeid_req_lookup != NULL)
    *rd_typeid_req_lookup = false;
  if (wr_typeid_req_lookup != NULL)
    *wr_typeid_req_lookup = false;
#endif

  mask &= rd_qos->present & wr_qos->present;
  *reason = DDS_INVALID_QOS_POLICY_ID;
  if ((mask & DDSI_QP_TOPIC_NAME) && strcmp (rd_qos->topic_name, wr_qos->topic_name) != 0)
    return false;

  if ((mask & DDSI_QP_RELIABILITY) && rd_qos->reliability.kind > wr_qos->reliability.kind) {
    *reason = DDS_RELIABILITY_QOS_POLICY_ID;
    return false;
  }
  if ((mask & DDSI_QP_DURABILITY) && rd_qos->durability.kind > wr_qos->durability.kind) {
    *reason = DDS_DURABILITY_QOS_POLICY_ID;
    return false;
  }
  if ((mask & DDSI_QP_PRESENTATION) && rd_qos->presentation.access_scope > wr_qos->presentation.access_scope) {
    *reason = DDS_PRESENTATION_QOS_POLICY_ID;
    return false;
  }
  if ((mask & DDSI_QP_PRESENTATION) && rd_qos->presentation.coherent_access > wr_qos->presentation.coherent_access) {
    *reason = DDS_PRESENTATION_QOS_POLICY_ID;
    return false;
  }
  if ((mask & DDSI_QP_PRESENTATION) && rd_qos->presentation.ordered_access > wr_qos->presentation.ordered_access) {
    *reason = DDS_PRESENTATION_QOS_POLICY_ID;
    return false;
  }
  if ((mask & DDSI_QP_DEADLINE) && rd_qos->deadline.deadline < wr_qos->deadline.deadline) {
    *reason = DDS_DEADLINE_QOS_POLICY_ID;
    return false;
  }
  if ((mask & DDSI_QP_LATENCY_BUDGET) && rd_qos->latency_budget.duration < wr_qos->latency_budget.duration) {
    *reason = DDS_LATENCYBUDGET_QOS_POLICY_ID;
    return false;
  }
  if ((mask & DDSI_QP_OWNERSHIP) && rd_qos->ownership.kind != wr_qos->ownership.kind) {
    *reason = DDS_OWNERSHIP_QOS_POLICY_ID;
    return false;
  }
  if ((mask & DDSI_QP_LIVELINESS) && rd_qos->liveliness.kind > wr_qos->liveliness.kind) {
    *reason = DDS_LIVELINESS_QOS_POLICY_ID;
    return false;
  }
  if ((mask & DDSI_QP_LIVELINESS) && rd_qos->liveliness.lease_duration < wr_qos->liveliness.lease_duration) {
    *reason = DDS_LIVELINESS_QOS_POLICY_ID;
    return false;
  }
  if ((mask & DDSI_QP_DESTINATION_ORDER) && rd_qos->destination_order.kind > wr_qos->destination_order.kind) {
    *reason = DDS_DESTINATIONORDER_QOS_POLICY_ID;
    return false;
  }
  if ((mask & DDSI_QP_PARTITION) && !partitions_match_p (rd_qos, wr_qos)) {
    *reason = DDS_PARTITION_QOS_POLICY_ID;
    return false;
  }
  if ((mask & DDSI_QP_DATA_REPRESENTATION) && !data_representation_match_p (rd_qos, wr_qos)) {
    *reason = DDS_DATA_REPRESENTATION_QOS_POLICY_ID;
    return false;
  }

#ifdef DDS_HAS_TYPE_DISCOVERY
  if (!type_pair_has_id (rd_type_pair) || !type_pair_has_id (wr_type_pair))
  {
    // Type info missing on either or both: automatic failure if "force type validation"
    // is set.  If it is missing for one, there is no point in requesting it for the
    // other (it wouldn't be inspected anyway).
    if (rd_qos->type_consistency.force_type_validation)
    {
      *reason = DDS_TYPE_CONSISTENCY_ENFORCEMENT_QOS_POLICY_ID;
      return false;
    }
    // If either the reader or writer does not provide a type id, the type names are consulted
    // (XTypes spec 7.6.3.4.2)
    if ((mask & DDSI_QP_TYPE_NAME) && strcmp (rd_qos->type_name, wr_qos->type_name) != 0)
      return false;
  }
  else
  {
    dds_type_consistency_enforcement_qospolicy_t tce = {
      .kind = DDS_TYPE_CONSISTENCY_ALLOW_TYPE_COERCION,
      .ignore_sequence_bounds = true,
      .ignore_string_bounds = true,
      .ignore_member_names = false,
      .prevent_type_widening = false,
      .force_type_validation = false
    };
    (void) dds_qget_type_consistency (rd_qos, &tce.kind, &tce.ignore_sequence_bounds, &tce.ignore_string_bounds, &tce.ignore_member_names, &tce.prevent_type_widening, &tce.force_type_validation);

    if (tce.kind == DDS_TYPE_CONSISTENCY_DISALLOW_TYPE_COERCION)
    {
      if (ddsi_typeid_compare (ddsi_type_pair_minimal_id (rd_type_pair), ddsi_type_pair_minimal_id (wr_type_pair)))
      {
        *reason = DDS_TYPE_CONSISTENCY_ENFORCEMENT_QOS_POLICY_ID;
        return false;
      }
    }
    else
    {
      uint32_t rd_resolved, wr_resolved;
      if (!(rd_resolved = is_endpoint_type_resolved (gv, rd_qos->type_name, rd_type_pair, rd_typeid_req_lookup, "rd"))
          || !(wr_resolved = is_endpoint_type_resolved (gv, wr_qos->type_name, wr_type_pair, wr_typeid_req_lookup, "wr")))
        return false;

      if (!ddsi_is_assignable_from (gv, rd_type_pair, rd_resolved, wr_type_pair, wr_resolved, &tce))
      {
        *reason = DDS_TYPE_CONSISTENCY_ENFORCEMENT_QOS_POLICY_ID;
        return false;
      }
    }
  }
#else
  if ((mask & DDSI_QP_TYPE_NAME) && strcmp (rd_qos->type_name, wr_qos->type_name) != 0)
    return false;
#endif

  return true;
}

bool ddsi_qos_match_p (
    struct ddsi_domaingv *gv,
    const dds_qos_t *rd_qos,
    const dds_qos_t *wr_qos,
    dds_qos_policy_id_t *reason
#ifdef DDS_HAS_TYPE_DISCOVERY
    , const struct ddsi_type_pair *rd_type_pair
    , const struct ddsi_type_pair *wr_type_pair
    , bool *rd_typeid_req_lookup
    , bool *wr_typeid_req_lookup
#endif
)
{
  dds_qos_policy_id_t dummy;
#ifdef DDS_HAS_TYPE_DISCOVERY
  return ddsi_qos_match_mask_p (gv, rd_qos, wr_qos, ~(uint64_t)0, reason ? reason : &dummy, rd_type_pair, wr_type_pair, rd_typeid_req_lookup, wr_typeid_req_lookup);
#else
  return ddsi_qos_match_mask_p (gv, rd_qos, wr_qos, ~(uint64_t)0, reason ? reason : &dummy);
#endif
}
