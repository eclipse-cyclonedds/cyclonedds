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

#include "CUnit/Test.h"

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
    g_qos = dds_create_qos();
    CU_ASSERT_PTR_NOT_NULL_FATAL(g_qos);

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

    g_pol_time_based_filter.minimum_separation.sec = 12000;
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
    g_pol_durability_service.max_samples = 12;
    g_pol_durability_service.max_instances = 3;
    g_pol_durability_service.max_samples_per_instance = 4;
    g_pol_durability_service.service_cleanup_delay.sec = 90000;
    g_pol_durability_service.service_cleanup_delay.nanosec = 99000;
}

static void
qos_fini(void)
{
    dds_delete_qos(g_qos);
    dds_free(g_pol_userdata.value._buffer);
    dds_free(g_pol_groupdata.value._buffer);
    dds_free(g_pol_topicdata.value._buffer);
}

static void
setup(void)
{
    qos_init();

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_participant > 0);
    g_topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, "RoundTrip", NULL, NULL);
    CU_ASSERT_FATAL(g_topic> 0);
    g_subscriber = dds_create_subscriber(g_participant, NULL, NULL);
    CU_ASSERT_FATAL(g_subscriber> 0);
    g_publisher = dds_create_publisher(g_participant, NULL, NULL);
    CU_ASSERT_FATAL(g_publisher> 0);
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
  t = (int64_t)x.sec * T_SECOND + x.nanosec;
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

  dds_qos_t *qos = dds_create_qos();
  CU_ASSERT_PTR_NOT_NULL_FATAL(qos);

  ret = dds_get_qos(entity, qos);
  CU_ASSERT_EQUAL_FATAL(ret, DDS_RETCODE_OK);

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
      CU_ASSERT_EQUAL(plen, 1);
      if (plen > 0) {
          CU_ASSERT_STRING_EQUAL(partitions[0], "__BUILT-IN PARTITION__");
      }
  } else if ((entity & DDS_ENTITY_KIND_MASK) == DDS_KIND_READER) {
      CU_ASSERT_EQUAL(durability_kind, DDS_DURABILITY_TRANSIENT_LOCAL);
      CU_ASSERT_EQUAL(presentation_access_scope_kind, DDS_PRESENTATION_TOPIC);
      CU_ASSERT_EQUAL(g_pol_presentation.coherent_access, false);
      CU_ASSERT_EQUAL(g_pol_presentation.ordered_access, false);
      CU_ASSERT_EQUAL(deadline, DDS_INFINITY);
      CU_ASSERT_EQUAL(ownership_kind, DDS_OWNERSHIP_SHARED);
      CU_ASSERT_EQUAL(liveliness_kind, DDS_LIVELINESS_AUTOMATIC);
      CU_ASSERT_EQUAL(minimum_separation, 0);
      CU_ASSERT_EQUAL(reliability_kind, DDS_RELIABILITY_RELIABLE);
      CU_ASSERT_EQUAL(max_blocking_time, DDS_MSECS(100));
      CU_ASSERT_EQUAL(destination_order_kind, DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP);
      CU_ASSERT_EQUAL(history_kind, DDS_HISTORY_KEEP_LAST);
      CU_ASSERT_EQUAL(g_pol_history.depth, 1);
      CU_ASSERT_EQUAL(g_pol_resource_limits.max_instances, DDS_LENGTH_UNLIMITED);
      CU_ASSERT_EQUAL(g_pol_resource_limits.max_samples, DDS_LENGTH_UNLIMITED);
      CU_ASSERT_EQUAL(g_pol_resource_limits.max_samples_per_instance, DDS_LENGTH_UNLIMITED);
      CU_ASSERT_EQUAL(autopurge_nowriter_samples_delay, DDS_INFINITY);
      CU_ASSERT_EQUAL(autopurge_disposed_samples_delay, DDS_INFINITY);
  } else {
      CU_FAIL_FATAL("Unsupported entity kind");
  }
  if (plen > 0) {
      for (uint32_t i = 0; i < plen; i++) {
          dds_free(partitions[i]);
      }
      dds_free(partitions);
  }
  dds_delete_qos(qos);
}

static dds_entity_t builtin_topic_handles[10];

