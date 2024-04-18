// Copyright(c) 2022 to 2023 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/dynlib.h"
#include "dds/ddsrt/mh3.h"
#include "dds/ddsrt/io.h"
#include "dds/ddsi/ddsi_locator.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/dds.h"
#include "dds__types.h"
#include "dds__psmx.h"
#include "dds__qos.h"
#include "dds__entity.h"
#include "dds__writer.h"

static struct dds_psmx_endpoint * psmx_create_endpoint (struct dds_psmx_topic *psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type);
static dds_return_t psmx_delete_endpoint (struct dds_psmx_endpoint *psmx_endpoint);

dds_return_t dds_add_psmx_topic_to_list (struct dds_psmx_topic *psmx_topic, struct dds_psmx_topic_list_elem **list)
{
  if (!psmx_topic)
    return DDS_RETCODE_BAD_PARAMETER;

  struct dds_psmx_topic_list_elem *ptr = dds_alloc (sizeof (struct dds_psmx_topic_list_elem));
  if (!ptr)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  ptr->topic = psmx_topic;
  ptr->next = NULL;

  if (!*list)
  {
    ptr->prev = NULL;
    *list = ptr;
  }
  else
  {
    struct dds_psmx_topic_list_elem *ptr2 = *list;
    while (ptr2->next)
      ptr2 = ptr2->next;
    ptr2->next = ptr;
    ptr->prev = ptr2;
  }

  return DDS_RETCODE_OK;
}

dds_return_t dds_remove_psmx_topic_from_list (struct dds_psmx_topic *psmx_topic, struct dds_psmx_topic_list_elem **list)
{
  if (!psmx_topic || !list || !*list)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_psmx_topic_list_elem *list_entry = *list;

  while (list_entry && list_entry->topic != psmx_topic)
    list_entry = list_entry->next;

  if (list_entry != NULL && (ret = list_entry->topic->psmx_instance->ops.delete_topic (list_entry->topic)) == DDS_RETCODE_OK)
  {
    if (list_entry->prev)
      list_entry->prev->next = list_entry->next;

    if (list_entry->next)
      list_entry->next->prev = list_entry->prev;

    if (list_entry == *list)
      *list = list_entry->next;

    dds_free (list_entry);
  }

  return ret;
}

dds_return_t dds_add_psmx_endpoint_to_list (struct dds_psmx_endpoint *psmx_endpoint, struct dds_psmx_endpoint_list_elem **list)
{
  if (!psmx_endpoint)
    return DDS_RETCODE_BAD_PARAMETER;

  struct dds_psmx_endpoint_list_elem *ptr = dds_alloc (sizeof (struct dds_psmx_endpoint_list_elem));
  if (!ptr)
    return DDS_RETCODE_OUT_OF_RESOURCES;

  ptr->endpoint = psmx_endpoint;
  ptr->next = NULL;

  if (!*list)
  {
    ptr->prev = NULL;
    *list = ptr;
  }
  else
  {
    struct dds_psmx_endpoint_list_elem *ptr2 = *list;
    while (ptr2->next)
      ptr2 = ptr2->next;
    ptr2->next = ptr;
    ptr->prev = ptr2;
  }

  return DDS_RETCODE_OK;
}

dds_return_t dds_remove_psmx_endpoint_from_list (struct dds_psmx_endpoint *psmx_endpoint, struct dds_psmx_endpoint_list_elem **list)
{
  if (!psmx_endpoint || !list || !*list)
    return DDS_RETCODE_BAD_PARAMETER;

  dds_return_t ret = DDS_RETCODE_OK;
  struct dds_psmx_endpoint_list_elem *list_entry = *list;

  while (list_entry && list_entry->endpoint != psmx_endpoint)
    list_entry = list_entry->next;

  if (list_entry != NULL && (ret = psmx_delete_endpoint (list_entry->endpoint)) == DDS_RETCODE_OK)
  {
    if (list_entry->prev)
      list_entry->prev->next = list_entry->next;

    if (list_entry->next)
      list_entry->next->prev = list_entry->prev;

    if (list_entry == *list)
      *list = list_entry->next;

    dds_free (list_entry);
  }

  return ret;
}

