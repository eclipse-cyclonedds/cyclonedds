/*
 * Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#include <string.h>

#include "dds/ddsrt/misc.h"

#include "dds/security/core/dds_security_utils.h"
#include "dds/security/core/dds_security_plugins.h"
#include "dds/security/dds_security_api.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/ddsi_security_msg.h"
#include "dds/ddsi/ddsi_security_omg.h"


#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/dynlib.h"
#include "dds/ddsrt/process.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/hopscotch.h"

#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_xevent.h"
#include "dds/ddsi/q_time.h"
#include "dds/ddsi/q_plist.h"
#include "dds/ddsrt/io.h"



#define AUTH_NAME "Authentication"
#define AC_NAME "Access Control"
#define CRYPTO_NAME "Cryptographic"

dds_return_t dds_security_check_plugin_configuration(
    const dds_security_plugin_suite_config *security_suite_config )
{

  dds_return_t result = DDS_RETCODE_ERROR;

  if (security_suite_config->access_control.library_path == NULL) {
    DDS_ERROR("AccessControl security plugin library path is not defined");
  } else if (strlen(security_suite_config->access_control.library_path)
      == 0) {
    DDS_ERROR("AccessControl security plugin library path is empty ");
  } else if (security_suite_config->access_control.library_init == NULL) {
    DDS_ERROR("AccessControl security plugin init function is not defined");
  } else if (strlen(security_suite_config->access_control.library_init)
      == 0) {
    DDS_ERROR("AccessControl security plugin init function is empty ");
  } else if (security_suite_config->access_control.library_finalize == NULL) {
    DDS_ERROR(
        "AccessControl security plugin finalize function is not defined ");
  } else if (strlen(security_suite_config->access_control.library_finalize)
      == 0) {
    DDS_ERROR("AccessControl security plugin finalize function is empty");
  } else if (security_suite_config->authentication.library_path == NULL) {
    DDS_ERROR(
        "Authentication security plugin library path is not defined in the configuration ");
  } else  if (strlen(security_suite_config->authentication.library_path)
      == 0) {
    DDS_ERROR("Authentication security plugin library path is empty ");
  } else if (security_suite_config->authentication.library_init == NULL) {
    DDS_ERROR("Authentication security plugin init function is not defined ");
  } else if (strlen(security_suite_config->authentication.library_init)
      == 0) {
    DDS_ERROR("Authentication security plugin init function is empty ");
  } else if (security_suite_config->authentication.library_finalize == NULL) {
    DDS_ERROR(
        "Authentication security plugin finalize function is not defined ");
  } else if (strlen(security_suite_config->authentication.library_finalize)
      == 0) {
    DDS_ERROR("Authentication security plugin finalize function is empty");
  } else if (security_suite_config->cryptography.library_path == NULL) {
    DDS_ERROR(
        "Cryptography security plugin library path is not defined in the configuration ");
  } else if (strlen(security_suite_config->cryptography.library_path)
      == 0) {
    DDS_ERROR("Cryptography security plugin library path is empty ");
  } else if (security_suite_config->cryptography.library_init == NULL) {
    DDS_ERROR("Cryptography security plugin init function is not defined ");
  } else if (strlen(security_suite_config->cryptography.library_init)
      == 0) {
    DDS_ERROR("Cryptography security plugin init function is empty ");
  } else if (security_suite_config->cryptography.library_finalize == NULL) {
    DDS_ERROR("Cryptography security plugin finalize function is not defined ");
  } else if (strlen(security_suite_config->cryptography.library_finalize)
      == 0) {
    DDS_ERROR("Cryptography security plugin finalize function is empty");
  } else {
    result = DDS_RETCODE_OK;
  }

  return result;
}

/*
 * checks the function pointer value and CHANGES the out-result value if it is NULL
 */
static bool verify_function(void *function_ptr, dds_security_plugin *plugin,
    const char *function_name)
{

  if ( function_ptr == NULL ) {
    DDS_ERROR("Could not find the function for %s: %s \n", plugin->name,
        function_name);
    return false;
  }
  else {
    return true;
  }
}

