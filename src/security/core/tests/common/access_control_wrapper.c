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
#include <stdio.h>
#include "CUnit/Test.h"
#include "dds/dds.h"
#include "dds/ddsrt/circlist.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/sync.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "access_control_wrapper.h"

int init_access_control(const char *argument, void **context, struct ddsi_domaingv *gv);
int finalize_access_control(void *context);

enum ac_plugin_mode {
  PLUGIN_MODE_ALL_OK,
  PLUGIN_MODE_WRAPPED,
  PLUGIN_MODE_NOT_ALLOWED,
  PLUGIN_MODE_MISSING_FUNC,
};

enum ac_plugin_not_allowed {
  NOT_ALLOWED_ID_LOCAL_PP,
  NOT_ALLOWED_ID_LOCAL_TOPIC,
  NOT_ALLOWED_ID_LOCAL_WRITER,
  NOT_ALLOWED_ID_LOCAL_READER,
  NOT_ALLOWED_ID_LOCAL_PERM,
  NOT_ALLOWED_ID_REMOTE_PP,
  NOT_ALLOWED_ID_REMOTE_TOPIC,
  NOT_ALLOWED_ID_REMOTE_WRITER,
  NOT_ALLOWED_ID_REMOTE_READER,
  NOT_ALLOWED_ID_REMOTE_READER_RELAY_ONLY,
  NOT_ALLOWED_ID_REMOTE_PERM,
};

#define NOT_ALLOWED_LOCAL_PP                      (1u << NOT_ALLOWED_ID_LOCAL_PP)
#define NOT_ALLOWED_LOCAL_TOPIC                   (1u << NOT_ALLOWED_ID_LOCAL_TOPIC)
#define NOT_ALLOWED_LOCAL_WRITER                  (1u << NOT_ALLOWED_ID_LOCAL_WRITER)
#define NOT_ALLOWED_LOCAL_READER                  (1u << NOT_ALLOWED_ID_LOCAL_READER)
#define NOT_ALLOWED_LOCAL_PERM                    (1u << NOT_ALLOWED_ID_LOCAL_PERM)
#define NOT_ALLOWED_REMOTE_PP                     (1u << NOT_ALLOWED_ID_REMOTE_PP)
#define NOT_ALLOWED_REMOTE_TOPIC                  (1u << NOT_ALLOWED_ID_REMOTE_TOPIC)
#define NOT_ALLOWED_REMOTE_WRITER                 (1u << NOT_ALLOWED_ID_REMOTE_WRITER)
#define NOT_ALLOWED_REMOTE_READER                 (1u << NOT_ALLOWED_ID_REMOTE_READER)
#define NOT_ALLOWED_REMOTE_READER_RELAY_ONLY      (1u << NOT_ALLOWED_ID_REMOTE_READER_RELAY_ONLY)
#define NOT_ALLOWED_REMOTE_PERM                   (1u << NOT_ALLOWED_ID_REMOTE_PERM)

struct returns_log_data {
  struct ddsrt_circlist_elem e;
  void * obj;
};


/**
 * Implementation structure for storing encapsulated members of the instance
 * while giving only the interface definition to user
 */
struct dds_security_access_control_impl {
  dds_security_access_control base;
  dds_security_access_control *instance;
  enum ac_plugin_mode mode;
  uint32_t not_allowed_mask;
  ddsrt_mutex_t returns_log_lock;
  struct ddsrt_circlist returns_log;
  bool invalid_return;
};


static void init_returns_log(struct dds_security_access_control_impl *impl)
{
  ddsrt_mutex_init (&impl->returns_log_lock);
  ddsrt_circlist_init (&impl->returns_log);
  impl->invalid_return = false;
}

static void fini_returns_log(struct dds_security_access_control_impl *impl)
{
  ddsrt_mutex_lock (&impl->returns_log_lock);
  while (!ddsrt_circlist_isempty (&impl->returns_log))
  {
    struct ddsrt_circlist_elem *list_elem = ddsrt_circlist_oldest (&impl->returns_log);
    ddsrt_circlist_remove (&impl->returns_log, list_elem);
    ddsrt_free (list_elem);
  }
  ddsrt_mutex_unlock (&impl->returns_log_lock);
  ddsrt_mutex_destroy (&impl->returns_log_lock);
}

