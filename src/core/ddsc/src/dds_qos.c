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
#include "dds__qos.h"
#include "dds__err.h"
#include "ddsi/q_config.h"

/* TODO: dd_duration_t is converted to nn_ddsi_time_t declared in q_time.h
   This structure contain seconds and fractions.
   Revisit on the conversion as default values are { 0x7fffffff, 0xffffffff }
*/

static void
dds_qos_data_copy_in(
    _Inout_ nn_octetseq_t * data,
    _In_reads_bytes_opt_(sz) const void * __restrict value,
    _In_ size_t sz)
{
    if (data->value)  {
        dds_free (data->value);
        data->value = NULL;
    }
    data->length = (uint32_t) sz;
    if (sz && value)  {
        data->value = dds_alloc (sz);
        memcpy (data->value, value, sz);
    }
}

static bool
dds_qos_data_copy_out(
    _In_ const nn_octetseq_t * data,
    _When_(*sz == 0, _At_(*value, _Post_null_))
    _When_(*sz > 0, _At_(*value, _Post_notnull_))
      _Outptr_result_bytebuffer_all_maybenull_(*sz) void ** value,
    _Out_ size_t * sz)
{
    if (sz == NULL && value != NULL) {
        return false;
    }
    if (sz) {
        *sz = data->length;
    }
    if (value) {
        if (data->length != 0) {
            assert(data->value);
            *value = dds_alloc(data->length);
            memcpy(*value, data->value, data->length);
        } else {
            *value = NULL;
        }
    }
    return true;
}

bool
validate_octetseq(
    const nn_octetseq_t* seq)
{
    /* default value is NULL with length 0 */
    return (((seq->length == 0) && (seq->value == NULL)) || (seq->length > 0));
}

bool
validate_stringseq(
    const nn_stringseq_t* seq)
{
    if (seq->n != 0) {
        unsigned i;
        for (i = 0; i < seq->n; i++) {
            if (!seq->strs[i]) {
                break;
            }
        }
        return (seq->n == i);
    } else {
        return (seq->strs == NULL);
    }
}

bool
validate_entityfactory_qospolicy(
    const nn_entity_factory_qospolicy_t * entityfactory)
{
    /* Bools must be 0 or 1, i.e., only the lsb may be set */
    return !(entityfactory->autoenable_created_entities & ~1);
}

bool
validate_reliability_qospolicy(
    const nn_reliability_qospolicy_t * reliability)
{
    return (
        (reliability->kind == NN_BEST_EFFORT_RELIABILITY_QOS || reliability->kind == NN_RELIABLE_RELIABILITY_QOS) &&
        (validate_duration(&reliability->max_blocking_time) == 0)
    );
}

bool
validate_deadline_and_timebased_filter(
    const nn_duration_t deadline,
    const nn_duration_t minimum_separation)
{
    return (
        (validate_duration(&deadline) == 0) &&
        (validate_duration(&minimum_separation) == 0) &&
        (nn_from_ddsi_duration(minimum_separation) <= nn_from_ddsi_duration(deadline))
    );
}

bool
dds_qos_validate_common (
    const dds_qos_t *qos)
{
    return !(
        ((qos->present & QP_DURABILITY) && (validate_durability_qospolicy (&qos->durability) != 0)) ||
        ((qos->present & QP_DEADLINE) && (validate_duration (&qos->deadline.deadline) != 0)) ||
        ((qos->present & QP_LATENCY_BUDGET) && (validate_duration (&qos->latency_budget.duration) != 0)) ||
        ((qos->present & QP_OWNERSHIP) && (validate_ownership_qospolicy (&qos->ownership) != 0)) ||
        ((qos->present & QP_LIVELINESS) && (validate_liveliness_qospolicy (&qos->liveliness) != 0)) ||
        ((qos->present & QP_RELIABILITY) && ! validate_reliability_qospolicy (&qos->reliability)) ||
        ((qos->present & QP_DESTINATION_ORDER) && (validate_destination_order_qospolicy (&qos->destination_order) != 0)) ||
        ((qos->present & QP_HISTORY) && (validate_history_qospolicy (&qos->history) != 0)) ||
        ((qos->present & QP_RESOURCE_LIMITS) && (validate_resource_limits_qospolicy (&qos->resource_limits) != 0))
    );
}

