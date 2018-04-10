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
#include "RoundTrip.h"
#include "Space.h"
#include "ddsc/dds.h"
#include "os/os.h"
#include "test-common.h"
#include <criterion/criterion.h>
#include <criterion/logging.h>

static dds_entity_t g_participant = 0;
static dds_entity_t g_subscriber  = 0;
static dds_entity_t g_publisher   = 0;
static dds_entity_t g_topic       = 0;

#define MAX_SAMPLES 1

static dds_sample_info_t g_info[MAX_SAMPLES];

static struct DDS_UserDataQosPolicy g_pol_userdata;
static struct DDS_TopicDataQosPolicy g_pol_topicdata;
static struct DDS_GroupDataQosPolicy g_pol_groupdata;
static struct DDS_DurabilityQosPolicy g_pol_durability;
static struct DDS_HistoryQosPolicy g_pol_history;
static struct DDS_ResourceLimitsQosPolicy g_pol_resource_limits;
static struct DDS_PresentationQosPolicy g_pol_presentation;
static struct DDS_LifespanQosPolicy g_pol_lifespan;
static struct DDS_DeadlineQosPolicy g_pol_deadline;
static struct DDS_LatencyBudgetQosPolicy g_pol_latency_budget;
static struct DDS_OwnershipQosPolicy g_pol_ownership;
static struct DDS_OwnershipStrengthQosPolicy g_pol_ownership_strength;
static struct DDS_LivelinessQosPolicy g_pol_liveliness;
static struct DDS_TimeBasedFilterQosPolicy g_pol_time_based_filter;
static struct DDS_PartitionQosPolicy g_pol_partition;
static struct DDS_ReliabilityQosPolicy g_pol_reliability;
static struct DDS_TransportPriorityQosPolicy g_pol_transport_priority;
static struct DDS_DestinationOrderQosPolicy g_pol_destination_order;
static struct DDS_WriterDataLifecycleQosPolicy g_pol_writer_data_lifecycle;
static struct DDS_ReaderDataLifecycleQosPolicy g_pol_reader_data_lifecycle;
static struct DDS_DurabilityServiceQosPolicy g_pol_durability_service;

static const char* c_userdata  = "user_key";
static const char* c_topicdata = "topic_key";
static const char* c_groupdata = "group_key";
static const char* c_partitions[] = {"Partition1", "Partition2"};

static dds_qos_t *g_qos = NULL;

static void
qos_init(void)
{
    g_qos = dds_qos_create();
    cr_assert_not_null(g_qos);

    g_pol_userdata.value._buffer = dds_alloc(strlen(c_userdata) + 1);
    g_pol_userdata.value._length = (uint32_t)strlen(c_userdata) + 1;
    g_pol_userdata.value._release = true;
    g_pol_userdata.value._maximum = 0;

    g_pol_topicdata.value._buffer = dds_alloc(strlen(c_topicdata) + 1);
    g_pol_topicdata.value._length = (uint32_t)strlen(c_topicdata) + 1;
    g_pol_topicdata.value._release = true;
    g_pol_topicdata.value._maximum = 0;

    g_pol_groupdata.value._buffer = dds_alloc(strlen(c_groupdata) + 1);
    g_pol_groupdata.value._length = (uint32_t)strlen(c_groupdata) + 1;
    g_pol_groupdata.value._release = true;
    g_pol_groupdata.value._maximum = 0;

    g_pol_history.kind  = DDS_KEEP_LAST_HISTORY_QOS;
    g_pol_history.depth = 1;

    g_pol_resource_limits.max_samples = 1;
    g_pol_resource_limits.max_instances = 1;
    g_pol_resource_limits.max_samples_per_instance = 1;

    g_pol_presentation.access_scope = DDS_INSTANCE_PRESENTATION_QOS;
    g_pol_presentation.coherent_access = true;
    g_pol_presentation.ordered_access = true;

    g_pol_lifespan.duration.sec = 10000;
    g_pol_lifespan.duration.nanosec = 11000;

    g_pol_deadline.period.sec = 20000;
    g_pol_deadline.period.nanosec = 220000;

    g_pol_latency_budget.duration.sec = 30000;
    g_pol_latency_budget.duration.nanosec = 33000;

    g_pol_ownership.kind = DDS_EXCLUSIVE_OWNERSHIP_QOS;

    g_pol_ownership_strength.value = 10;

    g_pol_liveliness.kind = DDS_AUTOMATIC_LIVELINESS_QOS;
    g_pol_liveliness.lease_duration.sec = 40000;
    g_pol_liveliness.lease_duration.nanosec = 44000;

    g_pol_time_based_filter.minimum_separation.sec = 50000;
    g_pol_time_based_filter.minimum_separation.nanosec = 55000;

    g_pol_partition.name._buffer = (char**)c_partitions;
    g_pol_partition.name._length = 2;

    g_pol_reliability.kind = DDS_RELIABLE_RELIABILITY_QOS;
    g_pol_reliability.max_blocking_time.sec = 60000;
    g_pol_reliability.max_blocking_time.nanosec = 66000;

    g_pol_transport_priority.value = 42;

    g_pol_destination_order.kind = DDS_BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS;

    g_pol_writer_data_lifecycle.autodispose_unregistered_instances = true;

    g_pol_reader_data_lifecycle.autopurge_disposed_samples_delay.sec = 70000;
    g_pol_reader_data_lifecycle.autopurge_disposed_samples_delay.nanosec= 77000;
    g_pol_reader_data_lifecycle.autopurge_nowriter_samples_delay.sec = 80000;
    g_pol_reader_data_lifecycle.autopurge_nowriter_samples_delay.nanosec = 88000;

    g_pol_durability_service.history_depth = 1;
    g_pol_durability_service.history_kind = DDS_KEEP_LAST_HISTORY_QOS;
    g_pol_durability_service.max_samples = 2;
    g_pol_durability_service.max_instances = 3;
    g_pol_durability_service.max_samples_per_instance = 4;
    g_pol_durability_service.service_cleanup_delay.sec = 90000;
    g_pol_durability_service.service_cleanup_delay.nanosec = 99000;
}

