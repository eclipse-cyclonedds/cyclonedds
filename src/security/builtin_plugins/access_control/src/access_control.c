// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#define ACCESS_CONTROL_USE_ONE_PERMISSION

#include <assert.h>
#include <string.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/misc.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/core/dds_security_timed_cb.h"
#include "dds/security/openssl_support.h"
#include "access_control.h"
#include "access_control_utils.h"
#include "access_control_objects.h"
#include "access_control_parser.h"
#include "ac_tokens.h"

typedef enum TOPIC_TYPE
{
  TOPIC_TYPE_USER = 0,
  TOPIC_TYPE_NON_SECURE_BUILTIN,
  TOPIC_TYPE_SECURE_ParticipantsSecure,
  TOPIC_TYPE_SECURE_PublicationsSecure,
  TOPIC_TYPE_SECURE_SubscriptionsSecure,
  TOPIC_TYPE_SECURE_ParticipantMessageSecure,
  TOPIC_TYPE_SECURE_ParticipantStatelessMessage,
  TOPIC_TYPE_SECURE_ParticipantVolatileMessageSecure
} TOPIC_TYPE;

/**
 * Implementation structure for storing encapsulated members of the instance
 * while giving only the interface definition to user
 */

typedef struct dds_security_access_control_impl
{
  dds_security_access_control base;
  ddsrt_mutex_t lock;

#ifdef ACCESS_CONTROL_USE_ONE_PERMISSION
  local_participant_access_rights *local_access_rights;
#else
  /* TODO: implement access rights per participant */
  struct AccessControlTable *local_permissions;
#endif
  struct AccessControlTable *remote_permissions;
  struct dds_security_timed_dispatcher *dispatcher;
  const dds_security_access_control_listener *listener;
} dds_security_access_control_impl;

static bool get_sec_attributes(dds_security_access_control_impl *ac, const DDS_Security_PermissionsHandle permissions_handle, const char *topic_name,
    DDS_Security_EndpointSecurityAttributes *attributes, DDS_Security_SecurityException *ex);
static local_participant_access_rights *check_and_create_local_participant_rights(DDS_Security_IdentityHandle identity_handle, int domain_id, const DDS_Security_Qos *participant_qos, DDS_Security_SecurityException *ex);
static remote_participant_access_rights *check_and_create_remote_participant_rights(DDS_Security_IdentityHandle remote_identity_handle, local_participant_access_rights *local_rights,
    const DDS_Security_PermissionsToken *remote_permissions_token, const DDS_Security_AuthenticatedPeerCredentialToken *remote_credential_token, DDS_Security_SecurityException *ex);
static local_participant_access_rights *find_local_access_rights(dds_security_access_control_impl *ac, DDS_Security_PermissionsHandle handle);
static local_participant_access_rights *find_local_rights_by_identity(dds_security_access_control_impl *ac, DDS_Security_IdentityHandle identity_handle);
static remote_participant_access_rights *find_remote_rights_by_identity(dds_security_access_control_impl *ac, DDS_Security_IdentityHandle identity_handle);
static DDS_Security_boolean domainid_within_sets(struct domain_id_set *domain, int domain_id);
static DDS_Security_boolean is_topic_in_criteria(const struct criteria *criteria, const char *topic_name);
static DDS_Security_boolean is_partition_qos_in_criteria(const struct criteria *criteria, const DDS_Security_PartitionQosPolicy *partitions, permission_rule_type rule_type) ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;
static DDS_Security_boolean is_partition_in_criteria(const struct criteria *criteria, const char *partition_name);
static struct domain_rule *find_domain_rule_in_governance(struct domain_rule *rule, int domain_id);
static DDS_Security_boolean get_participant_sec_attributes(dds_security_access_control *instance, const DDS_Security_PermissionsHandle permissions_handle,
    DDS_Security_ParticipantSecurityAttributes *attributes, DDS_Security_SecurityException *ex);
static DDS_Security_boolean get_permissions_token(dds_security_access_control *instance, DDS_Security_PermissionsToken *permissions_token, const DDS_Security_PermissionsHandle handle, DDS_Security_SecurityException *ex);
static remote_participant_access_rights *find_remote_permissions_by_permissions_handle(dds_security_access_control_impl *ac, DDS_Security_PermissionsHandle permissions_handle);
static struct topic_rule *find_topic_from_domain_rule(struct domain_rule *domain_rule, const char *topic_name);
static DDS_Security_boolean domainid_within_sets(struct domain_id_set *domain, int domain_id);
static DDS_Security_boolean compare_class_id_plugin_classname(DDS_Security_string class_id_1, DDS_Security_string class_id_2);
static DDS_Security_boolean compare_class_id_major_ver(DDS_Security_string class_id_1, DDS_Security_string class_id_2);
static dds_security_time_event_handle_t add_validity_end_trigger(dds_security_access_control_impl *ac, const DDS_Security_PermissionsHandle permissions_handle, dds_time_t end);
static bool is_participant_allowed_by_permissions (const struct permissions_parser *permissions, int domain_id, const char *identity_subject_name, DDS_Security_SecurityException *ex) ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;
static bool is_topic_allowed_by_permissions (const struct permissions_parser *permissions, int domain_id, const char *topic_name, const char *identity_subject_name, DDS_Security_SecurityException *ex) ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;
static bool is_readwrite_allowed_by_permissions (struct permissions_parser *permissions, int domain_id, const char *topic_name, const DDS_Security_PartitionQosPolicy *partitions, const char *identity_subject_name, permission_criteria_type criteria_type, DDS_Security_SecurityException *ex) ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;
static void sanity_check_local_access_rights(local_participant_access_rights *rights);
static void sanity_check_remote_access_rights(remote_participant_access_rights *rights);
static TOPIC_TYPE get_topic_type(const char *topic_name);


static DDS_Security_PermissionsHandle
validate_local_permissions(
    dds_security_access_control *instance,
    const dds_security_authentication *auth_plugin,
    const DDS_Security_IdentityHandle identity_handle,
    const DDS_Security_DomainId domain_id,
    const DDS_Security_Qos *participant_qos,
    DDS_Security_SecurityException *ex)
{
  dds_security_access_control_impl *ac = (dds_security_access_control_impl *)instance;
  local_participant_access_rights *rights;
  DDS_Security_PermissionsHandle permissions_handle = DDS_SECURITY_HANDLE_NIL;

  if (!instance || !auth_plugin || identity_handle == DDS_SECURITY_HANDLE_NIL || !participant_qos)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return DDS_SECURITY_HANDLE_NIL;
  }

#ifdef ACCESS_CONTROL_USE_ONE_PERMISSION
  ddsrt_mutex_lock(&ac->lock);
  if (ac->local_access_rights == NULL)
  {
    rights = check_and_create_local_participant_rights(identity_handle, domain_id, participant_qos, ex);
    ac->local_access_rights = rights;
  }
  else
  {
    ACCESS_CONTROL_OBJECT_KEEP(ac->local_access_rights);
    rights = ac->local_access_rights;
  }
  ddsrt_mutex_unlock(&ac->lock);
#else
  {
    local_participant_access_rights *existing = find_local_rights_by_identity(ac, identity_handle);
    if (existing)
    {
      ACCESS_CONTROL_OBJECT_RELEASE(existing);
      return ACCESS_CONTROL_OBJECT_HANDLE(existing);
    }

    rights = check_and_create_local_participant_rights(identity_handle, domain_id, participant_qos, ex);
    if (rights)
      access_control_table_insert(ac->local_permissions, (AccessControlObject *)rights);
  }
#endif

  if ((permissions_handle = ACCESS_CONTROL_OBJECT_HANDLE(rights)) != DDS_SECURITY_HANDLE_NIL)
  {
    assert (rights->_parent.permissions_expiry != DDS_TIME_INVALID);
    if (rights->_parent.permissions_expiry != 0)
      rights->_parent.timer = add_validity_end_trigger(ac, permissions_handle, rights->_parent.permissions_expiry);
  }

  return permissions_handle;
}

static DDS_Security_PermissionsHandle
validate_remote_permissions(
    dds_security_access_control *instance,
    const dds_security_authentication *auth_plugin,
    const DDS_Security_IdentityHandle local_identity_handle,
    const DDS_Security_IdentityHandle remote_identity_handle,
    const DDS_Security_PermissionsToken *remote_permissions_token,
    const DDS_Security_AuthenticatedPeerCredentialToken *remote_credential_token,
    DDS_Security_SecurityException *ex)
{
  dds_security_access_control_impl *ac = (dds_security_access_control_impl *)instance;
  local_participant_access_rights *local_rights;
  remote_participant_access_rights *remote_rights, *existing;
  DDS_Security_PermissionsHandle permissions_handle = DDS_SECURITY_HANDLE_NIL;

  if (!instance || !auth_plugin || local_identity_handle == DDS_SECURITY_HANDLE_NIL || remote_identity_handle == DDS_SECURITY_HANDLE_NIL ||
      !remote_permissions_token || !remote_permissions_token->class_id || !remote_credential_token)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return DDS_SECURITY_HANDLE_NIL;
  }

  if (!(local_rights = find_local_rights_by_identity(ac, local_identity_handle)))
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return DDS_SECURITY_HANDLE_NIL;
  }

  if ((existing = find_remote_rights_by_identity(ac, remote_identity_handle)))
  {
    if (existing->local_rights->local_identity == local_identity_handle)
    {
      ACCESS_CONTROL_OBJECT_RELEASE(existing);
      return ACCESS_CONTROL_OBJECT_HANDLE(existing);
    }
  }

#ifdef ACCESS_CONTROL_USE_ONE_PERMISSION
  if (existing)
  {
    /* No check because it has already been checked */
    remote_rights = ac_remote_participant_access_rights_new(remote_identity_handle, local_rights, existing->permissions, existing->_parent.permissions_expiry, remote_permissions_token, existing->identity_subject_name);
    sanity_check_remote_access_rights(remote_rights);
    /* TODO: copy or relate security attributes of existing with new remote permissions object */
  }
  else
  {
    remote_rights = check_and_create_remote_participant_rights(remote_identity_handle, local_rights, remote_permissions_token, remote_credential_token, ex);
  }
#else
  remote_rights = check_and_create_remote_participant_rights(remote_identity_handle, local_rights, remote_permissions_token, remote_credential_token, ex);
#endif

  if ((permissions_handle = ACCESS_CONTROL_OBJECT_HANDLE(remote_rights)) != DDS_SECURITY_HANDLE_NIL)
  {
    assert (remote_rights->_parent.permissions_expiry != DDS_TIME_INVALID);
    if (remote_rights->_parent.permissions_expiry != 0)
      remote_rights->_parent.timer = add_validity_end_trigger(ac, permissions_handle, remote_rights->_parent.permissions_expiry);
  }

  if (remote_rights)
    access_control_table_insert(ac->remote_permissions, (AccessControlObject *)remote_rights);

  ACCESS_CONTROL_OBJECT_RELEASE(existing);
  ACCESS_CONTROL_OBJECT_RELEASE(remote_rights);
  ACCESS_CONTROL_OBJECT_RELEASE(local_rights);

  return permissions_handle;
}

