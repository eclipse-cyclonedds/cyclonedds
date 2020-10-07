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
#include "dds/dds.h"
#include "test_common.h"

static dds_entity_t g_participant = 0;
static dds_entity_t g_subscriber  = 0;
static dds_entity_t g_publisher   = 0;
static dds_entity_t g_writer      = 0;
static dds_entity_t g_reader      = 0;
static dds_entity_t g_topic       = 0;

#define MAX_SAMPLES 2

static dds_sample_info_t g_info[MAX_SAMPLES];

static void
qos_init(void)
{
}

static void
qos_fini(void)
{
}

static void
setup(void)
{
    qos_init();

    g_participant = dds_create_participant(DDS_DOMAIN_DEFAULT, NULL, NULL);
    CU_ASSERT_FATAL(g_participant > 0);
    g_topic = dds_create_topic(g_participant, &RoundTripModule_DataType_desc, "RoundTrip", NULL, NULL);
    CU_ASSERT_FATAL(g_topic > 0);
    g_subscriber = dds_create_subscriber(g_participant, NULL, NULL);
    CU_ASSERT_FATAL(g_subscriber > 0);
    g_publisher = dds_create_publisher(g_participant, NULL, NULL);
    CU_ASSERT_FATAL(g_publisher > 0);
    g_writer = dds_create_writer(g_publisher, g_topic, NULL, NULL);
    CU_ASSERT_FATAL(g_writer > 0);
    g_reader = dds_create_reader(g_subscriber, g_topic, NULL, NULL);
    CU_ASSERT_FATAL(g_reader > 0);
}

static void
teardown(void)
{
    qos_fini();
    dds_delete(g_participant);
}

static void
check_default_qos_of_builtin_entity(dds_entity_t entity, bool isread)
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
  bool presentation_coherent_access;
  bool presentation_ordered_access;
  dds_ownership_kind_t ownership_kind;
  dds_liveliness_kind_t liveliness_kind;
  dds_reliability_kind_t reliability_kind;
  dds_destination_order_kind_t destination_order_kind;
  dds_history_kind_t history_kind;
  int32_t history_depth;
  int32_t resource_limits_max_samples;
  int32_t resource_limits_max_instances;
  int32_t resource_limits_max_samples_per_instance;

  char **partitions;
  uint32_t plen;

  dds_qos_t *qos = dds_create_qos();
  CU_ASSERT_FATAL(qos != NULL);

  ret = dds_get_qos(entity, qos);
  CU_ASSERT_FATAL(ret == DDS_RETCODE_OK);

  dds_qget_durability(qos, &durability_kind);
  dds_qget_presentation(qos, &presentation_access_scope_kind, &presentation_coherent_access, &presentation_ordered_access);
  dds_qget_deadline(qos,  &deadline);
  dds_qget_ownership(qos, &ownership_kind);
  dds_qget_liveliness(qos, &liveliness_kind, &liveliness_lease_duration);
  dds_qget_time_based_filter(qos, &minimum_separation);
  dds_qget_reliability(qos, &reliability_kind, &max_blocking_time);
  dds_qget_destination_order(qos, &destination_order_kind);
  dds_qget_history(qos, &history_kind, &history_depth);
  dds_qget_resource_limits(qos, &resource_limits_max_samples, &resource_limits_max_instances, &resource_limits_max_samples_per_instance);
  dds_qget_reader_data_lifecycle(qos, &autopurge_nowriter_samples_delay, &autopurge_disposed_samples_delay);
  dds_qget_partition(qos, &plen, &partitions);
  // no getter for ENTITY_FACTORY

  if (!isread) {
      CU_ASSERT_FATAL(plen == 1);
      CU_ASSERT_STRING_EQUAL_FATAL(partitions[0], "__BUILT-IN PARTITION__");
  } else {
      CU_ASSERT_FATAL(durability_kind == DDS_DURABILITY_TRANSIENT_LOCAL);
      CU_ASSERT_FATAL(presentation_access_scope_kind == DDS_PRESENTATION_TOPIC);
      CU_ASSERT_FATAL(presentation_coherent_access == false);
      CU_ASSERT_FATAL(presentation_ordered_access == false);
      CU_ASSERT_FATAL(deadline == DDS_INFINITY);
      CU_ASSERT_FATAL(ownership_kind == DDS_OWNERSHIP_SHARED);
      CU_ASSERT_FATAL(liveliness_kind == DDS_LIVELINESS_AUTOMATIC);
      CU_ASSERT_FATAL(minimum_separation == 0);
      CU_ASSERT_FATAL(reliability_kind == DDS_RELIABILITY_RELIABLE);
      CU_ASSERT_FATAL(max_blocking_time == DDS_MSECS(100));
      CU_ASSERT_FATAL(destination_order_kind == DDS_DESTINATIONORDER_BY_RECEPTION_TIMESTAMP);
      CU_ASSERT_FATAL(history_kind == DDS_HISTORY_KEEP_LAST);
      CU_ASSERT_FATAL(history_depth == 1);
      CU_ASSERT_FATAL(resource_limits_max_instances == DDS_LENGTH_UNLIMITED);
      CU_ASSERT_FATAL(resource_limits_max_samples == DDS_LENGTH_UNLIMITED);
      CU_ASSERT_FATAL(resource_limits_max_samples_per_instance == DDS_LENGTH_UNLIMITED);
      CU_ASSERT_FATAL(autopurge_nowriter_samples_delay == DDS_INFINITY);
      CU_ASSERT_FATAL(autopurge_disposed_samples_delay == DDS_INFINITY);
  }
  if (plen > 0) {
      for (uint32_t i = 0; i < plen; i++) {
          dds_free(partitions[i]);
      }
      dds_free(partitions);
  }
  dds_delete_qos(qos);
}