static void
qos_fini(void)
{
    dds_qos_delete(g_qos);
    dds_free(g_pol_userdata.value._buffer);
    dds_free(g_pol_groupdata.value._buffer);
    dds_free(g_pol_topicdata.value._buffer);
}

static void
setup(void)
{
    qos_init();

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(g_participant, 0, "Failed to create prerequisite g_participant");
    g_topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, "RoundTrip", NULL, NULL);
    cr_assert_gt(g_topic, 0, "Failed to create prerequisite g_topic");
    g_subscriber = dds_create_subscriber(g_participant, NULL, NULL);
    cr_assert_gt(g_subscriber, 0, "Failed to create prerequisite g_subscriber");
    g_publisher = dds_create_publisher(g_participant, NULL, NULL);
    cr_assert_gt(g_publisher, 0, "Failed to create prerequisite g_publisher");
}

static void
teardown(void)
{
    qos_fini();
    dds_delete(g_participant);
}
#define T_MILLISECOND 1000000ll
#define T_SECOND (1000 * T_MILLISECOND)

int64_t from_ddsi_duration (DDS_Duration_t x)
{
  int64_t t;
  t = x.sec * 10^9 + x.nanosec;
  return t;
}

static void
check_default_qos_of_builtin_entity(dds_entity_t entity)
{
  dds_return_t ret;
  int64_t deadline;
  int64_t liveliness_lease_duration;
  int64_t minimum_separation;
  int64_t max_blocking_time;
  int64_t autopurge_nowriter_samples_delay;
  int64_t autopurge_disposed_samples_delay;

  dds_durability_kind_t durability_kind;
  dds_presentation_access_scope_kind_t presentation_access_scope_kind;
  dds_ownership_kind_t ownership_kind;
  dds_liveliness_kind_t liveliness_kind;
  dds_reliability_kind_t reliability_kind;
  dds_destination_order_kind_t destination_order_kind;
  dds_history_kind_t history_kind;

  char **partitions;
  uint32_t plen;

  dds_qos_t *qos = dds_qos_create();
  cr_assert_not_null(qos);

  ret = dds_get_qos(entity, qos);
  cr_assert_eq(ret, DDS_RETCODE_OK, "Failed to get QOS of builtin entity");

  dds_qget_durability(qos, &durability_kind);
  dds_qget_presentation(qos, &presentation_access_scope_kind, &g_pol_presentation.coherent_access, &g_pol_presentation.ordered_access);
  dds_qget_deadline(qos,  &deadline);
  dds_qget_ownership(qos, &ownership_kind);
  dds_qget_liveliness(qos, &liveliness_kind, &liveliness_lease_duration);
  dds_qget_time_based_filter(qos, &minimum_separation);
  dds_qget_reliability(qos, &reliability_kind, &max_blocking_time);
  dds_qget_destination_order(qos, &destination_order_kind);
  dds_qget_history(qos, &history_kind, &g_pol_history.depth);
  dds_qget_resource_limits(qos, &g_pol_resource_limits.max_samples, &g_pol_resource_limits.max_instances, &g_pol_resource_limits.max_samples_per_instance);
  dds_qget_reader_data_lifecycle(qos, &autopurge_nowriter_samples_delay, &autopurge_disposed_samples_delay);
  dds_qget_partition(qos, &plen, &partitions);
  // no getter for ENTITY_FACTORY

  if ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_SUBSCRIBER) {
      cr_expect_eq(plen, 1);
      if (plen > 0) {
          cr_expect_str_eq(partitions[0], "__BUILT-IN PARTITION__");
      }
  } else if ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER) {
      cr_expect_eq(durability_kind, DDS_DURABILITY_TRANSIENT_LOCAL);
      cr_expect_eq(presentation_access_scope_kind, DDS_PRESENTATION_TOPIC);
      cr_expect_eq(g_pol_presentation.coherent_access, false);
      cr_expect_eq(g_pol_presentation.ordered_access, false);
      cr_expect_eq(deadline, DDS_INFINITY);
      cr_expect_eq(ownership_kind, DDS_OWNERSHIP_SHARED);
      cr_expect_eq(liveliness_kind, DDS_LIVELINESS_AUTOMATIC);
      cr_expect_eq(minimum_separation, 0);
      cr_expect_eq(reliability_kind, DDS_RELIABILITY_RELIABLE);
      cr_expect_eq(max_blocking_time, DDS_MSECS(100));
      cr_expect_eq(destination_order_kind, DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP);
      cr_expect_eq(history_kind, DDS_HISTORY_KEEP_LAST);
      cr_expect_eq(g_pol_history.depth, 1);
      cr_expect_eq(g_pol_resource_limits.max_instances, DDS_LENGTH_UNLIMITED);
      cr_expect_eq(g_pol_resource_limits.max_samples, DDS_LENGTH_UNLIMITED);
      cr_expect_eq(g_pol_resource_limits.max_samples_per_instance, DDS_LENGTH_UNLIMITED);
      cr_expect_eq(autopurge_nowriter_samples_delay, DDS_INFINITY);
      cr_expect_eq(autopurge_disposed_samples_delay, DDS_INFINITY);
  } else {
      cr_assert_fail("Unsupported entity kind %s", entity_kind_str(entity));
  }
  if (plen > 0) {
      for (uint32_t i = 0; i < plen; i++) {
          dds_free(partitions[i]);
      }
      dds_free(partitions);
  }
  dds_qos_delete(qos);
}

