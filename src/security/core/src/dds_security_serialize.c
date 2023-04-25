// Copyright(c) 2006 to 2020 ZettaScale Technology and others
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
#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/atomics.h"
#include "dds/ddsrt/string.h"

#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/bswap.h"


#define BYTE_ORDER_BIG_ENDIAN                   0x02
#define BYTE_ORDER_LITTLE_ENDIAN                0x03

#define PID_PAD                                 0x0u
#define PID_SENTINEL                            0x1u
#define PID_USER_DATA                           0x2cu
#define PID_TOPIC_NAME                          0x5u
#define PID_TYPE_NAME                           0x7u
#define PID_GROUP_DATA                          0x2du
#define PID_TOPIC_DATA                          0x2eu
#define PID_DURABILITY                          0x1du
#define PID_DURABILITY_SERVICE                  0x1eu
#define PID_DEADLINE                            0x23u
#define PID_LATENCY_BUDGET                      0x27u
#define PID_LIVELINESS                          0x1bu
#define PID_RELIABILITY                         0x1au
#define PID_LIFESPAN                            0x2bu
#define PID_DESTINATION_ORDER                   0x25u
#define PID_HISTORY                             0x40u
#define PID_RESOURCE_LIMITS                     0x41u
#define PID_OWNERSHIP                           0x1fu
#define PID_OWNERSHIP_STRENGTH                  0x6u
#define PID_PRESENTATION                        0x21u
#define PID_PARTITION                           0x29u
#define PID_TIME_BASED_FILTER                   0x4u
#define PID_TRANSPORT_PRIORITY                  0x49u
#define PID_PROTOCOL_VERSION                    0x15u
#define PID_VENDORID                            0x16u
#define PID_UNICAST_LOCATOR                     0x2fu
#define PID_MULTICAST_LOCATOR                   0x30u
#define PID_MULTICAST_IPADDRESS                 0x11u
#define PID_DEFAULT_UNICAST_LOCATOR             0x31u
#define PID_DEFAULT_MULTICAST_LOCATOR           0x48u
#define PID_METATRAFFIC_UNICAST_LOCATOR         0x32u
#define PID_METATRAFFIC_MULTICAST_LOCATOR       0x33u
#define PID_DEFAULT_UNICAST_IPADDRESS           0xcu
#define PID_DEFAULT_UNICAST_PORT                0xeu
#define PID_METATRAFFIC_UNICAST_IPADDRESS       0x45u
#define PID_METATRAFFIC_UNICAST_PORT            0xdu
#define PID_METATRAFFIC_MULTICAST_IPADDRESS     0xbu
#define PID_METATRAFFIC_MULTICAST_PORT          0x46u
#define PID_EXPECTS_INLINE_QOS                  0x43u
#define PID_PARTICIPANT_MANUAL_LIVELINESS_COUNT 0x34u
#define PID_PARTICIPANT_BUILTIN_ENDPOINTS       0x44u
#define PID_PARTICIPANT_LEASE_DURATION          0x2u
#define PID_CONTENT_FILTER_PROPERTY             0x35u
#define PID_PARTICIPANT_GUID                    0x50u
#define PID_PARTICIPANT_ENTITYID                0x51u
#define PID_GROUP_GUID                          0x52u
#define PID_GROUP_ENTITYID                      0x53u
#define PID_BUILTIN_ENDPOINT_SET                0x58u
#define PID_PROPERTY_LIST                       0x59u
#define PID_TYPE_MAX_SIZE_SERIALIZED            0x60u
#define PID_ENTITY_NAME                         0x62u
#define PID_KEYHASH                             0x70u
#define PID_STATUSINFO                          0x71u
#define PID_CONTENT_FILTER_INFO                 0x55u
#define PID_COHERENT_SET                        0x56u
#define PID_DIRECTED_WRITE                      0x57u
#define PID_ORIGINAL_WRITER_INFO                0x61u
#define PID_ENDPOINT_GUID                       0x5au
#define PID_TYPE_CONSISTENCY_ENFORCEMENT        0x74u
#define PID_TYPE_INFORMATION                    0x75u