CU_Test(ddsc_builtin_topics, types_allocation)
{
#define TEST_ALLOC(type) do { \
        DDS_##type##BuiltinTopicData *data = DDS_##type##BuiltinTopicData__alloc(); \
        CU_ASSERT_PTR_NOT_NULL(data); \
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

CU_Test(ddsc_builtin_topics, availability_builtin_topics, .init = setup, .fini = teardown)
{
  dds_entity_t topic;

  topic = dds_find_topic(g_participant, "DCPSParticipant");
  CU_ASSERT_FATAL(topic > 0);
  dds_delete(topic);
  topic = dds_find_topic(g_participant, "DCPSTopic");
  CU_ASSERT_FATAL(topic < 0);
  //TODO CHAM-347: dds_delete(topic);
  topic = dds_find_topic(g_participant, "DCPSType");
  CU_ASSERT_FATAL(topic < 0);
  //TODO CHAM-347: dds_delete(topic);
  topic = dds_find_topic(g_participant, "DCPSSubscription");
  CU_ASSERT_FATAL(topic < 0);
  //TODO CHAM-347: dds_delete(topic);
  topic = dds_find_topic(g_participant, "DCSPPublication");
  CU_ASSERT_FATAL(topic < 0);
  //TODO CHAM-347: dds_delete(topic);
}

CU_Test(ddsc_builtin_topics, read_publication_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
#if 0 /* disabled pending CHAM-347 */
  dds_return_t ret;
  DDS_PublicationBuiltinTopicData *data;
#endif
  void *samples[MAX_SAMPLES];


  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);

  samples[0] = DDS_PublicationBuiltinTopicData__alloc();
#if 0 /* disabled pending CHAM-347 */
  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret > 0);

  data = (DDS_PublicationBuiltinTopicData *)samples;
  CU_ASSERT_STRING_EQUAL_FATAL(data->topic_name, "DCPSPublication");
#endif

  DDS_PublicationBuiltinTopicData_free(samples[0], DDS_FREE_ALL);
}

CU_Test(ddsc_builtin_topics, create_reader)
{
    dds_entity_t participant;
    dds_entity_t t1;

    /* Create a participant */
    participant = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);

    /*
     * The topics are created by the middleware as soon as a participant
     * is created.
     */
#define TEST_FIND(p, t) do { \
        t1 = dds_find_topic(p, t); \
        CU_ASSERT(t1 > 0); \
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
        CU_ASSERT(t1 < 0); \
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
            CU_ASSERT(readers[i]> 0);

            if (i == 0) {
                /* Check the parent of reader is a subscriber */
                builtin_subscriber = dds_get_parent(readers[i]);
                CU_ASSERT_FATAL(builtin_subscriber > 0);
                CU_ASSERT_EQUAL_FATAL(builtin_subscriber & DDS_ENTITY_KIND_MASK, DDS_KIND_SUBSCRIBER);
            } else {
                /* Check the parent of reader equals parent of first reader */
                s = dds_get_parent(readers[i]);
                CU_ASSERT_FATAL(s > 0);
                CU_ASSERT_EQUAL_FATAL(s, builtin_subscriber);
                //dds_delete(s);
            }
        }
    }

#define TEST_FOUND(p, t) do { \
        t1 = dds_find_topic(p, t); \
        CU_ASSERT(t1 > 0); \
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

CU_Test(ddsc_builtin_topics, read_subscription_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
#if 0 /* not supported yet */
  dds_return_t ret;
  DDS_SubscriptionBuiltinTopicData *data;
#endif
  void * samples[MAX_SAMPLES];

  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  CU_ASSERT_FATAL(reader> 0);

  samples[0] = DDS_SubscriptionBuiltinTopicData__alloc();

#if 0 /* not supported yet */
  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret> 0);

  data = (DDS_SubscriptionBuiltinTopicData *)samples;
  CU_ASSERT_STRING_EQUAL_FATAL(data->topic_name, "DCPSSubscription");
#endif

  DDS_SubscriptionBuiltinTopicData_free(samples[0], DDS_FREE_ALL);
}

CU_Test(ddsc_builtin_topics, read_participant_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
  dds_return_t ret;
  void * samples[MAX_SAMPLES];

  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);

  samples[0] = DDS_ParticipantBuiltinTopicData__alloc();

  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret > 0);
#if 0
  {
      DDS_ParticipantBuiltinTopicData *data = (DDS_ParticipantBuiltinTopicData*)samples[0];
  }
