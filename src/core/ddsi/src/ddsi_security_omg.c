/*
 * Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v. 2.0 which is available at
 * http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
 * v. 1.0 which is available at
 * http://www.eclipse.org/org/documents/edl-v10.php.
 *
 * SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
 */
#ifdef DDSI_INCLUDE_SECURITY

#include <string.h>

#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/process.h"

#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_unused.h"
#include "dds/ddsi/q_radmin.h"
#include "dds/ddsi/ddsi_security_omg.h"
#include "dds/ddsi/ddsi_sertopic.h"

#include "dds/ddsi/q_config.h"
#include "dds/ddsi/q_log.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsi/q_ephash.h"


#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/core/dds_security_plugins.h"
#include "dds/ddsrt/hopscotch.h"

#include "dds/ddsi/q_entity.h"
#include "dds/ddsi/q_bswap.h"
#include "dds/ddsi/q_xevent.h"
#include "dds/ddsi/q_time.h"
#include "dds/ddsi/q_plist.h"



#define AUTH_NAME "Authentication"
#define AC_NAME "Access Control"
#define CRYPTO_NAME "Cryptographic"

#define SECURITY_EXCEPTION_INIT {NULL, 0, 0}



struct dds_security_context {
  dds_security_plugin auth_plugin;
  dds_security_plugin ac_plugin;
  dds_security_plugin crypto_plugin;

  dds_security_authentication *authentication_context;
  dds_security_cryptography *crypto_context;
  dds_security_access_control *access_control_context;
  ddsrt_mutex_t omg_security_lock;
  uint32_t next_plugin_id;
};

typedef struct dds_security_context dds_security_context;


static bool
q_omg_writer_is_payload_protected(
  const struct writer *wr);




static bool endpoint_is_DCPSParticipantSecure(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_READER) );
}

static bool endpoint_is_DCPSPublicationsSecure(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_READER) );
}

static bool endpoint_is_DCPSSubscriptionsSecure(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_READER) );
}

static bool endpoint_is_DCPSParticipantStatelessMessage(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_STATELESS_MESSAGE_READER) );
}

static bool endpoint_is_DCPSParticipantMessageSecure(const ddsi_guid_t *guid)
{
  return ((guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_MESSAGE_SECURE_READER) );
}

static bool endpoint_is_DCPSParticipantVolatileMessageSecure(const ddsi_guid_t *guid)
{
#if 1
  /* TODO: volatile endpoint. */
  DDSRT_UNUSED_ARG(guid);
  return false;
#else
  return ((guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_WRITER) ||
          (guid->entityid.u == NN_ENTITYID_P2P_BUILTIN_PARTICIPANT_VOLATILE_SECURE_READER) );
#endif
}

bool q_omg_is_security_loaded(  dds_security_context *sc ){
  if( sc->crypto_context == NULL && sc->authentication_context == NULL && sc->access_control_context == NULL){
    return false;
  } else {
    return true;
  }
}

void q_omg_security_init( dds_security_context **sc )
{


    *sc = ddsrt_malloc( sizeof( dds_security_context));
    memset( *sc, 0, sizeof( dds_security_context));
  //if( participant_reference_count == 0 ){

    (*sc)->auth_plugin.name = AUTH_NAME;
    (*sc)->ac_plugin.name = AC_NAME;
    (*sc)->crypto_plugin.name = CRYPTO_NAME;

    (void)ddsrt_mutex_init(&(*sc)->omg_security_lock);
    DDS_LOG(DDS_LC_TRACE,"DDS Security init\n");
#if HANDSHAKE_IMPLEMENTED
    //remote_participant_crypto_handle_list_init();
#endif
  //}

  //participant_reference_count++;
}



/**
 * Releases all plugins
 */
static void release_plugins( dds_security_context *security_context )
{
#if HANDSHAKE_IMPLEMENTED
  q_handshake_terminate();
#endif


  if (dds_security_plugin_release( &security_context->auth_plugin, security_context->authentication_context )) {
    DDS_ERROR("Error occured releasing %s plugin", security_context->auth_plugin.name);
  }

  if (dds_security_plugin_release( &security_context->crypto_plugin, security_context->crypto_context )) {
    DDS_ERROR("Error occured releasing %s plugin", security_context->crypto_plugin.name);
  }

  if (dds_security_plugin_release( &security_context->ac_plugin, security_context->access_control_context )) {
    DDS_ERROR("Error occured releasing %s plugin", security_context->ac_plugin.name);
  }

  security_context->authentication_context = NULL;
  security_context->access_control_context = NULL;
  security_context->crypto_context = NULL;
}


void q_omg_security_deinit( struct dds_security_context **security_context) {

  assert( security_context != NULL );
  assert( *security_context != NULL );

#if HANDSHAKE_IMPLEMENTED
    //remote_participant_crypto_handle_list_deinit();
#endif
    if( (*security_context)->authentication_context != NULL && (*security_context)->access_control_context != NULL && (*security_context)->crypto_context != NULL ){
      release_plugins( *security_context );
    }

    ddsrt_mutex_destroy(&(*security_context)->omg_security_lock);
    ddsrt_free( *security_context );
    *security_context = NULL;

    DDS_LOG(DDS_LC_TRACE,"DDS Security deinit\n");
}



