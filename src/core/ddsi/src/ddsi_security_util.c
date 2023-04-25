// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/features.h"
#ifdef DDS_HAS_SECURITY

#include <string.h>
#include <stdarg.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/bswap.h"

#include "ddsi__security_util.h"
#include "ddsi__plist.h"

void
ddsi_omg_shallow_copy_StringSeq(
    DDS_Security_StringSeq *dst,
    const ddsi_stringseq_t *src)
{
  unsigned i;
  assert(dst);
  assert(src);

  dst->_length  = src->n;
  dst->_maximum = src->n;
  dst->_buffer  = NULL;
  if (src->n > 0)
  {
    dst->_buffer = ddsrt_malloc(src->n * sizeof(DDS_Security_string));
    for (i = 0; i < src->n; i++)
      dst->_buffer[i] = src->strs[i];
  }
}

void
ddsi_omg_shallow_free_StringSeq(
    DDS_Security_StringSeq *obj)
{
  if (obj)
    ddsrt_free(obj->_buffer);
}

void
ddsi_omg_copy_PropertySeq (
    DDS_Security_PropertySeq *dst,
    const dds_propertyseq_t *src)
{
  uint32_t i;

  if (src)
  {
    dst->_length = dst->_maximum = src->n;
    if (src->n > 0)
      dst->_buffer = DDS_Security_PropertySeq_allocbuf(src->n);
    else
      dst->_buffer = NULL;

    for (i = 0; i < src->n; i++)
    {
      dst->_buffer[i].name =  src->props->name ? ddsrt_strdup(src->props->name) : ddsrt_strdup("");
      dst->_buffer[i].value = src->props->value ? ddsrt_strdup(src->props->value) : ddsrt_strdup("");
    }
  }
  else
    memset(dst, 0, sizeof(*dst));
}

void
ddsi_omg_shallow_copyin_PropertySeq (
   DDS_Security_PropertySeq *dst,
   const dds_propertyseq_t *src)
{
  unsigned i;
  assert(dst);
  assert(src);

  dst->_length  = src->n;
  dst->_maximum = src->n;
  dst->_buffer  = NULL;

  if (src->n > 0)
  {
    dst->_buffer = ddsrt_malloc(src->n * sizeof(DDS_Security_Property_t));
    for (i = 0; i < src->n; i++)
    {
      dst->_buffer[i].name      = src->props[i].name;
      dst->_buffer[i].value     = src->props[i].value;
      dst->_buffer[i].propagate = src->props[i].propagate;
    }
  }
}

void
ddsi_omg_shallow_copyout_PropertySeq (
    dds_propertyseq_t *dst,
    const DDS_Security_PropertySeq *src)
{
  unsigned i;
  assert(dst);
  assert(src);

  dst->n = src->_length;
  dst->props = NULL;

  if (src->_length > 0)
  {
    dst->props = ddsrt_malloc(src->_length * sizeof(dds_property_t));
    for (i = 0; i < src->_length; i++)
    {
      dst->props[i].name      = src->_buffer[i].name;
      dst->props[i].value     = src->_buffer[i].value;
//      dst->props[i].propagate = src->_buffer[i].propagate;
      dst->props[i].propagate = true;
    }
  }
}

void
ddsi_omg_shallow_free_PropertySeq (
    DDS_Security_PropertySeq *obj)
{
  assert(obj);
  ddsrt_free(obj->_buffer);
  obj->_length = 0;
  obj->_maximum = 0;
  obj->_buffer = NULL;
}

static void
ddsi_omg_shallow_free_dds_propertyseq (
    dds_propertyseq_t *obj)
{
  ddsrt_free(obj->props);
  obj->n = 0;
  obj->props = NULL;
}

