// Copyright(c) 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include "dds/dds.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsrt/environ.h"
#include "dds/ddsrt/threads.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsi/ddsi_locator.h"
#include "dds/ddsi/ddsi_protocol.h"
#include "dds/ddsi/ddsi_thread.h"
#include "dds/ddsc/dds_psmx.h"
#include "psmx_cdds_impl.h"
#include "psmx_cdds_data.h"

#define DDS_DOMAINID 50
#define DDS_CONFIG_BASE \
   "${CYCLONEDDS_URI}," \
   "<Discovery>" \
     "<Tag>${CYCLONEDDS_PID}</Tag>" \
   "</Discovery>"
#if 1
#define DDS_CONFIG DDS_CONFIG_BASE
#else
#define DDS_CONFIG DDS_CONFIG_BASE \
   "<Tracing>" \
     "<OutputFile>cyclonedds_psmx_impl.${CYCLONEDDS_DOMAIN_ID}.${CYCLONEDDS_PID}.log</OutputFile>" \
     "<Verbosity>finest</Verbosity>" \
   "</Tracing>"
#endif

#define ON_DATA_INIT       0
#define ON_DATA_RUNNING    1
#define ON_DATA_TERMINATE  2
#define ON_DATA_STOPPED    3

static dds_entity_t g_domain = -1;

struct cdds_psmx {
  struct dds_psmx c;
  char *service_name;
  dds_entity_t participant;
  dds_entity_t on_data_waitset;
  dds_entity_t stop_cond;
  ddsrt_atomic_uint32_t on_data_thread_state;
  ddsrt_atomic_uint32_t endpoint_refs;
};

struct cdds_psmx_topic {
  struct dds_psmx_topic c;
  dds_entity_t topic;
};

struct cdds_psmx_endpoint {
  struct dds_psmx_endpoint c;
  dds_entity_t psmx_cdds_endpoint;
  dds_entity_t cdds_endpoint;
  dds_entity_t deinit_cond;
  bool deleting;
};

struct on_data_available_thread_arg {
  struct cdds_psmx *cpsmx;
};

struct on_data_available_data {
  struct cdds_psmx_endpoint *cep;
};


static const uint32_t sample_padding = sizeof (struct dds_psmx_metadata) % 8 ? (sizeof (struct dds_psmx_metadata) / 8 + 1) * 8 : sizeof (struct dds_psmx_metadata);

static uint32_t on_data_available_thread (void *a);

static bool cdds_psmx_type_qos_supported (struct dds_psmx *psmx, dds_psmx_endpoint_type_t forwhat, dds_data_type_properties_t data_type_props, const struct dds_qos *qos);
static struct dds_psmx_topic * cdds_psmx_create_topic (struct dds_psmx * psmx,
    const char * topic_name, const char * type_name, dds_data_type_properties_t data_type_props);
static dds_return_t cdds_psmx_delete_topic (struct dds_psmx_topic *psmx_topic);
static dds_return_t cdds_psmx_deinit (struct dds_psmx *psmx);
static dds_psmx_node_identifier_t cdds_psmx_get_node_id (const struct dds_psmx *psmx);
static dds_psmx_features_t cdds_supported_features (const struct dds_psmx *psmx);

static const dds_psmx_ops_t psmx_instance_ops = {
  .type_qos_supported = cdds_psmx_type_qos_supported,
  .create_topic = cdds_psmx_create_topic,
  .delete_topic = cdds_psmx_delete_topic,
  .deinit = cdds_psmx_deinit,
  .get_node_id = cdds_psmx_get_node_id,
  .supported_features = cdds_supported_features
};

static struct dds_psmx_endpoint * cdds_psmx_create_endpoint (struct dds_psmx_topic *psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type);
static dds_return_t cdds_psmx_delete_endpoint (struct dds_psmx_endpoint *psmx_endpoint);

static const dds_psmx_topic_ops_t psmx_topic_ops = {
  .create_endpoint = cdds_psmx_create_endpoint,
  .delete_endpoint = cdds_psmx_delete_endpoint
};

static dds_loaned_sample_t * cdds_psmx_ep_request_loan (struct dds_psmx_endpoint *psmx_endpoint, uint32_t size_requested);
static dds_return_t cdds_psmx_ep_write (struct dds_psmx_endpoint *psmx_endpoint, dds_loaned_sample_t *data);
static dds_loaned_sample_t * cdds_psmx_ep_take (struct dds_psmx_endpoint *psmx_endpoint);
static dds_return_t cdds_psmx_ep_on_data_available (struct dds_psmx_endpoint *psmx_endpoint, dds_entity_t reader);

