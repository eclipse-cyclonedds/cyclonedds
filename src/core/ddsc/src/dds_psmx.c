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
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/strtol.h"
#include "dds/ddsi/ddsi_locator.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/ddsi/ddsi_endpoint.h"
#include "dds/dds.h"
#include "dds__types.h"
#include "dds__psmx.h"
#include "dds__qos.h"
#include "dds__entity.h"
#include "dds__writer.h"
#include "dds__guid.h"

static struct dds_psmx_endpoint_int *psmx_create_endpoint_v0_bincompat_wrapper (const struct dds_psmx_topic_int *psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type);
static struct dds_psmx_endpoint_int *psmx_create_endpoint_v0_wrapper (const struct dds_psmx_topic_int *psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type);
static struct dds_psmx_endpoint_int *psmx_create_endpoint_v1_wrapper (const struct dds_psmx_topic_int *psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type);

static bool get_check_interface_version (enum dds_psmx_interface_version *ifver, const struct dds_psmx_ops *ops)
{
  // We want do distinguish three cases:
  //
  // 1. Interface version 0 using the old memory layout (binary backwards compatibility)
  // 2. Interface version 0 using the new memory layout (source backwards compatibility)
  // 3. Interface version 1 (necessarily using the new memory layout)
  //
  // it would've been nice if the struct had a pointer to a struct with functions, but
  // that ship has sailed if source compatibility is desired ...
  //
  // Distinguishing between (1 or 2) and 3 is easy, just check the create_topic and deinit functions.
  if (ops->create_topic == NULL && ops->deinit == NULL)
  {
    // New mode, then create_topic_with_type and delete_psmx must be defined
    if (ops->create_topic_with_type == NULL || ops->delete_psmx == NULL)
      return false;
    *ifver = DDS_PSMX_INTERFACE_VERSION_1;
    return true;
  }

  // Compatibility mode, then both old functions must be defined and we never touch the new functions
    if (ops->create_topic == NULL || ops->deinit == NULL)
      return false;

  // Distinguishing between 1 vs 2 is trickier, but we get lucky here because interface
  // version 0 requires the plugin to
  // - zero-initialize most fields in struct dds_psmx
  // - to set the instance_name to a dds_alloc'd string
  // and it so happens that the instance_name immediately follows the operations, and
  // on all supported platforms sizeof(void(*)()) == sizeof(void*) and so the offset
  // of create_topic_with_type matches the offset of instance_name
  //
  // This check probably invokes undefined behaviour ...
  DDSRT_STATIC_ASSERT(
    offsetof (struct dds_psmx, ops) == 0 &&
    offsetof (struct dds_psmx_ops, create_topic_with_type) == offsetof (struct dds_psmx_v0, instance_name));
  DDSRT_STATIC_ASSERT (sizeof (ops->create_topic_with_type) == sizeof (char *));
  if (ops->create_topic_with_type == NULL)
    *ifver = DDS_PSMX_INTERFACE_VERSION_0;
  else
    *ifver = DDS_PSMX_INTERFACE_VERSION_0_BINCOMPAT;
  return true;
}

dds_return_t dds_add_psmx_topic_to_list (struct dds_psmx_topic *psmx_topic, void **list)
{
  // deprecated, kept for compatibility with older PSMX Plugin sources
  (void) psmx_topic; (void) list;
  return DDS_RETCODE_OK;
}

dds_return_t dds_remove_psmx_topic_from_list (struct dds_psmx_topic *psmx_topic, void **list)
{
  // deprecated, kept for compatibility with older PSMX Plugin sources
  (void) psmx_topic; (void) list;
  return DDS_RETCODE_OK;
}

dds_return_t dds_add_psmx_endpoint_to_list (struct dds_psmx_endpoint *psmx_endpoint, void **list)
{
  // deprecated, kept for compatibility with older PSMX Plugin sources
  (void) psmx_endpoint; (void) list;
  return DDS_RETCODE_OK;
}

dds_return_t dds_remove_psmx_endpoint_from_list (struct dds_psmx_endpoint *psmx_endpoint, void **list)
{
  // deprecated, kept for compatibility with older PSMX Plugin sources
  (void) psmx_endpoint; (void) list;
  return DDS_RETCODE_OK;
}

dds_return_t dds_psmx_init_generic (struct dds_psmx *psmx)
{
  // checking interface version won't fail (unless things have been
  // tampered with since constructing the psmx instance
  enum dds_psmx_interface_version ifver = DDS_PSMX_INTERFACE_VERSION_0;
  (void) get_check_interface_version (&ifver, &psmx->ops);

  struct ddsi_locator *loc = dds_alloc (sizeof (*loc));
  if (loc == NULL)
    return DDS_RETCODE_OUT_OF_RESOURCES;
  memset (loc, 0, sizeof (*loc));
  dds_psmx_node_identifier_t node_id = psmx->ops.get_node_id (psmx);
  memcpy (loc->address, &node_id, sizeof (node_id));
  loc->kind = DDSI_LOCATOR_KIND_PSMX;

  switch (ifver)
  {
    case DDS_PSMX_INTERFACE_VERSION_0_BINCOMPAT: {
      struct dds_psmx_v0 *psmx_v0 = (struct dds_psmx_v0 *) psmx;
      loc->port = psmx_v0->instance_id;
      psmx_v0->locator = loc;
      return DDS_RETCODE_OK;
    }
    case DDS_PSMX_INTERFACE_VERSION_0: {
      loc->port = psmx->instance_id;
      psmx->locator = loc;
      return DDS_RETCODE_OK;
    }
    case DDS_PSMX_INTERFACE_VERSION_1: {
      break;
    }
  }
  return DDS_RETCODE_ERROR;
}

