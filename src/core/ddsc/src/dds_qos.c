// Copyright(c) 2006 to 2022 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>
#include <stdbool.h>
#include "dds/dds.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_plist.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds__qos.h"

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
  ddsi_xqos_init_empty (qos);
  return qos;
}

void dds_reset_qos (dds_qos_t * __restrict qos)
{
  if (qos)
  {
    ddsi_xqos_fini (qos);
    ddsi_xqos_init_empty (qos);
  }
}

void dds_delete_qos (dds_qos_t * __restrict qos)
{
  if (qos)
  {
    ddsi_xqos_fini (qos);
    ddsrt_free (qos);
  }
}

dds_return_t dds_copy_qos (dds_qos_t * __restrict dst, const dds_qos_t * __restrict src)
{
  if (src == NULL || dst == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  ddsi_xqos_copy (dst, src);
  return DDS_RETCODE_OK;
}

void dds_merge_qos (dds_qos_t * __restrict dst, const dds_qos_t * __restrict src)
{
  /* Copy qos from source to destination unless already set */
  if (src != NULL && dst != NULL)
    ddsi_xqos_mergein_missing (dst, src, ~(uint64_t)0);
}

bool dds_qos_equal (const dds_qos_t * __restrict a, const dds_qos_t * __restrict b)
{
  /* FIXME: a bit of a hack - and I am not so sure I like accepting null pointers here anyway */
  if (a == NULL && b == NULL)
    return true;
  else if (a == NULL || b == NULL)
    return false;
  else
    return ddsi_xqos_delta (a, b, ~(DDSI_QP_TYPE_INFORMATION)) == 0;
}

void dds_qset_userdata (dds_qos_t * __restrict qos, const void * __restrict value, size_t sz)
{
  if (qos == NULL || (sz > 0 && value == NULL))
    return;
  dds_qos_data_copy_in (&qos->user_data, value, sz, qos->present & DDSI_QP_USER_DATA);
  qos->present |= DDSI_QP_USER_DATA;
}

void dds_qset_topicdata (dds_qos_t * __restrict qos, const void * __restrict value, size_t sz)
{
  if (qos == NULL || (sz > 0 && value == NULL))
    return;
  dds_qos_data_copy_in (&qos->topic_data, value, sz, qos->present & DDSI_QP_TOPIC_DATA);
  qos->present |= DDSI_QP_TOPIC_DATA;
}

void dds_qset_groupdata (dds_qos_t * __restrict qos, const void * __restrict value, size_t sz)
{
  if (qos == NULL || (sz > 0 && value == NULL))
    return;
  dds_qos_data_copy_in (&qos->group_data, value, sz, qos->present & DDSI_QP_GROUP_DATA);
  qos->present |= DDSI_QP_GROUP_DATA;
}

void dds_qset_durability (dds_qos_t * __restrict qos, dds_durability_kind_t kind)
{
  if (qos == NULL)
    return;
  qos->durability.kind = kind;
  qos->present |= DDSI_QP_DURABILITY;
}

void dds_qset_history (dds_qos_t * __restrict qos, dds_history_kind_t kind, int32_t depth)
{
  if (qos == NULL)
    return;
  qos->history.kind = kind;
  qos->history.depth = depth;
  qos->present |= DDSI_QP_HISTORY;
}

void dds_qset_resource_limits (dds_qos_t * __restrict qos, int32_t max_samples, int32_t max_instances, int32_t max_samples_per_instance)
{
  if (qos == NULL)
    return;
  qos->resource_limits.max_samples = max_samples;
  qos->resource_limits.max_instances = max_instances;
  qos->resource_limits.max_samples_per_instance = max_samples_per_instance;
  qos->present |= DDSI_QP_RESOURCE_LIMITS;
}

void dds_qset_presentation (dds_qos_t * __restrict qos, dds_presentation_access_scope_kind_t access_scope, bool coherent_access, bool ordered_access)
{
  if (qos == NULL)
    return;
  qos->presentation.access_scope = access_scope;
  qos->presentation.coherent_access = coherent_access;
  qos->presentation.ordered_access = ordered_access;
  qos->present |= DDSI_QP_PRESENTATION;
}

void dds_qset_lifespan (dds_qos_t * __restrict qos, dds_duration_t lifespan)
{
  if (qos == NULL)
    return;
  qos->lifespan.duration = lifespan;
  qos->present |= DDSI_QP_LIFESPAN;
}

void dds_qset_deadline (dds_qos_t * __restrict qos, dds_duration_t deadline)
{
  if (qos == NULL)
    return;
  qos->deadline.deadline = deadline;
  qos->present |= DDSI_QP_DEADLINE;
}

void dds_qset_latency_budget (dds_qos_t * __restrict qos, dds_duration_t duration)
{
  if (qos == NULL)
    return;
  qos->latency_budget.duration = duration;
  qos->present |= DDSI_QP_LATENCY_BUDGET;
}

void dds_qset_ownership (dds_qos_t * __restrict qos, dds_ownership_kind_t kind)
{
  if (qos == NULL)
    return;
  qos->ownership.kind = kind;
  qos->present |= DDSI_QP_OWNERSHIP;
}

void dds_qset_ownership_strength (dds_qos_t * __restrict qos, int32_t value)
{
  if (qos == NULL)
    return;
  qos->ownership_strength.value = value;
  qos->present |= DDSI_QP_OWNERSHIP_STRENGTH;
}

void dds_qset_liveliness (dds_qos_t * __restrict qos, dds_liveliness_kind_t kind, dds_duration_t lease_duration)
{
  if (qos == NULL)
    return;
  qos->liveliness.kind = kind;
  qos->liveliness.lease_duration = lease_duration;
  qos->present |= DDSI_QP_LIVELINESS;
}

void dds_qset_time_based_filter (dds_qos_t * __restrict qos, dds_duration_t minimum_separation)
{
  if (qos == NULL)
    return;
  qos->time_based_filter.minimum_separation = minimum_separation;
  qos->present |= DDSI_QP_TIME_BASED_FILTER;
}

void dds_qset_partition (dds_qos_t * __restrict qos, uint32_t n, const char ** __restrict ps)
{
  if (qos == NULL || (n > 0 && ps == NULL))
    return;
  if (qos->present & DDSI_QP_PARTITION)
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
  qos->present |= DDSI_QP_PARTITION;
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
  qos->present |= DDSI_QP_RELIABILITY;
}

void dds_qset_transport_priority (dds_qos_t * __restrict qos, int32_t value)
{
  if (qos == NULL)
    return;
  qos->transport_priority.value = value;
  qos->present |= DDSI_QP_TRANSPORT_PRIORITY;
}

void dds_qset_destination_order (dds_qos_t * __restrict qos, dds_destination_order_kind_t kind)
{
  if (qos == NULL)
    return;
  qos->destination_order.kind = kind;
  qos->present |= DDSI_QP_DESTINATION_ORDER;
}

void dds_qset_writer_data_lifecycle (dds_qos_t * __restrict qos, bool autodispose)
{
  if (qos == NULL)
    return;
  qos->writer_data_lifecycle.autodispose_unregistered_instances = autodispose;
  qos->present |= DDSI_QP_ADLINK_WRITER_DATA_LIFECYCLE;
}

void dds_qset_reader_data_lifecycle (dds_qos_t * __restrict qos, dds_duration_t autopurge_nowriter_samples_delay, dds_duration_t autopurge_disposed_samples_delay)
{
  if (qos == NULL)
    return;
  qos->reader_data_lifecycle.autopurge_nowriter_samples_delay = autopurge_nowriter_samples_delay;
  qos->reader_data_lifecycle.autopurge_disposed_samples_delay = autopurge_disposed_samples_delay;
  qos->present |= DDSI_QP_ADLINK_READER_DATA_LIFECYCLE;
}

void dds_qset_writer_batching (dds_qos_t * __restrict qos, bool batch_updates)
{
  if (qos == NULL)
    return;
  qos->writer_batching.batch_updates = batch_updates;
  qos->present |= DDSI_QP_CYCLONE_WRITER_BATCHING;
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
  qos->present |= DDSI_QP_DURABILITY_SERVICE;
}

void dds_qset_ignorelocal (dds_qos_t * __restrict qos, dds_ignorelocal_kind_t ignore)
{
  if (qos == NULL)
    return;
  qos->ignorelocal.value = ignore;
  qos->present |= DDSI_QP_CYCLONE_IGNORELOCAL;
}

static void dds_qprop_init (dds_qos_t * qos)
{
  if (!(qos->present & DDSI_QP_PROPERTY_LIST))
  {
    qos->property.value.n = 0;
    qos->property.value.props = NULL;
    qos->property.binary_value.n = 0;
    qos->property.binary_value.props = NULL;
    qos->present |= DDSI_QP_PROPERTY_LIST;
  }
}

#define DDS_QPROP_GET_INDEX(prop_type_, prop_field_) \
static bool dds_q##prop_type_##_get_index (const dds_qos_t * qos, const char * name, uint32_t * index) \
{ \
  if (qos == NULL || name == NULL || index == NULL || !(qos->present & DDSI_QP_PROPERTY_LIST)) \
    return false; \
  for (uint32_t i = 0; i < qos->property.prop_field_.n; i++) \
  { \
    if (strcmp (qos->property.prop_field_.props[i].name, name) == 0) \
    { \
      *index = i; \
      return true; \
    } \
  } \
  return false; \
}
DDS_QPROP_GET_INDEX (prop, value)
DDS_QPROP_GET_INDEX (bprop, binary_value)

#define DDS_QUNSET_PROP(prop_type_, prop_field_, value_field_) \
void dds_qunset_##prop_type_ (dds_qos_t * __restrict qos, const char * name) \
{ \
  uint32_t i; \
  if (qos == NULL || !(qos->present & DDSI_QP_PROPERTY_LIST) || qos->property.prop_field_.n == 0 || name == NULL) \
    return; \
  if (dds_q##prop_type_##_get_index (qos, name, &i)) \
  { \
    dds_free (qos->property.prop_field_.props[i].name); \
    dds_free (qos->property.prop_field_.props[i].value_field_); \
    if (qos->property.prop_field_.n > 1) \
    { \
      if (i < (qos->property.prop_field_.n - 1)) \
        memmove (qos->property.prop_field_.props + i, qos->property.prop_field_.props + i + 1, \
          (qos->property.prop_field_.n - i - 1) * sizeof (*qos->property.prop_field_.props)); \
      qos->property.prop_field_.props = dds_realloc (qos->property.prop_field_.props, \
        (qos->property.prop_field_.n - 1) * sizeof (*qos->property.prop_field_.props)); \
    } \
    else \
    { \
      dds_free (qos->property.prop_field_.props); \
      qos->property.prop_field_.props = NULL; \
    } \
    qos->property.prop_field_.n--; \
  } \
}
DDS_QUNSET_PROP (prop, value, value)
DDS_QUNSET_PROP (bprop, binary_value, value.value)

void dds_qset_prop (dds_qos_t * __restrict qos, const char * name, const char * value)
{
  uint32_t i;
  if (qos == NULL || name == NULL || value == NULL)
    return;

  dds_qprop_init (qos);
  if (dds_qprop_get_index (qos, name, &i))
  {
    assert (&qos->property.value.props[i] != NULL); /* for Clang static analyzer */
    dds_free (qos->property.value.props[i].value);
    qos->property.value.props[i].value = dds_string_dup (value);
  }
  else
  {
    qos->property.value.props = dds_realloc (qos->property.value.props,
      (qos->property.value.n + 1) * sizeof (*qos->property.value.props));
    qos->property.value.props[qos->property.value.n].propagate = 0;
    qos->property.value.props[qos->property.value.n].name = dds_string_dup (name);
    qos->property.value.props[qos->property.value.n].value = dds_string_dup (value);
    qos->property.value.n++;
  }
}

void dds_qset_bprop (dds_qos_t * __restrict qos, const char * name, const void * value, const size_t sz)
{
  uint32_t i;
  if (qos == NULL || name == NULL || (value == NULL && sz > 0))
    return;

  dds_qprop_init (qos);
  if (dds_qbprop_get_index (qos, name, &i))
  {
    assert (&qos->property.binary_value.props[i].value != NULL); /* for Clang static analyzer */
    dds_qos_data_copy_in (&qos->property.binary_value.props[i].value, value, sz, true);
  }
  else
  {
    qos->property.binary_value.props = dds_realloc (qos->property.binary_value.props,
      (qos->property.binary_value.n + 1) * sizeof (*qos->property.binary_value.props));
    qos->property.binary_value.props[qos->property.binary_value.n].propagate = 0;
    qos->property.binary_value.props[qos->property.binary_value.n].name = dds_string_dup (name);
    dds_qos_data_copy_in (&qos->property.binary_value.props[qos->property.binary_value.n].value, value, sz, false);
    qos->property.binary_value.n++;
  }
}

void dds_qset_entity_name (dds_qos_t * __restrict qos, const char * name)
{
  if (qos == NULL || name == NULL)
    return;
  qos->entity_name = dds_string_dup(name);
  qos->present |= DDSI_QP_ENTITY_NAME;
}

void dds_qset_type_consistency (dds_qos_t * __restrict qos, dds_type_consistency_kind_t kind,
  bool ignore_sequence_bounds, bool ignore_string_bounds, bool ignore_member_names, bool prevent_type_widening, bool force_type_validation)
{
  if (qos == NULL)
    return;
  qos->type_consistency.kind = kind;
  qos->type_consistency.ignore_sequence_bounds = ignore_sequence_bounds;
  qos->type_consistency.ignore_string_bounds = ignore_string_bounds;
  qos->type_consistency.ignore_member_names = ignore_member_names;
  qos->type_consistency.prevent_type_widening = prevent_type_widening;
  qos->type_consistency.force_type_validation = force_type_validation;
  qos->present |= DDSI_QP_TYPE_CONSISTENCY_ENFORCEMENT;
}

void dds_qset_data_representation (dds_qos_t * __restrict qos, uint32_t n, const dds_data_representation_id_t *values)
{
  if (qos == NULL || (n && !values))
    return;
  if ((qos->present & DDSI_QP_DATA_REPRESENTATION) && qos->data_representation.value.ids != NULL)
    ddsrt_free (qos->data_representation.value.ids);
  qos->data_representation.value.n = 0;
  qos->data_representation.value.ids = NULL;

  /* De-duplicate the provided list of data representation identifiers. The re-alloc
     approach is rather inefficient, but not really a problem because the list will
     typically have a very limited number of values */
  for (uint32_t x = 0; x < n; x++)
  {
    bool duplicate = false;
    for (uint32_t c = 0; !duplicate && c < x; c++)
      if (qos->data_representation.value.ids[c] == values[x])
        duplicate = true;
    if (!duplicate)
    {
      qos->data_representation.value.n++;
      qos->data_representation.value.ids = dds_realloc (qos->data_representation.value.ids, qos->data_representation.value.n * sizeof (*qos->data_representation.value.ids));
      qos->data_representation.value.ids[qos->data_representation.value.n - 1] = values[x];
    }
  }
  qos->present |= DDSI_QP_DATA_REPRESENTATION;
}

bool dds_qget_userdata (const dds_qos_t * __restrict qos, void **value, size_t *sz)
{
  if (qos == NULL || !(qos->present & DDSI_QP_USER_DATA))
    return false;
  return dds_qos_data_copy_out (&qos->user_data, value, sz);
}

bool dds_qget_topicdata (const dds_qos_t * __restrict qos, void **value, size_t *sz)
{
  if (qos == NULL || !(qos->present & DDSI_QP_TOPIC_DATA))
    return false;
  return dds_qos_data_copy_out (&qos->topic_data, value, sz);
}

bool dds_qget_groupdata (const dds_qos_t * __restrict qos, void **value, size_t *sz)
{
  if (qos == NULL || !(qos->present & DDSI_QP_GROUP_DATA))
    return false;
  return dds_qos_data_copy_out (&qos->group_data, value, sz);
}

bool dds_qget_durability (const dds_qos_t * __restrict qos, dds_durability_kind_t *kind)
{
  if (qos == NULL || !(qos->present & DDSI_QP_DURABILITY))
    return false;
  if (kind)
    *kind = qos->durability.kind;
  return true;
}

bool dds_qget_history (const dds_qos_t * __restrict qos, dds_history_kind_t *kind, int32_t *depth)
{
  if (qos == NULL || !(qos->present & DDSI_QP_HISTORY))
    return false;
  if (kind)
    *kind = qos->history.kind;
  if (depth)
    *depth = qos->history.depth;
  return true;
}

bool dds_qget_resource_limits (const dds_qos_t * __restrict qos, int32_t *max_samples, int32_t *max_instances, int32_t *max_samples_per_instance)
{
  if (qos == NULL || !(qos->present & DDSI_QP_RESOURCE_LIMITS))
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
  if (qos == NULL || !(qos->present & DDSI_QP_PRESENTATION))
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
  if (qos == NULL || !(qos->present & DDSI_QP_LIFESPAN))
    return false;
  if (lifespan)
    *lifespan = qos->lifespan.duration;
  return true;
}

bool dds_qget_deadline (const dds_qos_t * __restrict qos, dds_duration_t *deadline)
{
  if (qos == NULL || !(qos->present & DDSI_QP_DEADLINE))
    return false;
  if (deadline)
    *deadline = qos->deadline.deadline;
  return true;
}

bool dds_qget_latency_budget (const dds_qos_t * __restrict qos, dds_duration_t *duration)
{
  if (qos == NULL || !(qos->present & DDSI_QP_LATENCY_BUDGET))
    return false;
  if (duration)
    *duration = qos->latency_budget.duration;
  return true;
}

bool dds_qget_ownership (const dds_qos_t * __restrict qos, dds_ownership_kind_t *kind)
{
  if (qos == NULL || !(qos->present & DDSI_QP_OWNERSHIP))
    return false;
  if (kind)
    *kind = qos->ownership.kind;
  return true;
}

bool dds_qget_ownership_strength (const dds_qos_t * __restrict qos, int32_t *value)
{
  if (qos == NULL || !(qos->present & DDSI_QP_OWNERSHIP_STRENGTH))
    return false;
  if (value)
    *value = qos->ownership_strength.value;
  return true;
}

bool dds_qget_liveliness (const dds_qos_t * __restrict qos, dds_liveliness_kind_t *kind, dds_duration_t *lease_duration)
{
  if (qos == NULL || !(qos->present & DDSI_QP_LIVELINESS))
    return false;
  if (kind)
    *kind = qos->liveliness.kind;
  if (lease_duration)
    *lease_duration = qos->liveliness.lease_duration;
  return true;
}

bool dds_qget_time_based_filter (const dds_qos_t * __restrict qos, dds_duration_t *minimum_separation)
{
  if (qos == NULL || !(qos->present & DDSI_QP_TIME_BASED_FILTER))
    return false;
  if (minimum_separation)
    *minimum_separation = qos->time_based_filter.minimum_separation;
  return true;
}

bool dds_qget_partition (const dds_qos_t * __restrict qos, uint32_t *n, char ***ps)
{
  if (qos == NULL || !(qos->present & DDSI_QP_PARTITION))
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
  if (qos == NULL || !(qos->present & DDSI_QP_RELIABILITY))
    return false;
  if (kind)
    *kind = qos->reliability.kind;
  if (max_blocking_time)
    *max_blocking_time = qos->reliability.max_blocking_time;
  return true;
}

bool dds_qget_transport_priority (const dds_qos_t * __restrict qos, int32_t *value)
{
  if (qos == NULL || !(qos->present & DDSI_QP_TRANSPORT_PRIORITY))
    return false;
  if (value)
    *value = qos->transport_priority.value;
  return true;
}

bool dds_qget_destination_order (const dds_qos_t * __restrict qos, dds_destination_order_kind_t *kind)
{
  if (qos == NULL || !(qos->present & DDSI_QP_DESTINATION_ORDER))
    return false;
  if (kind)
    *kind = qos->destination_order.kind;
  return true;
}

bool dds_qget_writer_data_lifecycle (const dds_qos_t * __restrict qos, bool *autodispose)
{
  if (qos == NULL || !(qos->present & DDSI_QP_ADLINK_WRITER_DATA_LIFECYCLE))
    return false;
  if (autodispose)
    *autodispose = qos->writer_data_lifecycle.autodispose_unregistered_instances;
  return true;
}

bool dds_qget_reader_data_lifecycle (const dds_qos_t * __restrict qos, dds_duration_t *autopurge_nowriter_samples_delay, dds_duration_t *autopurge_disposed_samples_delay)
{
  if (qos == NULL || !(qos->present & DDSI_QP_ADLINK_READER_DATA_LIFECYCLE))
    return false;
  if (autopurge_nowriter_samples_delay)
    *autopurge_nowriter_samples_delay = qos->reader_data_lifecycle.autopurge_nowriter_samples_delay;
  if (autopurge_disposed_samples_delay)
    *autopurge_disposed_samples_delay = qos->reader_data_lifecycle.autopurge_disposed_samples_delay;
  return true;
}

bool dds_qget_writer_batching (const dds_qos_t * __restrict qos, bool *batch_updates)
{
  if (qos == NULL || !(qos->present & DDSI_QP_CYCLONE_WRITER_BATCHING))
    return false;
  if (batch_updates)
    *batch_updates = qos->writer_batching.batch_updates;
  return true;
}

bool dds_qget_durability_service (const dds_qos_t * __restrict qos, dds_duration_t *service_cleanup_delay, dds_history_kind_t *history_kind, int32_t *history_depth, int32_t *max_samples, int32_t *max_instances, int32_t *max_samples_per_instance)
{
  if (qos == NULL || !(qos->present & DDSI_QP_DURABILITY_SERVICE))
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
  if (qos == NULL || !(qos->present & DDSI_QP_CYCLONE_IGNORELOCAL))
    return false;
  if (ignore)
    *ignore = qos->ignorelocal.value;
  return true;
}

#define DDS_QGET_PROPNAMES(prop_type_, prop_field_) \
bool dds_qget_##prop_type_##names (const dds_qos_t * __restrict qos, uint32_t * n, char *** names) \
{ \
  bool props; \
  if (qos == NULL || (n == NULL && names == NULL)) \
    return false; \
  props = (qos->present & DDSI_QP_PROPERTY_LIST) && qos->property.prop_field_.n > 0; \
  if (n != NULL) \
    *n = props ? qos->property.prop_field_.n : 0; \
  if (names != NULL) \
  { \
    if (!props) \
      *names = NULL; \
    else \
    { \
      *names = dds_alloc (sizeof (char *) * qos->property.prop_field_.n); \
      for (uint32_t i = 0; i < qos->property.prop_field_.n; i++) \
        (*names)[i] = dds_string_dup (qos->property.prop_field_.props[i].name); \
    } \
  } \
  return props; \
}
DDS_QGET_PROPNAMES (prop, value)
DDS_QGET_PROPNAMES (bprop, binary_value)

bool dds_qget_prop (const dds_qos_t * __restrict qos, const char * name, char ** value)
{
  uint32_t i;
  bool found;

  if (qos == NULL || name == NULL)
    return false;

  found = dds_qprop_get_index (qos, name, &i);
  if (value != NULL)
    *value = found ? dds_string_dup (qos->property.value.props[i].value) : NULL;
  return found;
}

bool dds_qget_bprop (const dds_qos_t * __restrict qos, const char * name, void ** value, size_t * sz)
{
  uint32_t i;
  bool found;

  if (qos == NULL || name == NULL || (sz == NULL && value != NULL))
    return false;

  found = dds_qbprop_get_index (qos, name, &i);
  if (found)
  {
    if (value != NULL || sz != NULL)
      dds_qos_data_copy_out (&qos->property.binary_value.props[i].value, value, sz);
  }
  else
  {
    if (value != NULL)
      *value = NULL;
    if (sz != NULL)
      *sz = 0;
  }
  return found;
}

bool dds_qget_type_consistency (const dds_qos_t * __restrict qos, dds_type_consistency_kind_t *kind,
  bool *ignore_sequence_bounds, bool *ignore_string_bounds, bool *ignore_member_names, bool *prevent_type_widening, bool *force_type_validation)
{
  if (qos == NULL || !(qos->present & DDSI_QP_TYPE_CONSISTENCY_ENFORCEMENT))
    return false;
  if (kind)
    *kind = qos->type_consistency.kind;
  if (ignore_sequence_bounds)
    *ignore_sequence_bounds = qos->type_consistency.ignore_sequence_bounds;
  if (ignore_string_bounds)
    *ignore_string_bounds = qos->type_consistency.ignore_string_bounds;
  if (ignore_member_names)
    *ignore_member_names = qos->type_consistency.ignore_member_names;
  if (prevent_type_widening)
    *prevent_type_widening = qos->type_consistency.prevent_type_widening;
  if (force_type_validation)
    *force_type_validation = qos->type_consistency.force_type_validation;
  return true;
}

bool dds_qget_data_representation (const dds_qos_t * __restrict qos, uint32_t *n, dds_data_representation_id_t **values)
{
  if (qos == NULL || !(qos->present & DDSI_QP_DATA_REPRESENTATION))
    return false;
  if (n == NULL)
    return false;
  if (qos->data_representation.value.n > 0)
    assert (qos->data_representation.value.ids != NULL);
  *n = qos->data_representation.value.n;
  if (values != NULL)
  {
    if (qos->data_representation.value.n > 0)
    {
      size_t sz = qos->data_representation.value.n * sizeof (*qos->data_representation.value.ids);
      *values = dds_alloc (sz);
      memcpy (*values, qos->data_representation.value.ids, sz);
    }
    else
    {
      *values = NULL;
    }
  }
  return true;
}

dds_return_t dds_ensure_valid_data_representation (dds_qos_t *qos, uint32_t allowed_data_representations, bool topicqos)
{
  const bool allow1 = allowed_data_representations & DDS_DATA_REPRESENTATION_FLAG_XCDR1,
    allow2 = allowed_data_representations & DDS_DATA_REPRESENTATION_FLAG_XCDR2;

  if ((qos->present & DDSI_QP_DATA_REPRESENTATION) && qos->data_representation.value.n > 0)
  {
    assert (qos->data_representation.value.ids != NULL);
    for (uint32_t n = 0; n < qos->data_representation.value.n; n++)
    {
      switch (qos->data_representation.value.ids[n])
      {
        case DDS_DATA_REPRESENTATION_XML:
          return DDS_RETCODE_UNSUPPORTED;
        case DDS_DATA_REPRESENTATION_XCDR1:
          if (!allow1)
            return DDS_RETCODE_BAD_PARAMETER;
          break;
        case DDS_DATA_REPRESENTATION_XCDR2:
          if (!allow2)
            return DDS_RETCODE_BAD_PARAMETER;
          break;
        default:
          return DDS_RETCODE_BAD_PARAMETER;
      }
    }
  }
  else
  {
    if (!allow1 && !allow2)
      return DDS_RETCODE_BAD_PARAMETER;
    if (!allow1)
      dds_qset_data_representation (qos, 1, (dds_data_representation_id_t[]) { DDS_DATA_REPRESENTATION_XCDR2 });
    else if (!topicqos || !allow2)
      dds_qset_data_representation (qos, 1, (dds_data_representation_id_t[]) { DDS_DATA_REPRESENTATION_XCDR1 });
    else
      dds_qset_data_representation (qos, 2, (dds_data_representation_id_t[]) { DDS_DATA_REPRESENTATION_XCDR1, DDS_DATA_REPRESENTATION_XCDR2 });
  }
  return DDS_RETCODE_OK;
}

bool dds_qget_entity_name (const dds_qos_t * __restrict qos, char **name)
{
  if (qos == NULL || name == NULL || !(qos->present & DDSI_QP_ENTITY_NAME))
    return false;

  *name = dds_string_dup (qos->entity_name);
  return *name != NULL;
}

void dds_apply_entity_naming(dds_qos_t *qos, /* optional */ dds_qos_t *parent_qos, struct ddsi_domaingv *gv) {
  if (gv->config.entity_naming_mode == DDSI_ENTITY_NAMING_DEFAULT_FANCY && !(qos->present & DDSI_QP_ENTITY_NAME)) {
    char name_buf[16];
    ddsrt_mutex_lock(&gv->naming_lock);
    ddsrt_prng_random_name(&gv->naming_rng, name_buf, sizeof(name_buf));
    ddsrt_mutex_unlock(&gv->naming_lock);
    if (parent_qos && parent_qos->present & DDSI_QP_ENTITY_NAME) {
      // Copy the parent prefix
      memcpy(name_buf, parent_qos->entity_name, strnlen(parent_qos->entity_name, 3));
    }
    dds_qset_entity_name(qos, name_buf);
  }
}