void
ddsi_omg_shallow_copyin_BinaryPropertySeq (
    DDS_Security_BinaryPropertySeq *dst,
    const dds_binarypropertyseq_t *src)
{
  unsigned i;
  assert(dst);
  assert(src);

  dst->_length  = src->n;
  dst->_maximum = src->n;
  dst->_buffer  = NULL;

  if (src->n > 0)
  {
    dst->_buffer = ddsrt_malloc(src->n * sizeof(DDS_Security_BinaryProperty_t));
    for (i = 0; i < src->n; i++)
    {
      dst->_buffer[i].name           = src->props[i].name;
      dst->_buffer[i].value._length  = src->props[i].value.length;
      dst->_buffer[i].value._maximum = src->props[i].value.length;
      dst->_buffer[i].value._buffer  = src->props[i].value.value;
//      dst->_buffer[i].propagate      = src->props[i].propagate;
      dst->_buffer[i].propagate      = true;
    }
  }
}

void
ddsi_omg_shallow_copyout_BinaryPropertySeq (
    dds_binarypropertyseq_t *dst,
    const DDS_Security_BinaryPropertySeq *src)
{
  unsigned i;
  assert(dst);
  assert(src);

  dst->n  = src->_length;
  dst->props  = NULL;

  if (src->_length > 0)
  {
    dst->props = ddsrt_malloc(src->_length * sizeof(dds_binaryproperty_t));
    for (i = 0; i < src->_length; i++)
    {
      dst->props[i].name         = src->_buffer[i].name;
      dst->props[i].value.length = src->_buffer[i].value._length;
      dst->props[i].value.value  = src->_buffer[i].value._buffer;
      dst->props[i].propagate    = src->_buffer[i].propagate;
    }
  }
}

void
ddsi_omg_shallow_free_BinaryPropertySeq (
    DDS_Security_BinaryPropertySeq *obj)
{
  ddsrt_free(obj->_buffer);
  obj->_length = 0;
  obj->_maximum = 0;
  obj->_buffer = NULL;
}

static void
ddsi_omg_shallow_free_dds_binarypropertyseq (
    dds_binarypropertyseq_t *obj)
{
  ddsrt_free(obj->props);
  obj->n = 0;
  obj->props = NULL;
}

void
ddsi_omg_shallow_copy_PropertyQosPolicy (
    DDS_Security_PropertyQosPolicy *dst,
    const dds_property_qospolicy_t *src)
{
    assert(dst);
    assert(src);
    ddsi_omg_shallow_copyin_PropertySeq (&(dst->value), &(src->value));
    ddsi_omg_shallow_copyin_BinaryPropertySeq (&(dst->binary_value), &(src->binary_value));
}

void
ddsi_omg_shallow_copy_security_qos (
    DDS_Security_Qos *dst,
    const struct dds_qos *src)
{
  assert(src);
  assert(dst);

  /* DataTags not supported yet. */
  memset(&(dst->data_tags), 0, sizeof(DDS_Security_DataTagQosPolicy));

  if (src->present & DDSI_QP_PROPERTY_LIST)
    ddsi_omg_shallow_copy_PropertyQosPolicy (&(dst->property), &(src->property));
  else
    memset(&(dst->property), 0, sizeof(DDS_Security_PropertyQosPolicy));
}

void
ddsi_omg_shallow_free_PropertyQosPolicy (
    DDS_Security_PropertyQosPolicy *obj)
{
  ddsi_omg_shallow_free_PropertySeq (&(obj->value));
  ddsi_omg_shallow_free_BinaryPropertySeq (&(obj->binary_value));
}

void
ddsi_omg_shallow_free_security_qos (
    DDS_Security_Qos *obj)
{
  ddsi_omg_shallow_free_PropertyQosPolicy (&(obj->property));
}