dds_return_t dds_psmx_cleanup_generic (struct dds_psmx *psmx)
{
  // interface deprecated, kept for compatibility with older PSMX Plugin sources
  // also called by psmx_instance_fini because it actually needs to do the same thing
  // and so it needs to cover all versions

  // checking interface version won't fail (unless things have been
  // tampered with since constructing the psmx instance
  enum dds_psmx_interface_version ifver = DDS_PSMX_INTERFACE_VERSION_0;
  (void) get_check_interface_version (&ifver, &psmx->ops);
  switch (ifver)
  {
    case DDS_PSMX_INTERFACE_VERSION_0_BINCOMPAT: {
      struct dds_psmx_v0 *psmx_v0 = (struct dds_psmx_v0 *) psmx;
      dds_free ((void *) psmx_v0->instance_name);
      dds_free ((void *) psmx_v0->locator);
      break;
    }
    case DDS_PSMX_INTERFACE_VERSION_0:
    case DDS_PSMX_INTERFACE_VERSION_1: {
      dds_free ((void *) psmx->instance_name);
      dds_free ((void *) psmx->locator);
      break;
    }
  }
  return DDS_RETCODE_OK;
}

void dds_psmx_topic_init (struct dds_psmx_topic *psmx_topic, const struct dds_psmx *psmx, const char *topic_name, const char * type_name, dds_data_type_properties_t data_type_props)
{
  psmx_topic->psmx_instance = (struct dds_psmx *) psmx;
  psmx_topic->topic_name = topic_name;
  psmx_topic->type_name = type_name;
  uint32_t topic_hash = ddsrt_mh3 (psmx_topic->topic_name, strlen (psmx_topic->topic_name), 0);
  psmx_topic->data_type = ddsrt_mh3 (&psmx->instance_id, sizeof (psmx->instance_id), topic_hash);
  psmx_topic->data_type_props = data_type_props;
  psmx_topic->psmx_endpoints = NULL;
}

dds_return_t dds_psmx_topic_init_generic (struct dds_psmx_topic *psmx_topic, const dds_psmx_topic_ops_t *ops, const struct dds_psmx * psmx, const char *topic_name, const char * type_name, dds_data_type_properties_t data_type_props)
{
  // deprecated interface version, called by plugin, allocates copies of strings
  psmx_topic->ops = *ops;
  dds_psmx_topic_init (psmx_topic, psmx, dds_string_dup (topic_name), dds_string_dup (type_name), data_type_props);
  return DDS_RETCODE_OK;
}

dds_return_t dds_psmx_topic_cleanup_generic (struct dds_psmx_topic *psmx_topic)
{
  // deprecated interface version, called by plugin, frees the copies allocated by dds_psmx_topic_init_generic
  dds_free ((void *) psmx_topic->type_name);
  dds_free ((void *) psmx_topic->topic_name);
  return DDS_RETCODE_OK;
}

dds_loaned_sample_t *dds_psmx_endpoint_request_loan_v0_bincompat_wrapper (const struct dds_psmx_endpoint_int *psmx_endpoint, uint32_t sz)
{
  struct dds_psmx_endpoint_v0 const * const ext = (const struct dds_psmx_endpoint_v0 *) psmx_endpoint->ext;
  dds_loaned_sample_t *loaned_sample = ext->ops.request_loan (psmx_endpoint->ext, sz);
  if (loaned_sample == NULL)
    return NULL;
  loaned_sample->metadata->sample_state = DDS_LOANED_SAMPLE_STATE_UNITIALIZED;
  loaned_sample->metadata->sample_size = sz;
  loaned_sample->metadata->instance_id = psmx_endpoint->psmx_topic->psmx_instance->instance_id;
  loaned_sample->metadata->data_type = 0;
  return loaned_sample;
}

dds_loaned_sample_t *dds_psmx_endpoint_request_loan_v0_wrapper (const struct dds_psmx_endpoint_int *psmx_endpoint, uint32_t sz)
{
  dds_loaned_sample_t *loaned_sample = psmx_endpoint->ext->ops.request_loan (psmx_endpoint->ext, sz);
  if (loaned_sample == NULL)
    return NULL;
  loaned_sample->metadata->sample_state = DDS_LOANED_SAMPLE_STATE_UNITIALIZED;
  loaned_sample->metadata->sample_size = sz;
  loaned_sample->metadata->instance_id = psmx_endpoint->psmx_topic->psmx_instance->instance_id;
  loaned_sample->metadata->data_type = 0;
  return loaned_sample;
}

dds_loaned_sample_t *dds_psmx_endpoint_request_loan_v1_wrapper (const struct dds_psmx_endpoint_int *psmx_endpoint, uint32_t sz)
{
  dds_loaned_sample_t *loaned_sample = psmx_endpoint->ext->ops.request_loan (psmx_endpoint->ext, sz);
  if (loaned_sample == NULL)
    return NULL;
  loaned_sample->loan_origin.origin_kind = DDS_LOAN_ORIGIN_KIND_PSMX;
  loaned_sample->loan_origin.psmx_endpoint = psmx_endpoint->ext;
  loaned_sample->metadata->sample_state = DDS_LOANED_SAMPLE_STATE_UNITIALIZED;
  loaned_sample->metadata->sample_size = sz;
  loaned_sample->metadata->instance_id = psmx_endpoint->psmx_topic->psmx_instance->instance_id;
  loaned_sample->metadata->data_type = 0;
  ddsrt_atomic_st32 (&loaned_sample->refc, 1);
  return loaned_sample;
}