/* Security related PID values. */
#define PID_IDENTITY_TOKEN                      0x1001u
#define PID_PERMISSIONS_TOKEN                   0x1002u
#define PID_ENDPOINT_SECURITY_INFO              0x1004u
#define PID_PARTICIPANT_SECURITY_INFO           0x1005u
#define PID_IDENTITY_STATUS_TOKEN               0x1006u

struct DDS_Security_Serializer {
    unsigned char *buffer;
    size_t size;
    size_t offset;
    size_t increment;
    size_t marker;
};

struct DDS_Security_Deserializer {
    const unsigned char *buffer;
    const unsigned char *cursor;
    size_t size;
    size_t remain;
};


static size_t
alignup_size (
     size_t x,
     size_t a)
{
    size_t m = a-1;
    return (x+m) & ~m;
}

static size_t
alignup_ptr(
     const unsigned char *ptr,
     size_t a)
{
    size_t m = (a - 1);
    size_t x = (size_t) ptr;
    return ((x+m) & ~m) - x;
}

DDS_Security_Serializer
DDS_Security_Serializer_new(
     size_t size,
     size_t increment)
{
    DDS_Security_Serializer serializer;

    serializer = ddsrt_malloc(sizeof(*serializer));
    serializer->buffer = ddsrt_malloc(size);
    serializer->size = size;
    serializer->increment = increment;
    serializer->offset = 0;

    return serializer;
}

void
DDS_Security_Serializer_free(
     DDS_Security_Serializer ser)
{
    if (ser) {
        ddsrt_free(ser->buffer);
        ddsrt_free(ser);
    }
}

void
DDS_Security_Serializer_buffer(
     DDS_Security_Serializer ser,
     unsigned char **buffer,
     size_t *size)
{
    assert(ser);
    assert(buffer);
    assert(size);

    *buffer = ser->buffer;
    *size = ser->offset;
    ser->buffer = NULL;
}

static void
serbuffer_adjust_size(
    DDS_Security_Serializer ser,
    size_t needed)
{
    if (ser->size - ser->offset < needed) {
        ser->buffer = ddsrt_realloc(ser->buffer, ser->size + needed + ser->increment);
        ser->size += needed + ser->increment;
    }
}

static void
serbuffer_align(
     DDS_Security_Serializer ser,
     size_t alignment)
{
    size_t offset, i;

    offset = alignup_size(ser->offset, alignment);
    serbuffer_adjust_size(ser, offset-ser->offset);
    for (i = 0; i < offset - ser->offset; i++) {
        ser->buffer[ser->offset+i] = 0;
    }
    ser->offset = offset;
}

static void
DDS_Security_Serialize_mark_len(
     DDS_Security_Serializer ser)
{
    serbuffer_align(ser, 2);
    serbuffer_adjust_size(ser, 2);
    ser->marker = ser->offset;
    ser->offset += 2;
}

static void
DDS_Security_Serialize_update_len(
     DDS_Security_Serializer ser)
{
    unsigned short len;

    len = (unsigned short)(ser->offset - ser->marker - sizeof(len));
    *(unsigned short *)&(ser->buffer[ser->marker]) = ddsrt_toBE2u(len);
}

static void
DDS_Security_Serialize_uint16(
     DDS_Security_Serializer ser,
     unsigned short value)
{
    serbuffer_align(ser, sizeof(value));
    serbuffer_adjust_size(ser, sizeof(value));

    *(unsigned short *)&(ser->buffer[ser->offset]) = ddsrt_toBE2u(value);
    ser->offset += sizeof(value);
}

static void
DDS_Security_Serialize_uint32_t(
     DDS_Security_Serializer ser,
     uint32_t value)
{
    serbuffer_align(ser, sizeof(value));
    serbuffer_adjust_size(ser, sizeof(value));

    *(uint32_t *)&(ser->buffer[ser->offset]) = ddsrt_toBE4u(value);
    ser->offset += sizeof(value);
}

static void
DDS_Security_Serialize_string(
     DDS_Security_Serializer ser,
     const char *str)
{
    size_t len = strlen(str) + 1;

    DDS_Security_Serialize_uint32_t(ser, (uint32_t)len);
    serbuffer_adjust_size(ser, len);

    memcpy(&(ser->buffer[ser->offset]), str, len);
    ser->offset += len;
    serbuffer_align(ser, sizeof(uint32_t));
}