static dds_entity_t builtin_topic_handles[10];

Test(ddsc_builtin_topics, types_allocation)
{
#define TEST_ALLOC(type) do { \
        DDS_##type##BuiltinTopicData *data = DDS_##type##BuiltinTopicData__alloc(); \
        cr_expect_not_null(data, "Failed to allocate DDS_" #type "BuiltinTopicData"); \
        DDS_##type##BuiltinTopicData_free(data, DDS_FREE_ALL); \
    } while(0)

    TEST_ALLOC(Participant);
    TEST_ALLOC(CMParticipant);
    TEST_ALLOC(Type);
    TEST_ALLOC(Topic);
    TEST_ALLOC(Publication);
    TEST_ALLOC(CMPublisher);
    TEST_ALLOC(Subscription);
    TEST_ALLOC(CMSubscriber);
    TEST_ALLOC(CMDataWriter);
    TEST_ALLOC(CMDataReader);
#undef TEST_ALLOC
}

Test(ddsc_builtin_topics, availability_builtin_topics, .init = setup, .fini = teardown)
{
  dds_entity_t topic;

  topic = dds_find_topic(g_participant, "DCPSParticipant");
  cr_assert_gt(topic, 0);
  dds_delete(topic);
  topic = dds_find_topic(g_participant, "DCPSTopic");
  cr_assert_lt(topic, 0);
  //TODO CHAM-347: dds_delete(topic);
  topic = dds_find_topic(g_participant, "DCPSType");
  cr_assert_lt(topic, 0);
  //TODO CHAM-347: dds_delete(topic);
  topic = dds_find_topic(g_participant, "DCPSSubscription");
  cr_assert_lt(topic, 0);
  //TODO CHAM-347: dds_delete(topic);
  topic = dds_find_topic(g_participant, "DCSPPublication");
  cr_assert_lt(topic, 0);
  //TODO CHAM-347: dds_delete(topic);
}

Test(ddsc_builtin_topics, read_publication_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
#if 0 /* disabled pending CHAM-347 */
  dds_return_t ret;
  DDS_PublicationBuiltinTopicData *data;
#endif
  void *samples[MAX_SAMPLES];


  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
  cr_assert_gt(reader, 0, "Failed to create a data reader for DDS_BUILTIN_TOPIC_DCPSPUBLICATION.");

  samples[0] = DDS_PublicationBuiltinTopicData__alloc();
#if 0 /* disabled pending CHAM-347 */
  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  cr_assert_gt(ret, 0, "Failed to read samples DCPSPublication");

  data = (DDS_PublicationBuiltinTopicData *)samples;
  cr_assert_str_eq(data->topic_name, "DCPSPublication");
#endif

  DDS_PublicationBuiltinTopicData_free(samples[0], DDS_FREE_ALL);
}

Test(ddsc_builtin_topics, create_reader)
{
    dds_entity_t participant;
    dds_entity_t t1;

    /* Create a participant */
    participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
    cr_assert_gt(participant, 0, "dds_participant_create");

    /*
     * The topics are created by the middleware as soon as a participant
     * is created.
     */
#define TEST_FIND(p, t) do { \
        t1 = dds_find_topic(p, t); \
        cr_expect_gt(t1, 0, "dds_find_topic(\"" t "\") returned a valid handle"); \
        dds_delete(t1); \
    } while(0);

    /* A builtin-topic proxy is created 'on demand' and should not exist before a reader is created for it */
    TEST_FIND(participant, "DCPSParticipant");
    TEST_FIND(participant, "CMParticipant");
#undef TEST_FIND

    /*
     * TODO CHAM-347: Not all builtin topics are created at the start.
     */
#define TEST_NOTFOUND(p, t) do { \
        t1 = dds_find_topic(p, t); \
        cr_expect_lt(t1, 0, "dds_find_topic(\"" t "\") returned a valid handle"); \
    } while(0);

    /* A builtin-topic proxy is created 'on demand' and should not exist before a reader is created for it */
    TEST_NOTFOUND(participant, "DCPSType");
    TEST_NOTFOUND(participant, "DCPSTopic");
    TEST_NOTFOUND(participant, "DCPSPublication");
    TEST_NOTFOUND(participant, "CMPublisher");
    TEST_NOTFOUND(participant, "DCPSSubscription");
    TEST_NOTFOUND(participant, "CMSubscriber");
    TEST_NOTFOUND(participant, "CMDataWriter");
    TEST_NOTFOUND(participant, "CMDataReader");
