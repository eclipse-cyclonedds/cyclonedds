
#include <string.h>
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsi/ddsi_locator.h"
#include "dds/ddsc/dds_psmx.h"
#include "psmx_dummy_public.h"
#include "psmx_dummy_impl.h"

//### Helper functions ###

static char* get_config_option_value (const char* conf, const char* option_name)
{
  char* copy = ddsrt_strdup(conf), *cursor = copy, *tok;
  while ((tok = ddsrt_strsep(&cursor, ",/|;")) != NULL)
  {
    if (strlen(tok) == 0)
      continue;
    char* name = ddsrt_strsep(&tok, "=");
    if (name == NULL || tok == NULL)
    {
      ddsrt_free(copy);
      return NULL;
    }
    if (strcmp(name, option_name) == 0)
    {
      char* ret = ddsrt_strdup(tok);
      ddsrt_free(copy);
      return ret;
    }
  }
  ddsrt_free(copy);
  return NULL;
}

//### Dynamic library functions ###

static dummy_mockstats_t* g_mockstats;
static bool g_mockstats_owned;

static bool dummy_psmx_type_qos_supported(
  struct dds_psmx* psmx,
  dds_psmx_endpoint_type_t forwhat,
  dds_data_type_properties_t data_type_props,
  const struct dds_qos* qos
);
static struct dds_psmx_topic* dummy_psmx_create_topic(
  struct dds_psmx* psmx,
  const char* topic_name,
  const char* type_name,
  dds_data_type_properties_t data_type_props
);
static dds_return_t dummy_psmx_delete_topic(struct dds_psmx_topic* psmx_topic);
static dds_return_t dummy_psmx_deinit(struct dds_psmx* psmx);
static dds_psmx_node_identifier_t dummy_psmx_get_node_id(const struct dds_psmx* psmx);
static dds_psmx_features_t dummy_supported_features(const struct dds_psmx* psmx);

static struct dds_psmx_endpoint* dummy_psmx_create_endpoint(
  struct dds_psmx_topic* psmx_topic,
  const struct dds_qos* qos,
  dds_psmx_endpoint_type_t endpoint_type
);
static dds_return_t dummy_psmx_delete_endpoint(struct dds_psmx_endpoint* psmx_endpoint);

static dds_loaned_sample_t* dummy_psmx_ep_request_loan(struct dds_psmx_endpoint* psmx_endpoint, uint32_t size_requested);
static dds_return_t dummy_psmx_ep_write(struct dds_psmx_endpoint* psmx_endpoint, dds_loaned_sample_t* data);
static dds_loaned_sample_t* dummy_psmx_ep_take(struct dds_psmx_endpoint* psmx_endpoint);
static dds_return_t dummy_psmx_ep_on_data_available(struct dds_psmx_endpoint* psmx_endpoint, dds_entity_t reader);

static const dds_psmx_ops_t psmx_instance_ops = {
  .type_qos_supported = dummy_psmx_type_qos_supported,
  .create_topic = dummy_psmx_create_topic,
  .delete_topic = dummy_psmx_delete_topic,
  .deinit = dummy_psmx_deinit,
  .get_node_id = dummy_psmx_get_node_id,
  .supported_features = dummy_supported_features
};

static void dummy_mockstats_get_ownership(dummy_mockstats_t* mockstats)
{
  // Transfer ownership of mockstats to user.
  memcpy(mockstats, g_mockstats, sizeof(dummy_mockstats_t));
  if ( g_mockstats_owned ) {
    ddsrt_free(g_mockstats);
    g_mockstats_owned = false;
  }
  g_mockstats = mockstats;
}

static bool dummy_psmx_type_qos_supported(
  struct dds_psmx* psmx,
  dds_psmx_endpoint_type_t forwhat,
  dds_data_type_properties_t data_type_props,
  const struct dds_qos* qos
) {
  (void)psmx;
  (void)forwhat;
  (void)data_type_props;
  (void)qos;
  ++g_mockstats->cnt_type_qos_supported;
  return true;
}

static struct dds_psmx_topic* dummy_psmx_create_topic(
  struct dds_psmx* psmx,
  const char* topic_name,
  const char* type_name,
  dds_data_type_properties_t data_type_props
) {
  (void)data_type_props;
  struct dds_psmx_topic* topic = dds_alloc(sizeof(struct dds_psmx_topic));
  memset(topic, 0, sizeof(struct dds_psmx_topic));
  topic->ops.create_endpoint = dummy_psmx_create_endpoint;
  topic->ops.delete_endpoint = dummy_psmx_delete_endpoint;
  topic->psmx_instance = psmx;
  topic->topic_name = ddsrt_strdup(topic_name);
  topic->type_name = ddsrt_strdup(type_name);
  dds_add_psmx_topic_to_list(topic, &psmx->psmx_topics);
  ++g_mockstats->cnt_create_topic;
  return topic;
}

static dds_return_t dummy_psmx_delete_topic(struct dds_psmx_topic* psmx_topic)
{
  dds_psmx_topic_cleanup_generic(psmx_topic);
  dds_free(psmx_topic);
  ++g_mockstats->cnt_delete_topic;
  return DDS_RETCODE_OK;
}