static void
DDS_Security_Serialize_Property(
     DDS_Security_Serializer ser,
     const DDS_Security_Property_t *property)
{
    DDS_Security_Serialize_string(ser, property->name);
    DDS_Security_Serialize_string(ser, property->value);
}

static void
DDS_Security_Serialize_OctetSeq(
     DDS_Security_Serializer ser,
     const DDS_Security_OctetSeq *seq)
{
    DDS_Security_Serialize_uint32_t(ser, seq->_length);
    if (seq->_length) {
        serbuffer_adjust_size(ser, seq->_length);
        memcpy(&(ser->buffer[ser->offset]), seq->_buffer, seq->_length);
        ser->offset += seq->_length;
    }
}

static void
DDS_Security_Serialize_BinaryProperty(
     DDS_Security_Serializer ser,
     const DDS_Security_BinaryProperty_t *property)
{
    DDS_Security_Serialize_string(ser, property->name);
    DDS_Security_Serialize_OctetSeq(ser, &property->value);
}

void
DDS_Security_Serialize_PropertySeq(
     DDS_Security_Serializer ser,
     const DDS_Security_PropertySeq *seq)
{
    uint32_t i;

    DDS_Security_Serialize_uint32_t(ser, seq->_length);
    for (i = 0; i < seq->_length; i++) {
         DDS_Security_Serialize_Property(ser, &seq->_buffer[i]);
    }
}

void
DDS_Security_Serialize_BinaryPropertyArray(
     DDS_Security_Serializer serializer,
     const DDS_Security_BinaryProperty_t **properties,
     const uint32_t propertyLength)
{
    uint32_t i;

     DDS_Security_Serialize_uint32_t(serializer, propertyLength);
     for (i = 0; i < propertyLength ; i++) {
          DDS_Security_Serialize_BinaryProperty(serializer, properties[i]);
     }
}

void
DDS_Security_Serialize_BinaryPropertySeq(
     DDS_Security_Serializer serializer,
     const DDS_Security_BinaryPropertySeq *seq)
{
    uint32_t i;

    DDS_Security_Serialize_uint32_t(serializer, seq->_length);
    for (i = 0; i < seq->_length; i++) {
         DDS_Security_Serialize_BinaryProperty(serializer, &seq->_buffer[i]);
    }
}


static void
DDS_Security_Serialize_DataHolder(
     DDS_Security_Serializer ser,
     const DDS_Security_DataHolder *holder)
{
    DDS_Security_Serialize_string(ser, holder->class_id);
    DDS_Security_Serialize_PropertySeq(ser, &holder->properties);
    DDS_Security_Serialize_BinaryPropertySeq(ser, &holder->binary_properties);
}


static void
DDS_Security_Serialize_BuiltinTopicKey(
     DDS_Security_Serializer ser,
     DDS_Security_BuiltinTopicKey_t key)
{
    serbuffer_align(ser, sizeof(uint32_t));
    DDS_Security_Serialize_uint16(ser, PID_PARTICIPANT_GUID);
    DDS_Security_Serialize_uint16(ser, 16);
    DDS_Security_Serialize_uint32_t(ser, key[0]);
    DDS_Security_Serialize_uint32_t(ser, key[1]);
    DDS_Security_Serialize_uint32_t(ser, key[2]);
    /* 4 Bytes are expected for whatever reason (gid vs guid?). */
    DDS_Security_Serialize_uint32_t(ser, 0);
}

static void
DDS_Security_Serialize_UserDataQosPolicy(
     DDS_Security_Serializer ser,
     DDS_Security_OctetSeq *seq)
{
    if (seq->_length > 0) {
        serbuffer_align(ser, sizeof(uint32_t));
        DDS_Security_Serialize_uint16(ser, PID_USER_DATA);
        DDS_Security_Serialize_uint16(ser, (unsigned short)seq->_length);
        DDS_Security_Serialize_OctetSeq(ser, seq);
    }
}

