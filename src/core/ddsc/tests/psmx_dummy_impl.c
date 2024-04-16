
#include <string.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsi/ddsi_locator.h"
#include "dds/ddsc/dds_psmx.h"
#include "psmx_dummy_public.h"
#include "psmx_dummy_impl.h"

void dummy_topics_alloc(dummy_mockstats_t* mockstats, uint32_t topics_capacity)
{
  mockstats->topics._maximum = topics_capacity;
  mockstats->topics._length = 0;
  mockstats->topics._buffer = ddsrt_malloc(topics_capacity * sizeof(dds_psmx_topic_t));
}

void dummy_endpoints_alloc(dummy_mockstats_t* mockstats, uint32_t endpoints_capacity)
{
  mockstats->endpoints._maximum = endpoints_capacity;
  mockstats->endpoints._length = 0;
  mockstats->endpoints._buffer = ddsrt_malloc(endpoints_capacity * sizeof(dds_psmx_endpoint_t));
}

static dummy_mockstats_t g_mockstats;

static dummy_mockstats_t* dummy_mockstats_get_ptr()
{
  return &g_mockstats;
}

static dds_loaned_sample_t* dummy_psmx_ep_request_loan(dds_psmx_endpoint_t* psmx_endpoint, uint32_t size_requested)
{
  (void)psmx_endpoint;
  (void)size_requested;
  // Details yet to be implemented
  ++g_mockstats.cnt_request_loan;
  return NULL;
}

static dds_return_t dummy_psmx_ep_write(dds_psmx_endpoint_t* psmx_endpoint, dds_loaned_sample_t* data)
{
  (void)psmx_endpoint;
  (void)data;
  // Details yet to be implemented
  ++g_mockstats.cnt_write;
  return DDS_RETCODE_OK;
}

static dds_loaned_sample_t* dummy_psmx_ep_take(dds_psmx_endpoint_t* psmx_endpoint)
{
  (void)psmx_endpoint;
  // Details yet to be implemented
  ++g_mockstats.cnt_take;
  return NULL;
}

static dds_return_t dummy_psmx_ep_on_data_available(dds_psmx_endpoint_t* psmx_endpoint, dds_entity_t reader)
{
  (void)psmx_endpoint;
  (void)reader;
  // Details yet to be implemented
  ++g_mockstats.cnt_on_data_available;
  return DDS_RETCODE_OK;
}

static dds_psmx_endpoint_t* dummy_psmx_create_endpoint(
  dds_psmx_topic_t* psmx_topic,
  const struct dds_qos* qos,
  dds_psmx_endpoint_type_t endpoint_type
) {
  (void)qos;
  dds_psmx_endpoint_t* endp = (dds_psmx_endpoint_t*)g_mockstats.endpoints._buffer + g_mockstats.endpoints._length++;
  memset(endp, 0, sizeof(dds_psmx_endpoint_t));
  endp->ops.request_loan = dummy_psmx_ep_request_loan;
  endp->ops.write = dummy_psmx_ep_write;
  endp->ops.take = dummy_psmx_ep_take;
  endp->ops.on_data_available = dummy_psmx_ep_on_data_available;

  endp->psmx_topic = psmx_topic;
  endp->endpoint_type = endpoint_type;
  dds_add_psmx_endpoint_to_list(endp, &psmx_topic->psmx_endpoints);
  ++g_mockstats.cnt_create_endpoint;
  return endp;
}

static dds_return_t dummy_psmx_delete_endpoint(dds_psmx_endpoint_t* psmx_endpoint)
{
  memcpy(psmx_endpoint, (dds_psmx_endpoint_t*)g_mockstats.endpoints._buffer + (--g_mockstats.endpoints._length), sizeof(dds_psmx_endpoint_t));
  ++g_mockstats.cnt_delete_endpoint;
  return DDS_RETCODE_OK;
}

static bool dummy_psmx_type_qos_supported(
  dds_psmx_t* psmx,
  dds_psmx_endpoint_type_t forwhat,
  dds_data_type_properties_t data_type_props,
  const struct dds_qos* qos
) {
  (void)psmx;
  (void)forwhat;
  (void)data_type_props;
  (void)qos;
  ++g_mockstats.cnt_type_qos_supported;
  return true;
}