CU_Test(ddsc_builtin_topics, availability_builtin_topics, .init = setup, .fini = teardown)
{
  dds_entity_t topic;

  topic = dds_find_topic_scoped (DDS_FIND_SCOPE_PARTICIPANT, g_participant, "DCPSParticipant", 0);
  CU_ASSERT_EQUAL_FATAL (topic, 0);
  topic = dds_find_topic_scoped (DDS_FIND_SCOPE_PARTICIPANT, g_participant, "DCPSTopic", 0);
  CU_ASSERT_EQUAL_FATAL (topic, 0);
  topic = dds_find_topic_scoped (DDS_FIND_SCOPE_PARTICIPANT, g_participant, "DCPSSubscription", 0);
  CU_ASSERT_EQUAL_FATAL (topic, 0);
  topic = dds_find_topic_scoped (DDS_FIND_SCOPE_PARTICIPANT, g_participant, "DCPSPublication", 0);
  CU_ASSERT_EQUAL_FATAL (topic, 0);
}

CU_Test(ddsc_builtin_topics, read_publication_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
  dds_return_t ret;
  dds_builtintopic_endpoint_t *data;
  void *samples[MAX_SAMPLES];

  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);

  samples[0] = NULL;
  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  data = samples[0];
  CU_ASSERT_FATAL(ret > 0);
  CU_ASSERT_STRING_EQUAL_FATAL(data->topic_name, "RoundTrip");
  dds_return_loan(reader, samples, ret);
}

CU_Test(ddsc_builtin_topics, read_subscription_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
  dds_return_t ret;
  void * samples[MAX_SAMPLES];
  const char *exp[] = { "DCPSSubscription", "RoundTrip" };
  unsigned seen = 0;
  dds_qos_t *qos;

  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);

  samples[0] = NULL;
  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret == 2);
  qos = dds_create_qos();
  for (int i = 0; i < ret; i++) {
    dds_builtintopic_endpoint_t *data = samples[i];
    for (size_t j = 0; j < sizeof (exp) / sizeof (exp[0]); j++) {
      if (strcmp (data->topic_name, exp[j]) == 0) {
        seen |= 1u << j;
        dds_return_t get_qos_ret = dds_get_qos(j == 0 ? reader : g_reader, qos);
        CU_ASSERT_FATAL(get_qos_ret == DDS_RETCODE_OK);
        const bool eq = dds_qos_equal(qos, data->qos);
        CU_ASSERT_FATAL(eq);
      }
    }
  }
  CU_ASSERT_FATAL(seen == 3);
  dds_delete_qos(qos);

  dds_return_loan(reader, samples, ret);
}

CU_Test(ddsc_builtin_topics, read_participant_data, .init = setup, .fini = teardown)
{
  dds_entity_t reader;
  dds_return_t ret;
  //dds_builtintopic_participant_t *data;
  void * samples[MAX_SAMPLES];

  reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);

  samples[0] = NULL;
  ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret > 0);
  dds_return_loan(reader, samples, ret);
}