static void register_return_obj (struct dds_security_access_control_impl * impl, void * obj)
{
  assert(impl->mode == PLUGIN_MODE_WRAPPED);
  ddsrt_mutex_lock (&impl->returns_log_lock);
  struct returns_log_data * attr_data = ddsrt_malloc (sizeof (*attr_data));
  attr_data->obj = obj;
  ddsrt_circlist_append(&impl->returns_log, &attr_data->e);
  ddsrt_mutex_unlock (&impl->returns_log_lock);
}

static struct ddsrt_circlist_elem *find_return_obj_data (struct dds_security_access_control_impl * impl, void * obj)
{
  struct ddsrt_circlist_elem *elem0 = ddsrt_circlist_oldest (&impl->returns_log), *elem = elem0;
  if (elem != NULL)
  {
    do
    {
      struct returns_log_data *data = DDSRT_FROM_CIRCLIST (struct returns_log_data, e, elem);
      if (data->obj == obj)
        return elem;
      elem = elem->next;
    } while (elem != elem0);
  }
  return NULL;
}

static void unregister_return_obj (struct dds_security_access_control_impl * impl, void * obj)
{
  struct ddsrt_circlist_elem *elem;
  assert(impl->mode == PLUGIN_MODE_WRAPPED);
  ddsrt_mutex_lock (&impl->returns_log_lock);
  if ((elem = find_return_obj_data (impl, obj)) != NULL)
  {
    ddsrt_circlist_remove (&impl->returns_log, elem);
    ddsrt_free (elem);
  }
  else
  {
    impl->invalid_return = true;
    printf ("invalid return %p\n", obj);
  }
  ddsrt_mutex_unlock (&impl->returns_log_lock);
}

static bool all_returns_valid (struct dds_security_access_control_impl * impl)
{
  assert(impl->mode == PLUGIN_MODE_WRAPPED);
  ddsrt_mutex_lock (&impl->returns_log_lock);
  bool valid = !impl->invalid_return && ddsrt_circlist_isempty (&impl->returns_log);
  ddsrt_mutex_unlock (&impl->returns_log_lock);
  return valid;
}

static DDS_Security_PermissionsHandle validate_local_permissions(
    dds_security_access_control *instance,
    const dds_security_authentication *auth_plugin,
    const DDS_Security_IdentityHandle identity,
    const DDS_Security_DomainId domain_id,
    const DDS_Security_Qos *participant_qos,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_NOT_ALLOWED:
      if (impl->not_allowed_mask & NOT_ALLOWED_LOCAL_PERM)
      {
        ex->code = 1;
        ex->message = ddsrt_strdup ("not_allowed: validate_local_permissions");
        return 0;
      }
      /* fall through */
    case PLUGIN_MODE_WRAPPED:
    {
      DDS_Security_PermissionsHandle handle = impl->instance->validate_local_permissions(impl->instance, auth_plugin, identity, domain_id, participant_qos, ex);
      return handle;
    }

    default:
      return 1;
  }
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
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_NOT_ALLOWED:
      if (impl->not_allowed_mask & NOT_ALLOWED_REMOTE_PERM)
      {
        ex->code = 1;
        ex->message = ddsrt_strdup ("not_allowed: validate_remote_permissions");
        return 0;
      }
      /* fall through */
    case PLUGIN_MODE_WRAPPED:
    {
      DDS_Security_PermissionsHandle handle = impl->instance->validate_remote_permissions(impl->instance, auth_plugin, local_identity_handle, remote_identity_handle,
        remote_permissions_token, remote_credential_token, ex);
      return handle;
    }

    default:
      return 0;
  }
}