static dds_psmx_instance_id_t get_psmx_instance_id (const struct ddsi_domaingv * gv, const char *config_name)
{
  uint32_t ext_domainid = gv->config.extDomainId.value;
  uint32_t hashed_id = ddsrt_mh3 (&ext_domainid, sizeof (ext_domainid), 0x0);
  return ddsrt_mh3 (config_name, strlen (config_name), hashed_id);
}

static bool check_config_type_name (const char *config_name)
{
  const char *c = config_name;
  while (*c)
  {
    if (*c == ';') {
      return false;
    } else if (*c == '\\') {
      if (*(c + 1) == '\0')
        return false;
      c += 2;
    } else {
      c += 1;
    }
  }
  return true;
}

char *dds_pubsub_message_exchange_configstr (const char *config, const char *config_name)
{
  // Result is a \0-separated sequence of non-empty config strings, with only the first
  // such string possibly empty
  static const char INSTANCE_NAME[] = "INSTANCE_NAME";
  static const size_t INSTANCE_NAME_len = sizeof (INSTANCE_NAME) - 1;
  // SERVICE_NAME: backwards compatibility, synonym for instance name
  static const char SERVICE_NAME[] = "SERVICE_NAME";
  static const size_t SERVICE_NAME_len = sizeof (SERVICE_NAME) - 1;
  // Check syntax: only KEY=VALUE pairs separated by ;, with backslash an escape character
  //
  // We make no assumptions on the names of the keys or their values, except that no keys
  // may have CYCLONEDDS_ as a prefix, contain an escape character or an equals sign.
  //
  // We also add INSTANCE_NAME=name if no INSTANCE_NAME is present in config (so the
  // plug-in can rely on it being present), where name is taken from SERVICE_NAME (if
  // present) or else from config_name.  For backwards compatibility (old plugins might
  // reject config strings that contain unexpected keys), we add it immediately following
  // the first \0.  So the result will look like:
  //
  //   A=B;C=D;E=F;\0INSTANCE_NAME=G;\0\0
  //
  // in memory.  Possibly using config_name for this means we also have to require that
  // config_name is a valid value.  The caller is expected to guarantee this.
  const char *kstart = config; // init to pacify compiler
  enum { START, KEY0, KEY, VALUE_NORM, VALUE_ESCAPED } cs = START;
  const char *instance_name = NULL;
  const char *service_name_end = NULL;
  const char *service_name = NULL;
  bool in_service_name = false;
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
        if (*c == '=') {
          // key may not have CYCLONEDDS_ as prefix
          // we need to know if SERVICE_NAME is present so we can add it if not
          cs = VALUE_NORM;
          if (c - kstart >= 11 && memcmp (kstart, "CYCLONEDDS_", 11) == 0)
            goto malformed;
          else if ((size_t) (c - kstart) >= INSTANCE_NAME_len && memcmp (kstart, INSTANCE_NAME, INSTANCE_NAME_len) == 0)
          {
            // not accepting multiple INSTANCE_NAME/SERVICE_NAME in the config string
            if (service_name || instance_name)
              goto malformed;
            instance_name = kstart;
          }
          else if ((size_t) (c - kstart) >= SERVICE_NAME_len && memcmp (kstart, SERVICE_NAME, SERVICE_NAME_len) == 0)
          {
            if (service_name || instance_name)
              goto malformed;
            service_name = kstart;
            in_service_name = true;
          }
        }
        break;
      case VALUE_NORM: // non-escaped characters in value
        if (*c == ';' || *c == '\0') { // ; -> next key (end of string same)
          if (in_service_name)
            service_name_end = c;
          in_service_name = false;
          cs = KEY0;
        } else if (*c == '\\') { // escape next character
          cs = VALUE_ESCAPED;
        }
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
      if (in_service_name) {
        assert (service_name);
        service_name_end = service_name + strlen (service_name);
      }
      break;
    default:
      goto malformed;
  }
  // empty instance name is forbidden (instance_name is set iff the following character is '=',
  // so we can inspect the character beyond it)
  if (instance_name && (instance_name[INSTANCE_NAME_len + 1] == ';' || instance_name[INSTANCE_NAME_len + 1] == '\0'))
    goto malformed;
  if (service_name && (service_name[SERVICE_NAME_len + 1] == ';' || service_name[SERVICE_NAME_len + 1] == '\0'))
    goto malformed;

  // no instance name present? then add it, taking the value from service_name if present
  // config name if not
  const char *append_name = NULL;
  size_t append_name_len = 0;
  if (instance_name != NULL)
    ; // nothing to append
  else if (service_name == NULL)
  {
    append_name = config_name;
    append_name_len = strlen (config_name);
  }
  else
  {
    assert (service_name_end && service_name_end >= service_name + SERVICE_NAME_len + 1);
    append_name = service_name + SERVICE_NAME_len + 1;
    append_name_len = (size_t) (service_name_end - append_name);
  }

  const size_t input_len = strlen (config);
  size_t configsz = input_len + 2; /* this string ends at \0\0 */
  // only require a ; if input is non-empty; this is why the first config string can be
  // end up empty
  const bool append_semicolon = (cs == VALUE_NORM);
  if (append_semicolon)
    configsz += 1;
  if (instance_name == NULL)
    configsz += INSTANCE_NAME_len + 1 + append_name_len + 2;

  char *configstr = NULL;
  size_t pos = 0;
  if ((configstr = ddsrt_malloc (configsz)) == NULL)
    return NULL;
  memcpy (&configstr[pos], config, input_len);
  pos += input_len;
  if (append_semicolon)
    configstr[pos++] = ';';
  configstr[pos++] = '\0';
  if (append_name != NULL)
  {
    memcpy (&configstr[pos], INSTANCE_NAME, INSTANCE_NAME_len);
    pos += INSTANCE_NAME_len;
    configstr[pos++] = '=';
    memcpy (&configstr[pos], append_name, append_name_len);
    pos += append_name_len;
    configstr[pos++] = ';';
    configstr[pos++] = '\0';
  }
  configstr[pos++] = '\0';
  assert (pos == configsz);
  return configstr;