#undef TEST_NOTFOUND

    /* A reader is created by providing a special builtin-topic handle */
    {
        dds_entity_t readers[10];
        dds_entity_t builtin_subscriber, s;

        builtin_topic_handles[0] = DDS_BUILTIN_TOPIC_DCPSPARTICIPANT;
        builtin_topic_handles[1] = DDS_BUILTIN_TOPIC_CMPARTICIPANT;
        builtin_topic_handles[2] = DDS_BUILTIN_TOPIC_DCPSTYPE;
        builtin_topic_handles[3] = DDS_BUILTIN_TOPIC_DCPSTOPIC;
        builtin_topic_handles[4] = DDS_BUILTIN_TOPIC_DCPSPUBLICATION;
        builtin_topic_handles[5] = DDS_BUILTIN_TOPIC_CMPUBLISHER;
        builtin_topic_handles[6] = DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION;
        builtin_topic_handles[7] = DDS_BUILTIN_TOPIC_CMSUBSCRIBER;
        builtin_topic_handles[8] = DDS_BUILTIN_TOPIC_CMDATAWRITER;
        builtin_topic_handles[9] = DDS_BUILTIN_TOPIC_CMDATAREADER;


        for (int i = 0; i < 10; i++) {
            readers[i] = dds_create_reader(participant, builtin_topic_handles[i], NULL, NULL);
            cr_expect_gt(readers[i], 0, "Failed to created reader for builtin topic handle %d", builtin_topic_handles[i]);

            if (i == 0) {
                /* Check the parent of reader is a subscriber */
                builtin_subscriber = dds_get_parent(readers[i]);
                cr_assert_gt(builtin_subscriber, 0, "Failed to get parent of first builtin-reader (%s)", dds_err_str(builtin_subscriber));
                cr_assert_eq(builtin_subscriber & DDS_ENTITY_KIND_MASK, DDS_KIND_SUBSCRIBER, "Parent is not a subscriber");
            } else {
                /* Check the parent of reader equals parent of first reader */
                s = dds_get_parent(readers[i]);
                cr_assert_gt(s, 0, "Failed to get parent of builtin-reader (%s)", dds_err_str(s));
                cr_assert_eq(s, builtin_subscriber, "Parent subscriber of reader(%d) doesn't equal builtin-subscriber", i);
                //dds_delete(s);
            }
        }
    }

#define TEST_FOUND(p, t) do { \
        t1 = dds_find_topic(p, t); \
        cr_expect_gt(t1, 0, "dds_find_topic(\"" t "\") returned an invalid handle (%s)", dds_err_str(t1)); \
        if (t1 > 0) { \
            dds_delete(t1); \
        } \
    } while(0);

    /* Builtin-topics proxies should now be created */
    TEST_FOUND(participant, "DCPSParticipant");
    TEST_FOUND(participant, "CMParticipant");
    TEST_FOUND(participant, "DCPSType");
    TEST_FOUND(participant, "DCPSTopic");
    TEST_FOUND(participant, "DCPSPublication");
    TEST_FOUND(participant, "CMPublisher");
    TEST_FOUND(participant, "DCPSSubscription");
    TEST_FOUND(participant, "CMSubscriber");
    TEST_FOUND(participant, "CMDataWriter");
    TEST_FOUND(participant, "CMDataReader");
#undef TEST_FOUND

    dds_delete(participant);
}

Test(ddsc_builtin_topics, read_subscription_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
#if 0 /* not supported yet */
  dds_return_t ret;
  DDS_SubscriptionBuiltinTopicData *data;
#endif
  void * samples[MAX_SAMPLES];

  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  cr_assert_gt(reader, 0, "Failed to create a data reader for DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION.");

  samples[0] = DDS_SubscriptionBuiltinTopicData__alloc();

#if 0 /* not supported yet */
  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  cr_assert_gt(ret, 0, "Failed to read samples DCPSSubscription");

  data = (DDS_SubscriptionBuiltinTopicData *)samples;
  cr_assert_str_eq(data->topic_name, "DCPSSubscription");
#endif

  DDS_SubscriptionBuiltinTopicData_free(samples[0], DDS_FREE_ALL);
}

Test(ddsc_builtin_topics, read_participant_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
  dds_return_t ret;
  void * samples[MAX_SAMPLES];

  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);
  cr_assert_gt(reader, 0, "Failed to create a data reader for DDS_BUILTIN_TOPIC_DCPSPARTICIPANT.");

  samples[0] = DDS_ParticipantBuiltinTopicData__alloc();

  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  cr_assert_gt(ret, 0, "Failed to read samples DCPSParticipant");

  {
      DDS_ParticipantBuiltinTopicData *data = (DDS_ParticipantBuiltinTopicData*)samples[0];
      cr_log_info("Participant.key:      %x.%x.%x\n", data->key[0], data->key[1], data->key[2]);
      cr_log_info("Participant.userdata: %s\n", data->user_data.value._buffer);
  }

  DDS_ParticipantBuiltinTopicData_free(samples[0], DDS_FREE_ALL);
}

Test(ddsc_builtin_topics, read_cmparticipant_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
  dds_return_t ret;
  void * samples[MAX_SAMPLES];

  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_CMPARTICIPANT, NULL, NULL);
  cr_assert_gt(reader, 0, "Failed to create a data reader for DDS_BUILTIN_TOPIC_DCPSPARTICIPANT.");

  samples[0] = DDS_CMParticipantBuiltinTopicData__alloc();

  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  cr_assert_gt(ret, 0, "Failed to read samples CMParticipant");

  {
      DDS_CMParticipantBuiltinTopicData *data = (DDS_CMParticipantBuiltinTopicData*)samples[0];
      cr_log_info("CMParticipant.key:     %x.%x.%x\n", data->key[0], data->key[1], data->key[2]);
      cr_log_info("CMParticipant.product: %s\n", data->product.value);
  }

  DDS_CMParticipantBuiltinTopicData_free(samples[0], DDS_FREE_ALL);
}

