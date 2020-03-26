/*
 * Copyright(c) 2006 to 2020 ADLINK Technology Limited and others
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
#include <stdio.h>
#include "dds/dds.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "access_control_wrapper.h"

enum ac_plugin_mode {
  PLUGIN_MODE_ALL_OK,
  PLUGIN_MODE_MISSING_FUNC
};

/**
 * Implementation structure for storing encapsulated members of the instance
 * while giving only the interface definition to user
 */
struct dds_security_access_control_impl {
  dds_security_access_control base;
  enum ac_plugin_mode mode;
};

static DDS_Security_PermissionsHandle validate_local_permissions(
    dds_security_access_control *instance,
    const dds_security_authentication *auth_plugin,
    const DDS_Security_IdentityHandle identity,
    const DDS_Security_DomainId domain_id,
    const DDS_Security_Qos *participant_qos,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(auth_plugin);
  DDSRT_UNUSED_ARG(auth_plugin);
  DDSRT_UNUSED_ARG(identity);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(participant_qos);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return 1;
}

static DDS_Security_PermissionsHandle validate_remote_permissions(
    dds_security_access_control *instance,
    const dds_security_authentication *auth_plugin,
    const DDS_Security_IdentityHandle local_identity_handle,
    const DDS_Security_IdentityHandle remote_identity_handle,
    const DDS_Security_PermissionsToken *remote_permissions_token,
    const DDS_Security_AuthenticatedPeerCredentialToken *remote_credential_token,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(auth_plugin);
  DDSRT_UNUSED_ARG(local_identity_handle);
  DDSRT_UNUSED_ARG(remote_identity_handle);
  DDSRT_UNUSED_ARG(remote_permissions_token);
  DDSRT_UNUSED_ARG(remote_credential_token);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return 0;
}

