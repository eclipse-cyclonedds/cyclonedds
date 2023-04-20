// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include <assert.h>
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>

#include "dds/ddsrt/string.h"
#include "dds/ddsrt/misc.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/ddsrt/heap.h"

DDS_Security_BinaryProperty_t *
DDS_Security_BinaryProperty_alloc (void)
{
  DDS_Security_BinaryProperty_t *property;

  property = ddsrt_malloc(sizeof(DDS_Security_BinaryProperty_t));
  memset(property, 0, sizeof(DDS_Security_BinaryProperty_t));
  return property;
}

void
DDS_Security_BinaryProperty_deinit(
    DDS_Security_BinaryProperty_t *p)
{
    if (!p) {
        return;
    }

    ddsrt_free(p->name);
    if (p->value._length > 0) {
        memset (p->value._buffer, 0, p->value._length); /* because key material can be stored in binary property */
    }
    ddsrt_free(p->value._buffer);
}

void
DDS_Security_BinaryProperty_free(
    DDS_Security_BinaryProperty_t *p)
{
    if (p) {
        DDS_Security_BinaryProperty_deinit(p);
        ddsrt_free(p);
    }
}

void
DDS_Security_BinaryProperty_copy(
     DDS_Security_BinaryProperty_t *dst,
     const DDS_Security_BinaryProperty_t *src)
{
    dst->name = src->name ? ddsrt_strdup(src->name) : NULL;
    dst->propagate = src->propagate;
    dst->value._length = src->value._length;
    dst->value._maximum = src->value._maximum;

    if (src->value._buffer) {
        dst->value._buffer = ddsrt_malloc(src->value._length);
        memcpy(dst->value._buffer, src->value._buffer, src->value._length);
    } else {
        dst->value._buffer = NULL;
    }
}

bool
DDS_Security_BinaryProperty_equal(
     const DDS_Security_BinaryProperty_t *pa,
     const DDS_Security_BinaryProperty_t *pb)
{
    uint32_t i;

    if (pa->name && pb->name) {
        if (strcmp(pa->name, pb->name) != 0) {
            return false;
        }
    } else if (pa->name || pb->name) {
        return false;
    }

    if (pa->value._length != pb->value._length) {
        return false;
    }

    for (i = 0; i < pa->value._length; i++) {
        if (pa->value._buffer && pb->value._buffer) {
            if (memcmp(pa->value._buffer, pb->value._buffer, pa->value._length) != 0) {
                return false;
            }
        } else {
            return false;
        }
    }

    return true;
}

void
DDS_Security_BinaryProperty_set_by_value(
     DDS_Security_BinaryProperty_t *bp,
     const char *name,
     const unsigned char *data,
     uint32_t length)
{
    assert(bp);
    assert(name);
    assert(data);

    bp->name = ddsrt_strdup(name);
    bp->value._length = length;
    bp->value._maximum = length;
    bp->propagate = true;
    if (length) {
        bp->value._buffer = ddsrt_malloc(length);
        memcpy(bp->value._buffer, data, length);
    } else {
        bp->value._buffer = NULL;
    }
}

void
DDS_Security_BinaryProperty_set_by_string(
     DDS_Security_BinaryProperty_t *bp,
     const char *name,
     const char *data)
{
    uint32_t length;

    assert(bp);
    assert(name);
    assert(data);

    length = (uint32_t) strlen(data) + 1;
    DDS_Security_BinaryProperty_set_by_value(bp, name, (unsigned char *)data, length);
}

void
DDS_Security_BinaryProperty_set_by_ref(
     DDS_Security_BinaryProperty_t *bp,
     const char *name,
     unsigned char *data,
     uint32_t length)
{
    assert(bp);
    assert(name);
    assert(data);
    assert(length > 0);

    bp->name = ddsrt_strdup(name);
    bp->value._length = length;
    bp->value._maximum = length;
    bp->value._buffer = data;
    bp->propagate = true;
}

DDS_Security_BinaryPropertySeq *
DDS_Security_BinaryPropertySeq_alloc (void)
{
    DDS_Security_BinaryPropertySeq *seq;

    seq = ddsrt_malloc(sizeof(DDS_Security_BinaryPropertySeq));
    memset(seq, 0, sizeof(DDS_Security_BinaryPropertySeq));
    return seq;
}

DDS_Security_BinaryProperty_t *
DDS_Security_BinaryPropertySeq_allocbuf (
     DDS_Security_unsigned_long len)
{
    DDS_Security_BinaryProperty_t *buffer;

    buffer = ddsrt_malloc(len * sizeof(DDS_Security_BinaryProperty_t));
    memset(buffer, 0, len * sizeof(DDS_Security_BinaryProperty_t));
    return buffer;
}