dds_return_t dds_security_verify_plugin_functions(
    dds_security_authentication *authentication_context, dds_security_plugin *auth_plugin,
    dds_security_cryptography *crypto_context, dds_security_plugin *crypto_plugin,
    dds_security_access_control *access_control_context, dds_security_plugin *ac_plugin)
{

  if(
    verify_function(authentication_context->validate_local_identity, auth_plugin,
        "validate_local_identity" ) &&
    verify_function(authentication_context->get_identity_token, auth_plugin,
        "get_identity_token" ) &&
    verify_function(authentication_context->get_identity_status_token,
        auth_plugin, "get_identity_status_token" ) &&
    verify_function(authentication_context->set_permissions_credential_and_token,
        auth_plugin, "set_permissions_credential_and_token" ) &&
    verify_function(authentication_context->validate_remote_identity,
        auth_plugin, "validate_remote_identity" ) &&
    verify_function(authentication_context->begin_handshake_request, auth_plugin,
        "begin_handshake_request" ) &&
    verify_function(authentication_context->begin_handshake_reply, auth_plugin,
        "begin_handshake_reply" ) &&
    verify_function(authentication_context->process_handshake, auth_plugin,
        "process_handshake" ) &&
    verify_function(authentication_context->get_shared_secret, auth_plugin,
        "get_shared_secret" ) &&
    verify_function(
        authentication_context->get_authenticated_peer_credential_token,
        auth_plugin, "get_authenticated_peer_credential_token" ) &&
    verify_function(authentication_context->set_listener, auth_plugin,
        "set_listener" ) &&
    verify_function(authentication_context->return_identity_token, auth_plugin,
        "return_identity_token" ) &&
    verify_function(authentication_context->return_identity_status_token,
        auth_plugin, "return_identity_status_token" ) &&

    verify_function(
        authentication_context->return_authenticated_peer_credential_token,
        auth_plugin, "return_authenticated_peer_credential_token" ) &&
    verify_function(authentication_context->return_handshake_handle, auth_plugin,
        "return_handshake_handle" ) &&
    verify_function(authentication_context->return_identity_handle, auth_plugin,
        "return_identity_handle" ) &&
    verify_function(authentication_context->return_sharedsecret_handle,
        auth_plugin, "return_sharedsecret_handle" ) &&

    verify_function(access_control_context->validate_local_permissions,
        ac_plugin, "validate_local_permissions" ) &&
    verify_function(access_control_context->validate_remote_permissions,
        ac_plugin, "validate_remote_permissions" ) &&
    verify_function(access_control_context->check_create_participant, ac_plugin,
        "check_create_participant" ) &&
    verify_function(access_control_context->check_create_datawriter, ac_plugin,
        "check_create_datawriter" ) &&
    verify_function(access_control_context->check_create_datareader, ac_plugin,
        "check_create_datareader" ) &&

    verify_function(access_control_context->check_create_topic, ac_plugin,
        "check_create_topic" ) &&
    verify_function(
        access_control_context->check_local_datawriter_register_instance,
        ac_plugin, "check_local_datawriter_register_instance" ) &&
    verify_function(
        access_control_context->check_local_datawriter_dispose_instance,
        ac_plugin, "check_local_datawriter_dispose_instance" ) &&
    verify_function(access_control_context->check_remote_participant, ac_plugin,
        "check_remote_participant" ) &&
    verify_function(access_control_context->check_remote_datawriter, ac_plugin,
        "check_remote_datawriter" ) &&
    verify_function(access_control_context->check_remote_datareader, ac_plugin,
        "check_remote_datareader" ) &&
    verify_function(access_control_context->check_remote_topic, ac_plugin,
        "check_remote_topic" ) &&
    verify_function(access_control_context->check_local_datawriter_match,
        ac_plugin, "check_local_datawriter_match" ) &&
    verify_function(access_control_context->check_local_datareader_match,
        ac_plugin, "check_local_datareader_match" ) &&
    verify_function(
        access_control_context->check_remote_datawriter_register_instance,
        ac_plugin, "check_remote_datawriter_register_instance" ) &&
    verify_function(
        access_control_context->check_remote_datawriter_dispose_instance,
        ac_plugin, "check_remote_datawriter_dispose_instance" ) &&
    verify_function(access_control_context->get_permissions_token, ac_plugin,
        "get_permissions_token" ) &&
    verify_function(access_control_context->get_permissions_credential_token,
        ac_plugin, "get_permissions_credential_token" ) &&
    verify_function(access_control_context->set_listener, ac_plugin,
        "set_listener" ) &&
    verify_function(access_control_context->return_permissions_token, ac_plugin,
        "return_permissions_token" ) &&
    verify_function(access_control_context->return_permissions_credential_token,
        ac_plugin, "return_permissions_credential_token" ) &&
    verify_function(access_control_context->get_participant_sec_attributes,
        ac_plugin, "get_participant_sec_attributes" ) &&
    verify_function(access_control_context->get_topic_sec_attributes, ac_plugin,
        "get_topic_sec_attributes" ) &&
    verify_function(access_control_context->get_datawriter_sec_attributes,
        ac_plugin, "get_datawriter_sec_attributes" ) &&
    verify_function(access_control_context->get_datareader_sec_attributes,
        ac_plugin, "get_datareader_sec_attributes" ) &&
    verify_function(access_control_context->return_participant_sec_attributes,
        ac_plugin, "return_participant_sec_attributes" ) &&
    verify_function(access_control_context->return_datawriter_sec_attributes,
        ac_plugin, "return_datawriter_sec_attributes" ) &&
    verify_function(access_control_context->return_datareader_sec_attributes,
        ac_plugin, "return_datareader_sec_attributes" ) &&
    verify_function(access_control_context->return_permissions_handle,
          ac_plugin, "return_permissions_handle" ) &&

    verify_function(
        crypto_context->crypto_key_factory->register_local_participant,
        crypto_plugin, "register_local_participant" ) &&
    verify_function(
        crypto_context->crypto_key_factory->register_matched_remote_participant,
        crypto_plugin, "register_matched_remote_participant" ) &&
    verify_function(crypto_context->crypto_key_factory->register_local_datawriter,
        crypto_plugin, "register_local_datawriter" ) &&
    verify_function(
        crypto_context->crypto_key_factory->register_matched_remote_datareader,
        crypto_plugin, "register_matched_remote_datareader" ) &&
    verify_function(crypto_context->crypto_key_factory->register_local_datareader,
        crypto_plugin, "register_local_datareader" ) &&
    verify_function(
        crypto_context->crypto_key_factory->register_matched_remote_datawriter,
        crypto_plugin, "register_matched_remote_datawriter" ) &&
    verify_function(crypto_context->crypto_key_factory->unregister_participant,
        crypto_plugin, "unregister_participant" ) &&
    verify_function(crypto_context->crypto_key_factory->unregister_datawriter,
        crypto_plugin, "unregister_datawriter" ) &&
    verify_function(crypto_context->crypto_key_factory->unregister_datareader,
        crypto_plugin, "unregister_datareader" ) &&

    verify_function(
        crypto_context->crypto_key_exchange->create_local_participant_crypto_tokens,
        crypto_plugin, "create_local_participant_crypto_tokens" ) &&
    verify_function(
        crypto_context->crypto_key_exchange->set_remote_participant_crypto_tokens,
        crypto_plugin, "set_remote_participant_crypto_tokens" ) &&
    verify_function(
        crypto_context->crypto_key_exchange->create_local_datawriter_crypto_tokens,
        crypto_plugin, "create_local_datawriter_crypto_tokens" ) &&
    verify_function(
        crypto_context->crypto_key_exchange->set_remote_datawriter_crypto_tokens,
        crypto_plugin, "set_remote_datawriter_crypto_tokens" ) &&
    verify_function(
        crypto_context->crypto_key_exchange->create_local_datareader_crypto_tokens,
        crypto_plugin, "create_local_datareader_crypto_tokens" ) &&
    verify_function(
        crypto_context->crypto_key_exchange->set_remote_datareader_crypto_tokens,
        crypto_plugin, "set_remote_datareader_crypto_tokens" ) &&
    verify_function(crypto_context->crypto_key_exchange->return_crypto_tokens,
        crypto_plugin, "return_crypto_tokens" ) &&

    verify_function(crypto_context->crypto_transform->encode_serialized_payload,
        crypto_plugin, "encode_serialized_payload" ) &&
    verify_function(
        crypto_context->crypto_transform->encode_datawriter_submessage,
        crypto_plugin, "encode_datawriter_submessage" ) &&
    verify_function(
        crypto_context->crypto_transform->encode_datareader_submessage,
        crypto_plugin, "encode_datareader_submessage" ) &&
    verify_function(crypto_context->crypto_transform->encode_rtps_message,
        crypto_plugin, "encode_rtps_message" ) &&
    verify_function(crypto_context->crypto_transform->decode_rtps_message,
        crypto_plugin, "decode_rtps_message" ) &&
    verify_function(crypto_context->crypto_transform->preprocess_secure_submsg,
        crypto_plugin, "preprocess_secure_submsg" ) &&
    verify_function(
        crypto_context->crypto_transform->decode_datawriter_submessage,
        crypto_plugin, "decode_datawriter_submessage" ) &&
    verify_function(
        crypto_context->crypto_transform->decode_datareader_submessage,
        crypto_plugin, "decode_datareader_submessage" ) &&
    verify_function(crypto_context->crypto_transform->decode_serialized_payload,
        crypto_plugin, "decode_serialized_payload" ) ){
    return DDS_RETCODE_OK;
  }
  else {
    return DDS_RETCODE_ERROR;
  }

}