static void
dds_qos_to_security_plugin_configuration(
   const dds_qos_t *qos,
   dds_security_plugin_suite_config *suite_config)
{
  uint32_t i;

#define CHECK_SECURITY_PROPERTY( security_property, target ) \
    if(strcmp (qos->property.value.props[i].name, security_property) == 0){ \
      target = ddsrt_strdup( qos->property.value.props[i].value ); \
    }

  for (i = 0; i < qos->property.value.n; i++) {
    CHECK_SECURITY_PROPERTY( DDS_SEC_PROP_AUTH_LIBRARY_PATH, suite_config->authentication.library_path )
    else CHECK_SECURITY_PROPERTY( DDS_SEC_PROP_AUTH_LIBRARY_INIT, suite_config->authentication.library_init )
    else CHECK_SECURITY_PROPERTY( DDS_SEC_PROP_AUTH_LIBRARY_FINALIZE, suite_config->authentication.library_finalize )
    else CHECK_SECURITY_PROPERTY( DDS_SEC_PROP_CRYPTO_LIBRARY_PATH, suite_config->cryptography.library_path )
    else CHECK_SECURITY_PROPERTY( DDS_SEC_PROP_CRYPTO_LIBRARY_INIT, suite_config->cryptography.library_init )
    else CHECK_SECURITY_PROPERTY( DDS_SEC_PROP_CRYPTO_LIBRARY_FINALIZE, suite_config->cryptography.library_finalize )
    else CHECK_SECURITY_PROPERTY( DDS_SEC_PROP_ACCESS_LIBRARY_PATH, suite_config->access_control.library_path )
    else CHECK_SECURITY_PROPERTY( DDS_SEC_PROP_ACCESS_LIBRARY_INIT, suite_config->access_control.library_init )
    else CHECK_SECURITY_PROPERTY( DDS_SEC_PROP_ACCESS_LIBRARY_FINALIZE, suite_config->access_control.library_finalize )
  }

#undef CHECK_SECURITY_PROPERTY
}

static void deinit_plugin_config(dds_security_plugin_config *plugin_config){
  ddsrt_free( plugin_config->library_path );
  ddsrt_free( plugin_config->library_init );
  ddsrt_free( plugin_config->library_finalize );
}

static void deinit_plugin_suite_config(dds_security_plugin_suite_config *suite_config ){
  deinit_plugin_config( &suite_config->access_control );
  deinit_plugin_config( &suite_config->authentication );
  deinit_plugin_config( &suite_config->cryptography );

}

dds_return_t q_omg_security_load( dds_security_context *security_context,
    const dds_qos_t *qos)
{
  dds_return_t ret = DDS_RETCODE_ERROR;

  ddsrt_mutex_lock(&security_context->omg_security_lock);

  dds_security_plugin_suite_config plugin_suite_config;

  memset ( &plugin_suite_config, 0, sizeof(dds_security_plugin_suite_config));
  /* Get plugin information */

  dds_qos_to_security_plugin_configuration( qos, &plugin_suite_config);

  /* Check configuration content */
  if( dds_security_check_plugin_configuration( &plugin_suite_config ) == DDS_RETCODE_OK ){

    if (dds_security_load_security_library(
        &(plugin_suite_config.authentication), &security_context->auth_plugin,
        (void**) &security_context->authentication_context) == DDS_RETCODE_OK) {

      if (dds_security_load_security_library(
          &(plugin_suite_config.access_control), &security_context->ac_plugin,
          (void**) &security_context->access_control_context)  == DDS_RETCODE_OK ) {

        if (dds_security_load_security_library(
                  &(plugin_suite_config.cryptography), &security_context->crypto_plugin,
                  (void**) &security_context->crypto_context) == DDS_RETCODE_OK ) {
          /* now check if all plugin functions are implemented */
          if( dds_security_verify_plugin_functions(
              security_context->authentication_context,&security_context->auth_plugin,
              security_context->crypto_context,&security_context->crypto_plugin,
              security_context->access_control_context, &security_context->ac_plugin) == DDS_RETCODE_OK){

            /* Add listeners */
#if LISTENERS_IMPLEMENTED
            if ( access_control_context->set_listener(access_control_context, &listener_ac, &ex)) {
              if ( authentication_context->set_listener(authentication_context, &listener_auth, &ex)) {
#if HANDSHAKE_IMPLEMENTED
              (void)q_handshake_initialize();
#endif
              } else {
                DDS_ERROR("Could not set authentication listener: %s\n",
                          ex.message ? ex.message : "<unknown error>");
              }

            } else {
              DDS_ERROR("Could not set access_control listener: %s\n",
                        ex.message ? ex.message : "<unknown error>");
            }
#endif //LISTENERS_IMPLEMENTED

            //tried_to_load = true;
            //ret = last_load_result = DDS_RETCODE_OK;
            ret = DDS_RETCODE_OK;
            //omg_security_plugin_loaded = true;
            DDS_INFO( "DDS Security plugins have been loaded\n" );
          } else {
            release_plugins( security_context );
          }

        } else{
          DDS_ERROR("Could not load %s library\n", security_context->crypto_plugin.name);
        }
      }else{
        DDS_ERROR("Could not load %s library\n", security_context->ac_plugin.name);
      }

    }
    else{
      DDS_ERROR("Could not load %s plugin.\n", security_context->auth_plugin.name);

    }

  }

  deinit_plugin_suite_config( &plugin_suite_config );

  ddsrt_mutex_unlock( &security_context->omg_security_lock );


  return ret;
}

bool
q_omg_participant_is_secure(
  const struct participant *pp)
{
  /* TODO: Register local participant. */
  DDSRT_UNUSED_ARG(pp);
  return false;
}

static bool
q_omg_writer_is_discovery_protected(
  const struct writer *wr)
{
  /* TODO: Register local writer. */
  DDSRT_UNUSED_ARG(wr);
  return false;
}

static bool
q_omg_reader_is_discovery_protected(
  const struct reader *rd)
{
  /* TODO: Register local reader. */
  DDSRT_UNUSED_ARG(rd);
  return false;
}

bool
q_omg_get_writer_security_info(
  const struct writer *wr,
  nn_security_info_t *info)
{
  assert(wr);
  assert(info);
  /* TODO: Register local writer. */
  DDSRT_UNUSED_ARG(wr);

  info->plugin_security_attributes = 0;
  if (q_omg_writer_is_payload_protected(wr))
  {
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID|
                                NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_PAYLOAD_PROTECTED;
  }
  else
  {
    info->security_attributes = 0;
  }
  return true;
}