dds_return_t dds_psmx_init_generic (struct dds_psmx * psmx)
{
  struct ddsi_locator *loc = dds_alloc (sizeof (*loc));
  if (loc == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  memset (loc, 0, sizeof (*loc));

  dds_psmx_node_identifier_t node_id = psmx->ops.get_node_id (psmx);
  memcpy (loc->address, &node_id, sizeof (node_id));
  loc->port = psmx->instance_id;
  loc->kind = DDSI_LOCATOR_KIND_PSMX;

  psmx->locator = loc;

  return DDS_RETCODE_OK;
}

dds_return_t dds_psmx_cleanup_generic (struct dds_psmx *psmx)
{
  dds_return_t ret = DDS_RETCODE_OK;
  dds_free ((void *) psmx->instance_name);
  dds_free ((void *) psmx->locator);

  while (ret == DDS_RETCODE_OK && psmx->psmx_topics)
    ret = dds_remove_psmx_topic_from_list (psmx->psmx_topics->topic, &psmx->psmx_topics);

  return ret;
}

dds_return_t dds_psmx_topic_init_generic (struct dds_psmx_topic *psmx_topic, const dds_psmx_topic_ops_t *ops, const struct dds_psmx * psmx, const char *topic_name, const char * type_name, dds_data_type_properties_t data_type_props)
{
  psmx_topic->ops = *ops;
  psmx_topic->psmx_instance = (struct dds_psmx *) psmx;
  psmx_topic->topic_name = dds_string_dup (topic_name);
  psmx_topic->type_name = dds_string_dup (type_name);
  uint32_t topic_hash = ddsrt_mh3 (psmx_topic->topic_name, strlen (psmx_topic->topic_name), 0);
  psmx_topic->data_type = ddsrt_mh3 (&psmx->instance_id, sizeof (psmx->instance_id), topic_hash);
  psmx_topic->data_type_props = data_type_props;
  psmx_topic->psmx_endpoints = NULL;
  return DDS_RETCODE_OK;
}

dds_return_t dds_psmx_topic_cleanup_generic (struct dds_psmx_topic *psmx_topic)
{
  dds_return_t ret = DDS_RETCODE_OK;
  while (ret == DDS_RETCODE_OK && psmx_topic->psmx_endpoints)
    ret = dds_remove_psmx_endpoint_from_list (psmx_topic->psmx_endpoints->endpoint, &psmx_topic->psmx_endpoints);
  dds_free (psmx_topic->type_name);
  dds_free (psmx_topic->topic_name);
  return ret;
}

dds_loaned_sample_t * dds_psmx_endpoint_request_loan (struct dds_psmx_endpoint *psmx_endpoint, uint32_t sz)
{
  assert (psmx_endpoint->ops.request_loan);
  dds_loaned_sample_t *loaned_sample = psmx_endpoint->ops.request_loan (psmx_endpoint, sz);
  if (loaned_sample)
  {
    loaned_sample->metadata->sample_state = DDS_LOANED_SAMPLE_STATE_UNITIALIZED;
    loaned_sample->metadata->sample_size = sz;
    loaned_sample->metadata->instance_id = psmx_endpoint->psmx_topic->psmx_instance->instance_id;
    loaned_sample->metadata->data_type = psmx_endpoint->psmx_topic->data_type;
  }
  return loaned_sample;
}

static dds_psmx_instance_id_t get_psmx_instance_id (const struct ddsi_domaingv * gv, const char *config_name)
{
  uint32_t ext_domainid = gv->config.extDomainId.value;
  uint32_t hashed_id = ddsrt_mh3 (&ext_domainid, sizeof (ext_domainid), 0x0);
  return ddsrt_mh3 (config_name, strlen (config_name), hashed_id);
}

char *dds_pubsub_message_exchange_configstr (const char *config)
{
  // Check syntax: only KEY=VALUE pairs separated by ;, with backslash an escape character
  // We make no assumptions on the names of the keys or their values, except that no keys
  // may have CYCLONEDDS_ as a prefix, contain an escape character or an equals sign.
  const char *kstart = config; // init to pacify compiler
  enum { START, KEY0, KEY, VALUE_NORM, VALUE_ESCAPED } cs = START;
  for (const char *c = config; *c; c++) {
    switch (cs) {
      case START: // start of string, signalled for acceptance check
      case KEY0: // first character of key
        kstart = c;
        if (*c == '=') // key may not be empty
          goto malformed;
        cs = KEY;
        // falls through
      case KEY: // following characters of key
        if (*c == ';' || *c == '\\') // key may not contain ; or backslash
          goto malformed;
        if (*c == '=') { // key may not have CYCLONEDDS_ as prefix
          cs = VALUE_NORM;
          if (c - kstart >= 11 && memcmp (kstart, "CYCLONEDDS_", 11) == 0)
            goto malformed;
        }
        break;
      case VALUE_NORM: // non-escaped characters in value
        if (*c == ';' || *c == '\0') // ; -> next key (end of string same)
          cs = KEY0;
        else if (*c == '\\') // escape next character
          cs = VALUE_ESCAPED;
        break;
      case VALUE_ESCAPED: // anything goes
        cs = VALUE_NORM; // but only for this one character
        break;
    }
  }
  switch (cs)
  {
    case START:      // empty config string is ok
    case KEY0:       // looking at the next key (after ';')
    case VALUE_NORM: // end of value, we accept a missing ';' at the end
      break;
    default:
      goto malformed;
  }

  char *configstr = NULL;
  // Config checking verifies structure of config string and absence of any CYCLONEDDS_
  // We append a semicolon if the original config string did not end on one
  ddsrt_asprintf (&configstr, "%s%s", config, (cs == VALUE_NORM) ? ";" : "");
  return configstr;

malformed:
  return NULL;
}

static dds_return_t psmx_instance_load (const struct ddsi_domaingv *gv, const struct ddsi_config_psmx *config, struct dds_psmx **out, ddsrt_dynlib_t *lib_handle)
{
  dds_psmx_create_fn creator = NULL;
  const char *lib_name;
  ddsrt_dynlib_t handle;
  char load_fn[100];
  dds_return_t ret = DDS_RETCODE_ERROR;
  struct dds_psmx *psmx_instance = NULL;

  if (!config->library || config->library[0] == '\0')
    lib_name = config->name;
  else
    lib_name = config->library;

  char *configstr;
  if ((configstr = dds_pubsub_message_exchange_configstr (config->config)) == NULL)
  {
    GVERROR ("Configuration for PSMX instance '%s' is invalid\n", config->name);
    goto err_configstr;
  }

  if ((ret = ddsrt_dlopen (lib_name, true, &handle)) != DDS_RETCODE_OK)
  {
    char buf[1024];
    (void) ddsrt_dlerror (buf, sizeof(buf));
    GVERROR ("Failed to load PSMX library '%s' with error \"%s\".\n", lib_name, buf);
    goto err_dlopen;
  }

  (void) snprintf (load_fn, sizeof (load_fn), "%s_create_psmx", config->name);

  if ((ret = ddsrt_dlsym (handle, load_fn, (void**) &creator)) != DDS_RETCODE_OK)
  {
    GVERROR ("Failed to initialize PSMX instance '%s', could not load init function '%s'.\n", config->name, load_fn);
    goto err_dlsym;
  }

  if ((ret = creator (&psmx_instance, get_psmx_instance_id (gv, config->name), configstr)) != DDS_RETCODE_OK)
  {
    GVERROR ("Failed to initialize PSMX instance '%s'.\n", config->name);
    goto err_init;
  }
  psmx_instance->priority = config->priority.value;
  *out = psmx_instance;
  *lib_handle = handle;
  ddsrt_free (configstr);
  return DDS_RETCODE_OK;

err_init:
err_dlsym:
  ddsrt_dlclose (handle);
err_dlopen:
  ddsrt_free (configstr);
err_configstr:
  return ret;
}

static int compare_psmx_prio (const void *va, const void *vb)
{
  const struct dds_psmx *psmx1 = va;
  const struct dds_psmx *psmx2 = vb;
  return (psmx1->priority == psmx2->priority) ? 0 : ((psmx1->priority < psmx2->priority) ? 1 : -1);
}

dds_return_t dds_pubsub_message_exchange_init (const struct ddsi_domaingv *gv, struct dds_domain *domain)
{
  dds_return_t ret = DDS_RETCODE_OK;
  if (gv->config.psmx_instances != NULL)
  {
    struct ddsi_config_psmx_listelem *iface = gv->config.psmx_instances;
    while (iface && domain->psmx_instances.length < DDS_MAX_PSMX_INSTANCES)
    {
      GVLOG(DDS_LC_INFO, "Loading PSMX instances %s\n", iface->cfg.name);
      struct dds_psmx *psmx = NULL;
      ddsrt_dynlib_t lib_handle;
      if (psmx_instance_load (gv, &iface->cfg, &psmx, &lib_handle) == DDS_RETCODE_OK)
      {
        domain->psmx_instances.instances[domain->psmx_instances.length] = psmx;
        domain->psmx_instances.lib_handles[domain->psmx_instances.length] = lib_handle;
        domain->psmx_instances.length++;
      }
      else
      {
        GVERROR ("error loading PSMX instance \"%s\"\n", iface->cfg.name);
        ret = DDS_RETCODE_ERROR;
        break;
      }
      iface = iface->next;
    }

    qsort (domain->psmx_instances.instances, domain->psmx_instances.length, sizeof (*domain->psmx_instances.instances), compare_psmx_prio);
  }
  return ret;
}

dds_return_t dds_pubsub_message_exchange_fini (struct dds_domain *domain)
{
  dds_return_t ret = DDS_RETCODE_OK;
  for (uint32_t i = 0; ret == DDS_RETCODE_OK && i < domain->psmx_instances.length; i++)
  {
    struct dds_psmx *psmx = domain->psmx_instances.instances[i];
    if ((ret = psmx->ops.deinit (psmx)) == DDS_RETCODE_OK)
    {
      (void) ddsrt_dlclose (domain->psmx_instances.lib_handles[i]);
      domain->psmx_instances.instances[i] = NULL;
    }
  }
  return ret;
}

static struct dds_psmx_endpoint * psmx_create_endpoint (struct dds_psmx_topic *psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type)
{
  assert (psmx_topic && psmx_topic->ops.create_endpoint);
  return psmx_topic->ops.create_endpoint (psmx_topic, qos, endpoint_type);
}

static dds_return_t psmx_delete_endpoint (struct dds_psmx_endpoint *psmx_endpoint)
{
  assert (psmx_endpoint && psmx_endpoint->psmx_topic && psmx_endpoint->psmx_topic->ops.delete_endpoint);
  return psmx_endpoint->psmx_topic->ops.delete_endpoint (psmx_endpoint);
}

dds_return_t dds_endpoint_add_psmx_endpoint (struct dds_endpoint *ep, const dds_qos_t *qos, struct dds_psmx_topics_set *psmx_topics, dds_psmx_endpoint_type_t endpoint_type)
{
  ep->psmx_endpoints.length = 0;
  memset (ep->psmx_endpoints.endpoints, 0, sizeof (ep->psmx_endpoints.endpoints));
  for (uint32_t i = 0; psmx_topics != NULL && i < psmx_topics->length; i++)
  {
    struct dds_psmx_topic *psmx_topic = psmx_topics->topics[i];
    if (!dds_qos_has_psmx_instances (qos, psmx_topic->psmx_instance->instance_name))
      continue;
    if (!psmx_topic->psmx_instance->ops.type_qos_supported (psmx_topic->psmx_instance, endpoint_type, psmx_topic->data_type_props, qos))
      continue;
    struct dds_psmx_endpoint *psmx_endpoint = psmx_create_endpoint (psmx_topic, qos, endpoint_type);
    if (psmx_endpoint == NULL)
      goto err;

    ep->psmx_endpoints.endpoints[ep->psmx_endpoints.length++] = psmx_endpoint;
  }
  return DDS_RETCODE_OK;

err:
  dds_endpoint_remove_psmx_endpoints (ep);
  return DDS_RETCODE_ERROR;
}

void dds_endpoint_remove_psmx_endpoints (struct dds_endpoint *ep)
{
  for (uint32_t i = 0; i < ep->psmx_endpoints.length; i++)
  {
    struct dds_psmx_endpoint *psmx_endpoint = ep->psmx_endpoints.endpoints[i];
    if (psmx_endpoint == NULL)
      continue;
    (void) psmx_delete_endpoint (psmx_endpoint);
  }
}

struct ddsi_psmx_locators_set *dds_get_psmx_locators_set (const dds_qos_t *qos, const struct dds_psmx_set *psmx_instances)
{
  struct ddsi_psmx_locators_set *psmx_locators_set = dds_alloc (sizeof (*psmx_locators_set));
  psmx_locators_set->length = 0;
  psmx_locators_set->locators = NULL;

  for (uint32_t s = 0; s < psmx_instances->length; s++)
  {
    if (dds_qos_has_psmx_instances (qos, psmx_instances->instances[s]->instance_name))
    {
      psmx_locators_set->length++;
      psmx_locators_set->locators = dds_realloc (psmx_locators_set->locators, psmx_locators_set->length * sizeof (*psmx_locators_set->locators));
      psmx_locators_set->locators[psmx_locators_set->length - 1] = *(psmx_instances->instances[s]->locator);
    }
  }
  return psmx_locators_set;
}

void dds_psmx_locators_set_free (struct ddsi_psmx_locators_set *psmx_locators_set)
{
  if (psmx_locators_set->length > 0)
    dds_free (psmx_locators_set->locators);
  dds_free (psmx_locators_set);
}

dds_psmx_features_t dds_psmx_supported_features (const struct dds_psmx *psmx_instance)
{
  if (psmx_instance == NULL || psmx_instance->ops.supported_features == NULL)
    return 0u;
  return psmx_instance->ops.supported_features (psmx_instance);
}

static bool endpoint_is_shm (const struct dds_endpoint *endpoint)
{
  bool is_shm_available = false;
  // TODO: implement correct behavior in case of multiple PSMX endpoints
  for (uint32_t i = 0; !is_shm_available && i < endpoint->psmx_endpoints.length; i++)
  {
    struct dds_psmx_endpoint *psmx_endpoint = endpoint->psmx_endpoints.endpoints[i];
    if (psmx_endpoint == NULL)
      continue;
    is_shm_available = dds_psmx_supported_features (psmx_endpoint->psmx_topic->psmx_instance) & DDS_PSMX_FEATURE_SHARED_MEMORY;
  }
  return is_shm_available;
}

bool dds_is_shared_memory_available (const dds_entity_t entity)
{
  bool is_shm_available = false;
  dds_entity *e;

  if (dds_entity_pin (entity, &e) != DDS_RETCODE_OK)
    return false;

  switch (dds_entity_kind (e))
  {
    case DDS_KIND_READER: {
      struct dds_reader const *const rd = (struct dds_reader *) e;
      is_shm_available = endpoint_is_shm (&rd->m_endpoint);
      break;
    }
    case DDS_KIND_WRITER: {
      struct dds_writer const *const wr = (struct dds_writer *)e;
      is_shm_available = endpoint_is_shm (&wr->m_endpoint);
      break;
    }
    default:
      break;
  }

  dds_entity_unpin (e);
  return is_shm_available;
}

static bool endpoint_is_loan_available (const struct dds_endpoint *endpoint)
{
  bool is_loan_available = false;
  // TODO: implement correct behavior in case of multiple PSMX endpoints
  for (uint32_t i = 0; !is_loan_available && i < endpoint->psmx_endpoints.length; i++)
  {
    struct dds_psmx_endpoint *psmx_endpoint = endpoint->psmx_endpoints.endpoints[i];
    if (psmx_endpoint == NULL)
      continue;
    bool is_shm_available = dds_psmx_supported_features (psmx_endpoint->psmx_topic->psmx_instance) & DDS_PSMX_FEATURE_SHARED_MEMORY;
    is_loan_available = is_shm_available && (psmx_endpoint->psmx_topic->data_type_props & DDS_DATA_TYPE_IS_MEMCPY_SAFE);
  }
  return is_loan_available;
}

bool dds_is_loan_available (const dds_entity_t entity)
{
  bool is_loan_available = false;
  dds_entity *e;

  if (dds_entity_pin (entity, &e) != DDS_RETCODE_OK)
    return false;

  switch (dds_entity_kind (e))
  {
    case DDS_KIND_READER: {
      struct dds_reader const *const rd = (struct dds_reader *) e;
      is_loan_available = endpoint_is_loan_available (&rd->m_endpoint);
      break;
    }
    case DDS_KIND_WRITER: {
      struct dds_writer const *const wr = (struct dds_writer *)e;
      is_loan_available = endpoint_is_loan_available (&wr->m_endpoint);
      break;
    }
    default:
      break;
  }

  dds_entity_unpin (e);
  return is_loan_available;
}

dds_return_t dds_request_loan_of_size (dds_entity_t writer, size_t size, void **sample)
{
  dds_entity *e;
  dds_return_t ret = DDS_RETCODE_OK;

  if (dds_entity_pin (writer, &e) != DDS_RETCODE_OK)
    return false;

  if (dds_entity_kind (e) == DDS_KIND_WRITER)
    ret = dds_request_writer_loan ((struct dds_writer *) e, DDS_WRITER_LOAN_RAW, (uint32_t) size, sample);
  else
    ret = DDS_RETCODE_BAD_PARAMETER;

  dds_entity_unpin (e);
  return ret;
}