void
DDS_Security_BinaryPropertySeq_deinit(
     DDS_Security_BinaryPropertySeq *seq)
{
    uint32_t i;

    if (!seq) {
        return;
    }
    for (i = 0; i < seq->_length; i++) {
        ddsrt_free(seq->_buffer[i].name);
        DDS_Security_OctetSeq_deinit(&seq->_buffer[i].value);
    }
}

void
DDS_Security_BinaryPropertySeq_free(
     DDS_Security_BinaryPropertySeq *seq)
{
    DDS_Security_BinaryPropertySeq_deinit(seq);
    ddsrt_free(seq);
}


DDS_Security_Property_t *
DDS_Security_Property_alloc (void)
{
  DDS_Security_Property_t *property;

  property = ddsrt_malloc(sizeof(DDS_Security_Property_t));
  memset(property, 0, sizeof(DDS_Security_Property_t));
  return property;
}

void
DDS_Security_Property_free(
    DDS_Security_Property_t *p)
{
    if (p) {
        DDS_Security_Property_deinit(p);
        ddsrt_free(p);
    }
}

void
DDS_Security_Property_deinit(
    DDS_Security_Property_t *p)
{
    if (!p) {
        return;
    }

    ddsrt_free(p->name);
    ddsrt_free(p->value);
}

void
DDS_Security_Property_copy(
     DDS_Security_Property_t *dst,
     const DDS_Security_Property_t *src)
{
    dst->name = src->name ? ddsrt_strdup(src->name) : NULL;
    dst->value = src->value ? ddsrt_strdup(src->value) : NULL;
    dst->propagate = src->propagate;
}

bool
DDS_Security_Property_equal(
     const DDS_Security_Property_t *pa,
     const DDS_Security_Property_t *pb)
{
    if (pa->name && pb->name) {
        if (strcmp(pa->name, pb->name) != 0) {
            return false;
        }
    } else if (pa->name || pb->name) {
        return false;
    }

    if (pa->value && pb->value) {
        if (strcmp(pa->value, pb->value) != 0) {
            return false;
        }
    } else if (pa->value || pb->value) {
        return false;
    }

    return true;
}

char *
DDS_Security_Property_get_value(
     const DDS_Security_PropertySeq *properties,
     const char *name)
{
    uint32_t i;
    char *value = NULL;

    assert(properties);
    assert(name);

    for (i = 0; !value && (i < properties->_length); i++) {
        if (properties->_buffer[i].name &&
            (strcmp(name, properties->_buffer[i].name) == 0)) {
            if (properties->_buffer[i].value) {
                value = ddsrt_strdup(properties->_buffer[i].value);
            }
        }
    }

    return value;
}

DDS_Security_PropertySeq *
DDS_Security_PropertySeq_alloc (void)
{
    DDS_Security_PropertySeq *seq;

    seq = ddsrt_malloc(sizeof(DDS_Security_PropertySeq));
    memset(seq, 0, sizeof(DDS_Security_PropertySeq));
    return seq;
}

DDS_Security_Property_t *
DDS_Security_PropertySeq_allocbuf (
     DDS_Security_unsigned_long len)
{
    DDS_Security_Property_t *buffer;

    buffer = ddsrt_malloc(len * sizeof(DDS_Security_Property_t));
    memset(buffer, 0, len * sizeof(DDS_Security_Property_t));

    return buffer;
}

void
DDS_Security_PropertySeq_freebuf(
     DDS_Security_PropertySeq *seq)
{
  uint32_t i;

  if (seq) {
    for (i = 0; i < seq->_length; i++) {
      ddsrt_free(seq->_buffer[i].name);
      ddsrt_free(seq->_buffer[i].value);
    }
    ddsrt_free(seq->_buffer);
    seq->_length = 0;
    seq->_maximum = 0;
    seq->_buffer = NULL;
  }
}

void
DDS_Security_PropertySeq_free(
     DDS_Security_PropertySeq *seq)
{
    DDS_Security_PropertySeq_deinit(seq);
    ddsrt_free(seq);
}

void
DDS_Security_PropertySeq_deinit(
    DDS_Security_PropertySeq *seq)
{
    uint32_t i;

    if (!seq) {
        return;
    }
    for (i = 0; i < seq->_length; i++) {
        ddsrt_free(seq->_buffer[i].name);
        ddsrt_free(seq->_buffer[i].value);
    }
    ddsrt_free(seq->_buffer);
}

const DDS_Security_Property_t *
DDS_Security_PropertySeq_find_property (
     const DDS_Security_PropertySeq *property_seq,
     const char *name )
{

    DDS_Security_Property_t *result = NULL;
    unsigned i, len;

    assert(property_seq);
    assert(name);

    len = (unsigned)strlen(name);
    for (i = 0; !result && (i < property_seq->_length); i++) {
        if (property_seq->_buffer[i].name &&
            (strncmp(name, property_seq->_buffer[i].name, len+ 1) == 0)) {
            result = &property_seq->_buffer[i];
        }
    }

    return result;
}

