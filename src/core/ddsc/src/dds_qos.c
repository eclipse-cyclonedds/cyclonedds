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
#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "cyclonedds/dds.h"
#include "cyclonedds/ddsrt/heap.h"
#include "cyclonedds/ddsrt/string.h"
#include "cyclonedds/ddsi/q_plist.h"

static void dds_qos_data_copy_in (ddsi_octetseq_t *data, const void * __restrict value, size_t sz, bool overwrite)
{
  if (overwrite && data->value)
    ddsrt_free (data->value);
  data->length = (uint32_t) sz;
  data->value = value ? ddsrt_memdup (value, sz) : NULL;
}

static bool dds_qos_data_copy_out (const ddsi_octetseq_t *data, void **value, size_t *sz)
{
  assert (data->length < UINT32_MAX);
  if (sz == NULL && value != NULL)
    return false;

  if (sz)
    *sz = data->length;
  if (value)
  {
    if (data->length == 0)
      *value = NULL;
    else
    {
      assert (data->value);
      *value = dds_alloc (data->length + 1);
      memcpy (*value, data->value, data->length);
      ((char *) (*value))[data->length] = 0;
    }
  }
  return true;
}

dds_qos_t *dds_create_qos (void)
{
  dds_qos_t *qos = ddsrt_malloc (sizeof (dds_qos_t));
  nn_xqos_init_empty (qos);
  return qos;
}

dds_qos_t *dds_qos_create (void)
{
  return dds_create_qos ();
}

void dds_reset_qos (dds_qos_t * __restrict qos)
{
  if (qos)
  {
    nn_xqos_fini (qos);
    nn_xqos_init_empty (qos);
  }
}

void dds_qos_reset (dds_qos_t * __restrict qos)
{
  dds_reset_qos (qos);
}

void dds_delete_qos (dds_qos_t * __restrict qos)
{
  if (qos)
  {
    nn_xqos_fini (qos);
    ddsrt_free (qos);
  }
}

void dds_qos_delete (dds_qos_t * __restrict qos)
{
  dds_delete_qos (qos);
}