bool
q_omg_get_reader_security_info(
  const struct reader *rd,
  nn_security_info_t *info)
{
  assert(rd);
  assert(info);
  /* TODO: Register local reader. */
  DDSRT_UNUSED_ARG(rd);
  info->plugin_security_attributes = 0;
  info->security_attributes = 0;
  return false;
}

static bool
q_omg_proxyparticipant_is_authenticated(
  const struct proxy_participant *proxy_pp)
{
  /* TODO: Handshake */
  DDSRT_UNUSED_ARG(proxy_pp);
  return false;
}

int64_t
q_omg_security_get_local_participant_handle(
  struct participant *pp)
{
  /* TODO: Local registration */
  DDSRT_UNUSED_ARG(pp);
  return 0;
}

int64_t
q_omg_security_get_remote_participant_handle(
  struct proxy_participant *proxypp)
{
  /* TODO: Handshake */
  DDSRT_UNUSED_ARG(proxypp);
  return 0;
}

unsigned
determine_subscription_writer(
  const struct reader *rd)
{
  if (q_omg_reader_is_discovery_protected(rd))
  {
    return NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_SECURE_WRITER;
  }
  return NN_ENTITYID_SEDP_BUILTIN_SUBSCRIPTIONS_WRITER;
}

unsigned
determine_publication_writer(
  const struct writer *wr)
{
  if (q_omg_writer_is_discovery_protected(wr))
  {
    return NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_SECURE_WRITER;
  }
  return NN_ENTITYID_SEDP_BUILTIN_PUBLICATIONS_WRITER;
}

bool
is_proxy_participant_deletion_allowed(
  struct q_globals * const gv,
  const struct ddsi_guid *guid,
  const ddsi_entityid_t pwr_entityid)
{
  struct proxy_participant *proxypp;

  assert(gv);
  assert(guid);

  /* TODO: Check if the proxy writer guid prefix matches that of the proxy
   *       participant. Deletion is not allowed when they're not equal. */

  /* Always allow deletion from a secure proxy writer. */
  if (pwr_entityid.u == NN_ENTITYID_SPDP_RELIABLE_BUILTIN_PARTICIPANT_SECURE_WRITER)
    return true;

  /* Not from a secure proxy writer.
   * Only allow deletion when proxy participant is not authenticated. */
  proxypp = ephash_lookup_proxy_participant_guid(gv->guid_hash, guid);
  if (!proxypp)
  {
    GVLOGDISC (" unknown");
    return false;
  }
  return (!q_omg_proxyparticipant_is_authenticated(proxypp));
}

bool
q_omg_security_is_remote_rtps_protected(
  struct proxy_participant *proxy_pp,
  ddsi_entityid_t entityid)
{
  /* TODO: Handshake */
  DDSRT_UNUSED_ARG(proxy_pp);
  DDSRT_UNUSED_ARG(entityid);
  return false;
}

bool
q_omg_security_is_local_rtps_protected(
  struct participant *pp,
  ddsi_entityid_t entityid)
{
  /* TODO: Handshake */
  DDSRT_UNUSED_ARG(pp);
  DDSRT_UNUSED_ARG(entityid);
  return false;
}

void
set_proxy_participant_security_info(
  struct proxy_participant *proxypp,
  const nn_plist_t *plist)
{
  assert(proxypp);
  assert(plist);
  if (plist->present & PP_PARTICIPANT_SECURITY_INFO) {
    proxypp->security_info.security_attributes = plist->participant_security_info.security_attributes;
    proxypp->security_info.plugin_security_attributes = plist->participant_security_info.plugin_security_attributes;
  } else {
    proxypp->security_info.security_attributes = 0;
    proxypp->security_info.plugin_security_attributes = 0;
  }
}

static void
q_omg_get_proxy_endpoint_security_info(
  const struct entity_common *entity,
  nn_security_info_t *proxypp_sec_info,
  const nn_plist_t *plist,
  nn_security_info_t *info)
{
  bool proxypp_info_available;

  proxypp_info_available = (proxypp_sec_info->security_attributes != 0) ||
                           (proxypp_sec_info->plugin_security_attributes != 0);

  /*
   * If Security info is present, use that.
   * Otherwise, use the specified values for the secure builtin endpoints.
   *      (Table 20 – EndpointSecurityAttributes for all "Builtin Security Endpoints")
   * Otherwise, reset.
   */
  if (plist->present & PP_ENDPOINT_SECURITY_INFO)
  {
    info->security_attributes = plist->endpoint_security_info.security_attributes;
    info->plugin_security_attributes = plist->endpoint_security_info.plugin_security_attributes;
  }
  else if (endpoint_is_DCPSParticipantSecure(&(entity->guid)) ||
           endpoint_is_DCPSPublicationsSecure(&(entity->guid)) ||
           endpoint_is_DCPSSubscriptionsSecure(&(entity->guid)) )
  {
    info->plugin_security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    if (proxypp_info_available)
    {
      if (proxypp_sec_info->security_attributes & NN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_PROTECTED)
      {
        info->security_attributes |= NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      }
      if (proxypp_sec_info->plugin_security_attributes & NN_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_ENCRYPTED)
      {
        info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
      }
      if (proxypp_sec_info->plugin_security_attributes & NN_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_DISCOVERY_AUTHENTICATED)
      {
        info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED;
      }
    }
    else
    {
      /* No participant info: assume hardcoded OpenSplice V6.10.0 values. */
      info->security_attributes |= NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
    }
  }
  else if (endpoint_is_DCPSParticipantMessageSecure(&(entity->guid)))
  {
    info->plugin_security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    if (proxypp_info_available)
    {
      if (proxypp_sec_info->security_attributes & NN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_PROTECTED)
      {
        info->security_attributes |= NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      }
      if (proxypp_sec_info->plugin_security_attributes & NN_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_ENCRYPTED)
      {
        info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
      }
      if (proxypp_sec_info->plugin_security_attributes & NN_PLUGIN_PARTICIPANT_SECURITY_ATTRIBUTES_FLAG_IS_LIVELINESS_AUTHENTICATED)
      {
        info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED;
      }
    }
    else
    {
      /* No participant info: assume hardcoded OpenSplice V6.10.0 values. */
      info->security_attributes |= NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
      info->plugin_security_attributes |= NN_PLUGIN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
    }
  }
  else if (endpoint_is_DCPSParticipantStatelessMessage(&(entity->guid)))
  {
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID;
    info->plugin_security_attributes = 0;
  }
  else if (endpoint_is_DCPSParticipantVolatileMessageSecure(&(entity->guid)))
  {
    info->security_attributes = NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_VALID |
                                NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED;
    info->plugin_security_attributes = 0;
  }
  else
  {
    info->security_attributes = 0;
    info->plugin_security_attributes = 0;
  }
}