void
ddsi_omg_security_dataholder_copyin (
    ddsi_dataholder_t *dh,
    const DDS_Security_DataHolder *holder)
{
  uint32_t i;

  dh->class_id = holder->class_id ? ddsrt_strdup(holder->class_id) : NULL;
  dh->properties.n = holder->properties._length;
  dh->properties.props = dh->properties.n ? ddsrt_malloc(dh->properties.n * sizeof(dds_property_t)) : NULL;
  for (i = 0; i < dh->properties.n; i++)
  {
    DDS_Security_Property_t *prop = &(holder->properties._buffer[i]);
    dh->properties.props[i].name = prop->name ? ddsrt_strdup(prop->name) : NULL;
    dh->properties.props[i].value = prop->value ? ddsrt_strdup(prop->value) : NULL;
    dh->properties.props[i].propagate = prop->propagate;
  }
  dh->binary_properties.n = holder->binary_properties._length;
  dh->binary_properties.props = dh->binary_properties.n ? ddsrt_malloc(dh->binary_properties.n * sizeof(dds_binaryproperty_t)) : NULL;
  for (i = 0; i < dh->binary_properties.n; i++)
  {
    DDS_Security_BinaryProperty_t *prop = &(holder->binary_properties._buffer[i]);
    dh->binary_properties.props[i].name = prop->name ? ddsrt_strdup(prop->name) : NULL;
    dh->binary_properties.props[i].value.length = prop->value._length;
    if (dh->binary_properties.props[i].value.length)
    {
      dh->binary_properties.props[i].value.value = ddsrt_malloc(prop->value._length);
      memcpy(dh->binary_properties.props[i].value.value, prop->value._buffer, prop->value._length);
    }
    else
    {
      dh->binary_properties.props[i].value.value = NULL;
    }
    dh->binary_properties.props[i].propagate = prop->propagate;
  }
}

void
ddsi_omg_security_dataholder_copyout (
    DDS_Security_DataHolder *holder,
    const ddsi_dataholder_t *dh)
{
  uint32_t i;

  holder->class_id = dh->class_id ? ddsrt_strdup(dh->class_id) : NULL;
  holder->properties._length = holder->properties._maximum = dh->properties.n;
  holder->properties._buffer = dh->properties.n ? DDS_Security_PropertySeq_allocbuf(dh->properties.n) : NULL;
  for (i = 0; i < dh->properties.n; i++)
  {
    dds_property_t *props = &(dh->properties.props[i]);
    holder->properties._buffer[i].name = props->name ? ddsrt_strdup(props->name) : NULL;
    holder->properties._buffer[i].value = props->value ? ddsrt_strdup(props->value) : NULL;
    holder->properties._buffer[i].propagate = props->propagate;
  }
  holder->binary_properties._length = holder->binary_properties._maximum = dh->binary_properties.n;
  holder->binary_properties._buffer = dh->binary_properties.n ? DDS_Security_BinaryPropertySeq_allocbuf(dh->binary_properties.n) : NULL;
  for (i = 0; i < dh->binary_properties.n; i++)
  {
    dds_binaryproperty_t *props = &(dh->binary_properties.props[i]);
    holder->binary_properties._buffer[i].name = props->name ? ddsrt_strdup(props->name) : NULL;
    holder->binary_properties._buffer[i].value._length = holder->binary_properties._buffer[i].value._maximum = props->value.length;
    if (props->value.length)
    {
      holder->binary_properties._buffer[i].value._buffer = ddsrt_malloc(props->value.length);
      memcpy(holder->binary_properties._buffer[i].value._buffer, props->value.value, props->value.length);
    }
    else
    {
      holder->binary_properties._buffer[i].value._buffer= NULL;
    }
    holder->binary_properties._buffer[i].propagate = props->propagate;
  }
}

void
ddsi_omg_shallow_copyin_DataHolder (
    DDS_Security_DataHolder *dst,
    const ddsi_dataholder_t *src)
{
    assert(dst);
    assert(src);
    dst->class_id = src->class_id;
    ddsi_omg_shallow_copyin_PropertySeq (&dst->properties, &src->properties);
    ddsi_omg_shallow_copyin_BinaryPropertySeq (&dst->binary_properties, &src->binary_properties);
}

void
ddsi_omg_shallow_copyout_DataHolder (
    ddsi_dataholder_t *dst,
    const DDS_Security_DataHolder *src)
{
    assert(dst);
    assert(src);
    dst->class_id = src->class_id;
    ddsi_omg_shallow_copyout_PropertySeq (&dst->properties, &src->properties);
    ddsi_omg_shallow_copyout_BinaryPropertySeq (&dst->binary_properties, &src->binary_properties);
}

void
ddsi_omg_shallow_free_DataHolder (
    DDS_Security_DataHolder *obj)
{
    ddsi_omg_shallow_free_PropertySeq (&obj->properties);
    ddsi_omg_shallow_free_BinaryPropertySeq (&obj->binary_properties);
}

