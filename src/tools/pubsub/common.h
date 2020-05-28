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
#ifndef COMMON_H
#define COMMON_H

#include "dds/dds.h"
#include <assert.h>

#define DDS_USERDATA_QOS_POLICY_NAME                            "UserData"
#define DDS_DURABILITY_QOS_POLICY_NAME                          "Durability"
#define DDS_PRESENTATION_QOS_POLICY_NAME                        "Presentation"
#define DDS_DEADLINE_QOS_POLICY_NAME                            "Deadline"
#define DDS_LATENCYBUDGET_QOS_POLICY_NAME                       "LatencyBudget"
#define DDS_OWNERSHIP_QOS_POLICY_NAME                           "Ownership"
#define DDS_OWNERSHIPSTRENGTH_QOS_POLICY_NAME                   "OwnershipStrength"
#define DDS_LIVELINESS_QOS_POLICY_NAME                          "Liveliness"
#define DDS_TIMEBASEDFILTER_QOS_POLICY_NAME                     "TimeBasedFilter"
#define DDS_PARTITION_QOS_POLICY_NAME                           "Partition"
#define DDS_RELIABILITY_QOS_POLICY_NAME                         "Reliability"
#define DDS_DESTINATIONORDER_QOS_POLICY_NAME                    "DestinationOrder"
#define DDS_HISTORY_QOS_POLICY_NAME                             "History"
#define DDS_RESOURCELIMITS_QOS_POLICY_NAME                      "ResourceLimits"
#define DDS_ENTITYFACTORY_QOS_POLICY_NAME                       "EntityFactory"
#define DDS_WRITERDATALIFECYCLE_QOS_POLICY_NAME                 "WriterDataLifecycle"
#define DDS_READERDATALIFECYCLE_QOS_POLICY_NAME                 "ReaderDataLifecycle"
#define DDS_TOPICDATA_QOS_POLICY_NAME                           "TopicData"
#define DDS_GROUPDATA_QOS_POLICY_NAME                           "GroupData"
#define DDS_TRANSPORTPRIORITY_QOS_POLICY_NAME                   "TransportPriority"
#define DDS_LIFESPAN_QOS_POLICY_NAME                            "Lifespan"
#define DDS_DURABILITYSERVICE_QOS_POLICY_NAME                   "DurabilityService"
#define DDS_SUBSCRIPTIONKEY_QOS_POLICY_NAME                     "SubscriptionKey"
#define DDS_VIEWKEY_QOS_POLICY_NAME                             "ViewKey"
#define DDS_READERLIFESPAN_QOS_POLICY_NAME                      "ReaderLifespan"
#define DDS_SHARE_QOS_POLICY_NAME                               "Share"
#define DDS_SCHEDULING_QOS_POLICY_NAME                          "Scheduling"
#define DDS_PROPERTY_QOS_POLICY_NAME                            "Property"
#define DDS_TYPE_CONSISTENCY_ENFORCEMENT_QOS_POLICY_NAME        "TypeConsistencyEnforcement"

#define DDS_SUBSCRIPTIONKEY_QOS_POLICY_ID                       24
#define DDS_VIEWKEY_QOS_POLICY_ID                               25
#define DDS_READERLIFESPAN_QOS_POLICY_ID                        26
#define DDS_SHARE_QOS_POLICY_ID                                 27
#define DDS_SCHEDULING_QOS_POLICY_ID                            28

extern dds_entity_t dp;
extern const dds_topic_descriptor_t *ts_KeyedSeq;
extern const dds_topic_descriptor_t *ts_Keyed32;
extern const dds_topic_descriptor_t *ts_Keyed64;
extern const dds_topic_descriptor_t *ts_Keyed128;
extern const dds_topic_descriptor_t *ts_Keyed256;
extern const dds_topic_descriptor_t *ts_OneULong;
extern const char *saved_argv0;
extern const char *qos_arg_usagestr;

//#define BINS_LENGTH (8 * sizeof(unsigned long long) + 1)