Test(ddsc_builtin_topics, read_topic_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
#if 0 /* disabled pending CHAM-347 */
  dds_return_t ret;
  DDS_TopicBuiltinTopicData *data;
#endif
  void * samples[MAX_SAMPLES];


  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  cr_assert_gt(reader, 0, "Failed to create a data reader for DDS_BUILTIN_TOPIC_DCPSTOPIC.");

  samples[0] = DDS_TopicBuiltinTopicData__alloc();
#if 0 /* disabled pending CHAM-347 */
  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  cr_assert_gt(ret, 0, "Failed to read samples DCPSTopic");

  data = (DDS_TopicBuiltinTopicData *)samples;
  cr_assert_str_eq(data->name, "DCPSSubscription");
#endif
  DDS_ParticipantBuiltinTopicData_free(samples[0], DDS_FREE_ALL);
}

Test(ddsc_builtin_topics, read_type_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
#if 0 /* disabled pending CHAM-347 */
  dds_return_t ret;
  DDS_TypeBuiltinTopicData *data;
#endif
  void * samples[MAX_SAMPLES];

  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSTYPE, NULL, NULL);
  cr_assert_gt(reader, 0, "Failed to create a data reader for DDS_BUILTIN_TOPIC_DCPSTYPE.");

  samples[0] = DDS_TypeBuiltinTopicData__alloc();
#if 0 /* disabled pending CHAM-347 */
  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  cr_assert_gt(ret, 0, "Failed to read samples DCPSType");

  data = (DDS_TypeBuiltinTopicData *)samples;
  cr_assert_str_eq(data->name, "DCPSType");
#endif
  DDS_TypeBuiltinTopicData_free(samples[0], DDS_FREE_ALL);
}

Test(ddsc_builtin_topics, same_subscriber, .init = setup, .fini = teardown)
{
  dds_entity_t subscription_rdr;
  dds_entity_t subscription_subscriber;

  dds_entity_t publication_rdr;
  dds_entity_t publication_subscriber;

  dds_entity_t participant_rdr;
  dds_entity_t participant_subscriber;

  dds_entity_t topic_rdr;
  dds_entity_t topic_subscriber;

  dds_entity_t type_rdr;
  dds_entity_t type_subscriber;

  subscription_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  cr_assert_gt(subscription_rdr, 0, "Failed to create a data reader for DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION.");
  subscription_subscriber = dds_get_parent(subscription_rdr);
  cr_assert_gt(subscription_subscriber, 0, "Could not find builtin subscriber for DSCPSSubscription-reader.");

  publication_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
  cr_assert_gt(publication_rdr, 0, "Failed to create a data reader for DDS_BUILTIN_TOPIC_DCPSPUBLICATION.");
  publication_subscriber = dds_get_parent(publication_rdr);
  cr_assert_gt(publication_subscriber, 0, "Could not find builtin subscriber for DSCPSPublication-reader.");

  cr_assert_eq(subscription_subscriber, publication_subscriber);

  participant_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);
  cr_assert_gt(participant_rdr, 0, "Failed to create a data reader for DDS_BUILTIN_TOPIC_DCPSPARTICIPANT.");
  participant_subscriber = dds_get_parent(participant_rdr);
  cr_assert_gt(participant_subscriber, 0, "Could not find builtin subscriber for DSCPSParticipant-reader.");

  cr_assert_eq(publication_subscriber, participant_subscriber);

  topic_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  cr_assert_gt(topic_rdr, 0, "Failed to create a data reader for DDS_BUILTIN_TOPIC_DCPSTOPIC.");
  topic_subscriber = dds_get_parent(topic_rdr);
  cr_assert_gt(topic_subscriber, 0, "Could not find builtin subscriber for DSCPSTopic-reader.");

  cr_assert_eq(participant_subscriber, topic_subscriber);

  type_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSTYPE, NULL, NULL);
  cr_assert_gt(type_rdr, 0, "Failed to create a data reader for DDS_BUILTIN_TOPIC_DCPSTYPE.");
  type_subscriber = dds_get_parent(type_rdr);
  cr_assert_gt(type_subscriber, 0, "Could not find builtin subscriber for DSCPSType-reader.");

  cr_assert_eq(topic_subscriber, type_subscriber);
}

Test(ddsc_builtin_topics, builtin_qos, .init = setup, .fini = teardown)
{
  dds_entity_t dds_sub_rdr;
  dds_entity_t dds_sub_subscriber;

  dds_sub_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  cr_assert_gt(dds_sub_rdr, 0, "Failed to create a data reader for DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION.");
  check_default_qos_of_builtin_entity(dds_sub_rdr);

  dds_sub_subscriber = dds_get_parent(dds_sub_rdr);
  cr_assert_gt(dds_sub_subscriber, 0, "Could not find builtin subscriber for DSCPSSubscription-reader.");
  check_default_qos_of_builtin_entity(dds_sub_subscriber);
}