void
ddsi_omg_shallow_free_ddsi_dataholder (
    ddsi_dataholder_t *holder)
{
  ddsi_omg_shallow_free_dds_propertyseq (&holder->properties);
  ddsi_omg_shallow_free_dds_binarypropertyseq (&holder->binary_properties);
}

void
ddsi_omg_shallow_copyin_DataHolderSeq (
    DDS_Security_DataHolderSeq *dst,
    const ddsi_dataholderseq_t *src)
{
  unsigned i;

  dst->_length  = src->n;
  dst->_maximum = src->n;
  dst->_buffer  = NULL;

  if (src->n > 0)
  {
    dst->_buffer = ddsrt_malloc(src->n * sizeof(DDS_Security_DataHolder));
    for (i = 0; i < src->n; i++)
    {
      ddsi_omg_shallow_copyin_DataHolder (&dst->_buffer[i], &src->tags[i]);
    }
  }
}

void
ddsi_omg_copyin_DataHolderSeq (
    DDS_Security_DataHolderSeq *dst,
    const ddsi_dataholderseq_t *src)
{
  unsigned i;

  dst->_length  = src->n;
  dst->_maximum = src->n;
  dst->_buffer  = NULL;

  if (src->n > 0)
  {
    dst->_buffer = ddsrt_malloc(src->n * sizeof(DDS_Security_DataHolder));
    for (i = 0; i < src->n; i++)
    {
      ddsi_omg_security_dataholder_copyout (&dst->_buffer[i], &src->tags[i]);
    }
  }
}



void
ddsi_omg_shallow_copyout_DataHolderSeq (
    ddsi_dataholderseq_t  *dst,
    const DDS_Security_DataHolderSeq *src)
{
  unsigned i;

  dst->n  = src->_length;
  dst->tags  = NULL;

  if (src->_length > 0)
  {
    dst->tags = ddsrt_malloc(src->_length * sizeof(ddsi_dataholder_t));
    for (i = 0; i < src->_length; i++)
    {
      ddsi_omg_shallow_copyout_DataHolder (&dst->tags[i], &src->_buffer[i]);
    }
  }
}

void
ddsi_omg_shallow_free_DataHolderSeq (
    DDS_Security_DataHolderSeq *obj)
{
  unsigned i;

  for (i = 0; i  < obj->_length; i++)
  {
    ddsi_omg_shallow_free_DataHolder (&(obj->_buffer[i]));
  }
}

void
ddsi_omg_shallow_free_ddsi_dataholderseq (
    ddsi_dataholderseq_t *obj)
{
  unsigned i;

  for (i = 0; i  < obj->n; i++)
  {
    ddsi_omg_shallow_free_ddsi_dataholder (&(obj->tags[i]));
  }
  if (obj->n > 0)
    ddsrt_free(obj->tags);
}

static DDS_Security_Duration_t convert_duration(dds_duration_t d)
{
  DDS_Security_Duration_t sd;

  if (d == DDS_INFINITY)
  {
    sd.sec = INT32_MAX;
    sd.nanosec = INT32_MAX;
  }
  else
  {
    sd.sec = ((int)(d/DDS_NSECS_IN_SEC));
    sd.nanosec = ((uint32_t)((d)%DDS_NSECS_IN_SEC));
  }
  return sd;
}

static void
g_omg_shallow_copy_octSeq(
    DDS_Security_OctetSeq *dst,
    const ddsi_octetseq_t *src)
{
  dst->_length  = src->length;
  dst->_maximum = src->length;
  dst->_buffer  = src->value;
}

static void
g_omg_shallow_free_octSeq(
    DDS_Security_OctetSeq *obj)
{
  DDSRT_UNUSED_ARG(obj);
  /* Nothing to free. */
}