DDS_Security_DataHolder *
DDS_Security_DataHolder_alloc(void)
{
    DDS_Security_DataHolder *holder;
    holder = ddsrt_malloc(sizeof(*holder));
    memset(holder, 0, sizeof(*holder));
    return holder;
}

void
DDS_Security_DataHolder_free(
    DDS_Security_DataHolder *holder)
{
    if (!holder) {
        return;
    }
    DDS_Security_DataHolder_deinit(holder);
    ddsrt_free(holder);
}

void
DDS_Security_DataHolder_deinit(
    DDS_Security_DataHolder *holder)
{
    uint32_t i;

    if (!holder) {
        return;
    }

    ddsrt_free(holder->class_id);

    for (i = 0; i < holder->properties._length; i++) {
        DDS_Security_Property_deinit(&holder->properties._buffer[i]);
    }
    ddsrt_free(holder->properties._buffer);

    for (i = 0; i < holder->binary_properties._length; i++) {
        DDS_Security_BinaryProperty_deinit(&holder->binary_properties._buffer[i]);
    }
    ddsrt_free(holder->binary_properties._buffer);

    memset(holder, 0, sizeof(*holder));
}

void
DDS_Security_DataHolder_copy(
     DDS_Security_DataHolder *dst,
     const DDS_Security_DataHolder *src)
{
    uint32_t i;

    assert(dst);
    assert(src);

    if (src->class_id) {
        dst->class_id = ddsrt_strdup(src->class_id);
    } else {
        dst->class_id = NULL;
    }

    dst->properties = src->properties;
    if (src->properties._buffer) {
        dst->properties._buffer = DDS_Security_PropertySeq_allocbuf(src->properties._length);
        for (i = 0; i < src->properties._length; i++) {
            DDS_Security_Property_copy(&dst->properties._buffer[i], &src->properties._buffer[i]);
        }
    }

    dst->binary_properties = src->binary_properties;
    if (src->binary_properties._buffer) {
        dst->binary_properties._buffer = DDS_Security_BinaryPropertySeq_allocbuf(src->binary_properties._length);
        for (i = 0; i < src->binary_properties._length; i++) {
            DDS_Security_BinaryProperty_copy(&dst->binary_properties._buffer[i], &src->binary_properties._buffer[i]);
        }
    }
}

bool
DDS_Security_DataHolder_equal(
     const DDS_Security_DataHolder *psa,
     const DDS_Security_DataHolder *psb)
{
    uint32_t i;

    if (psa->class_id && psb->class_id) {
        if (strcmp(psa->class_id, psb->class_id) != 0) {
            return false;
        }
    } else if (psa->class_id || psb->class_id) {
        return false;
    }

    for (i = 0; i < psa->properties._length; i++) {
        if (!DDS_Security_Property_equal(&psa->properties._buffer[i], &psb->properties._buffer[i])) {
            return false;
        }
    }

    for (i = 0; i < psa->binary_properties._length; i++) {
        if (!DDS_Security_BinaryProperty_equal(&psa->binary_properties._buffer[i], &psb->binary_properties._buffer[i])) {
            return false;
        }
    }

    return true;
}


const DDS_Security_Property_t *
DDS_Security_DataHolder_find_property(
     const DDS_Security_DataHolder *holder,
     const char *name)
{

    assert(holder);
    assert(name);

    return DDS_Security_PropertySeq_find_property ( &(holder->properties), name );
}

const DDS_Security_BinaryProperty_t *
DDS_Security_DataHolder_find_binary_property(
     const DDS_Security_DataHolder *holder,
     const char *name)
{
    DDS_Security_BinaryProperty_t *result = NULL;
    unsigned i, len;

    assert(holder);
    assert(name);

    len = (unsigned)strlen(name);

    for (i = 0; !result && (i < holder->binary_properties._length); i++) {
        if (holder->binary_properties._buffer[i].name &&
           (strncmp(name, holder->binary_properties._buffer[i].name, len+1) == 0)) {
            result = &holder->binary_properties._buffer[i];
        }
    }

    return result;
}

DDS_Security_DataHolderSeq *
DDS_Security_DataHolderSeq_alloc (void)
{
    DDS_Security_DataHolderSeq *holder;

    holder = ddsrt_malloc(sizeof(DDS_Security_DataHolderSeq));
    memset(holder, 0, sizeof(DDS_Security_DataHolderSeq));
    return holder;
}

DDS_Security_DataHolder *
DDS_Security_DataHolderSeq_allocbuf (
     DDS_Security_unsigned_long len)
{
    DDS_Security_DataHolder *buffer;

    buffer = ddsrt_malloc(len * sizeof(DDS_Security_DataHolder));
    memset(buffer, 0, len * sizeof(DDS_Security_DataHolder));
    return buffer;
}