static DDS_Security_boolean
check_create_participant(dds_security_access_control *instance,
                         const DDS_Security_PermissionsHandle permissions_handle,
                         const DDS_Security_DomainId domain_id,
                         const DDS_Security_Qos *participant_qos,
                         DDS_Security_SecurityException *ex)
{
  dds_security_access_control_impl *ac = (dds_security_access_control_impl *)instance;
  local_participant_access_rights *rights;
  struct domain_rule *domainRule = NULL;
  struct topic_rule *topicRule = NULL;
  DDS_Security_ParticipantSecurityAttributes participantSecurityAttributes;
  DDS_Security_boolean result = false;

  if (instance == NULL || permissions_handle == DDS_SECURITY_HANDLE_NIL || participant_qos == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }

  /* Retrieve rights */
  if ((rights = find_local_access_rights(ac, permissions_handle)) == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "Could not find local rights for the participant.");
    return false;
  }

  /* Retrieve domain rules */
  domainRule = find_domain_rule_in_governance(rights->governance_tree->dds->domain_access_rules->domain_rule, domain_id);
  if (domainRule == NULL || domainRule->topic_access_rules == NULL || domainRule->topic_access_rules->topic_rule == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_FIND_DOMAIN_IN_GOVERNANCE_CODE, 0, DDS_SECURITY_ERR_CAN_NOT_FIND_DOMAIN_IN_GOVERNANCE_MESSAGE, domain_id);
    goto exit;
  }

  /* Iterate over topics rules*/
  topicRule = domainRule->topic_access_rules->topic_rule;
  while (topicRule != NULL)
  {
    if (!topicRule->enable_read_access_control->value || !topicRule->enable_write_access_control->value)
    {
      /* Governance specifies any topics on the DomainParticipant
         domain_id with enable_read_access_control set to false or with enable_write_access_control set to false */
      result = true;
      goto exit;
    }
    topicRule = (struct topic_rule *)topicRule->node.next;
  }

  if (!get_participant_sec_attributes(instance, permissions_handle, &participantSecurityAttributes, ex))
    goto exit;

  if (!participantSecurityAttributes.is_access_protected)
  {
    result = true;
    goto exit;
  }

  /* Is this participant permitted? */
  result = is_participant_allowed_by_permissions(rights->permissions_tree, domain_id, rights->identity_subject_name, ex);

exit:
  ACCESS_CONTROL_OBJECT_RELEASE(rights);
  return result;
}

static DDS_Security_boolean
check_create_datawriter(dds_security_access_control *instance,
                        const DDS_Security_PermissionsHandle permissions_handle,
                        const DDS_Security_DomainId domain_id, const char *topic_name,
                        const DDS_Security_Qos *writer_qos,
                        const DDS_Security_PartitionQosPolicy *partition,
                        const DDS_Security_DataTags *data_tag,
                        DDS_Security_SecurityException *ex)
{
  DDS_Security_TopicSecurityAttributes topic_sec_attr;
  local_participant_access_rights *local_rights;
  DDS_Security_boolean result = false;
  DDSRT_UNUSED_ARG(data_tag);

  if (instance == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "Plugin instance not provided");
    return false;
  }
  if (permissions_handle == DDS_SECURITY_HANDLE_NIL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "Permissions handle not provided");
    return false;
  }
  if (topic_name == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "Topic name not provided");
    return false;
  }
  if (writer_qos == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "QoS not provided");
    return false;
  }
  if (partition == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "Partition not provided");
    return false;
  }
  if ((local_rights = find_local_access_rights((dds_security_access_control_impl *)instance, permissions_handle)) == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "Could not find rights material");
    return false;
  }
  if (local_rights->domain_id != domain_id)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0,
        "Given domain_id (%d) does not match the related participant domain_id (%d)\n", domain_id, local_rights->domain_id);
    goto exit;
  }

  /* Find a topic with the specified topic name in the Governance */
  if (!(result = instance->get_topic_sec_attributes(instance, permissions_handle, topic_name, &topic_sec_attr, ex)))
    goto exit;

  if (!topic_sec_attr.is_write_protected)
  {
    result = true;
    goto exit;
  }

  /* Find a topic with the specified topic name in the Governance */
  result = is_readwrite_allowed_by_permissions(local_rights->permissions_tree, domain_id, topic_name, partition, local_rights->identity_subject_name, PUBLISH_CRITERIA, ex);

exit:
  ACCESS_CONTROL_OBJECT_RELEASE(local_rights);
  return result;
}

static DDS_Security_boolean
check_create_datareader(dds_security_access_control *instance,
                        const DDS_Security_PermissionsHandle permissions_handle,
                        const DDS_Security_DomainId domain_id,
                        const char *topic_name,
                        const DDS_Security_Qos *reader_qos,
                        const DDS_Security_PartitionQosPolicy *partition,
                        const DDS_Security_DataTags *data_tag,
                        DDS_Security_SecurityException *ex)
{
  DDS_Security_TopicSecurityAttributes topic_sec_attr;
  local_participant_access_rights *local_rights;
  DDS_Security_boolean result = false;

  DDSRT_UNUSED_ARG(data_tag);

  if (instance == NULL || permissions_handle == DDS_SECURITY_HANDLE_NIL || topic_name == NULL || reader_qos == NULL || partition == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }
  if ((local_rights = find_local_access_rights((dds_security_access_control_impl *)instance, permissions_handle)) == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }
  if (local_rights->domain_id != domain_id)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0,
        "Given domain_id (%d) does not match the related participant domain_id (%d)\n", domain_id, local_rights->domain_id);
    goto exit;
  }

  /* Find a topic with the specified topic name in the Governance */
  if ((result = instance->get_topic_sec_attributes(instance, permissions_handle, topic_name, &topic_sec_attr, ex)) == false)
    goto exit;

  if (topic_sec_attr.is_read_protected == false)
  {
    result = true;
    goto exit;
  }

  /* Find a topic with the specified topic name in the Governance */
  result = is_readwrite_allowed_by_permissions(local_rights->permissions_tree, domain_id, topic_name, partition, local_rights->identity_subject_name, SUBSCRIBE_CRITERIA, ex);

exit:
  ACCESS_CONTROL_OBJECT_RELEASE(local_rights);
  return result;
}

static DDS_Security_boolean
check_create_topic(dds_security_access_control *instance,
                   const DDS_Security_PermissionsHandle permissions_handle,
                   const DDS_Security_DomainId domain_id, const char *topic_name,
                   const DDS_Security_Qos *qos, DDS_Security_SecurityException *ex)
{
  DDS_Security_TopicSecurityAttributes topic_sec_attr;
  local_participant_access_rights *local_rights;
  DDS_Security_boolean result = false;

  if (instance == NULL || permissions_handle == DDS_SECURITY_HANDLE_NIL || qos == NULL || topic_name == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }
  if ((local_rights = find_local_access_rights((dds_security_access_control_impl *)instance, permissions_handle)) == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }
  if (local_rights->domain_id != domain_id)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0,
        "Given domain_id (%d) does not match the related participant domain_id (%d)\n", domain_id, local_rights->domain_id);
    goto exit;
  }

  /* Find a topic with the specified topic name in the Governance */
  if ((result = instance->get_topic_sec_attributes(instance, permissions_handle, topic_name, &topic_sec_attr, ex)) == false)
    goto exit;

  if (topic_sec_attr.is_read_protected == false || topic_sec_attr.is_write_protected == false)
  {
    result = true;
    goto exit;
  }

  /* Find a topic with the specified topic name in the Governance */
  result = is_topic_allowed_by_permissions(local_rights->permissions_tree, domain_id, topic_name, local_rights->identity_subject_name, ex);

exit:
  ACCESS_CONTROL_OBJECT_RELEASE(local_rights);
  return result;
}

static DDS_Security_boolean
check_local_datawriter_register_instance(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_Entity *writer, const DDS_Security_DynamicData *key,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(writer);
  DDSRT_UNUSED_ARG(key);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  /* Not implemented */
  return true;
}

static DDS_Security_boolean
check_local_datawriter_dispose_instance(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_Entity *writer, const DDS_Security_DynamicData key,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(permissions_handle);
  DDSRT_UNUSED_ARG(writer);
  DDSRT_UNUSED_ARG(key);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);

  /* Not implemented */
  return true;
}

static DDS_Security_boolean
check_remote_participant(dds_security_access_control *instance,
                         const DDS_Security_PermissionsHandle permissions_handle,
                         const DDS_Security_DomainId domain_id,
                         const DDS_Security_ParticipantBuiltinTopicDataSecure *participant_data,
                         DDS_Security_SecurityException *ex)
{
  dds_security_access_control_impl *ac = (dds_security_access_control_impl *)instance;
  remote_participant_access_rights *remote_rights = NULL;
  DDS_Security_ParticipantSecurityAttributes participantSecurityAttributes;
  DDS_Security_PermissionsHandle local_permissions_handle;
  DDS_Security_string class_id_remote_str;
  DDS_Security_boolean result = false;

  if (instance == NULL || permissions_handle == DDS_SECURITY_HANDLE_NIL || participant_data == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }

  /* retrieve the cached remote DomainParticipant Governance; the permissions_handle is associated with the remote participant */
  if ((remote_rights = find_remote_permissions_by_permissions_handle(ac, permissions_handle)) == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }

  /* The local rights pointer is actually the local permissions handle. */
  local_permissions_handle = ACCESS_CONTROL_OBJECT_HANDLE(remote_rights->local_rights);
  if (!get_participant_sec_attributes(instance, local_permissions_handle, &participantSecurityAttributes, ex))
    goto exit;
  if (participantSecurityAttributes.is_access_protected == false)
  {
    result = true;
    goto exit;
  }

  /* 2) If the PluginClassName or the MajorVersion of the local permissions_token differ from those in the remote_permissions_token,
       the operation shall return false. */
  class_id_remote_str = remote_rights->permissions->remote_permissions_token_class_id;
  if (compare_class_id_plugin_classname(class_id_remote_str, DDS_ACTOKEN_PERMISSIONS_CLASS_ID) == false)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_CLASSNAME_CODE, 0, DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_CLASSNAME_MESSAGE);
    goto exit;
  }
  if (compare_class_id_major_ver(class_id_remote_str, DDS_ACTOKEN_PERMISSIONS_CLASS_ID) == false)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_MAJORVERSION_CODE, 0, DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_MAJORVERSION_MESSAGE);
    goto exit;
  }

  /* 3) If the Permissions document contains a Grant for the remote DomainParticipant and the Grant contains an allow rule on
       the DomainParticipant domain_id, then the  operation shall succeed and return true. */
  /* Iterate over the grants and rules of the remote participant */
  result = is_participant_allowed_by_permissions(remote_rights->permissions->permissions_tree, domain_id, remote_rights->identity_subject_name, ex);

exit:
  ACCESS_CONTROL_OBJECT_RELEASE(remote_rights);
  return result;
}

static DDS_Security_boolean
check_remote_datawriter(dds_security_access_control *instance,
                        const DDS_Security_PermissionsHandle permissions_handle,
                        const DDS_Security_DomainId domain_id,
                        const DDS_Security_PublicationBuiltinTopicDataSecure *publication_data,
                        DDS_Security_SecurityException *ex)
{
  dds_security_access_control_impl *ac = (dds_security_access_control_impl *)instance;
  DDS_Security_TopicSecurityAttributes topic_sec_attr;
  remote_participant_access_rights *remote_rights;
  DDS_Security_string class_id_remote_str;
  DDS_Security_boolean result = false;

  if (instance == NULL || permissions_handle == DDS_SECURITY_HANDLE_NIL || publication_data == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }
  if ((remote_rights = find_remote_permissions_by_permissions_handle(ac, permissions_handle)) == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }
  if ((result = instance->get_topic_sec_attributes(instance, ACCESS_CONTROL_OBJECT_HANDLE(remote_rights->local_rights), publication_data->topic_name, &topic_sec_attr, ex)) == false)
    goto exit;
  if (topic_sec_attr.is_write_protected == false)
  {
    result = true;
    goto exit;
  }

  /* Compare PluginClassName and MajorVersion parts */
  class_id_remote_str = remote_rights->permissions->remote_permissions_token_class_id;
  if (compare_class_id_plugin_classname(class_id_remote_str, DDS_ACTOKEN_PERMISSIONS_CLASS_ID) == false)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_CLASSNAME_CODE, 0,
        DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_CLASSNAME_MESSAGE);
    goto exit;
  }
  if (compare_class_id_major_ver(class_id_remote_str, DDS_ACTOKEN_PERMISSIONS_CLASS_ID) == false)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_MAJORVERSION_CODE, 0,
        DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_MAJORVERSION_MESSAGE);
    goto exit;
  }

  /* Find a topic with the specified topic name in the Governance */
  result = is_readwrite_allowed_by_permissions(remote_rights->permissions->permissions_tree, domain_id, publication_data->topic_name,
      &publication_data->partition, remote_rights->identity_subject_name, PUBLISH_CRITERIA, ex);