void
ddsi_omg_shallow_copy_ParticipantBuiltinTopicDataSecure (
    DDS_Security_ParticipantBuiltinTopicDataSecure *dst,
    const ddsi_guid_t *guid,
    const ddsi_plist_t *plist)
{
  assert(dst);
  assert(guid);
  assert(plist);

  memset(dst, 0, sizeof(DDS_Security_ParticipantBuiltinTopicDataSecure));

  /* The participant guid is the key. */
  dst->key[0] = guid->prefix.u[0];
  dst->key[1] = guid->prefix.u[1];
  dst->key[2] = guid->prefix.u[2];

  /* Copy the DDS_Security_OctetSeq content (length, pointer, etc), not the buffer content. */
  if (plist->qos.present & DDSI_QP_USER_DATA)
    g_omg_shallow_copy_octSeq(&dst->user_data.value, &plist->qos.user_data);
  /* Tokens are actually DataHolders. */
  if (plist->present & PP_IDENTITY_TOKEN)
    ddsi_omg_shallow_copyin_DataHolder (&(dst->identity_token), &(plist->identity_token));
  if (plist->present & PP_PERMISSIONS_TOKEN)
    ddsi_omg_shallow_copyin_DataHolder (&(dst->permissions_token), &(plist->permissions_token));
  if (plist->present & PP_IDENTITY_STATUS_TOKEN)
    ddsi_omg_shallow_copyin_DataHolder (&(dst->identity_status_token), &(plist->identity_status_token));
  if (plist->qos.present & DDSI_QP_PROPERTY_LIST)
    ddsi_omg_shallow_copy_PropertyQosPolicy (&(dst->property), &(plist->qos.property));
  if (plist->present & PP_PARTICIPANT_SECURITY_INFO)
  {
    dst->security_info.participant_security_attributes = plist->participant_security_info.security_attributes;
    dst->security_info.plugin_participant_security_attributes = plist->participant_security_info.plugin_security_attributes;
  }
}

void
ddsi_omg_shallow_free_ParticipantBuiltinTopicDataSecure (
    DDS_Security_ParticipantBuiltinTopicDataSecure *obj)
{
  assert(obj);
  ddsi_omg_shallow_free_DataHolder (&(obj->identity_token));
  ddsi_omg_shallow_free_DataHolder (&(obj->permissions_token));
  ddsi_omg_shallow_free_DataHolder (&(obj->identity_status_token));
  ddsi_omg_shallow_free_PropertyQosPolicy (&(obj->property));
}

