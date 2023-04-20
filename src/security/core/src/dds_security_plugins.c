// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <string.h>
#include <assert.h>

#include "dds/security/core/dds_security_plugins.h"
#include "dds/ddsrt/log.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/dynlib.h"
#include "dds/ddsrt/io.h"

static bool check_plugin_configuration (const dds_security_plugin_config *config, const char *name, struct ddsi_domaingv *gv)
{
  if (config->library_path == NULL || *config->library_path == 0) {
    GVERROR ("%s security plugin library path is undefined or empty\n", name);
    return false;
  }
  if (config->library_init == NULL || *config->library_init == 0) {
    GVERROR ("%s security plugin init function is undefined or empty\n", name);
    return false;
  }
  if (config->library_finalize == NULL || *config->library_finalize == 0) {
    GVERROR ("%s security plugin finalize function is undefined or empty\n", name);
    return false;
  }
  return true;
}

dds_return_t dds_security_check_plugin_configuration (const dds_security_plugin_suite_config *security_suite_config, struct ddsi_domaingv *gv)
{
  if (check_plugin_configuration (&security_suite_config->access_control, "AccessControl", gv) &&
      check_plugin_configuration (&security_suite_config->authentication, "Authentication", gv) &&
      check_plugin_configuration (&security_suite_config->cryptography, "Cryptography", gv))
    return DDS_RETCODE_OK;
  else
    return DDS_RETCODE_ERROR;
}

static bool verify_function (const void *function_ptr, dds_security_plugin *plugin, const char *function_name, struct ddsi_domaingv *gv)
{
  if (function_ptr != NULL)
    return true;
  else
  {
    GVERROR ("Could not find the function for %s: %s\n", plugin->name, function_name);
    return false;
  }
}

struct verify_plugin_functions_tab {
  size_t off;
  const char *name;
};

static bool verify_plugin_functions (const void *context, dds_security_plugin *plugin, const struct verify_plugin_functions_tab *entries, size_t nentries, struct ddsi_domaingv *gv)
{
  for (size_t i = 0; i < nentries; i++)
  {
    const char *p = (const char *) context + entries[i].off;
    if (!verify_function (*((void **) p), plugin, entries[i].name, gv))
      return false;
  }
  return true;
}

dds_return_t dds_security_verify_plugin_functions(
    dds_security_authentication *authentication_context, dds_security_plugin *auth_plugin,
    dds_security_cryptography *crypto_context, dds_security_plugin *crypto_plugin,
    dds_security_access_control *access_control_context, dds_security_plugin *ac_plugin,
    struct ddsi_domaingv *gv)
{
#define FGEN(context, name) { offsetof (context, name), #name }
#define F(name) FGEN (dds_security_authentication, name)
  static const struct verify_plugin_functions_tab auth[] = {
    F (validate_local_identity),
    F (get_identity_token),
    F (get_identity_status_token),
    F (set_permissions_credential_and_token),
    F (validate_remote_identity),
    F (begin_handshake_request),
    F (begin_handshake_reply),
    F (process_handshake),
    F (get_shared_secret),
    F (get_authenticated_peer_credential_token),
    F (set_listener),
    F (return_identity_token),
    F (return_identity_status_token),
    F (return_authenticated_peer_credential_token),
    F (return_handshake_handle),
    F (return_sharedsecret_handle)
  };
#undef F
#define F(name) FGEN (dds_security_access_control, name)
  static const struct verify_plugin_functions_tab ac[] = {
    F (validate_local_permissions),
    F (validate_remote_permissions),
    F (check_create_participant),
    F (check_create_datawriter),
    F (check_create_datareader),
    F (check_create_topic),
    F (check_local_datawriter_register_instance),
    F (check_local_datawriter_dispose_instance),
    F (check_remote_participant),
    F (check_remote_datawriter),
    F (check_remote_datareader),
    F (check_remote_topic),
    F (check_local_datawriter_match),
    F (check_local_datareader_match),
    F (check_remote_datawriter_register_instance),
    F (check_remote_datawriter_dispose_instance),
    F (get_permissions_token),
    F (get_permissions_credential_token),
    F (set_listener),
    F (return_permissions_token),
    F (return_permissions_credential_token),
    F (get_participant_sec_attributes),
    F (get_topic_sec_attributes),
    F (get_datawriter_sec_attributes),
    F (get_datareader_sec_attributes),
    F (return_participant_sec_attributes),
    F (return_datawriter_sec_attributes),
    F (return_datareader_sec_attributes),
    F (return_permissions_handle)
  };
#undef F
#define F(name) FGEN (dds_security_crypto_key_factory, name)
  static const struct verify_plugin_functions_tab cryptoF[] = {
    F (register_local_participant),
    F (register_matched_remote_participant),
    F (register_local_datawriter),
    F (register_matched_remote_datareader),
    F (register_local_datareader),
    F (register_matched_remote_datawriter),
    F (unregister_participant),
    F (unregister_datawriter),
    F (unregister_datareader)
  };
#undef F
#define F(name) FGEN (dds_security_crypto_key_exchange, name)
  static const struct verify_plugin_functions_tab cryptoX[] = {
    F (create_local_participant_crypto_tokens),
    F (set_remote_participant_crypto_tokens),
    F (create_local_datawriter_crypto_tokens),
    F (set_remote_datawriter_crypto_tokens),
    F (create_local_datareader_crypto_tokens),
    F (set_remote_datareader_crypto_tokens),
    F (return_crypto_tokens)
  };
#undef F
#define F(name) FGEN (dds_security_crypto_transform, name)
  static const struct verify_plugin_functions_tab cryptoT[] = {
    F (encode_serialized_payload),
    F (encode_datawriter_submessage),
    F (encode_datareader_submessage),
    F (encode_rtps_message),
    F (decode_rtps_message),
    F (preprocess_secure_submsg),
    F (decode_datawriter_submessage),
    F (decode_datareader_submessage),
    F (decode_serialized_payload)
  };
#undef F
#define C(context, plugin, table) verify_plugin_functions (context, plugin, table, sizeof (table) / sizeof (table[0]), gv)
  if (C (authentication_context, auth_plugin, auth) &&
      C (access_control_context, ac_plugin, ac) &&
      C (crypto_context->crypto_key_factory, crypto_plugin, cryptoF) &&
      C (crypto_context->crypto_key_exchange, crypto_plugin, cryptoX) &&
      C (crypto_context->crypto_transform, crypto_plugin, cryptoT))
  {
    return DDS_RETCODE_OK;
  }
  else
  {
    return DDS_RETCODE_ERROR;
  }
#undef C
}

