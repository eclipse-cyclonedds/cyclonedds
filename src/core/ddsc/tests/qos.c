// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "CUnit/Test.h"
#include "dds/dds.h"
#include <assert.h>

/****************************************************************************
 * Convenience global policies
 ****************************************************************************/
struct pol_userdata {
    void *value;
    size_t sz;
};

struct pol_topicdata {
    void *value;
    size_t sz;
};

struct pol_groupdata {
    void *value;
    size_t sz;
};

struct pol_durability {
    dds_durability_kind_t kind;
};

struct pol_history {
    dds_history_kind_t kind;
    int32_t depth;
};

struct pol_resource_limits {
    int32_t max_samples;
    int32_t max_instances;
    int32_t max_samples_per_instance;
};

struct pol_presentation {
    dds_presentation_access_scope_kind_t access_scope;
    bool coherent_access;
    bool ordered_access;
};

struct pol_lifespan {
    dds_duration_t lifespan;
};

struct pol_deadline {
    dds_duration_t deadline;
};

struct pol_latency_budget {
    dds_duration_t duration;
};

struct pol_ownership {
    dds_ownership_kind_t kind;
};

struct pol_ownership_strength {
    int32_t value;
};

struct pol_liveliness {
    dds_liveliness_kind_t kind;
    dds_duration_t lease_duration;
};

struct pol_time_based_filter {
    dds_duration_t minimum_separation;
};

struct pol_partition {
    uint32_t n;
    char **ps;
};

struct pol_reliability {
    dds_reliability_kind_t kind;
    dds_duration_t max_blocking_time;
};

struct pol_transport_priority {
    int32_t value;
};

struct pol_destination_order {
    dds_destination_order_kind_t kind;
};

struct pol_writer_data_lifecycle {
    bool autodispose;
};

struct pol_reader_data_lifecycle {
    dds_duration_t autopurge_nowriter_samples_delay;
    dds_duration_t autopurge_disposed_samples_delay;
};

struct pol_durability_service {
    dds_duration_t service_cleanup_delay;
    dds_history_kind_t history_kind;
    int32_t history_depth;
    int32_t max_samples;
    int32_t max_instances;
    int32_t max_samples_per_instance;
};

struct pol_type_consistency_enforcement {
  dds_type_consistency_kind_t kind;
  bool ignore_sequence_bounds;
  bool ignore_string_bounds;
  bool ignore_member_names;
  bool prevent_type_widening;
  bool force_type_validation;
};

static struct pol_userdata g_pol_userdata;
static struct pol_topicdata g_pol_topicdata;
static struct pol_groupdata g_pol_groupdata;
static struct pol_durability g_pol_durability;
static struct pol_history g_pol_history;
static struct pol_resource_limits g_pol_resource_limits;
static struct pol_presentation g_pol_presentation;
static struct pol_lifespan g_pol_lifespan;
static struct pol_deadline g_pol_deadline;
static struct pol_latency_budget g_pol_latency_budget;
static struct pol_ownership g_pol_ownership;
static struct pol_ownership_strength g_pol_ownership_strength;
static struct pol_liveliness g_pol_liveliness;
static struct pol_time_based_filter g_pol_time_based_filter;
static struct pol_partition g_pol_partition;
static struct pol_reliability g_pol_reliability;
static struct pol_transport_priority g_pol_transport_priority;
static struct pol_destination_order g_pol_destination_order;
static struct pol_writer_data_lifecycle g_pol_writer_data_lifecycle;
static struct pol_reader_data_lifecycle g_pol_reader_data_lifecycle;
static struct pol_durability_service g_pol_durability_service;
static struct pol_type_consistency_enforcement g_pol_type_consistency_enforcement;


static const char* c_userdata  = "user_key";
static const char* c_topicdata = "topic_key";
static const char* c_groupdata = "group_key";
static const char* c_partitions[] = {"Partition1", "Partition2"};
static const char* c_property_names[] = {"prop1", "prop2", "prop3"};
static const char* c_property_values[] = {"val1", "val2", "val3"};
static const char* c_bproperty_names[] = {"bprop1", "bprop2", "bprop3"};
static const unsigned char c_bproperty_values[3][3] = {{0x0, 0x1, 0x2}, {0x2, 0x3, 0x4}, {0x5, 0x6, 0x7}};


/****************************************************************************
 * Test initializations and teardowns.
 ****************************************************************************/
static dds_qos_t *g_qos = NULL;