malformed:
  return NULL;
}

static size_t value_size (const char *value)
{
  const char *v = value;
  size_t n = 0;
  while (*v)
  {
    n++;
    if (*v == ';') {
      return n;
    } else if (*v == '\\') {
      assert (*(v + 1) != '\0');
      v += 2;
    } else {
      v += 1;
    }
  }
  return n + 1;
}

static char *extract_value (const char *value)
{
  size_t n = value_size (value);
  const char *s = value;
  char *dst = ddsrt_malloc (n), *d = dst;
  if (dst == NULL)
    return NULL;
  while (--n)
  {
    if (*s == '\\')
      s++;
    *d++ = *s++;
  }
  *d = 0;
  return dst;
}

static char *get_config_option_value_1 (const char *config, const char *option, size_t option_len, const char **endp)
{
  // *endp points to the first \0 encountered if the option was not found
  // *endp points to a '=' if a lack of available memory caused a null pointer return
  // we are strict about config string syntax during initialization, so now we can relax
  const char *kstart = config; // init to pacify compiler
  enum { KEY0, KEY, VALUE_NORM, VALUE_ESCAPED } cs = KEY0;
  const char *cursor;
  for (cursor = config; *cursor; cursor++) {
    switch (cs) {
      case KEY0:
        kstart = cursor;
        cs = KEY;
        // falls through
      case KEY:
        if (*cursor == '=') {
          cs = VALUE_NORM;
          if ((size_t) (cursor - kstart) == option_len && memcmp (kstart, option, option_len) == 0) {
            *endp = cursor; // keep the promised NULL return in case of an out-of-memory condition
            return extract_value (cursor + 1);
          }
        }
        break;
      case VALUE_NORM:
        if (*cursor == ';') cs = KEY0;
        else if (*cursor == '\\') cs = VALUE_ESCAPED;
        break;
      case VALUE_ESCAPED:
        cs = VALUE_NORM;
        break;
    }
  }
  *endp = cursor;
  return NULL;
}

char *dds_psmx_get_config_option_value (const char *config, const char *option)
{
  if (config == NULL || option == NULL || *option == '\0')
    return NULL;
  if (strcmp (option, "SERVICE_NAME") == 0)
    option = "INSTANCE_NAME";
  const size_t option_len = strlen (option);
  do {
    const char *endp;
    char *v = get_config_option_value_1 (config, option, option_len, &endp);
    if (v != NULL)
      return v;
    if (endp[0] != '\0') // out-of-memory
      return NULL;
    if (endp[1] == '\0') // \0\0 -> end of input
      return NULL;
    config = endp + 1;
  } while (*config);
  return NULL;
}

static dds_return_t node_id_from_string (dds_psmx_node_identifier_t *nodeid, const char *lstr)
{
  if (strlen (lstr) != 32)
    return DDS_RETCODE_BAD_PARAMETER;
  unsigned char * const dst = (unsigned char *) nodeid->x;
  for (uint32_t n = 0; n < 32 && lstr[n]; n++)
  {
    int32_t num;
    if ((num = ddsrt_todigit (lstr[n])) < 0 || num >= 16)
      return DDS_RETCODE_BAD_PARAMETER;
    if ((n % 2) == 0)
      dst[n / 2] = (uint8_t) (num << 4);
    else
      dst[n / 2] |= (uint8_t) num;
  }
  return DDS_RETCODE_OK;
}

static dds_return_t psmx_instance_init (struct dds_psmx *psmx, dds_psmx_instance_id_t id, const char *name, int32_t priority, const char *locstr)
{
  // ops: set by plugin
  // topics: reserved
  // everything else: to be initialized
  dds_return_t ret = DDS_RETCODE_ERROR;
  psmx->instance_name = dds_string_dup (name);
  if (psmx->instance_name == NULL)
  {
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto err_instance_name;
  }
  psmx->priority = priority;
  struct ddsi_locator *loc = dds_alloc (sizeof (*loc));
  if (loc == NULL) {
    ret = DDS_RETCODE_OUT_OF_RESOURCES;
    goto err_locator;
  }
  memset (loc, 0, sizeof (*loc));
  if (locstr == NULL)
  {
    dds_psmx_node_identifier_t node_id = psmx->ops.get_node_id (psmx);
    memcpy (loc->address, &node_id, sizeof (node_id));
  }
  else
  {
    dds_psmx_node_identifier_t node_id;
    if ((ret = node_id_from_string (&node_id, locstr)) != DDS_RETCODE_OK)
      goto err_locator_conversion;
    memcpy (loc->address, &node_id, sizeof (node_id));
  }
  loc->port = psmx->instance_id;
  loc->kind = DDSI_LOCATOR_KIND_PSMX;
  psmx->locator = loc;
  psmx->instance_id = id;
  psmx->psmx_topics = NULL;
  return DDS_RETCODE_OK;

err_locator_conversion:
  dds_free ((void *) psmx->locator);
err_locator:
  dds_free ((void *) psmx->instance_name);
err_instance_name:
  psmx->instance_name = NULL;
  psmx->locator = NULL;
  return ret;
}