static void
DDS_Security_Serialize_IdentityToken(
     DDS_Security_Serializer ser,
     DDS_Security_IdentityToken *token)
{
    serbuffer_align(ser, sizeof(uint32_t));
    DDS_Security_Serialize_uint16(ser, PID_IDENTITY_TOKEN);
    DDS_Security_Serialize_mark_len(ser);
    DDS_Security_Serialize_DataHolder(ser, token);
    DDS_Security_Serialize_update_len(ser);
}

static void
DDS_Security_Serialize_PermissionsToken(
     DDS_Security_Serializer ser,
     DDS_Security_PermissionsToken *token)
{
    serbuffer_align(ser, sizeof(uint32_t));
    DDS_Security_Serialize_uint16(ser, PID_PERMISSIONS_TOKEN);
    DDS_Security_Serialize_mark_len(ser);
    DDS_Security_Serialize_DataHolder(ser, token);
    DDS_Security_Serialize_update_len(ser);
}

static void
DDS_Security_Serialize_PropertyQosPolicy(
     DDS_Security_Serializer ser,
     DDS_Security_PropertyQosPolicy *policy)
{
    serbuffer_align(ser, sizeof(uint32_t));
    DDS_Security_Serialize_uint16(ser, PID_PROPERTY_LIST);
    DDS_Security_Serialize_mark_len(ser);
    DDS_Security_Serialize_PropertySeq(ser, &policy->value);
    if (policy->binary_value._length > 0)
      DDS_Security_Serialize_BinaryPropertySeq(ser, &policy->binary_value);
    DDS_Security_Serialize_update_len(ser);
}

static void
DDS_Security_Serialize_ParticipantSecurityInfo(
     DDS_Security_Serializer ser,
     DDS_Security_ParticipantSecurityInfo *info)
{
    serbuffer_align(ser, sizeof(uint32_t));
    DDS_Security_Serialize_uint16(ser, PID_PARTICIPANT_SECURITY_INFO);
    DDS_Security_Serialize_uint16(ser, 8);
    DDS_Security_Serialize_uint32_t(ser, info->participant_security_attributes);
    DDS_Security_Serialize_uint32_t(ser, info->plugin_participant_security_attributes);
}


void
DDS_Security_Serialize_ParticipantBuiltinTopicData(
     DDS_Security_Serializer ser,
     DDS_Security_ParticipantBuiltinTopicData *pdata)
{
    DDS_Security_Serialize_BuiltinTopicKey(ser, pdata->key);
    DDS_Security_Serialize_UserDataQosPolicy(ser, &pdata->user_data.value);
    DDS_Security_Serialize_IdentityToken(ser, &pdata->identity_token);
    DDS_Security_Serialize_PermissionsToken(ser, &pdata->permissions_token);
    DDS_Security_Serialize_PropertyQosPolicy(ser, &pdata->property);
    DDS_Security_Serialize_ParticipantSecurityInfo(ser, &pdata->security_info);
    serbuffer_align(ser, sizeof(uint32_t));
    DDS_Security_Serialize_uint16(ser, PID_SENTINEL);
    DDS_Security_Serialize_uint16(ser, 0);
}

static void
DDS_Security_Serialize_OctetArray(
     DDS_Security_Serializer ser,
     const DDS_Security_octet *data,
     uint32_t length)
{
    serbuffer_adjust_size(ser, length);
    memcpy(&ser->buffer[ser->offset], data, length);
    ser->offset += length;
}

void
DDS_Security_Serialize_KeyMaterial_AES_GCM_GMAC(
     DDS_Security_Serializer ser,
     const DDS_Security_KeyMaterial_AES_GCM_GMAC *data)
{
    DDS_Security_Serialize_OctetArray(ser, data->transformation_kind, sizeof(data->transformation_kind));
    DDS_Security_Serialize_OctetSeq(ser, &data->master_salt);
    DDS_Security_Serialize_OctetArray(ser, data->sender_key_id, sizeof(data->sender_key_id));
    DDS_Security_Serialize_OctetSeq(ser, &data->master_sender_key);
    DDS_Security_Serialize_OctetArray(ser, data->receiver_specific_key_id, sizeof(data->receiver_specific_key_id));
    DDS_Security_Serialize_OctetSeq(ser, &data->master_receiver_specific_key);
}