void
DDS_Security_DataHolderSeq_freebuf(
     DDS_Security_DataHolderSeq *seq)
{
    uint32_t i;

    if (seq) {
        for (i = 0; i < seq->_length; i++) {
            DDS_Security_DataHolder_deinit(&seq->_buffer[i]);
        }
        ddsrt_free(seq->_buffer);
        seq->_buffer = NULL;
        seq->_length = 0;
        seq->_maximum = 0;
    }
}

void
DDS_Security_DataHolderSeq_free(
     DDS_Security_DataHolderSeq *seq)
{
    if (seq) {
        DDS_Security_DataHolderSeq_freebuf(seq);
        ddsrt_free(seq);
    }
}

void
DDS_Security_DataHolderSeq_deinit(
     DDS_Security_DataHolderSeq *seq)
{
    if (seq) {
        DDS_Security_DataHolderSeq_freebuf(seq);
    }
}

void
DDS_Security_DataHolderSeq_copy(
     DDS_Security_DataHolderSeq *dst,
     const DDS_Security_DataHolderSeq *src)
{
    uint32_t i;

    assert(dst);
    assert(src);

    *dst = *src;

    if (src->_length) {
        dst->_buffer = DDS_Security_DataHolderSeq_allocbuf(src->_length);
    }

    for (i = 0; i < src->_length; i++) {
        DDS_Security_DataHolder_copy(&dst->_buffer[i], &src->_buffer[i]);
    }
}

DDS_Security_ParticipantBuiltinTopicData *
DDS_Security_ParticipantBuiltinTopicData_alloc(void)
{
    DDS_Security_ParticipantBuiltinTopicData *result;

    result = ddsrt_malloc(sizeof(*result));
    memset(result, 0, sizeof(*result));

    return result;
}

void
DDS_Security_ParticipantBuiltinTopicData_free(
     DDS_Security_ParticipantBuiltinTopicData *data)
{
    DDS_Security_ParticipantBuiltinTopicData_deinit(data);
    ddsrt_free(data);
}

void
DDS_Security_ParticipantBuiltinTopicData_deinit(
     DDS_Security_ParticipantBuiltinTopicData *data)
{
    if (!data) {
        return;
    }
    DDS_Security_DataHolder_deinit(&data->identity_token);
    DDS_Security_DataHolder_deinit(&data->permissions_token);
    DDS_Security_PropertyQosPolicy_deinit(&data->property);
    DDS_Security_OctetSeq_deinit(&data->user_data.value);
}

DDS_Security_OctetSeq *
DDS_Security_OctetSeq_alloc (void)
{
  return (DDS_Security_OctetSeq *)ddsrt_malloc(sizeof(DDS_Security_OctetSeq ));
}

DDS_Security_octet *
DDS_Security_OctetSeq_allocbuf (
     DDS_Security_unsigned_long len)
{
  return (DDS_Security_octet*)ddsrt_malloc(sizeof(DDS_Security_octet)*len);
}

void
DDS_Security_OctetSeq_freebuf(
     DDS_Security_OctetSeq *seq)
{
    if (!seq) {
        return;
    }
    ddsrt_free(seq->_buffer);
    seq->_buffer = NULL;
    seq->_length = 0;
    seq->_maximum = 0;
}

void
DDS_Security_OctetSeq_free(
     DDS_Security_OctetSeq *seq)
{
    DDS_Security_OctetSeq_deinit(seq);
    ddsrt_free(seq);
}

void
DDS_Security_OctetSeq_deinit(
     DDS_Security_OctetSeq *seq)
{
    DDS_Security_OctetSeq_freebuf(seq);
}


void
DDS_Security_OctetSeq_copy(
     DDS_Security_OctetSeq *dst,
     const DDS_Security_OctetSeq *src)
{
    if (dst->_length > 0) {
        DDS_Security_OctetSeq_deinit(dst);
    }
    dst->_length = src->_length;
    dst->_maximum = src->_maximum;

    if (src->_length) {
        dst->_buffer = ddsrt_malloc(src->_length);
        memcpy(dst->_buffer, src->_buffer, src->_length);
    } else {
        dst->_buffer = NULL;
    }
}

DDS_Security_HandleSeq *
DDS_Security_HandleSeq_alloc(void)
{
    DDS_Security_HandleSeq *seq;

    seq = ddsrt_malloc(sizeof(*seq));
    seq->_buffer = NULL;
    seq->_length = 0;
    seq->_maximum = 0;

    return seq;
}

DDS_Security_long_long *
DDS_Security_HandleSeq_allocbuf(
     DDS_Security_unsigned_long length)
{
    DDS_Security_long_long *buffer;
    buffer = ddsrt_malloc(length * sizeof(DDS_Security_long_long));
    memset(buffer, 0, length * sizeof(DDS_Security_long_long));
    return buffer;
}