void
set_proxy_reader_security_info(
  struct proxy_reader *prd,
  const nn_plist_t *plist)
{
  assert(prd);
  q_omg_get_proxy_endpoint_security_info(&(prd->e),
                                         &(prd->c.proxypp->security_info),
                                         plist,
                                         &(prd->c.security_info));
}

void
set_proxy_writer_security_info(
  struct proxy_writer *pwr,
  const nn_plist_t *plist)
{
  assert(pwr);
  q_omg_get_proxy_endpoint_security_info(&(pwr->e),
                                         &(pwr->c.proxypp->security_info),
                                         plist,
                                         &(pwr->c.security_info));
}


static bool
q_omg_security_encode_datareader_submessage(
  struct reader            *rd,
  const ddsi_guid_prefix_t *dst_prefix,
  const unsigned char      *src_buf,
  const unsigned int        src_len,
  unsigned char           **dst_buf,
  unsigned int             *dst_len)
{
  /* TODO: Use proper keys to actually encode (need key-exchange). */
  DDSRT_UNUSED_ARG(rd);
  DDSRT_UNUSED_ARG(dst_prefix);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  return false;
}

static bool
q_omg_security_encode_datawriter_submessage(
  struct writer            *wr,
  const ddsi_guid_prefix_t *dst_prefix,
  const unsigned char      *src_buf,
  const unsigned int        src_len,
  unsigned char           **dst_buf,
  unsigned int             *dst_len)
{
  /* TODO: Use proper keys to actually encode (need key-exchange). */
  DDSRT_UNUSED_ARG(wr);
  DDSRT_UNUSED_ARG(dst_prefix);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  return false;
}

static bool
q_omg_security_decode_submessage(
  const ddsi_guid_prefix_t* const src_prefix,
  const ddsi_guid_prefix_t* const dst_prefix,
  const unsigned char   *src_buf,
  const unsigned int     src_len,
  unsigned char        **dst_buf,
  unsigned int          *dst_len)
{
  /* TODO: Use proper keys to actually decode (need key-exchange). */
  DDSRT_UNUSED_ARG(src_prefix);
  DDSRT_UNUSED_ARG(dst_prefix);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  return false;
}

static bool
q_omg_security_encode_serialized_payload(
  const struct writer *wr,
  const unsigned char *src_buf,
  const unsigned int   src_len,
  unsigned char     **dst_buf,
  unsigned int       *dst_len)
{
  /* TODO: Use proper keys to actually encode (need key-exchange). */
  DDSRT_UNUSED_ARG(wr);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  return false;
}

static bool
q_omg_security_decode_serialized_payload(
  struct proxy_writer *pwr,
  const unsigned char *src_buf,
  const unsigned int   src_len,
  unsigned char     **dst_buf,
  unsigned int       *dst_len)
{
  /* TODO: Use proper keys to actually decode (need key-exchange). */
  DDSRT_UNUSED_ARG(pwr);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  return false;
}

bool
q_omg_security_encode_rtps_message(
  int64_t                 src_handle,
  ddsi_guid_t            *src_guid,
  const unsigned char    *src_buf,
  const unsigned int      src_len,
  unsigned char        **dst_buf,
  unsigned int          *dst_len,
  int64_t                dst_handle)
{
  /* TODO: Use proper keys to actually encode (need key-exchange). */
  DDSRT_UNUSED_ARG(src_handle);
  DDSRT_UNUSED_ARG(src_guid);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  DDSRT_UNUSED_ARG(dst_handle);
  return false;
}

static bool
q_omg_security_decode_rtps_message(
  struct proxy_participant *proxypp,
  const unsigned char      *src_buf,
  const unsigned int        src_len,
  unsigned char          **dst_buf,
  unsigned int            *dst_len)
{
  /* TODO: Use proper keys to actually decode (need key-exchange). */
  DDSRT_UNUSED_ARG(proxypp);
  DDSRT_UNUSED_ARG(src_buf);
  DDSRT_UNUSED_ARG(src_len);
  DDSRT_UNUSED_ARG(dst_buf);
  DDSRT_UNUSED_ARG(dst_len);
  return false;
}

static bool
q_omg_writer_is_payload_protected(
  const struct writer *wr)
{
  /* TODO: Local registration. */
  DDSRT_UNUSED_ARG(wr);
  return false;
}

static bool
q_omg_writer_is_submessage_protected(
  struct writer *wr)
{
  /* TODO: Local registration. */
  DDSRT_UNUSED_ARG(wr);
  return false;
}

static bool
q_omg_reader_is_submessage_protected(
  struct reader *rd)
{
  /* TODO: Local registration. */
  DDSRT_UNUSED_ARG(rd);
  return false;
}

bool
encode_payload(
  struct writer *wr,
  ddsrt_iovec_t *vec,
  unsigned char **buf)
{
  bool ok = true;
  *buf = NULL;
  if (q_omg_writer_is_payload_protected(wr))
  {
    /* Encrypt the data. */
    unsigned char *enc_buf;
    unsigned int   enc_len;
    ok = q_omg_security_encode_serialized_payload(
                    wr,
                    vec->iov_base,
                    (unsigned int)vec->iov_len,
                    &enc_buf,
                    &enc_len);
    if (ok)
    {
      /* Replace the iov buffer, which should always be aliased. */
      vec->iov_base = (char *)enc_buf;
      vec->iov_len = enc_len;
      /* Remember the pointer to be able to free the memory. */
      *buf = enc_buf;
    }
  }
  return ok;
}