exit:
  ACCESS_CONTROL_OBJECT_RELEASE(remote_rights);
  return result;
}

static DDS_Security_boolean
check_remote_datareader(dds_security_access_control *instance,
                        const DDS_Security_PermissionsHandle permissions_handle,
                        const DDS_Security_DomainId domain_id,
                        const DDS_Security_SubscriptionBuiltinTopicDataSecure *subscription_data,
                        DDS_Security_boolean *relay_only,
                        DDS_Security_SecurityException *ex)
{
  dds_security_access_control_impl *ac = (dds_security_access_control_impl *)instance;
  DDS_Security_TopicSecurityAttributes topic_sec_attr;
  remote_participant_access_rights *remote_rights;
  DDS_Security_string class_id_remote_str;

  DDS_Security_boolean result = false;

  if (instance == NULL || permissions_handle == DDS_SECURITY_HANDLE_NIL || subscription_data == NULL || relay_only == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }

  *relay_only = false;
  if ((remote_rights = find_remote_permissions_by_permissions_handle(ac, permissions_handle)) == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }
  if (!(instance->get_topic_sec_attributes(instance, ACCESS_CONTROL_OBJECT_HANDLE(remote_rights->local_rights), subscription_data->topic_name, &topic_sec_attr, ex)))
    goto exit;
  if (!topic_sec_attr.is_read_protected)
  {
    result = true;
    goto exit;
  }

  /* Compare PluginClassName and MajorVersion parts */
  class_id_remote_str = remote_rights->permissions->remote_permissions_token_class_id;
  if (compare_class_id_plugin_classname(class_id_remote_str, DDS_ACTOKEN_PERMISSIONS_CLASS_ID) == false)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_CLASSNAME_CODE, 0,
        DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_CLASSNAME_MESSAGE);
    goto exit;
  }
  if (compare_class_id_major_ver(class_id_remote_str, DDS_ACTOKEN_PERMISSIONS_CLASS_ID) == false)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_MAJORVERSION_CODE, 0,
                               DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_MAJORVERSION_MESSAGE);
    ACCESS_CONTROL_OBJECT_RELEASE(remote_rights);
    goto exit;
  }

  /* Find a topic with the specified topic name in the Governance */
  result = is_readwrite_allowed_by_permissions(remote_rights->permissions->permissions_tree, domain_id, subscription_data->topic_name,
      &subscription_data->partition, remote_rights->identity_subject_name, SUBSCRIBE_CRITERIA, ex);

exit:
  ACCESS_CONTROL_OBJECT_RELEASE(remote_rights);
  return result;
}

static DDS_Security_boolean
check_remote_topic(dds_security_access_control *instance,
                   const DDS_Security_PermissionsHandle permissions_handle,
                   const DDS_Security_DomainId domain_id,
                   const DDS_Security_TopicBuiltinTopicData *topic_data,
                   DDS_Security_SecurityException *ex)
{
  dds_security_access_control_impl *ac = (dds_security_access_control_impl *)instance;
  DDS_Security_TopicSecurityAttributes topic_sec_attr;
  remote_participant_access_rights *remote_rights;
  DDS_Security_string class_id_remote_str;
  DDS_Security_boolean result = false;

  if (instance == NULL || permissions_handle == DDS_SECURITY_HANDLE_NIL || topic_data == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }
  if ((remote_rights = find_remote_permissions_by_permissions_handle(ac, permissions_handle)) == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }
  if ((result = instance->get_topic_sec_attributes(instance, ACCESS_CONTROL_OBJECT_HANDLE(remote_rights->local_rights), topic_data->name, &topic_sec_attr, ex)) == false)
    goto exit;
  if (!topic_sec_attr.is_read_protected || !topic_sec_attr.is_write_protected)
  {
    result = true;
    goto exit;
  }

  /* Compare PluginClassName and MajorVersion parts */
  class_id_remote_str = remote_rights->permissions->remote_permissions_token_class_id;
  if (!compare_class_id_plugin_classname(class_id_remote_str, DDS_ACTOKEN_PERMISSIONS_CLASS_ID))
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_CLASSNAME_CODE, 0,
      DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_CLASSNAME_MESSAGE);
    goto exit;
  }
  if (!compare_class_id_major_ver(class_id_remote_str, DDS_ACTOKEN_PERMISSIONS_CLASS_ID))
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_MAJORVERSION_CODE, 0,
      DDS_SECURITY_ERR_INCOMPATIBLE_REMOTE_PLUGIN_MAJORVERSION_MESSAGE);
    goto exit;
  }

  result = is_topic_allowed_by_permissions(remote_rights->permissions->permissions_tree, domain_id, topic_data->name, remote_rights->identity_subject_name, ex);

exit:
  ACCESS_CONTROL_OBJECT_RELEASE(remote_rights);
  return result;
}

static DDS_Security_boolean
check_local_datawriter_match(
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

  /* This function is not implemented because it relies on DataTagging,
     an optional DDS Security feature that is not implemented */
  return true;
}

static DDS_Security_boolean
check_local_datareader_match(
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

  /* Not implemented */
  return true;
}

static DDS_Security_boolean
check_remote_datawriter_register_instance(
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

  /* Not implemented */
  return true;
}

static DDS_Security_boolean
check_remote_datawriter_dispose_instance(
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

  /* Not implemented */
  return true;
}

static DDS_Security_boolean
get_permissions_token(dds_security_access_control *instance,
                      DDS_Security_PermissionsToken *permissions_token,
                      const DDS_Security_PermissionsHandle handle,
                      DDS_Security_SecurityException *ex)
{
  local_participant_access_rights *rights;
  if (!ex)
    return false;
  if (!instance)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_permissions_token: No instance provided");
    return false;
  }
  if (!permissions_token)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_permissions_token: No permissions token provided");
    return false;
  }
  if (handle == DDS_SECURITY_HANDLE_NIL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_permissions_token: No permissions handle provided");
    return false;
  }
  if ((rights = find_local_access_rights((dds_security_access_control_impl *)instance, handle)) == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "get_permissions_token: Unused permissions handle provided");
    return false;
  }

  ACCESS_CONTROL_OBJECT_RELEASE(rights);
  memset(permissions_token, 0, sizeof(*permissions_token));
  permissions_token->class_id = ddsrt_strdup(DDS_ACTOKEN_PERMISSIONS_CLASS_ID);
  return true;
}

static DDS_Security_boolean
get_permissions_credential_token(
    dds_security_access_control *instance,
    DDS_Security_PermissionsCredentialToken *permissions_credential_token,
    const DDS_Security_PermissionsHandle handle,
    DDS_Security_SecurityException *ex)
{
  dds_security_access_control_impl *ac = (dds_security_access_control_impl *)instance;
  local_participant_access_rights *rights;
  if (!ex)
    return false;
  if (!instance)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_permissions_credential_token: No instance provided");
    return false;
  }
  if (!permissions_credential_token)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_permissions_credential_token: No permissions credential token provided");
    return false;
  }
  if (handle == DDS_SECURITY_HANDLE_NIL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, DDS_SECURITY_VALIDATION_FAILED, "get_permissions_credential_token: No permissions handle provided");
    return false;
  }
  if ((rights = find_local_access_rights(ac, handle)) == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "get_permissions_credential_token: Unused permissions handle provided");
    return false;
  }

  memset(permissions_credential_token, 0, sizeof(*permissions_credential_token));
  permissions_credential_token->class_id = ddsrt_strdup(DDS_ACTOKEN_PERMISSIONS_CREDENTIAL_CLASS_ID);
  permissions_credential_token->properties._length = permissions_credential_token->properties._maximum = 1;
  permissions_credential_token->properties._buffer = DDS_Security_PropertySeq_allocbuf(1);
  permissions_credential_token->properties._buffer[0].name = ddsrt_strdup(DDS_ACTOKEN_PROP_PERM_CERT);
  permissions_credential_token->properties._buffer[0].value = ddsrt_strdup(rights->permissions_document);
  ACCESS_CONTROL_OBJECT_RELEASE(rights);
  return true;
}


static DDS_Security_boolean
set_listener(dds_security_access_control *instance,
             const dds_security_access_control_listener *listener,
             DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(ex);

  dds_security_access_control_impl *ac = (dds_security_access_control_impl *)instance;
  ac->listener = listener;
  if (listener)
    dds_security_timed_dispatcher_enable(ac->dispatcher);
  else
    (void) dds_security_timed_dispatcher_disable(ac->dispatcher);

  return true;
}

static DDS_Security_boolean
return_permissions_token(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsToken *token,
    DDS_Security_SecurityException *ex)
{
  if (!instance || !token)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }
  DDS_Security_DataHolder_deinit((DDS_Security_DataHolder *)token);
  return true;
}

static DDS_Security_boolean
return_permissions_credential_token(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsCredentialToken *permissions_credential_token,
    DDS_Security_SecurityException *ex)
{
  if (!instance || !permissions_credential_token)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }
  DDS_Security_DataHolder_deinit((DDS_Security_DataHolder *)permissions_credential_token);
  return true;
}

static void
protectionkind_to_participant_attribute(
    DDS_Security_ProtectionKind kind,
    DDS_Security_boolean *is_protected,
    DDS_Security_ParticipantSecurityAttributesMask *mask,
    DDS_Security_ParticipantSecurityAttributesMask encryption_bit,
    DDS_Security_ParticipantSecurityAttributesMask authentication_bit)
{
  switch (kind)
  {
  case DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION:
    (*mask) |= authentication_bit;
    (*mask) |= encryption_bit;
    (*is_protected) = true;
    break;
  case DDS_SECURITY_PROTECTION_KIND_ENCRYPT:
    (*mask) |= encryption_bit;
    (*is_protected) = true;
    break;
  case DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION:
    (*mask) |= authentication_bit;
    (*is_protected) = true;
    break;
  case DDS_SECURITY_PROTECTION_KIND_SIGN:
    (*is_protected) = true;
    break;
  case DDS_SECURITY_PROTECTION_KIND_NONE:
  default:
    (*is_protected) = false;
    break;
  }
}

static DDS_Security_PluginEndpointSecurityAttributesMask
get_plugin_endpoint_security_attributes_mask(
    DDS_Security_boolean is_payload_encrypted,
    DDS_Security_boolean is_submessage_encrypted,
    DDS_Security_boolean is_submessage_origin_authenticated)
{
  DDS_Security_PluginEndpointSecurityAttributesMask mask = DDS_SECURITY_ENDPOINT_ATTRIBUTES_FLAG_IS_VALID;
  if (is_submessage_encrypted)
    mask |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
  if (is_payload_encrypted)
    mask |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_PAYLOAD_ENCRYPTED;
  if (is_submessage_origin_authenticated)
    mask |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ORIGIN_AUTHENTICATED;
  return mask;
}