#endif

  DDS_ParticipantBuiltinTopicData_free(samples[0], DDS_FREE_ALL);
}

CU_Test(ddsc_builtin_topics, read_cmparticipant_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
  dds_return_t ret;
  void * samples[MAX_SAMPLES];

  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_CMPARTICIPANT, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);

  samples[0] = DDS_CMParticipantBuiltinTopicData__alloc();

  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret > 0);
#if 0
  {
      DDS_CMParticipantBuiltinTopicData *data = (DDS_CMParticipantBuiltinTopicData*)samples[0];
  }
#endif

  DDS_CMParticipantBuiltinTopicData_free(samples[0], DDS_FREE_ALL);
}

CU_Test(ddsc_builtin_topics, read_topic_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
#if 0 /* disabled pending CHAM-347 */
  dds_return_t ret;
  DDS_TopicBuiltinTopicData *data;
#endif
  void * samples[MAX_SAMPLES];


  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);

  samples[0] = DDS_TopicBuiltinTopicData__alloc();
#if 0 /* disabled pending CHAM-347 */
  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret> 0);

  data = (DDS_TopicBuiltinTopicData *)samples;
  CU_ASSERT_STRING_EQUAL_FATAL(data->name, "DCPSSubscription");
#endif
  DDS_ParticipantBuiltinTopicData_free(samples[0], DDS_FREE_ALL);
}

CU_Test(ddsc_builtin_topics, read_type_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
#if 0 /* disabled pending CHAM-347 */
  dds_return_t ret;
  DDS_TypeBuiltinTopicData *data;
#endif
  void * samples[MAX_SAMPLES];

  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSTYPE, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);

  samples[0] = DDS_TypeBuiltinTopicData__alloc();
#if 0 /* disabled pending CHAM-347 */
  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret > 0);

  data = (DDS_TypeBuiltinTopicData *)samples;
  CU_ASSERT_STRING_EQUAL_FATAL(data->name, "DCPSType");
#endif
  DDS_TypeBuiltinTopicData_free(samples[0], DDS_FREE_ALL);
}

CU_Test(ddsc_builtin_topics, same_subscriber, .init = setup, .fini = teardown)
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
  CU_ASSERT_FATAL(subscription_rdr > 0);
  subscription_subscriber = dds_get_parent(subscription_rdr);
  CU_ASSERT_FATAL(subscription_subscriber > 0);

  publication_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
  CU_ASSERT_FATAL(publication_rdr > 0);
  publication_subscriber = dds_get_parent(publication_rdr);
  CU_ASSERT_FATAL(publication_subscriber > 0);

  CU_ASSERT_EQUAL_FATAL(subscription_subscriber, publication_subscriber);

  participant_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);
  CU_ASSERT_FATAL(participant_rdr > 0);
  participant_subscriber = dds_get_parent(participant_rdr);
  CU_ASSERT_FATAL(participant_subscriber > 0);

  CU_ASSERT_EQUAL_FATAL(publication_subscriber, participant_subscriber);

  topic_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  CU_ASSERT_FATAL(topic_rdr > 0);
  topic_subscriber = dds_get_parent(topic_rdr);
  CU_ASSERT_FATAL(topic_subscriber > 0);

  CU_ASSERT_EQUAL_FATAL(participant_subscriber, topic_subscriber);

  type_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSTYPE, NULL, NULL);
  CU_ASSERT_FATAL(type_rdr > 0);
  type_subscriber = dds_get_parent(type_rdr);
  CU_ASSERT_FATAL(type_subscriber > 0);

  CU_ASSERT_EQUAL_FATAL(topic_subscriber, type_subscriber);
}

CU_Test(ddsc_builtin_topics, builtin_qos, .init = setup, .fini = teardown)
{
  dds_entity_t dds_sub_rdr;
  dds_entity_t dds_sub_subscriber;

  dds_sub_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  CU_ASSERT_FATAL(dds_sub_rdr > 0);
  check_default_qos_of_builtin_entity(dds_sub_rdr);

  dds_sub_subscriber = dds_get_parent(dds_sub_rdr);
  CU_ASSERT_FATAL(dds_sub_subscriber > 0);
  check_default_qos_of_builtin_entity(dds_sub_subscriber);
}