static const dds_psmx_endpoint_ops_t psmx_ep_ops = {
  .request_loan = cdds_psmx_ep_request_loan,
  .write = cdds_psmx_ep_write,
  .take = cdds_psmx_ep_take,
  .on_data_available = cdds_psmx_ep_on_data_available
};

static void cdds_loaned_sample_free (struct dds_loaned_sample *loaned_sample);

static const dds_loaned_sample_ops_t ls_ops = {
  .free = cdds_loaned_sample_free
};


static bool cdds_psmx_type_qos_supported (struct dds_psmx *psmx, dds_psmx_endpoint_type_t forwhat, dds_data_type_properties_t data_type_props, const struct dds_qos *qos)
{
  (void) psmx; (void) forwhat; (void) data_type_props; (void) qos;
  return true;
}

static struct dds_psmx_topic * cdds_psmx_create_topic (struct dds_psmx * psmx,
    const char * topic_name, const char * type_name, dds_data_type_properties_t data_type_props)
{
  struct cdds_psmx *cpsmx = (struct cdds_psmx *) psmx;
  if (g_domain == -1)
  {
    char *conf = ddsrt_expand_envvars (DDS_CONFIG, DDS_DOMAINID);
    g_domain = dds_create_domain (DDS_DOMAINID, conf);
    assert (g_domain >= 0);
    ddsrt_free (conf);
  }

  if (cpsmx->participant == -1)
  {
    cpsmx->participant = dds_create_participant (DDS_DOMAINID, NULL, NULL);
    assert (cpsmx->participant >= 0);
    cpsmx->on_data_waitset = dds_create_waitset (cpsmx->participant);
    assert (cpsmx->on_data_waitset >= 0);
    cpsmx->stop_cond = dds_create_guardcondition (cpsmx->participant);
    dds_return_t ret = dds_waitset_attach (cpsmx->on_data_waitset, cpsmx->stop_cond, 0);
    assert (ret == DDS_RETCODE_OK);
    (void) ret;

    ddsrt_atomic_st32 (&cpsmx->on_data_thread_state, ON_DATA_RUNNING);
    struct on_data_available_thread_arg *data = dds_alloc (sizeof (*data));
    data->cpsmx = cpsmx;

    ddsrt_thread_t tid;
    ddsrt_threadattr_t tattr;
    ddsrt_threadattr_init (&tattr);
    ddsrt_thread_create (&tid, "psmx_cdds_ondata", &tattr, on_data_available_thread, data);
  }

  struct cdds_psmx_topic *ctp = dds_alloc (sizeof (*ctp));
  char *ext_topic_name;
  ddsrt_asprintf (&ext_topic_name, "%s/%s", cpsmx->service_name ? cpsmx->service_name : "", topic_name);
  ctp->topic = dds_create_topic (cpsmx->participant, &cdds_psmx_data_desc, ext_topic_name, NULL, NULL);
  ddsrt_free (ext_topic_name);
  dds_psmx_topic_init_generic (&ctp->c, &psmx_topic_ops, psmx, topic_name, type_name, data_type_props);
  dds_add_psmx_topic_to_list (&ctp->c, &cpsmx->c.psmx_topics);
  return (struct dds_psmx_topic *) ctp;
}

static dds_return_t cdds_psmx_delete_topic (struct dds_psmx_topic *psmx_topic)
{
  struct cdds_psmx_topic *ctp = (struct cdds_psmx_topic *) psmx_topic;
  dds_psmx_topic_cleanup_generic (&ctp->c);
  dds_delete (ctp->topic);
  dds_free (ctp);
  return DDS_RETCODE_OK;
}


static uint32_t deinit_thread (void *arg)
{
  struct cdds_psmx *cpsmx = (struct cdds_psmx *) arg;

  while (ddsrt_atomic_ld32 (&cpsmx->on_data_thread_state) != ON_DATA_STOPPED)
    dds_sleepfor (DDS_MSECS (10));
  dds_delete (cpsmx->participant); // in separate thread because of thread state
  return 0;
}

static dds_return_t cdds_psmx_deinit (struct dds_psmx *psmx)
{
  struct cdds_psmx *cpsmx = (struct cdds_psmx *) psmx;

  dds_psmx_cleanup_generic (&cpsmx->c);

  ddsrt_atomic_st32 (&cpsmx->on_data_thread_state, ON_DATA_TERMINATE);
  dds_set_guardcondition (cpsmx->stop_cond, true);

  ddsrt_thread_t tid;
  ddsrt_threadattr_t tattr;
  ddsrt_threadattr_init (&tattr);
  ddsrt_thread_create (&tid, "cdds_psmx_deinit", &tattr, deinit_thread, cpsmx);

  ddsrt_thread_join (tid, NULL);

  ddsrt_free (cpsmx->service_name);
  dds_free (cpsmx);

  return DDS_RETCODE_OK;
}