static void
domain_rule_to_participant_attributes(
    const struct domain_rule *rule,
    DDS_Security_ParticipantSecurityAttributes *attributes)
{
  /* Expect proper rule. */
  assert(rule);
  assert(rule->allow_unauthenticated_participants);
  assert(rule->enable_join_access_control);
  assert(rule->liveliness_protection_kind);
  assert(rule->discovery_protection_kind);
  assert(rule->rtps_protection_kind);
  assert(attributes);

  memset(attributes, 0, sizeof(DDS_Security_ParticipantSecurityAttributes));

  attributes->allow_unauthenticated_participants = rule->allow_unauthenticated_participants->value;
  attributes->is_access_protected = rule->enable_join_access_control->value;

  attributes->plugin_participant_attributes = DDS_SECURITY_PARTICIPANT_ATTRIBUTES_FLAG_IS_VALID;

  protectionkind_to_participant_attribute(
      rule->discovery_protection_kind->value,
      &(attributes->is_discovery_protected),
      &(attributes->plugin_participant_attributes),
      DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_ENCRYPTED,
      DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_DISCOVERY_AUTHENTICATED);

  protectionkind_to_participant_attribute(
      rule->liveliness_protection_kind->value,
      &(attributes->is_liveliness_protected),
      &(attributes->plugin_participant_attributes),
      DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_ENCRYPTED,
      DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_LIVELINESS_AUTHENTICATED);

  protectionkind_to_participant_attribute(
      rule->rtps_protection_kind->value,
      &(attributes->is_rtps_protected),
      &(attributes->plugin_participant_attributes),
      DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_ENCRYPTED,
      DDS_SECURITY_PLUGIN_PARTICIPANT_ATTRIBUTES_FLAG_IS_RTPS_AUTHENTICATED);
}

static DDS_Security_boolean
domainid_within_sets(
    struct domain_id_set *domain,
    int domain_id)
{
  DDS_Security_boolean found = false;
  int32_t min;
  int32_t max;

  while (domain != NULL && !found)
  {
    assert(domain->min);
    min = domain->min->value;
    max = domain->max ? domain->max->value : min;
    if ((domain_id >= min) && (domain_id <= max))
      found = true;
    domain = (struct domain_id_set *)domain->node.next;
  }
  return found;
}

static struct domain_rule *
find_domain_rule_in_governance(struct domain_rule *rule, int domain_id)
{
  struct domain_rule *found = NULL;
  while ((rule != NULL) && (found == NULL))
  {
    assert(rule->domains);
    if (domainid_within_sets(rule->domains->domain_id_set, domain_id))
      found = rule;
    rule = (struct domain_rule *)rule->node.next;
  }
  return found;
}

static DDS_Security_boolean
get_participant_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    DDS_Security_ParticipantSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  dds_security_access_control_impl *ac = (dds_security_access_control_impl *)instance;
  local_participant_access_rights *local_rights;
  struct domain_rule *found = NULL;
  DDS_Security_boolean result = false;

  if (instance == 0 || permissions_handle == DDS_SECURITY_HANDLE_NIL || attributes == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }

  /* The local rights are actually the local permissions handle. Check that. */
  if ((local_rights = find_local_access_rights(ac, permissions_handle)) == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "Invalid permissions handle");
    return false;
  }
  if ((found = find_domain_rule_in_governance(local_rights->governance_tree->dds->domain_access_rules->domain_rule, local_rights->domain_id)))
  {
    domain_rule_to_participant_attributes(found, attributes);
    result = true;
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_IDENTITY_EMPTY_CODE, 0, "Could not domain id within governance file.");
  }
  ACCESS_CONTROL_OBJECT_RELEASE(local_rights);
  return result;
}

static DDS_Security_boolean
compare_class_id_plugin_classname(DDS_Security_string classid1, DDS_Security_string classid2)
{
  char *classname1 = strrchr(classid1, ':');
  char *classname2 = strrchr(classid2, ':');
  const ptrdiff_t len1 = classname1 - classid1;
  const ptrdiff_t len2 = classname2 - classid2;
  return len1 == len2 && classname1 && classname2 &&
    ddsrt_strncasecmp(classid1, classid2, (size_t) len1) == 0;
}

static DDS_Security_boolean
compare_class_id_major_ver(DDS_Security_string classid1, DDS_Security_string classid2)
{
  char *version_1 = strrchr(classid1, ':');
  char *version_2 = strrchr(classid2, ':');
  if (version_1 && version_2)
  {
    const char *majorVersion_1 = strrchr(version_1, '.');
    const char *majorVersion_2 = strrchr(version_2, '.');
    const ptrdiff_t len1 = majorVersion_1 - version_1;
    const ptrdiff_t len2 = majorVersion_2 - version_2;
    return len1 == len2 && majorVersion_1 && majorVersion_2 &&
      ddsrt_strncasecmp(version_1, version_2, (size_t) len1) == 0;
  }
  return false;
}

static DDS_Security_boolean
is_partition_qos_in_criteria(
    const struct criteria *criteria,
    const DDS_Security_PartitionQosPolicy *partitions,
    permission_rule_type rule_type)
{
  const DDS_Security_PartitionQosPolicy defaultPartitions = { .name = {
    ._length = 1, ._maximum = 1, ._buffer = (char *[]) { "" }
  } };
  const DDS_Security_PartitionQosPolicy *partitionsToCheck = (partitions->name._length == 0) ? &defaultPartitions : partitions;
  switch (rule_type)
  {
    case ALLOW_RULE: // allow rules: all partitions must be allowed
      for (unsigned partition_index = 0; partition_index < partitionsToCheck->name._length; partition_index++)
        if (!is_partition_in_criteria (criteria, partitionsToCheck->name._buffer[partition_index]))
          return false;
      return true;
    case DENY_RULE: // deny rules: some partitions disallowed
      for (unsigned partition_index = 0; partition_index < partitionsToCheck->name._length; partition_index++)
        if (is_partition_in_criteria (criteria, partitionsToCheck->name._buffer[partition_index]))
          return true;
      return false;
  }
  assert (0);
  return false;
}

static DDS_Security_boolean
is_partition_in_criteria (
    const struct criteria *criteria,
    const char *partition_name)
{
  struct partitions *current_partitions;
  struct string_value *current_partition;

  if (criteria == NULL || partition_name == NULL)
    return false;

  // FIXME: the spec is wrong in requiring "fnmatch" because of wildcards in partitions
  //
  // (naturally without specifying what options to use for "fnmatch" even though the spec it refers
  // to for fnmatch has options ...)
  //
  // To match an allow rule, the set of partitions spanned by the partition set of the reader/writer
  // should be fully contained in the set of partitions spanned by the set in the rule, but with
  // fnmatch, "allow ?" and "read/write *" match (because "*" is a single character and hence matches
  // the pattern "?", but it clearly can result in matching partition names that do not consist of a
  // single character.
  //
  // Deny rules are similar: instead of "contained in" what matters is that the intersection of the
  // two sets must be empty, or it must be denied.

  current_partitions = (struct partitions *) criteria->partitions;
  while (current_partitions != NULL)
  {
    current_partition = current_partitions->partition;
    while (current_partition != NULL)
    {
      // empty partition string in permission document comes out as a null pointer
      if (ac_fnmatch (current_partition->value ? current_partition->value : "", partition_name))
        return true;
      current_partition = (struct string_value *)current_partition->node.next;
    }
    current_partitions = (struct partitions *)current_partitions->node.next;
  }
  return false;
}

static DDS_Security_boolean
is_topic_in_criteria(
    const struct criteria *criteria,
    const char *topic_name)
{
  struct topics *current_topics;
  struct string_value *current_topic;

  if (criteria == NULL || topic_name == NULL)
    return false;

  /* Start by checking for a matching topic */
  current_topics = criteria->topics;
  while (current_topics != NULL)
  {
    current_topic = current_topics->topic;
    while (current_topic != NULL)
    {
      if (ac_fnmatch(current_topic->value, topic_name))
        return true;
      current_topic = (struct string_value *)current_topic->node.next;
    }
    current_topics = (struct topics *)current_topics->node.next;
  }
  return false;
}

static struct topic_rule *
find_topic_from_domain_rule(
    struct domain_rule *domain_rule,
    const char *topic_name)
{
  struct topic_rule *topic_rule;
  struct topic_rule *topic_found = NULL;

  if (domain_rule->topic_access_rules != NULL &&
      domain_rule->topic_access_rules->topic_rule != NULL)
  {
    topic_rule = domain_rule->topic_access_rules->topic_rule;
    while (topic_rule != NULL && topic_found == NULL)
    {
      assert(topic_rule->topic_expression);
      if (ac_fnmatch(topic_rule->topic_expression->value, topic_name))
        topic_found = topic_rule;
      topic_rule = (struct topic_rule *)topic_rule->node.next;
    }
  }
  return topic_found;
}

static DDS_Security_boolean
get_topic_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const char *topic_name,
    DDS_Security_TopicSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  dds_security_access_control_impl *ac = (dds_security_access_control_impl *)instance;
  local_participant_access_rights *rights;
  struct domain_rule *found;
  DDS_Security_boolean result = false;

  if (instance == 0)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "No plugin instance provided");
    return false;
  }
  if (permissions_handle == DDS_SECURITY_HANDLE_NIL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "No permissions handle provided");
    return false;
  }
  if (topic_name == NULL || strlen(topic_name) == 0)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "No topic name provided");
    return false;
  }
  if (attributes == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "No attributes provided");
    return false;
  }
  rights = find_local_access_rights(ac, permissions_handle);
  if (rights == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "Unused permissions handle provided");
    return false;
  }

  memset(attributes, 0, sizeof(*attributes));

  if (get_topic_type(topic_name) != TOPIC_TYPE_USER)
  {
    /* No attributes are set for builtin topics. */
    ACCESS_CONTROL_OBJECT_RELEASE(rights);
    return true;
  }

  if ((found = find_domain_rule_in_governance(rights->governance_tree->dds->domain_access_rules->domain_rule, rights->domain_id)))
  {
    struct topic_rule *topic_rule = find_topic_from_domain_rule(found, topic_name);
    if (topic_rule)
    {
      attributes->is_discovery_protected = topic_rule->enable_discovery_protection->value;
      attributes->is_liveliness_protected = topic_rule->enable_liveliness_protection->value;
      attributes->is_read_protected = topic_rule->enable_read_access_control->value;
      attributes->is_write_protected = topic_rule->enable_write_access_control->value;
      result = true;
    }
    else
    {
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_FIND_TOPIC_IN_DOMAIN_CODE, 0, DDS_SECURITY_ERR_CAN_NOT_FIND_TOPIC_IN_DOMAIN_MESSAGE, topic_name, rights->domain_id);
    }
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_FIND_DOMAIN_IN_GOVERNANCE_CODE, 0, DDS_SECURITY_ERR_CAN_NOT_FIND_DOMAIN_IN_GOVERNANCE_MESSAGE, rights->domain_id);
  }

  ACCESS_CONTROL_OBJECT_RELEASE(rights);
  return result;
}

static DDS_Security_boolean
get_datawriter_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const char *topic_name,
    const DDS_Security_PartitionQosPolicy *partition,
    const DDS_Security_DataTagQosPolicy *data_tag,
    DDS_Security_EndpointSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(partition);
  DDSRT_UNUSED_ARG(data_tag);
  dds_security_access_control_impl *ac = (dds_security_access_control_impl *)instance;

  if (instance == 0 || permissions_handle == DDS_SECURITY_HANDLE_NIL || topic_name == 0 || strlen(topic_name) == 0 || attributes == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }
  return get_sec_attributes(ac, permissions_handle, topic_name, attributes, ex);
}

static DDS_Security_boolean
get_datareader_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const char *topic_name,
    const DDS_Security_PartitionQosPolicy *partition,
    const DDS_Security_DataTagQosPolicy *data_tag,
    DDS_Security_EndpointSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(partition);
  DDSRT_UNUSED_ARG(data_tag);
  dds_security_access_control_impl *ac = (dds_security_access_control_impl *)instance;

  if (instance == 0 || permissions_handle == DDS_SECURITY_HANDLE_NIL || topic_name == 0 || strlen(topic_name) == 0 || attributes == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }
  return get_sec_attributes(ac, permissions_handle, topic_name, attributes, ex);
}