static DDS_Security_boolean check_create_participant( dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const DDS_Security_Qos *participant_qos,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(participant_qos);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean check_create_datawriter( dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const char *topic_name,
    const DDS_Security_Qos *writer_qos,
    const DDS_Security_PartitionQosPolicy *partition,
    const DDS_Security_DataTags *data_tag,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(topic_name);
  DDSRT_UNUSED_ARG(writer_qos);
  DDSRT_UNUSED_ARG(partition);
  DDSRT_UNUSED_ARG(data_tag);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean check_create_datareader( dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const char *topic_name,
    const DDS_Security_Qos *reader_qos,
    const DDS_Security_PartitionQosPolicy *partition,
    const DDS_Security_DataTags *data_tag,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(topic_name);
  DDSRT_UNUSED_ARG(reader_qos);
  DDSRT_UNUSED_ARG(partition);
  DDSRT_UNUSED_ARG(data_tag);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean check_create_topic( dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const char *topic_name,
    const DDS_Security_Qos *qos,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(topic_name);
  DDSRT_UNUSED_ARG(qos);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean check_local_datawriter_register_instance(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_Entity *writer,
    const DDS_Security_DynamicData *key,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(writer);
  DDSRT_UNUSED_ARG(key);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean check_local_datawriter_dispose_instance(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_Entity *writer,
    const DDS_Security_DynamicData key,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(writer);
  DDSRT_UNUSED_ARG(key);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean check_remote_participant( dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const DDS_Security_ParticipantBuiltinTopicDataSecure *participant_data,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(participant_data);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean check_remote_datawriter( dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const DDS_Security_PublicationBuiltinTopicDataSecure *publication_data,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(publication_data);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean check_remote_datareader( dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const DDS_Security_SubscriptionBuiltinTopicDataSecure *subscription_data,
    DDS_Security_boolean *relay_only, DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(subscription_data);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  *relay_only = false;
  return true;
}

static DDS_Security_boolean check_remote_topic( dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const DDS_Security_TopicBuiltinTopicData *topic_data,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(domain_id);
  DDSRT_UNUSED_ARG(topic_data);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean check_local_datawriter_match(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle writer_permissions_handle,
    const DDS_Security_PermissionsHandle reader_permissions_handle,
    const DDS_Security_PublicationBuiltinTopicDataSecure *publication_data,
    const DDS_Security_SubscriptionBuiltinTopicDataSecure *subscription_data,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(writer_permissions_handle);
  DDSRT_UNUSED_ARG(reader_permissions_handle);
  DDSRT_UNUSED_ARG(publication_data);
  DDSRT_UNUSED_ARG(subscription_data);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean check_local_datareader_match(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle reader_permissions_handle,
    const DDS_Security_PermissionsHandle writer_permissions_handle,
    const DDS_Security_SubscriptionBuiltinTopicDataSecure *subscription_data,
    const DDS_Security_PublicationBuiltinTopicDataSecure *publication_data,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(reader_permissions_handle);
  DDSRT_UNUSED_ARG(writer_permissions_handle);
  DDSRT_UNUSED_ARG(subscription_data);
  DDSRT_UNUSED_ARG(publication_data);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean check_remote_datawriter_register_instance(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_Entity *reader,
    const DDS_Security_InstanceHandle publication_handle,
    const DDS_Security_DynamicData key,
    const DDS_Security_InstanceHandle instance_handle,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(reader);
  DDSRT_UNUSED_ARG(publication_handle);
  DDSRT_UNUSED_ARG(key);
  DDSRT_UNUSED_ARG(instance_handle);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean check_remote_datawriter_dispose_instance(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_Entity *reader,
    const DDS_Security_InstanceHandle publication_handle,
    const DDS_Security_DynamicData key,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(reader);
  DDSRT_UNUSED_ARG(publication_handle);
  DDSRT_UNUSED_ARG(key);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean get_permissions_token(
    dds_security_access_control *instance,
    DDS_Security_PermissionsToken *permissions_token,
    const DDS_Security_PermissionsHandle handle,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(handle);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  memset(permissions_token, 0, sizeof(*permissions_token));
  permissions_token->class_id = ddsrt_strdup("");
  return true;
}

static DDS_Security_boolean get_permissions_credential_token(
    dds_security_access_control *instance,
    DDS_Security_PermissionsCredentialToken *permissions_credential_token,
    const DDS_Security_PermissionsHandle handle,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_credential_token);
  DDSRT_UNUSED_ARG(handle);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean set_listener( dds_security_access_control *instance,
    const dds_security_access_control_listener *listener,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(listener);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean return_permissions_token( dds_security_access_control *instance,
    const DDS_Security_PermissionsToken *token,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  ddsrt_free (token->class_id);
  return true;
}

static DDS_Security_boolean return_permissions_credential_token(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsCredentialToken *permissions_credential_token,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_credential_token);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean get_participant_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    DDS_Security_ParticipantSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(attributes);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean get_topic_sec_attributes( dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const char *topic_name,
    DDS_Security_TopicSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(topic_name);
  DDSRT_UNUSED_ARG(attributes);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean get_datawriter_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const char *topic_name,
    const DDS_Security_PartitionQosPolicy *partition,
    const DDS_Security_DataTagQosPolicy *data_tag,
    DDS_Security_EndpointSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(topic_name);
  DDSRT_UNUSED_ARG(partition);
  DDSRT_UNUSED_ARG(data_tag);
  DDSRT_UNUSED_ARG(attributes);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean get_datareader_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const char *topic_name,
    const DDS_Security_PartitionQosPolicy *partition,
    const DDS_Security_DataTagQosPolicy *data_tag,
    DDS_Security_EndpointSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(topic_name);
  DDSRT_UNUSED_ARG(partition);
  DDSRT_UNUSED_ARG(data_tag);
  DDSRT_UNUSED_ARG(attributes);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean return_participant_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_ParticipantSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(attributes);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean return_topic_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_TopicSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(attributes);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean return_datawriter_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_EndpointSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(attributes);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean return_datareader_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_EndpointSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(attributes);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}

static DDS_Security_boolean return_permissions_handle(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  return true;
}


static struct dds_security_access_control_impl * init_test_access_control_common()
{
  struct dds_security_access_control_impl *impl = ddsrt_malloc(sizeof(*impl));
  memset(impl, 0, sizeof(*impl));
  impl->base.validate_local_permissions = &validate_local_permissions;
  impl->base.validate_remote_permissions = &validate_remote_permissions;
  impl->base.check_create_participant = &check_create_participant;
  impl->base.check_create_datawriter = &check_create_datawriter;
  impl->base.check_create_datareader = &check_create_datareader;
  impl->base.check_create_topic = &check_create_topic;
  impl->base.check_local_datawriter_register_instance = &check_local_datawriter_register_instance;
  impl->base.check_local_datawriter_dispose_instance = &check_local_datawriter_dispose_instance;
  impl->base.check_remote_participant = &check_remote_participant;
  impl->base.check_remote_datawriter = &check_remote_datawriter;
  impl->base.check_remote_datareader = &check_remote_datareader;
  impl->base.check_remote_topic = &check_remote_topic;
  impl->base.check_local_datawriter_match = &check_local_datawriter_match;
  impl->base.check_local_datareader_match = &check_local_datareader_match;
  impl->base.check_remote_datawriter_register_instance = &check_remote_datawriter_register_instance;
  impl->base.check_remote_datawriter_dispose_instance = &check_remote_datawriter_dispose_instance;
  impl->base.get_permissions_token = &get_permissions_token;
  impl->base.get_permissions_credential_token = &get_permissions_credential_token;
  impl->base.set_listener = &set_listener;
  impl->base.return_permissions_token = &return_permissions_token;
  impl->base.return_permissions_credential_token = &return_permissions_credential_token;
  impl->base.get_participant_sec_attributes = &get_participant_sec_attributes;
  impl->base.get_topic_sec_attributes = &get_topic_sec_attributes;
  impl->base.get_datawriter_sec_attributes = &get_datawriter_sec_attributes;
  impl->base.get_datareader_sec_attributes = &get_datareader_sec_attributes;
  impl->base.return_participant_sec_attributes = &return_participant_sec_attributes;
  impl->base.return_topic_sec_attributes = &return_topic_sec_attributes;
  impl->base.return_datawriter_sec_attributes = &return_datawriter_sec_attributes;
  impl->base.return_datareader_sec_attributes = &return_datareader_sec_attributes;
  impl->base.return_permissions_handle = &return_permissions_handle;
  return impl;
}

int32_t init_test_access_control_all_ok(const char *argument, void **context, struct ddsi_domaingv *gv)
{
  DDSRT_UNUSED_ARG(argument);
  DDSRT_UNUSED_ARG(context);
  DDSRT_UNUSED_ARG(gv);
  struct dds_security_access_control_impl *impl = init_test_access_control_common();
  impl->mode = PLUGIN_MODE_ALL_OK;
  *context = impl;
  return 0;
}

int32_t finalize_test_access_control_all_ok(void *context)
{
  assert(((struct dds_security_access_control_impl *)context)->mode == PLUGIN_MODE_ALL_OK);
  ddsrt_free(context);
  return 0;
}

int32_t init_test_access_control_missing_func(const char *argument, void **context, struct ddsi_domaingv *gv)
{
  DDSRT_UNUSED_ARG(argument);
  DDSRT_UNUSED_ARG(context);
  DDSRT_UNUSED_ARG(gv);
  struct dds_security_access_control_impl *impl = init_test_access_control_common();
  impl->base.check_create_datareader = NULL;
  impl->mode = PLUGIN_MODE_MISSING_FUNC;
  *context = impl;
  return 0;
}

int32_t finalize_test_access_control_missing_func(void *context)
{
  assert(((struct dds_security_access_control_impl *)context)->mode == PLUGIN_MODE_MISSING_FUNC);
  ddsrt_free(context);
  return 0;
}