static dds_psmx_node_identifier_t cdds_psmx_get_node_id (const struct dds_psmx *psmx)
{
  struct cdds_psmx *cpsmx = (struct cdds_psmx *) psmx;
  dds_guid_t guid;
  (void) dds_get_guid (cpsmx->participant, &guid);
  dds_psmx_node_identifier_t node_id;
  memcpy (node_id.x, &guid, sizeof (node_id.x));
  return node_id;
}

static dds_psmx_features_t cdds_supported_features (const struct dds_psmx *psmx)
{
  (void) psmx;
  return DDS_PSMX_FEATURE_ZERO_COPY;
}

static struct dds_psmx_endpoint * cdds_psmx_create_endpoint (struct dds_psmx_topic *psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type)
{
  struct cdds_psmx_topic * ctp = (struct cdds_psmx_topic *) psmx_topic;
  struct cdds_psmx *cpsmx = (struct cdds_psmx *) ctp->c.psmx_instance;
  struct cdds_psmx_endpoint *cep = dds_alloc (sizeof (*cep));
  cep->c.ops = psmx_ep_ops;
  cep->c.psmx_topic = psmx_topic;
  cep->c.endpoint_type = endpoint_type;

  cep->deinit_cond = dds_create_guardcondition (cpsmx->participant);
  dds_return_t ret = dds_waitset_attach (cpsmx->on_data_waitset, cep->deinit_cond, (dds_attach_t) cep);
  assert (ret == DDS_RETCODE_OK);
  (void) ret;
  cep->deleting = false;
  ddsrt_atomic_inc32 (&cpsmx->endpoint_refs);

  dds_qos_t *psmx_ep_qos = dds_create_qos ();
  uint32_t n_partitions;
  char **partitions;
  if (dds_qget_partition (qos, &n_partitions, &partitions) && n_partitions > 0)
  {
    dds_qset_partition (psmx_ep_qos, n_partitions, (const char **) partitions);
    for (uint32_t n = 0; n < n_partitions; n++)
      dds_free (partitions[n]);
    if (n_partitions > 0)
      dds_free (partitions);
  }

  switch (endpoint_type)
  {
    case DDS_PSMX_ENDPOINT_TYPE_READER:
      cep->psmx_cdds_endpoint = dds_create_reader (cpsmx->participant, ctp->topic, psmx_ep_qos, NULL);
      break;
    case DDS_PSMX_ENDPOINT_TYPE_WRITER:
      cep->psmx_cdds_endpoint = dds_create_writer (cpsmx->participant, ctp->topic, psmx_ep_qos, NULL);
      break;
    case DDS_PSMX_ENDPOINT_TYPE_UNSET:
      return NULL;
  }
  assert (cep->psmx_cdds_endpoint >= 0);
  dds_delete_qos (psmx_ep_qos);

  dds_add_psmx_endpoint_to_list (&cep->c, &ctp->c.psmx_endpoints);

  return (struct dds_psmx_endpoint *) cep;
}

static dds_return_t cdds_psmx_delete_endpoint (struct dds_psmx_endpoint *psmx_endpoint)
{
  struct cdds_psmx_endpoint *cep = (struct cdds_psmx_endpoint *) psmx_endpoint;
  cep->deleting = true;
  dds_set_guardcondition (cep->deinit_cond, true);
  return DDS_RETCODE_OK;
}

static dds_loaned_sample_t * cdds_psmx_ep_request_loan (struct dds_psmx_endpoint *psmx_ep, uint32_t size_requested)
{
  struct cdds_psmx_endpoint *cep = (struct cdds_psmx_endpoint *) psmx_ep;
  dds_loaned_sample_t *ls = NULL;
  if (cep->c.endpoint_type == DDS_PSMX_ENDPOINT_TYPE_WRITER)
  {
    uint32_t sz = size_requested + sample_padding;

    ls = dds_alloc (sizeof (*ls));
    ls->ops = ls_ops;
    ls->loan_origin.origin_kind = DDS_LOAN_ORIGIN_KIND_PSMX;
    ls->loan_origin.psmx_endpoint = (struct dds_psmx_endpoint *) cep;
    ls->metadata = dds_alloc (sizeof (*ls->metadata));
    ls->sample_ptr = dds_alloc (sz);
    memset (ls->sample_ptr, 0, sz);
    ddsrt_atomic_st32 (&ls->refc, 1);
  }
  return ls;
}