void
DDS_Security_HandleSeq_freebuf(
     DDS_Security_HandleSeq *seq)
{
    if (!seq) {
        return;
    }
    ddsrt_free(seq->_buffer);
    seq->_maximum = 0;
    seq->_length = 0;
}

void
DDS_Security_HandleSeq_free(
     DDS_Security_HandleSeq *seq)
{
    if (!seq) {
        return;
    }
    DDS_Security_HandleSeq_freebuf(seq);
    ddsrt_free(seq);
}

void
DDS_Security_HandleSeq_deinit(
     DDS_Security_HandleSeq *seq)
{
    if (!seq) {
        return;
    }
    DDS_Security_HandleSeq_freebuf(seq);
}

void DDS_Security_Exception_vset (DDS_Security_SecurityException *ex, const char *context, int code, int minor_code, const char *fmt, va_list args1)
{
  int32_t ret;
  size_t len;
  char buf[1] = { '\0' };
  char *str = NULL;
  va_list args2;

  assert(context);
  assert(fmt);
  assert(ex);
  DDSRT_UNUSED_ARG( context );

  va_copy(args2, args1);

  if ((ret = vsnprintf(buf, sizeof(buf), fmt, args1)) >= 0) {
    len = (size_t)ret; /* +1 for null byte */
    if ((str = ddsrt_malloc(len + 1)) == NULL) {
      assert(false);
    } else if ((ret = vsnprintf(str, len + 1, fmt, args2)) >= 0) {
      assert((size_t) ret == len);
    } else {
      ddsrt_free(str);
      str = NULL;
    }
  }

  va_end(args2);

  ex->message = str;
  ex->code = code;
  ex->minor_code = minor_code;
}

void DDS_Security_Exception_set (DDS_Security_SecurityException *ex, const char *context, int code, int minor_code, const char *fmt, ...)
{
  va_list args1;
  assert(context);
  assert(fmt);
  assert(ex);
  va_start(args1, fmt);
  DDS_Security_Exception_vset (ex, context, code, minor_code, fmt, args1);
  va_end(args1);
}

void
DDS_Security_Exception_reset(
    DDS_Security_SecurityException *ex)
{
    if (ex) {
        if (ex->message) {
            ddsrt_free(ex->message);
        }
        DDS_Security_Exception_clean(ex);
    }
}

void
DDS_Security_Exception_clean(
     DDS_Security_SecurityException *ex)
{
    if (ex) {
      ex->code = 0;
      ex->minor_code = 0;
      ex->message = NULL;
    }
}

void
DDS_Security_PropertyQosPolicy_deinit(
     DDS_Security_PropertyQosPolicy *policy)
{
    if (!policy) {
        return;
    }
    DDS_Security_PropertySeq_deinit(&policy->value);
    DDS_Security_BinaryPropertySeq_deinit(&policy->binary_value);
}

void
DDS_Security_PropertyQosPolicy_free(
     DDS_Security_PropertyQosPolicy *policy)
{
    DDS_Security_PropertyQosPolicy_deinit(policy);
    ddsrt_free(policy);
}


void
DDS_Security_set_token_nil(
     DDS_Security_DataHolder *token)
{
    DDS_Security_DataHolder_deinit(token);
    memset(token, 0, sizeof(*token));
    token->class_id = ddsrt_strdup("");
}

void
DDS_Security_KeyMaterial_AES_GCM_GMAC_deinit(
     DDS_Security_KeyMaterial_AES_GCM_GMAC *key_material)
{
    if (key_material) {
        if (key_material->master_receiver_specific_key._buffer != NULL) {
            memset (key_material->master_receiver_specific_key._buffer, 0, key_material->master_receiver_specific_key._length);
            ddsrt_free(key_material->master_receiver_specific_key._buffer);
        }
        if( key_material->master_salt._buffer != NULL){
            memset (key_material->master_salt._buffer, 0, key_material->master_salt._length);
            ddsrt_free(key_material->master_salt._buffer);
        }
        if( key_material->master_sender_key._buffer != NULL){
            memset (key_material->master_sender_key._buffer, 0, key_material->master_sender_key._length);
            ddsrt_free(key_material->master_sender_key._buffer);
        }
    }
}

static uint32_t DDS_Security_getKeySize (const DDS_Security_PropertySeq *properties)
{
    const DDS_Security_Property_t *key_size_property;
    if (properties != NULL)
    {
        key_size_property = DDS_Security_PropertySeq_find_property (properties, DDS_SEC_PROP_CRYPTO_KEYSIZE);
        if (key_size_property != NULL && !strcmp(key_size_property->value, "128"))
            return 128;
    }
    return 256;
}