Test(ddsc_builtin_topics, datareader_qos, .init = setup, .fini = teardown)
{
  dds_entity_t rdr;
  dds_entity_t subscription_rdr;
  void * subscription_samples[MAX_SAMPLES];
#if 0 /* disabled pending CHAM-347 */
  dds_return_t ret;
  DDS_SubscriptionBuiltinTopicData *subscription_data;
#endif

  //  Set some qos' which differ from the default
  dds_qset_durability(g_qos, (dds_durability_kind_t)g_pol_durability.kind);
  dds_qset_deadline(g_qos, from_ddsi_duration(g_pol_deadline.period));
  dds_qset_latency_budget(g_qos, from_ddsi_duration(g_pol_latency_budget.duration));
  dds_qset_liveliness(g_qos, (dds_liveliness_kind_t)g_pol_liveliness.kind, from_ddsi_duration(g_pol_liveliness.lease_duration));
  dds_qset_reliability(g_qos, (dds_reliability_kind_t)g_pol_reliability.kind, from_ddsi_duration(g_pol_reliability.max_blocking_time));
  dds_qset_ownership(g_qos, (dds_ownership_kind_t)g_pol_ownership.kind);
  dds_qset_destination_order(g_qos, (dds_destination_order_kind_t)g_pol_destination_order.kind);
  dds_qset_userdata(g_qos, g_pol_userdata.value._buffer, g_pol_userdata.value._length);
  dds_qset_time_based_filter(g_qos, from_ddsi_duration(g_pol_time_based_filter.minimum_separation));
  dds_qset_presentation(g_qos, g_pol_presentation.access_scope, g_pol_presentation.coherent_access, g_pol_presentation.ordered_access);
  dds_qset_partition(g_qos, g_pol_partition.name._length, c_partitions);
  dds_qset_topicdata(g_qos, g_pol_topicdata.value._buffer, g_pol_topicdata.value._length);
  dds_qset_groupdata(g_qos, g_pol_groupdata.value._buffer, g_pol_groupdata.value._length);

  rdr = dds_create_reader(g_subscriber, g_topic, g_qos, NULL);

  subscription_samples[0] = DDS_SubscriptionBuiltinTopicData__alloc();

  subscription_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  cr_assert_gt(subscription_rdr, 0, "Failed to retrieve built-in datareader for DCPSSubscription");
#if 0 /* disabled pending CHAM-347 */
  ret = dds_read(subscription_rdr, subscription_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  cr_assert_gt(ret, 0, "Failed to read Subscription data");

  // Check the QOS settings of the 'remote' qos'
  subscription_data = (DDS_SubscriptionBuiltinTopicData *)subscription_samples[0];

  cr_assert_str_eq(subscription_data->topic_name, "RoundTrip");
  cr_assert_str_eq(subscription_data->type_name, "RoundTripModule::DataType");
  cr_assert_eq(subscription_data->durability.kind, g_pol_durability.kind);
  cr_assert_eq(subscription_data->deadline.period.sec, g_pol_deadline.period.sec);
  cr_assert_eq(subscription_data->deadline.period.nanosec, g_pol_deadline.period.nanosec);
  cr_assert_eq(subscription_data->latency_budget.duration.sec, g_pol_latency_budget.duration.sec);
  cr_assert_eq(subscription_data->latency_budget.duration.nanosec, g_pol_latency_budget.duration.nanosec);
  cr_assert_eq(subscription_data->liveliness.kind, g_pol_liveliness.kind);
  cr_assert_eq(subscription_data->liveliness.lease_duration.sec, g_pol_liveliness.lease_duration.sec);
  cr_assert_eq(subscription_data->liveliness.lease_duration.nanosec, g_pol_liveliness.lease_duration.nanosec);
  cr_assert_eq(subscription_data->reliability.kind, g_pol_reliability.kind);
  cr_assert_eq(subscription_data->reliability.max_blocking_time.sec, g_pol_reliability.max_blocking_time.sec);
  cr_assert_eq(subscription_data->reliability.max_blocking_time.nanosec, g_pol_reliability.max_blocking_time.nanosec);
  cr_assert_eq(subscription_data->ownership.kind, g_pol_ownership.kind);
  cr_assert_eq(subscription_data->destination_order.kind, g_pol_destination_order.kind);
  cr_assert_eq(subscription_data->user_data.value._buffer, g_pol_userdata.value._buffer);
  cr_assert_eq(subscription_data->user_data.value._length, g_pol_userdata.value._length);
  cr_assert_eq(subscription_data->time_based_filter.minimum_separation.sec, g_pol_time_based_filter.minimum_separation.sec);
  cr_assert_eq(subscription_data->time_based_filter.minimum_separation.nanosec, g_pol_time_based_filter.minimum_separation.nanosec);
  cr_assert_eq(subscription_data->presentation.access_scope, g_pol_presentation.access_scope);
  cr_assert_eq(subscription_data->presentation.coherent_access, g_pol_presentation.coherent_access);
  cr_assert_eq(subscription_data->presentation.ordered_access, g_pol_presentation.ordered_access);

  cr_assert_eq(subscription_data->partition.name._length, g_pol_partition.name._length);
  for (uint32_t i = 0; i < subscription_data->partition.name._length; ++i)
  {
    cr_assert_str_eq(subscription_data->partition.name._buffer[i], c_partitions[i]);
  }

  cr_assert_str_eq(subscription_data->topic_data.value._buffer, g_pol_topicdata.value._buffer);
  cr_assert_eq(subscription_data->topic_data.value._length, g_pol_topicdata.value._length);
  cr_assert_str_eq(subscription_data->group_data.value._buffer, g_pol_groupdata.value._buffer);
  cr_assert_eq(subscription_data->group_data.value._length, g_pol_groupdata.value._length);
#endif
  DDS_SubscriptionBuiltinTopicData_free(subscription_samples[0], DDS_FREE_ALL);
}

Test(ddsc_builtin_topics, datawriter_qos, .init = setup, .fini = teardown)
{
  dds_entity_t wrtr;
  dds_entity_t publication_rdr;
#if 0 /* disabled pending CHAM-347 */
  dds_return_t ret;
  DDS_PublicationBuiltinTopicData *publication_data;
#endif
  void * publication_samples[MAX_SAMPLES];


  dds_qset_durability(g_qos, g_pol_durability.kind);
  dds_qset_deadline(g_qos, from_ddsi_duration(g_pol_deadline.period));
  dds_qset_latency_budget(g_qos, from_ddsi_duration(g_pol_latency_budget.duration));
  dds_qset_liveliness(g_qos, (dds_liveliness_kind_t)g_pol_liveliness.kind, from_ddsi_duration(g_pol_liveliness.lease_duration));
  dds_qset_reliability(g_qos, (dds_reliability_kind_t)g_pol_reliability.kind, from_ddsi_duration(g_pol_reliability.max_blocking_time));
  dds_qset_lifespan(g_qos, from_ddsi_duration(g_pol_lifespan.duration));
  dds_qset_destination_order(g_qos, (dds_destination_order_kind_t)g_pol_destination_order.kind);
  dds_qset_userdata(g_qos, g_pol_userdata.value._buffer, g_pol_userdata.value._length);
  dds_qset_ownership(g_qos, (dds_ownership_kind_t)g_pol_ownership.kind);
  dds_qset_ownership_strength(g_qos, g_pol_ownership_strength.value);
  dds_qset_presentation(g_qos, g_pol_presentation.access_scope, g_pol_presentation.coherent_access, g_pol_presentation.ordered_access);
  dds_qset_partition(g_qos, g_pol_partition.name._length, c_partitions);
  dds_qset_topicdata(g_qos, g_pol_topicdata.value._buffer, g_pol_topicdata.value._length);

  wrtr = dds_create_writer(g_publisher, g_topic, g_qos, NULL);

  publication_samples[0] = DDS_PublicationBuiltinTopicData__alloc();

  publication_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
  cr_assert_gt(publication_rdr, 0, "Failed to retrieve built-in datareader for DCPSPublication");

#if 0 /* disabled pending CHAM-347 */
  ret = dds_read(publication_rdr, publication_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  cr_assert_gt(ret, 0, "Failed to read Publication data");

  // Check the QOS settings of the 'remote' qos'
  publication_data = (DDS_PublicationBuiltinTopicData *)publication_samples[0];

  cr_assert_str_eq(publication_data->topic_name, "RoundTrip");
  cr_assert_str_eq(publication_data->type_name, "RoundTripModule::DataType");
  cr_assert_eq(publication_data->durability.kind, g_pol_durability.kind);
  cr_assert_eq(publication_data->deadline.period.sec, g_pol_deadline.period.sec);
  cr_assert_eq(publication_data->deadline.period.nanosec, g_pol_deadline.period.nanosec);
  cr_assert_eq(publication_data->latency_budget.duration.sec, g_pol_latency_budget.duration.sec);
  cr_assert_eq(publication_data->latency_budget.duration.nanosec, g_pol_latency_budget.duration.nanosec);
  cr_assert_eq(publication_data->liveliness.kind, g_pol_liveliness.kind);
  cr_assert_eq(publication_data->liveliness.lease_duration.sec, g_pol_liveliness.lease_duration.sec);
  cr_assert_eq(publication_data->liveliness.lease_duration.nanosec, g_pol_liveliness.lease_duration.nanosec);
  cr_assert_eq(publication_data->reliability.kind, g_pol_reliability.kind);
  cr_assert_eq(publication_data->reliability.max_blocking_time.sec, g_pol_reliability.max_blocking_time.sec);
  cr_assert_eq(publication_data->reliability.max_blocking_time.nanosec, g_pol_reliability.max_blocking_time.nanosec);
  cr_assert_eq(publication_data->lifespan.duration.sec, g_pol_lifespan.duration.sec);
  cr_assert_eq(publication_data->lifespan.duration.nanosec, g_pol_lifespan.duration.nanosec);
  cr_assert_eq(publication_data->destination_order.kind, g_pol_destination_order.kind);
  cr_assert_eq(publication_data->user_data.value._buffer, g_pol_userdata.value._buffer);
  cr_assert_eq(publication_data->user_data.value._length, g_pol_userdata.value._length);
  cr_assert_eq(publication_data->ownership.kind, g_pol_ownership.kind);
  cr_assert_eq(publication_data->ownership_strength.value, g_pol_ownership_strength.value);
  cr_assert_eq(publication_data->presentation.access_scope, g_pol_presentation.access_scope);
  cr_assert_eq(publication_data->presentation.coherent_access, g_pol_presentation.coherent_access);
  cr_assert_eq(publication_data->presentation.ordered_access, g_pol_presentation.ordered_access);

  cr_assert_eq(publication_data->partition.name._length, g_pol_partition.name._length);
  for (uint32_t i = 0; i < publication_data->partition.name._length; ++i)
  {
    cr_assert_str_eq(publication_data->partition.name._buffer[i], c_partitions[i]);
  }

  cr_assert_str_eq(publication_data->topic_data.value._buffer, g_pol_topicdata.value._buffer);
  cr_assert_eq(publication_data->topic_data.value._length, g_pol_topicdata.value._length);
  cr_assert_str_eq(publication_data->group_data.value._buffer, g_pol_groupdata.value._buffer);
  cr_assert_eq(publication_data->group_data.value._length, g_pol_groupdata.value._length);
#endif
  DDS_PublicationBuiltinTopicData_free(publication_samples[0], DDS_FREE_ALL);
}

Test(ddsc_builtin_topics, topic_qos, .init = setup, .fini = teardown)
{
  dds_entity_t tpc;
  dds_entity_t topic_rdr;

#if 0 /* disabled pending CHAM-347 */
  dds_return_t ret;
  DDS_TopicBuiltinTopicData *topic_data;
#endif

  void * topic_samples[MAX_SAMPLES];

  dds_qset_durability(g_qos, g_pol_durability.kind);
  dds_qset_durability_service(g_qos,
                              from_ddsi_duration(g_pol_durability_service.service_cleanup_delay),
                              (dds_history_kind_t)g_pol_durability_service.history_kind,
                              g_pol_durability_service.history_depth,
                              g_pol_durability_service.max_samples,
                              g_pol_durability_service.max_instances,
                              g_pol_durability_service.max_samples_per_instance);
  dds_qset_deadline(g_qos, from_ddsi_duration(g_pol_deadline.period));
  dds_qset_latency_budget(g_qos, from_ddsi_duration(g_pol_latency_budget.duration));
  dds_qset_liveliness(g_qos, (dds_liveliness_kind_t)g_pol_liveliness.kind, from_ddsi_duration(g_pol_liveliness.lease_duration));
  dds_qset_reliability(g_qos, (dds_reliability_kind_t)g_pol_reliability.kind, from_ddsi_duration(g_pol_reliability.max_blocking_time));
  dds_qset_transport_priority(g_qos, g_pol_transport_priority.value);
  dds_qset_lifespan(g_qos, from_ddsi_duration(g_pol_lifespan.duration));
  dds_qset_destination_order(g_qos, (dds_destination_order_kind_t)g_pol_destination_order.kind);
  dds_qset_history(g_qos, (dds_history_kind_t)g_pol_history.kind, g_pol_history.depth);
  dds_qset_resource_limits(g_qos, g_pol_resource_limits.max_samples, g_pol_resource_limits.max_instances,
                           g_pol_resource_limits.max_samples_per_instance);
  dds_qset_ownership(g_qos, (dds_ownership_kind_t)g_pol_ownership.kind);
  dds_qset_topicdata(g_qos, g_pol_topicdata.value._buffer, g_pol_topicdata.value._length);


  tpc = dds_create_topic(g_participant, &Space_Type1_desc, "SpaceType1", g_qos, NULL);

  topic_samples[0] = DDS_PublicationBuiltinTopicData__alloc();

  topic_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  cr_assert_gt(topic_rdr, 0, "Failed to retrieve built-in datareader for DCPSPublication");
#if 0 /* disabled pending CHAM-347 */
  ret = dds_read(topic_rdr, topic_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  cr_assert_gt(ret, 0, "Failed to read Topic data");

  topic_data = (DDS_TopicBuiltinTopicData *)topic_samples[0];

  cr_assert_str_eq(topic_data->name, "SpaceType1");
  cr_assert_str_eq(topic_data->type_name, "RoundTripModule::DataType");
  cr_assert_eq(topic_data->durability.kind, g_pol_durability.kind);
  cr_assert_eq(topic_data->durability_service.service_cleanup_delay.sec, g_pol_durability_service.service_cleanup_delay.sec);
  cr_assert_eq(topic_data->durability_service.service_cleanup_delay.nanosec, g_pol_durability_service.service_cleanup_delay.nanosec);
  cr_assert_eq(topic_data->durability_service.history_kind, g_pol_durability_service.history_kind);
  cr_assert_eq(topic_data->durability_service.history_depth, g_pol_durability_service.history_depth);
  cr_assert_eq(topic_data->durability_service.max_samples, g_pol_durability_service.max_samples);
  cr_assert_eq(topic_data->durability_service.max_instances, g_pol_durability_service.max_instances);
  cr_assert_eq(topic_data->durability_service.max_samples_per_instance, g_pol_durability_service.max_samples_per_instance);
  cr_assert_eq(topic_data->deadline.period.sec, g_pol_deadline.period.sec);
  cr_assert_eq(topic_data->deadline.period.nanosec, g_pol_deadline.period.nanosec);
  cr_assert_eq(topic_data->latency_budget.duration.sec, g_pol_latency_budget.duration.sec);
  cr_assert_eq(topic_data->latency_budget.duration.nanosec, g_pol_latency_budget.duration.nanosec);
  cr_assert_eq(topic_data->liveliness.kind, g_pol_liveliness.kind);
  cr_assert_eq(topic_data->liveliness.lease_duration.sec, g_pol_liveliness.lease_duration.sec);
  cr_assert_eq(topic_data->liveliness.lease_duration.nanosec, g_pol_liveliness.lease_duration.nanosec);
  cr_assert_eq(topic_data->reliability.kind, g_pol_reliability.kind);
  cr_assert_eq(topic_data->reliability.max_blocking_time.sec, g_pol_reliability.max_blocking_time.sec);
  cr_assert_eq(topic_data->reliability.max_blocking_time.nanosec, g_pol_reliability.max_blocking_time.nanosec);
  cr_assert_eq(topic_data->transport_priority.value, g_pol_transport_priority.value);
  cr_assert_eq(topic_data->lifespan.duration.sec, g_pol_lifespan.duration.sec);
  cr_assert_eq(topic_data->lifespan.duration.nanosec, g_pol_lifespan.duration.nanosec);
  cr_assert_eq(topic_data->destination_order.kind, g_pol_destination_order.kind);
  cr_assert_eq(topic_data->history.kind, g_pol_history.kind);
  cr_assert_eq(topic_data->history.depth, g_pol_history.depth);
  cr_assert_eq(topic_data->resource_limits.max_samples, g_pol_resource_limits.max_samples);
  cr_assert_eq(topic_data->resource_limits.max_instances, g_pol_resource_limits.max_instances);
  cr_assert_eq(topic_data->resource_limits.max_samples_per_instance, g_pol_resource_limits.max_samples_per_instance);
  cr_assert_eq(topic_data->ownership.kind, g_pol_ownership.kind);
  cr_assert_str_eq(topic_data->topic_data.value._buffer, g_pol_topicdata.value._buffer);
  cr_assert_eq(topic_data->topic_data.value._length, g_pol_topicdata.value._length);
#endif
  DDS_TopicBuiltinTopicData_free(topic_samples[0], DDS_FREE_ALL);
}