CU_Test(ddsc_builtin_topics, datareader_qos, .init = setup, .fini = teardown)
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
  dds_qset_presentation(g_qos, (dds_presentation_access_scope_kind_t)g_pol_presentation.access_scope, g_pol_presentation.coherent_access, g_pol_presentation.ordered_access);
  dds_qset_partition(g_qos, g_pol_partition.name._length, c_partitions);
  dds_qset_topicdata(g_qos, g_pol_topicdata.value._buffer, g_pol_topicdata.value._length);
  dds_qset_groupdata(g_qos, g_pol_groupdata.value._buffer, g_pol_groupdata.value._length);

  rdr = dds_create_reader(g_subscriber, g_topic, g_qos, NULL);
  CU_ASSERT_FATAL(rdr > 0);

  subscription_samples[0] = DDS_SubscriptionBuiltinTopicData__alloc();

  subscription_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  CU_ASSERT_FATAL(subscription_rdr > 0);
#if 0 /* disabled pending CHAM-347 */
  ret = dds_read(subscription_rdr, subscription_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret > 0);

  // Check the QOS settings of the 'remote' qos'
  subscription_data = (DDS_SubscriptionBuiltinTopicData *)subscription_samples[0];

  CU_ASSERT_STRING_EQUAL_FATAL(subscription_data->topic_name, "RoundTrip");
  CU_ASSERT_STRING_EQUAL_FATAL(subscription_data->type_name, "RoundTripModule::DataType");
  CU_ASSERT_EQUAL_FATAL(subscription_data->durability.kind, g_pol_durability.kind);
  CU_ASSERT_EQUAL_FATAL(subscription_data->deadline.period.sec, g_pol_deadline.period.sec);
  CU_ASSERT_EQUAL_FATAL(subscription_data->deadline.period.nanosec, g_pol_deadline.period.nanosec);
  CU_ASSERT_EQUAL_FATAL(subscription_data->latency_budget.duration.sec, g_pol_latency_budget.duration.sec);
  CU_ASSERT_EQUAL_FATAL(subscription_data->latency_budget.duration.nanosec, g_pol_latency_budget.duration.nanosec);
  CU_ASSERT_EQUAL_FATAL(subscription_data->liveliness.kind, g_pol_liveliness.kind);
  CU_ASSERT_EQUAL_FATAL(subscription_data->liveliness.lease_duration.sec, g_pol_liveliness.lease_duration.sec);
  CU_ASSERT_EQUAL_FATAL(subscription_data->liveliness.lease_duration.nanosec, g_pol_liveliness.lease_duration.nanosec);
  CU_ASSERT_EQUAL_FATAL(subscription_data->reliability.kind, g_pol_reliability.kind);
  CU_ASSERT_EQUAL_FATAL(subscription_data->reliability.max_blocking_time.sec, g_pol_reliability.max_blocking_time.sec);
  CU_ASSERT_EQUAL_FATAL(subscription_data->reliability.max_blocking_time.nanosec, g_pol_reliability.max_blocking_time.nanosec);
  CU_ASSERT_EQUAL_FATAL(subscription_data->ownership.kind, g_pol_ownership.kind);
  CU_ASSERT_EQUAL_FATAL(subscription_data->destination_order.kind, g_pol_destination_order.kind);
  CU_ASSERT_EQUAL_FATAL(subscription_data->user_data.value._buffer, g_pol_userdata.value._buffer);
  CU_ASSERT_EQUAL_FATAL(subscription_data->user_data.value._length, g_pol_userdata.value._length);
  CU_ASSERT_EQUAL_FATAL(subscription_data->time_based_filter.minimum_separation.sec, g_pol_time_based_filter.minimum_separation.sec);
  CU_ASSERT_EQUAL_FATAL(subscription_data->time_based_filter.minimum_separation.nanosec, g_pol_time_based_filter.minimum_separation.nanosec);
  CU_ASSERT_EQUAL_FATAL(subscription_data->presentation.access_scope, g_pol_presentation.access_scope);
  CU_ASSERT_EQUAL_FATAL(subscription_data->presentation.coherent_access, g_pol_presentation.coherent_access);
  CU_ASSERT_EQUAL_FATAL(subscription_data->presentation.ordered_access, g_pol_presentation.ordered_access);

  CU_ASSERT_EQUAL_FATAL(subscription_data->partition.name._length, g_pol_partition.name._length);
  for (uint32_t i = 0; i < subscription_data->partition.name._length; ++i)
  {
      CU_ASSERT_STRING_EQUAL_FATAL(subscription_data->partition.name._buffer[i], c_partitions[i]);
  }

  CU_ASSERT_STRING_EQUAL_FATAL(subscription_data->topic_data.value._buffer, g_pol_topicdata.value._buffer);
  CU_ASSERT_EQUAL_FATAL(subscription_data->topic_data.value._length, g_pol_topicdata.value._length);
  CU_ASSERT_STRING_EQUAL_FATAL(subscription_data->group_data.value._buffer, g_pol_groupdata.value._buffer);
  CU_ASSERT_EQUAL_FATAL(subscription_data->group_data.value._length, g_pol_groupdata.value._length);