static bool
decode_payload(
  const struct q_globals *gv,
  struct nn_rsample_info *sampleinfo,
  unsigned char *payloadp,
  uint32_t *payloadsz,
  size_t *submsg_len)
{
  bool ok = true;

  assert(payloadp);
  assert(payloadsz);
  assert(*payloadsz);
  assert(submsg_len);
  assert(sampleinfo);

  if (sampleinfo->pwr == NULL)
  {
    /* No specified proxy writer means no encoding. */
    return true;
  }

  /* Only decode when the attributes tell us so. */
  if ((sampleinfo->pwr->c.security_info.security_attributes & NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_PAYLOAD_PROTECTED)
                                                           == NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_PAYLOAD_PROTECTED)
  {
    unsigned char *dst_buf = NULL;
    unsigned int   dst_len = 0;

    /* Decrypt the payload. */
    if (q_omg_security_decode_serialized_payload(sampleinfo->pwr, payloadp, *payloadsz, &dst_buf, &dst_len))
    {
      /* Expect result to always fit into the original buffer. */
      assert(*payloadsz >= dst_len);

      /* Reduce submessage and payload lengths. */
      *submsg_len -= (*payloadsz - dst_len);
      *payloadsz   = dst_len;

      /* Replace the encrypted payload with the decrypted. */
      memcpy(payloadp, dst_buf, dst_len);
      ddsrt_free(dst_buf);
    }
    else
    {
      GVWARNING("decode_payload: failed to decrypt data from "PGUIDFMT"", PGUID (sampleinfo->pwr->e.guid));
      ok = false;
    }
  }

  return ok;
}

bool
decode_Data(
  const struct q_globals *gv,
  struct nn_rsample_info *sampleinfo,
  unsigned char *payloadp,
  uint32_t payloadsz,
  size_t *submsg_len)
{
  int ok = true;
  /* Only decode when there's actual data. */
  if (payloadp && (payloadsz > 0))
  {
    ok = decode_payload(gv, sampleinfo, payloadp, &payloadsz, submsg_len);
    if (ok)
    {
      /* It's possible that the payload size (and thus the sample size) has been reduced. */
      sampleinfo->size = payloadsz;
    }
  }
  return ok;
}

bool
decode_DataFrag(
  const struct q_globals *gv,
  struct nn_rsample_info *sampleinfo,
  unsigned char *payloadp,
  uint32_t payloadsz,
  size_t *submsg_len)
{
  int ok = true;
  /* Only decode when there's actual data. */
  if (payloadp && (payloadsz > 0))
  {
    ok = decode_payload(gv, sampleinfo, payloadp, &payloadsz, submsg_len);
    /* Do not touch the sampleinfo->size in contradiction to decode_Data() (it has been calculated differently). */
  }
  return ok;
}


void
encode_datareader_submsg(
  struct nn_xmsg *msg,
  struct nn_xmsg_marker sm_marker,
  struct proxy_writer *pwr,
  const struct ddsi_guid *rd_guid)
{
  struct reader *rd = ephash_lookup_reader_guid(pwr->e.gv->guid_hash, rd_guid);
  struct participant *pp = NULL;
  /* Only encode when needed. */
  if( rd != NULL ){
    pp = rd->c.pp;
  }
  if (!pp && q_omg_participant_is_secure( pp ))
  {
    if (q_omg_reader_is_submessage_protected(rd))
    {
      unsigned char *src_buf;
      unsigned int   src_len;
      unsigned char *dst_buf;
      unsigned int   dst_len;

      /* Make one blob of the current sub-message by appending the serialized payload. */
      nn_xmsg_submsg_append_refd_payload(msg, sm_marker);

      /* Get the sub-message buffer. */
      src_buf = (unsigned char*)nn_xmsg_submsg_from_marker(msg, sm_marker);
      src_len = (unsigned int)nn_xmsg_submsg_size(msg, sm_marker);

      /* Do the actual encryption. */
      if (q_omg_security_encode_datareader_submessage(rd, &(pwr->e.guid.prefix), src_buf, src_len, &dst_buf, &dst_len))
      {
        /* Replace the old sub-message with the new encoded one(s). */
        nn_xmsg_submsg_replace(msg, sm_marker, dst_buf, dst_len);
        ddsrt_free(dst_buf);
      }
      else
      {
        /* The sub-message should have been encoded, which failed.
         * Remove it to prevent it from being send. */
        nn_xmsg_submsg_remove(msg, sm_marker);
      }
    }
  }
}

void
encode_datawriter_submsg(
  struct nn_xmsg *msg,
  struct nn_xmsg_marker sm_marker,
  struct writer *wr)
{
  struct participant *pp = wr->c.pp;
  /* Only encode when needed. */
  if (q_omg_participant_is_secure( pp ))
  {
    if (q_omg_writer_is_submessage_protected(wr))
    {
      unsigned char *src_buf;
      unsigned int   src_len;
      unsigned char *dst_buf;
      unsigned int   dst_len;
      ddsi_guid_prefix_t dst_guid_prefix;
      ddsi_guid_prefix_t *dst = NULL;

      /* Make one blob of the current sub-message by appending the serialized payload. */
      nn_xmsg_submsg_append_refd_payload(msg, sm_marker);

      /* Get the sub-message buffer. */
      src_buf = (unsigned char*)nn_xmsg_submsg_from_marker(msg, sm_marker);
      src_len = (unsigned int)nn_xmsg_submsg_size(msg, sm_marker);

      if (nn_xmsg_getdst1prefix(msg, &dst_guid_prefix))
      {
        dst = &dst_guid_prefix;
      }

      /* Do the actual encryption. */
      if (q_omg_security_encode_datawriter_submessage(wr, dst, src_buf, src_len, &dst_buf, &dst_len))
      {
        /* Replace the old sub-message with the new encoded one(s). */
        nn_xmsg_submsg_replace(msg, sm_marker, dst_buf, dst_len);
        ddsrt_free(dst_buf);
      }
      else
      {
        /* The sub-message should have been encoded, which failed.
         * Remove it to prevent it from being send. */
        nn_xmsg_submsg_remove(msg, sm_marker);
      }
    }
  }
}