dds_return_t
dds_qos_validate_mutable_common (
    _In_ const dds_qos_t *qos)
{
    dds_return_t ret = DDS_RETCODE_OK;

    /* TODO: Check whether immutable QoS are changed should actually incorporate change to current QoS */
    if (qos->present & QP_DEADLINE) {
        DDS_ERROR("Deadline QoS policy caused immutable error\n");
        ret = DDS_ERRNO(DDS_RETCODE_IMMUTABLE_POLICY);
    }
    if (qos->present & QP_OWNERSHIP) {
        DDS_ERROR("Ownership QoS policy caused immutable error\n");
        ret = DDS_ERRNO(DDS_RETCODE_IMMUTABLE_POLICY);
    }
    if (qos->present & QP_LIVELINESS) {
        DDS_ERROR("Liveliness QoS policy caused immutable error\n");
        ret = DDS_ERRNO(DDS_RETCODE_IMMUTABLE_POLICY);
    }
    if (qos->present & QP_RELIABILITY) {
        DDS_ERROR("Reliability QoS policy caused immutable error\n");
        ret = DDS_ERRNO(DDS_RETCODE_IMMUTABLE_POLICY);
    }
    if (qos->present & QP_DESTINATION_ORDER) {
        DDS_ERROR("Destination order QoS policy caused immutable error\n");
        ret = DDS_ERRNO(DDS_RETCODE_IMMUTABLE_POLICY);
    }
    if (qos->present & QP_HISTORY) {
        DDS_ERROR("History QoS policy caused immutable error\n");
        ret = DDS_ERRNO(DDS_RETCODE_IMMUTABLE_POLICY);
    }
    if (qos->present & QP_RESOURCE_LIMITS) {
        DDS_ERROR("Resource limits QoS policy caused immutable error\n");
        ret = DDS_ERRNO(DDS_RETCODE_IMMUTABLE_POLICY);
    }

    return ret;
}

static void
dds_qos_init_defaults (
    _Inout_ dds_qos_t * __restrict qos)
{
    assert (qos);
    memset (qos, 0, sizeof (*qos));
    qos->durability.kind = (nn_durability_kind_t) DDS_DURABILITY_VOLATILE;
    qos->deadline.deadline = nn_to_ddsi_duration (DDS_INFINITY);
    qos->durability_service.service_cleanup_delay = nn_to_ddsi_duration (0);
    qos->durability_service.history.kind = (nn_history_kind_t) DDS_HISTORY_KEEP_LAST;
    qos->durability_service.history.depth = 1;
    qos->durability_service.resource_limits.max_samples = DDS_LENGTH_UNLIMITED;
    qos->durability_service.resource_limits.max_instances = DDS_LENGTH_UNLIMITED;
    qos->durability_service.resource_limits.max_samples_per_instance = DDS_LENGTH_UNLIMITED;
    qos->presentation.access_scope = (nn_presentation_access_scope_kind_t) DDS_PRESENTATION_INSTANCE;
    qos->latency_budget.duration = nn_to_ddsi_duration (0);
    qos->ownership.kind = (nn_ownership_kind_t) DDS_OWNERSHIP_SHARED;
    qos->liveliness.kind = (nn_liveliness_kind_t) DDS_LIVELINESS_AUTOMATIC;
    qos->liveliness.lease_duration = nn_to_ddsi_duration (DDS_INFINITY);
    qos->time_based_filter.minimum_separation = nn_to_ddsi_duration (0);
    qos->reliability.kind = (nn_reliability_kind_t) DDS_RELIABILITY_BEST_EFFORT;
    qos->reliability.max_blocking_time = nn_to_ddsi_duration (DDS_MSECS (100));
    qos->lifespan.duration = nn_to_ddsi_duration (DDS_INFINITY);
    qos->destination_order.kind = (nn_destination_order_kind_t) DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP;
    qos->history.kind = (nn_history_kind_t) DDS_HISTORY_KEEP_LAST;
    qos->history.depth = 1;
    qos->resource_limits.max_samples = DDS_LENGTH_UNLIMITED;
    qos->resource_limits.max_instances = DDS_LENGTH_UNLIMITED;
    qos->resource_limits.max_samples_per_instance = DDS_LENGTH_UNLIMITED;
    qos->writer_data_lifecycle.autodispose_unregistered_instances = true;
    qos->reader_data_lifecycle.autopurge_nowriter_samples_delay = nn_to_ddsi_duration (DDS_INFINITY);
    qos->reader_data_lifecycle.autopurge_disposed_samples_delay = nn_to_ddsi_duration (DDS_INFINITY);
}