static struct dds_psmx_topic_int *psmx_create_topic_v0_bincompat_wrapper (struct dds_psmx_int *psmx, struct dds_ktopic *ktp, struct ddsi_sertype *sertype, struct ddsi_type *type)
{
  (void) type;
  struct dds_psmx_topic * const topic = psmx->ext->ops.create_topic (psmx->ext, ktp->name, sertype->type_name, sertype->data_type_props);
  if (topic == NULL)
    return NULL;
  struct dds_psmx_topic_int *x = ddsrt_malloc (sizeof (*x));
  x->ops.create_endpoint = psmx_create_endpoint_v0_bincompat_wrapper;
  x->ops.delete_endpoint = topic->ops.delete_endpoint;
  x->ext = topic;
  x->data_type_props = sertype->data_type_props;
  x->psmx_instance = psmx;
  return x;
}

static struct dds_psmx_topic_int *psmx_create_topic_v0_wrapper (struct dds_psmx_int *psmx, struct dds_ktopic *ktp, struct ddsi_sertype *sertype, struct ddsi_type *type)
{
  (void) type;
  struct dds_psmx_topic * const topic = psmx->ext->ops.create_topic (psmx->ext, ktp->name, sertype->type_name, sertype->data_type_props);
  if (topic == NULL)
    return NULL;
  struct dds_psmx_topic_int *x = ddsrt_malloc (sizeof (*x));
  x->ops.create_endpoint = psmx_create_endpoint_v0_wrapper;
  x->ops.delete_endpoint = topic->ops.delete_endpoint;
  x->ext = topic;
  x->data_type_props = sertype->data_type_props;
  x->psmx_instance = psmx;
  return x;
}

static struct dds_psmx_topic_int *psmx_create_topic_v1_wrapper (struct dds_psmx_int *psmx, struct dds_ktopic *ktp, struct ddsi_sertype *sertype, struct ddsi_type *type)
{
  struct dds_psmx_topic * const topic = psmx->ext->ops.create_topic_with_type (psmx->ext, ktp->name, sertype->type_name, sertype->data_type_props, type, sertype->sizeof_type);
  if (topic == NULL)
    return NULL;
  // plugin is only allowed to initialize the ops in the new interface; do a sanity check
  if (topic->psmx_instance || topic->topic_name || topic->type_name)
  {
    (void) psmx->ext->ops.delete_topic (topic);
    return NULL;
  }

  dds_psmx_topic_init (topic, psmx->ext, ktp->name, sertype->type_name, sertype->data_type_props);
  struct dds_psmx_topic_int *x = ddsrt_malloc (sizeof (*x));
  x->ops.create_endpoint = psmx_create_endpoint_v1_wrapper;
  x->ops.delete_endpoint = topic->ops.delete_endpoint;
  x->ext = topic;
  x->data_type_props = sertype->data_type_props;
  x->psmx_instance = psmx;
  return x;
}

static void psmx_delete_topic_wrapper (struct dds_psmx_topic_int *topic)
{
  (void) topic->psmx_instance->ext->ops.delete_topic (topic->ext);
  ddsrt_free (topic);
}

static void psmx_delete_psmx_v0_bincompat_wrapper (struct dds_psmx_int *psmx)
{
  psmx->ext->ops.deinit (psmx->ext);
  ddsrt_free (psmx);
}

static void psmx_delete_psmx_v0_wrapper (struct dds_psmx_int *psmx)
{
  psmx->ext->ops.deinit (psmx->ext);
  ddsrt_free (psmx);
}

static void psmx_delete_psmx_v1_wrapper (struct dds_psmx_int *psmx)
{
  (void) dds_psmx_cleanup_generic (psmx->ext);
  psmx->ext->instance_name = NULL;
  psmx->ext->locator = NULL;
  psmx->ext->ops.delete_psmx (psmx->ext);
  ddsrt_free (psmx);
}

static struct dds_psmx_int *new_psmx_int (struct dds_psmx *ext, enum dds_psmx_interface_version ifver)
{
  struct dds_psmx_int *x = ddsrt_malloc (sizeof (*x));
  x->ops.type_qos_supported = ext->ops.type_qos_supported;
  x->ops.delete_topic = psmx_delete_topic_wrapper;
  x->ops.get_node_id = ext->ops.get_node_id;
  x->ops.supported_features = ext->ops.supported_features;
  x->ext = ext;
  switch (ifver)
  {
    case DDS_PSMX_INTERFACE_VERSION_0_BINCOMPAT: {
      struct dds_psmx_v0 const * const ext_v0 = (const struct dds_psmx_v0 *) ext;
      x->ops.create_topic_with_type = psmx_create_topic_v0_bincompat_wrapper;
      x->ops.delete_psmx = psmx_delete_psmx_v0_bincompat_wrapper;
      x->instance_id = ext_v0->instance_id;
      x->instance_name = ext_v0->instance_name;
      x->locator = *ext_v0->locator;
      x->priority = ext_v0->priority;
      break;
    }
    case DDS_PSMX_INTERFACE_VERSION_0: {
      x->ops.create_topic_with_type = psmx_create_topic_v0_wrapper;
      x->ops.delete_psmx = psmx_delete_psmx_v0_wrapper;
      x->instance_id = ext->instance_id;
      x->instance_name = ext->instance_name;
      x->locator = *ext->locator;
      x->priority = ext->priority;
      break;
    }
    case DDS_PSMX_INTERFACE_VERSION_1: {
      x->ops.create_topic_with_type = psmx_create_topic_v1_wrapper;
      x->ops.delete_psmx = psmx_delete_psmx_v1_wrapper;
      x->instance_id = ext->instance_id;
      x->instance_name = ext->instance_name;
      x->locator = *ext->locator;
      x->priority = ext->priority;
      break;
    }
  }
  return x;
}