#endif
  DDS_SubscriptionBuiltinTopicData_free(subscription_samples[0], DDS_FREE_ALL);
}

CU_Test(ddsc_builtin_topics, datawriter_qos, .init = setup, .fini = teardown)
{
  dds_entity_t wrtr;
  dds_entity_t publication_rdr;
#if 0 /* disabled pending CHAM-347 */
  dds_return_t ret;
  DDS_PublicationBuiltinTopicData *publication_data;
#endif
  void * publication_samples[MAX_SAMPLES];


  dds_qset_durability(g_qos, (dds_durability_kind_t)g_pol_durability.kind);
  dds_qset_deadline(g_qos, from_ddsi_duration(g_pol_deadline.period));
  dds_qset_latency_budget(g_qos, from_ddsi_duration(g_pol_latency_budget.duration));
  dds_qset_liveliness(g_qos, (dds_liveliness_kind_t)g_pol_liveliness.kind, from_ddsi_duration(g_pol_liveliness.lease_duration));
  dds_qset_reliability(g_qos, (dds_reliability_kind_t)g_pol_reliability.kind, from_ddsi_duration(g_pol_reliability.max_blocking_time));
  dds_qset_lifespan(g_qos, from_ddsi_duration(g_pol_lifespan.duration));
  dds_qset_destination_order(g_qos, (dds_destination_order_kind_t)g_pol_destination_order.kind);
  dds_qset_userdata(g_qos, g_pol_userdata.value._buffer, g_pol_userdata.value._length);
  dds_qset_ownership(g_qos, (dds_ownership_kind_t)g_pol_ownership.kind);
  dds_qset_ownership_strength(g_qos, g_pol_ownership_strength.value);
  dds_qset_presentation(g_qos, (dds_presentation_access_scope_kind_t)g_pol_presentation.access_scope, g_pol_presentation.coherent_access, g_pol_presentation.ordered_access);
  dds_qset_partition(g_qos, g_pol_partition.name._length, c_partitions);
  dds_qset_topicdata(g_qos, g_pol_topicdata.value._buffer, g_pol_topicdata.value._length);

  wrtr = dds_create_writer(g_publisher, g_topic, g_qos, NULL);
  CU_ASSERT_FATAL(wrtr > 0);

  publication_samples[0] = DDS_PublicationBuiltinTopicData__alloc();

  publication_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
  CU_ASSERT_FATAL(publication_rdr > 0);

#if 0 /* disabled pending CHAM-347 */
  ret = dds_read(publication_rdr, publication_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret> 0);

  // Check the QOS settings of the 'remote' qos'
  publication_data = (DDS_PublicationBuiltinTopicData *)publication_samples[0];

  CU_ASSERT_STRING_EQUAL_FATAL(publication_data->topic_name, "RoundTrip");
  CU_ASSERT_STRING_EQUAL_FATAL(publication_data->type_name, "RoundTripModule::DataType");
  CU_ASSERT_EQUAL_FATAL(publication_data->durability.kind, g_pol_durability.kind);
  CU_ASSERT_EQUAL_FATAL(publication_data->deadline.period.sec, g_pol_deadline.period.sec);
  CU_ASSERT_EQUAL_FATAL(publication_data->deadline.period.nanosec, g_pol_deadline.period.nanosec);
  CU_ASSERT_EQUAL_FATAL(publication_data->latency_budget.duration.sec, g_pol_latency_budget.duration.sec);
  CU_ASSERT_EQUAL_FATAL(publication_data->latency_budget.duration.nanosec, g_pol_latency_budget.duration.nanosec);
  CU_ASSERT_EQUAL_FATAL(publication_data->liveliness.kind, g_pol_liveliness.kind);
  CU_ASSERT_EQUAL_FATAL(publication_data->liveliness.lease_duration.sec, g_pol_liveliness.lease_duration.sec);
  CU_ASSERT_EQUAL_FATAL(publication_data->liveliness.lease_duration.nanosec, g_pol_liveliness.lease_duration.nanosec);
  CU_ASSERT_EQUAL_FATAL(publication_data->reliability.kind, g_pol_reliability.kind);
  CU_ASSERT_EQUAL_FATAL(publication_data->reliability.max_blocking_time.sec, g_pol_reliability.max_blocking_time.sec);
  CU_ASSERT_EQUAL_FATAL(publication_data->reliability.max_blocking_time.nanosec, g_pol_reliability.max_blocking_time.nanosec);
  CU_ASSERT_EQUAL_FATAL(publication_data->lifespan.duration.sec, g_pol_lifespan.duration.sec);
  CU_ASSERT_EQUAL_FATAL(publication_data->lifespan.duration.nanosec, g_pol_lifespan.duration.nanosec);
  CU_ASSERT_EQUAL_FATAL(publication_data->destination_order.kind, g_pol_destination_order.kind);
  CU_ASSERT_EQUAL_FATAL(publication_data->user_data.value._buffer, g_pol_userdata.value._buffer);
  CU_ASSERT_EQUAL_FATAL(publication_data->user_data.value._length, g_pol_userdata.value._length);
  CU_ASSERT_EQUAL_FATAL(publication_data->ownership.kind, g_pol_ownership.kind);
  CU_ASSERT_EQUAL_FATAL(publication_data->ownership_strength.value, g_pol_ownership_strength.value);
  CU_ASSERT_EQUAL_FATAL(publication_data->presentation.access_scope, g_pol_presentation.access_scope);
  CU_ASSERT_EQUAL_FATAL(publication_data->presentation.coherent_access, g_pol_presentation.coherent_access);
  CU_ASSERT_EQUAL_FATAL(publication_data->presentation.ordered_access, g_pol_presentation.ordered_access);

  CU_ASSERT_EQUAL_FATAL(publication_data->partition.name._length, g_pol_partition.name._length);
  for (uint32_t i = 0; i < publication_data->partition.name._length; ++i)
  {
      CU_ASSERT_STRING_EQUAL_FATAL(publication_data->partition.name._buffer[i], c_partitions[i]);
  }

  CU_ASSERT_STRING_EQUAL_FATAL(publication_data->topic_data.value._buffer, g_pol_topicdata.value._buffer);
  CU_ASSERT_EQUAL_FATAL(publication_data->topic_data.value._length, g_pol_topicdata.value._length);
  CU_ASSERT_STRING_EQUAL_FATAL(publication_data->group_data.value._buffer, g_pol_groupdata.value._buffer);
  CU_ASSERT_EQUAL_FATAL(publication_data->group_data.value._length, g_pol_groupdata.value._length);
#endif
  DDS_PublicationBuiltinTopicData_free(publication_samples[0], DDS_FREE_ALL);
}