CU_Test(ddsc_builtin_topics, read_topic_data, .init = setup, .fini = teardown)
{
#ifdef DDS_HAS_TOPIC_DISCOVERY
  const char *exp[] = { "RoundTrip", "DCPSPublication", "DCPSSubscription", "DCPSTopic" };
  unsigned seen = 0;
  dds_entity_t reader = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
  CU_ASSERT_FATAL(reader > 0);
  void * samples[MAX_SAMPLES] = { NULL };
  dds_return_t ret = dds_read(reader, samples, g_info, MAX_SAMPLES, MAX_SAMPLES);
  CU_ASSERT_FATAL(ret >= 0);
  for (int i = 0; i < ret; i++)
  {
    dds_builtintopic_topic_t *data = samples[i];
    for (size_t j = 0; j < sizeof (exp) / sizeof (exp[0]); j++)
    {
      if (strcmp (data->topic_name, exp[j]) == 0)
        seen |= 1u << j;
    }
  }
  CU_ASSERT_FATAL(seen == 1); // built-in topics should not be reported as DCPSTopic samples
  dds_return_loan(reader, samples, ret);
#endif /* DDS_HAS_TOPIC_DISCOVERY */
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

  subscription_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  CU_ASSERT_FATAL(subscription_rdr > 0);
  subscription_subscriber = dds_get_parent(subscription_rdr);
  CU_ASSERT_FATAL(subscription_subscriber > 0);

  publication_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPUBLICATION, NULL, NULL);
  CU_ASSERT_FATAL(publication_rdr > 0);
  publication_subscriber = dds_get_parent(publication_rdr);
  CU_ASSERT_FATAL(publication_subscriber > 0);

  CU_ASSERT_FATAL(subscription_subscriber == publication_subscriber);

  participant_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSPARTICIPANT, NULL, NULL);
  CU_ASSERT_FATAL(participant_rdr > 0);
  participant_subscriber = dds_get_parent(participant_rdr);
  CU_ASSERT_FATAL(participant_subscriber > 0);

  CU_ASSERT_FATAL(publication_subscriber == participant_subscriber);

  topic_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSTOPIC, NULL, NULL);
#ifdef DDS_HAS_TOPIC_DISCOVERY
  CU_ASSERT_FATAL(topic_rdr > 0);
  topic_subscriber = dds_get_parent(topic_rdr);
  CU_ASSERT_FATAL(topic_subscriber > 0);
  CU_ASSERT_FATAL(participant_subscriber == topic_subscriber);
#else
  (void) topic_subscriber;
  CU_ASSERT_EQUAL_FATAL(topic_rdr, DDS_RETCODE_UNSUPPORTED);
#endif
}

CU_Test(ddsc_builtin_topics, builtin_qos, .init = setup, .fini = teardown)
{
  dds_entity_t dds_sub_rdr;
  dds_entity_t dds_sub_subscriber;

  dds_sub_rdr = dds_create_reader(g_participant, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  CU_ASSERT_FATAL(dds_sub_rdr > 0);
  check_default_qos_of_builtin_entity(dds_sub_rdr, 1);

  dds_sub_subscriber = dds_get_parent(dds_sub_rdr);
  CU_ASSERT_FATAL(dds_sub_subscriber > 0);
  check_default_qos_of_builtin_entity(dds_sub_subscriber, 0);
}

CU_Test(ddsc_builtin_topics, read_nothing)
{
  dds_entity_t pp;
  dds_entity_t rd;
  dds_return_t ret;
  dds_sample_info_t si;
  void *raw1, *raw2;
  int32_t n1, n2;

  pp = dds_create_participant (DDS_DOMAIN_DEFAULT, NULL, NULL);
  CU_ASSERT_FATAL (pp > 0);
  rd = dds_create_reader (pp, DDS_BUILTIN_TOPIC_DCPSSUBSCRIPTION, NULL, NULL);
  CU_ASSERT_FATAL (rd > 0);

  /* Can't guarantee there's no other process around with a publication, but
     we can take until nothing remains.  The point is checking handling of
     freeing memory when a loan was outstanding, memory had to be allocated,
     and subsequently had to be freed because of an absence of data. */
  raw1 = raw2 = NULL;
  n1 = dds_take (rd, &raw1, &si, 1, 1);
  CU_ASSERT_FATAL (n1 >= 0);
  n2 = dds_take (rd, &raw2, &si, 1, 1);
  CU_ASSERT_FATAL (n2 >= 0);
  ret = dds_return_loan (rd, &raw1, n1);
  CU_ASSERT_FATAL (ret == 0);
  ret = dds_return_loan (rd, &raw2, n2);
  CU_ASSERT_FATAL (ret == 0);

  ret = dds_delete (pp);
  CU_ASSERT_FATAL (ret == 0);
}