dds_return_t dds_copy_qos (dds_qos_t * __restrict dst, const dds_qos_t * __restrict src)
{
  if (src == NULL || dst == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  nn_xqos_copy (dst, src);
  return DDS_RETCODE_OK;
}

dds_return_t dds_qos_copy (dds_qos_t * __restrict dst, const dds_qos_t * __restrict src)
{
  return dds_copy_qos (dst, src);
}

void dds_merge_qos (dds_qos_t * __restrict dst, const dds_qos_t * __restrict src)
{
  /* Copy qos from source to destination unless already set */
  if (src != NULL && dst != NULL)
    nn_xqos_mergein_missing (dst, src, ~(uint64_t)0);
}

void dds_qos_merge (dds_qos_t * __restrict dst, const dds_qos_t * __restrict src)
{
  dds_merge_qos (dst, src);
}

bool dds_qos_equal (const dds_qos_t * __restrict a, const dds_qos_t * __restrict b)
{
  /* FIXME: a bit of a hack - and I am not so sure I like accepting null pointers here anyway */
  if (a == NULL && b == NULL)
    return true;
  else if (a == NULL || b == NULL)
    return false;
  else
    return nn_xqos_delta (a, b, ~(uint64_t)0) == 0;
}

void dds_qset_userdata (dds_qos_t * __restrict qos, const void * __restrict value, size_t sz)
{
  if (qos == NULL || (sz > 0 && value == NULL))
    return;
  dds_qos_data_copy_in (&qos->user_data, value, sz, qos->present & QP_USER_DATA);
  qos->present |= QP_USER_DATA;
}

void dds_qset_topicdata (dds_qos_t * __restrict qos, const void * __restrict value, size_t sz)
{
  if (qos == NULL || (sz > 0 && value == NULL))
    return;
  dds_qos_data_copy_in (&qos->topic_data, value, sz, qos->present & QP_TOPIC_DATA);
  qos->present |= QP_TOPIC_DATA;
}

void dds_qset_groupdata (dds_qos_t * __restrict qos, const void * __restrict value, size_t sz)
{
  if (qos == NULL || (sz > 0 && value == NULL))
    return;
  dds_qos_data_copy_in (&qos->group_data, value, sz, qos->present & QP_GROUP_DATA);
  qos->present |= QP_GROUP_DATA;
}

void dds_qset_durability (dds_qos_t * __restrict qos, dds_durability_kind_t kind)
{
  if (qos == NULL)
    return;
  qos->durability.kind = kind;
  qos->present |= QP_DURABILITY;
}

void dds_qset_history (dds_qos_t * __restrict qos, dds_history_kind_t kind, int32_t depth)
{
  if (qos == NULL)
    return;
  qos->history.kind = kind;
  qos->history.depth = depth;
  qos->present |= QP_HISTORY;
}

void dds_qset_resource_limits (dds_qos_t * __restrict qos, int32_t max_samples, int32_t max_instances, int32_t max_samples_per_instance)
{
  if (qos == NULL)
    return;
  qos->resource_limits.max_samples = max_samples;
  qos->resource_limits.max_instances = max_instances;
  qos->resource_limits.max_samples_per_instance = max_samples_per_instance;
  qos->present |= QP_RESOURCE_LIMITS;
}

void dds_qset_presentation (dds_qos_t * __restrict qos, dds_presentation_access_scope_kind_t access_scope, bool coherent_access, bool ordered_access)
{
  if (qos == NULL)
    return;
  qos->presentation.access_scope = access_scope;
  qos->presentation.coherent_access = coherent_access;
  qos->presentation.ordered_access = ordered_access;
  qos->present |= QP_PRESENTATION;
}

void dds_qset_lifespan (dds_qos_t * __restrict qos, dds_duration_t lifespan)
{
  if (qos == NULL)
    return;
  qos->lifespan.duration = lifespan;
  qos->present |= QP_LIFESPAN;
}

void dds_qset_deadline (dds_qos_t * __restrict qos, dds_duration_t deadline)
{
  if (qos == NULL)
    return;
  qos->deadline.deadline = deadline;
  qos->present |= QP_DEADLINE;
}

void dds_qset_latency_budget (dds_qos_t * __restrict qos, dds_duration_t duration)
{
  if (qos == NULL)
    return;
  qos->latency_budget.duration = duration;
  qos->present |= QP_LATENCY_BUDGET;
}

void dds_qset_ownership (dds_qos_t * __restrict qos, dds_ownership_kind_t kind)
{
  if (qos == NULL)
    return;
  qos->ownership.kind = kind;
  qos->present |= QP_OWNERSHIP;
}

void dds_qset_ownership_strength (dds_qos_t * __restrict qos, int32_t value)
{
  if (qos == NULL)
    return;
  qos->ownership_strength.value = value;
  qos->present |= QP_OWNERSHIP_STRENGTH;
}

void dds_qset_liveliness (dds_qos_t * __restrict qos, dds_liveliness_kind_t kind, dds_duration_t lease_duration)
{
  if (qos == NULL)
    return;
  qos->liveliness.kind = kind;
  qos->liveliness.lease_duration = lease_duration;
  qos->present |= QP_LIVELINESS;
}

void dds_qset_time_based_filter (dds_qos_t * __restrict qos, dds_duration_t minimum_separation)
{
  if (qos == NULL)
    return;
  qos->time_based_filter.minimum_separation = minimum_separation;
  qos->present |= QP_TIME_BASED_FILTER;
}

void dds_qset_partition (dds_qos_t * __restrict qos, uint32_t n, const char ** __restrict ps)
{
  if (qos == NULL || (n > 0 && ps == NULL))
    return;
  if (qos->present & QP_PARTITION)
  {
    for (uint32_t i = 0; i < qos->partition.n; i++)
      ddsrt_free (qos->partition.strs[i]);
    ddsrt_free (qos->partition.strs);
  }
  qos->partition.n = n;
  if (qos->partition.n == 0)
    qos->partition.strs = NULL;
  else
  {
    qos->partition.strs = ddsrt_malloc (n * sizeof (*qos->partition.strs));
    for (uint32_t i = 0; i < n; i++)
      qos->partition.strs[i] = ddsrt_strdup (ps[i]);
  }
  qos->present |= QP_PARTITION;
}

void dds_qset_partition1 (dds_qos_t * __restrict qos, const char * __restrict name)
{
  if (name == NULL)
    dds_qset_partition (qos, 0, NULL);
  else
    dds_qset_partition (qos, 1, (const char **) &name);
}

void dds_qset_reliability (dds_qos_t * __restrict qos, dds_reliability_kind_t kind, dds_duration_t max_blocking_time)
{
  if (qos == NULL)
    return;
  qos->reliability.kind = kind;
  qos->reliability.max_blocking_time = max_blocking_time;
  qos->present |= QP_RELIABILITY;
}

void dds_qset_transport_priority (dds_qos_t * __restrict qos, int32_t value)
{
  if (qos == NULL)
    return;
  qos->transport_priority.value = value;
  qos->present |= QP_TRANSPORT_PRIORITY;
}

void dds_qset_destination_order (dds_qos_t * __restrict qos, dds_destination_order_kind_t kind)
{
  if (qos == NULL)
    return;
  qos->destination_order.kind = kind;
  qos->present |= QP_DESTINATION_ORDER;
}

void dds_qset_writer_data_lifecycle (dds_qos_t * __restrict qos, bool autodispose)
{
  if (qos == NULL)
    return;
  qos->writer_data_lifecycle.autodispose_unregistered_instances = autodispose;
  qos->present |= QP_PRISMTECH_WRITER_DATA_LIFECYCLE;
}

void dds_qset_reader_data_lifecycle (dds_qos_t * __restrict qos, dds_duration_t autopurge_nowriter_samples_delay, dds_duration_t autopurge_disposed_samples_delay)
{
  if (qos == NULL)
    return;
  qos->reader_data_lifecycle.autopurge_nowriter_samples_delay = autopurge_nowriter_samples_delay;
  qos->reader_data_lifecycle.autopurge_disposed_samples_delay = autopurge_disposed_samples_delay;
  qos->present |= QP_PRISMTECH_READER_DATA_LIFECYCLE;
}

void dds_qset_durability_service (dds_qos_t * __restrict qos, dds_duration_t service_cleanup_delay, dds_history_kind_t history_kind, int32_t history_depth, int32_t max_samples, int32_t max_instances, int32_t max_samples_per_instance)
{
  if (qos == NULL)
    return;
  qos->durability_service.service_cleanup_delay = service_cleanup_delay;
  qos->durability_service.history.kind = history_kind;
  qos->durability_service.history.depth = history_depth;
  qos->durability_service.resource_limits.max_samples = max_samples;
  qos->durability_service.resource_limits.max_instances = max_instances;
  qos->durability_service.resource_limits.max_samples_per_instance = max_samples_per_instance;
  qos->present |= QP_DURABILITY_SERVICE;
}

void dds_qset_ignorelocal (dds_qos_t * __restrict qos, dds_ignorelocal_kind_t ignore)
{
  if (qos == NULL)
    return;
  qos->ignorelocal.value = ignore;
  qos->present |= QP_CYCLONE_IGNORELOCAL;
}

bool dds_qget_userdata (const dds_qos_t * __restrict qos, void **value, size_t *sz)
{
  if (qos == NULL || !(qos->present & QP_USER_DATA))
    return false;
  return dds_qos_data_copy_out (&qos->user_data, value, sz);
}

bool dds_qget_topicdata (const dds_qos_t * __restrict qos, void **value, size_t *sz)
{
  if (qos == NULL || !(qos->present & QP_TOPIC_DATA))
    return false;
  return dds_qos_data_copy_out (&qos->topic_data, value, sz);
}

bool dds_qget_groupdata (const dds_qos_t * __restrict qos, void **value, size_t *sz)
{
  if (qos == NULL || !(qos->present & QP_GROUP_DATA))
    return false;
  return dds_qos_data_copy_out (&qos->group_data, value, sz);
}

bool dds_qget_durability (const dds_qos_t * __restrict qos, dds_durability_kind_t *kind)
{
  if (qos == NULL || !(qos->present & QP_DURABILITY))
    return false;
  if (kind)
    *kind = qos->durability.kind;
  return true;
}

bool dds_qget_history (const dds_qos_t * __restrict qos, dds_history_kind_t *kind, int32_t *depth)
{
  if (qos == NULL || !(qos->present & QP_HISTORY))
    return false;
  if (kind)
    *kind = qos->history.kind;
  if (depth)
    *depth = qos->history.depth;
  return true;
}

bool dds_qget_resource_limits (const dds_qos_t * __restrict qos, int32_t *max_samples, int32_t *max_instances, int32_t *max_samples_per_instance)
{
  if (qos == NULL || !(qos->present & QP_RESOURCE_LIMITS))
    return false;
  if (max_samples)
    *max_samples = qos->resource_limits.max_samples;
  if (max_instances)
    *max_instances = qos->resource_limits.max_instances;
  if (max_samples_per_instance)
    *max_samples_per_instance = qos->resource_limits.max_samples_per_instance;
  return true;
}

bool dds_qget_presentation (const dds_qos_t * __restrict qos, dds_presentation_access_scope_kind_t *access_scope, bool *coherent_access, bool *ordered_access)
{
  if (qos == NULL || !(qos->present & QP_PRESENTATION))
    return false;
  if (access_scope)
    *access_scope = qos->presentation.access_scope;
  if (coherent_access)
    *coherent_access = qos->presentation.coherent_access;
  if (ordered_access)
    *ordered_access = qos->presentation.ordered_access;
  return true;
}

bool dds_qget_lifespan (const dds_qos_t * __restrict qos, dds_duration_t *lifespan)
{
  if (qos == NULL || !(qos->present & QP_LIFESPAN))
    return false;
  if (lifespan)
    *lifespan = qos->lifespan.duration;
  return true;
}

bool dds_qget_deadline (const dds_qos_t * __restrict qos, dds_duration_t *deadline)
{
  if (qos == NULL || !(qos->present & QP_DEADLINE))
    return false;
  if (deadline)
    *deadline = qos->deadline.deadline;
  return true;
}

bool dds_qget_latency_budget (const dds_qos_t * __restrict qos, dds_duration_t *duration)
{
  if (qos == NULL || !(qos->present & QP_LATENCY_BUDGET))
    return false;
  if (duration)
    *duration = qos->latency_budget.duration;
  return true;
}

bool dds_qget_ownership (const dds_qos_t * __restrict qos, dds_ownership_kind_t *kind)
{
  if (qos == NULL || !(qos->present & QP_OWNERSHIP))
    return false;
  if (kind)
    *kind = qos->ownership.kind;
  return true;
}

bool dds_qget_ownership_strength (const dds_qos_t * __restrict qos, int32_t *value)
{
  if (qos == NULL || !(qos->present & QP_OWNERSHIP_STRENGTH))
    return false;
  if (value)
    *value = qos->ownership_strength.value;
  return true;
}

bool dds_qget_liveliness (const dds_qos_t * __restrict qos, dds_liveliness_kind_t *kind, dds_duration_t *lease_duration)
{
  if (qos == NULL || !(qos->present & QP_LIVELINESS))
    return false;
  if (kind)
    *kind = qos->liveliness.kind;
  if (lease_duration)
    *lease_duration = qos->liveliness.lease_duration;
  return true;
}

bool dds_qget_time_based_filter (const dds_qos_t * __restrict qos, dds_duration_t *minimum_separation)
{
  if (qos == NULL || !(qos->present & QP_TIME_BASED_FILTER))
    return false;
  if (minimum_separation)
    *minimum_separation = qos->time_based_filter.minimum_separation;
  return true;
}

bool dds_qget_partition (const dds_qos_t * __restrict qos, uint32_t *n, char ***ps)
{
  if (qos == NULL || !(qos->present & QP_PARTITION))
    return false;
  if (n == NULL && ps != NULL)
    return false;
  if (n)
    *n = qos->partition.n;
  if (ps)
  {
    if (qos->partition.n == 0)
      *ps = NULL;
    else
    {
      *ps = dds_alloc (sizeof (char*) * qos->partition.n);
      for (uint32_t i = 0; i < qos->partition.n; i++)
        (*ps)[i] = dds_string_dup (qos->partition.strs[i]);
    }
  }
  return true;
}

bool dds_qget_reliability (const dds_qos_t * __restrict qos, dds_reliability_kind_t *kind, dds_duration_t *max_blocking_time)
{
  if (qos == NULL || !(qos->present & QP_RELIABILITY))
    return false;
  if (kind)
    *kind = qos->reliability.kind;
  if (max_blocking_time)
    *max_blocking_time = qos->reliability.max_blocking_time;
  return true;
}

bool dds_qget_transport_priority (const dds_qos_t * __restrict qos, int32_t *value)
{
  if (qos == NULL || !(qos->present & QP_TRANSPORT_PRIORITY))
    return false;
  if (value)
    *value = qos->transport_priority.value;
  return true;
}

bool dds_qget_destination_order (const dds_qos_t * __restrict qos, dds_destination_order_kind_t *kind)
{
  if (qos == NULL || !(qos->present & QP_DESTINATION_ORDER))
    return false;
  if (kind)
    *kind = qos->destination_order.kind;
  return true;
}

bool dds_qget_writer_data_lifecycle (const dds_qos_t * __restrict qos, bool *autodispose)
{
  if (qos == NULL || !(qos->present & QP_PRISMTECH_WRITER_DATA_LIFECYCLE))
    return false;
  if (autodispose)
    *autodispose = qos->writer_data_lifecycle.autodispose_unregistered_instances;
  return true;
}

bool dds_qget_reader_data_lifecycle (const dds_qos_t * __restrict qos, dds_duration_t *autopurge_nowriter_samples_delay, dds_duration_t *autopurge_disposed_samples_delay)
{
  if (qos == NULL || !(qos->present & QP_PRISMTECH_READER_DATA_LIFECYCLE))
    return false;
  if (autopurge_nowriter_samples_delay)
    *autopurge_nowriter_samples_delay = qos->reader_data_lifecycle.autopurge_nowriter_samples_delay;
  if (autopurge_disposed_samples_delay)
    *autopurge_disposed_samples_delay = qos->reader_data_lifecycle.autopurge_disposed_samples_delay;
  return true;
}

bool dds_qget_durability_service (const dds_qos_t * __restrict qos, dds_duration_t *service_cleanup_delay, dds_history_kind_t *history_kind, int32_t *history_depth, int32_t *max_samples, int32_t *max_instances, int32_t *max_samples_per_instance)
{
  if (qos == NULL || !(qos->present & QP_DURABILITY_SERVICE))
    return false;
  if (service_cleanup_delay)
    *service_cleanup_delay = qos->durability_service.service_cleanup_delay;
  if (history_kind)
    *history_kind = qos->durability_service.history.kind;
  if (history_depth)
    *history_depth = qos->durability_service.history.depth;
  if (max_samples)
    *max_samples = qos->durability_service.resource_limits.max_samples;
  if (max_instances)
    *max_instances = qos->durability_service.resource_limits.max_instances;
  if (max_samples_per_instance)
    *max_samples_per_instance = qos->durability_service.resource_limits.max_samples_per_instance;
  return true;
}

bool dds_qget_ignorelocal (const dds_qos_t * __restrict qos, dds_ignorelocal_kind_t *ignore)
{
  if (qos == NULL || !(qos->present & QP_CYCLONE_IGNORELOCAL))
    return false;
  if (ignore)
    *ignore = qos->ignorelocal.value;
  return true;
}
