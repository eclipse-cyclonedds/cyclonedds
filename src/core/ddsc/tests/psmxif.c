// Copyright(c) 2020 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <limits.h>

#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/md5.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/static_assert.h"

#include "dds/dds.h"
#include "dds/ddsi/ddsi_entity_index.h"
#include "ddsi__addrset.h"
#include "ddsi__entity.h"
#include "ddsi__xevent.h"
#include "dds__entity.h"
#include "dds__serdata_default.h"

#include "config_env.h"
#include "test_common.h"
#include "psmx_dummy_public.h"
#include "Array100.h"
#include "DynamicData.h"
#include "PsmxDataModels.h"

static void free_strings(uint32_t len, char** strings)
{
  if (len != 0 && strings != NULL) {
    for (uint32_t i = 0; i < len; i++) {
      dds_free(strings[i]);
    }
  }
  dds_free(strings);
}

/** @brief Convert a set of stats to a string.
 * 
 * Truncates the output string if the string buffer capacity is too small.
 * 
 * @param[in] dmock stats to convert
 * @param[out] str_out string buffer to write the string into
 * @param[in] str_capacity number of bytes the string buffer can hold
 * 
 * @return Upon successful return, it returns the number of characters printed
 *         (excluding the null byte used to end output to strings).
 *         If an output error is encountered, a negative value is returned.
*/
static int dummy_mockstats_tostring(const dummy_mockstats_t* dmock, char* str_out, size_t str_capacity)
{
  return snprintf(
    str_out,
    str_capacity,
    "\
  create_psmx: %i\n\
  \n\
  type_qos_supported: %i\n\
  create_topic: %i\n\
  delete_topic: %i\n\
  deinit: %i\n\
  get_node_id: %i\n\
  supported_features: %i\n\
  \n\
  create_endpoint: %i\n\
  delete_endpoint: %i\n\
  \n\
  request_loan: %i\n\
  write: %i\n\
  take: %i\n\
  on_data_available: %i\n",
    dmock->cnt_create_psmx,
    dmock->cnt_type_qos_supported,
    dmock->cnt_create_topic,
    dmock->cnt_delete_topic,
    dmock->cnt_deinit,
    dmock->cnt_get_node_id,
    dmock->cnt_supported_features,
    dmock->cnt_create_endpoint,
    dmock->cnt_delete_endpoint,
    dmock->cnt_request_loan,
    dmock->cnt_write,
    dmock->cnt_take,
    dmock->cnt_on_data_available
  );
}

static dds_entity_t create_participant(dds_domainid_t domainId)
{
  char configstr[] = "\
${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}\
<General>\
<AllowMulticast>spdp</AllowMulticast>\
<Interfaces>\
  <PubSubMessageExchange name=\"dummy\" library=\"psmx_dummy\" priority=\"1000000\" config=\"LOCATOR=4a4d203df6996395e1412fbecc2de4b6;SERVICE_NAME=service_psmx_dummy;KEYED_TOPICS=true;\" />\
</Interfaces>\
</General>\
<Tracing>\
<OutputFile>cdds.log.0</OutputFile>\
</Tracing>\
";
  char* configstr_in = ddsrt_expand_envvars (configstr, domainId);
  const dds_entity_t domain = dds_create_domain(domainId, configstr_in);
  ddsrt_free(configstr_in);
  CU_ASSERT_FATAL(domain > 0);
  const dds_entity_t participant = dds_create_participant(domainId, NULL, NULL);
  CU_ASSERT_FATAL(participant > 0);
  return participant;
}

/// @brief Check that creating a domain with more than one psmx interface fails.
/// @methodology
/// - Create a config string with two psmx interfaces.
/// - Try to create a domain using this config string.
/// - Expectation: Failed to create the domain.
/// 
CU_Test(ddsc_psmxif, config_multiple_psmx)
{
  dds_domainid_t domainId = 0;
  char* configstr_in = NULL;
  {
    char configstr[] = "\
${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}\
<General>\
  <AllowMulticast>spdp</AllowMulticast>\
  <Interfaces>\
    <PubSubMessageExchange name=\"dummy\" library=\"psmx_dummy\" priority=\"1000000\" config=\"SERVICE_NAME=psmx0;KEYED_TOPICS=true;\" />\
    <PubSubMessageExchange name=\"iox\" library=\"psmx_iox\" priority=\"1000000\" config=\"SERVICE_NAME=psmx0;KEYED_TOPICS=true;\" />\
  </Interfaces>\
</General>\
<Discovery>\
  <Tag>${CYCLONEDDS_PID}</Tag>\
  <ExternalDomainId>0</ExternalDomainId>\
</Discovery>\
<Tracing>\
  <OutputFile>cdds.log.0</OutputFile>\
</Tracing>\
  ";
    configstr_in = ddsrt_expand_envvars(configstr, domainId);
  }
  const dds_entity_t domain = dds_create_domain (domainId, configstr_in);
  ddsrt_free(configstr_in);
  CU_ASSERT_FATAL(domain <= 0);
}