/**
 * All fields of the library properties are supposed to be non-empty
 */
dds_return_t dds_security_load_security_library(
    const dds_security_plugin_config *plugin_config,
    dds_security_plugin *security_plugin,
    void **security_plugin_context)
{
  dds_return_t ret = DDS_RETCODE_ERROR;
  dds_return_t lib_ret = DDS_RETCODE_ERROR;
  char * init_parameters = "";
  char *library_str;

  assert( plugin_config->library_path );
  assert( plugin_config->library_init );
  assert( plugin_config->library_finalize );

  if ( strlen(plugin_config->library_path) > 0 ) {

    //library_str = ddsrt_malloc(strlen(plugin_config->library_path) + 1);

    if (strncmp(plugin_config->library_path, "file://", 7) == 0) {
      (void)ddsrt_asprintf(&library_str, "%s", &plugin_config->library_path[7]);
    } else {
      (void)ddsrt_asprintf(&library_str, "%s", plugin_config->library_path);
    }

    lib_ret = ddsrt_dlopen( library_str, true, &security_plugin->lib_handle);
    ddsrt_free(library_str);
    if( lib_ret == DDS_RETCODE_OK && security_plugin->lib_handle){

      /* Get init and fini functions . */
      if ( ddsrt_dlsym(security_plugin->lib_handle, plugin_config->library_init, (void **)&security_plugin->func_init) == DDS_RETCODE_OK){
        if ( ddsrt_dlsym(security_plugin->lib_handle, plugin_config->library_finalize, (void **)&security_plugin->func_finalize) == DDS_RETCODE_OK){

          /* Initialize plugin. */
          if ( security_plugin->func_init != NULL) {
            lib_ret = security_plugin->func_init(init_parameters, (void **) security_plugin_context);

            if (lib_ret == DDS_RETCODE_OK){ /* error occured on init */
              return DDS_RETCODE_OK;
            } else{
              DDS_ERROR("Error occured while initializing %s plugin\n",
                 security_plugin->name);
              goto library_error;
            }
          }

        }
        else {
          DDS_ERROR("Could not find the function: %s\n", plugin_config->library_finalize);
          goto library_error;
        }


      }
      else{
        DDS_ERROR("Could not find the function: %s\n",plugin_config->library_init);
        goto library_error;
      }

    } else {
      char buffer[256];
      ddsrt_dlerror(buffer, sizeof(buffer));
      DDS_ERROR("Could not load %s library: %s\n", security_plugin->name, buffer);
      goto load_error;
    }


    return ret;
  }


library_error:
  ddsrt_dlclose(security_plugin->lib_handle);
  security_plugin->lib_handle = NULL;
load_error:
  return ret;
}

dds_return_t dds_security_plugin_release( const dds_security_plugin *security_plugin, void *context ){
  dds_return_t result= DDS_RETCODE_OK;
  assert( security_plugin->lib_handle );
  assert( security_plugin->func_finalize );

  /* if get error from either finalize OR close,  return error */
  if( security_plugin->func_finalize( context ) != DDS_RETCODE_OK){
    DDS_ERROR("Error occured while finaizing %s plugin", security_plugin->name);
    result = DDS_RETCODE_ERROR;
  }
  if( ddsrt_dlclose( security_plugin->lib_handle ) != DDS_RETCODE_OK){
    result = DDS_RETCODE_ERROR;
  }
  return result;
}