DDS_Security_CryptoTransformKind_Enum
DDS_Security_basicprotectionkind2transformationkind(
     const DDS_Security_PropertySeq *properties,
     DDS_Security_BasicProtectionKind protection)
{
    uint32_t keysize = DDS_Security_getKeySize (properties);
    switch (protection) {
        case DDS_SECURITY_BASICPROTECTION_KIND_NONE:
            return CRYPTO_TRANSFORMATION_KIND_NONE;
        case DDS_SECURITY_BASICPROTECTION_KIND_SIGN:
            return (keysize == 128) ? CRYPTO_TRANSFORMATION_KIND_AES128_GMAC : CRYPTO_TRANSFORMATION_KIND_AES256_GMAC;
        case DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT:
            return (keysize == 128) ? CRYPTO_TRANSFORMATION_KIND_AES128_GCM : CRYPTO_TRANSFORMATION_KIND_AES256_GCM;
        default:
            return CRYPTO_TRANSFORMATION_KIND_INVALID;
    }
}

DDS_Security_CryptoTransformKind_Enum
DDS_Security_protectionkind2transformationkind(
     const DDS_Security_PropertySeq *properties,
     DDS_Security_ProtectionKind protection)
{
    uint32_t keysize = DDS_Security_getKeySize (properties);
    switch (protection) {
        case DDS_SECURITY_PROTECTION_KIND_NONE:
            return CRYPTO_TRANSFORMATION_KIND_NONE;
        case DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION:
        case DDS_SECURITY_PROTECTION_KIND_SIGN:
            return (keysize == 128) ? CRYPTO_TRANSFORMATION_KIND_AES128_GMAC : CRYPTO_TRANSFORMATION_KIND_AES256_GMAC;
        case DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION:
        case DDS_SECURITY_PROTECTION_KIND_ENCRYPT:
            return (keysize == 128) ? CRYPTO_TRANSFORMATION_KIND_AES128_GCM : CRYPTO_TRANSFORMATION_KIND_AES256_GCM;
        default:
            return CRYPTO_TRANSFORMATION_KIND_INVALID;
    }
}

#ifndef NDEBUG
void
print_binary_debug(
     char* name,
     unsigned char *value,
     uint32_t size)
{
    uint32_t i;
    printf("%s: ",name );
    for( i=0; i<  size; i++)
    {
        printf("%x",value[i]);
    }
    printf("\n");
}

void
print_binary_properties_debug(
     const DDS_Security_DataHolder *token)
{
    uint32_t i;
    for (i = 0; i < token->binary_properties._length ; i++) {
        print_binary_debug( token->binary_properties._buffer[i].name, token->binary_properties._buffer[i].value._buffer, token->binary_properties._buffer[i].value._length);
    }

}
#endif


DDS_Security_config_item_prefix_t
DDS_Security_get_conf_item_type(
     const char *str,
     char **data)
{
    DDS_Security_config_item_prefix_t kind = DDS_SECURITY_CONFIG_ITEM_PREFIX_UNKNOWN;
    const char *CONFIG_FILE_PREFIX   = "file:";
    const char *CONFIG_DATA_PREFIX   = "data:,";
    const char *CONFIG_PKCS11_PREFIX = "pkcs11:";
    size_t CONFIG_FILE_PREFIX_LEN   = strlen(CONFIG_FILE_PREFIX);
    size_t CONFIG_DATA_PREFIX_LEN   = strlen(CONFIG_DATA_PREFIX);
    size_t CONFIG_PKCS11_PREFIX_LEN = strlen(CONFIG_PKCS11_PREFIX);

    assert(str);
    assert(data);

    for (; *str == ' ' || *str == '\t'; str++)
      /* ignore leading whitespace */;

    if (strncmp(str, CONFIG_FILE_PREFIX, CONFIG_FILE_PREFIX_LEN) == 0) {
        const char *DOUBLE_SLASH = "//";
        size_t DOUBLE_SLASH_LEN = 2;
        if (strncmp(&(str[CONFIG_FILE_PREFIX_LEN]), DOUBLE_SLASH, DOUBLE_SLASH_LEN) == 0) {
            *data = ddsrt_strdup(&(str[CONFIG_FILE_PREFIX_LEN + DOUBLE_SLASH_LEN]));
        } else {
            *data = ddsrt_strdup(&(str[CONFIG_FILE_PREFIX_LEN]));
        }
        kind = DDS_SECURITY_CONFIG_ITEM_PREFIX_FILE;
    } else if (strncmp(str, CONFIG_DATA_PREFIX, CONFIG_DATA_PREFIX_LEN) == 0) {
        kind = DDS_SECURITY_CONFIG_ITEM_PREFIX_DATA;
        *data = ddsrt_strdup(&(str[CONFIG_DATA_PREFIX_LEN]));
    } else if (strncmp(str, CONFIG_PKCS11_PREFIX, CONFIG_PKCS11_PREFIX_LEN) == 0) {
        kind = DDS_SECURITY_CONFIG_ITEM_PREFIX_PKCS11;
        *data = ddsrt_strdup(&(str[CONFIG_PKCS11_PREFIX_LEN]));
    }

    return kind;
}