void
ddsi_omg_shallow_copy_SubscriptionBuiltinTopicDataSecure (
    DDS_Security_SubscriptionBuiltinTopicDataSecure *dst,
    const ddsi_guid_t *guid,
    const struct dds_qos *qos,
    const ddsi_security_info_t *secinfo)
{
  memset(dst, 0, sizeof(DDS_Security_SubscriptionBuiltinTopicDataSecure));

  /* Keys are inspired by write_builtin_topic_copyin_subscriptionInfo() */
  dst->key[0] = ddsrt_toBE4u(guid->prefix.u[0]);
  dst->key[1] = ddsrt_toBE4u(guid->prefix.u[1]);
  dst->key[2] = ddsrt_toBE4u(guid->prefix.u[2]);

  dst->participant_key[0] = ddsrt_toBE4u(guid->prefix.u[0]);
  dst->participant_key[1] = ddsrt_toBE4u(guid->prefix.u[1]);
  dst->participant_key[2] = ddsrt_toBE4u(guid->prefix.u[2]);

  if (qos->present & DDSI_QP_TOPIC_NAME)
    dst->topic_name = (DDS_Security_string)qos->topic_name;
  if (qos->present & DDSI_QP_TYPE_NAME)
    dst->type_name  = (DDS_Security_string)qos->type_name;

  dst->security_info.endpoint_security_mask = secinfo->security_attributes;
  dst->security_info.plugin_endpoint_security_mask = secinfo->plugin_security_attributes;

  if (qos->present & DDSI_QP_DURABILITY)
    dst->durability.kind = (DDS_Security_DurabilityQosPolicyKind)qos->durability.kind;
  if (qos->present & DDSI_QP_DEADLINE)
    dst->deadline.period = convert_duration(qos->deadline.deadline);
  if (qos->present & DDSI_QP_LATENCY_BUDGET)
    dst->latency_budget.duration = convert_duration(qos->latency_budget.duration);
  if (qos->present & DDSI_QP_LIVELINESS)
  {
    dst->liveliness.kind = (DDS_Security_LivelinessQosPolicyKind)qos->liveliness.kind;
    dst->liveliness.lease_duration = convert_duration(qos->liveliness.lease_duration);
  }
  if (qos->present & DDSI_QP_OWNERSHIP)
    dst->ownership.kind = qos->ownership.kind == DDS_OWNERSHIP_SHARED ? DDS_SECURITY_SHARED_OWNERSHIP_QOS : DDS_SECURITY_EXCLUSIVE_OWNERSHIP_QOS;
  if (qos->present & DDSI_QP_DESTINATION_ORDER)
    dst->destination_order.kind = (DDS_Security_DestinationOrderQosPolicyKind)qos->destination_order.kind;
  if (qos->present & DDSI_QP_PRESENTATION)
  {
    dst->presentation.access_scope = (DDS_Security_PresentationQosPolicyAccessScopeKind)qos->presentation.access_scope;
    dst->presentation.coherent_access = qos->presentation.coherent_access;
    dst->presentation.ordered_access = qos->presentation.ordered_access;
  }
  if (qos->present & DDSI_QP_TIME_BASED_FILTER)
    dst->time_based_filter.minimum_separation = convert_duration(qos->time_based_filter.minimum_separation);
  if (qos->present & DDSI_QP_RELIABILITY)
  {
    dst->reliability.kind               = (DDS_Security_ReliabilityQosPolicyKind)(qos->reliability.kind);
    dst->reliability.max_blocking_time  = convert_duration(qos->reliability.max_blocking_time);
    dst->reliability.synchronous        = 0;
  }
  if (qos->present & DDSI_QP_PARTITION)
    ddsi_omg_shallow_copy_StringSeq(&dst->partition.name, &qos->partition);
  if (qos->present & DDSI_QP_USER_DATA)
    g_omg_shallow_copy_octSeq(&dst->user_data.value, &qos->user_data);
  if (qos->present & DDSI_QP_TOPIC_DATA)
    g_omg_shallow_copy_octSeq(&dst->topic_data.value, &qos->topic_data);
  if (qos->present & DDSI_QP_GROUP_DATA)
    g_omg_shallow_copy_octSeq(&dst->group_data.value, &qos->group_data);

  /* The dst->data_tags is not supported yet. It is memset to 0, so ok. */
}

void
ddsi_omg_shallow_free_SubscriptionBuiltinTopicDataSecure (
    DDS_Security_SubscriptionBuiltinTopicDataSecure *obj)
{
  g_omg_shallow_free_octSeq(&obj->user_data.value);
  g_omg_shallow_free_octSeq(&obj->topic_data.value);
  g_omg_shallow_free_octSeq(&obj->group_data.value);
  ddsi_omg_shallow_free_StringSeq(&obj->partition.name);
}

