// Copyright(c) 2026 ZettaScale Technology and others
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

#include <string.h>

#include "dds__sysdef_model.h"
#include "dds__sysdef_parser.h"

static const char sysdef_all_constructs[] =
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
"<dds xmlns=\"http://www.omg.org/spec/DDS-XML\" xmlns:xsi=\"http://www.w3.org/2001/XMLSchema-instance\" xsi:schemaLocation=\"unused\" xsi:noNamespaceSchemaLocation=\"unused\">"
"  <types>"
"    <external_type_ref name=\"Msg\"/>"
"    <external_type_ref name=\"OtherMsg\"/>"
"  </types>"
"  <qos_library name=\"QosLib\">"
"    <qos_profile name=\"BaseProfile\">"
"      <datareader_qos name=\"ReaderDefaults\">"
"        <deadline><period><sec>1</sec><nanosec>2</nanosec></period></deadline>"
"        <destination_order><kind>BY_SOURCE_TIMESTAMP_DESTINATIONORDER_QOS</kind></destination_order>"
"        <durability><kind>TRANSIENT_LOCAL_DURABILITY_QOS</kind></durability>"
"        <history><kind>KEEP_LAST_HISTORY_QOS</kind><depth>4</depth></history>"
"        <latency_budget><duration><sec>0</sec><nanosec>10</nanosec></duration></latency_budget>"
"        <liveliness><kind>AUTOMATIC_LIVELINESS_QOS</kind><lease_duration><sec>3</sec><nanosec>4</nanosec></lease_duration></liveliness>"
"        <ownership><kind>SHARED_OWNERSHIP_QOS</kind></ownership>"
"        <reader_data_lifecycle><autopurge_nowriter_samples_delay><sec>5</sec><nanosec>6</nanosec></autopurge_nowriter_samples_delay><autopurge_disposed_samples_delay><sec>7</sec><nanosec>8</nanosec></autopurge_disposed_samples_delay></reader_data_lifecycle>"
"        <reliability><kind>RELIABLE_RELIABILITY_QOS</kind><max_blocking_time><sec>9</sec><nanosec>10</nanosec></max_blocking_time></reliability>"
"        <resource_limits><max_samples>LENGTH_UNLIMITED</max_samples><max_instances>11</max_instances><max_samples_per_instance>12</max_samples_per_instance></resource_limits>"
"        <time_based_filter><minimum_separation><sec>13</sec><nanosec>14</nanosec></minimum_separation></time_based_filter>"
"        <user_data><value>QUJD</value></user_data>"
"        <data_representation><id><element>XCDR1</element><element>XCDR2</element><element>XML</element></id></data_representation>"
"        <type_consistency><kind>ALLOW_TYPE_COERCION</kind><ignore_sequence_bounds>true</ignore_sequence_bounds><ignore_string_bounds>1</ignore_string_bounds><ignore_member_names>false</ignore_member_names><prevent_type_widening>0</prevent_type_widening><force_type_validation>true</force_type_validation></type_consistency>"
"      </datareader_qos>"
"      <datawriter_qos name=\"WriterDefaults\">"
"        <durability_service><service_cleanup_delay><sec>1</sec><nanosec>0</nanosec></service_cleanup_delay><history_kind>KEEP_ALL_HISTORY_QOS</history_kind><max_samples>5</max_samples><max_instances>LENGTH_UNLIMITED</max_instances><max_samples_per_instance>6</max_samples_per_instance></durability_service>"
"        <lifespan><duration><sec>DURATION_INFINITE_SEC</sec></duration></lifespan>"
"        <ownership_strength><value>19</value></ownership_strength>"
"        <transport_priority><value>20</value></transport_priority>"
"        <writer_data_lifecycle><autodispose_unregistered_instances>true</autodispose_unregistered_instances></writer_data_lifecycle>"
"        <writer_batching><batch_updates>false</batch_updates></writer_batching>"
"        <user_data/>"
"      </datawriter_qos>"
"      <datawriter_qos>"
"        <writer_data_lifecycle><autodispose_unregistered_instances>false</autodispose_unregistered_instances></writer_data_lifecycle>"
"      </datawriter_qos>"
"      <publisher_qos name=\"PublisherDefaults\"><group_data><value>REVG</value></group_data><partition><name><element>Alpha</element><element>Beta</element></name></partition><presentation><access_scope>GROUP_PRESENTATION_QOS</access_scope><coherent_access>true</coherent_access><ordered_access>false</ordered_access></presentation><entity_factory><autoenable_created_entities>true</autoenable_created_entities></entity_factory></publisher_qos>"
"      <subscriber_qos name=\"SubscriberDefaults\"><group_data/></subscriber_qos>"
"      <topic_qos name=\"TopicDefaults\"><topic_data><value>R0hJ</value></topic_data><durability><kind>VOLATILE_DURABILITY_QOS</kind></durability></topic_qos>"
"      <domain_participant_qos name=\"ParticipantDefaults\"><user_data><value>SlNM</value></user_data><entity_factory><autoenable_created_entities>false</autoenable_created_entities></entity_factory></domain_participant_qos>"
"    </qos_profile>"
"    <qos_profile name=\"DerivedProfile\" base_name=\"BaseProfile\">"
"      <domainparticipant_qos base_name=\"QosLib::BaseProfile\"/>"
"      <datawriter_qos base_name=\"BaseProfile\"/>"
"    </qos_profile>"
"  </qos_library>"
"  <domain_library name=\"DomainLib\">"
"    <domain name=\"DomainA\" domain_id=\"7\">"
"      <register_type name=\"RegMsg\" type_ref=\"Msg\"/>"
"      <topic name=\"DomainTopic\" register_type_ref=\"RegMsg\"><topic_qos><topic_data/></topic_qos></topic>"
"    </domain>"
"  </domain_library>"
"  <domain_participant_library name=\"ParticipantLib\">"
"    <domain_participant name=\"BaseParticipant\" domain_ref=\"DomainLib::DomainA\">"
"      <register_type name=\"LocalReg\" type_ref=\"OtherMsg\"/>"
"      <topic name=\"LocalTopic\" register_type_ref=\"LocalReg\"/>"
"      <publisher name=\"BasePublisher\"><data_writer name=\"BaseWriter\" topic_ref=\"LocalTopic\"/></publisher>"
"      <subscriber name=\"BaseSubscriber\"><data_reader name=\"BaseReader\" topic_ref=\"DomainTopic\"/></subscriber>"
"    </domain_participant>"
"    <domain_participant name=\"DerivedParticipant\" domain_ref=\"DomainLib::DomainA\" base_name=\"ParticipantLib::BaseParticipant\"/>"
"  </domain_participant_library>"
"  <application_library name=\"AppLib\">"
"    <application name=\"AppA\">"
"      <domain_participant name=\"AppParticipant\" domain_ref=\"DomainLib::DomainA\" base_name=\"ParticipantLib::BaseParticipant\">"
"        <register_type name=\"AppReg\" type_ref=\"Msg\"/>"
"        <topic name=\"AppTopic\" register_type_ref=\"AppReg\"><topic_qos/></topic>"
"        <domain_participant_qos/>"
"        <publisher name=\"PubA\"><publisher_qos/><data_writer name=\"WriterA\" topic_ref=\"AppTopic\"><datawriter_qos/></data_writer></publisher>"
"        <subscriber name=\"SubA\"><subscriber_qos/><data_reader name=\"ReaderA\" topic_ref=\"DomainTopic\"><datareader_qos/></data_reader></subscriber>"
"      </domain_participant>"
"    </application>"
"  </application_library>"
"  <node_library name=\"NodeLib\">"
"    <node name=\"NodeA\"><hostname>node-a.example</hostname><ipv4_address>192.0.2.1</ipv4_address><ipv6_address>2001:db8::1</ipv6_address><mac_address>01:23:45:67:89:ab</mac_address></node>"
"  </node_library>"
"  <deployment_library name=\"DeploymentLib\">"
"    <deployment name=\"DeploymentA\">"
"      <node node_ref=\"NodeLib::NodeA\"/>"
"      <application_list><application application_ref=\"AppLib::AppA\"/></application_list>"
"      <configuration><tsn>"
"        <tsn_talker name=\"TalkerA\" stream_name=\"StreamA\" datawriter_ref=\"AppLib::AppA::AppParticipant::PubA::WriterA\">"
"          <data_frame_specification>"
"            <vlan_tag><priority_code_point>3</priority_code_point><vlan_id>42</vlan_id></vlan_tag>"
"            <mac_addresses><destination_mac_address>aa:bb:cc:dd:ee:ff</destination_mac_address><source_mac_address>01:02:03:04:05:06</source_mac_address></mac_addresses>"
"            <ipv4_tuple><source_ip_address>192.0.2.10</source_ip_address><destination_ip_address>192.0.2.20</destination_ip_address><dscp>10</dscp><protocol>17</protocol><source_port>7400</source_port><destination_port>7410</destination_port></ipv4_tuple>"
"            <ipv6_tuple><source_ip_address>2001:db8::10</source_ip_address><destination_ip_address>2001:db8::20</destination_ip_address><dscp>11</dscp><protocol>17</protocol><source_port>7500</source_port><destination_port>7510</destination_port></ipv6_tuple>"
"          </data_frame_specification>"
"          <traffic_specification><periodicity><sec>0</sec><nanosec>1000000</nanosec></periodicity><samples_per_period>2</samples_per_period><max_bytes_per_sample>256</max_bytes_per_sample><transmission_selection>STRICT_PRIORITY</transmission_selection><time_aware><earliest_transmit_offset>1</earliest_transmit_offset><latest_transmit_offset>2</latest_transmit_offset><jitter>3</jitter></time_aware></traffic_specification>"
"          <network_requirements><num_seamless_trees>1</num_seamless_trees><max_latency>500</max_latency></network_requirements>"
"        </tsn_talker>"
"        <tsn_listener name=\"ListenerA\" stream_name=\"StreamA\" datareader_ref=\"AppLib::AppA::AppParticipant::SubA::ReaderA\"><network_requirements><num_seamless_trees>2</num_seamless_trees><max_latency>600</max_latency></network_requirements></tsn_listener>"
"      </tsn></configuration>"
"    </deployment>"
"  </deployment_library>"
"</dds>";