DDS_Security_Deserializer
DDS_Security_Deserializer_new(
     const unsigned char *data,
     size_t size)
{
    DDS_Security_Deserializer deserializer;

    deserializer = ddsrt_malloc(sizeof(*deserializer));

    deserializer->buffer = data;
    deserializer->cursor = data;
    deserializer->size = size;
    deserializer->remain = size;

    return deserializer;
}

void
DDS_Security_Deserializer_free(
     DDS_Security_Deserializer dser)
{
    ddsrt_free(dser);
}

static void
DDS_Security_Deserialize_align(
     DDS_Security_Deserializer dser,
     size_t size)
{
    size_t l = alignup_ptr(dser->cursor, size);

    if (dser->remain >= l) {
        dser->cursor += l;
        dser->remain -= l;
    } else {
        dser->remain = 0;
    }
}

static int
DDS_Security_Deserialize_uint16(
     DDS_Security_Deserializer dser,
     unsigned short *value)
{
    size_t l = sizeof(*value);

    DDS_Security_Deserialize_align(dser, l);

    if (dser->remain < l) {
        return 0;
    }
    *value = ddsrt_fromBE2u(*(unsigned short *)dser->cursor);
    dser->cursor += l;
    dser->remain -= l;

    return 1;
}

static int
DDS_Security_Deserialize_uint32_t(
     DDS_Security_Deserializer dser,
     uint32_t *value)
{
    size_t l = sizeof(*value);

    DDS_Security_Deserialize_align(dser, l);

    if (dser->remain < l) {
        return 0;
    }
    *value = ddsrt_fromBE4u(*(uint32_t *)dser->cursor);
    dser->cursor += l;
    dser->remain -= l;

    return 1;
}

static int
DDS_Security_Deserialize_string(
      DDS_Security_Deserializer dser,
      char **value)
{
    uint32_t len;
    size_t sz;

    if (!DDS_Security_Deserialize_uint32_t(dser, &len)) {
        return 0;
    }

    sz = (size_t)len;

    if (dser->remain < sz) {
        return 0;
    }

    if (sz > 0 && (dser->cursor[sz-1] == 0)) {
       *value = ddsrt_strdup((char *)dser->cursor);
       /* Consider padding */
       sz = alignup_size(sz, sizeof(uint32_t));
       dser->cursor += sz;
       dser->remain -= sz;
    } else {
       *value = ddsrt_strdup("");
    }
    return 1;
}

static int
DDS_Security_Deserialize_OctetArray(
     DDS_Security_Deserializer dser,
     unsigned char *arr,
     uint32_t length)
{
    if (dser->remain < length) {
        return 0;
    }
    memcpy(arr, dser->cursor, length);
    dser->cursor += length;
    dser->remain -= length;

    return 1;
}

static int
DDS_Security_Deserialize_OctetSeq(
     DDS_Security_Deserializer dser,
     DDS_Security_OctetSeq *seq)
{
    if (!DDS_Security_Deserialize_uint32_t(dser, &seq->_length)) {
        return 0;
    }

    if (dser->remain < seq->_length) {
        return 0;
    }

    if (seq->_length > 0) {
        /* Consider padding */
        size_t a_size = alignup_size(seq->_length, sizeof(uint32_t));
        seq->_buffer = ddsrt_malloc(seq->_length);
        memcpy(seq->_buffer, dser->cursor, seq->_length);
        dser->cursor += a_size;
        dser->remain -= a_size;
    } else {
        seq->_buffer = NULL;
    }
    return 1;
}

static int
DDS_Security_Deserialize_Property(
     DDS_Security_Deserializer dser,
     DDS_Security_Property_t *property)
{
    return DDS_Security_Deserialize_string(dser, &property->name) &&
           DDS_Security_Deserialize_string(dser, &property->value);
}

static int
DDS_Security_Deserialize_BinaryProperty(
     DDS_Security_Deserializer dser,
     DDS_Security_BinaryProperty_t *property)
{
    return DDS_Security_Deserialize_string(dser, &property->name) &&
           DDS_Security_Deserialize_OctetSeq(dser, &property->value);
}