static DDS_Security_boolean
return_participant_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_ParticipantSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(attributes);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  /* Nothing to do. */
  return true;
}

static DDS_Security_boolean
return_topic_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_TopicSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(attributes);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  /* Nothing to do. */
  return true;
}

static DDS_Security_boolean
return_datawriter_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_EndpointSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(attributes);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  /* Nothing to do. */
  return true;
}

static DDS_Security_boolean
return_datareader_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_EndpointSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  DDSRT_UNUSED_ARG(attributes);
  DDSRT_UNUSED_ARG(ex);
  DDSRT_UNUSED_ARG(instance);
  /* Nothing to do. */
  return true;
}

static DDS_Security_boolean
return_permissions_handle(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    DDS_Security_SecurityException *ex)
{
  dds_security_access_control_impl *ac = (dds_security_access_control_impl *)instance;
  AccessControlObject *object;

  if (!instance || !permissions_handle)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }

#ifdef ACCESS_CONTROL_USE_ONE_PERMISSION
  ddsrt_mutex_lock(&ac->lock);
  if (permissions_handle == ACCESS_CONTROL_OBJECT_HANDLE(ac->local_access_rights))
  {
    ddsrt_mutex_unlock(&ac->lock);
    return true;
  }
  ddsrt_mutex_unlock(&ac->lock);
#else
  object = access_control_table_find(ac->local_permissions, permissions_handle);
  if (object)
  {
    if (object->timer != 0)
      dds_security_timed_dispatcher_remove(ac->dispatcher, object->timer);

    access_control_table_remove_object(ac->local_permissions, object);
    access_control_object_release(object);
    return true;
  }
#endif

  object = access_control_table_find(ac->remote_permissions, permissions_handle);
  if (!object)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }

  if (object->timer != 0)
    dds_security_timed_dispatcher_remove(ac->dispatcher, object->timer);

  access_control_table_remove_object(ac->remote_permissions, object);
  access_control_object_release(object);
  return true;
}

int init_access_control(const char *argument, void **context, struct ddsi_domaingv *gv)
{
  DDSRT_UNUSED_ARG(argument);

  dds_security_access_control_impl *access_control = ddsrt_malloc(sizeof(*access_control));
  memset(access_control, 0, sizeof(*access_control));
  access_control->base.gv = gv;
  access_control->listener = NULL;
  access_control->dispatcher = dds_security_timed_dispatcher_new(gv->xevents);
  access_control->base.validate_local_permissions = &validate_local_permissions;
  access_control->base.validate_remote_permissions = &validate_remote_permissions;
  access_control->base.check_create_participant = &check_create_participant;
  access_control->base.check_create_datawriter = &check_create_datawriter;
  access_control->base.check_create_datareader = &check_create_datareader;
  access_control->base.check_create_topic = &check_create_topic;
  access_control->base.check_local_datawriter_register_instance = &check_local_datawriter_register_instance;
  access_control->base.check_local_datawriter_dispose_instance = &check_local_datawriter_dispose_instance;
  access_control->base.check_remote_participant = &check_remote_participant;
  access_control->base.check_remote_datawriter = &check_remote_datawriter;
  access_control->base.check_remote_datareader = &check_remote_datareader;
  access_control->base.check_remote_topic = &check_remote_topic;
  access_control->base.check_local_datawriter_match = &check_local_datawriter_match;
  access_control->base.check_local_datareader_match = &check_local_datareader_match;
  access_control->base.check_remote_datawriter_register_instance = &check_remote_datawriter_register_instance;
  access_control->base.check_remote_datawriter_dispose_instance = &check_remote_datawriter_dispose_instance;
  access_control->base.get_permissions_token = &get_permissions_token;
  access_control->base.get_permissions_credential_token = &get_permissions_credential_token;
  access_control->base.set_listener = &set_listener;
  access_control->base.return_permissions_token = &return_permissions_token;
  access_control->base.return_permissions_credential_token = &return_permissions_credential_token;
  access_control->base.get_participant_sec_attributes = &get_participant_sec_attributes;
  access_control->base.get_topic_sec_attributes = &get_topic_sec_attributes;
  access_control->base.get_datawriter_sec_attributes = &get_datawriter_sec_attributes;
  access_control->base.get_datareader_sec_attributes = &get_datareader_sec_attributes;
  access_control->base.return_participant_sec_attributes = &return_participant_sec_attributes;
  access_control->base.return_topic_sec_attributes = &return_topic_sec_attributes;
  access_control->base.return_datawriter_sec_attributes = &return_datawriter_sec_attributes;
  access_control->base.return_datareader_sec_attributes = &return_datareader_sec_attributes;
  access_control->base.return_permissions_handle = &return_permissions_handle;
  ddsrt_mutex_init(&access_control->lock);

#ifdef ACCESS_CONTROL_USE_ONE_PERMISSION
  access_control->local_access_rights = NULL;
#else
  access_control->local_permissions = access_control_table_new();
#endif
  access_control->remote_permissions = access_control_table_new();

  dds_openssl_init ();
  *context = access_control;
  return 0;
}

static bool
get_sec_attributes(
    dds_security_access_control_impl *ac,
    const DDS_Security_PermissionsHandle permissions_handle,
    const char *topic_name,
    DDS_Security_EndpointSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  local_participant_access_rights *rights;
  DDS_Security_boolean result = false;
  TOPIC_TYPE topic_type;
  assert(topic_name);
  assert(attributes);
  memset(attributes, 0, sizeof(DDS_Security_EndpointSecurityAttributes));
  if ((rights = find_local_access_rights(ac, permissions_handle)) == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, "Invalid permissions handle");
    return false;
  }

  if ((topic_type = get_topic_type(topic_name)) != TOPIC_TYPE_USER)
  {
    /* Builtin topics are treated in a special manner. */
    result = true;

    if (topic_type == TOPIC_TYPE_SECURE_ParticipantsSecure || topic_type == TOPIC_TYPE_SECURE_PublicationsSecure ||
        topic_type == TOPIC_TYPE_SECURE_SubscriptionsSecure || topic_type == TOPIC_TYPE_SECURE_ParticipantMessageSecure)
    {
      struct domain_rule *found = find_domain_rule_in_governance(rights->governance_tree->dds->domain_access_rules->domain_rule, rights->domain_id);
      if (found)
      { /* Domain matched */
        /* is_submessage_protected should match is_liveliness_protected of
             * ParticipantSecurityAttributes for DCPSParticipantMessageSecure.
             * is_submessage_protected should match is_discovery_protected of
             * ParticipantSecurityAttributes for OTHER 3.*/
        if (topic_type == TOPIC_TYPE_SECURE_ParticipantMessageSecure)
        {
          attributes->is_submessage_protected = !(found->liveliness_protection_kind->value == DDS_SECURITY_PROTECTION_KIND_NONE);
          attributes->plugin_endpoint_attributes = get_plugin_endpoint_security_attributes_mask(
              /* payload encrypted */
              false,
              /* submsg encrypted */
              found->liveliness_protection_kind->value == DDS_SECURITY_PROTECTION_KIND_ENCRYPT ||
                  found->liveliness_protection_kind->value == DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION,
              /* submsg authenticated */
              found->liveliness_protection_kind->value == DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION ||
                  found->liveliness_protection_kind->value == DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION);
        }
        else
        {
          attributes->is_submessage_protected = !(found->discovery_protection_kind->value == DDS_SECURITY_PROTECTION_KIND_NONE);
          attributes->plugin_endpoint_attributes = get_plugin_endpoint_security_attributes_mask(
              /* payload encrypted */
              false,
              /* submsg encrypted */
              found->discovery_protection_kind->value == DDS_SECURITY_PROTECTION_KIND_ENCRYPT ||
                  found->discovery_protection_kind->value == DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION,
              /* submsg authenticated */
              found->discovery_protection_kind->value == DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION ||
                  found->discovery_protection_kind->value == DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION);
        }
      }
      else
      {
        DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_FIND_DOMAIN_IN_GOVERNANCE_CODE, 0, DDS_SECURITY_ERR_CAN_NOT_FIND_DOMAIN_IN_GOVERNANCE_MESSAGE, rights->domain_id);
        result = false;
      }
      attributes->is_read_protected = false;
      attributes->is_write_protected = false;
      attributes->is_payload_protected = false;
      attributes->is_key_protected = false;
    }
    else if (topic_type == TOPIC_TYPE_SECURE_ParticipantStatelessMessage)
    {
      attributes->plugin_endpoint_attributes = DDS_SECURITY_ENDPOINT_ATTRIBUTES_FLAG_IS_VALID;
      attributes->is_read_protected = false;
      attributes->is_write_protected = false;
      attributes->is_payload_protected = false;
      attributes->is_key_protected = false;
      attributes->is_submessage_protected = false;
    }
    else if (topic_type == TOPIC_TYPE_SECURE_ParticipantVolatileMessageSecure)
    {
      attributes->plugin_endpoint_attributes = DDS_SECURITY_ENDPOINT_ATTRIBUTES_FLAG_IS_VALID |
                                               DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;
      attributes->is_read_protected = false;
      attributes->is_write_protected = false;
      attributes->is_payload_protected = false;
      attributes->is_key_protected = false;
      attributes->is_submessage_protected = true;
    }
    else
    {
      /* Non secure builtin topics. */
      attributes->plugin_endpoint_attributes = DDS_SECURITY_ENDPOINT_ATTRIBUTES_FLAG_IS_VALID;
      attributes->is_read_protected = false;
      attributes->is_write_protected = false;
      attributes->is_payload_protected = false;
      attributes->is_key_protected = false;
      attributes->is_submessage_protected = false;
    }
  }
  else
  {
    /* Normal user topic attributes are acquired from governance and permission documents. */
    struct domain_rule *found = find_domain_rule_in_governance(rights->governance_tree->dds->domain_access_rules->domain_rule, rights->domain_id);
    if (found)
    { /* Domain matched */
      struct topic_rule *topic_rule = find_topic_from_domain_rule(found, topic_name);
      if (topic_rule)
      { /* Topic matched */
        attributes->is_discovery_protected = topic_rule->enable_discovery_protection->value;
        attributes->is_liveliness_protected = topic_rule->enable_liveliness_protection->value;
        attributes->is_read_protected = topic_rule->enable_read_access_control->value;
        attributes->is_write_protected = topic_rule->enable_write_access_control->value;
        attributes->is_payload_protected = topic_rule->data_protection_kind->value != DDS_SECURITY_BASICPROTECTION_KIND_NONE;
        attributes->is_submessage_protected = topic_rule->metadata_protection_kind->value != DDS_SECURITY_PROTECTION_KIND_NONE;
        attributes->is_key_protected = topic_rule->data_protection_kind->value == DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT;

        /*calculate and assign the mask */
        attributes->plugin_endpoint_attributes = get_plugin_endpoint_security_attributes_mask(
            topic_rule->data_protection_kind->value == DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT,
            topic_rule->metadata_protection_kind->value == DDS_SECURITY_PROTECTION_KIND_ENCRYPT ||
                    topic_rule->metadata_protection_kind->value == DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION,
            topic_rule->metadata_protection_kind->value == DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION ||
                    topic_rule->metadata_protection_kind->value == DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION);

        memset(&attributes->ac_endpoint_properties, 0, sizeof(DDS_Security_PropertySeq));
        result = true;
      }
      else
      {
        DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_FIND_TOPIC_IN_DOMAIN_CODE, 0, DDS_SECURITY_ERR_CAN_NOT_FIND_TOPIC_IN_DOMAIN_MESSAGE, topic_name, rights->domain_id);
      }
    }
    else
    {
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_FIND_DOMAIN_IN_GOVERNANCE_CODE, 0, DDS_SECURITY_ERR_CAN_NOT_FIND_DOMAIN_IN_GOVERNANCE_MESSAGE, rights->domain_id);
    }
  }
  ACCESS_CONTROL_OBJECT_RELEASE(rights);
  return result;
}

