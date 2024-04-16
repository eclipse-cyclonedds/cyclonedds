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

/**
 * @brief Create a participant object, providing a configuration to choose the psmx interface.
 * 
 * @param[in] int_dom the domain id
 * @param[in] cdds_psmx_name the name of the psmx interface to use
 * @param[in] locator an array of 16 bytes, NULL if not used
 * @return dds_entity_t the participant
 */
static char* create_config(dds_domainid_t int_dom, const char* cdds_psmx_name, const uint8_t* locator)
{
  char *configstr;
  char locator_str[74];
  if ( locator == NULL ) {
    locator_str[0] = 0;
  } else {
    const uint8_t *l = locator;
    snprintf(
      locator_str,
      sizeof(locator_str),
      "LOCATOR=%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x;",
      l[0], l[1], l[2], l[3], l[4], l[5], l[6], l[7], l[8], l[9], l[10], l[11], l[12], l[13], l[14], l[15]
    );
  }
  ddsrt_asprintf (&configstr, "\
${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}\
<General>\
  <AllowMulticast>spdp</AllowMulticast>\
  <Interfaces>\
    <PubSubMessageExchange name=\"%s\" library=\"psmx_%s\" priority=\"1000000\" config=\"%sSERVICE_NAME=psmx%d;KEYED_TOPICS=true;\" />\
  </Interfaces>\
</General>\
<Discovery>\
  <Tag>${CYCLONEDDS_PID}</Tag>\
  <ExternalDomainId>0</ExternalDomainId>\
</Discovery>\
<Tracing>\
  <OutputFile>cdds.log.%d</OutputFile>\
</Tracing>\
",
    cdds_psmx_name,
    cdds_psmx_name,
    locator_str,
    locator ? (int) locator[0] : -1, // This prevents plugins from forwarding across the "network".
    (int) int_dom // log file name
  );
  char *xconfigstr = ddsrt_expand_envvars (configstr, int_dom);
  ddsrt_free (configstr);
  return xconfigstr;
}