static void assert_psmx_instance_name(dds_entity_t endpt, const char* name_expected)
{
  dds_qos_t* qos = dds_create_qos();
  dds_get_qos(endpt, qos);
  uint32_t strs_len = 0;
  char** strs = NULL;
  CU_ASSERT_FATAL(dds_qget_psmx_instances(qos, &strs_len, &strs));
  CU_ASSERT_FATAL(strs_len == 1 && strcmp(strs[0], name_expected) == 0);
  free_strings(strs_len, strs);
  dds_delete_qos(qos);
}

/// @brief Check that the instance_name is as provided from dummy_create_psmx().
/// @methodology
/// - Create domain with a config containing the name for the psmx instance.
/// - Create readers and writers.
/// - Using the QoS interface, for each endpoint check that the instance_name is correct.
/// 
CU_Test(ddsc_psmxif, instance_name)
{
  const dds_domainid_t domainId = 0;
  dds_entity_t participant = create_participant(domainId);
  dds_entity_t domain = dds_get_parent(participant);
  dummy_mockstats_t* dmock = dummy_mockstats_get_ptr();

  dds_entity_t writer1 = 0, reader1 = 0, writer2 = 0, reader2 = 0;

  char topicname[100];
  dummy_topics_alloc(dmock, 2);
  dummy_endpoints_alloc(dmock, 4);

  create_unique_topic_name("shared_memory", topicname, sizeof(topicname));
  dds_entity_t topic1 = dds_create_topic(participant, &SC_Model_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL(topic1 > 0);
  create_unique_topic_name("shared_memory", topicname, sizeof(topicname));
  dds_entity_t topic2 = dds_create_topic(participant, &PsmxType1_desc, topicname, NULL, NULL);
  CU_ASSERT_FATAL(topic2 > 0);

  writer1 = dds_create_writer(participant, topic1, NULL, NULL);
  CU_ASSERT_FATAL(writer1 > 0);
  assert_psmx_instance_name(writer1, "dummy_psmx");
  reader1 = dds_create_reader(participant, topic1, NULL, NULL);
  CU_ASSERT_FATAL(reader1 > 0);
  assert_psmx_instance_name(reader1, "dummy_psmx");
  writer2 = dds_create_writer(participant, topic2, NULL, NULL);
  CU_ASSERT_FATAL(writer2 > 0);
  assert_psmx_instance_name(writer2, "dummy_psmx");
  reader2 = dds_create_reader(participant, topic2, NULL, NULL);
  CU_ASSERT_FATAL(reader2 > 0);
  assert_psmx_instance_name(reader2, "dummy_psmx");
  dds_delete(domain);
}

/// @brief Check that shared memory availability and entity and loan pointers are correctly propagated through the psmx interface.
/// @methodology
/// - Check that the data types I'm planning to use are actually suitable for use with shared memory.
/// - Expectation: They are memcopy-safe.
/// 
/// - Create a configuration with a psmx interface.
/// - Create a domain using this configuration.
/// - Assert that there is exactly one psmx instance.
/// - Decide whether shared memory is supported (communicated to the dummy psmx).
/// - Create some entities
/// - Assert that the psmx_topic created during dummy_psmx_create_topic, is propagated to dummy_psmx_create_endpoint().
/// - Assert that the psmx_endpoint created during dummy_psmx_create_endpoint(), is propagated to dummy_psmx_request_loan(), dummy_psmx_write(), dummy_psmx_delete_endpoint().
/// - Assert that the loan created during dummy_psmx_request_loan(), is propagated to dummy_psmx_write().
/// - Check if shared memory is available.
/// - Expectation: Shared memory is available iff the psmx interface supports it (check for the false and the true case).
/// - Delete the domain
/// - Check the function call counts of the dummy psmx.
/// - Expectation: The counts match expectations. In particular, create counts must match their delete counterpart.
/// 
CU_Test(ddsc_psmxif, shared_memory)
{
  char strbuf[512];
  const size_t strbuf_size = sizeof(strbuf);
  {
    // Check that the data types I'm planning to use are actually suitable for use with shared memory.
    dds_data_type_properties_t props;
    props = dds_stream_data_types(SC_Model_desc.m_ops);
    CU_ASSERT_FATAL((props & DDS_DATA_TYPE_IS_MEMCPY_SAFE) == DDS_DATA_TYPE_IS_MEMCPY_SAFE);
    props = dds_stream_data_types(PsmxType1_desc.m_ops);
    CU_ASSERT_FATAL((props & DDS_DATA_TYPE_IS_MEMCPY_SAFE) == DDS_DATA_TYPE_IS_MEMCPY_SAFE);
  }

  for (size_t i = 0; i < 2; ++i) {
    const dds_domainid_t domainId = 0;
    dds_entity_t participant = create_participant(domainId);
    dds_entity_t domain = dds_get_parent(participant);
    dummy_mockstats_t* dmock = dummy_mockstats_get_ptr();
    CU_ASSERT_FATAL(dmock->cnt_create_psmx == 1); // Confirm the dummy psmx has been loaded.
    {
      // Assert that there is exactly one psmx instance.
      dds_entity* x = NULL;
      CU_ASSERT_FATAL(dds_entity_pin(domain, &x) == DDS_RETCODE_OK && dds_entity_kind(x) == DDS_KIND_DOMAIN);
      CU_ASSERT_FATAL(((dds_domain*)x)->psmx_instances.length == 1);
      dds_entity_unpin(x);
    }

    bool supports_shared_memory_expected = (bool)i;
    dmock->supports_shared_memory = supports_shared_memory_expected;

    dds_psmx_topic_t* psmx_topic_expected = NULL;
    dds_psmx_endpoint_t* psmx_endpt_expected = NULL;
    dds_psmx_endpoint_t* delete_endpoint_expected[4];
    memset(delete_endpoint_expected, 0x0, sizeof(delete_endpoint_expected));
    size_t delete_endpoint_idx = 0;
    const size_t endpt_cnt = sizeof(delete_endpoint_expected) / sizeof(delete_endpoint_expected[0]);

    // Check that the config string passed to `dds_create_domain()` has been correctly forwarded to the dummy psmx.
    char dmock_config_expected[] = "LOCATOR=4a4d203df6996395e1412fbecc2de4b6;SERVICE_NAME=service_psmx_dummy;KEYED_TOPICS=true;";
    CU_ASSERT_FATAL(strcmp(dmock->config, dmock_config_expected) == 0);

    void* sample = NULL;
    dds_entity_t writer1 = 0, reader1 = 0, writer2 = 0, reader2 = 0;

    char topicname[100];
    dummy_topics_alloc(dmock, 2);
    dummy_endpoints_alloc(dmock, endpt_cnt);

    create_unique_topic_name("shared_memory", topicname, sizeof(topicname));
    psmx_topic_expected = (dds_psmx_topic_t*)dmock->topics._buffer + dmock->topics._length;
    dds_entity_t topic1 = dds_create_topic(participant, &SC_Model_desc, topicname, NULL, NULL);
    CU_ASSERT_FATAL(topic1 > 0);

    psmx_endpt_expected = (dds_psmx_endpoint_t*)dmock->endpoints._buffer + dmock->endpoints._length;
    delete_endpoint_expected[delete_endpoint_idx++] = psmx_endpt_expected;
    writer1 = dds_create_writer(participant, topic1, NULL, NULL);
    CU_ASSERT_FATAL(writer1 > 0);
    CU_ASSERT_FATAL(dmock->create_endpoint_rcv_topic == psmx_topic_expected);
    CU_ASSERT_FATAL(dds_request_loan(writer1, &sample) == DDS_RETCODE_OK);
    CU_ASSERT_FATAL(dmock->request_loan_rcv_endpt == psmx_endpt_expected);
    dmock->write_rcv_loan = NULL;
    CU_ASSERT_FATAL(dds_write(writer1, sample) == DDS_RETCODE_OK);
    CU_ASSERT_FATAL(dmock->write_rcv_endpt == psmx_endpt_expected);
    CU_ASSERT_FATAL(dmock->write_rcv_loan == &dmock->loan);

    psmx_endpt_expected = (dds_psmx_endpoint_t*)dmock->endpoints._buffer + dmock->endpoints._length;
    delete_endpoint_expected[delete_endpoint_idx++] = psmx_endpt_expected;
    reader1 = dds_create_reader(participant, topic1, NULL, NULL);
    CU_ASSERT_FATAL(reader1 > 0);
    CU_ASSERT_FATAL(dmock->create_endpoint_rcv_topic == psmx_topic_expected);

    create_unique_topic_name("shared_memory", topicname, sizeof(topicname));
    psmx_topic_expected = (dds_psmx_topic_t*)dmock->topics._buffer + dmock->topics._length;
    dds_entity_t topic2 = dds_create_topic(participant, &PsmxType1_desc, topicname, NULL, NULL);
    CU_ASSERT_FATAL(topic2 > 0);

    psmx_endpt_expected = (dds_psmx_endpoint_t*)dmock->endpoints._buffer + dmock->endpoints._length;
    delete_endpoint_expected[delete_endpoint_idx++] = psmx_endpt_expected;
    writer2 = dds_create_writer(participant, topic2, NULL, NULL);
    CU_ASSERT_FATAL(writer2 > 0);
    CU_ASSERT_FATAL(dmock->create_endpoint_rcv_topic == psmx_topic_expected);
    CU_ASSERT_FATAL(dds_request_loan(writer2, &sample) == DDS_RETCODE_OK);
    CU_ASSERT_FATAL(dmock->request_loan_rcv_endpt == psmx_endpt_expected);
    dmock->write_rcv_loan = NULL;
    CU_ASSERT_FATAL(dds_write(writer2, sample) == DDS_RETCODE_OK);
    CU_ASSERT_FATAL(dmock->write_rcv_endpt == psmx_endpt_expected);
    CU_ASSERT_FATAL(dmock->write_rcv_loan == &dmock->loan);

    psmx_endpt_expected = (dds_psmx_endpoint_t*)dmock->endpoints._buffer + dmock->endpoints._length;
    delete_endpoint_expected[delete_endpoint_idx++] = psmx_endpt_expected;
    reader2 = dds_create_reader(participant, topic2, NULL, NULL);
    CU_ASSERT_FATAL(reader2 > 0);
    CU_ASSERT_FATAL(dmock->create_endpoint_rcv_topic == psmx_topic_expected);

    // Check that shared memory is available when it should, and not available when it shouldn't.
    CU_ASSERT_FATAL(dds_is_shared_memory_available(writer1) == supports_shared_memory_expected);
    CU_ASSERT_FATAL(dds_is_shared_memory_available(reader1) == supports_shared_memory_expected);
    CU_ASSERT_FATAL(dds_is_shared_memory_available(writer2) == supports_shared_memory_expected);
    CU_ASSERT_FATAL(dds_is_shared_memory_available(reader2) == supports_shared_memory_expected);

    // Check that psmx_endpoint pointers originally from `dummy_psmx_create_endpoint()`, end up in `dummy_psmx_delete_endpoint()`.
    CU_ASSERT_FATAL(delete_endpoint_idx == endpt_cnt);
    dds_delete(reader2);
    CU_ASSERT_FATAL(dmock->delete_endpoint_rcv_endpt == delete_endpoint_expected[--delete_endpoint_idx]);
    dds_delete(writer2);
    CU_ASSERT_FATAL(dmock->delete_endpoint_rcv_endpt == delete_endpoint_expected[--delete_endpoint_idx]);
    dds_delete(reader1);
    CU_ASSERT_FATAL(dmock->delete_endpoint_rcv_endpt == delete_endpoint_expected[--delete_endpoint_idx]);
    dds_delete(writer1);
    CU_ASSERT_FATAL(dmock->delete_endpoint_rcv_endpt == delete_endpoint_expected[--delete_endpoint_idx]);
    dds_delete(domain);

    // Check number of calls against expected counts.
    dummy_mockstats_tostring(dmock, strbuf, strbuf_size);
    printf("ddsc_psmxif_shared_memory calls counts:\n%s\n", strbuf);

    CU_ASSERT_FATAL(dmock->cnt_create_psmx == 1);

    CU_ASSERT_FATAL(dmock->cnt_type_qos_supported == 10);
    CU_ASSERT_FATAL(dmock->cnt_create_topic == 2);
    CU_ASSERT_FATAL(dmock->cnt_delete_topic == 2);
    CU_ASSERT_FATAL(dmock->cnt_deinit == 1);
    CU_ASSERT_FATAL(dmock->cnt_get_node_id == 1);
    CU_ASSERT_FATAL(dmock->cnt_supported_features == 4);

    CU_ASSERT_FATAL(dmock->cnt_create_endpoint == 4);
    CU_ASSERT_FATAL(dmock->cnt_delete_endpoint == 4);

    CU_ASSERT_FATAL(dmock->cnt_request_loan == 2);
    CU_ASSERT_FATAL(dmock->cnt_write == 2);
    CU_ASSERT_FATAL(dmock->cnt_take == 0);
    CU_ASSERT_FATAL(dmock->cnt_on_data_available == 2);
  }
}