static void
sanity_check_local_access_rights(
    local_participant_access_rights *rights)
{
#ifndef NDEBUG
  if (rights)
  {
    assert(rights->permissions_document);
    assert(rights->governance_tree);
    assert(rights->governance_tree->dds);
    assert(rights->governance_tree->dds->domain_access_rules);
    assert(rights->governance_tree->dds->domain_access_rules->domain_rule);
    assert(rights->permissions_tree);
    assert(rights->permissions_tree->dds);
    assert(rights->permissions_tree->dds->permissions);
    assert(rights->permissions_tree->dds->permissions->grant);
  }
#else
  DDSRT_UNUSED_ARG(rights);
#endif
}

static void
sanity_check_remote_access_rights(
    remote_participant_access_rights *rights)
{
#ifndef NDEBUG
  /* Just some sanity checks. */
  if (rights)
  {
    assert(rights->permissions);
    assert(rights->permissions->permissions_tree);
    assert(rights->permissions->permissions_tree->dds);
    assert(rights->permissions->permissions_tree->dds->permissions);
    assert(rights->permissions->remote_permissions_token_class_id);
    assert(rights->local_rights);
    sanity_check_local_access_rights(rights->local_rights);
  }
#else
  DDSRT_UNUSED_ARG(rights);
#endif
}

static local_participant_access_rights *
find_local_access_rights(
    dds_security_access_control_impl *ac,
    DDS_Security_PermissionsHandle handle)
{
  local_participant_access_rights *rights = NULL;

#ifdef ACCESS_CONTROL_USE_ONE_PERMISSION
  DDSRT_UNUSED_ARG(handle);

  ddsrt_mutex_lock(&ac->lock);
  if (handle == ACCESS_CONTROL_OBJECT_HANDLE(ac->local_access_rights))
    rights = (local_participant_access_rights *)ACCESS_CONTROL_OBJECT_KEEP(ac->local_access_rights);
  ddsrt_mutex_unlock(&ac->lock);
#else
  rights = (local_participant_access_rights *)access_control_table_find(ac->local_permissions, handle);
#endif

  sanity_check_local_access_rights(rights);
  return rights;
}

struct find_by_identity_arg
{
  AccessControlObject *object;
  DDS_Security_IdentityHandle handle;
};

#ifndef ACCESS_CONTROL_USE_ONE_PERMISSION
static int
local_identity_handle_match(
    AccessControlObject *obj,
    void *arg)
{
  local_participant_access_rights *rights = (local_participant_access_rights *)obj;
  struct find_by_identity_arg *info = arg;

  if (rights->local_identity == info->handle)
  {
    info->object = obj;
    return 0;
  }

  return 1;
}
#endif

static int
remote_identity_handle_match(
    AccessControlObject *obj,
    void *arg)
{
  remote_participant_access_rights *rights = (remote_participant_access_rights *)obj;
  struct find_by_identity_arg *info = arg;

  if (rights->remote_identity == info->handle)
  {
    info->object = ACCESS_CONTROL_OBJECT_KEEP(obj);
    return 0;
  }

  return 1;
}

static local_participant_access_rights *
find_local_rights_by_identity(
    dds_security_access_control_impl *ac,
    DDS_Security_IdentityHandle identity_handle)
{
  local_participant_access_rights *rights = NULL;

#ifdef ACCESS_CONTROL_USE_ONE_PERMISSION
  DDSRT_UNUSED_ARG(identity_handle);

  ddsrt_mutex_lock(&ac->lock);
  rights = (local_participant_access_rights *)ACCESS_CONTROL_OBJECT_KEEP(ac->local_access_rights);
  ddsrt_mutex_unlock(&ac->lock);
#else
  {
    struct find_by_identity_arg args;
    args.object = NULL;
    args.handle = identity_handle;
    access_control_table_walk(ac->local_permissions, local_identity_handle_match, &args);
    rights = (local_participant_access_rights *)args.object;
  }
#endif
  sanity_check_local_access_rights(rights);
  return rights;
}

static remote_participant_access_rights *
find_remote_rights_by_identity(
    dds_security_access_control_impl *ac,
    DDS_Security_IdentityHandle identity_handle)
{
  struct find_by_identity_arg args;
  args.object = NULL;
  args.handle = identity_handle;
  access_control_table_walk(ac->remote_permissions, remote_identity_handle_match, &args);
  sanity_check_remote_access_rights((remote_participant_access_rights *)args.object);
  return (remote_participant_access_rights *)args.object;
}

struct find_by_permissions_handle_arg
{
  AccessControlObject *object;
  DDS_Security_PermissionsHandle handle;
};

static int
remote_permissions_handle_match(
    AccessControlObject *obj,
    void *arg)
{
  struct find_by_permissions_handle_arg *info = arg;
  if (obj->handle == info->handle)
  {
    info->object = ACCESS_CONTROL_OBJECT_KEEP(obj);
    return 0;
  }
  return 1;
}

static remote_participant_access_rights *
find_remote_permissions_by_permissions_handle(
    dds_security_access_control_impl *ac,
    DDS_Security_PermissionsHandle permissions_handle)
{
  struct find_by_permissions_handle_arg args;
  args.object = NULL;
  args.handle = permissions_handle;
  access_control_table_walk(ac->remote_permissions, remote_permissions_handle_match, &args);
  sanity_check_remote_access_rights((remote_participant_access_rights *)args.object);
  return (remote_participant_access_rights *)args.object;
}


typedef struct
{
  dds_security_access_control_impl *ac;
  DDS_Security_PermissionsHandle hdl;
} validity_cb_info;

static void
validity_callback(dds_security_time_event_handle_t timer, dds_time_t trigger_time, dds_security_timed_cb_kind_t kind, void *arg)
{
  validity_cb_info *info = arg;

  DDSRT_UNUSED_ARG(timer);
  DDSRT_UNUSED_ARG(trigger_time);

  assert(info);

  if (kind == DDS_SECURITY_TIMED_CB_KIND_TIMEOUT)
  {
    struct AccessControlObject *object = NULL;

    assert(info->ac->listener);

#ifdef ACCESS_CONTROL_USE_ONE_PERMISSION
    if (info->hdl == info->ac->local_access_rights->_parent.handle)
      object = access_control_object_keep((struct AccessControlObject *)info->ac->local_access_rights);
#else
      object = access_control_table_find(info->ac->local_permissions, info->hdl);
#endif

    if (!object)
      object = access_control_table_find(info->ac->remote_permissions, info->hdl);

    if (object)
    {
      const dds_security_access_control_listener *ac_listener = info->ac->listener;

      assert(object->timer == timer);
      object->timer = 0;
      if (ac_listener->on_revoke_permissions)
        ac_listener->on_revoke_permissions((dds_security_access_control *)info->ac, info->hdl);
      access_control_object_release(object);
    }
  }
  ddsrt_free(arg);
}

static dds_security_time_event_handle_t
add_validity_end_trigger(dds_security_access_control_impl *ac,
                         const DDS_Security_PermissionsHandle permissions_handle,
                         dds_time_t end)
{
  validity_cb_info *arg = ddsrt_malloc(sizeof(validity_cb_info));
  arg->ac = ac;
  arg->hdl = permissions_handle;
  return dds_security_timed_dispatcher_add(ac->dispatcher, validity_callback, end, (void *)arg);
}

static bool is_grant_applicable (const struct grant *permissions_grant, const char *identity_subject_name)
{
  return (permissions_grant->subject_name != NULL &&
          permissions_grant->subject_name->value != NULL &&
          strcmp (permissions_grant->subject_name->value, identity_subject_name) == 0);
}

static bool is_grant_valid (const struct grant *permissions_grant, DDS_Security_SecurityException *ex)
{
  const dds_time_t tnow = dds_time ();
  if (tnow <= DDS_Security_parse_xml_date(permissions_grant->validity->not_before->value))
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_VALIDITY_PERIOD_NOT_STARTED_CODE, 0,
        DDS_SECURITY_ERR_VALIDITY_PERIOD_NOT_STARTED_MESSAGE, permissions_grant->subject_name->value, permissions_grant->validity->not_before->value);
    return false;
  }
  if (tnow >= DDS_Security_parse_xml_date(permissions_grant->validity->not_after->value))
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_VALIDITY_PERIOD_EXPIRED_CODE, 0,
        DDS_SECURITY_ERR_VALIDITY_PERIOD_EXPIRED_MESSAGE, permissions_grant->subject_name->value, permissions_grant->validity->not_after->value);
    return false;
  }
  return true;
}

static const struct grant *find_permissions_grant (const struct permissions_parser *permissions, const char *identity_subject_name, DDS_Security_SecurityException *ex)
{
  assert(permissions->dds);
  assert(permissions->dds->permissions);

  for (const struct grant *permissions_grant = permissions->dds->permissions->grant; permissions_grant; permissions_grant = (struct grant *) permissions_grant->node.next)
  {
    if (is_grant_applicable (permissions_grant, identity_subject_name))
    {
      if (is_grant_valid (permissions_grant, ex))
        return permissions_grant;
      else // exception set by is_grant_valid
        return NULL;
    }
  }
  DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CAN_NOT_FIND_PERMISSIONS_GRANT_CODE, 0, DDS_SECURITY_ERR_CAN_NOT_FIND_PERMISSIONS_GRANT_MESSAGE);
  return NULL;
}

static bool is_allowed_by_default_rule (const struct grant *permissions_grant, const char *topic_name, DDS_Security_SecurityException *ex)
  ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

static bool is_allowed_by_default_rule (const struct grant *permissions_grant, const char *topic_name, DDS_Security_SecurityException *ex)
{
  if (permissions_grant->default_action == NULL)
  {
    DDS_Security_Exception_set (ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_ACCESS_DENIED_CODE, 0, "No rule found for %s", topic_name);
    return false;
  }
  if (strcmp (permissions_grant->default_action->value, "ALLOW") == 0)
  {
    return true;
  }
  else
  {
    DDS_Security_Exception_set (ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_ACCESS_DENIED_CODE, 0, "%s denied by default rule", topic_name);
    return false;
  }
}

static bool is_allowed_by_rule (const struct allow_deny_rule *current_rule, const char *topic_name, DDS_Security_SecurityException *ex) ddsrt_nonnull_all ddsrt_attribute_warn_unused_result;

static bool is_allowed_by_rule (const struct allow_deny_rule *current_rule, const char *topic_name, DDS_Security_SecurityException *ex)
{
  switch (current_rule->rule_type)
  {
    case ALLOW_RULE:
      return true;
    case DENY_RULE:
      DDS_Security_Exception_set (ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_ACCESS_DENIED_CODE, 0, "%s found in deny_rule.", topic_name);
      break;
  }
  return false;
}

typedef struct rule_iter {
  const struct grant *grant;
  int domain_id;
  const struct allow_deny_rule *rule; // next applicable rule based on grant & domain id, or null
} rule_iter_t;

static const struct allow_deny_rule *rule_iter_next (rule_iter_t *it)
{
  const struct allow_deny_rule *rule = it->rule;
  if (it->rule)
  {
    do {
      it->rule = (const struct allow_deny_rule *) it->rule->node.next;
    } while (it->rule && !domainid_within_sets (it->rule->domains->domain_id_set, it->domain_id));
  }
  return rule;
}