static bool endpoint_has_psmx_enabled (dds_entity_t rd_or_wr)
{
  dds_return_t rc;
  struct dds_entity *x;
  bool psmx_enabled = false;
  rc = dds_entity_pin (rd_or_wr, &x);
  CU_ASSERT_FATAL (rc == DDS_RETCODE_OK);
  switch (dds_entity_kind (x))
  {
    case DDS_KIND_READER: {
      struct dds_reader const * const rd = (struct dds_reader *) x;
      psmx_enabled = (rd->m_endpoint.psmx_endpoints.length > 0);
      break;
    }
    case DDS_KIND_WRITER: {
      struct dds_writer const * const wr = (struct dds_writer *) x;
      psmx_enabled = (wr->m_endpoint.psmx_endpoints.length > 0);
      break;
    }
    default: {
      CU_ASSERT_FATAL (dds_entity_kind (x) == DDS_KIND_READER || dds_entity_kind (x) == DDS_KIND_WRITER);
      break;
    }
  }
  dds_entity_unpin (x);
  return psmx_enabled;
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

/// @brief Check that creating a domain with more than one psmx interface fails.
/// @methodology
/// - Create a config string with two psmx interfaces.
/// - Try to create a domain using this config string.
/// - Expectation: Failed to create the domain.
/// 
CU_Test(ddsc_psmxif, config_multiple_psmx)
{
  dds_domainid_t domainId = 0;
  const char* cdds_psmx_name1 = "dummy";
  const char* cdds_psmx_name2 = "iox";
  char* configstr_in = NULL;
  {
    char *configstr;
    ddsrt_asprintf (&configstr, "\
${CYCLONEDDS_URI}${CYCLONEDDS_URI:+,}\
<General>\
  <AllowMulticast>spdp</AllowMulticast>\
  <Interfaces>\
    <PubSubMessageExchange name=\"%s\" library=\"psmx_%s\" priority=\"1000000\" config=\"SERVICE_NAME=psmx%d;KEYED_TOPICS=true;\" />\
    <PubSubMessageExchange name=\"%s\" library=\"psmx_%s\" priority=\"1000000\" config=\"SERVICE_NAME=psmx%d;KEYED_TOPICS=true;\" />\
  </Interfaces>\
</General>\
<Discovery>\
  <Tag>${CYCLONEDDS_PID}</Tag>\
  <ExternalDomainId>0</ExternalDomainId>\
</Discovery>\
<Tracing>\
  <OutputFile>cdds.log.%d</OutputFile>\
</Tracing>\
  ",
      cdds_psmx_name1,
      cdds_psmx_name1,
      -1, // This prevents plugins from forwarding across the "network".
      cdds_psmx_name2,
      cdds_psmx_name2,
      -1, // This prevents plugins from forwarding across the "network".
      (int) domainId // log file name
    );
    char *xconfigstr = ddsrt_expand_envvars(configstr, domainId);
    ddsrt_free(configstr);
    configstr_in = xconfigstr;
  }
  const dds_entity_t domain = dds_create_domain (domainId, configstr_in);
  CU_ASSERT_FATAL(domain <= 0);
}

/// @brief Check that shared memory can be enabled (when supported) and the used locator can be set.
/// @methodology
/// - Check that the data types I'm planning to use are actually suitable for use with shared memory.
/// - Expectation: They are memcopy-safe.
/// 
/// - Create a configuration with a psmx interface and specify the locator.
/// - Create a domain using this configuration.
/// - Check the locator used by the psmx instance.
/// - Expectation: The locator is the same as specified in the config for the domain.
/// - Query whether shared memory is supported.
/// - Assert that there is exactly one psmx instance.
/// - Assert that the psmx instance has a nonempty instance_name.
/// - Create some entities
/// - Check if shared memory is enabled.
/// - Expectation: Shared memory is enabled iff the psmx interface supports it (use queried value).
/// - Delete the domain
/// - Check the function call counts of the dummy psmx.
/// - Expectation: The counts match expectations. In particular, create counts must match their delete counterpart.
/// 
/// - Create a configuration with a psmx interface capable of shared memory and don't specify a locator.
/// - Create a domain using this configuration.
/// - Query whether shared memory is supported.
/// - Assert that there is exactly one psmx instance.
/// - Assert that the psmx instance has a nonempty instance_name.
/// - Create some entities
/// - Check if shared memory is enabled.
/// - Expectation: Shared memory is enabled iff the psmx interface supports it (use queried value).
/// - Delete the domain
/// - Check the function call counts of the dummy psmx.
/// - Expectation: The counts match expectations. In particular, create counts must match their delete counterpart.
/// 
CU_Test(ddsc_psmxif, shared_memory)
{
  dummy_mockstats_t* dmock = NULL;
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

  const dds_domainid_t domainId = 0;
  bool supports_shared_memory_expected = true;
  dds_psmx_endpoint_t* psmx_endpt_expected = NULL;
  dds_psmx_topic_t* psmx_topic_expected = NULL;
  bool specify_locator[] = {true, false};
  uint8_t locator_in[16];
  memset(locator_in, 0x0, sizeof(locator_in)); // Avoid warning 'uninitialized value'.
  ((uint64_t*)locator_in)[0] = (uint64_t)0x4a4d203df6996395;
  ((uint64_t*)locator_in)[1] = (uint64_t)0xe1412fbecc2de4b6;

  int N = sizeof(specify_locator) / sizeof(specify_locator[0]);
  for (int i = 0; i < N; ++i)
  {
    const uint8_t* locator = specify_locator[i] ? locator_in : NULL;
    char* configstr_in = create_config(domainId, "dummy", locator);
    const dds_entity_t domain = dds_create_domain(domainId, configstr_in);
    CU_ASSERT_FATAL(domain > 0);
    const dds_entity_t participant = dds_create_participant(domainId, NULL, NULL);
    CU_ASSERT_FATAL(participant > 0);
    {
      // Query whether shared memory is supported.
      dds_domain* dom = NULL;
      {
        dds_entity* x = NULL;
        dds_return_t rc = dds_entity_pin(domain, &x);
        CU_ASSERT_FATAL(rc == DDS_RETCODE_OK && dds_entity_kind(x) == DDS_KIND_DOMAIN);
        dom = (dds_domain*)x;
      }
      assert(dom->psmx_instances.length >= 1);
      struct dummy_psmx* dpsmx = (struct dummy_psmx*)dom->psmx_instances.instances[0];
      dmock = dpsmx->mockstats_get_ptr();
      CU_ASSERT_FATAL(dmock->cnt_create_psmx == 1); // Confirm the dummy psmx has been loaded.
      dds_entity_unpin(&dom->m_entity);
    }
    // Check that the config string passed to `dds_create_domain()` has been correctly forwarded to the dummy psmx.
    CU_ASSERT_FATAL(strstr(configstr_in, dmock->config) != NULL); // dmock->config is a substring of the original xml.
    ddsrt_free(configstr_in);

    dds_entity_t writer1 = 0, reader1 = 0, writer2 = 0, reader2 = 0;
    {
      char topicname[100];
      dummy_topics_alloc(dmock, 2);
      dummy_endpoints_alloc(dmock, 4);

      create_unique_topic_name("shared_memory", topicname, sizeof(topicname));
      psmx_topic_expected = (dds_psmx_topic_t*)dmock->topics._buffer + dmock->topics._length;
      dds_entity_t topic1 = dds_create_topic(participant, &SC_Model_desc, topicname, NULL, NULL);
      CU_ASSERT_FATAL(topic1 > 0);
      {
        dds_entity* x = NULL;
        dds_return_t rc = dds_entity_pin(topic1, &x);
        CU_ASSERT_FATAL(rc == DDS_RETCODE_OK && dds_entity_kind(x) == DDS_KIND_TOPIC);
        struct dds_psmx_topics_set* topics_set = &((dds_topic*)x)->m_ktopic->psmx_topics;
        CU_ASSERT_FATAL(topics_set->length == 1 && topics_set->topics[0] == psmx_topic_expected);
        dds_entity_unpin(x);
      }

      psmx_endpt_expected = (dds_psmx_endpoint_t*)dmock->endpoints._buffer + dmock->endpoints._length;
      writer1 = dds_create_writer(participant, topic1, NULL, NULL);
      CU_ASSERT_FATAL(writer1 > 0);
      {
        dds_entity* x = NULL;
        dds_return_t rc = dds_entity_pin(writer1, &x);
        CU_ASSERT_FATAL(rc == DDS_RETCODE_OK && dds_entity_kind(x) == DDS_KIND_WRITER);
        struct dds_psmx_endpoints_set* endpt_set = &((dds_writer*)x)->m_endpoint.psmx_endpoints;
        CU_ASSERT_FATAL(endpt_set->length == 1 && endpt_set->endpoints[0] == psmx_endpt_expected);
        dds_entity_unpin(x);
      }

      psmx_endpt_expected = (dds_psmx_endpoint_t*)dmock->endpoints._buffer + dmock->endpoints._length;
      reader1 = dds_create_reader(participant, topic1, NULL, NULL);
      CU_ASSERT_FATAL(reader1 > 0);
      {
        // Check the dummy psmx instance_name.
        dds_qos_t* qos = dds_create_qos();
        dds_get_qos(reader1, qos);
        uint32_t strs_len = 0;
        char** strs = NULL;
        CU_ASSERT_FATAL(dds_qget_psmx_instances(qos, &strs_len, &strs));
        CU_ASSERT_FATAL(strs_len == 1 && strcmp(strs[0], "dummy_psmx") == 0);
        free_strings(strs_len, strs);
        dds_delete_qos(qos);
      }
      {
        dds_entity* x = NULL;
        dds_return_t rc = dds_entity_pin(reader1, &x);
        CU_ASSERT_FATAL(rc == DDS_RETCODE_OK && dds_entity_kind(x) == DDS_KIND_READER);
        struct dds_psmx_endpoints_set* endpt_set = &((dds_reader*)x)->m_endpoint.psmx_endpoints;
        CU_ASSERT_FATAL(endpt_set->length == 1 && endpt_set->endpoints[0] == psmx_endpt_expected);
        dds_entity_unpin(x);
      }

      create_unique_topic_name("shared_memory", topicname, sizeof(topicname));
      psmx_topic_expected = (dds_psmx_topic_t*)dmock->topics._buffer + dmock->topics._length;
      dds_entity_t topic2 = dds_create_topic(participant, &PsmxType1_desc, topicname, NULL, NULL);
      CU_ASSERT_FATAL(topic2 > 0);
      {
        dds_entity* x = NULL;
        dds_return_t rc = dds_entity_pin(topic2, &x);
        CU_ASSERT_FATAL(rc == DDS_RETCODE_OK && dds_entity_kind(x) == DDS_KIND_TOPIC);
        struct dds_psmx_topics_set* topics_set = &((dds_topic*)x)->m_ktopic->psmx_topics;
        CU_ASSERT_FATAL(topics_set->length == 1 && topics_set->topics[0] == psmx_topic_expected);
        dds_entity_unpin(x);
      }

      psmx_endpt_expected = (dds_psmx_endpoint_t*)dmock->endpoints._buffer + dmock->endpoints._length;
      writer2 = dds_create_writer(participant, topic2, NULL, NULL);
      CU_ASSERT_FATAL(writer2 > 0);
      {
        dds_entity* x = NULL;
        dds_return_t rc = dds_entity_pin(writer2, &x);
        CU_ASSERT_FATAL(rc == DDS_RETCODE_OK && dds_entity_kind(x) == DDS_KIND_WRITER);
        struct dds_psmx_endpoints_set* endpt_set = &((dds_writer*)x)->m_endpoint.psmx_endpoints;
        CU_ASSERT_FATAL(endpt_set->length == 1 && endpt_set->endpoints[0] == psmx_endpt_expected);
        dds_entity_unpin(x);
      }

      psmx_endpt_expected = (dds_psmx_endpoint_t*)dmock->endpoints._buffer + dmock->endpoints._length;
      reader2 = dds_create_reader(participant, topic2, NULL, NULL);
      CU_ASSERT_FATAL(reader2 > 0);
      {
        dds_entity* x = NULL;
        dds_return_t rc = dds_entity_pin(reader2, &x);
        CU_ASSERT_FATAL(rc == DDS_RETCODE_OK && dds_entity_kind(x) == DDS_KIND_READER);
        struct dds_psmx_endpoints_set* endpt_set = &((dds_reader*)x)->m_endpoint.psmx_endpoints;
        CU_ASSERT_FATAL(endpt_set->length == 1 && endpt_set->endpoints[0] == psmx_endpt_expected);
        dds_entity_unpin(x);
      }
    }

    {
      // Check that shared memory is enabled when it should, and not enabled when it shouldn't.
      bool psmx_enabled;
      psmx_enabled = endpoint_has_psmx_enabled(writer1);
      CU_ASSERT_FATAL(psmx_enabled);
      CU_ASSERT_FATAL(dds_is_shared_memory_available(writer1) == supports_shared_memory_expected);

      psmx_enabled = endpoint_has_psmx_enabled(reader1);
      CU_ASSERT_FATAL(psmx_enabled);
      CU_ASSERT_FATAL(dds_is_shared_memory_available(reader1) == supports_shared_memory_expected);

      psmx_enabled = endpoint_has_psmx_enabled(writer2);
      CU_ASSERT_FATAL(psmx_enabled);
      CU_ASSERT_FATAL(dds_is_shared_memory_available(writer2) == supports_shared_memory_expected);

      psmx_enabled = endpoint_has_psmx_enabled(reader2);
      CU_ASSERT_FATAL(psmx_enabled);
      CU_ASSERT_FATAL(dds_is_shared_memory_available(reader2) == supports_shared_memory_expected);
    }
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

    CU_ASSERT_FATAL(dmock->cnt_request_loan == 0);
    CU_ASSERT_FATAL(dmock->cnt_write == 0);
    CU_ASSERT_FATAL(dmock->cnt_take == 0);
    CU_ASSERT_FATAL(dmock->cnt_on_data_available == 2);
    dmock = NULL;
  }
}