/**
 * All fields of the library properties are supposed to be non-empty
 */
dds_return_t dds_security_load_security_library (const dds_security_plugin_config *plugin_config, dds_security_plugin *security_plugin,
    void **security_plugin_context, struct ddsi_domaingv *gv)
{
  dds_return_t lib_ret;
  char *init_parameters = "";
  char *library_str;

  assert (plugin_config->library_path);
  assert (plugin_config->library_init);
  assert (plugin_config->library_finalize);

  security_plugin->lib_handle = NULL;
  if (*plugin_config->library_path == 0)
    return DDS_RETCODE_ERROR;

  const size_t poff = (strncmp (plugin_config->library_path, "file://", 7) == 0) ? 7 : 0;
  (void) ddsrt_asprintf (&library_str, "%s", plugin_config->library_path + poff);
  lib_ret = ddsrt_dlopen (library_str, true, &security_plugin->lib_handle);
  ddsrt_free (library_str);
  if (lib_ret != DDS_RETCODE_OK)
  {
    char buffer[256];
    ddsrt_dlerror (buffer, sizeof (buffer));
    GVERROR ("Could not load %s library: %s\n", security_plugin->name, buffer);
    goto load_error;
  }

  void *tmp;
  if (ddsrt_dlsym (security_plugin->lib_handle, plugin_config->library_init, &tmp) != DDS_RETCODE_OK)
  {
    GVERROR ("Could not find the function: %s\n", plugin_config->library_init);
    goto library_error;
  }
  security_plugin->func_init = (plugin_init) tmp;

  if (ddsrt_dlsym (security_plugin->lib_handle, plugin_config->library_finalize, &tmp) != DDS_RETCODE_OK)
  {
    GVERROR ("Could not find the function: %s\n", plugin_config->library_finalize);
    goto library_error;
  }
  security_plugin->func_finalize = (plugin_finalize) tmp;

  if (security_plugin->func_init != 0)
  {
    if (security_plugin->func_init (init_parameters, (void **) security_plugin_context, gv) != DDS_RETCODE_OK)
    {
      GVERROR ("Error occurred while initializing %s plugin\n", security_plugin->name);
      goto library_error;
    }
  }
  return DDS_RETCODE_OK;

library_error:
  (void) ddsrt_dlclose (security_plugin->lib_handle);
  security_plugin->lib_handle = NULL;
load_error:
  return DDS_RETCODE_ERROR;
}

dds_return_t dds_security_plugin_release (const dds_security_plugin *security_plugin, void *context)
{
  dds_return_t result = DDS_RETCODE_OK;
  assert (security_plugin->lib_handle);
  assert (security_plugin->func_finalize);

  /* if get error from either finalize OR close,  return error */
  if (security_plugin->func_finalize (context) != DDS_RETCODE_OK)
  {
    DDS_ERROR("Error occurred while finaizing %s plugin", security_plugin->name);
    result = DDS_RETCODE_ERROR;
  }
  if (ddsrt_dlclose (security_plugin->lib_handle) != DDS_RETCODE_OK){
    result = DDS_RETCODE_ERROR;
  }
  return result;
}