static dds_psmx_topic_t* dummy_psmx_create_topic(
  dds_psmx_t* psmx,
  const char* topic_name,
  const char* type_name,
  dds_data_type_properties_t data_type_props
) {
  (void)data_type_props;
  assert(g_mockstats.topics._length < g_mockstats.topics._maximum);
  dds_psmx_topic_t* topic = (dds_psmx_topic_t*)g_mockstats.topics._buffer + g_mockstats.topics._length++;
  memset(topic, 0, sizeof(dds_psmx_topic_t));
  topic->ops.create_endpoint = dummy_psmx_create_endpoint;
  topic->ops.delete_endpoint = dummy_psmx_delete_endpoint;
  topic->psmx_instance = psmx;
  topic->topic_name = ddsrt_strdup(topic_name);
  topic->type_name = ddsrt_strdup(type_name);
  dds_add_psmx_topic_to_list(topic, &psmx->psmx_topics);
  ++g_mockstats.cnt_create_topic;
  return topic;
}

static dds_return_t dummy_psmx_delete_topic(dds_psmx_topic_t* psmx_topic)
{
  dds_psmx_topic_cleanup_generic(psmx_topic);
  memcpy(psmx_topic, (dds_psmx_topic_t*)g_mockstats.topics._buffer + (--g_mockstats.topics._length), sizeof(dds_psmx_topic_t));
  ++g_mockstats.cnt_delete_topic;
  return DDS_RETCODE_OK;
}

static dds_return_t dummy_psmx_deinit(dds_psmx_t* psmx)
{
  dds_psmx_cleanup_generic(psmx);
  dds_free(psmx);
  ++g_mockstats.cnt_deinit;
  ddsrt_free(g_mockstats.config);
  ddsrt_free(g_mockstats.topics._buffer);
  ddsrt_free(g_mockstats.endpoints._buffer);
  return DDS_RETCODE_OK;
}

static dds_psmx_node_identifier_t dummy_psmx_get_node_id(const dds_psmx_t* psmx)
{
  (void)psmx;
  dds_psmx_node_identifier_t node_id;
  memset(&node_id, 0, sizeof(dds_psmx_node_identifier_t));
  ++g_mockstats.cnt_get_node_id;
  return node_id;
}

static dds_psmx_features_t dummy_supported_features(const dds_psmx_t* psmx)
{
  (void)psmx;
  ++g_mockstats.cnt_supported_features;
  return DDS_PSMX_FEATURE_SHARED_MEMORY | DDS_PSMX_FEATURE_ZERO_COPY;
}

dds_return_t dummy_create_psmx(dds_psmx_t** psmx_out, dds_psmx_instance_id_t instance_id, const char* config)
{
  assert(psmx_out);
  memset(&g_mockstats, 0, sizeof(dummy_mockstats_t));
  g_mockstats.cnt_create_psmx = 1;

  dummy_psmx_t* psmx = dds_alloc(sizeof(dummy_psmx_t));
  memset(psmx, 0, sizeof(dummy_psmx_t));
  psmx->c.instance_name = dds_string_dup("dummy_psmx");
  psmx->c.instance_id = instance_id;

  psmx->c.ops.type_qos_supported = dummy_psmx_type_qos_supported;
  psmx->c.ops.create_topic = dummy_psmx_create_topic;
  psmx->c.ops.delete_topic = dummy_psmx_delete_topic;
  psmx->c.ops.deinit = dummy_psmx_deinit;
  psmx->c.ops.get_node_id = dummy_psmx_get_node_id;
  psmx->c.ops.supported_features = dummy_supported_features;
  dds_psmx_init_generic(&psmx->c);

  g_mockstats.config = ddsrt_strdup(config);

  psmx->mockstats_get_ptr = dummy_mockstats_get_ptr;
  *psmx_out = (dds_psmx_t*)psmx;
  return DDS_RETCODE_OK;
}