static DDS_Security_boolean check_create_participant(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const DDS_Security_Qos *participant_qos,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_NOT_ALLOWED:
      if (impl->not_allowed_mask & NOT_ALLOWED_LOCAL_PP)
      {
        ex->code = 1;
        ex->message = ddsrt_strdup ("not_allowed: check_create_participant");
        return false;
      }
      /* fall through */
    case PLUGIN_MODE_WRAPPED:
      return impl->instance->check_create_participant(impl->instance, permissions_handle, domain_id, participant_qos, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean check_create_datawriter(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const char *topic_name,
    const DDS_Security_Qos *writer_qos,
    const DDS_Security_PartitionQosPolicy *partition,
    const DDS_Security_DataTags *data_tag,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_NOT_ALLOWED:
      if (impl->not_allowed_mask & NOT_ALLOWED_LOCAL_WRITER)
      {
        if (topic_name && strncmp (topic_name, AC_WRAPPER_TOPIC_PREFIX, strlen (AC_WRAPPER_TOPIC_PREFIX)) == 0)
        {
          ex->code = 1;
          ex->message = ddsrt_strdup ("not_allowed: check_create_datawriter");
          return false;
        }
      }
      /* fall through */
    case PLUGIN_MODE_WRAPPED:
      return impl->instance->check_create_datawriter(impl->instance, permissions_handle, domain_id, topic_name, writer_qos, partition, data_tag, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean check_create_datareader(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const char *topic_name,
    const DDS_Security_Qos *reader_qos,
    const DDS_Security_PartitionQosPolicy *partition,
    const DDS_Security_DataTags *data_tag,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_NOT_ALLOWED:
      if (impl->not_allowed_mask & NOT_ALLOWED_LOCAL_READER)
      {
        ex->code = 1;
        ex->message = ddsrt_strdup ("not_allowed: check_create_datareader");
        return false;
      }
      /* fall through */
    case PLUGIN_MODE_WRAPPED:
      return impl->instance->check_create_datareader(impl->instance, permissions_handle, domain_id, topic_name, reader_qos, partition, data_tag, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean check_create_topic(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const char *topic_name,
    const DDS_Security_Qos *qos,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_NOT_ALLOWED:
      if (impl->not_allowed_mask & NOT_ALLOWED_LOCAL_TOPIC)
      {
        ex->code = 1;
        ex->message = ddsrt_strdup ("not_allowed: check_create_topic");
        return false;
      }
      /* fall through */
    case PLUGIN_MODE_WRAPPED:
      return impl->instance->check_create_topic(impl->instance, permissions_handle, domain_id, topic_name, qos, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean check_local_datawriter_register_instance(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_Entity *writer,
    const DDS_Security_DynamicData *key,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->check_local_datawriter_register_instance(impl->instance, permissions_handle, writer, key, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean check_local_datawriter_dispose_instance(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_Entity *writer,
    const DDS_Security_DynamicData key,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->check_local_datawriter_dispose_instance(impl->instance, permissions_handle, writer, key, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean check_remote_participant(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const DDS_Security_ParticipantBuiltinTopicDataSecure *participant_data,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_NOT_ALLOWED:
      if (impl->not_allowed_mask & NOT_ALLOWED_REMOTE_PP)
      {
        ex->code = 1;
        ex->message = ddsrt_strdup ("not_allowed: check_remote_participant");
        return false;
      }
      /* fall through */
    case PLUGIN_MODE_WRAPPED:
      return impl->instance->check_remote_participant(impl->instance, permissions_handle, domain_id, participant_data, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean check_remote_datawriter(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const DDS_Security_PublicationBuiltinTopicDataSecure *publication_data,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_NOT_ALLOWED:
      if (impl->not_allowed_mask & NOT_ALLOWED_REMOTE_WRITER)
      {
        ex->code = 1;
        ex->message = ddsrt_strdup ("not_allowed: check_remote_datawriter");
        return false;
      }
      /* fall through */
    case PLUGIN_MODE_WRAPPED:
      return impl->instance->check_remote_datawriter(impl->instance, permissions_handle, domain_id, publication_data, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean check_remote_datareader(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const DDS_Security_SubscriptionBuiltinTopicDataSecure *subscription_data,
    DDS_Security_boolean *relay_only,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_NOT_ALLOWED:
      if (impl->not_allowed_mask & NOT_ALLOWED_REMOTE_READER)
      {
        if (subscription_data->topic_name && strncmp (subscription_data->topic_name, AC_WRAPPER_TOPIC_PREFIX, strlen (AC_WRAPPER_TOPIC_PREFIX)) == 0)
        {
          ex->code = 1;
          ex->message = ddsrt_strdup ("not_allowed: check_remote_datareader");
          return false;
        }
      }
      /* fall through */
    case PLUGIN_MODE_WRAPPED:
    {
      bool ret;
      if ((ret = impl->instance->check_remote_datareader(impl->instance, permissions_handle, domain_id, subscription_data, relay_only, ex)))
      {
        /* Only relay_only for the user reader, not the builtin ones. */
        if (impl->mode == PLUGIN_MODE_NOT_ALLOWED && impl->not_allowed_mask & NOT_ALLOWED_REMOTE_READER_RELAY_ONLY)
        {
          if (subscription_data->topic_name && strncmp (subscription_data->topic_name, AC_WRAPPER_TOPIC_PREFIX, strlen (AC_WRAPPER_TOPIC_PREFIX)) == 0)
            *relay_only = true;
        }
      }
      return ret;
    }

    default:
      *relay_only = false;
      return true;
  }
}

static DDS_Security_boolean check_remote_topic(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_DomainId domain_id,
    const DDS_Security_TopicBuiltinTopicData *topic_data,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_NOT_ALLOWED:
      if (impl->not_allowed_mask & NOT_ALLOWED_REMOTE_TOPIC)
      {
        ex->code = 1;
        ex->message = ddsrt_strdup ("not_allowed: check_remote_topic");
        return false;
      }
      /* fall through */
    case PLUGIN_MODE_WRAPPED:
      return impl->instance->check_remote_topic(impl->instance, permissions_handle, domain_id, topic_data, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean check_local_datawriter_match(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle writer_permissions_handle,
    const DDS_Security_PermissionsHandle reader_permissions_handle,
    const DDS_Security_PublicationBuiltinTopicDataSecure *publication_data,
    const DDS_Security_SubscriptionBuiltinTopicDataSecure *subscription_data,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->check_local_datawriter_match(impl->instance, writer_permissions_handle, reader_permissions_handle, publication_data, subscription_data, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean check_local_datareader_match(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle reader_permissions_handle,
    const DDS_Security_PermissionsHandle writer_permissions_handle,
    const DDS_Security_SubscriptionBuiltinTopicDataSecure *subscription_data,
    const DDS_Security_PublicationBuiltinTopicDataSecure *publication_data,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->check_local_datareader_match(impl->instance, reader_permissions_handle, writer_permissions_handle, subscription_data, publication_data, ex);

    default:
      return true;
  }
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
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->check_remote_datawriter_register_instance(impl->instance, permissions_handle, reader, publication_handle, key, instance_handle, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean check_remote_datawriter_dispose_instance(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const DDS_Security_Entity *reader,
    const DDS_Security_InstanceHandle publication_handle,
    const DDS_Security_DynamicData key,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->check_remote_datawriter_dispose_instance(impl->instance, permissions_handle, reader, publication_handle, key, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean get_permissions_token(
    dds_security_access_control *instance,
    DDS_Security_PermissionsToken *permissions_token,
    const DDS_Security_PermissionsHandle handle,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
      register_return_obj (impl, (void*) permissions_token);
      /* fall through */
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->get_permissions_token(impl->instance, permissions_token, handle, ex);

    default:
      memset(permissions_token, 0, sizeof(*permissions_token));
      permissions_token->class_id = ddsrt_strdup ("");
      return true;
  }
}

static DDS_Security_boolean get_permissions_credential_token(
    dds_security_access_control *instance,
    DDS_Security_PermissionsCredentialToken *permissions_credential_token,
    const DDS_Security_PermissionsHandle handle,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
      register_return_obj (impl, (void*) permissions_credential_token);
      /* fall through */
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->get_permissions_credential_token(impl->instance, permissions_credential_token, handle, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean set_listener(
    dds_security_access_control *instance,
    const dds_security_access_control_listener *listener,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->set_listener (impl->instance, listener, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean return_permissions_token(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsToken *token,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
      unregister_return_obj (impl, (void*) token);
      /* fall through */
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->return_permissions_token (impl->instance, token, ex);

    default:
      ddsrt_free (token->class_id);
      return true;
  }
}

static DDS_Security_boolean return_permissions_credential_token(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsCredentialToken *permissions_credential_token,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
      unregister_return_obj (impl, (void*) permissions_credential_token);
      /* fall through */
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->return_permissions_credential_token(impl->instance, permissions_credential_token, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean get_participant_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    DDS_Security_ParticipantSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
      register_return_obj (impl, (void*) attributes);
      /* fall through */
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->get_participant_sec_attributes(impl->instance, permissions_handle, attributes, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean get_topic_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    const char *topic_name,
    DDS_Security_TopicSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
      register_return_obj (impl, (void*) attributes);
      /* fall through */
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->get_topic_sec_attributes(impl->instance, permissions_handle, topic_name, attributes, ex);

    default:
      return true;
  }
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
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
      register_return_obj (impl, (void*) attributes);
      /* fall through */
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->get_datawriter_sec_attributes(impl->instance, permissions_handle, topic_name, partition, data_tag, attributes, ex);

    default:
      return true;
  }
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
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
      register_return_obj (impl, (void*) attributes);
      /* fall through */
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->get_datareader_sec_attributes(impl->instance, permissions_handle, topic_name, partition, data_tag, attributes, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean return_participant_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_ParticipantSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
      unregister_return_obj (impl, (void*) attributes);
      /* fall through */
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->return_participant_sec_attributes(impl->instance, attributes, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean return_topic_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_TopicSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
      unregister_return_obj (impl, (void*) attributes);
      /* fall through */
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->return_topic_sec_attributes(impl->instance, attributes, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean return_datawriter_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_EndpointSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
      unregister_return_obj (impl, (void*) attributes);
      /* fall through */
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->return_datawriter_sec_attributes(impl->instance, attributes, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean return_datareader_sec_attributes(
    dds_security_access_control *instance,
    const DDS_Security_EndpointSecurityAttributes *attributes,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
      unregister_return_obj (impl, (void*) attributes);
      /* fall through */
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->return_datareader_sec_attributes(impl->instance, attributes, ex);

    default:
      return true;
  }
}

static DDS_Security_boolean return_permissions_handle(
    dds_security_access_control *instance,
    const DDS_Security_PermissionsHandle permissions_handle,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_access_control_impl *impl = (struct dds_security_access_control_impl *)instance;
  switch (impl->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_NOT_ALLOWED:
      return impl->instance->return_permissions_handle(impl->instance, permissions_handle, ex);

    default:
      return true;
  }
}

static struct dds_security_access_control_impl * init_test_access_control_common(const char *argument, bool wrapped, struct ddsi_domaingv *gv)
{
  struct dds_security_access_control_impl *impl = ddsrt_malloc(sizeof(*impl));
  memset(impl, 0, sizeof(*impl));

  if (wrapped)
  {
    if (init_access_control(argument, (void **)&impl->instance, gv) != DDS_SECURITY_SUCCESS)
      return NULL;
  }

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

static int finalize_test_access_control_common(struct dds_security_access_control_impl * impl, bool wrapped)
{
  int32_t ret;
  if (wrapped && (ret = finalize_access_control(impl->instance)) != DDS_SECURITY_SUCCESS)
    return ret;
  ddsrt_free(impl);
  return DDS_SECURITY_SUCCESS;
}

int init_test_access_control_all_ok(const char *argument, void **context, struct ddsi_domaingv *gv)
{
  DDSRT_UNUSED_ARG(context);
  struct dds_security_access_control_impl *impl = init_test_access_control_common(argument, false, gv);
  impl->mode = PLUGIN_MODE_ALL_OK;
  *context = impl;
  return 0;
}

int finalize_test_access_control_all_ok(void *context)
{
  struct dds_security_access_control_impl* impl = (struct dds_security_access_control_impl*) context;
  assert(impl->mode == PLUGIN_MODE_ALL_OK);
  return finalize_test_access_control_common(impl, false);
}

int init_test_access_control_wrapped(const char *argument, void **context, struct ddsi_domaingv *gv)
{
  struct dds_security_access_control_impl *impl = init_test_access_control_common(argument, true, gv);
  if (!impl)
    return DDS_SECURITY_FAILED;
  impl->mode = PLUGIN_MODE_WRAPPED;
  init_returns_log (impl);
  *context = impl;
  return DDS_SECURITY_SUCCESS;
}

int finalize_test_access_control_wrapped(void *context)
{
  struct dds_security_access_control_impl* impl = (struct dds_security_access_control_impl*) context;
  assert (impl->mode == PLUGIN_MODE_WRAPPED);
  bool returns_valid = all_returns_valid (impl);
  fini_returns_log (impl);
  printf("returns result (impl %p): %s\n", impl, returns_valid ? "all valid" : "invalid");
  CU_ASSERT_FATAL (returns_valid);
  return finalize_test_access_control_common (impl, true);
}

int init_test_access_control_missing_func(const char *argument, void **context, struct ddsi_domaingv *gv)
{
  DDSRT_UNUSED_ARG(context);
  struct dds_security_access_control_impl *impl = init_test_access_control_common(argument, false, gv);
  impl->base.check_create_datareader = NULL;
  impl->mode = PLUGIN_MODE_MISSING_FUNC;
  *context = impl;
  return 0;
}

int finalize_test_access_control_missing_func(void *context)
{
  struct dds_security_access_control_impl* impl = (struct dds_security_access_control_impl*) context;
  assert(impl->mode == PLUGIN_MODE_MISSING_FUNC);
  return finalize_test_access_control_common(impl, false);
}

#define INIT_NOT_ALLOWED(name_, mask_) \
  int init_test_access_control_##name_ (const char *argument, void **context, struct ddsi_domaingv *gv) \
  { \
    DDSRT_UNUSED_ARG(context); \
    struct dds_security_access_control_impl *impl = init_test_access_control_common(argument, true, gv); \
    assert(impl); \
    impl->mode = PLUGIN_MODE_NOT_ALLOWED; \
    impl->not_allowed_mask = mask_; \
    *context = impl; \
    return 0; \
  }

INIT_NOT_ALLOWED(local_participant_not_allowed, NOT_ALLOWED_LOCAL_PP)
INIT_NOT_ALLOWED(local_topic_not_allowed, NOT_ALLOWED_LOCAL_TOPIC)
INIT_NOT_ALLOWED(local_writer_not_allowed, NOT_ALLOWED_LOCAL_WRITER)
INIT_NOT_ALLOWED(local_reader_not_allowed, NOT_ALLOWED_LOCAL_READER)
INIT_NOT_ALLOWED(local_permissions_not_allowed, NOT_ALLOWED_LOCAL_PERM)
INIT_NOT_ALLOWED(remote_participant_not_allowed, NOT_ALLOWED_REMOTE_PP)
INIT_NOT_ALLOWED(remote_topic_not_allowed, NOT_ALLOWED_REMOTE_TOPIC)
INIT_NOT_ALLOWED(remote_writer_not_allowed, NOT_ALLOWED_REMOTE_WRITER)
INIT_NOT_ALLOWED(remote_reader_not_allowed, NOT_ALLOWED_REMOTE_READER)
INIT_NOT_ALLOWED(remote_reader_relay_only, NOT_ALLOWED_REMOTE_READER_RELAY_ONLY)
INIT_NOT_ALLOWED(remote_permissions_not_allowed, NOT_ALLOWED_REMOTE_PERM)

int finalize_test_access_control_not_allowed(void *context)
{
  struct dds_security_access_control_impl* impl = (struct dds_security_access_control_impl*) context;
  assert(impl->mode == PLUGIN_MODE_NOT_ALLOWED);
  return finalize_test_access_control_common(impl, true);
}