static dds_return_t cdds_psmx_ep_write (struct dds_psmx_endpoint *psmx_ep, dds_loaned_sample_t *data)
{
  struct cdds_psmx_endpoint *cep = (struct cdds_psmx_endpoint *) psmx_ep;

  struct cdds_psmx_data sample = {
    .sample_state = (uint32_t) data->metadata->sample_state,
    .data_type = data->metadata->data_type,
    .psmx_instance_id = data->metadata->instance_id,
    .sample_size = data->metadata->sample_size,
    .timestamp = data->metadata->timestamp,
    .statusinfo = data->metadata->statusinfo,
    .cdr_identifier = data->metadata->cdr_identifier,
    .cdr_options = data->metadata->cdr_options,
  };
  memcpy (&sample.guid, &data->metadata->guid, sizeof (sample.guid));
  sample.data._length = sample.data._maximum = data->metadata->sample_size;
  sample.data._release = true;
  sample.data._buffer = data->sample_ptr;
  dds_return_t rc = dds_write (cep->psmx_cdds_endpoint, &sample);
  assert (rc == 0);
  (void) rc;
  return DDS_RETCODE_OK;
}

static dds_loaned_sample_t * cdds_psmx_ep_take (struct dds_psmx_endpoint *psmx_ep)
{
  (void) psmx_ep;
  return NULL;
}

static dds_loaned_sample_t * incoming_sample_to_loan (struct cdds_psmx_endpoint *cep, const struct cdds_psmx_data *psmx_sample)
{
  struct dds_psmx_metadata *psmx_md = dds_alloc (sizeof (*psmx_md));
  psmx_md->cdr_identifier = psmx_sample->cdr_identifier;
  psmx_md->cdr_options = psmx_sample->cdr_options;
  psmx_md->instance_id = psmx_sample->psmx_instance_id;
  psmx_md->data_type = psmx_sample->data_type;
  memcpy (&psmx_md->guid, &psmx_sample->guid, sizeof (psmx_sample->guid));
  psmx_md->sample_size = psmx_sample->sample_size;
  psmx_md->sample_state = (enum dds_loaned_sample_state) psmx_sample->sample_state;
  psmx_md->statusinfo = psmx_sample->statusinfo;
  psmx_md->timestamp = psmx_sample->timestamp;

  dds_loaned_sample_t *ls = dds_alloc (sizeof (*ls));
  ls->ops = ls_ops;
  ls->loan_origin.origin_kind = DDS_LOAN_ORIGIN_KIND_PSMX;
  ls->loan_origin.psmx_endpoint = (struct dds_psmx_endpoint *) cep;
  ls->metadata = psmx_md;
  ls->sample_ptr = dds_alloc (psmx_sample->data._length);
  if (psmx_sample->data._length > 0)
    memcpy (ls->sample_ptr, psmx_sample->data._buffer, psmx_sample->data._length);
  ddsrt_atomic_st32 (&ls->refc, 1);
  return ls;
}

// FIXME: should be less?
// FIXME: I think so, this eats a lot of stack, is extremely unlikely to ever occur in general, and more specifically so in our test cases
#define MAX_TRIGGERS 999

static uint32_t on_data_available_thread (void *a)
{
  struct on_data_available_thread_arg *args = (struct on_data_available_thread_arg *) a;
  struct cdds_psmx *cpsmx = (struct cdds_psmx *) args->cpsmx;
  dds_free (args);

  while (ddsrt_atomic_ld32 (&cpsmx->on_data_thread_state) == ON_DATA_RUNNING || ddsrt_atomic_ld32 (&cpsmx->endpoint_refs) > 0)
  {
    dds_attach_t triggered[MAX_TRIGGERS];
    dds_return_t n_triggers = dds_waitset_wait (cpsmx->on_data_waitset, triggered, MAX_TRIGGERS, DDS_MSECS (10));
    assert (n_triggers <= MAX_TRIGGERS);
    if (n_triggers > 0)
    {
      for (int32_t t = 0; t < n_triggers; t++)
      {
        struct cdds_psmx_endpoint *cep = (struct cdds_psmx_endpoint *) triggered[t];
        if (cep && cep->deleting)
        {
          dds_waitset_detach (cpsmx->on_data_waitset, cep->deinit_cond);
          dds_delete (cep->deinit_cond);
          dds_waitset_detach (cpsmx->on_data_waitset, cep->psmx_cdds_endpoint);
          dds_delete (cep->psmx_cdds_endpoint);
          dds_free (cep);
          ddsrt_atomic_dec32 (&cpsmx->endpoint_refs);
          // restart from dds_waitset_wait because this endpoint may additionally have data
          // and we can't run the risk of encountering it in a following entry in "triggered"
          break;
        }

        if (ddsrt_atomic_ld32 (&cpsmx->on_data_thread_state) == ON_DATA_RUNNING)
        {
          assert (cep);
          dds_sample_info_t si;
          dds_return_t n, rc;
          void *raw = NULL;
          while ((n = dds_take (cep->psmx_cdds_endpoint, &raw, &si, 1, 1)) == 1)
          {
            if (si.valid_data)
            {
              dds_loaned_sample_t *loaned_sample = incoming_sample_to_loan (cep, raw);
              (void) dds_reader_store_loaned_sample (cep->cdds_endpoint, loaned_sample);
              dds_loaned_sample_unref (loaned_sample);
            }
            rc = dds_return_loan (cep->psmx_cdds_endpoint, &raw, n);
            assert (rc == 0);
            assert (raw == NULL);
            (void) rc;
          }
          assert (n == 0);
        }
      }
    }
  }

  ddsrt_atomic_st32 (&cpsmx->on_data_thread_state, ON_DATA_STOPPED);
  return 0;
}