_Ret_notnull_
dds_qos_t * dds_create_qos (void)
{
    dds_qos_t *qos = dds_alloc (sizeof (dds_qos_t));
    dds_qos_init_defaults (qos);
    return qos;
}

_Ret_notnull_
dds_qos_t * dds_qos_create (void)
{
    return dds_create_qos ();
}

void
dds_reset_qos(
    _Out_ dds_qos_t * __restrict qos)
{
    if (qos) {
        nn_xqos_fini (qos);
        dds_qos_init_defaults (qos);
    } else {
        DDS_WARNING("Argument QoS is NULL\n");
    }
}

void
dds_qos_reset(
    _Out_ dds_qos_t * __restrict qos)
{
    dds_reset_qos (qos);
}

void
dds_delete_qos(
    _In_ _Post_invalid_ dds_qos_t * __restrict qos)
{
    if (qos) {
        dds_reset_qos(qos);
        dds_free(qos);
    }
}

void
dds_qos_delete(
    _In_ _Post_invalid_ dds_qos_t * __restrict qos)
{
    dds_delete_qos (qos);
}

dds_return_t
dds_copy_qos (
    _Out_ dds_qos_t * __restrict dst,
    _In_ const dds_qos_t * __restrict src)
{
    if(!src){
        DDS_ERROR("Argument source(src) is NULL\n");
        return DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }
    if(!dst){
        DDS_ERROR("Argument destination(dst) is NULL\n");
        return DDS_ERRNO(DDS_RETCODE_BAD_PARAMETER);
    }
    nn_xqos_copy (dst, src);
    return DDS_RETCODE_OK;
}

dds_return_t
dds_qos_copy (
    _Out_ dds_qos_t * __restrict dst,
    _In_ const dds_qos_t * __restrict src)
{
    return dds_copy_qos (dst, src);
}

void dds_merge_qos (
    _Inout_ dds_qos_t * __restrict dst,
    _In_ const dds_qos_t * __restrict src)
{
    if(!src){
        DDS_ERROR("Argument source(src) is NULL\n");
        return;
    }
    if(!dst){
        DDS_ERROR("Argument destination(dst) is NULL\n");
        return;
    }
    /* Copy qos from source to destination unless already set */
    nn_xqos_mergein_missing (dst, src);
}

void dds_qos_merge (
    _Inout_ dds_qos_t * __restrict dst,
    _In_ const dds_qos_t * __restrict src)
{
    dds_merge_qos (dst, src);
}

bool dds_qos_equal (
    _In_opt_ const dds_qos_t * __restrict a,
    _In_opt_ const dds_qos_t * __restrict b)
{
    /* FIXME: a bit of a hack - and I am not so sure I like accepting null pointers here anyway */
    if (a == NULL && b == NULL) {
        return true;
    } else if (a == NULL || b == NULL) {
        return false;
    } else {
        return nn_xqos_delta(a, b, ~(uint64_t)0) == 0;
    }
}

void dds_qset_userdata(
    _Inout_ dds_qos_t * __restrict qos,
    _In_reads_bytes_opt_(sz) const void * __restrict value,
    _In_ size_t sz)
{
    if (!qos) {
        DDS_ERROR("Argument qos is NULL\n");
        return ;
    }
    dds_qos_data_copy_in(&qos->user_data, value, sz);
    qos->present |= QP_USER_DATA;
}

void dds_qset_topicdata(
    _Inout_ dds_qos_t * __restrict qos,
    _In_reads_bytes_opt_(sz) const void * __restrict value,
    _In_ size_t sz)
{
    if (!qos) {
        DDS_ERROR("Argument qos is NULL\n");
        return ;
    }
    dds_qos_data_copy_in (&qos->topic_data, value, sz);
    qos->present |= QP_TOPIC_DATA;
}

