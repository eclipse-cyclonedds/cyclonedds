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
#ifndef SECURITY_ACCESS_CONTROL_ALLOK_H_
#define SECURITY_BUILTIN_PLUGINS_AUTHENTICATION_H_

#include "dds/security/access_control_missing_function_export.h"
#include "dds/security/dds_security_api.h"

SECURITY_EXPORT int32_t
init_access_control(const char *argument, void **context);

SECURITY_EXPORT int32_t
finalize_access_control(void *context);


/**
 * AccessControl Interface
 */

DDS_Security_PermissionsHandle
validate_local_permissions
        ( dds_security_access_control *instance,
          const dds_security_authentication *auth_plugin,
          const DDS_Security_IdentityHandle identity,
          const DDS_Security_DomainId domain_id,
          const DDS_Security_Qos *participant_qos,
          DDS_Security_SecurityException *ex);

DDS_Security_PermissionsHandle
validate_remote_permissions
        ( dds_security_access_control *instance,
          const dds_security_authentication *auth_plugin,
          const DDS_Security_IdentityHandle local_identity_handle,
          const DDS_Security_IdentityHandle remote_identity_handle,
          const DDS_Security_PermissionsToken *remote_permissions_token,
          const DDS_Security_AuthenticatedPeerCredentialToken *remote_credential_token,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
check_create_participant
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_DomainId domain_id,
          const DDS_Security_Qos *participant_qos,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
check_create_datawriter
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_DomainId domain_id,
          const DDS_Security_char *topic_name,
          const DDS_Security_Qos *writer_qos,
          const DDS_Security_PartitionQosPolicy *partition,
          const DDS_Security_DataTags *data_tag,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
check_create_datareader
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_DomainId domain_id,
          const DDS_Security_char *topic_name,
          const DDS_Security_Qos *reader_qos,
          const DDS_Security_PartitionQosPolicy *partition,
          const DDS_Security_DataTags *data_tag,
          DDS_Security_SecurityException *ex);


DDS_Security_boolean
check_create_topic
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_DomainId domain_id,
          const DDS_Security_char *topic_name,
          const DDS_Security_Qos *topic_qos,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
check_local_datawriter_register_instance
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_Entity *writer,
          const DDS_Security_DynamicData *key,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
check_local_datawriter_dispose_instance
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_Entity *writer,
          const DDS_Security_DynamicData key,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
check_remote_participant
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_DomainId domain_id,
          const DDS_Security_ParticipantBuiltinTopicDataSecure *participant_data,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
check_remote_datawriter
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_DomainId domain_id,
          const DDS_Security_PublicationBuiltinTopicDataSecure *publication_data,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
check_remote_datareader
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_DomainId domain_id,
          const DDS_Security_SubscriptionBuiltinTopicDataSecure *subscription_data,
          DDS_Security_boolean *relay_only,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
check_remote_topic
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_DomainId domain_id,
          const DDS_Security_TopicBuiltinTopicData *topic_data,
          DDS_Security_SecurityException *ex);


DDS_Security_boolean
check_local_datawriter_match
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle writer_permissions_handle,
          const DDS_Security_PermissionsHandle reader_permissions_handle,
          const DDS_Security_PublicationBuiltinTopicDataSecure *publication_data,
          const DDS_Security_SubscriptionBuiltinTopicDataSecure *subscription_data,
          DDS_Security_SecurityException *ex);


DDS_Security_boolean
check_local_datareader_match
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle reader_permissions_handle,
          const DDS_Security_PermissionsHandle writer_permissions_handle,
          const DDS_Security_SubscriptionBuiltinTopicDataSecure *subscription_data,
          const DDS_Security_PublicationBuiltinTopicDataSecure *publication_data,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
check_remote_datawriter_register_instance
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_Entity *reader,
          const DDS_Security_InstanceHandle publication_handle,
          const DDS_Security_DynamicData key,
          const DDS_Security_InstanceHandle instance_handle,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
check_remote_datawriter_dispose_instance
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_Entity *reader,
          const DDS_Security_InstanceHandle publication_handle,
          const DDS_Security_DynamicData key,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
get_permissions_token
        ( dds_security_access_control *instance,
          DDS_Security_PermissionsToken *permissions_token,
          const DDS_Security_PermissionsHandle handle,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
get_permissions_credential_token
        ( dds_security_access_control *instance,
          DDS_Security_PermissionsCredentialToken *permissions_credential_token,
          const DDS_Security_PermissionsHandle handle,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
set_listener
        ( dds_security_access_control *instance,
          const dds_security_access_control_listener *listener,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
return_permissions_token
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsToken *token,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
return_permissions_credential_token
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsCredentialToken *permissions_credential_token,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
get_participant_sec_attributes
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          DDS_Security_ParticipantSecurityAttributes *attributes,
          DDS_Security_SecurityException *ex);


DDS_Security_boolean
get_topic_sec_attributes
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_char *topic_name,
          DDS_Security_TopicSecurityAttributes *attributes,
          DDS_Security_SecurityException *ex);


DDS_Security_boolean
get_datawriter_sec_attributes
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_char *topic_name,
          const DDS_Security_PartitionQosPolicy *partition,
          const DDS_Security_DataTagQosPolicy *data_tag,
          DDS_Security_EndpointSecurityAttributes *attributes,
          DDS_Security_SecurityException *ex);


DDS_Security_boolean
get_datareader_sec_attributes
        ( dds_security_access_control *instance,
          const DDS_Security_PermissionsHandle permissions_handle,
          const DDS_Security_char *topic_name,
          const DDS_Security_PartitionQosPolicy *partition,
          const DDS_Security_DataTagQosPolicy *data_tag,
          DDS_Security_EndpointSecurityAttributes *attributes,
          DDS_Security_SecurityException *ex);


DDS_Security_boolean
return_participant_sec_attributes
        ( dds_security_access_control *instance,
          const DDS_Security_ParticipantSecurityAttributes *attributes,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
return_topic_sec_attributes
        ( dds_security_access_control *instance,
          const DDS_Security_TopicSecurityAttributes *attributes,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
return_datawriter_sec_attributes
        ( dds_security_access_control *instance,
          const DDS_Security_EndpointSecurityAttributes *attributes,
          DDS_Security_SecurityException *ex);


DDS_Security_boolean
return_datareader_sec_attributes
        ( dds_security_access_control *instance,
          const DDS_Security_EndpointSecurityAttributes *attributes,
          DDS_Security_SecurityException *ex);

DDS_Security_boolean
return_permissions_handle
        ( dds_security_access_control *instance,
          const  DDS_Security_PermissionsHandle permissions_handle,
          DDS_Security_SecurityException *ex);



#endif /* SECURITY_BUILTIN_PLUGINS_AUTHENTICATION_H_ */