static int
DDS_Security_Deserialize_PropertySeq(
     DDS_Security_Deserializer dser,
     DDS_Security_PropertySeq *seq)
{
    /* A well-formed CDR string is length + content including terminating 0, length is
       4 bytes and 4-byte aligned, so the minimum length for a non-empty property
       sequence is 4+1+(3 pad)+4+1 = 13 bytes.  Just use 8 because it is way faster
       and just as good for checking that the length value isn't completely ridiculous. */
    const uint32_t minpropsize = (uint32_t) (2 * sizeof (uint32_t));
    int r = 1;
    uint32_t i;

    if (!DDS_Security_Deserialize_uint32_t(dser, &seq->_length)) {
        return 0;
    } else if (seq->_length > dser->remain / minpropsize) {
        seq->_length = 0;
        return 0;
    } else if (seq->_length > 0) {
        seq->_buffer = DDS_Security_PropertySeq_allocbuf(seq->_length);
        for (i = 0; i < seq->_length && r; i++) {
            r = DDS_Security_Deserialize_Property(dser, &seq->_buffer[i]);
        }
    }

    return r;
}

static int
DDS_Security_Deserialize_BinaryPropertySeq(
     DDS_Security_Deserializer dser,
     DDS_Security_BinaryPropertySeq *seq)
{
    /* A well-formed CDR string + a well-formed octet sequence: 4+1+(3 pad)+4 = 12 bytes.
       Just use 8 because it is way faster and just as good for checking that the length
       value isn't completely ridiculous. */
    const uint32_t minpropsize = (uint32_t) (2 * sizeof (uint32_t));
    int r = 1;
    uint32_t i;

    if (!DDS_Security_Deserialize_uint32_t(dser, &seq->_length)) {
        return 0;
    } else if (seq->_length > dser->remain / minpropsize) {
        seq->_length = 0;
        return 0;
    } else if (seq->_length > 0) {
        seq->_buffer = DDS_Security_BinaryPropertySeq_allocbuf(seq->_length);
        for (i = 0; i < seq->_length && r; i++) {
            r = DDS_Security_Deserialize_BinaryProperty(dser, &seq->_buffer[i]);
        }
    }

    return r;
}

static int
DDS_Security_Deserialize_DataHolder(
      DDS_Security_Deserializer dser,
      DDS_Security_DataHolder *holder)
{
    return DDS_Security_Deserialize_string(dser, &holder->class_id) &&
           DDS_Security_Deserialize_PropertySeq(dser, &holder->properties) &&
           DDS_Security_Deserialize_BinaryPropertySeq(dser, &holder->binary_properties);
}


static int
DDS_Security_Deserialize_PropertyQosPolicy(
     DDS_Security_Deserializer dser,
     DDS_Security_PropertyQosPolicy *policy,
     size_t len)
{
    size_t sl = dser->remain;

    if (!DDS_Security_Deserialize_PropertySeq(dser, &policy->value))
        return 0;

    DDS_Security_Deserialize_align(dser, 4);
    size_t consumed = sl - dser->remain;
    if (consumed > len) {
        return 0;
    } else if (len - consumed >= 4) {
         return DDS_Security_Deserialize_BinaryPropertySeq(dser, &policy->binary_value);
    }
    return 1;
}

static int
DDS_Security_Deserialize_BuiltinTopicKey(
     DDS_Security_Deserializer dser,
     DDS_Security_BuiltinTopicKey_t key)
{
    int r = DDS_Security_Deserialize_uint32_t(dser, (uint32_t *)&key[0]) &&
        DDS_Security_Deserialize_uint32_t(dser, (uint32_t *)&key[1]) &&
        DDS_Security_Deserialize_uint32_t(dser, (uint32_t *)&key[2]);

    /* guid is 16 bytes, so skip the last 4 bytes */
    dser->cursor += 4;
    dser->remain -= 4;

    return r;
}