static bool rule_iter_init (rule_iter_t *it, const struct permissions_parser *permissions, int domain_id, const char *identity_subject_name, DDS_Security_SecurityException *ex)
{
  if ((it->grant = find_permissions_grant (permissions, identity_subject_name, ex)) == NULL)
    return false;
  it->domain_id = domain_id;
  it->rule = it->grant->allow_deny_rule;
  while (it->rule && !domainid_within_sets (it->rule->domains->domain_id_set, it->domain_id))
    it->rule = (const struct allow_deny_rule *) it->rule->node.next;
  return true;
}

static bool is_participant_allowed_by_permissions (const struct permissions_parser *permissions, int domain_id, const char *identity_subject_name, DDS_Security_SecurityException *ex)
{
  rule_iter_t it;
  const struct allow_deny_rule *rule;
  if (!rule_iter_init (&it, permissions, domain_id, identity_subject_name, ex))
    return false;
  while ((rule = rule_iter_next (&it)) != NULL)
    if (rule->rule_type == ALLOW_RULE)
      return true;
  return is_allowed_by_default_rule (it.grant, "participant", ex);
}

static bool is_topic_allowed_by_permissions (const struct permissions_parser *permissions, int domain_id, const char *topic_name, const char *identity_subject_name, DDS_Security_SecurityException *ex)
{
  rule_iter_t it;
  const struct allow_deny_rule *rule;
  if (!rule_iter_init (&it, permissions, domain_id, identity_subject_name, ex))
    return false;
  while ((rule = rule_iter_next (&it)) != NULL)
  {
    for (const struct criteria *crit = rule->criteria; crit; crit = (const struct criteria *) crit->node.next)
      if (is_topic_in_criteria (crit, topic_name))
        return is_allowed_by_rule (rule, topic_name, ex);
  }
  return is_allowed_by_default_rule (it.grant, topic_name, ex);
}

static bool is_readwrite_allowed_by_permissions (struct permissions_parser *permissions, int domain_id, const char *topic_name, const DDS_Security_PartitionQosPolicy *partitions, const char *identity_subject_name, permission_criteria_type criteria_type, DDS_Security_SecurityException *ex)
{
  rule_iter_t it;
  const struct allow_deny_rule *rule;
  if (!rule_iter_init (&it, permissions, domain_id, identity_subject_name, ex))
    return false;
  while ((rule = rule_iter_next (&it)) != NULL)
  {
    for (const struct criteria *crit = rule->criteria; crit; crit = (const struct criteria *) crit->node.next)
      if (crit->criteria_type == criteria_type && is_topic_in_criteria (crit, topic_name) && is_partition_qos_in_criteria (crit, partitions, rule->rule_type))
        return is_allowed_by_rule (rule, topic_name, ex);
  }
  return is_allowed_by_default_rule (it.grant, topic_name, ex);
}

static bool
read_document_from_file(
    const char *filename,
    char **doc,
    DDS_Security_SecurityException *ex)
{
  DDSRT_WARNING_MSVC_OFF(4996);
  FILE *fp;
  char *document = NULL;
  char *fname = NULL;
  size_t sz, r;

  assert(doc);
  *doc = NULL;
  /* Get portable file name. */
  fname = DDS_Security_normalize_file(filename);
  if (fname)
  {
    /* Get size if it is a accessible regular file (no dir or link). */
    sz = ac_regular_file_size(fname);
    if (sz > 0)
    {
      /* Open the actual file. */
      fp = fopen(fname, "r");
      if (fp)
      {
        /* Read the content. */
        document = ddsrt_malloc(sz + 1);
        r = fread(document, 1, sz, fp);
        if (r == 0)
        {
          ddsrt_free(document);
        }
        else
        {
          document[r] = '\0';
          *doc = document;
        }
        (void)fclose(fp);
      }
    }
    ddsrt_free(fname);
  }

  if ((*doc) == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_FILE_PATH_CODE, 0, DDS_SECURITY_ERR_INVALID_FILE_PATH_MESSAGE, (filename ? filename : "NULL"));
    return false;
  }
  return true;
  DDSRT_WARNING_MSVC_ON(4996);
}

static bool
read_document(
    const char *doc_uri,
    char **doc,
    DDS_Security_SecurityException *ex)
{
  char *data = NULL;
  switch (DDS_Security_get_conf_item_type(doc_uri, &data))
  {
    case DDS_SECURITY_CONFIG_ITEM_PREFIX_DATA:
      *doc = data;
      return true;

    case DDS_SECURITY_CONFIG_ITEM_PREFIX_FILE: {
      const bool result = read_document_from_file(data, doc, ex);
      ddsrt_free(data);
      return result;
    }

    case DDS_SECURITY_CONFIG_ITEM_PREFIX_PKCS11:
    case DDS_SECURITY_CONFIG_ITEM_PREFIX_UNKNOWN:
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_URI_TYPE_NOT_SUPPORTED_CODE, 0, DDS_SECURITY_ERR_URI_TYPE_NOT_SUPPORTED_MESSAGE, doc_uri);
      ddsrt_free(data);
      return false;
  }
  assert (0);
  return false;
}

static bool
validate_subject_name_in_permissions(struct permissions_parser *permissions_tree,
                                     const char *identity_subject_name,
                                     char **permission_subject_name,
                                     dds_time_t *permission_validity_not_after,
                                     DDS_Security_SecurityException *ex)
{

  struct grant *permissions_grant;
  assert(permission_subject_name);

  *permission_subject_name = NULL;
  if (permissions_tree == NULL || permissions_tree->dds == NULL || permissions_tree->dds->permissions == NULL || permissions_tree->dds->permissions->grant == NULL)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, 0, DDS_SECURITY_ERR_INVALID_PARAMETER_MESSAGE);
    return false;
  }

  permissions_grant = permissions_tree->dds->permissions->grant;
  while (permissions_grant != NULL)
  {
    /* Verify that it is within the validity date and the subject name matches */
    if (identity_subject_name != NULL && ac_check_subjects_are_equal(permissions_grant->subject_name->value, identity_subject_name))
    {
      dds_time_t tnow = dds_time ();
      if (tnow <= DDS_Security_parse_xml_date(permissions_grant->validity->not_before->value))
      {
        DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_VALIDITY_PERIOD_NOT_STARTED_CODE, 0,
            DDS_SECURITY_ERR_VALIDITY_PERIOD_NOT_STARTED_MESSAGE, permissions_grant->subject_name->value, permissions_grant->validity->not_before->value);
        return false;
      }
      if (tnow >= DDS_Security_parse_xml_date(permissions_grant->validity->not_after->value))
      {
        DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_VALIDITY_PERIOD_EXPIRED_CODE, 0,
            DDS_SECURITY_ERR_VALIDITY_PERIOD_EXPIRED_MESSAGE, permissions_grant->subject_name->value, permissions_grant->validity->not_after->value);
        return false;
      }

      /* identity subject name and permission subject name may not be exactly same because of different string representations
        * That's why we are returning the string in permissions file to be stored for further comparisons */
      *permission_subject_name = ddsrt_strdup(permissions_grant->subject_name->value);
      *permission_validity_not_after = DDS_Security_parse_xml_date(permissions_grant->validity->not_after->value);
      return true;
    }
    permissions_grant = (struct grant *)permissions_grant->node.next;
  }

  DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_SUBJECT_NAME_CODE, 0,
      DDS_SECURITY_ERR_INVALID_SUBJECT_NAME_MESSAGE);
  return false;
}

static local_participant_access_rights *
check_and_create_local_participant_rights(
    DDS_Security_IdentityHandle identity_handle,
    int domain_id,
    const DDS_Security_Qos *participant_qos,
    DDS_Security_SecurityException *ex)
{
  local_participant_access_rights *rights = NULL;
  X509 *identity_cert;
  X509 *permission_ca = NULL;
  size_t pdlen;
  size_t gvlen;
  char *identity_cert_data = NULL;
  char *permission_ca_data = NULL;
  char *permission_document = NULL;
  char *governance_document = NULL;
  char *permission_xml = NULL;
  char *governance_xml = NULL;
  char *identity_subject = NULL;
  struct governance_parser *governance_tree = NULL;
  struct permissions_parser *permissions_tree = NULL;
  char *permission_subject = NULL;
  char *permissions_uri = NULL;
  char *governance_uri = NULL;
  dds_time_t permission_expiry = DDS_TIME_INVALID;

  /* Retrieve the identity certificate from the participant QoS */
  identity_cert_data = DDS_Security_Property_get_value(&participant_qos->property.value, DDS_SEC_PROP_AUTH_IDENTITY_CERT);
  if (!identity_cert_data)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_MISSING_PROPERTY_CODE, 0,
        DDS_SECURITY_ERR_MISSING_PROPERTY_MESSAGE, DDS_SEC_PROP_AUTH_IDENTITY_CERT);
    goto err_no_identity_cert;
  }

  if (!ac_X509_certificate_read(identity_cert_data, &identity_cert, ex))
    goto err_inv_identity_cert;

  if (!(identity_subject = ac_get_certificate_subject_name(identity_cert, ex)))
    goto err_inv_identity_cert;

  if (!(governance_uri = DDS_Security_Property_get_value(&participant_qos->property.value, DDS_SEC_PROP_ACCESS_GOVERNANCE)))
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_MISSING_PROPERTY_CODE, 0,
        DDS_SECURITY_ERR_MISSING_PROPERTY_MESSAGE, DDS_SEC_PROP_ACCESS_GOVERNANCE);
    goto err_no_governance;
  }

  if (!(permissions_uri = DDS_Security_Property_get_value(&participant_qos->property.value, DDS_SEC_PROP_ACCESS_PERMISSIONS)))
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_MISSING_PROPERTY_CODE, 0,
        DDS_SECURITY_ERR_MISSING_PROPERTY_MESSAGE, DDS_SEC_PROP_ACCESS_PERMISSIONS);
    goto err_no_permissions;
  }

  if (!(permission_ca_data = DDS_Security_Property_get_value(&participant_qos->property.value, DDS_SEC_PROP_ACCESS_PERMISSIONS_CA)))
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_MISSING_PROPERTY_CODE, 0,
        DDS_SECURITY_ERR_MISSING_PROPERTY_MESSAGE, DDS_SEC_PROP_ACCESS_PERMISSIONS_CA);
    goto err_no_permission_ca;
  }

  if (strlen(governance_uri) == 0 && strlen(permissions_uri) == 0 && strlen(permission_ca_data) == 0)
  {
    bool result;

    result = ac_parse_governance_xml(DDS_SECURITY_DEFAULT_GOVERNANCE, &governance_tree, ex);
    assert(result);
    DDSRT_UNUSED_ARG(result);

    result = ac_parse_permissions_xml(DDS_SECURITY_DEFAULT_PERMISSIONS, &permissions_tree, ex);
    assert(result);
    DDSRT_UNUSED_ARG(result);

    /*set subject name on default permissions */
    ddsrt_free(permissions_tree->dds->permissions->grant->subject_name->value);
    permissions_tree->dds->permissions->grant->subject_name->value = ddsrt_strdup(identity_subject);
    permission_document = ddsrt_strdup("");

    rights = ac_local_participant_access_rights_new(identity_handle, domain_id, permission_document, NULL, identity_subject, governance_tree, permissions_tree);
    sanity_check_local_access_rights(rights);
  }
  else if (strlen(governance_uri) > 0 && strlen(permissions_uri) > 0 && strlen(permission_ca_data) > 0)
  {
    /* Retrieve the permission ca certificate from the participant QoS */
    if (!ac_X509_certificate_read(permission_ca_data, &permission_ca, ex))
      goto err_inv_permission_ca;

    /* Retrieve the permissions document from the participant QoS */
    if (!read_document(permissions_uri, &permission_document, ex))
      goto err_read_perm_doc;

    if ((pdlen = strlen(permission_document)) == 0)
    {
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PERMISSION_DOCUMENT_PROPERTY_CODE,
          DDS_SECURITY_VALIDATION_FAILED, DDS_SECURITY_ERR_INVALID_PERMISSION_DOCUMENT_PROPERTY_MESSAGE);
      goto err_read_perm_doc;
    }

    /* Retrieve the governance from the participant QoS */
    if (!read_document(governance_uri, &governance_document, ex))
      goto err_read_gov_doc;

    if ((gvlen = strlen(governance_document)) == 0)
    {
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_GOVERNANCE_DOCUMENT_PROPERTY_CODE,
          DDS_SECURITY_VALIDATION_FAILED, DDS_SECURITY_ERR_INVALID_GOVERNANCE_DOCUMENT_PROPERTY_MESSAGE);
      goto err_read_gov_doc;
    }

    if (!ac_PKCS7_document_check(permission_document, pdlen, permission_ca, &permission_xml, ex))
      goto err_inv_perm_doc;

    if (!ac_PKCS7_document_check(governance_document, gvlen, permission_ca, &governance_xml, ex))
      goto err_inv_gov_doc;

    if (!ac_parse_governance_xml(governance_xml, &governance_tree, ex))
      goto err_inv_gov_xml;

    if (!ac_parse_permissions_xml(permission_xml, &permissions_tree, ex))
    {
      ac_return_governance_tree(governance_tree);
      goto err_inv_perm_xml;
    }

    /* check if subject name of identity certificate matches the subject name in the permissions document */
    if (!validate_subject_name_in_permissions(permissions_tree, identity_subject, &permission_subject, &permission_expiry, ex))
    {
      ac_return_governance_tree(governance_tree);
      ac_return_permissions_tree(permissions_tree);
      goto err_inv_subject;
    }

    rights = ac_local_participant_access_rights_new(identity_handle, domain_id, permission_document, permission_ca, permission_subject, governance_tree, permissions_tree);
    rights->_parent.permissions_expiry = permission_expiry;
    sanity_check_local_access_rights(rights);
  }
  else
  { /*one of them is not empty but the others */
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PARAMETER_CODE, DDS_SECURITY_VALIDATION_FAILED,
        "Governance, Permissions and Permissions CA properties do not exist properly. Either all must be empty or all must be valid");
    goto err_inv_properties;
  }