static dds_return_t dummy_psmx_deinit(struct dds_psmx* psmx)
{
  dds_psmx_cleanup_generic(psmx);
  dds_free(psmx);
  ++g_mockstats->cnt_deinit;
  if ( g_mockstats_owned ) {
    ddsrt_free(g_mockstats);
    g_mockstats = NULL;
    g_mockstats_owned = false;
  }
  return DDS_RETCODE_OK;
}

static dds_psmx_node_identifier_t dummy_psmx_get_node_id(const struct dds_psmx* psmx)
{
  (void)psmx;
  dds_psmx_node_identifier_t node_id;
  memset(&node_id, 0, sizeof(dds_psmx_node_identifier_t));
  ++g_mockstats->cnt_get_node_id;
  return node_id;
}

static dds_psmx_features_t dummy_supported_features(const struct dds_psmx* psmx)
{
  (void)psmx;
  ++g_mockstats->cnt_supported_features;
  return DDS_PSMX_FEATURE_SHARED_MEMORY | DDS_PSMX_FEATURE_ZERO_COPY;
}

dds_return_t dummy_create_psmx(dds_psmx_t** psmx_out, dds_psmx_instance_id_t instance_id, const char* config)
{
  assert(psmx_out);

  g_mockstats = ddsrt_malloc(sizeof(dummy_mockstats_t));
  memset(g_mockstats, 0, sizeof(dummy_mockstats_t));
  g_mockstats_owned = true; // I own the mockstats until transferred to the user.

  struct dummy_psmx* psmx = dds_alloc(sizeof(struct dummy_psmx));
  memset(psmx, 0, sizeof(struct dummy_psmx));
  psmx->c.instance_name = dds_string_dup("dummy_psmx");
  psmx->c.instance_id = instance_id;
  psmx->c.ops = psmx_instance_ops;
  dds_psmx_init_generic(&psmx->c);

  if (config != NULL && strlen (config) > 0)
  {
    char* lstr = get_config_option_value (config, "LOCATOR");
    if (lstr != NULL)
    {
      if (strlen (lstr) != 32)
      {
        dds_free (lstr);
        goto err_locator;
      }
      uint8_t* const dst = (uint8_t*) psmx->c.locator->address;
      for (uint32_t n = 0; n < 32 && lstr[n]; n++)
      {
        int32_t num;
        if ((num = ddsrt_todigit (lstr[n])) < 0 || num >= 16)
        {
          dds_free (lstr);
          goto err_locator;
        }
        if ((n % 2) == 0)
          dst[n / 2] = (uint8_t) (num << 4);
        else
          dst[n / 2] |= (uint8_t) num;
      }
      dds_free (lstr);
    }
  }

  psmx->mockstats_get_ownership = dummy_mockstats_get_ownership;
  *psmx_out = (dds_psmx_t*)psmx;
  return DDS_RETCODE_OK;

err_locator:
  dds_psmx_cleanup_generic (&psmx->c);
  dds_free (psmx);
  return DDS_RETCODE_BAD_PARAMETER;
}

static struct dds_psmx_endpoint* dummy_psmx_create_endpoint(
  struct dds_psmx_topic* psmx_topic,
  const struct dds_qos* qos,
  dds_psmx_endpoint_type_t endpoint_type
) {
  (void)qos;
  struct dds_psmx_endpoint* endp = dds_alloc(sizeof(struct dds_psmx_endpoint));
  memset(endp, 0, sizeof(struct dds_psmx_endpoint));
  endp->ops.request_loan = dummy_psmx_ep_request_loan;
  endp->ops.write = dummy_psmx_ep_write;
  endp->ops.take = dummy_psmx_ep_take;
  endp->ops.on_data_available = dummy_psmx_ep_on_data_available;

  endp->psmx_topic = psmx_topic;
  endp->endpoint_type = endpoint_type;
  dds_add_psmx_endpoint_to_list(endp, &psmx_topic->psmx_endpoints);
  ++g_mockstats->cnt_create_endpoint;
  return endp;
}

static dds_return_t dummy_psmx_delete_endpoint(struct dds_psmx_endpoint* psmx_endpoint)
{
  dds_free(psmx_endpoint);
  ++g_mockstats->cnt_delete_endpoint;
  return DDS_RETCODE_OK;
}

static dds_loaned_sample_t* dummy_psmx_ep_request_loan(struct dds_psmx_endpoint* psmx_endpoint, uint32_t size_requested)
{
  (void)psmx_endpoint;
  (void)size_requested;
  // Details yet to be implemented
  ++g_mockstats->cnt_request_loan;
  return NULL;
}

static dds_return_t dummy_psmx_ep_write(struct dds_psmx_endpoint* psmx_endpoint, dds_loaned_sample_t* data)
{
  (void)psmx_endpoint;
  (void)data;
  // Details yet to be implemented
  ++g_mockstats->cnt_write;
  return DDS_RETCODE_OK;
}

static dds_loaned_sample_t* dummy_psmx_ep_take(struct dds_psmx_endpoint* psmx_endpoint)
{
  (void)psmx_endpoint;
  // Details yet to be implemented
  ++g_mockstats->cnt_take;
  return NULL;
}

static dds_return_t dummy_psmx_ep_on_data_available(struct dds_psmx_endpoint* psmx_endpoint, dds_entity_t reader)
{
  (void)psmx_endpoint;
  (void)reader;
  // Details yet to be implemented
  ++g_mockstats->cnt_on_data_available;
  return DDS_RETCODE_OK;
}