static int
DDS_Security_Deserialize_ParticipantSecurityInfo(
    DDS_Security_Deserializer dser,
    DDS_Security_ParticipantSecurityInfo *info)
{
    return DDS_Security_Deserialize_uint32_t(dser, &info->participant_security_attributes) &&
         DDS_Security_Deserialize_uint32_t(dser, &info->plugin_participant_security_attributes);
}

int
DDS_Security_Deserialize_ParticipantBuiltinTopicData(
     DDS_Security_Deserializer dser,
     DDS_Security_ParticipantBuiltinTopicData *pdata,
     DDS_Security_SecurityException *ex)
{
    unsigned short len=0;
    unsigned short pid=0;
    int r, ready = 0;

    do {
        DDS_Security_Deserialize_align(dser, 4);
        r = DDS_Security_Deserialize_uint16(dser, &pid) &&
            DDS_Security_Deserialize_uint16(dser, &len);

        if (r && (len <= dser->remain)) {
            const unsigned char *next_cursor = dser->cursor + len;

            switch (pid) {
            case PID_PARTICIPANT_GUID:
                r = DDS_Security_Deserialize_BuiltinTopicKey(dser, pdata->key);
                break;
            case PID_USER_DATA:
                r = DDS_Security_Deserialize_OctetSeq(dser, &pdata->user_data.value);
                break;
            case PID_IDENTITY_TOKEN:
                r = DDS_Security_Deserialize_DataHolder(dser, &pdata->identity_token);
                break;
            case PID_PERMISSIONS_TOKEN:
                r = DDS_Security_Deserialize_DataHolder(dser, &pdata->permissions_token);
                break;
            case PID_PROPERTY_LIST:
                r = DDS_Security_Deserialize_PropertyQosPolicy(dser, &pdata->property, len);
                break;
            case PID_PARTICIPANT_SECURITY_INFO:
                r = DDS_Security_Deserialize_ParticipantSecurityInfo(dser, &pdata->security_info);
                break;
            case PID_SENTINEL:
                ready = 1;
                break;
            default:
                dser->cursor += len;
                dser->remain -= len;
                break;
            }

            if (r) {
                if (dser->cursor != next_cursor) {
                    DDS_Security_Exception_set(ex, "Deserialization", DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                            "Deserialize PID 0x%x failed: internal_size %d != external_size %d", pid, (int)len + (int)(dser->cursor - next_cursor), (int)len);
                    r = 0;
                }
            } else {
                DDS_Security_Exception_set(ex, "Deserialization", DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                        "Deserialize PID 0x%x failed: parsing failed", pid);
            }
        } else {
            if (!r) {
                DDS_Security_Exception_set(ex, "Deserialization", DDS_SECURITY_ERR_UNDEFINED_CODE, DDS_SECURITY_VALIDATION_FAILED,
                        "Deserialize parameter header failed");
            }
        }
    } while (r && !ready && dser->remain > 0);

    return ready;
}

void
DDS_Security_BuiltinTopicKeyBE(
     DDS_Security_BuiltinTopicKey_t dst,
     const  DDS_Security_BuiltinTopicKey_t src)
{
    dst[0] = ddsrt_toBE4u(src[0]);
    dst[1] = ddsrt_toBE4u(src[1]);
    dst[2] = ddsrt_toBE4u(src[2]);
}

int
DDS_Security_Deserialize_KeyMaterial_AES_GCM_GMAC(
     DDS_Security_Deserializer dser,
     DDS_Security_KeyMaterial_AES_GCM_GMAC *data)
{
    memset(data, 0, sizeof(*data));
    return
        DDS_Security_Deserialize_OctetArray(dser, data->transformation_kind, sizeof(data->transformation_kind)) &&
        DDS_Security_Deserialize_OctetSeq(dser, &data->master_salt) &&
        DDS_Security_Deserialize_OctetArray(dser, data->sender_key_id, sizeof(data->sender_key_id)) &&
        DDS_Security_Deserialize_OctetSeq(dser, &data->master_sender_key) &&
        DDS_Security_Deserialize_OctetArray(dser, data->receiver_specific_key_id, sizeof(data->receiver_specific_key_id)) &&
        DDS_Security_Deserialize_OctetSeq(dser, &data->master_receiver_specific_key);
}