err_inv_subject:
err_inv_perm_xml:
err_inv_gov_xml:
  ddsrt_free(governance_xml);
err_inv_gov_doc:
  ddsrt_free(permission_xml);
err_inv_perm_doc:
err_read_gov_doc:
  ddsrt_free(governance_document);
err_read_perm_doc:
  if (!rights)
  {
    ddsrt_free(permission_document);
    X509_free(permission_ca);
  }
err_inv_properties:
err_inv_permission_ca:
  ddsrt_free(permission_ca_data);
err_no_permission_ca:
  ddsrt_free(permissions_uri);
err_no_permissions:
  ddsrt_free(governance_uri);
err_no_governance:
  X509_free(identity_cert);
err_inv_identity_cert:
  ddsrt_free(identity_subject);
  ddsrt_free(permission_subject);
  ddsrt_free(identity_cert_data);
err_no_identity_cert:
  return rights;
}

static remote_participant_access_rights *
check_and_create_remote_participant_rights(
    DDS_Security_IdentityHandle remote_identity_handle,
    local_participant_access_rights *local_rights,
    const DDS_Security_PermissionsToken *remote_permissions_token,
    const DDS_Security_AuthenticatedPeerCredentialToken *remote_credential_token,
    DDS_Security_SecurityException *ex)
{
  remote_participant_access_rights *rights = NULL;
  X509 *identity_cert = NULL;
  const DDS_Security_Property_t *identity_cert_property;
  const DDS_Security_Property_t *permission_doc_property;
  char *identity_subject = NULL;
  char *permissions_xml = NULL;
  remote_permissions *permissions = NULL;
  char *permission_subject = NULL;
  dds_time_t permission_expiry = DDS_TIME_INVALID;
  size_t len;

  /* Retrieve the remote identity certificate from the remote_credential_token */
  identity_cert_property = DDS_Security_DataHolder_find_property(remote_credential_token, DDS_ACTOKEN_PROP_C_ID);
  if (!identity_cert_property || !identity_cert_property->value)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_MISSING_PROPERTY_CODE, 0,
        DDS_SECURITY_ERR_MISSING_PROPERTY_MESSAGE, DDS_ACTOKEN_PROP_C_ID);
    goto err_no_identity_cert;
  }

  len = strlen(identity_cert_property->value);
  assert (len <= INT32_MAX);
  if (!ac_X509_certificate_from_data(identity_cert_property->value, (int) len, &identity_cert, ex))
    goto err_inv_identity_cert;

  if (!(identity_subject = ac_get_certificate_subject_name(identity_cert, ex)))
    goto err_inv_identity_cert;

  /* Retrieve the remote permissions document from the remote_credential_token */
  permission_doc_property = DDS_Security_DataHolder_find_property(remote_credential_token, DDS_ACTOKEN_PROP_C_PERM);
  if (!permission_doc_property || !permission_doc_property->value)
  {
    DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_MISSING_PROPERTY_CODE, 0, DDS_SECURITY_ERR_MISSING_PROPERTY_MESSAGE, DDS_ACTOKEN_PROP_C_PERM);
    goto err_inv_perm_doc;
  }

  if (strlen(permission_doc_property->value) == 0)
  {
    /* use default permissions document (all deny) if there is no permissions file
         *to communicate with access_control=false and comply with previous release */
    struct domain_rule *domainRule = find_domain_rule_in_governance(local_rights->governance_tree->dds->domain_access_rules->domain_rule, local_rights->domain_id);
    if (domainRule && !domainRule->enable_join_access_control->value)
    {
      permissions_xml = ddsrt_str_replace(DDS_SECURITY_DEFAULT_PERMISSIONS, "DEFAULT_SUBJECT", identity_subject, 1);
    }
    else
    {
      DDS_Security_Exception_set(ex, DDS_ACCESS_CONTROL_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_PERMISSION_DOCUMENT_PROPERTY_CODE, 0, DDS_SECURITY_ERR_INVALID_PERMISSION_DOCUMENT_PROPERTY_MESSAGE);
      goto err_inv_perm_doc;
    }
  }
  else
  {
    if (!ac_PKCS7_document_check(permission_doc_property->value, strlen(permission_doc_property->value), local_rights->permissions_ca, &permissions_xml, ex))
      goto err_inv_perm_doc;
  }

  permissions = ddsrt_malloc(sizeof(remote_permissions));
  permissions->ref_cnt = 0;
  permissions->permissions_tree = NULL;
  permissions->remote_permissions_token_class_id = NULL;
  if (!ac_parse_permissions_xml(permissions_xml, &(permissions->permissions_tree), ex))
  {
    ddsrt_free(permissions);
    goto err_inv_perm_xml;
  }

  /* check if subject name of identity certificate matches the subject name in the permissions document */
  if (!validate_subject_name_in_permissions(permissions->permissions_tree, identity_subject, &permission_subject, &permission_expiry, ex))
  {
    ac_return_permissions_tree(permissions->permissions_tree);
    ddsrt_free(permissions);
    goto err_inv_subject;
  }
  rights = ac_remote_participant_access_rights_new(remote_identity_handle, local_rights, permissions, permission_expiry, remote_permissions_token, permission_subject);
  sanity_check_remote_access_rights(rights);
  ddsrt_free(permission_subject);

err_inv_subject:
err_inv_perm_xml:
  ddsrt_free(permissions_xml);
err_inv_perm_doc:
  X509_free(identity_cert);
err_inv_identity_cert:
  ddsrt_free(identity_subject);
err_no_identity_cert:
  return rights;
}

static TOPIC_TYPE
get_topic_type(
    const char *topic_name)
{
  TOPIC_TYPE type = TOPIC_TYPE_USER;
  assert(topic_name);

  /* All builtin topics start with "DCPS" */
  if (strncmp(topic_name, "DCPS", 4) == 0)
  {
    /* There are a number of builtin topics starting with "DCPSParticipant" */
    if (strncmp(&(topic_name[4]), &DDS_BUILTIN_TOPIC_PARTICIPANT_NAME[4], 11) == 0)
    {
      if (strcmp(&(topic_name[15]), "") == 0)
        type = TOPIC_TYPE_NON_SECURE_BUILTIN; /* DCPSParticipant */
      else if (strcmp(&(topic_name[15]), &DDS_BUILTIN_TOPIC_PARTICIPANT_MESSAGE_NAME[15]) == 0)
        type = TOPIC_TYPE_NON_SECURE_BUILTIN; /* DCPSParticipantMessage */
      else if (strcmp(&(topic_name[15]), &DDS_BUILTIN_TOPIC_PARTICIPANT_MESSAGE_SECURE_NAME[15]) == 0)
        type = TOPIC_TYPE_SECURE_ParticipantMessageSecure; /* DCPSParticipantMessageSecure */
      else if (strcmp(&(topic_name[15]), &DDS_BUILTIN_TOPIC_PARTICIPANT_VOLATILE_MESSAGE_SECURE_NAME[15]) == 0)
        type = TOPIC_TYPE_SECURE_ParticipantVolatileMessageSecure; /* DCPSParticipantVolatileMessageSecure */
      else if (strcmp(&(topic_name[15]), &DDS_BUILTIN_TOPIC_PARTICIPANT_STATELESS_MESSAGE_NAME[15]) == 0)
        type = TOPIC_TYPE_SECURE_ParticipantStatelessMessage; /* DCPSParticipantStatelessMessage */
      else if (strcmp(&(topic_name[15]), &DDS_BUILTIN_TOPIC_PARTICIPANT_SECURE_NAME[15]) == 0)
        type = TOPIC_TYPE_SECURE_ParticipantsSecure; /* DCPSParticipantsSecure */
    }
    else if (strcmp(&(topic_name[4]), &DDS_BUILTIN_TOPIC_SUBSCRIPTION_SECURE_NAME[4]) == 0)
      type = TOPIC_TYPE_SECURE_SubscriptionsSecure; /* DCPSSubscriptionsSecure */
    else if (strcmp(&(topic_name[4]), &DDS_BUILTIN_TOPIC_PUBLICATION_SECURE_NAME[4]) == 0)
      type = TOPIC_TYPE_SECURE_PublicationsSecure; /* DCPSPublicationsSecure */
    else if (strcmp(&(topic_name[4]), &DDS_BUILTIN_TOPIC_TOPIC_NAME[4]) == 0 ||
             strcmp(&(topic_name[4]), &DDS_BUILTIN_TOPIC_PUBLICATION_NAME[4]) == 0 ||
             strcmp(&(topic_name[4]), &DDS_BUILTIN_TOPIC_SUBSCRIPTION_NAME[4]) == 0 ||
             strcmp(&(topic_name[4]), &DDS_BUILTIN_TOPIC_TYPELOOKUP_REQUEST_NAME[4]) == 0 ||
             strcmp(&(topic_name[4]), &DDS_BUILTIN_TOPIC_TYPELOOKUP_REPLY_NAME[4]) == 0)
    {
      /* DCPSTopic        */
      /* DCPSPublication  */
      /* DCPSSubscription */
      type = TOPIC_TYPE_NON_SECURE_BUILTIN;
    }
  }
  return type;
}

int finalize_access_control(void *context)
{
  dds_security_access_control_impl *access_control = context;
  if (access_control)
  {
    dds_security_timed_dispatcher_free(access_control->dispatcher);
    access_control_table_free(access_control->remote_permissions);
#ifdef ACCESS_CONTROL_USE_ONE_PERMISSION
    if (access_control->local_access_rights)
      access_control_object_free((AccessControlObject *)access_control->local_access_rights);
#else
    access_control_table_free(access_control->local_permissions);
#endif
    ddsrt_mutex_destroy(&access_control->lock);
    ddsrt_free(access_control);
  }
  return 0;
}
