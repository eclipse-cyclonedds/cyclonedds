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
#include "dds/ddsi/ddsi_serdata.h"
#include "dds/ddsi/ddsi_sertype.h"
#include "dds__qos.h"
#include "dds__topic.h"
#include "dds__psmx.h"

static void dds_qos_data_copy_in (ddsi_octetseq_t *data, const void *value, size_t sz, bool overwrite)
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

void dds_reset_qos (dds_qos_t *qos)
{
  if (qos)
  {
    ddsi_xqos_fini (qos);
    ddsi_xqos_init_empty (qos);
  }
}

void dds_delete_qos (dds_qos_t *qos)
{
  if (qos)
  {
    ddsi_xqos_fini (qos);
    ddsrt_free (qos);
  }
}

dds_return_t dds_copy_qos (dds_qos_t *dst, const dds_qos_t *src)
{
  if (src == NULL || dst == NULL)
    return DDS_RETCODE_BAD_PARAMETER;
  ddsi_xqos_copy (dst, src);
  return DDS_RETCODE_OK;
}

void dds_merge_qos (dds_qos_t *dst, const dds_qos_t *src)
{
  /* Copy qos from source to destination unless already set */
  if (src != NULL && dst != NULL)
    ddsi_xqos_mergein_missing (dst, src, ~(uint64_t)0);
}

bool dds_qos_equal (const dds_qos_t *a, const dds_qos_t *b)
{
  /* FIXME: a bit of a hack - and I am not so sure I like accepting null pointers here anyway */
  if (a == NULL && b == NULL)
    return true;
  else if (a == NULL || b == NULL)
    return false;
  else
    return ddsi_xqos_delta (a, b, ~(DDSI_QP_TYPE_INFORMATION)) == 0;
}

void dds_qset_userdata (dds_qos_t *qos, const void *value, size_t sz)
{
  if (qos == NULL || (sz > 0 && value == NULL))
    return;
  dds_qos_data_copy_in (&qos->user_data, value, sz, qos->present & DDSI_QP_USER_DATA);
  qos->present |= DDSI_QP_USER_DATA;
}

void dds_qset_topicdata (dds_qos_t *qos, const void *value, size_t sz)
{
  if (qos == NULL || (sz > 0 && value == NULL))
    return;
  dds_qos_data_copy_in (&qos->topic_data, value, sz, qos->present & DDSI_QP_TOPIC_DATA);
  qos->present |= DDSI_QP_TOPIC_DATA;
}

void dds_qset_groupdata (dds_qos_t *qos, const void *value, size_t sz)
{
  if (qos == NULL || (sz > 0 && value == NULL))
    return;
  dds_qos_data_copy_in (&qos->group_data, value, sz, qos->present & DDSI_QP_GROUP_DATA);
  qos->present |= DDSI_QP_GROUP_DATA;
}

void dds_qset_durability (dds_qos_t *qos, dds_durability_kind_t kind)
{
  if (qos == NULL)
    return;
  qos->durability.kind = kind;
  qos->present |= DDSI_QP_DURABILITY;
}

void dds_qset_history (dds_qos_t *qos, dds_history_kind_t kind, int32_t depth)
{
  if (qos == NULL)
    return;
  qos->history.kind = kind;
  qos->history.depth = depth;
  qos->present |= DDSI_QP_HISTORY;
}

void dds_qset_resource_limits (dds_qos_t *qos, int32_t max_samples, int32_t max_instances, int32_t max_samples_per_instance)
{
  if (qos == NULL)
    return;
  qos->resource_limits.max_samples = max_samples;
  qos->resource_limits.max_instances = max_instances;
  qos->resource_limits.max_samples_per_instance = max_samples_per_instance;
  qos->present |= DDSI_QP_RESOURCE_LIMITS;
}

void dds_qset_presentation (dds_qos_t *qos, dds_presentation_access_scope_kind_t access_scope, bool coherent_access, bool ordered_access)
{
  if (qos == NULL)
    return;
  qos->presentation.access_scope = access_scope;
  qos->presentation.coherent_access = coherent_access;
  qos->presentation.ordered_access = ordered_access;
  qos->present |= DDSI_QP_PRESENTATION;
}