CU_Test(ddsc_builtin_topics, topic_qos, .init = setup, .fini = teardown)
{
  dds_entity_t tpc;
  dds_entity_t topic_rdr;

#if 0 /* disabled pending CHAM-347 */
  dds_return_t ret;
  DDS_TopicBuiltinTopicData *topic_data;
#endif

  void * topic_samples[MAX_SAMPLES];

  dds_qset_durability(g_qos, (dds_durability_kind_t)g_pol_durability.kind);
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
  CU_ASSERT_FATAL(tpc > 0);

  topic_samples[0] = DDS_PublicationBuiltinTopicData__alloc();

  topic_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  CU_ASSERT_FATAL(topic_rdr > 0 );
#if 0 /* disabled pending CHAM-347 */
  ret = dds_read(topic_rdr, topic_samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret > 0);

  topic_data = (DDS_TopicBuiltinTopicData *)topic_samples[0];

  CU_ASSERT_STRING_EQUAL_FATAL(topic_data->name, "SpaceType1");
  CU_ASSERT_STRING_EQUAL_FATAL(topic_data->type_name, "RoundTripModule::DataType");
  CU_ASSERT_EQUAL_FATAL(topic_data->durability.kind, g_pol_durability.kind);
  CU_ASSERT_EQUAL_FATAL(topic_data->durability_service.service_cleanup_delay.sec, g_pol_durability_service.service_cleanup_delay.sec);
  CU_ASSERT_EQUAL(topic_data->durability_service.service_cleanup_delay.nanosec, g_pol_durability_service.service_cleanup_delay.nanosec);
  CU_ASSERT_EQUAL_FATAL(topic_data->durability_service.history_kind, g_pol_durability_service.history_kind);
  CU_ASSERT_EQUAL_FATAL(topic_data->durability_service.history_depth, g_pol_durability_service.history_depth);
  CU_ASSERT_EQUAL_FATAL(topic_data->durability_service.max_samples, g_pol_durability_service.max_samples);
  CU_ASSERT_EQUAL_FATAL(topic_data->durability_service.max_instances, g_pol_durability_service.max_instances);
  CU_ASSERT_EQUAL_FATAL(topic_data->durability_service.max_samples_per_instance, g_pol_durability_service.max_samples_per_instance);
  CU_ASSERT_EQUAL_FATAL(topic_data->deadline.period.sec, g_pol_deadline.period.sec);
  CU_ASSERT_EQUAL_FATAL(topic_data->deadline.period.nanosec, g_pol_deadline.period.nanosec);
  CU_ASSERT_EQUAL_FATAL(topic_data->latency_budget.duration.sec, g_pol_latency_budget.duration.sec);
  CU_ASSERT_EQUAL_FATAL(topic_data->latency_budget.duration.nanosec, g_pol_latency_budget.duration.nanosec);
  CU_ASSERT_EQUAL_FATAL(topic_data->liveliness.kind, g_pol_liveliness.kind);
  CU_ASSERT_EQUAL_FATAL(topic_data->liveliness.lease_duration.sec, g_pol_liveliness.lease_duration.sec);
  CU_ASSERT_EQUAL_FATAL(topic_data->liveliness.lease_duration.nanosec, g_pol_liveliness.lease_duration.nanosec);
  CU_ASSERT_EQUAL_FATAL(topic_data->reliability.kind, g_pol_reliability.kind);
  CU_ASSERT_EQUAL_FATAL(topic_data->reliability.max_blocking_time.sec, g_pol_reliability.max_blocking_time.sec);
  CU_ASSERT_EQUAL_FATAL(topic_data->reliability.max_blocking_time.nanosec, g_pol_reliability.max_blocking_time.nanosec);
  CU_ASSERT_EQUAL_FATAL(topic_data->transport_priority.value, g_pol_transport_priority.value);
  CU_ASSERT_EQUAL_FATAL(topic_data->lifespan.duration.sec, g_pol_lifespan.duration.sec);
  CU_ASSERT_EQUAL_FATAL(topic_data->lifespan.duration.nanosec, g_pol_lifespan.duration.nanosec);
  CU_ASSERT_EQUAL_FATAL(topic_data->destination_order.kind, g_pol_destination_order.kind);
  CU_ASSERT_EQUAL_FATAL(topic_data->history.kind, g_pol_history.kind);
  CU_ASSERT_EQUAL_FATAL(topic_data->history.depth, g_pol_history.depth);
  CU_ASSERT_EQUAL_FATAL(topic_data->resource_limits.max_samples, g_pol_resource_limits.max_samples);
  CU_ASSERT_EQUAL_FATAL(topic_data->resource_limits.max_instances, g_pol_resource_limits.max_instances);
  CU_ASSERT_EQUAL_FATAL(topic_data->resource_limits.max_samples_per_instance, g_pol_resource_limits.max_samples_per_instance);
  CU_ASSERT_EQUAL_FATAL(topic_data->ownership.kind, g_pol_ownership.kind);
  CU_ASSERT_STRING_EQUAL_FATAL(topic_data->topic_data.value._buffer, g_pol_topicdata.value._buffer);
  CU_ASSERT_EQUAL_FATAL(topic_data->topic_data.value._length, g_pol_topicdata.value._length);
#endif
  DDS_TopicBuiltinTopicData_free(topic_samples[0], DDS_FREE_ALL);
}