static dds_return_t psmx_instance_load (const struct ddsi_domaingv *gv, const struct ddsi_config_psmx *config, struct dds_psmx_int **out, ddsrt_dynlib_t *lib_handle, enum dds_psmx_interface_version *interface_version)
{
  dds_psmx_create_fn creator = NULL;
  char *lib_name;
  ddsrt_dynlib_t handle;
  char load_fn[100];
  dds_return_t ret = DDS_RETCODE_ERROR;
  struct dds_psmx *psmx_ext = NULL;

  if (config->library && config->library[0] != '\0')
    lib_name = config->library;
  else
    ddsrt_asprintf (&lib_name, "psmx_%s", config->type);

  if (!check_config_type_name (config->type))
  {
    GVERROR ("Configuration for PSMX instance '%s' has invalid type\n", config->type);
    goto err_configstr;
  }

  char *configstr;
  if ((configstr = dds_pubsub_message_exchange_configstr (config->config, config->type)) == NULL)
  {
    GVERROR ("Configuration for PSMX instance '%s' is invalid\n", config->type);
    goto err_configstr;
  }

  if ((ret = ddsrt_dlopen (lib_name, true, &handle)) != DDS_RETCODE_OK)
  {
    char buf[1024];
    (void) ddsrt_dlerror (buf, sizeof(buf));
    GVERROR ("Failed to load PSMX library '%s' with error \"%s\".\n", lib_name, buf);
    goto err_dlopen;
  }

  (void) snprintf (load_fn, sizeof (load_fn), "%s_create_psmx", config->type);
  if ((ret = ddsrt_dlsym (handle, load_fn, (void **) &creator)) != DDS_RETCODE_OK)
  {
    GVERROR ("Failed to initialize PSMX instance '%s', could not load init function '%s'.\n", config->type, load_fn);
    goto err_dlsym;
  }

  char * const psmx_name = dds_psmx_get_config_option_value (configstr, "INSTANCE_NAME");
  assert (psmx_name); // INSTANCE_NAME's presence is guaranteed by dds_pubsub_message_exchange_configstr
  const dds_psmx_instance_id_t id = get_psmx_instance_id (gv, psmx_name);
  if ((ret = creator (&psmx_ext, id, configstr)) != DDS_RETCODE_OK)
  {
    GVERROR ("Failed to initialize PSMX instance '%s'.\n", psmx_name);
    goto err_init;
  }
  enum dds_psmx_interface_version ifver;
  if (!get_check_interface_version (&ifver, &psmx_ext->ops))
  {
    GVERROR ("Failed to initialize PSMX instance '%s', can't determine interface version.\n", psmx_name);
    ret = DDS_RETCODE_ERROR;
    goto err_ifver;
  }

  struct dds_psmx_int *psmx_int = NULL;
  switch (ifver)
  {
    case DDS_PSMX_INTERFACE_VERSION_0_BINCOMPAT: {
      struct dds_psmx_v0 * const psmx_ext_v0 = (struct dds_psmx_v0 *) psmx_ext;
      // fields should have been set by "creator", either directly or by dds_psmx_init_generic
      // might as well do a sanity check
      if (psmx_ext_v0->instance_id == 0 || psmx_ext_v0->instance_name == NULL)
      {
        GVERROR ("Failed to initialize PSMX instance '%s', missing initializations.\n", psmx_name);
        goto err_ifver;
      }
      psmx_ext_v0->priority = config->priority.value;
      psmx_int = new_psmx_int (psmx_ext, ifver);
      break;
    }
    case DDS_PSMX_INTERFACE_VERSION_0: {
      // fields should have been set by "creator", either directly or by dds_psmx_init_generic
      // might as well do a sanity check
      if (psmx_ext->instance_id == 0 || psmx_ext->instance_name == NULL)
      {
        GVERROR ("Failed to initialize PSMX instance '%s', missing initializations.\n", psmx_name);
        goto err_ifver;
      }
      psmx_ext->priority = config->priority.value;
      psmx_int = new_psmx_int (psmx_ext, ifver);
      break;
    }
    case DDS_PSMX_INTERFACE_VERSION_1: {
      // fields should not have been set by "creator"
      if (psmx_ext->instance_id != 0 || psmx_ext->instance_name != NULL)
      {
        GVERROR ("Failed to initialize PSMX instance '%s', fields initialized that should not have been.\n", psmx_name);
        goto err_ifver;
      }
      char *locstr = dds_psmx_get_config_option_value (configstr, "LOCATOR");
      ret = psmx_instance_init (psmx_ext, id, psmx_name, config->priority.value, locstr);
      ddsrt_free (locstr);
      if (ret != DDS_RETCODE_OK)
      {
        GVERROR ("Failed to initialize PSMX instance '%s', out of resources.\n", psmx_name);
        goto err_ifver;
      }
      psmx_int = new_psmx_int (psmx_ext, ifver);
      break;
    }
  }

  *out = psmx_int;
  *interface_version = ifver;
  *lib_handle = handle;
  ddsrt_free (psmx_name);
  ddsrt_free (configstr);
  if (lib_name != config->library)
    ddsrt_free (lib_name);
  return DDS_RETCODE_OK;

err_ifver:
  if (psmx_ext->ops.deinit)
    psmx_ext->ops.deinit (psmx_ext);
  // might expect: else psmx_instance->ops.delete_psmx (psmx_instance)
  // but we don't know if it exists ... leaking a little bit is better
  // than calling garbage
err_init:
  ddsrt_free (psmx_name);
err_dlsym:
  ddsrt_dlclose (handle);
err_dlopen:
  ddsrt_free (configstr);
err_configstr:
  if (lib_name != config->library)
    ddsrt_free (lib_name);
  return ret;
}