void dds_qset_lifespan (dds_qos_t *qos, dds_duration_t lifespan)
{
  if (qos == NULL)
    return;
  qos->lifespan.duration = lifespan;
  qos->present |= DDSI_QP_LIFESPAN;
}

void dds_qset_deadline (dds_qos_t *qos, dds_duration_t deadline)
{
  if (qos == NULL)
    return;
  qos->deadline.deadline = deadline;
  qos->present |= DDSI_QP_DEADLINE;
}

void dds_qset_latency_budget (dds_qos_t *qos, dds_duration_t duration)
{
  if (qos == NULL)
    return;
  qos->latency_budget.duration = duration;
  qos->present |= DDSI_QP_LATENCY_BUDGET;
}

void dds_qset_ownership (dds_qos_t *qos, dds_ownership_kind_t kind)
{
  if (qos == NULL)
    return;
  qos->ownership.kind = kind;
  qos->present |= DDSI_QP_OWNERSHIP;
}

void dds_qset_ownership_strength (dds_qos_t *qos, int32_t value)
{
  if (qos == NULL)
    return;
  qos->ownership_strength.value = value;
  qos->present |= DDSI_QP_OWNERSHIP_STRENGTH;
}

void dds_qset_liveliness (dds_qos_t *qos, dds_liveliness_kind_t kind, dds_duration_t lease_duration)
{
  if (qos == NULL)
    return;
  qos->liveliness.kind = kind;
  qos->liveliness.lease_duration = lease_duration;
  qos->present |= DDSI_QP_LIVELINESS;
}

void dds_qset_time_based_filter (dds_qos_t *qos, dds_duration_t minimum_separation)
{
  if (qos == NULL)
    return;
  qos->time_based_filter.minimum_separation = minimum_separation;
  qos->present |= DDSI_QP_TIME_BASED_FILTER;
}

void dds_qset_partition (dds_qos_t *qos, uint32_t n, const char **ps)
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

void dds_qset_partition1 (dds_qos_t *qos, const char *name)
{
  if (name == NULL)
    dds_qset_partition (qos, 0, NULL);
  else
    dds_qset_partition (qos, 1, (const char **) &name);
}

void dds_qset_reliability (dds_qos_t *qos, dds_reliability_kind_t kind, dds_duration_t max_blocking_time)
{
  if (qos == NULL)
    return;
  qos->reliability.kind = kind;
  qos->reliability.max_blocking_time = max_blocking_time;
  qos->present |= DDSI_QP_RELIABILITY;
}

void dds_qset_transport_priority (dds_qos_t *qos, int32_t value)
{
  if (qos == NULL)
    return;
  qos->transport_priority.value = value;
  qos->present |= DDSI_QP_TRANSPORT_PRIORITY;
}

void dds_qset_destination_order (dds_qos_t *qos, dds_destination_order_kind_t kind)
{
  if (qos == NULL)
    return;
  qos->destination_order.kind = kind;
  qos->present |= DDSI_QP_DESTINATION_ORDER;
}

void dds_qset_writer_data_lifecycle (dds_qos_t *qos, bool autodispose)
{
  if (qos == NULL)
    return;
  qos->writer_data_lifecycle.autodispose_unregistered_instances = autodispose;
  qos->present |= DDSI_QP_ADLINK_WRITER_DATA_LIFECYCLE;
}

void dds_qset_reader_data_lifecycle (dds_qos_t *qos, dds_duration_t autopurge_nowriter_samples_delay, dds_duration_t autopurge_disposed_samples_delay)
{
  if (qos == NULL)
    return;
  qos->reader_data_lifecycle.autopurge_nowriter_samples_delay = autopurge_nowriter_samples_delay;
  qos->reader_data_lifecycle.autopurge_disposed_samples_delay = autopurge_disposed_samples_delay;
  qos->present |= DDSI_QP_ADLINK_READER_DATA_LIFECYCLE;
}