void dds_qset_groupdata(
    _Inout_ dds_qos_t * __restrict qos,
    _In_reads_bytes_opt_(sz) const void * __restrict value,
    _In_ size_t sz)
{
    if (!qos) {
        DDS_ERROR("Argument qos is NULL\n");
        return ;
    }
    dds_qos_data_copy_in (&qos->group_data, value, sz);
    qos->present |= QP_GROUP_DATA;
}

void dds_qset_durability
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(DDS_DURABILITY_VOLATILE, DDS_DURABILITY_PERSISTENT) dds_durability_kind_t kind
)
{
    if (qos) {
        qos->durability.kind = (nn_durability_kind_t) kind;
        qos->present |= QP_DURABILITY;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_history
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(DDS_HISTORY_KEEP_LAST, DDS_HISTORY_KEEP_ALL) dds_history_kind_t kind,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t depth
)
{
    if (qos) {
        qos->history.kind = (nn_history_kind_t) kind;
        qos->history.depth = depth;
        qos->present |= QP_HISTORY;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_resource_limits
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t max_samples,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t max_instances,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t max_samples_per_instance
)
{
    if (qos) {
        qos->resource_limits.max_samples = max_samples;
        qos->resource_limits.max_instances = max_instances;
        qos->resource_limits.max_samples_per_instance = max_samples_per_instance;
        qos->present |= QP_RESOURCE_LIMITS;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_presentation
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(DDS_PRESENTATION_INSTANCE, DDS_PRESENTATION_GROUP) dds_presentation_access_scope_kind_t access_scope,
    _In_ bool coherent_access,
    _In_ bool ordered_access
)
{
    if (qos) {
        qos->presentation.access_scope = (nn_presentation_access_scope_kind_t) access_scope;
        qos->presentation.coherent_access = coherent_access;
        qos->presentation.ordered_access = ordered_access;
        qos->present |= QP_PRESENTATION;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_lifespan
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(0, DDS_INFINITY) dds_duration_t lifespan
)
{
    if (qos) {
        qos->lifespan.duration = nn_to_ddsi_duration (lifespan);
        qos->present |= QP_LIFESPAN;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_deadline
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(0, DDS_INFINITY) dds_duration_t deadline
)
{
    if (qos) {
        qos->deadline.deadline = nn_to_ddsi_duration (deadline);
        qos->present |= QP_DEADLINE;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_latency_budget
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(0, DDS_INFINITY) dds_duration_t duration
)
{
    if (qos) {
        qos->latency_budget.duration = nn_to_ddsi_duration (duration);
        qos->present |= QP_LATENCY_BUDGET;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_ownership
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(DDS_OWNERSHIP_SHARED, DDS_OWNERSHIP_EXCLUSIVE) dds_ownership_kind_t kind
)
{
    if (qos) {
        qos->ownership.kind = (nn_ownership_kind_t) kind;
        qos->present |= QP_OWNERSHIP;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_ownership_strength
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_ int32_t value
)
{
    if (qos) {
        qos->ownership_strength.value = value;
        qos->present |= QP_OWNERSHIP_STRENGTH;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_liveliness
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(DDS_LIVELINESS_AUTOMATIC, DDS_LIVELINESS_MANUAL_BY_TOPIC) dds_liveliness_kind_t kind,
    _In_range_(0, DDS_INFINITY) dds_duration_t lease_duration
)
{
    if (qos) {
        qos->liveliness.kind = (nn_liveliness_kind_t) kind;
        qos->liveliness.lease_duration = nn_to_ddsi_duration (lease_duration);
        qos->present |= QP_LIVELINESS;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_time_based_filter
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(0, DDS_INFINITY) dds_duration_t minimum_separation
)
{
    if (qos) {
        qos->time_based_filter.minimum_separation = nn_to_ddsi_duration (minimum_separation);
        qos->present |= QP_TIME_BASED_FILTER;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_partition
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_ uint32_t n,
    _In_count_(n) _Deref_pre_z_ const char ** __restrict ps
)
{
    uint32_t i;

    if(!qos) {
        DDS_ERROR("Argument qos may not be NULL\n");
        return ;
    }
    if(n && !ps) {
        DDS_ERROR("Argument ps is NULL, but n (%u) > 0", n);
        return ;
    }

    if (qos->partition.strs != NULL){
        for (i = 0; i < qos->partition.n; i++) {
            dds_free(qos->partition.strs[i]);
        }
        dds_free(qos->partition.strs);
        qos->partition.strs = NULL;
    }

    qos->partition.n = n;
    if(n){
        qos->partition.strs = dds_alloc (sizeof (char*) * n);
    }

    for (i = 0; i < n; i++) {
        qos->partition.strs[i] = dds_string_dup (ps[i]);
    }
    qos->present |= QP_PARTITION;
}

void dds_qset_reliability
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(DDS_RELIABILITY_BEST_EFFORT, DDS_RELIABILITY_RELIABLE) dds_reliability_kind_t kind,
    _In_range_(0, DDS_INFINITY) dds_duration_t max_blocking_time
)
{
    if (qos) {
        qos->reliability.kind = (nn_reliability_kind_t) kind;
        qos->reliability.max_blocking_time = nn_to_ddsi_duration (max_blocking_time);
        qos->present |= QP_RELIABILITY;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_transport_priority
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_ int32_t value
)
{
    if (qos) {
        qos->transport_priority.value = value;
        qos->present |= QP_TRANSPORT_PRIORITY;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_destination_order
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP,
        DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP) dds_destination_order_kind_t kind
)
{
    if (qos) {
        qos->destination_order.kind = (nn_destination_order_kind_t) kind;
        qos->present |= QP_DESTINATION_ORDER;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_writer_data_lifecycle
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_ bool autodispose
)
{
    if(qos) {
        qos->writer_data_lifecycle.autodispose_unregistered_instances = autodispose;
        qos->present |= QP_PRISMTECH_WRITER_DATA_LIFECYCLE;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_reader_data_lifecycle
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(0, DDS_INFINITY) dds_duration_t autopurge_nowriter_samples_delay,
    _In_range_(0, DDS_INFINITY) dds_duration_t autopurge_disposed_samples_delay
)
{
    if (qos) {
        qos->reader_data_lifecycle.autopurge_nowriter_samples_delay = \
          nn_to_ddsi_duration (autopurge_nowriter_samples_delay);
        qos->reader_data_lifecycle.autopurge_disposed_samples_delay = \
          nn_to_ddsi_duration (autopurge_disposed_samples_delay);
        qos->present |= QP_PRISMTECH_READER_DATA_LIFECYCLE;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

void dds_qset_durability_service
(
    _Inout_ dds_qos_t * __restrict qos,
    _In_range_(0, DDS_INFINITY) dds_duration_t service_cleanup_delay,
    _In_range_(DDS_HISTORY_KEEP_LAST, DDS_HISTORY_KEEP_ALL) dds_history_kind_t history_kind,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t history_depth,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t max_samples,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t max_instances,
    _In_range_(>=, DDS_LENGTH_UNLIMITED) int32_t max_samples_per_instance
)
{
    if (qos) {
        qos->durability_service.service_cleanup_delay = nn_to_ddsi_duration (service_cleanup_delay);
        qos->durability_service.history.kind = (nn_history_kind_t) history_kind;
        qos->durability_service.history.depth = history_depth;
        qos->durability_service.resource_limits.max_samples = max_samples;
        qos->durability_service.resource_limits.max_instances = max_instances;
        qos->durability_service.resource_limits.max_samples_per_instance = max_samples_per_instance;
        qos->present |= QP_DURABILITY_SERVICE;
    } else {
        DDS_ERROR("Argument QoS is NULL\n");
    }
}

bool dds_qget_userdata (const dds_qos_t * __restrict qos, void **value, size_t *sz)
{
    if (!qos || !(qos->present & QP_USER_DATA)) {
        return false;
    }
    return dds_qos_data_copy_out (&qos->user_data, value, sz);
}

bool dds_qget_topicdata (const dds_qos_t * __restrict qos, void **value, size_t *sz)
{
    if (!qos || !(qos->present & QP_TOPIC_DATA)) {
        return false;
    }
    return dds_qos_data_copy_out (&qos->topic_data, value, sz);
}

bool dds_qget_groupdata (const dds_qos_t * __restrict qos, void **value, size_t *sz)
{
    if (!qos || !(qos->present & QP_GROUP_DATA)) {
        return false;
    }
    return dds_qos_data_copy_out (&qos->group_data, value, sz);
}

bool dds_qget_durability (const dds_qos_t * __restrict qos, dds_durability_kind_t *kind)
{
    if (!qos || !(qos->present & QP_DURABILITY)) {
        return false;
    }
    if (kind) {
        *kind = (dds_durability_kind_t) qos->durability.kind;
    }
    return true;
}

bool dds_qget_history (const dds_qos_t * __restrict qos, dds_history_kind_t *kind, int32_t *depth)
{
    if (!qos || !(qos->present & QP_HISTORY)) {
        return false;
    }
    if (kind) {
        *kind = (dds_history_kind_t) qos->history.kind;
    }
    if (depth) {
        *depth = qos->history.depth;
    }
    return true;
}

bool dds_qget_resource_limits (const dds_qos_t * __restrict qos, int32_t *max_samples, int32_t *max_instances, int32_t *max_samples_per_instance)
{
    if (!qos || !(qos->present & QP_RESOURCE_LIMITS)) {
        return false;
    }
    if (max_samples) {
        *max_samples = qos->resource_limits.max_samples;
    }
    if (max_instances) {
        *max_instances = qos->resource_limits.max_instances;
    }
    if (max_samples_per_instance) {
        *max_samples_per_instance = qos->resource_limits.max_samples_per_instance;
    }
    return true;
}

bool dds_qget_presentation (const dds_qos_t * __restrict qos, dds_presentation_access_scope_kind_t *access_scope, bool *coherent_access, bool *ordered_access)
{
    if (!qos || !(qos->present & QP_PRESENTATION)) {
        return false;
    }
    if (access_scope) {
        *access_scope = (dds_presentation_access_scope_kind_t) qos->presentation.access_scope;
    }
    if (coherent_access) {
        *coherent_access = qos->presentation.coherent_access;
    }
    if (ordered_access) {
        *ordered_access = qos->presentation.ordered_access;
    }
    return true;
}

bool dds_qget_lifespan (const dds_qos_t * __restrict qos, dds_duration_t *lifespan)
{
    if (!qos || !(qos->present & QP_LIFESPAN)) {
        return false;
    }
    if (lifespan) {
        *lifespan = nn_from_ddsi_duration (qos->lifespan.duration);
    }
    return true;
}

bool dds_qget_deadline (const dds_qos_t * __restrict qos, dds_duration_t *deadline)
{
    if (!qos || !(qos->present & QP_DEADLINE)) {
        return false;
    }
    if (deadline) {
        *deadline = nn_from_ddsi_duration (qos->deadline.deadline);
    }
    return true;
}

bool dds_qget_latency_budget (const dds_qos_t * __restrict qos, dds_duration_t *duration)
{
    if (!qos || !(qos->present & QP_LATENCY_BUDGET)) {
        return false;
    }
    if (duration) {
        *duration = nn_from_ddsi_duration (qos->latency_budget.duration);
    }
    return true;
}

bool dds_qget_ownership (const dds_qos_t * __restrict qos, dds_ownership_kind_t *kind)
{
    if (!qos || !(qos->present & QP_OWNERSHIP)) {
        return false;
    }
    if (kind) {
        *kind = (dds_ownership_kind_t) qos->ownership.kind;
    }
    return true;
}

bool dds_qget_ownership_strength (const dds_qos_t * __restrict qos, int32_t *value)
{
    if (!qos || !(qos->present & QP_OWNERSHIP_STRENGTH)) {
        return false;
    }
    if (value) {
        *value = qos->ownership_strength.value;
    }
    return true;
}

bool dds_qget_liveliness (const dds_qos_t * __restrict qos, dds_liveliness_kind_t *kind, dds_duration_t *lease_duration)
{
    if (!qos || !(qos->present & QP_LIVELINESS)) {
        return false;
    }
    if (kind) {
        *kind = (dds_liveliness_kind_t) qos->liveliness.kind;
    }
    if (lease_duration) {
        *lease_duration = nn_from_ddsi_duration (qos->liveliness.lease_duration);
    }
    return true;
}

bool dds_qget_time_based_filter (const dds_qos_t * __restrict qos, dds_duration_t *minimum_separation)
{
    if (!qos || !(qos->present & QP_TIME_BASED_FILTER)) {
        return false;
    }
    if (minimum_separation) {
        *minimum_separation = nn_from_ddsi_duration (qos->time_based_filter.minimum_separation);
    }
    return true;
}

bool dds_qget_partition (const dds_qos_t * __restrict qos, uint32_t *n, char ***ps)
{
    if (!qos || !(qos->present & QP_PARTITION)) {
        return false;
    }
    if (n == NULL && ps != NULL) {
        return false;
    }
    if (n) {
        *n = qos->partition.n;
    }
    if (ps) {
        if (qos->partition.n != 0) {
            *ps = dds_alloc(sizeof(char*) * qos->partition.n);
            for (uint32_t i = 0; i < qos->partition.n; i++) {
                (*ps)[i] = dds_string_dup(qos->partition.strs[i]);
            }
        } else {
            *ps = NULL;
        }
    }
    return true;
}

bool dds_qget_reliability (const dds_qos_t * __restrict qos, dds_reliability_kind_t *kind, dds_duration_t *max_blocking_time)
{
    if (!qos || !(qos->present & QP_RELIABILITY)) {
        return false;
    }
    if (kind) {
        *kind = (dds_reliability_kind_t) qos->reliability.kind;
    }
    if (max_blocking_time) {
        *max_blocking_time = nn_from_ddsi_duration (qos->reliability.max_blocking_time);
    }
    return true;
}

bool dds_qget_transport_priority (const dds_qos_t * __restrict qos, int32_t *value)
{
    if (!qos || !(qos->present & QP_TRANSPORT_PRIORITY)) {
        return false;
    }
    if (value) {
        *value = qos->transport_priority.value;
    }
    return true;
}

bool dds_qget_destination_order (const dds_qos_t * __restrict qos, dds_destination_order_kind_t *kind)
{
    if (!qos || !(qos->present & QP_DESTINATION_ORDER)) {
        return false;
    }
    if (kind) {
        *kind = (dds_destination_order_kind_t) qos->destination_order.kind;
    }
    return true;
}

bool dds_qget_writer_data_lifecycle (const dds_qos_t * __restrict qos, bool *autodispose)
{
    if (!qos || !(qos->present & QP_PRISMTECH_WRITER_DATA_LIFECYCLE)) {
        return false;
    }
    if (autodispose) {
        *autodispose = qos->writer_data_lifecycle.autodispose_unregistered_instances;
    }
    return true;
}

bool dds_qget_reader_data_lifecycle (const dds_qos_t * __restrict qos, dds_duration_t *autopurge_nowriter_samples_delay, dds_duration_t *autopurge_disposed_samples_delay)
{
    if (!qos || !(qos->present & QP_PRISMTECH_READER_DATA_LIFECYCLE)) {
        return false;
    }
    if (autopurge_nowriter_samples_delay) {
        *autopurge_nowriter_samples_delay = nn_from_ddsi_duration (qos->reader_data_lifecycle.autopurge_nowriter_samples_delay);
    }
    if (autopurge_disposed_samples_delay) {
        *autopurge_disposed_samples_delay = nn_from_ddsi_duration (qos->reader_data_lifecycle.autopurge_disposed_samples_delay);
    }
    return true;
}

bool dds_qget_durability_service (const dds_qos_t * __restrict qos, dds_duration_t *service_cleanup_delay, dds_history_kind_t *history_kind, int32_t *history_depth, int32_t *max_samples, int32_t *max_instances, int32_t *max_samples_per_instance)
{
    if (!qos || !(qos->present & QP_DURABILITY_SERVICE)) {
        return false;
    }
    if (service_cleanup_delay) {
        *service_cleanup_delay = nn_from_ddsi_duration (qos->durability_service.service_cleanup_delay);
    }
    if (history_kind) {
        *history_kind = (dds_history_kind_t) qos->durability_service.history.kind;
    }
    if (history_depth) {
        *history_depth = qos->durability_service.history.depth;
    }
    if (max_samples) {
        *max_samples = qos->durability_service.resource_limits.max_samples;
    }
    if (max_instances) {
        *max_instances = qos->durability_service.resource_limits.max_instances;
    }
    if (max_samples_per_instance) {
        *max_samples_per_instance = qos->durability_service.resource_limits.max_samples_per_instance;
    }
    return true;
}