static void assert_parse_result (const char *xml, dds_return_t expected)
{
  struct dds_sysdef_system *sysdef = NULL;
  const dds_return_t ret = dds_sysdef_init_sysdef_str (xml, &sysdef, SYSDEF_SCOPE_ALL_LIB);
  CU_ASSERT_EQ_FATAL (ret, expected);
  if (ret == DDS_RETCODE_OK)
  {
    CU_ASSERT_NEQ_FATAL (sysdef, NULL);
    dds_sysdef_fini_sysdef (sysdef);
  }
}

static const struct dds_sysdef_qos *find_qos (
  const struct dds_sysdef_qos_profile *profile,
  enum dds_sysdef_qos_kind kind,
  const char *name)
{
  for (const struct dds_sysdef_qos *qos = profile->qos; qos != NULL; qos = (const struct dds_sysdef_qos *) qos->xmlnode.next)
  {
    if (qos->kind == kind && ((name == NULL && qos->name == NULL) || (name != NULL && qos->name != NULL && strcmp (qos->name, name) == 0)))
      return qos;
  }
  CU_FAIL_FATAL ("QoS not found");
  return NULL;
}

static void assert_data_representation (const dds_qos_t *qos)
{
  CU_ASSERT_EQ (qos->data_representation.value.n, 3);
  CU_ASSERT_EQ (qos->data_representation.value.ids[0], DDS_DATA_REPRESENTATION_XCDR1);
  CU_ASSERT_EQ (qos->data_representation.value.ids[1], DDS_DATA_REPRESENTATION_XCDR2);
  CU_ASSERT_EQ (qos->data_representation.value.ids[2], DDS_DATA_REPRESENTATION_XML);
}