static int qsort_cmp_psmx_prio_descending (const void *va, const void *vb)
{
  const struct dds_psmx_set_elem *a = va;
  const struct dds_psmx_set_elem *b = vb;
  const int32_t aprio = a->instance->priority;
  const int32_t bprio = b->instance->priority;
  return (int)(aprio < bprio) - (int)(aprio > bprio);
}

dds_return_t dds_pubsub_message_exchange_init (const struct ddsi_domaingv *gv, struct dds_domain *domain)
{
  struct ddsi_config_psmx_listelem *iface = gv->config.psmx_instances;
  while (iface != NULL)
  {
    if (domain->psmx_instances.length >= DDS_MAX_PSMX_INSTANCES)
    {
      GVERROR("error loading PSMX instance, at most %d simultaneous instances supported\n", DDS_MAX_PSMX_INSTANCES);
      return DDS_RETCODE_UNSUPPORTED;
    }

    GVLOG(DDS_LC_INFO, "Loading PSMX instance %s\n", iface->cfg.type);
    struct dds_psmx_int *psmx = NULL;
    ddsrt_dynlib_t lib_handle;
    enum dds_psmx_interface_version ifver;
    if (psmx_instance_load (gv, &iface->cfg, &psmx, &lib_handle, &ifver) == DDS_RETCODE_OK)
    {
      domain->psmx_instances.elems[domain->psmx_instances.length++] = (struct dds_psmx_set_elem){
        .instance = psmx,
        .interface_version = ifver,
        .lib_handle = lib_handle
      };
    }
    else
    {
      GVERROR ("error loading PSMX instance \"%s\"\n", iface->cfg.type);
      return DDS_RETCODE_ERROR;
    }
    iface = iface->next;
  }
  qsort (domain->psmx_instances.elems, domain->psmx_instances.length, sizeof (*domain->psmx_instances.elems), qsort_cmp_psmx_prio_descending);
  return DDS_RETCODE_OK;
}

void dds_pubsub_message_exchange_fini (struct dds_domain *domain)
{
  for (uint32_t i = 0; i < domain->psmx_instances.length; i++)
  {
    struct dds_psmx_int *psmx = domain->psmx_instances.elems[i].instance;
    psmx->ops.delete_psmx (psmx);
    (void) ddsrt_dlclose (domain->psmx_instances.elems[i].lib_handle);
  }
  domain->psmx_instances.length = 0;
}

static dds_return_t dds_psmx_endpoint_write_wrapper (const struct dds_psmx_endpoint_int *psmx_endpoint, dds_loaned_sample_t *data, size_t keysz, const void *key)
{
  (void) keysz; (void) key;
  // FreeRTOS #defines "write" ...
  return (psmx_endpoint->ext->ops.write) (psmx_endpoint->ext, data);
}

static dds_return_t dds_psmx_endpoint_write_with_key_wrapper (const struct dds_psmx_endpoint_int *psmx_endpoint, dds_loaned_sample_t *data, size_t keysz, const void *key)
{
  return psmx_endpoint->ext->ops.write_with_key (psmx_endpoint->ext, data, keysz, key);
}

static struct dds_psmx_endpoint_int *new_psmx_endpoint_int (struct dds_psmx_endpoint *ep, dds_psmx_endpoint_type_t endpoint_type, const struct dds_psmx_topic_int *topic, dds_psmx_endpoint_request_loan_int_fn request_loan, dds_psmx_endpoint_write_with_key_int_fn write_with_key)
{
  struct dds_psmx_endpoint_int *x = ddsrt_malloc (sizeof (*x));
  x->ops.on_data_available = ep->ops.on_data_available;
  x->ops.request_loan = request_loan;
  x->ops.take = ep->ops.take;
  x->ops.write_with_key = write_with_key;
  x->ext = ep;
  x->endpoint_type = endpoint_type;
  x->psmx_topic = topic;
  x->wants_key = (write_with_key == dds_psmx_endpoint_write_with_key_wrapper);
  return x;
}

static struct dds_psmx_endpoint_int *psmx_create_endpoint_v0_bincompat_wrapper (const struct dds_psmx_topic_int *psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type)
{
  assert (psmx_topic && psmx_topic->ops.create_endpoint);
  struct dds_psmx_endpoint * const ep_ext = psmx_topic->ext->ops.create_endpoint (psmx_topic->ext, qos, endpoint_type);
  if (ep_ext == NULL)
    return NULL;
  return new_psmx_endpoint_int (ep_ext, endpoint_type, psmx_topic, dds_psmx_endpoint_request_loan_v0_bincompat_wrapper, dds_psmx_endpoint_write_wrapper);
}