static void
qos_init(void)
{
    g_qos = dds_create_qos();
    CU_ASSERT_PTR_NOT_NULL_FATAL(g_qos);

    g_pol_userdata.value = (void*)c_userdata;
    g_pol_userdata.sz = strlen((char*)g_pol_userdata.value) + 1;

    g_pol_topicdata.value = (void*)c_topicdata;
    g_pol_topicdata.sz = strlen((char*)g_pol_topicdata.value) + 1;

    g_pol_groupdata.value = (void*)c_groupdata;
    g_pol_groupdata.sz = strlen((char*)g_pol_groupdata.value) + 1;

    g_pol_durability.kind = DDS_DURABILITY_TRANSIENT;

    g_pol_history.kind  = DDS_HISTORY_KEEP_LAST;
    g_pol_history.depth = 1;

    g_pol_resource_limits.max_samples = 1;
    g_pol_resource_limits.max_instances = 1;
    g_pol_resource_limits.max_samples_per_instance = 1;

    g_pol_presentation.access_scope = DDS_PRESENTATION_INSTANCE;
    g_pol_presentation.coherent_access = true;
    g_pol_presentation.ordered_access = true;

    g_pol_lifespan.lifespan = 10000;

    g_pol_deadline.deadline = 20000;

    g_pol_latency_budget.duration = 30000;

    g_pol_ownership.kind = DDS_OWNERSHIP_EXCLUSIVE;

    g_pol_ownership_strength.value = 10;

    g_pol_liveliness.kind = DDS_LIVELINESS_AUTOMATIC;
    g_pol_liveliness.lease_duration = 40000;

    g_pol_time_based_filter.minimum_separation = 50000;

    g_pol_partition.ps = (char**)c_partitions;
    g_pol_partition.n  = 2;

    g_pol_reliability.kind = DDS_RELIABILITY_RELIABLE;
    g_pol_reliability.max_blocking_time = 60000;

    g_pol_transport_priority.value = 42;

    g_pol_destination_order.kind = DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP;

    g_pol_writer_data_lifecycle.autodispose = true;

    g_pol_reader_data_lifecycle.autopurge_disposed_samples_delay = 70000;
    g_pol_reader_data_lifecycle.autopurge_nowriter_samples_delay = 80000;

    g_pol_durability_service.history_depth = 1;
    g_pol_durability_service.history_kind = DDS_HISTORY_KEEP_LAST;
    g_pol_durability_service.max_samples = 2;
    g_pol_durability_service.max_instances = 3;
    g_pol_durability_service.max_samples_per_instance = 4;
    g_pol_durability_service.service_cleanup_delay = 90000;

    g_pol_type_consistency_enforcement.kind = DDS_TYPE_CONSISTENCY_ALLOW_TYPE_COERCION;
    g_pol_type_consistency_enforcement.ignore_sequence_bounds = true;
    g_pol_type_consistency_enforcement.ignore_string_bounds = false;
    g_pol_type_consistency_enforcement.ignore_member_names = true;
    g_pol_type_consistency_enforcement.prevent_type_widening = false;
    g_pol_type_consistency_enforcement.force_type_validation = true;
}

static void
qos_fini(void)
{
    dds_delete_qos(g_qos);
}

/****************************************************************************
 * API tests
 ****************************************************************************/