/* The result of os_fileNormalize should be freed with os_free */
char *
DDS_Security_normalize_file(
    const char *filepath)
{
    char *norm;
    const char *fpPtr;
    char *normPtr;
#if _WIN32
  #define __FILESEPCHAR '\\'
#else
  #define __FILESEPCHAR '/'
#endif
    norm = NULL;
    if ((filepath != NULL) && (*filepath != '\0')) {
        norm = ddsrt_malloc(strlen(filepath) + 1);
        /* replace any / or \ by OS_FILESEPCHAR */
        fpPtr = (char *) filepath;
        normPtr = norm;
        while (*fpPtr != '\0') {
            *normPtr = *fpPtr;
            if ((*fpPtr == '/') || (*fpPtr == '\\')) {
                *normPtr = __FILESEPCHAR;
                normPtr++;
            } else {
                if (*fpPtr != '\"') {
                    normPtr++;
                }
            }
            fpPtr++;
        }
        *normPtr = '\0';
    }
#undef __FILESEPCHAR
    return norm;
}

/**
 * Parses an XML date string and returns this as a dds_time_t value. As leap seconds are not permitted
 * in the XML date format (as stated in the XML Schema specification), this parser function does not
 * accept leap seconds in its input string. This complies with the dds_time_t representation on posix,
 * which is a unix timestamp (that also ignores leap seconds).
 *
 * As a dds_time_t is expressed as nanoseconds, the fractional seconds part of the input string will
 * be rounded in case the fractional part has more than 9 digits.
 */