void
ddsi_omg_shallow_copy_PublicationBuiltinTopicDataSecure (
    DDS_Security_PublicationBuiltinTopicDataSecure *dst,
    const ddsi_guid_t *guid,
    const struct dds_qos *qos,
    const ddsi_security_info_t *secinfo)
{

  memset(dst, 0, sizeof(DDS_Security_PublicationBuiltinTopicDataSecure));

  /* Keys are inspired by write_builtin_topic_copyin_subscriptionInfo() */
  dst->key[0] = ddsrt_toBE4u(guid->prefix.u[0]);
  dst->key[1] = ddsrt_toBE4u(guid->prefix.u[1]);
  dst->key[2] = ddsrt_toBE4u(guid->prefix.u[2]);

  dst->participant_key[0] = ddsrt_toBE4u(guid->prefix.u[0]);
  dst->participant_key[1] = ddsrt_toBE4u(guid->prefix.u[1]);
  dst->participant_key[2] = ddsrt_toBE4u(guid->prefix.u[2]);

  if (qos->present & DDSI_QP_TOPIC_NAME)
    dst->topic_name = (DDS_Security_string)qos->topic_name;
  if (qos->present & DDSI_QP_TYPE_NAME)
    dst->type_name  = (DDS_Security_string)qos->type_name;

  dst->security_info.endpoint_security_mask = secinfo->security_attributes;
  dst->security_info.plugin_endpoint_security_mask = secinfo->plugin_security_attributes;

  if (qos->present & DDSI_QP_DURABILITY)
    dst->durability.kind = (DDS_Security_DurabilityQosPolicyKind)qos->durability.kind;
  if (qos->present & DDSI_QP_DEADLINE)
    dst->deadline.period = convert_duration(qos->deadline.deadline);
  if (qos->present & DDSI_QP_LATENCY_BUDGET)
    dst->latency_budget.duration = convert_duration(qos->latency_budget.duration);
  if (qos->present & DDSI_QP_LIVELINESS)
  {
    dst->liveliness.kind = (DDS_Security_LivelinessQosPolicyKind)qos->liveliness.kind;
    dst->liveliness.lease_duration = convert_duration(qos->liveliness.lease_duration);
  }
  if (qos->present & DDSI_QP_OWNERSHIP)
    dst->ownership.kind = qos->ownership.kind == DDS_OWNERSHIP_SHARED ? DDS_SECURITY_SHARED_OWNERSHIP_QOS : DDS_SECURITY_EXCLUSIVE_OWNERSHIP_QOS;
  if (qos->present & DDSI_QP_DESTINATION_ORDER)
    dst->destination_order.kind = (DDS_Security_DestinationOrderQosPolicyKind)qos->destination_order.kind;
  if (qos->present & DDSI_QP_PRESENTATION)
  {
    dst->presentation.access_scope = (DDS_Security_PresentationQosPolicyAccessScopeKind)qos->presentation.access_scope;
    dst->presentation.coherent_access = qos->presentation.coherent_access;
    dst->presentation.ordered_access = qos->presentation.ordered_access;
  }
  if (qos->present & DDSI_QP_OWNERSHIP_STRENGTH)
    dst->ownership_strength.value = qos->ownership_strength.value;
  if (qos->present & DDSI_QP_RELIABILITY)
  {
    dst->reliability.kind              = (DDS_Security_ReliabilityQosPolicyKind)(qos->reliability.kind);
    dst->reliability.max_blocking_time = convert_duration(qos->reliability.max_blocking_time);
    dst->reliability.synchronous       = 0;
  }
  if (qos->present & DDSI_QP_LIFESPAN)
    dst->lifespan.duration = convert_duration(qos->lifespan.duration);
  if (qos->present & DDSI_QP_PARTITION)
    ddsi_omg_shallow_copy_StringSeq(&dst->partition.name, &qos->partition);
  if (qos->present & DDSI_QP_USER_DATA)
    g_omg_shallow_copy_octSeq(&dst->user_data.value, &qos->user_data);

  if (qos->present & DDSI_QP_TOPIC_DATA)
    g_omg_shallow_copy_octSeq(&dst->topic_data.value, &qos->topic_data);
  if (qos->present & DDSI_QP_GROUP_DATA)
    g_omg_shallow_copy_octSeq(&dst->group_data.value, &qos->group_data);

  /* The dst->data_tags is not supported yet. It is memset to 0, so ok. */
}

void
ddsi_omg_shallow_free_PublicationBuiltinTopicDataSecure (
    DDS_Security_PublicationBuiltinTopicDataSecure *obj)
{
  g_omg_shallow_free_octSeq(&obj->user_data.value);
  g_omg_shallow_free_octSeq(&obj->topic_data.value);
  g_omg_shallow_free_octSeq(&obj->group_data.value);
  ddsi_omg_shallow_free_StringSeq(&obj->partition.name);
}

void
ddsi_omg_shallow_copy_TopicBuiltinTopicData (
    DDS_Security_TopicBuiltinTopicData *dst,
    const char *topic_name,
    const char *type_name)
{
  memset(dst, 0, sizeof(DDS_Security_TopicBuiltinTopicData));
  dst->name = (DDS_Security_string)topic_name;
  dst->type_name = (DDS_Security_string)type_name;
}

void
ddsi_omg_shallow_free_TopicBuiltinTopicData (
    DDS_Security_TopicBuiltinTopicData *obj)
{
  DDSRT_UNUSED_ARG(obj);
}



#endif /* DDS_HAS_SECURITY */
