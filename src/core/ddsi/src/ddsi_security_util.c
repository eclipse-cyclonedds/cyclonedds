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
#include <stdarg.h>

#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"

#include "dds/ddsi/ddsi_security_util.h"

void
g_omg_shallow_copy_StringSeq(
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
g_omg_shallow_free_StringSeq(
    DDS_Security_StringSeq *obj)
{
  if (obj)
    ddsrt_free(obj->_buffer);
}

void
q_omg_copy_PropertySeq(
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
q_omg_shallow_copyin_PropertySeq(
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
q_omg_shallow_copyout_PropertySeq(
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
      dst->props[i].propagate = src->_buffer[i].propagate;
    }
  }
}

void
q_omg_shallow_free_PropertySeq(
    DDS_Security_PropertySeq *obj)
{
  assert(obj);
  ddsrt_free(obj->_buffer);
  obj->_length = 0;
  obj->_maximum = 0;
  obj->_buffer = NULL;
}

static void
q_omg_shallow_free_dds_propertyseq(
    dds_propertyseq_t *obj)
{
  ddsrt_free(obj->props);
  obj->n = 0;
  obj->props = NULL;
}

void
q_omg_shallow_copyin_BinaryPropertySeq(
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
      dst->_buffer[i].propagate      = src->props[i].propagate;
    }
  }
}

void
q_omg_shallow_copyout_BinaryPropertySeq(
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
q_omg_shallow_free_BinaryPropertySeq(
    DDS_Security_BinaryPropertySeq *obj)
{
  ddsrt_free(obj->_buffer);
  obj->_length = 0;
  obj->_maximum = 0;
  obj->_buffer = NULL;
}

static void
q_omg_shallow_free_dds_binarypropertyseq(
    dds_binarypropertyseq_t *obj)
{
  ddsrt_free(obj->props);
  obj->n = 0;
  obj->props = NULL;
}

void
q_omg_shallow_copy_PropertyQosPolicy(
    DDS_Security_PropertyQosPolicy *dst,
    const dds_property_qospolicy_t *src)
{
    assert(dst);
    assert(src);
    q_omg_shallow_copyin_PropertySeq(&(dst->value), &(src->value));
    q_omg_shallow_copyin_BinaryPropertySeq(&(dst->binary_value), &(src->binary_value));
}

void
q_omg_shallow_copy_security_qos(
    DDS_Security_Qos *dst,
    const struct dds_qos *src)
{
  assert(src);
  assert(dst);

  /* DataTags not supported yet. */
  memset(&(dst->data_tags), 0, sizeof(DDS_Security_DataTagQosPolicy));

  if (src->present & QP_PROPERTY_LIST)
    q_omg_shallow_copy_PropertyQosPolicy(&(dst->property), &(src->property));
  else
    memset(&(dst->property), 0, sizeof(DDS_Security_PropertyQosPolicy));
}

void
q_omg_shallow_free_PropertyQosPolicy(
    DDS_Security_PropertyQosPolicy *obj)
{
  q_omg_shallow_free_PropertySeq(&(obj->value));
  q_omg_shallow_free_BinaryPropertySeq(&(obj->binary_value));
}

void
q_omg_shallow_free_security_qos(
    DDS_Security_Qos *obj)
{
  q_omg_shallow_free_PropertyQosPolicy(&(obj->property));
}

void
q_omg_security_dataholder_copyin(
    nn_dataholder_t *dh,
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
q_omg_security_dataholder_copyout(
    DDS_Security_DataHolder *holder,
    const nn_dataholder_t *dh)
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
  holder->binary_properties._buffer = dh->binary_properties.n ? DDS_Security_BinaryPropertySeq_allocbuf(dh->properties.n) : NULL;
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
q_omg_shallow_copyin_DataHolder(
    DDS_Security_DataHolder *dst,
    const nn_dataholder_t *src)
{
    assert(dst);
    assert(src);
    dst->class_id = src->class_id;
    q_omg_shallow_copyin_PropertySeq(&dst->properties, &src->properties);
    q_omg_shallow_copyin_BinaryPropertySeq(&dst->binary_properties, &src->binary_properties);
}

void
q_omg_shallow_copyout_DataHolder(
    nn_dataholder_t *dst,
    const DDS_Security_DataHolder *src)
{
    assert(dst);
    assert(src);
    dst->class_id = src->class_id;
    q_omg_shallow_copyout_PropertySeq(&dst->properties, &src->properties);
    q_omg_shallow_copyout_BinaryPropertySeq(&dst->binary_properties, &src->binary_properties);
}

void
q_omg_shallow_free_DataHolder(
    DDS_Security_DataHolder *obj)
{
    q_omg_shallow_free_PropertySeq(&obj->properties);
    q_omg_shallow_free_BinaryPropertySeq(&obj->binary_properties);
}

void
q_omg_shallow_free_nn_dataholder(
    nn_dataholder_t *holder)
{
  q_omg_shallow_free_dds_propertyseq(&holder->properties);
  q_omg_shallow_free_dds_binarypropertyseq(&holder->binary_properties);
}

void
q_omg_shallow_copyin_DataHolderSeq(
    DDS_Security_DataHolderSeq *dst,
    const nn_dataholderseq_t *src)
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
      q_omg_shallow_copyin_DataHolder(&dst->_buffer[i], &src->tags[i]);
    }
  }
}

void
q_omg_shallow_free_DataHolderSeq(
    DDS_Security_DataHolderSeq *obj)
{
  unsigned i;

  for (i = 0; i  < obj->_length; i++)
  {
    q_omg_shallow_free_DataHolder(&(obj->_buffer[i]));
  }
}

void
q_omg_shallow_copy_ParticipantBuiltinTopicDataSecure(
    DDS_Security_ParticipantBuiltinTopicDataSecure *dst,
    const ddsi_guid_t *guid,
    const nn_plist_t *plist)
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
    if (plist->qos.present & QP_USER_DATA) {
        memcpy(&(dst->user_data.value), &(plist->qos.user_data.value), sizeof(DDS_Security_OctetSeq));
    }

    /* Tokens are actually DataHolders. */
    if (plist->present & PP_IDENTITY_TOKEN) {
      q_omg_shallow_copyin_DataHolder(&(dst->identity_token), &(plist->identity_token));
    }
    if (plist->present & PP_PERMISSIONS_TOKEN) {
      q_omg_shallow_copyin_DataHolder(&(dst->permissions_token), &(plist->permissions_token));
    }
    if (plist->present & PP_IDENTITY_STATUS_TOKEN) {
      q_omg_shallow_copyin_DataHolder(&(dst->identity_status_token), &(plist->identity_status_token));
    }

    if (plist->qos.present & QP_PROPERTY_LIST) {
        q_omg_shallow_copy_PropertyQosPolicy(&(dst->property), &(plist->qos.property));
    }

    if (plist->present & PP_PARTICIPANT_SECURITY_INFO) {
        dst->security_info.participant_security_attributes = plist->participant_security_info.security_attributes;
        dst->security_info.plugin_participant_security_attributes = plist->participant_security_info.plugin_security_attributes;
    }
}

void
q_omg_shallow_free_ParticipantBuiltinTopicDataSecure(
    DDS_Security_ParticipantBuiltinTopicDataSecure *obj)
{
    assert(obj);
    q_omg_shallow_free_DataHolder(&(obj->identity_token));
    q_omg_shallow_free_DataHolder(&(obj->permissions_token));
    q_omg_shallow_free_DataHolder(&(obj->identity_status_token));
    q_omg_shallow_free_PropertyQosPolicy(&(obj->property));
}
















#endif /* DDSI_INCLUDE_SECURITY */