void dds_qset_writer_batching (dds_qos_t *qos, bool batch_updates)
{
  if (qos == NULL)
    return;
  qos->writer_batching.batch_updates = batch_updates;
  qos->present |= DDSI_QP_CYCLONE_WRITER_BATCHING;
}

void dds_qset_durability_service (dds_qos_t *qos, dds_duration_t service_cleanup_delay, dds_history_kind_t history_kind, int32_t history_depth, int32_t max_samples, int32_t max_instances, int32_t max_samples_per_instance)
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

void dds_qset_ignorelocal (dds_qos_t *qos, dds_ignorelocal_kind_t ignore)
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
void dds_qunset_##prop_type_ (dds_qos_t *qos, const char * name) \
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

void dds_qset_prop (dds_qos_t *qos, const char * name, const char * value)
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

void dds_qset_bprop (dds_qos_t *qos, const char * name, const void * value, const size_t sz)
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

void dds_qset_entity_name (dds_qos_t *qos, const char * name)
{
  if (qos == NULL || name == NULL)
    return;
  if (qos->present & DDSI_QP_ENTITY_NAME)
    dds_free (qos->entity_name);
  qos->entity_name = dds_string_dup (name);
  qos->present |= DDSI_QP_ENTITY_NAME;
}

void dds_qset_type_consistency (dds_qos_t *qos, dds_type_consistency_kind_t kind,
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

void dds_qset_data_representation (dds_qos_t *qos, uint32_t n, const dds_data_representation_id_t *values)
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

void dds_qset_psmx_instances (dds_qos_t *qos, uint32_t n, const char **values)
{
  if (qos == NULL || (n > 0 && values == NULL) || n > DDS_MAX_PSMX_INSTANCES)
    return;

  // check that names are set
  for (uint32_t i = 0; i < n; i++)
  {
    if (strlen (values[i]) == 0)
      return;
  }

  // cleanup old data
  if ((qos->present & DDSI_QP_PSMX) && qos->psmx.n > 0)
  {
    assert (qos->psmx.strs != NULL);
    for (uint32_t i = 0; i < qos->psmx.n; i++)
      dds_free (qos->psmx.strs[i]);
    dds_free (qos->psmx.strs);
    qos->psmx.strs = NULL;
  }

  // copy in new data
  qos->psmx.n = n;
  if (n > 0)
  {
    qos->psmx.strs = dds_alloc (n * sizeof (*qos->psmx.strs));
    for (uint32_t i = 0; i < n; i++)
      qos->psmx.strs[i] = dds_string_dup (values[i]);
  }
  else
    qos->psmx.strs = NULL;

  qos->present |= DDSI_QP_PSMX;
}

bool dds_qget_userdata (const dds_qos_t *qos, void **value, size_t *sz)
{
  if (qos == NULL || !(qos->present & DDSI_QP_USER_DATA))
    return false;
  return dds_qos_data_copy_out (&qos->user_data, value, sz);
}

bool dds_qget_topicdata (const dds_qos_t *qos, void **value, size_t *sz)
{
  if (qos == NULL || !(qos->present & DDSI_QP_TOPIC_DATA))
    return false;
  return dds_qos_data_copy_out (&qos->topic_data, value, sz);
}

bool dds_qget_groupdata (const dds_qos_t *qos, void **value, size_t *sz)
{
  if (qos == NULL || !(qos->present & DDSI_QP_GROUP_DATA))
    return false;
  return dds_qos_data_copy_out (&qos->group_data, value, sz);
}

bool dds_qget_durability (const dds_qos_t *qos, dds_durability_kind_t *kind)
{
  if (qos == NULL || !(qos->present & DDSI_QP_DURABILITY))
    return false;
  if (kind)
    *kind = qos->durability.kind;
  return true;
}

bool dds_qget_history (const dds_qos_t *qos, dds_history_kind_t *kind, int32_t *depth)
{
  if (qos == NULL || !(qos->present & DDSI_QP_HISTORY))
    return false;
  if (kind)
    *kind = qos->history.kind;
  if (depth)
    *depth = qos->history.depth;
  return true;
}

bool dds_qget_resource_limits (const dds_qos_t *qos, int32_t *max_samples, int32_t *max_instances, int32_t *max_samples_per_instance)
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

bool dds_qget_presentation (const dds_qos_t *qos, dds_presentation_access_scope_kind_t *access_scope, bool *coherent_access, bool *ordered_access)
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

bool dds_qget_lifespan (const dds_qos_t *qos, dds_duration_t *lifespan)
{
  if (qos == NULL || !(qos->present & DDSI_QP_LIFESPAN))
    return false;
  if (lifespan)
    *lifespan = qos->lifespan.duration;
  return true;
}

bool dds_qget_deadline (const dds_qos_t *qos, dds_duration_t *deadline)
{
  if (qos == NULL || !(qos->present & DDSI_QP_DEADLINE))
    return false;
  if (deadline)
    *deadline = qos->deadline.deadline;
  return true;
}

bool dds_qget_latency_budget (const dds_qos_t *qos, dds_duration_t *duration)
{
  if (qos == NULL || !(qos->present & DDSI_QP_LATENCY_BUDGET))
    return false;
  if (duration)
    *duration = qos->latency_budget.duration;
  return true;
}

bool dds_qget_ownership (const dds_qos_t *qos, dds_ownership_kind_t *kind)
{
  if (qos == NULL || !(qos->present & DDSI_QP_OWNERSHIP))
    return false;
  if (kind)
    *kind = qos->ownership.kind;
  return true;
}

bool dds_qget_ownership_strength (const dds_qos_t *qos, int32_t *value)
{
  if (qos == NULL || !(qos->present & DDSI_QP_OWNERSHIP_STRENGTH))
    return false;
  if (value)
    *value = qos->ownership_strength.value;
  return true;
}

bool dds_qget_liveliness (const dds_qos_t *qos, dds_liveliness_kind_t *kind, dds_duration_t *lease_duration)
{
  if (qos == NULL || !(qos->present & DDSI_QP_LIVELINESS))
    return false;
  if (kind)
    *kind = qos->liveliness.kind;
  if (lease_duration)
    *lease_duration = qos->liveliness.lease_duration;
  return true;
}

bool dds_qget_time_based_filter (const dds_qos_t *qos, dds_duration_t *minimum_separation)
{
  if (qos == NULL || !(qos->present & DDSI_QP_TIME_BASED_FILTER))
    return false;
  if (minimum_separation)
    *minimum_separation = qos->time_based_filter.minimum_separation;
  return true;
}

bool dds_qget_partition (const dds_qos_t *qos, uint32_t *n, char ***ps)
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

bool dds_qget_reliability (const dds_qos_t *qos, dds_reliability_kind_t *kind, dds_duration_t *max_blocking_time)
{
  if (qos == NULL || !(qos->present & DDSI_QP_RELIABILITY))
    return false;
  if (kind)
    *kind = qos->reliability.kind;
  if (max_blocking_time)
    *max_blocking_time = qos->reliability.max_blocking_time;
  return true;
}

bool dds_qget_transport_priority (const dds_qos_t *qos, int32_t *value)
{
  if (qos == NULL || !(qos->present & DDSI_QP_TRANSPORT_PRIORITY))
    return false;
  if (value)
    *value = qos->transport_priority.value;
  return true;
}

bool dds_qget_destination_order (const dds_qos_t *qos, dds_destination_order_kind_t *kind)
{
  if (qos == NULL || !(qos->present & DDSI_QP_DESTINATION_ORDER))
    return false;
  if (kind)
    *kind = qos->destination_order.kind;
  return true;
}

bool dds_qget_writer_data_lifecycle (const dds_qos_t *qos, bool *autodispose)
{
  if (qos == NULL || !(qos->present & DDSI_QP_ADLINK_WRITER_DATA_LIFECYCLE))
    return false;
  if (autodispose)
    *autodispose = qos->writer_data_lifecycle.autodispose_unregistered_instances;
  return true;
}

bool dds_qget_reader_data_lifecycle (const dds_qos_t *qos, dds_duration_t *autopurge_nowriter_samples_delay, dds_duration_t *autopurge_disposed_samples_delay)
{
  if (qos == NULL || !(qos->present & DDSI_QP_ADLINK_READER_DATA_LIFECYCLE))
    return false;
  if (autopurge_nowriter_samples_delay)
    *autopurge_nowriter_samples_delay = qos->reader_data_lifecycle.autopurge_nowriter_samples_delay;
  if (autopurge_disposed_samples_delay)
    *autopurge_disposed_samples_delay = qos->reader_data_lifecycle.autopurge_disposed_samples_delay;
  return true;
}

bool dds_qget_writer_batching (const dds_qos_t *qos, bool *batch_updates)
{
  if (qos == NULL || !(qos->present & DDSI_QP_CYCLONE_WRITER_BATCHING))
    return false;
  if (batch_updates)
    *batch_updates = qos->writer_batching.batch_updates;
  return true;
}

bool dds_qget_durability_service (const dds_qos_t *qos, dds_duration_t *service_cleanup_delay, dds_history_kind_t *history_kind, int32_t *history_depth, int32_t *max_samples, int32_t *max_instances, int32_t *max_samples_per_instance)
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

bool dds_qget_ignorelocal (const dds_qos_t *qos, dds_ignorelocal_kind_t *ignore)
{
  if (qos == NULL || !(qos->present & DDSI_QP_CYCLONE_IGNORELOCAL))
    return false;
  if (ignore)
    *ignore = qos->ignorelocal.value;
  return true;
}

#define DDS_QGET_PROPNAMES(prop_type_, prop_field_) \
bool dds_qget_##prop_type_##names (const dds_qos_t *qos, uint32_t * n, char *** names) \
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

bool dds_qget_prop (const dds_qos_t *qos, const char * name, char ** value)
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

bool dds_qget_bprop (const dds_qos_t *qos, const char * name, void ** value, size_t * sz)
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

bool dds_qget_type_consistency (const dds_qos_t *qos, dds_type_consistency_kind_t *kind,
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

bool dds_qget_data_representation (const dds_qos_t *qos, uint32_t *n, dds_data_representation_id_t **values)
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

dds_return_t dds_ensure_valid_data_representation (dds_qos_t *qos, uint32_t allowed_data_representations, dds_data_type_properties_t data_type_props, dds_entity_kind_t entitykind)
{
  const bool allow1 = allowed_data_representations & DDS_DATA_REPRESENTATION_FLAG_XCDR1;
  const bool allow2 = allowed_data_representations & DDS_DATA_REPRESENTATION_FLAG_XCDR2;
  const bool prefer2 = data_type_props & DDS_DATA_TYPE_CONTAINS_OPTIONAL; // TO-DO: prefer xcdr2 if type contains appendable/mutable

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

    dds_data_representation_id_t xs[2];
    uint32_t nxs = 0;
    if (prefer2)
    {
      if (allow2) xs[nxs++] = DDS_DATA_REPRESENTATION_XCDR2;
      if (allow1) xs[nxs++] = DDS_DATA_REPRESENTATION_XCDR1;
    }
    else
    {
      if (allow1) xs[nxs++] = DDS_DATA_REPRESENTATION_XCDR1;
      if (allow2) xs[nxs++] = DDS_DATA_REPRESENTATION_XCDR2;
    }
    assert (nxs >= 1);
    dds_qset_data_representation (qos, (entitykind == DDS_KIND_TOPIC || entitykind == DDS_KIND_READER) ? nxs : 1, xs);
  }
  return DDS_RETCODE_OK;
}


bool dds_qget_psmx_instances (const dds_qos_t *qos, uint32_t *n_out, char ***values)
{
  if (qos == NULL || !(qos->present & DDSI_QP_PSMX) || n_out == NULL)
    return false;

  if (qos->psmx.n > 0)
    assert (qos->psmx.strs != NULL);

  *n_out = qos->psmx.n;
  if (values != NULL)
  {
    if (*n_out > 0)
    {
      *values = dds_alloc ((*n_out) * sizeof (**values));
      for (uint32_t i = 0; i < *n_out; i++)
        (*values)[i] = dds_string_dup (qos->psmx.strs[i]);
    }
    else
      *values = NULL;
  }

  return true;
}

dds_return_t dds_ensure_valid_psmx_instances (dds_qos_t *qos, dds_psmx_endpoint_type_t forwhat, const struct ddsi_sertype *stype, const struct dds_psmx_set *psmx_instances)
{
  uint32_t n_supported = 0;
  const char *supported_psmx[DDS_MAX_PSMX_INSTANCES];
  dds_return_t ret = DDS_RETCODE_OK;

  // Check sertype has operations required by PSMX
  if ((forwhat == DDS_PSMX_ENDPOINT_TYPE_WRITER && stype->serdata_ops->from_loaned_sample) ||
      (forwhat == DDS_PSMX_ENDPOINT_TYPE_READER && stype->serdata_ops->from_psmx))
  {
    if (!(qos->present & DDSI_QP_PSMX))
    {
      assert (psmx_instances->length <= DDS_MAX_PSMX_INSTANCES);
      for (uint32_t i = 0; i < psmx_instances->length; i++)
      {
        struct dds_psmx_int *psmx = psmx_instances->elems[i].instance;
        if (psmx->ops.type_qos_supported (psmx->ext, forwhat, stype->data_type_props, qos))
          supported_psmx[n_supported++] = psmx->instance_name;
      }
    }
    else
    {
      uint32_t n = 0;
      char **values;
      dds_qget_psmx_instances (qos, &n, &values);
      for (uint32_t i = 0; ret == DDS_RETCODE_OK && i < n; i++)
      {
        struct dds_psmx_int *psmx = NULL;
        for (uint32_t s = 0; psmx == NULL && s < psmx_instances->length; s++)
        {
          assert (psmx_instances->elems[s].instance);
          if (strcmp (psmx_instances->elems[s].instance->instance_name, values[i]) == 0)
            psmx = psmx_instances->elems[s].instance;
        }
        if (psmx == NULL || !psmx->ops.type_qos_supported (psmx->ext, forwhat, stype->data_type_props, qos))
          ret = DDS_RETCODE_BAD_PARAMETER;
        else
        {
          uint32_t j;
          for (j = 0; j < n_supported; j++)
            if (supported_psmx[j] == psmx->instance_name)
              break;
          if (j == n_supported)
            supported_psmx[n_supported++] = psmx->instance_name;
          else
            ret = DDS_RETCODE_BAD_PARAMETER;
        }
      }
      for (uint32_t i = 0; i < n; i++)
        dds_free (values[i]);
      dds_free (values);
    }
  }

  if (ret == DDS_RETCODE_OK)
    dds_qset_psmx_instances (qos, n_supported, supported_psmx);
  return ret;
}

bool dds_qos_has_psmx_instances (const dds_qos_t *qos, const char *psmx_instance_name)
{
  uint32_t n_instances = 0;
  char **values = NULL;
  bool found = false;
  dds_qget_psmx_instances (qos, &n_instances, &values);
  for (uint32_t i = 0; values != NULL && i < n_instances; i++)
  {
    if (strcmp (psmx_instance_name, values[i]) == 0)
      found = true;
    dds_free (values[i]);
  }
  if (n_instances > 0)
    dds_free (values);
  return found;
}

bool dds_qget_entity_name (const dds_qos_t *qos, char **name)
{
  if (qos == NULL || name == NULL || !(qos->present & DDSI_QP_ENTITY_NAME))
    return false;

  *name = dds_string_dup (qos->entity_name);
  return *name != NULL;
}

void dds_apply_entity_naming(dds_qos_t *qos, /* optional */ dds_qos_t *parent_qos, struct ddsi_domaingv *gv)
{
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