bool
validate_msg_decoding(
  const struct entity_common *e,
  const struct proxy_endpoint_common *c,
  struct proxy_participant *proxypp,
  struct receiver_state *rst,
  SubmessageKind_t prev_smid)
{
  assert(e);
  assert(c);
  assert(proxypp);
  assert(rst);

  /* If this endpoint is expected to have submessages protected, it means that the
   * previous submessage id (prev_smid) has to be SMID_SEC_PREFIX. That caused the
   * protected submessage to be copied into the current RTPS message as a clear
   * submessage, which we are currently handling.
   * However, we have to check if the prev_smid is actually SMID_SEC_PREFIX, otherwise
   * a rascal can inject data as just a clear submessage. */
  if ((c->security_info.security_attributes & NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED)
                                           == NN_ENDPOINT_SECURITY_ATTRIBUTES_FLAG_IS_SUBMESSAGE_PROTECTED)
  {
    if (prev_smid != SMID_SEC_PREFIX)
    {
      return false;
    }
  }

  /* At this point, we should also check if the complete RTPS message was encoded when
   * that is expected. */
  if (q_omg_security_is_remote_rtps_protected(proxypp, e->guid.entityid) && !rst->rtps_encoded)
  {
    return 0;
  }

  return true;
}

static int
validate_submsg(struct q_globals *gv, unsigned char smid, unsigned char *submsg, unsigned char * const end, int byteswap)
{
  int result = -1;
  if ((submsg + RTPS_SUBMESSAGE_HEADER_SIZE) <= end)
  {
    SubmessageHeader_t *hdr = (SubmessageHeader_t*)submsg;
    if ((smid == 0 /* don't care */) || (hdr->submessageId == smid))
    {
      unsigned short size = hdr->octetsToNextHeader;
      if (byteswap)
      {
         size = ddsrt_bswap2u(size);
      }
      result = (int)size + (int)RTPS_SUBMESSAGE_HEADER_SIZE;
      if ((submsg + result) > end)
      {
        result = -1;
      }
    }
    else
    {
      GVWARNING("Unexpected submsg 0x%02x (0x%02x expected)", hdr->submessageId, smid);
    }
  }
  else
  {
    GVWARNING("Submsg 0x%02x does not fit message", smid);
  }
  return result;
}


static int
padding_submsg(struct q_globals *gv, unsigned char *start, unsigned char *end, int byteswap)
{
  SubmessageHeader_t *padding = (SubmessageHeader_t*)start;
  size_t size = (size_t)(end - start);
  int result = -1;

  assert(start <= end);

  if (size > sizeof(SubmessageHeader_t))
  {
    result = (int)size;
    padding->submessageId = SMID_PAD;
    padding->flags = (byteswap ? !(DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) : (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN));
    padding->octetsToNextHeader = (unsigned short)(size - sizeof(SubmessageHeader_t));
    if (byteswap)
    {
      padding->octetsToNextHeader = ddsrt_bswap2u(padding->octetsToNextHeader);
    }
  }
  else
  {
    GVWARNING("Padding submessage doesn't fit");
  }
  return result;
}

int
decode_SecPrefix(
  struct receiver_state *rst,
  unsigned char *submsg,
  size_t submsg_size,
  unsigned char * const msg_end,
  const ddsi_guid_prefix_t * const src_prefix,
  const ddsi_guid_prefix_t * const dst_prefix,
  int byteswap)
{
  int result = -1;
  int totalsize = (int)submsg_size;
  unsigned char *body_submsg;
  unsigned char *prefix_submsg;
  unsigned char *postfix_submsg;
  SubmessageHeader_t *hdr = (SubmessageHeader_t*)submsg;
  uint8_t flags = hdr->flags;

  if (byteswap)
  {
    if ((DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN))
      hdr->flags |= 0x01;
    else
      hdr->flags &= 0xFE;
  }

  /* First sub-message is the SEC_PREFIX. */
  prefix_submsg = submsg;

  /* Next sub-message is SEC_BODY when encrypted or the original submessage when only signed. */
  body_submsg = submsg + submsg_size;
  result = validate_submsg(rst->gv, 0 /* don't care smid */, body_submsg, msg_end, byteswap);
  if (result > 0)
  {
    totalsize += result;

    /* Third sub-message should be the SEC_POSTFIX. */
    postfix_submsg = submsg + totalsize;
    result = validate_submsg(rst->gv, SMID_SEC_POSTFIX, postfix_submsg, msg_end, byteswap);
    if (result > 0)
    {
      bool decoded;
      unsigned char *dst_buf;
      unsigned int   dst_len;

      totalsize += result;

      /* Decode all three submessages. */
      decoded = q_omg_security_decode_submessage(src_prefix, dst_prefix, submsg, (unsigned int)totalsize, &dst_buf, &dst_len);
      if (decoded && dst_buf)
      {
        /*
         * The 'normal' submessage sequence handling will continue after the
         * given security SEC_PREFIX.
         */
        if (*body_submsg == SMID_SEC_BODY)
        {
          /*
           * Copy the decoded buffer into the original message, replacing (part
           * of) SEC_BODY.
           *
           * By replacing the SEC_BODY with the decoded submessage, everything
           * can continue as if there was never an encoded submessage.
           */
          assert((int)dst_len <= ((int)totalsize - (int)submsg_size));
          memcpy(body_submsg, dst_buf, dst_len);

          /* Remainder of SEC_BODY & SEC_POSTFIX should be padded to keep the submsg sequence going. */
          result = padding_submsg(rst->gv, body_submsg + dst_len, prefix_submsg + totalsize, byteswap);
        }
        else
        {
          /*
           * When only signed, then the submessage is already available and
           * SMID_SEC_POSTFIX will be ignored.
           * So, we don't really have to do anything.
           */
        }
        ddsrt_free(dst_buf);
      }
      else
      {
        /*
         * Decoding or signing failed.
         *
         * Replace the security submessages with padding. This also removes a plain
         * submessage when a signature check failed.
         */
        result = padding_submsg(rst->gv, body_submsg, prefix_submsg + totalsize, byteswap);
      }
    }
  }
  /* Restore flags. */
  hdr->flags = flags;
  return result;
}