static dds_return_t cdds_psmx_ep_on_data_available (struct dds_psmx_endpoint *psmx_endpoint, dds_entity_t reader)
{
  struct cdds_psmx_endpoint *cep = (struct cdds_psmx_endpoint *) psmx_endpoint;
  struct cdds_psmx *cpsmx = (struct cdds_psmx *) cep->c.psmx_topic->psmx_instance;
  cep->cdds_endpoint = reader;

  dds_return_t ret = dds_set_status_mask (cep->psmx_cdds_endpoint, DDS_DATA_AVAILABLE_STATUS);
  assert (ret == DDS_RETCODE_OK);
  ret = dds_waitset_attach (cpsmx->on_data_waitset, cep->psmx_cdds_endpoint, (dds_attach_t) cep);
  assert (ret == DDS_RETCODE_OK);
  (void) ret;

  return DDS_RETCODE_OK;
}

static void cdds_loaned_sample_free (struct dds_loaned_sample *loaned_sample)
{
  dds_free (loaned_sample->metadata);
  dds_free (loaned_sample->sample_ptr);
  dds_free (loaned_sample);
}

static char * get_config_option_value (const char *conf, const char *option_name)
{
  char *copy = ddsrt_strdup(conf), *cursor = copy, *tok;
  while ((tok = ddsrt_strsep(&cursor, ",/|;")) != NULL)
  {
    if (strlen(tok) == 0)
      continue;
    char *name = ddsrt_strsep(&tok, "=");
    if (name == NULL || tok == NULL)
    {
      ddsrt_free(copy);
      return NULL;
    }
    if (strcmp(name, option_name) == 0)
    {
      char *ret = ddsrt_strdup(tok);
      ddsrt_free(copy);
      return ret;
    }
  }
  ddsrt_free(copy);
  return NULL;
}

dds_return_t cdds_create_psmx (dds_psmx_t **psmx_out, dds_psmx_instance_id_t instance_id, const char *config)
{
  assert (psmx_out);

  ddsrt_atomic_st32 (&ddsi_thread_nested_gv_allowed, 1);

  struct cdds_psmx *psmx = dds_alloc (sizeof (*psmx));
  psmx->c.instance_name = dds_string_dup ("cdds-psmx");
  psmx->c.instance_id = instance_id;
  psmx->c.ops = psmx_instance_ops;
  dds_psmx_init_generic (&psmx->c);
  psmx->participant = -1;
  ddsrt_atomic_st32 (&psmx->on_data_thread_state, ON_DATA_INIT);
  ddsrt_atomic_st32 (&psmx->endpoint_refs, 0);
  memset ((char *) psmx->c.locator->address, 0, sizeof (psmx->c.locator->address));

  if (config != NULL && strlen (config) > 0)
  {
    char *lstr = get_config_option_value (config, "LOCATOR");
    if (lstr != NULL)
    {
      if (strlen (lstr) != 32)
      {
        dds_free (lstr);
        goto err_locator;
      }
      unsigned char * const dst = (unsigned char *) psmx->c.locator->address;
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

    char *sn = get_config_option_value (config, "SERVICE_NAME");
    psmx->service_name = sn;
  }

  *psmx_out = (dds_psmx_t *) psmx;
  return DDS_RETCODE_OK;

err_locator:
  dds_psmx_cleanup_generic (&psmx->c);
  dds_free (psmx);
  return DDS_RETCODE_BAD_PARAMETER;
}