static void assert_ip_addr (const struct dds_sysdef_ip_addr *addr, int family, const char *expected)
{
  char buf[INET6_ADDRSTRLEN];
  CU_ASSERT_NEQ_FATAL (addr, NULL);
  CU_ASSERT_EQ (addr->addr.ss_family, family);
  CU_ASSERT_EQ_FATAL (ddsrt_sockaddrtostr (&addr->addr, buf, sizeof (buf)), DDS_RETCODE_OK);
  CU_ASSERT_STREQ (buf, expected);
}

static void assert_mac_addr (const struct dds_sysdef_mac_addr *addr, const uint8_t expected[6])
{
  CU_ASSERT_NEQ_FATAL (addr, NULL);
  CU_ASSERT_MEMEQ (addr->addr, sizeof (addr->addr), expected, 6);
}

CU_Test (ddsc_sysdef_parser, all_constructs)
{
  assert_parse_result (sysdef_all_constructs, DDS_RETCODE_OK);
}

CU_Test (ddsc_sysdef_parser, all_constructs_values)
{
  struct dds_sysdef_system *sysdef = NULL;
  const dds_return_t ret = dds_sysdef_init_sysdef_str (sysdef_all_constructs, &sysdef, SYSDEF_SCOPE_ALL_LIB);
  CU_ASSERT_EQ_FATAL (ret, DDS_RETCODE_OK);
  CU_ASSERT_NEQ_FATAL (sysdef, NULL);

  const struct dds_sysdef_type_lib *type_lib = sysdef->type_libs;
  CU_ASSERT_NEQ_FATAL (type_lib, NULL);
  CU_ASSERT_EQ (type_lib->xmlnode.next, NULL);
  const struct dds_sysdef_type *type = type_lib->types;
  CU_ASSERT_NEQ_FATAL (type, NULL);
  CU_ASSERT_STREQ (type->name, "Msg");
  CU_ASSERT_EQ (type->kind, DDS_SYSDEF_TYPE_REF_EXTERNAL);
  type = (const struct dds_sysdef_type *) type->xmlnode.next;
  CU_ASSERT_NEQ_FATAL (type, NULL);
  CU_ASSERT_STREQ (type->name, "OtherMsg");
  CU_ASSERT_EQ (type->xmlnode.next, NULL);

  const struct dds_sysdef_qos_lib *qos_lib = sysdef->qos_libs;
  CU_ASSERT_NEQ_FATAL (qos_lib, NULL);
  CU_ASSERT_STREQ (qos_lib->name, "QosLib");
  CU_ASSERT_EQ (qos_lib->xmlnode.next, NULL);
  const struct dds_sysdef_qos_profile *base_profile = qos_lib->qos_profiles;
  CU_ASSERT_NEQ_FATAL (base_profile, NULL);
  CU_ASSERT_STREQ (base_profile->name, "BaseProfile");
  CU_ASSERT_EQ (base_profile->base_profile, NULL);

  const struct dds_sysdef_qos *reader = find_qos (base_profile, DDS_SYSDEF_READER_QOS, "ReaderDefaults");
  CU_ASSERT_EQ (reader->qos->present,
    DDSI_QP_DEADLINE | DDSI_QP_DESTINATION_ORDER | DDSI_QP_DURABILITY | DDSI_QP_HISTORY |
    DDSI_QP_LATENCY_BUDGET | DDSI_QP_LIVELINESS | DDSI_QP_OWNERSHIP |
    DDSI_QP_ADLINK_READER_DATA_LIFECYCLE | DDSI_QP_RELIABILITY | DDSI_QP_RESOURCE_LIMITS |
    DDSI_QP_TIME_BASED_FILTER | DDSI_QP_USER_DATA | DDSI_QP_DATA_REPRESENTATION |
    DDSI_QP_TYPE_CONSISTENCY_ENFORCEMENT);
  CU_ASSERT_EQ (reader->qos->deadline.deadline, DDS_SECS (1) + 2);
  CU_ASSERT_EQ (reader->qos->destination_order.kind, DDS_DESTINATIONORDER_BY_SOURCE_TIMESTAMP);
  CU_ASSERT_EQ (reader->qos->durability.kind, DDS_DURABILITY_TRANSIENT_LOCAL);
  CU_ASSERT_EQ (reader->qos->history.kind, DDS_HISTORY_KEEP_LAST);
  CU_ASSERT_EQ (reader->qos->history.depth, 4);
  CU_ASSERT_EQ (reader->qos->latency_budget.duration, 10);
  CU_ASSERT_EQ (reader->qos->liveliness.kind, DDS_LIVELINESS_AUTOMATIC);
  CU_ASSERT_EQ (reader->qos->liveliness.lease_duration, DDS_SECS (3) + 4);
  CU_ASSERT_EQ (reader->qos->ownership.kind, DDS_OWNERSHIP_SHARED);
  CU_ASSERT_EQ (reader->qos->reader_data_lifecycle.autopurge_nowriter_samples_delay, DDS_SECS (5) + 6);
  CU_ASSERT_EQ (reader->qos->reader_data_lifecycle.autopurge_disposed_samples_delay, DDS_SECS (7) + 8);
  CU_ASSERT_EQ (reader->qos->reliability.kind, DDS_RELIABILITY_RELIABLE);
  CU_ASSERT_EQ (reader->qos->reliability.max_blocking_time, DDS_SECS (9) + 10);
  CU_ASSERT_EQ (reader->qos->resource_limits.max_samples, DDS_LENGTH_UNLIMITED);
  CU_ASSERT_EQ (reader->qos->resource_limits.max_instances, 11);
  CU_ASSERT_EQ (reader->qos->resource_limits.max_samples_per_instance, 12);
  CU_ASSERT_EQ (reader->qos->time_based_filter.minimum_separation, DDS_SECS (13) + 14);
  CU_ASSERT_MEMEQ (reader->qos->user_data.value, reader->qos->user_data.length, "ABC", strlen ("ABC"));
  assert_data_representation (reader->qos);
  CU_ASSERT_EQ (reader->qos->type_consistency.kind, DDS_TYPE_CONSISTENCY_ALLOW_TYPE_COERCION);
  CU_ASSERT (reader->qos->type_consistency.ignore_sequence_bounds);
  CU_ASSERT (reader->qos->type_consistency.ignore_string_bounds);
  CU_ASSERT (!(reader->qos->type_consistency.ignore_member_names));
  CU_ASSERT (!(reader->qos->type_consistency.prevent_type_widening));
  CU_ASSERT (reader->qos->type_consistency.force_type_validation);

  const struct dds_sysdef_qos *writer = find_qos (base_profile, DDS_SYSDEF_WRITER_QOS, "WriterDefaults");
  CU_ASSERT_EQ (writer->qos->durability_service.service_cleanup_delay, DDS_SECS (1));
  CU_ASSERT_EQ (writer->qos->durability_service.history.kind, DDS_HISTORY_KEEP_ALL);
  CU_ASSERT_EQ (writer->qos->durability_service.resource_limits.max_samples, 5);
  CU_ASSERT_EQ (writer->qos->durability_service.resource_limits.max_instances, DDS_LENGTH_UNLIMITED);
  CU_ASSERT_EQ (writer->qos->durability_service.resource_limits.max_samples_per_instance, 6);
  CU_ASSERT_EQ (writer->qos->lifespan.duration, DDS_INFINITY);
  CU_ASSERT_EQ (writer->qos->ownership_strength.value, 19);
  CU_ASSERT_EQ (writer->qos->transport_priority.value, 20);
  CU_ASSERT (writer->qos->writer_data_lifecycle.autodispose_unregistered_instances);
  CU_ASSERT (!(writer->qos->writer_batching.batch_updates));
  CU_ASSERT_EQ (writer->qos->user_data.length, 0);

  const struct dds_sysdef_qos *publisher_qos = find_qos (base_profile, DDS_SYSDEF_PUBLISHER_QOS, "PublisherDefaults");
  CU_ASSERT_MEMEQ (publisher_qos->qos->group_data.value, publisher_qos->qos->group_data.length, "DEF", strlen ("DEF"));
  CU_ASSERT_EQ (publisher_qos->qos->partition.n, 2);
  CU_ASSERT_STREQ (publisher_qos->qos->partition.strs[0], "Alpha");
  CU_ASSERT_STREQ (publisher_qos->qos->partition.strs[1], "Beta");
  CU_ASSERT_EQ (publisher_qos->qos->presentation.access_scope, DDS_PRESENTATION_GROUP);
  CU_ASSERT (publisher_qos->qos->presentation.coherent_access);
  CU_ASSERT (!(publisher_qos->qos->presentation.ordered_access));
  CU_ASSERT (publisher_qos->qos->entity_factory.autoenable_created_entities);

  const struct dds_sysdef_qos *topic_qos = find_qos (base_profile, DDS_SYSDEF_TOPIC_QOS, "TopicDefaults");
  CU_ASSERT_MEMEQ (topic_qos->qos->topic_data.value, topic_qos->qos->topic_data.length, "GHI", strlen ("GHI"));
  CU_ASSERT_EQ (topic_qos->qos->durability.kind, DDS_DURABILITY_VOLATILE);
  const struct dds_sysdef_qos *participant_qos = find_qos (base_profile, DDS_SYSDEF_PARTICIPANT_QOS, "ParticipantDefaults");
  CU_ASSERT_MEMEQ (participant_qos->qos->user_data.value, participant_qos->qos->user_data.length, "JSL", strlen ("JSL"));
  CU_ASSERT (!(participant_qos->qos->entity_factory.autoenable_created_entities));

  const struct dds_sysdef_qos_profile *derived_profile = (const struct dds_sysdef_qos_profile *) base_profile->xmlnode.next;
  CU_ASSERT_NEQ_FATAL (derived_profile, NULL);
  CU_ASSERT_STREQ (derived_profile->name, "DerivedProfile");
  CU_ASSERT_EQ (derived_profile->base_profile, base_profile);
  const struct dds_sysdef_qos *derived_participant_qos = find_qos (derived_profile, DDS_SYSDEF_PARTICIPANT_QOS, NULL);
  CU_ASSERT_EQ (derived_participant_qos->base_profile, base_profile);
  const struct dds_sysdef_qos *derived_datawriter_qos = find_qos (derived_profile, DDS_SYSDEF_WRITER_QOS, NULL);
  CU_ASSERT_EQ (derived_datawriter_qos->base_profile, base_profile);
  CU_ASSERT_EQ (derived_profile->xmlnode.next, NULL);

  const struct dds_sysdef_domain_lib *domain_lib = sysdef->domain_libs;
  CU_ASSERT_NEQ_FATAL (domain_lib, NULL);
  CU_ASSERT_STREQ (domain_lib->name, "DomainLib");
  const struct dds_sysdef_domain *domain = domain_lib->domains;
  CU_ASSERT_NEQ_FATAL (domain, NULL);
  CU_ASSERT_STREQ (domain->name, "DomainA");
  CU_ASSERT_EQ (domain->domain_id, 7);
  const struct dds_sysdef_register_type *domain_reg = domain->register_types;
  CU_ASSERT_NEQ_FATAL (domain_reg, NULL);
  CU_ASSERT_STREQ (domain_reg->name, "RegMsg");
  CU_ASSERT_STREQ (domain_reg->type_ref->name, "Msg");
  const struct dds_sysdef_topic *domain_topic = domain->topics;
  CU_ASSERT_NEQ_FATAL (domain_topic, NULL);
  CU_ASSERT_STREQ (domain_topic->name, "DomainTopic");
  CU_ASSERT_EQ (domain_topic->register_type_ref, domain_reg);
  CU_ASSERT_NEQ_FATAL (domain_topic->qos, NULL);
  CU_ASSERT_EQ (domain_topic->qos->kind, DDS_SYSDEF_TOPIC_QOS);

  const struct dds_sysdef_participant_lib *participant_lib = sysdef->participant_libs;
  CU_ASSERT_NEQ_FATAL (participant_lib, NULL);
  CU_ASSERT_STREQ (participant_lib->name, "ParticipantLib");
  const struct dds_sysdef_participant *base_participant = participant_lib->participants;
  CU_ASSERT_NEQ_FATAL (base_participant, NULL);
  CU_ASSERT_STREQ (base_participant->name, "BaseParticipant");
  CU_ASSERT_EQ (base_participant->domain_ref, domain);
  const struct dds_sysdef_participant *derived_participant = (const struct dds_sysdef_participant *) base_participant->xmlnode.next;
  CU_ASSERT_NEQ_FATAL (derived_participant, NULL);
  CU_ASSERT_STREQ (derived_participant->name, "DerivedParticipant");
  CU_ASSERT_EQ (derived_participant->base, base_participant);
  const struct dds_sysdef_register_type *local_reg = base_participant->register_types;
  CU_ASSERT_NEQ_FATAL (local_reg, NULL);
  CU_ASSERT_STREQ (local_reg->name, "LocalReg");
  CU_ASSERT_STREQ (local_reg->type_ref->name, "OtherMsg");
  const struct dds_sysdef_topic *local_topic = base_participant->topics;
  CU_ASSERT_NEQ_FATAL (local_topic, NULL);
  CU_ASSERT_STREQ (local_topic->name, "LocalTopic");
  CU_ASSERT_EQ (local_topic->register_type_ref, local_reg);
  const struct dds_sysdef_publisher *base_publisher = base_participant->publishers;
  CU_ASSERT_NEQ_FATAL (base_publisher, NULL);
  CU_ASSERT_STREQ (base_publisher->name, "BasePublisher");
  const struct dds_sysdef_writer *base_writer = base_publisher->writers;
  CU_ASSERT_NEQ_FATAL (base_writer, NULL);
  CU_ASSERT_STREQ (base_writer->name, "BaseWriter");
  CU_ASSERT_EQ (base_writer->topic, local_topic);
  const struct dds_sysdef_subscriber *base_subscriber = base_participant->subscribers;
  CU_ASSERT_NEQ_FATAL (base_subscriber, NULL);
  CU_ASSERT_STREQ (base_subscriber->name, "BaseSubscriber");
  const struct dds_sysdef_reader *base_reader = base_subscriber->readers;
  CU_ASSERT_NEQ_FATAL (base_reader, NULL);
  CU_ASSERT_STREQ (base_reader->name, "BaseReader");
  CU_ASSERT_EQ (base_reader->topic, domain_topic);

  const struct dds_sysdef_application_lib *application_lib = sysdef->application_libs;
  CU_ASSERT_NEQ_FATAL (application_lib, NULL);
  CU_ASSERT_STREQ (application_lib->name, "AppLib");
  const struct dds_sysdef_application *application = application_lib->applications;
  CU_ASSERT_NEQ_FATAL (application, NULL);
  CU_ASSERT_STREQ (application->name, "AppA");
  const struct dds_sysdef_participant *app_participant = application->participants;
  CU_ASSERT_NEQ_FATAL (app_participant, NULL);
  CU_ASSERT_STREQ (app_participant->name, "AppParticipant");
  CU_ASSERT_EQ (app_participant->domain_ref, domain);
  CU_ASSERT_EQ (app_participant->base, base_participant);
  const struct dds_sysdef_topic *app_topic = app_participant->topics;
  CU_ASSERT_NEQ_FATAL (app_topic, NULL);
  CU_ASSERT_STREQ (app_topic->name, "AppTopic");
  const struct dds_sysdef_publisher *app_publisher = app_participant->publishers;
  CU_ASSERT_NEQ_FATAL (app_publisher, NULL);
  CU_ASSERT_STREQ (app_publisher->name, "PubA");
  const struct dds_sysdef_writer *app_writer = app_publisher->writers;
  CU_ASSERT_NEQ_FATAL (app_writer, NULL);
  CU_ASSERT_STREQ (app_writer->name, "WriterA");
  CU_ASSERT_EQ (app_writer->topic, app_topic);
  const struct dds_sysdef_subscriber *app_subscriber = app_participant->subscribers;
  CU_ASSERT_NEQ_FATAL (app_subscriber, NULL);
  CU_ASSERT_STREQ (app_subscriber->name, "SubA");
  const struct dds_sysdef_reader *app_reader = app_subscriber->readers;
  CU_ASSERT_NEQ_FATAL (app_reader, NULL);
  CU_ASSERT_STREQ (app_reader->name, "ReaderA");
  CU_ASSERT_EQ (app_reader->topic, domain_topic);

  const struct dds_sysdef_node_lib *node_lib = sysdef->node_libs;
  CU_ASSERT_NEQ_FATAL (node_lib, NULL);
  CU_ASSERT_STREQ (node_lib->name, "NodeLib");
  const struct dds_sysdef_node *node = node_lib->nodes;
  CU_ASSERT_NEQ_FATAL (node, NULL);
  CU_ASSERT_STREQ (node->name, "NodeA");
  CU_ASSERT_STREQ (node->hostname, "node-a.example");
  assert_ip_addr (node->ipv4_addrs, AF_INET, "192.0.2.1");
  CU_ASSERT_EQ (node->ipv4_addrs->xmlnode.next, NULL);
  assert_ip_addr (node->ipv6_addrs, AF_INET6, "2001:db8::1");
  CU_ASSERT_EQ (node->ipv6_addrs->xmlnode.next, NULL);
  assert_mac_addr (node->mac_addr, (uint8_t[]) { 0x01, 0x23, 0x45, 0x67, 0x89, 0xab });

  const struct dds_sysdef_deployment_lib *deployment_lib = sysdef->deployment_libs;
  CU_ASSERT_NEQ_FATAL (deployment_lib, NULL);
  CU_ASSERT_STREQ (deployment_lib->name, "DeploymentLib");
  const struct dds_sysdef_deployment *deployment = deployment_lib->deployments;
  CU_ASSERT_NEQ_FATAL (deployment, NULL);
  CU_ASSERT_STREQ (deployment->name, "DeploymentA");
  CU_ASSERT_EQ (deployment->node, node);
  CU_ASSERT_NEQ_FATAL (deployment->application_list, NULL);
  CU_ASSERT_EQ (deployment->application_list->application_refs->application, application);
  CU_ASSERT_NEQ_FATAL (deployment->configuration, NULL);
  const struct dds_sysdef_tsn_configuration *tsn = deployment->configuration->tsn_configuration;
  CU_ASSERT_NEQ_FATAL (tsn, NULL);
  const struct dds_sysdef_tsn_talker_configuration *talker = tsn->tsn_talker_configurations;
  CU_ASSERT_NEQ_FATAL (talker, NULL);
  CU_ASSERT_STREQ (talker->name, "TalkerA");
  CU_ASSERT_STREQ (talker->stream_name, "StreamA");
  CU_ASSERT_EQ (talker->writer, app_writer);
  CU_ASSERT_EQ (talker->data_frame_specification->vlan_tag->priority_code_point, 3);
  CU_ASSERT_EQ (talker->data_frame_specification->vlan_tag->vlan_id, 42);
  assert_mac_addr (talker->data_frame_specification->mac_addresses->destination_mac_address, (uint8_t[]) { 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff });
  assert_mac_addr (talker->data_frame_specification->mac_addresses->source_mac_address, (uint8_t[]) { 0x01, 0x02, 0x03, 0x04, 0x05, 0x06 });
  CU_ASSERT_STREQ (talker->data_frame_specification->ipv4_tuple->source_ip_address, "192.0.2.10");
  CU_ASSERT_STREQ (talker->data_frame_specification->ipv4_tuple->destination_ip_address, "192.0.2.20");
  CU_ASSERT_EQ (talker->data_frame_specification->ipv4_tuple->dscp, 10);
  CU_ASSERT_EQ (talker->data_frame_specification->ipv4_tuple->protocol, 17);
  CU_ASSERT_EQ (talker->data_frame_specification->ipv4_tuple->source_port, 7400);
  CU_ASSERT_EQ (talker->data_frame_specification->ipv4_tuple->destination_port, 7410);
  CU_ASSERT_STREQ (talker->data_frame_specification->ipv6_tuple->source_ip_address, "2001:db8::10");
  CU_ASSERT_STREQ (talker->data_frame_specification->ipv6_tuple->destination_ip_address, "2001:db8::20");
  CU_ASSERT_EQ (talker->data_frame_specification->ipv6_tuple->dscp, 11);
  CU_ASSERT_EQ (talker->data_frame_specification->ipv6_tuple->source_port, 7500);
  CU_ASSERT_EQ (talker->data_frame_specification->ipv6_tuple->destination_port, 7510);
  CU_ASSERT_EQ (talker->traffic_specification->periodicity, DDS_MSECS (1));
  CU_ASSERT_EQ (talker->traffic_specification->samples_per_period, 2);
  CU_ASSERT_EQ (talker->traffic_specification->max_bytes_per_sample, 256);
  CU_ASSERT_EQ (talker->traffic_specification->transmission_selection, DDS_SYSDEF_TSN_TRAFFIC_TRANSMISSION_STRICT_PRIORITY);
  CU_ASSERT_EQ (talker->traffic_specification->time_aware->earliest_transmit_offset, 1);
  CU_ASSERT_EQ (talker->traffic_specification->time_aware->latest_transmit_offset, 2);
  CU_ASSERT_EQ (talker->traffic_specification->time_aware->jitter, 3);
  CU_ASSERT_EQ (talker->network_requirements->num_seamless_trees, 1);
  CU_ASSERT_EQ (talker->network_requirements->max_latency, 500);

  const struct dds_sysdef_tsn_listener_configuration *listener = tsn->tsn_listener_configurations;
  CU_ASSERT_NEQ_FATAL (listener, NULL);
  CU_ASSERT_STREQ (listener->name, "ListenerA");
  CU_ASSERT_STREQ (listener->stream_name, "StreamA");
  CU_ASSERT_EQ (listener->reader, app_reader);
  CU_ASSERT_EQ (listener->network_requirements->num_seamless_trees, 2);
  CU_ASSERT_EQ (listener->network_requirements->max_latency, 600);

  dds_sysdef_fini_sysdef (sysdef);
}

CU_Test (ddsc_sysdef_parser, invalid_cases)
{
  assert_parse_result ("<dds><qos_library name=\"Q\"><qos_profile name=\"P\"><datareader_qos><durability><kind>TRANSIENT_DURABILITY_QOS</kind></durability></datareader_qos></qos_profile></qos_library></dds>", DDS_RETCODE_ERROR);
  assert_parse_result ("<dds><types><external_type_ref name=\"int\"/></types></dds>", DDS_RETCODE_ERROR);
  assert_parse_result ("<dds><node_library name=\"N\"><node name=\"NodeA\"><mac_address>01:02:03:04:05:zz</mac_address></node></node_library></dds>", DDS_RETCODE_ERROR);
  assert_parse_result ("<dds><domain_library name=\"D\"><domain name=\"DomainA\" domain_id=\"1\"><topic name=\"T\" register_type_ref=\"Missing\"/></domain></domain_library></dds>", DDS_RETCODE_ERROR);
}