static nn_rtps_msg_state_t
check_rtps_message_is_secure(
    struct q_globals *gv,
    Header_t *hdr,
    unsigned char *buff,
    bool isstream,
    struct proxy_participant **proxypp)
{
  nn_rtps_msg_state_t ret = NN_RTPS_MSG_STATE_ERROR;

  SubmessageHeader_t *submsg;
  uint32_t offset = RTPS_MESSAGE_HEADER_SIZE + (isstream ? sizeof(MsgLen_t) : 0);

  submsg = (SubmessageHeader_t *)(buff + offset);
  if (submsg->submessageId == SMID_SRTPS_PREFIX)
  {
    ddsi_guid_t guid;

    guid.prefix = hdr->guid_prefix;
    guid.entityid.u = NN_ENTITYID_PARTICIPANT;

    GVTRACE(" from "PGUIDFMT, PGUID(guid));

    *proxypp = ephash_lookup_proxy_participant_guid(gv->guid_hash, &guid);
    if (*proxypp)
    {
      if (q_omg_proxyparticipant_is_authenticated(*proxypp))
      {
        ret = NN_RTPS_MSG_STATE_ENCODED;
      }
      else
      {
        GVTRACE ("received encoded rtps message from unauthenticated participant");
      }
    }
    else
    {
      GVTRACE ("received encoded rtps message from unknown participant");
    }
    GVTRACE("\n");
  }
  else
  {
    ret = NN_RTPS_MSG_STATE_PLAIN;
  }

  return ret;
}

nn_rtps_msg_state_t
decode_rtps_message(
  struct thread_state1 * const ts1,
  struct q_globals *gv,
  struct nn_rmsg **rmsg,
  Header_t **hdr,
  unsigned char **buff,
  ssize_t *sz,
  struct nn_rbufpool *rbpool,
  bool isstream)
{
  nn_rtps_msg_state_t ret = NN_RTPS_MSG_STATE_ERROR;
  struct proxy_participant *proxypp = NULL;
  unsigned char *dstbuf;
  unsigned char *srcbuf;
  uint32_t srclen, dstlen;
  bool decoded;

  /* Currently the decode_rtps_message returns a new allocated buffer.
   * This could be optimized by providing a pre-allocated nn_rmsg buffer to
   * copy the decoded rtps message in.
   */
  thread_state_awake_fixed_domain (ts1);
  ret = check_rtps_message_is_secure(gv, *hdr, *buff, isstream, &proxypp);
  if (ret == NN_RTPS_MSG_STATE_ENCODED)
  {
    if (isstream)
    {
      /* Remove MsgLen Submessage which was only needed for a stream to determine the end of the message */
      srcbuf = *buff + sizeof(MsgLen_t);
      srclen = (uint32_t)((size_t)(*sz) - sizeof(MsgLen_t));
      memmove(srcbuf, *buff, RTPS_MESSAGE_HEADER_SIZE);
    }
    else
    {
      srcbuf = *buff;
      srclen = (uint32_t)*sz;
    }

    decoded = q_omg_security_decode_rtps_message(proxypp, srcbuf, srclen, &dstbuf, &dstlen);
    if (decoded)
    {
      nn_rmsg_commit (*rmsg);
      *rmsg = nn_rmsg_new (rbpool);

      *buff = (unsigned char *) NN_RMSG_PAYLOAD (*rmsg);

      memcpy(*buff, dstbuf, dstlen);
      nn_rmsg_setsize (*rmsg, dstlen);

      ddsrt_free(dstbuf);

      *hdr = (Header_t*) *buff;
      (*hdr)->guid_prefix = nn_ntoh_guid_prefix ((*hdr)->guid_prefix);
      *sz = (ssize_t)dstlen;
    } else {
      ret = NN_RTPS_MSG_STATE_ERROR;
    }
  }
  thread_state_asleep (ts1);
  return ret;
}