CU_Test(ddsc_qos, copy_bad_source, .init=qos_init, .fini=qos_fini)
{
    dds_return_t result;

        result = dds_copy_qos(g_qos, NULL);
        CU_ASSERT_EQUAL_FATAL(result, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_qos, copy_bad_destination, .init=qos_init, .fini=qos_fini)
{
        dds_return_t result;

        result = dds_copy_qos(NULL, g_qos);
        CU_ASSERT_EQUAL_FATAL(result, DDS_RETCODE_BAD_PARAMETER);
}

CU_Test(ddsc_qos, copy_with_partition, .init=qos_init, .fini=qos_fini)
{
        dds_return_t result;
        dds_qos_t *qos;
        struct pol_partition p = { 0, NULL };

        qos = dds_create_qos();
        CU_ASSERT_PTR_NOT_NULL_FATAL(qos);

        dds_qset_partition(g_qos, g_pol_partition.n, (const char **)g_pol_partition.ps);
        result = dds_copy_qos(qos, g_qos);

        CU_ASSERT_EQUAL_FATAL(result, DDS_RETCODE_OK);
        dds_qget_partition(qos, &p.n, &p.ps);
        CU_ASSERT_EQUAL_FATAL(p.n, g_pol_partition.n);

        for (uint32_t cnt = 0; cnt < p.n; cnt++) {
            CU_ASSERT_STRING_EQUAL_FATAL(p.ps[cnt], g_pol_partition.ps[cnt]);
        }

        for (uint32_t cnt = 0; cnt < p.n; cnt++) {
            dds_free (p.ps[cnt]);
        }
        dds_free (p.ps);
        dds_delete_qos(qos);
}

CU_Test(ddsc_qos, userdata, .init=qos_init, .fini=qos_fini)
{
    struct pol_userdata p = { NULL, 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_userdata(NULL, g_pol_userdata.value, g_pol_userdata.sz);
    dds_qget_userdata(NULL, &p.value, &p.sz);
    dds_qget_userdata(g_qos, NULL, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_userdata(g_qos, g_pol_userdata.value, g_pol_userdata.sz);
    dds_qget_userdata(g_qos, &p.value, &p.sz);
    CU_ASSERT_EQUAL_FATAL(p.sz, g_pol_userdata.sz);
    CU_ASSERT_STRING_EQUAL_FATAL(p.value, g_pol_userdata.value);

    dds_free(p.value);
}

CU_Test(ddsc_qos, topicdata, .init=qos_init, .fini=qos_fini)
{
    struct pol_topicdata p = { NULL, 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_topicdata(NULL, g_pol_topicdata.value, g_pol_topicdata.sz);
    dds_qget_topicdata(NULL, &p.value, &p.sz);
    dds_qget_topicdata(g_qos, NULL, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_topicdata(g_qos, g_pol_topicdata.value, g_pol_topicdata.sz);
    dds_qget_topicdata(g_qos, &p.value, &p.sz);
    CU_ASSERT_EQUAL_FATAL(p.sz, g_pol_topicdata.sz);
    CU_ASSERT_STRING_EQUAL_FATAL(p.value, g_pol_topicdata.value);

    dds_free(p.value);
}

CU_Test(ddsc_qos, groupdata, .init=qos_init, .fini=qos_fini)
{
    struct pol_groupdata p = { NULL, 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_groupdata(NULL, g_pol_groupdata.value, g_pol_groupdata.sz);
    dds_qget_groupdata(NULL, &p.value, &p.sz);
    dds_qget_groupdata(g_qos, NULL, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_groupdata(g_qos, g_pol_groupdata.value, g_pol_groupdata.sz);
    dds_qget_groupdata(g_qos, &p.value, &p.sz);
    CU_ASSERT_EQUAL_FATAL(p.sz, g_pol_groupdata.sz);
    CU_ASSERT_STRING_EQUAL_FATAL(p.value, g_pol_groupdata.value);

    dds_free(p.value);
}

CU_Test(ddsc_qos, durability, .init=qos_init, .fini=qos_fini)
{
    struct pol_durability p = { DDS_DURABILITY_VOLATILE };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_durability(NULL, g_pol_durability.kind);
    dds_qget_durability(NULL, &p.kind);
    dds_qget_durability(g_qos, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_durability(g_qos, g_pol_durability.kind);
    dds_qget_durability(g_qos, &p.kind);
    CU_ASSERT_EQUAL_FATAL(p.kind, g_pol_durability.kind);
}

CU_Test(ddsc_qos, history, .init=qos_init, .fini=qos_fini)
{
    struct pol_history p = { DDS_HISTORY_KEEP_ALL, 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_history(NULL, g_pol_history.kind, g_pol_history.depth);
    dds_qget_history(NULL, &p.kind, &p.depth);
    dds_qget_history(g_qos, NULL, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_history(g_qos, g_pol_history.kind, g_pol_history.depth);
    dds_qget_history(g_qos, &p.kind, &p.depth);
    CU_ASSERT_EQUAL_FATAL(p.kind, g_pol_history.kind);
    CU_ASSERT_EQUAL_FATAL(p.depth, g_pol_history.depth);
}

CU_Test(ddsc_qos, resource_limits, .init=qos_init, .fini=qos_fini)
{
    struct pol_resource_limits p = { 0, 0, 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_resource_limits(NULL, g_pol_resource_limits.max_samples, g_pol_resource_limits.max_instances, g_pol_resource_limits.max_samples_per_instance);
    dds_qget_resource_limits(NULL, &p.max_samples, &p.max_instances, &p.max_samples_per_instance);
    dds_qget_resource_limits(g_qos, NULL, NULL, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_resource_limits(g_qos, g_pol_resource_limits.max_samples, g_pol_resource_limits.max_instances, g_pol_resource_limits.max_samples_per_instance);
    dds_qget_resource_limits(g_qos, &p.max_samples, &p.max_instances, &p.max_samples_per_instance);
    CU_ASSERT_EQUAL_FATAL(p.max_samples, g_pol_resource_limits.max_samples);
    CU_ASSERT_EQUAL_FATAL(p.max_instances, g_pol_resource_limits.max_instances);
    CU_ASSERT_EQUAL_FATAL(p.max_samples_per_instance, g_pol_resource_limits.max_samples_per_instance);
}

CU_Test(ddsc_qos, presentation, .init=qos_init, .fini=qos_fini)
{
    struct pol_presentation p = { DDS_PRESENTATION_INSTANCE, false, false };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_presentation(NULL, g_pol_presentation.access_scope, g_pol_presentation.coherent_access, g_pol_presentation.ordered_access);
    dds_qget_presentation(NULL, &p.access_scope, &p.coherent_access, &p.ordered_access);
    dds_qget_presentation(g_qos, NULL, NULL, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_presentation(g_qos, g_pol_presentation.access_scope, g_pol_presentation.coherent_access, g_pol_presentation.ordered_access);
    dds_qget_presentation(g_qos, &p.access_scope, &p.coherent_access, &p.ordered_access);
    CU_ASSERT_EQUAL_FATAL(p.access_scope, g_pol_presentation.access_scope);
    CU_ASSERT_EQUAL_FATAL(p.coherent_access, g_pol_presentation.coherent_access);
    CU_ASSERT_EQUAL_FATAL(p.ordered_access, g_pol_presentation.ordered_access);
}

CU_Test(ddsc_qos, lifespan, .init=qos_init, .fini=qos_fini)
{
    struct pol_lifespan p = { 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_lifespan(NULL, g_pol_lifespan.lifespan);
    dds_qget_lifespan(NULL, &p.lifespan);
    dds_qget_lifespan(g_qos, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_lifespan(g_qos, g_pol_lifespan.lifespan);
    dds_qget_lifespan(g_qos, &p.lifespan);
    CU_ASSERT_EQUAL_FATAL(p.lifespan, g_pol_lifespan.lifespan);
}

CU_Test(ddsc_qos, deadline, .init=qos_init, .fini=qos_fini)
{
    struct pol_deadline p = { 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_deadline(NULL, g_pol_deadline.deadline);
    dds_qget_deadline(NULL, &p.deadline);
    dds_qget_deadline(g_qos, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_deadline(g_qos, g_pol_deadline.deadline);
    dds_qget_deadline(g_qos, &p.deadline);
    CU_ASSERT_EQUAL_FATAL(p.deadline, g_pol_deadline.deadline);
}

CU_Test(ddsc_qos, latency_budget, .init=qos_init, .fini=qos_fini)
{
    struct pol_latency_budget p = { 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_latency_budget(NULL, g_pol_latency_budget.duration);
    dds_qget_latency_budget(NULL, &p.duration);
    dds_qget_latency_budget(g_qos, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_latency_budget(g_qos, g_pol_latency_budget.duration);
    dds_qget_latency_budget(g_qos, &p.duration);
    CU_ASSERT_EQUAL_FATAL(p.duration, g_pol_latency_budget.duration);
}

CU_Test(ddsc_qos, ownership, .init=qos_init, .fini=qos_fini)
{
    struct pol_ownership p = { DDS_OWNERSHIP_SHARED };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_ownership(NULL, g_pol_ownership.kind);
    dds_qget_ownership(NULL, &p.kind);
    dds_qget_ownership(g_qos, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_ownership(g_qos, g_pol_ownership.kind);
    dds_qget_ownership(g_qos, &p.kind);
    CU_ASSERT_EQUAL_FATAL(p.kind, g_pol_ownership.kind);
}

CU_Test(ddsc_qos, ownership_strength, .init=qos_init, .fini=qos_fini)
{
    struct pol_ownership_strength p = { 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_ownership_strength(NULL, g_pol_ownership_strength.value);
    dds_qget_ownership_strength(NULL, &p.value);
    dds_qget_ownership_strength(g_qos, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_ownership_strength(g_qos, g_pol_ownership_strength.value);
    dds_qget_ownership_strength(g_qos, &p.value);
    CU_ASSERT_EQUAL_FATAL(p.value, g_pol_ownership_strength.value);
}

CU_Test(ddsc_qos, liveliness, .init=qos_init, .fini=qos_fini)
{
    struct pol_liveliness p = { DDS_LIVELINESS_AUTOMATIC, 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_liveliness(NULL, g_pol_liveliness.kind, g_pol_liveliness.lease_duration);
    dds_qget_liveliness(NULL, &p.kind, &p.lease_duration);
    dds_qget_liveliness(g_qos, NULL, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_liveliness(g_qos, g_pol_liveliness.kind, g_pol_liveliness.lease_duration);
    dds_qget_liveliness(g_qos, &p.kind, &p.lease_duration);
    CU_ASSERT_EQUAL_FATAL(p.kind, g_pol_liveliness.kind);
    CU_ASSERT_EQUAL_FATAL(p.lease_duration, g_pol_liveliness.lease_duration);
}

CU_Test(ddsc_qos, time_base_filter, .init=qos_init, .fini=qos_fini)
{
    struct pol_time_based_filter p = { 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_time_based_filter(NULL, g_pol_time_based_filter.minimum_separation);
    dds_qget_time_based_filter(NULL, &p.minimum_separation);
    dds_qget_time_based_filter(g_qos, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_time_based_filter(g_qos, g_pol_time_based_filter.minimum_separation);
    dds_qget_time_based_filter(g_qos, &p.minimum_separation);
    CU_ASSERT_EQUAL_FATAL(p.minimum_separation, g_pol_time_based_filter.minimum_separation);
}

CU_Test(ddsc_qos, partition, .init=qos_init, .fini=qos_fini)
{
    struct pol_partition p = { 0, NULL };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_partition(NULL, g_pol_partition.n, c_partitions);
    dds_qget_partition(NULL, &p.n, &p.ps);
    dds_qget_partition(g_qos, NULL, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_partition(g_qos, g_pol_partition.n, c_partitions);
    dds_qget_partition(g_qos, &p.n, &p.ps);
    CU_ASSERT_EQUAL_FATAL(p.n, 2);
    CU_ASSERT_EQUAL_FATAL(p.n, g_pol_partition.n);
    CU_ASSERT_STRING_EQUAL_FATAL(p.ps[0], g_pol_partition.ps[0]);
    CU_ASSERT_STRING_EQUAL_FATAL(p.ps[1], g_pol_partition.ps[1]);

    dds_free(p.ps[0]);
    dds_free(p.ps[1]);
    dds_free(p.ps);
}

CU_Test(ddsc_qos, reliability, .init=qos_init, .fini=qos_fini)
{
    struct pol_reliability p = { DDS_RELIABILITY_BEST_EFFORT, 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_reliability(NULL, g_pol_reliability.kind, g_pol_reliability.max_blocking_time);
    dds_qget_reliability(NULL, &p.kind, &p.max_blocking_time);
    dds_qget_reliability(g_qos, NULL, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_reliability(g_qos, g_pol_reliability.kind, g_pol_reliability.max_blocking_time);
    dds_qget_reliability(g_qos, &p.kind, &p.max_blocking_time);
    CU_ASSERT_EQUAL_FATAL(p.kind, g_pol_reliability.kind);
    CU_ASSERT_EQUAL_FATAL(p.max_blocking_time, g_pol_reliability.max_blocking_time);
}

CU_Test(ddsc_qos, transport_priority, .init=qos_init, .fini=qos_fini)
{
    struct pol_transport_priority p = { 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_transport_priority(NULL, g_pol_transport_priority.value);
    dds_qget_transport_priority(NULL, &p.value);
    dds_qget_transport_priority(g_qos, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_transport_priority(g_qos, g_pol_transport_priority.value);
    dds_qget_transport_priority(g_qos, &p.value);
    CU_ASSERT_EQUAL_FATAL(p.value, g_pol_transport_priority.value);
}

CU_Test(ddsc_qos, destination_order, .init=qos_init, .fini=qos_fini)
{
    struct pol_destination_order p = { DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_destination_order(NULL, g_pol_destination_order.kind);
    dds_qget_destination_order(NULL, &p.kind);
    dds_qget_destination_order(g_qos, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_destination_order(g_qos, g_pol_destination_order.kind);
    dds_qget_destination_order(g_qos, &p.kind);
    CU_ASSERT_EQUAL_FATAL(p.kind, g_pol_destination_order.kind);
}

CU_Test(ddsc_qos, writer_data_lifecycle, .init=qos_init, .fini=qos_fini)
{
    struct pol_writer_data_lifecycle p = { false };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_writer_data_lifecycle(NULL, g_pol_writer_data_lifecycle.autodispose);
    dds_qget_writer_data_lifecycle(NULL, &p.autodispose);
    dds_qget_writer_data_lifecycle(g_qos, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_writer_data_lifecycle(g_qos, g_pol_writer_data_lifecycle.autodispose);
    dds_qget_writer_data_lifecycle(g_qos, &p.autodispose);
    CU_ASSERT_EQUAL_FATAL(p.autodispose, g_pol_writer_data_lifecycle.autodispose);
}

CU_Test(ddsc_qos, reader_data_lifecycle, .init=qos_init, .fini=qos_fini)
{
    struct pol_reader_data_lifecycle p = { 0, 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_reader_data_lifecycle(NULL, g_pol_reader_data_lifecycle.autopurge_nowriter_samples_delay, g_pol_reader_data_lifecycle.autopurge_disposed_samples_delay);
    dds_qget_reader_data_lifecycle(NULL, &p.autopurge_nowriter_samples_delay, &p.autopurge_disposed_samples_delay);
    dds_qget_reader_data_lifecycle(g_qos, NULL, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_reader_data_lifecycle(g_qos, g_pol_reader_data_lifecycle.autopurge_nowriter_samples_delay, g_pol_reader_data_lifecycle.autopurge_disposed_samples_delay);
    dds_qget_reader_data_lifecycle(g_qos, &p.autopurge_nowriter_samples_delay, &p.autopurge_disposed_samples_delay);
    CU_ASSERT_EQUAL_FATAL(p.autopurge_nowriter_samples_delay, g_pol_reader_data_lifecycle.autopurge_nowriter_samples_delay);
    CU_ASSERT_EQUAL_FATAL(p.autopurge_disposed_samples_delay, g_pol_reader_data_lifecycle.autopurge_disposed_samples_delay);
}

CU_Test(ddsc_qos, durability_service, .init=qos_init, .fini=qos_fini)
{
    struct pol_durability_service p = { 0, DDS_HISTORY_KEEP_LAST, 0, 0, 0, 0 };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_durability_service(NULL,
            g_pol_durability_service.service_cleanup_delay,
            g_pol_durability_service.history_kind,
            g_pol_durability_service.history_depth,
            g_pol_durability_service.max_samples,
            g_pol_durability_service.max_instances,
            g_pol_durability_service.max_samples_per_instance);
    dds_qget_durability_service(NULL,
            &p.service_cleanup_delay,
            &p.history_kind,
            &p.history_depth,
            &p.max_samples,
            &p.max_instances,
            &p.max_samples_per_instance);
    dds_qget_durability_service(g_qos,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL,
            NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_durability_service(g_qos,
            g_pol_durability_service.service_cleanup_delay,
            g_pol_durability_service.history_kind,
            g_pol_durability_service.history_depth,
            g_pol_durability_service.max_samples,
            g_pol_durability_service.max_instances,
            g_pol_durability_service.max_samples_per_instance);
    dds_qget_durability_service(g_qos,
            &p.service_cleanup_delay,
            &p.history_kind,
            &p.history_depth,
            &p.max_samples,
            &p.max_instances,
            &p.max_samples_per_instance);
    CU_ASSERT_EQUAL_FATAL(p.service_cleanup_delay, g_pol_durability_service.service_cleanup_delay);
    CU_ASSERT_EQUAL_FATAL(p.history_kind, g_pol_durability_service.history_kind);
    CU_ASSERT_EQUAL_FATAL(p.history_depth, g_pol_durability_service.history_depth);
    CU_ASSERT_EQUAL_FATAL(p.max_samples, g_pol_durability_service.max_samples);
    CU_ASSERT_EQUAL_FATAL(p.max_instances, g_pol_durability_service.max_instances);
    CU_ASSERT_EQUAL_FATAL(p.max_samples_per_instance, g_pol_durability_service.max_samples_per_instance);
}

CU_Test(ddsc_qos, property, .init=qos_init, .fini=qos_fini)
{
    char * value = NULL;
    char ** names = NULL;
    uint32_t cnt = 0;

    /* NULLs shouldn't crash and be a noops. */
    CU_ASSERT_FATAL (!dds_qget_prop (g_qos, NULL, NULL));
    CU_ASSERT_FATAL (!dds_qget_prop (g_qos, c_property_names[0], NULL));
    CU_ASSERT_FATAL (!dds_qget_prop (g_qos, NULL, &value));
    CU_ASSERT_FATAL (!dds_qget_prop (NULL, c_property_names[0], &value));

    dds_qset_prop (g_qos, NULL, NULL);
    dds_qset_prop (g_qos, NULL, c_property_values[0]);
    dds_qset_prop (NULL, c_property_names[0], c_property_values[0]);

    /* Set null value should not succeed, setting empty string should */
    dds_qset_prop (g_qos, c_property_names[0], NULL);
    CU_ASSERT_FATAL (!dds_qget_prop (g_qos, c_property_names[0], &value));
    dds_qset_prop (g_qos, c_property_names[0], "");
    CU_ASSERT_FATAL (dds_qget_prop (g_qos, c_property_names[0], &value));
    CU_ASSERT_STRING_EQUAL_FATAL (value, "");
    dds_free (value);

    /* Getting after setting, should yield the original input. */
    dds_qset_prop (g_qos, c_property_names[0], c_property_values[0]);
    CU_ASSERT_FATAL (dds_qget_prop (g_qos, c_property_names[0], &value));
    CU_ASSERT_STRING_EQUAL_FATAL (value, c_property_values[0]);
    dds_free (value);

    /* Overwrite value for existing property (and reset value) */
    dds_qset_prop (g_qos, c_property_names[0], c_property_values[1]);
    CU_ASSERT_FATAL (dds_qget_prop (g_qos, c_property_names[0], &value));
    CU_ASSERT_STRING_EQUAL_FATAL (value, c_property_values[1]);
    dds_free (value);
    dds_qset_prop (g_qos, c_property_names[0], c_property_values[0]);

    /* Set 2nd prop and get length */
    dds_qset_prop (g_qos, c_property_names[1], c_property_values[1]);
    CU_ASSERT_FATAL (dds_qget_propnames (g_qos, &cnt, NULL));
    CU_ASSERT_EQUAL_FATAL (cnt, 2);

    /* Set another property and get list of property names */
    dds_qset_prop (g_qos, c_property_names[2], c_property_values[2]);
    CU_ASSERT_FATAL (dds_qget_propnames (g_qos, &cnt, &names));
    CU_ASSERT_EQUAL_FATAL (cnt, 3);
    for (uint32_t i = 0; i < cnt; i++)
    {
        CU_ASSERT_STRING_EQUAL_FATAL (names[i], c_property_names[i]);
        dds_free (names[i]);
    }
    dds_free (names);

    /* Unset a property and check if removed */
    dds_qunset_prop (g_qos, c_property_names[1]);
    CU_ASSERT_FATAL (!dds_qget_prop (g_qos, c_property_names[1], &value));
    CU_ASSERT_FATAL (dds_qget_propnames (g_qos, &cnt, NULL));
    CU_ASSERT_EQUAL_FATAL (cnt, 2);
    CU_ASSERT_FATAL(dds_qget_prop (g_qos, c_property_names[0], &value));
    CU_ASSERT_STRING_EQUAL_FATAL (value, c_property_values[0]);
    dds_free (value);
    CU_ASSERT_FATAL (dds_qget_prop (g_qos, c_property_names[2], &value));
    CU_ASSERT_STRING_EQUAL_FATAL (value, c_property_values[2]);
    dds_free (value);
    dds_qunset_prop (g_qos, c_property_names[0]);
    dds_qunset_prop (g_qos, c_property_names[2]);
    CU_ASSERT_FATAL (!dds_qget_propnames (g_qos, &cnt, NULL));
}

CU_Test(ddsc_qos, bproperty, .init=qos_init, .fini=qos_fini)
{
    void * bvalue = NULL;
    size_t size = 0;
    char ** names = NULL;
    uint32_t cnt = 0;

    /* NULLs shouldn't crash and be a noops. */
    CU_ASSERT_FATAL (!dds_qget_bprop (g_qos, NULL, NULL, NULL));
    CU_ASSERT_FATAL (!dds_qget_bprop (g_qos, c_bproperty_names[0], NULL, NULL));
    CU_ASSERT_FATAL (!dds_qget_bprop (g_qos, NULL, &bvalue, &size));
    CU_ASSERT_FATAL (!dds_qget_bprop (NULL, c_bproperty_names[0], &bvalue, &size));

    dds_qset_bprop (g_qos, NULL, NULL, 0);
    dds_qset_bprop (g_qos, NULL, &c_bproperty_values[0], 0);
    dds_qset_bprop (NULL, c_bproperty_names[0], c_bproperty_values[0], 0);

    /* Set null value should succeed */
    dds_qset_bprop (g_qos, c_bproperty_names[0], NULL, 0);
    CU_ASSERT_FATAL (dds_qget_bprop (g_qos, c_bproperty_names[0], &bvalue, &size));
    CU_ASSERT_EQUAL_FATAL (bvalue, NULL);
    CU_ASSERT_EQUAL_FATAL (size, 0);

    /* Getting after setting, should yield the original input. */
    dds_qset_bprop (g_qos, c_bproperty_names[0], c_bproperty_values[0], 3);
    CU_ASSERT_FATAL(dds_qget_bprop (g_qos, c_bproperty_names[0], &bvalue, &size));
    CU_ASSERT_FATAL (bvalue != NULL);
    CU_ASSERT_EQUAL_FATAL (size, 3);
    assert (c_bproperty_values[0] != NULL); /* for Clang static analyzer */
    CU_ASSERT_EQUAL_FATAL (memcmp (bvalue, c_bproperty_values[0], size), 0);
    dds_free (bvalue);

    /* Overwrite value for existing binary property (and reset value) */
    dds_qset_bprop (g_qos, c_bproperty_names[0], c_bproperty_values[1], 3);
    CU_ASSERT_FATAL (dds_qget_bprop (g_qos, c_bproperty_names[0], &bvalue, &size));
    CU_ASSERT_FATAL (bvalue != NULL);
    CU_ASSERT_EQUAL_FATAL (size, 3);
    assert (c_bproperty_values[1] != NULL); /* for Clang static analyzer */
    CU_ASSERT_EQUAL_FATAL (memcmp (bvalue, c_bproperty_values[1], size), 0);
    dds_free (bvalue);
    dds_qset_bprop (g_qos, c_bproperty_names[0], &c_bproperty_values[0], 3);

    /* Set 2nd binary prop and get length */
    dds_qset_bprop (g_qos, c_bproperty_names[1], &c_bproperty_values[1], 3);
    CU_ASSERT_FATAL (dds_qget_bpropnames (g_qos, &cnt, NULL));
    CU_ASSERT_EQUAL_FATAL (cnt, 2);

    /* Set another binary property and get list of property names */
    dds_qset_bprop (g_qos, c_bproperty_names[2], &c_bproperty_values[2], 3);
    CU_ASSERT_FATAL (dds_qget_bpropnames (g_qos, &cnt, &names));
    CU_ASSERT_EQUAL_FATAL (cnt, 3);
    for (uint32_t i = 0; i < cnt; i++)
    {
        CU_ASSERT_STRING_EQUAL_FATAL (names[i], c_bproperty_names[i]);
        dds_free (names[i]);
    }
    dds_free (names);

    /* Unset a binary property and check if removed */
    dds_qunset_bprop (g_qos, c_bproperty_names[1]);
    CU_ASSERT_FATAL (!dds_qget_bprop (g_qos, c_bproperty_names[1], &bvalue, &size));
    CU_ASSERT_FATAL (dds_qget_bpropnames (g_qos, &cnt, NULL));
    CU_ASSERT_EQUAL_FATAL (cnt, 2);
    CU_ASSERT_FATAL (dds_qget_bprop (g_qos, c_bproperty_names[0], &bvalue, &size));
    CU_ASSERT_FATAL (bvalue != NULL);
    CU_ASSERT_EQUAL_FATAL (size, 3);
    assert (c_bproperty_values[0] != NULL); /* for Clang static analyzer */
    CU_ASSERT_EQUAL_FATAL (memcmp (bvalue, c_bproperty_values[0], size), 0);
    dds_free (bvalue);
    CU_ASSERT_FATAL (dds_qget_bprop (g_qos, c_bproperty_names[2], &bvalue, &size));
    CU_ASSERT_FATAL (bvalue != NULL);
    CU_ASSERT_EQUAL_FATAL (size, 3);
    assert (c_bproperty_values[2] != NULL); /* for Clang static analyzer */
    CU_ASSERT_EQUAL_FATAL (memcmp (bvalue, c_bproperty_values[2], size), 0);
    dds_free (bvalue);
    dds_qunset_bprop (g_qos, c_bproperty_names[0]);
    dds_qunset_bprop (g_qos, c_bproperty_names[2]);
    CU_ASSERT_FATAL (!dds_qget_bpropnames (g_qos, &cnt, NULL));
}

CU_Test(ddsc_qos, property_mixed, .init=qos_init, .fini=qos_fini)
{
    char * value = NULL;
    void * bvalue = NULL;
    size_t size = 0;
    uint32_t cnt = 0;

    /* Set property and binary property with same name */
    dds_qset_prop (g_qos, c_property_names[0], c_property_values[0]);
    dds_qset_bprop (g_qos, c_property_names[0], c_bproperty_values[0], 3);

    /* Check property values and count */
    CU_ASSERT_FATAL (dds_qget_bprop (g_qos, c_property_names[0], &bvalue, &size));
    CU_ASSERT_FATAL (bvalue != NULL);
    CU_ASSERT_EQUAL_FATAL (size, 3);
    assert (c_bproperty_values[0] != NULL); /* for Clang static analyzer */
    CU_ASSERT_EQUAL_FATAL (memcmp (bvalue, c_bproperty_values[0], size), 0);
    dds_free (bvalue);
    CU_ASSERT_FATAL (dds_qget_prop (g_qos, c_property_names[0], &value));
    CU_ASSERT_STRING_EQUAL_FATAL (value, c_property_values[0]);
    dds_free (value);

    CU_ASSERT_FATAL (dds_qget_propnames (g_qos, &cnt, NULL));
    CU_ASSERT_EQUAL_FATAL (cnt, 1);
    CU_ASSERT_FATAL (dds_qget_bpropnames (g_qos, &cnt, NULL));
    CU_ASSERT_EQUAL_FATAL (cnt, 1);

    /* Unset and check */
    dds_qunset_bprop (g_qos, c_property_names[0]);
    CU_ASSERT_FATAL (!dds_qget_bprop (g_qos, c_property_names[0], &bvalue, &size));
    CU_ASSERT_FATAL (dds_qget_prop (g_qos, c_property_names[0], &value));
    CU_ASSERT_STRING_EQUAL_FATAL (value, c_property_values[0]);
    dds_free (value);

    dds_qunset_prop (g_qos, c_property_names[0]);
    CU_ASSERT_FATAL (!dds_qget_prop (g_qos, c_property_names[0], &value));
    CU_ASSERT_FATAL (!dds_qget_propnames (g_qos, &cnt, NULL));
    CU_ASSERT_FATAL (!dds_qget_bpropnames (g_qos, &cnt, NULL));
}

CU_Test(ddsc_qos, type_consistency, .init=qos_init, .fini=qos_fini)
{
    struct pol_type_consistency_enforcement p = { DDS_TYPE_CONSISTENCY_ALLOW_TYPE_COERCION, true, false, true, false, true };

    /* NULLs shouldn't crash and be a noops. */
    dds_qset_type_consistency(NULL,
        g_pol_type_consistency_enforcement.kind,
        g_pol_type_consistency_enforcement.ignore_sequence_bounds,
        g_pol_type_consistency_enforcement.ignore_string_bounds,
        g_pol_type_consistency_enforcement.ignore_member_names,
        g_pol_type_consistency_enforcement.prevent_type_widening,
        g_pol_type_consistency_enforcement.force_type_validation);
    dds_qget_type_consistency(NULL,
        &p.kind,
        &p.ignore_sequence_bounds,
        &p.ignore_string_bounds,
        &p.ignore_member_names,
        &p.prevent_type_widening,
        &p.force_type_validation);
    dds_qget_type_consistency(g_qos, NULL, NULL, NULL, NULL, NULL, NULL);

    /* Getting after setting, should yield the original input. */
    dds_qset_type_consistency(g_qos,
        g_pol_type_consistency_enforcement.kind,
        g_pol_type_consistency_enforcement.ignore_sequence_bounds,
        g_pol_type_consistency_enforcement.ignore_string_bounds,
        g_pol_type_consistency_enforcement.ignore_member_names,
        g_pol_type_consistency_enforcement.prevent_type_widening,
        g_pol_type_consistency_enforcement.force_type_validation);
    dds_qget_type_consistency(g_qos,
        &p.kind,
        &p.ignore_sequence_bounds,
        &p.ignore_string_bounds,
        &p.ignore_member_names,
        &p.prevent_type_widening,
        &p.force_type_validation);
    CU_ASSERT_EQUAL_FATAL(p.kind, g_pol_type_consistency_enforcement.kind);
    CU_ASSERT_EQUAL_FATAL(p.ignore_sequence_bounds, g_pol_type_consistency_enforcement.ignore_sequence_bounds);
    CU_ASSERT_EQUAL_FATAL(p.ignore_string_bounds, g_pol_type_consistency_enforcement.ignore_string_bounds);
    CU_ASSERT_EQUAL_FATAL(p.ignore_member_names, g_pol_type_consistency_enforcement.ignore_member_names);
    CU_ASSERT_EQUAL_FATAL(p.prevent_type_widening, g_pol_type_consistency_enforcement.prevent_type_widening);
    CU_ASSERT_EQUAL_FATAL(p.force_type_validation, g_pol_type_consistency_enforcement.force_type_validation);
}