static struct dds_psmx_endpoint_int *psmx_create_endpoint_v0_wrapper (const struct dds_psmx_topic_int *psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type)
{
  struct dds_psmx_endpoint * const ep_ext = psmx_topic->ext->ops.create_endpoint (psmx_topic->ext, qos, endpoint_type);
  if (ep_ext == NULL)
    return NULL;
  return new_psmx_endpoint_int (ep_ext, endpoint_type, psmx_topic, dds_psmx_endpoint_request_loan_v0_wrapper, dds_psmx_endpoint_write_wrapper);
}

static struct dds_psmx_endpoint_int *psmx_create_endpoint_v1_wrapper (const struct dds_psmx_topic_int *psmx_topic, const struct dds_qos *qos, dds_psmx_endpoint_type_t endpoint_type)
{
  struct dds_psmx_endpoint * const ep_ext = psmx_topic->ext->ops.create_endpoint (psmx_topic->ext, qos, endpoint_type);
  if (ep_ext == NULL)
    return NULL;
  ep_ext->endpoint_type = endpoint_type;
  ep_ext->psmx_topic = psmx_topic->ext;
  return new_psmx_endpoint_int (ep_ext, endpoint_type, psmx_topic, dds_psmx_endpoint_request_loan_v1_wrapper, ep_ext->ops.write_with_key ? dds_psmx_endpoint_write_with_key_wrapper : dds_psmx_endpoint_write_wrapper);
}

static void psmx_delete_endpoint (struct dds_psmx_endpoint_int *psmx_endpoint)
{
  assert (psmx_endpoint && psmx_endpoint->psmx_topic && psmx_endpoint->psmx_topic->ops.delete_endpoint);
  (void) psmx_endpoint->psmx_topic->ops.delete_endpoint (psmx_endpoint->ext);
  ddsrt_free (psmx_endpoint);
}

dds_return_t dds_endpoint_add_psmx_endpoint (struct dds_endpoint *ep, const dds_qos_t *qos, struct dds_psmx_topics_set *psmx_topics, dds_psmx_endpoint_type_t endpoint_type)
{
  ep->psmx_endpoints.length = 0;
  for (uint32_t i = 0; i < psmx_topics->length; i++)
  {
    struct dds_psmx_topic_int const * const psmx_topic = psmx_topics->topics[i];
    struct dds_psmx_int const * const psmx = psmx_topic->psmx_instance;
    if (!dds_qos_has_psmx_instances (qos, psmx->instance_name))
      continue;
    if (!psmx->ops.type_qos_supported (psmx_topic->psmx_instance->ext, endpoint_type, psmx_topic->data_type_props, qos))
      continue;
    struct dds_psmx_endpoint_int *psmx_endpoint = psmx_topic->ops.create_endpoint (psmx_topic, qos, endpoint_type);
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
    struct dds_psmx_endpoint_int *psmx_endpoint = ep->psmx_endpoints.endpoints[i];
    psmx_delete_endpoint (psmx_endpoint);
  }
}

struct ddsi_psmx_locators_set *dds_get_psmx_locators_set (const dds_qos_t *qos, const struct dds_psmx_set *psmx_instances)
{
  struct ddsi_psmx_locators_set *psmx_locators_set = dds_alloc (sizeof (*psmx_locators_set));
  psmx_locators_set->length = 0;
  psmx_locators_set->locators = NULL;

  for (uint32_t s = 0; s < psmx_instances->length; s++)
  {
    if (dds_qos_has_psmx_instances (qos, psmx_instances->elems[s].instance->instance_name))
    {
      psmx_locators_set->length++;
      psmx_locators_set->locators = dds_realloc (psmx_locators_set->locators, psmx_locators_set->length * sizeof (*psmx_locators_set->locators));
      psmx_locators_set->locators[psmx_locators_set->length - 1] = psmx_instances->elems[s].instance->locator;
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

static dds_psmx_features_t dds_psmx_supported_features (const struct dds_psmx_int *psmx_instance)
{
  if (psmx_instance == NULL || psmx_instance->ops.supported_features == NULL)
    return 0u;
  return psmx_instance->ops.supported_features (psmx_instance->ext);
}

static bool endpoint_is_shm (const struct dds_endpoint *endpoint)
{
  for (uint32_t i = 0; i < endpoint->psmx_endpoints.length; i++)
  {
    struct dds_psmx_endpoint_int *psmx_endpoint = endpoint->psmx_endpoints.endpoints[i];
    if (dds_psmx_supported_features (psmx_endpoint->psmx_topic->psmx_instance) & DDS_PSMX_FEATURE_SHARED_MEMORY)
      return true;
  }
  return false;
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
    struct dds_psmx_endpoint_int *psmx_endpoint = endpoint->psmx_endpoints.endpoints[i];
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

  if ((ret = dds_entity_pin (writer, &e)) != DDS_RETCODE_OK)
    return ret;

  if (dds_entity_kind (e) == DDS_KIND_WRITER)
    ret = dds_request_writer_loan ((struct dds_writer *) e, DDS_WRITER_LOAN_RAW, (uint32_t) size, sample);
  else
    ret = DDS_RETCODE_BAD_PARAMETER;

  dds_entity_unpin (e);
  return ret;
}

void dds_psmx_set_loan_writeinfo (struct dds_loaned_sample *loan, const ddsi_guid_t *wr_guid, dds_time_t timestamp, uint32_t statusinfo)
{
  assert (loan->metadata->sample_state != DDS_LOANED_SAMPLE_STATE_UNITIALIZED);
  struct dds_psmx_metadata *md = loan->metadata;
  md->guid = dds_guid_from_ddsi_guid (*wr_guid);
  md->timestamp = timestamp;
  md->statusinfo = statusinfo;
}