//void nowll_as_ddstime(DDS_Time_t *t);
//void bindelta(unsigned long long *bins, unsigned long long d, unsigned repeat);
//void binprint(unsigned long long *bins, unsigned long long telapsed);

struct hist;
struct hist *hist_new(unsigned nbins, uint64_t binwidth, uint64_t bin0);
void hist_free(struct hist *h);
void hist_reset_minmax(struct hist *h);
void hist_reset(struct hist *h);
void hist_record(struct hist *h, uint64_t x, unsigned weight);
void hist_print(struct hist *h, dds_time_t dt, int reset);

void error(const char *fmt, ...);
#define error_abort(rc, ...) if (rc < DDS_RETCODE_OK) { error(__VA_ARGS__); DDS_ERR_CHECK(rc, DDS_CHECK_FAIL); }
#define error_report(rc, ...) if (rc < DDS_RETCODE_OK) { error(__VA_ARGS__); DDS_ERR_CHECK(rc, DDS_CHECK_REPORT); }
#define error_return(rc, ...) if (rc < DDS_RETCODE_OK) { error_report(rc, __VA_ARGS__); return; }
#define error_exit(...) { error(__VA_ARGS__); exit(2); }
#define os_error_exit(osres, ...) if (osres != DDS_RETCODE_OK) { error(__VA_ARGS__); exit(2); }

void save_argv0(const char *argv0);
int common_init(const char *argv0);
void common_fini(void);
int change_publisher_partitions(dds_entity_t pub, unsigned npartitions, const char *partitions[]);
int change_subscriber_partitions(dds_entity_t sub, unsigned npartitions, const char *partitions[]);
dds_entity_t new_publisher(dds_qos_t *q, unsigned npartitions, const char **partitions);
dds_entity_t new_subscriber(dds_qos_t *q, unsigned npartitions, const char **partitions);
dds_qos_t *new_tqos(void);
dds_qos_t *new_rdqos(dds_entity_t tp);
dds_qos_t *new_wrqos(dds_entity_t tp);
void set_infinite_dds_duration(dds_duration_t *dd);
int double_to_dds_duration(dds_duration_t *dd, double d);
dds_entity_t new_topic(const char *name, const dds_topic_descriptor_t *topicDesc, const dds_qos_t *q);
dds_entity_t new_datawriter(const dds_entity_t pub, const dds_entity_t tp, const dds_qos_t *q);
dds_entity_t new_datareader(const dds_entity_t sub, const dds_entity_t tp, const dds_qos_t *q);
dds_entity_t new_datawriter_listener(const dds_entity_t pub, const dds_entity_t tp, const dds_qos_t *q, const dds_listener_t *l);
dds_entity_t new_datareader_listener(const dds_entity_t sub, const dds_entity_t tp, const dds_qos_t *q, const dds_listener_t *l);
void qos_liveliness(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void qos_deadline(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void qos_durability(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void qos_history(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void qos_destination_order(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void qos_ownership(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void qos_transport_priority(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void qos_reliability(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void qos_resource_limits(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void qos_durability_service(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void qos_user_data(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void qos_latency_budget(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void qos_lifespan(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void qos_presentation(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void qos_autodispose_unregistered_instances(dds_entity_kind_t qt, dds_qos_t *q, const char *arg);
void set_qosprovider(const char *arg);
void setqos_from_args(dds_entity_kind_t qt, dds_qos_t *q, int n, const char *args[]);

bool dds_err_check (dds_return_t err, unsigned flags, const char *where);

#define DDS_CHECK_REPORT 0x01
#define DDS_CHECK_FAIL 0x02
#define DDS_CHECK_EXIT 0x04

#define dds_err_str(x) (dds_strretcode(x))

#define DDS_TO_STRING(n) #n
#define DDS_INT_TO_STRING(n) DDS_TO_STRING(n)
#define DDS_ERR_CHECK(e, f) (dds_err_check ((e), (f), __FILE__ ":" DDS_INT_TO_STRING(__LINE__)))

#endif