ssize_t
secure_conn_write(
    ddsi_tran_conn_t conn,
    const nn_locator_t *dst,
    size_t niov,
    const ddsrt_iovec_t *iov,
    uint32_t flags,
    MsgLen_t *msg_len,
    bool dst_one,
    nn_msg_sec_info_t *sec_info,
    ddsi_tran_write_fn_t conn_write_cb)
{
  ssize_t ret = -1;

  unsigned i;
  Header_t *hdr;
  ddsi_guid_t guid;
  unsigned char stbuf[2048];
  unsigned char *srcbuf;
  unsigned char *dstbuf = NULL;
  uint32_t srclen, dstlen;
  int64_t dst_handle = 0;

  assert(iov);
  assert(conn);
  assert(msg_len);
  assert(sec_info);
  assert(niov > 0);
  assert(conn_write_cb);

  if (dst_one)
  {
    dst_handle = sec_info->dst_pp_handle;
    if (dst_handle == 0) {
      return -1;
    }
  }

  hdr = (Header_t *)iov[0].iov_base;
  guid.prefix = nn_ntoh_guid_prefix(hdr->guid_prefix);
  guid.entityid.u = NN_ENTITYID_PARTICIPANT;

  /* first determine the size of the message, then select the
   *  on-stack buffer or allocate one on the heap ...
   */
  srclen = 0;
  for (i = 0; i < (unsigned)niov; i++)
  {
    /* Do not copy MsgLen submessage in case of a stream connection */
    if ((i != 1) || !conn->m_stream)
      srclen += (uint32_t) iov[i].iov_len;
  }
  if (srclen <= sizeof (stbuf))
  {
    srcbuf = stbuf;
  }
  else
  {
    srcbuf = ddsrt_malloc (srclen);
  }

  /* ... then copy data into buffer */
  srclen = 0;
  for (i = 0; i < (unsigned)niov; i++)
  {
    if ((i != 1) || !conn->m_stream)
    {
      memcpy(srcbuf + srclen, iov[i].iov_base, iov[i].iov_len);
      srclen += (uint32_t) iov[i].iov_len;
    }
  }

  if (q_omg_security_encode_rtps_message(sec_info->src_pp_handle, &guid, srcbuf, srclen, &dstbuf, &dstlen, dst_handle))
  {
    ddsrt_iovec_t tmp_iov[3];
    size_t tmp_niov;

    if (conn->m_stream)
    {
      /* Add MsgLen submessage after Header */
      msg_len->length = dstlen + (uint32_t)sizeof(*msg_len);

      tmp_iov[0].iov_base = dstbuf;
      tmp_iov[0].iov_len = RTPS_MESSAGE_HEADER_SIZE;
      tmp_iov[1].iov_base = (void*) msg_len;
      tmp_iov[1].iov_len = sizeof (*msg_len);
      tmp_iov[2].iov_base = dstbuf + RTPS_MESSAGE_HEADER_SIZE;
      tmp_iov[2].iov_len = dstlen - RTPS_MESSAGE_HEADER_SIZE;
      tmp_niov = 3;
    }
    else
    {
      msg_len->length = dstlen;

      tmp_iov[0].iov_base = dstbuf;
      tmp_iov[0].iov_len = dstlen;
      tmp_niov = 1;
    }
    ret = conn_write_cb (conn, dst, tmp_niov, tmp_iov, flags);
  }

  if (srcbuf != stbuf)
  {
    ddsrt_free (srcbuf);
  }

  ddsrt_free(dstbuf);

  return ret;
}

#else /* DDSI_INCLUDE_SECURITY */

#include "dds/ddsi/ddsi_security_omg.h"

extern inline bool q_omg_security_enabled(void);

extern inline bool q_omg_participant_is_secure(
  UNUSED_ARG(const struct participant *pp));

extern inline unsigned determine_subscription_writer(
  UNUSED_ARG(const struct reader *rd));

extern inline unsigned determine_publication_writer(
  UNUSED_ARG(const struct writer *wr));

extern inline bool is_proxy_participant_deletion_allowed(
  UNUSED_ARG(struct q_globals * const gv),
  UNUSED_ARG(const struct ddsi_guid *guid),
  UNUSED_ARG(const ddsi_entityid_t pwr_entityid));

extern inline void set_proxy_participant_security_info(
  UNUSED_ARG(struct proxy_participant *prd),
  UNUSED_ARG(const nn_plist_t *plist));

extern inline void set_proxy_reader_security_info(
  UNUSED_ARG(struct proxy_reader *prd),
  UNUSED_ARG(const nn_plist_t *plist));

extern inline void set_proxy_writer_security_info(
  UNUSED_ARG(struct proxy_writer *pwr),
  UNUSED_ARG(const nn_plist_t *plist));

extern inline bool decode_Data(
  UNUSED_ARG(const struct q_globals *gv),
  UNUSED_ARG(struct nn_rsample_info *sampleinfo),
  UNUSED_ARG(unsigned char *payloadp),
  UNUSED_ARG(uint32_t payloadsz),
  UNUSED_ARG(size_t *submsg_len));

extern inline bool decode_DataFrag(
  UNUSED_ARG(const struct q_globals *gv),
  UNUSED_ARG(struct nn_rsample_info *sampleinfo),
  UNUSED_ARG(unsigned char *payloadp),
  UNUSED_ARG(uint32_t payloadsz),
  UNUSED_ARG(size_t *submsg_len));

extern inline void encode_datareader_submsg(
  UNUSED_ARG(struct nn_xmsg *msg),
  UNUSED_ARG(struct nn_xmsg_marker sm_marker),
  UNUSED_ARG(struct proxy_writer *pwr),
  UNUSED_ARG(const struct ddsi_guid *rd_guid));

extern inline void encode_datawriter_submsg(
  UNUSED_ARG(struct nn_xmsg *msg),
  UNUSED_ARG(struct nn_xmsg_marker sm_marker),
  UNUSED_ARG(struct writer *wr));

extern inline bool validate_msg_decoding(
  UNUSED_ARG(const struct entity_common *e),
  UNUSED_ARG(const struct proxy_endpoint_common *c),
  UNUSED_ARG(struct proxy_participant *proxypp),
  UNUSED_ARG(struct receiver_state *rst),
  UNUSED_ARG(SubmessageKind_t prev_smid));

extern inline int decode_SecPrefix(
  UNUSED_ARG(struct receiver_state *rst),
  UNUSED_ARG(unsigned char *submsg),
  UNUSED_ARG(size_t submsg_size),
  UNUSED_ARG(unsigned char * const msg_end),
  UNUSED_ARG(const ddsi_guid_prefix_t * const src_prefix),
  UNUSED_ARG(const ddsi_guid_prefix_t * const dst_prefix),
  UNUSED_ARG(int byteswap));

extern inline nn_rtps_msg_state_t decode_rtps_message(
  UNUSED_ARG(struct thread_state1 * const ts1),
  UNUSED_ARG(struct q_globals *gv),
  UNUSED_ARG(struct nn_rmsg **rmsg),
  UNUSED_ARG(Header_t **hdr),
  UNUSED_ARG(unsigned char **buff),
  UNUSED_ARG(ssize_t *sz),
  UNUSED_ARG(struct nn_rbufpool *rbpool),
  UNUSED_ARG(bool isstream));

#endif /* DDSI_INCLUDE_SECURITY */