dds_time_t
DDS_Security_parse_xml_date(
    char *buf)
{
  int32_t year = -1;
  int32_t month = -1;
  int32_t day = -1;
  int32_t hour = -1;
  int32_t minute = -1;
  int32_t second = -1;
  int32_t hour_offset = -1;
  int32_t minute_offset = -1;

  int64_t frac_ns = 0;

  size_t cnt = 0;
  size_t cnt_frac_sec = 0;

  assert(buf != NULL);

  /* Make an integrity check of the string before the conversion*/
  while (buf[cnt] != '\0')
  {
    if (cnt == 4 || cnt == 7)
    {
      if (buf[cnt] != '-')
        return DDS_TIME_INVALID;
    }
    else if (cnt == 10)
    {
      if (buf[cnt] != 'T')
        return DDS_TIME_INVALID;
    }
    else if (cnt == 13 || cnt == 16)
    {
      if (buf[cnt] != ':')
        return DDS_TIME_INVALID;
    }
    else if (cnt == 19)
    {
      if (buf[cnt] != 'Z' && buf[cnt] != '+' && buf[cnt] != '-' && buf[cnt] != '.')
        return DDS_TIME_INVALID;

      /* If a dot is found then a variable number of fractional seconds is present.
               A second integrity loop to account for the variability is used */
      if (buf[cnt] == '.' && !cnt_frac_sec)
      {
        cnt_frac_sec = 1;
        while (buf[cnt + 1] != '\0' && buf[cnt + 1] >= '0' && buf[cnt + 1] <= '9')
        {
          cnt_frac_sec++;
          cnt++;
        }
      }
    }
    else if (cnt == 19 + cnt_frac_sec)
    {
      if (buf[cnt] != 'Z' && buf[cnt] != '+' && buf[cnt] != '-')
        return DDS_TIME_INVALID;
    }
    else if (cnt == 22 + cnt_frac_sec)
    {
      if (buf[cnt] != ':')
        return DDS_TIME_INVALID;
    }
    else
    {
      if (buf[cnt] < '0' || buf[cnt] > '9')
        return DDS_TIME_INVALID;
    }
    cnt++;
  }

  /* Do not allow more than 12 (13 including the dot) and less than 1 fractional second digits if they are used */
  if (cnt_frac_sec && (cnt_frac_sec < 2 || cnt_frac_sec > 13))
    return DDS_TIME_INVALID;

  /* Valid string length value at this stage are 19, 20 and 25 plus the fractional seconds part */
  if (cnt != 19 + cnt_frac_sec && cnt != 20 + cnt_frac_sec && cnt != 25 + cnt_frac_sec)
    return DDS_TIME_INVALID;

  year = ddsrt_todigit(buf[0]) * 1000 + ddsrt_todigit(buf[1]) * 100 + ddsrt_todigit(buf[2]) * 10 + ddsrt_todigit(buf[3]);
  month = ddsrt_todigit(buf[5]) * 10 + ddsrt_todigit(buf[6]);
  day = ddsrt_todigit(buf[8]) * 10 + ddsrt_todigit(buf[9]);

  hour = ddsrt_todigit(buf[11]) * 10 + ddsrt_todigit(buf[12]);
  minute = ddsrt_todigit(buf[14]) * 10 + ddsrt_todigit(buf[15]);
  second = ddsrt_todigit(buf[17]) * 10 + ddsrt_todigit(buf[18]);

  {
    int64_t frac_ns_pow = DDS_NSECS_IN_SEC / 10;
    size_t n = 0;
    for (n = 0; cnt_frac_sec && n < cnt_frac_sec - 1; n++)
    {
      /* Maximum granularity is nanosecond so round to maximum 9 digits */
      if (n == 9)
      {
        if (ddsrt_todigit(buf[20 + n]) >= 5)
          frac_ns++;
        break;
      }
      frac_ns += ddsrt_todigit(buf[20 + n]) * frac_ns_pow;
      frac_ns_pow = frac_ns_pow / 10;
    }
  }

  /* If the length is 20 the last character must be a Z representing UTC time zone */
  if (cnt == 19 + cnt_frac_sec || (cnt == 20 + cnt_frac_sec && buf[19 + cnt_frac_sec] == 'Z'))
  {
    hour_offset = 0;
    minute_offset = 0;
  }
  else if (cnt == 25 + cnt_frac_sec)
  {
    hour_offset = ddsrt_todigit(buf[20 + cnt_frac_sec]) * 10 + ddsrt_todigit(buf[21 + cnt_frac_sec]);
    minute_offset = ddsrt_todigit(buf[23 + cnt_frac_sec]) * 10 + ddsrt_todigit(buf[24 + cnt_frac_sec]);
  }
  else
    return DDS_TIME_INVALID;

  /* Make a limit check to make sure that all the numbers are within absolute boundaries.
     Note that leap seconds are not allowed in XML dates and therefore not supported. */
  if (year < 1970 || year > 2262 || month < 1 || month > 12 || day < 1 || day > 31 ||
      hour < 0 || hour > 23 || minute < 0 || minute > 59 || second < 0 || second > 59 ||
      ((hour_offset < 0 || hour_offset > 11 || minute_offset < 0 || minute_offset > 59) && (hour_offset != 12 || minute_offset != 0)))
  {
    return DDS_TIME_INVALID;
  }

  /*  Boundary check including consideration for month and leap years */
  if (!(((month == 4 || month == 6 || month == 9 || month == 11) && (day >= 1 && day <= 30)) ||
      ((month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12) && (day >= 1 && day <= 31)) ||
      (month == 2 && ((year % 100 != 0 && year % 4 == 0) || (year % 400 == 0)) && (day >= 1 && day <= 29)) ||
      (month == 2 && (day >= 1 && day <= 28))))
  {
    return DDS_TIME_INVALID;
  }

  /* Convert the year-month-day to total number of days */
  int32_t total_leap_years = (year - 1970 + 1) / 4;
  /* Leap year count decreased by the number of xx00 years before current year because these are not leap years,
     except for 2000. The year 2400 is not in the valid year range so we don't take that into account. */
  if (year > 2100)
    total_leap_years -= year / 100 - 20;
  if (year == 2200)
    total_leap_years++;

  int32_t total_reg_years = year - 1970 - total_leap_years;
  int32_t total_num_days = total_leap_years * 366 + total_reg_years * 365;
  int32_t month_cnt;

  for (month_cnt = 1; month_cnt < month; month_cnt++)
  {
    if (month_cnt == 4 || month_cnt == 6 || month_cnt == 9 || month_cnt == 11)
      total_num_days += 30;
    else if (month_cnt == 2)
    {
      if (year % 400 == 0 || (year % 100 != 0 && year % 4 == 0))
        total_num_days += 29;
      else
        total_num_days += 28;
    }
    else
      total_num_days += 31;
  }
  total_num_days += day - 1;

  /* Correct the offset sign if negative */
  if (buf[19 + cnt_frac_sec] == '-')
  {
    hour_offset = -hour_offset;
    minute_offset = -minute_offset;
  }
  /* Convert the total number of days to seconds */
  int64_t ts_days = (int64_t)total_num_days * 24 * 60 * 60;
  int64_t ts_hms = hour * 60 * 60 + minute * 60 + second;
  if (ts_days + ts_hms > INT64_MAX / DDS_NSECS_IN_SEC)
    return DDS_TIME_INVALID;
  int64_t ts = DDS_SECS(ts_days + ts_hms);

  /* Apply the hour and minute offset */
  int64_t ts_offset = DDS_SECS((int64_t)hour_offset * 60 * 60 + minute_offset * 60);

  /* Prevent the offset from making the timestamp negative or overflow it */
  if ((ts_offset <= 0 || (ts_offset > 0 && ts_offset < ts)) && INT64_MAX - ts - frac_ns >= -ts_offset)
    return ts - ts_offset + frac_ns;

  return DDS_TIME_INVALID;
}

