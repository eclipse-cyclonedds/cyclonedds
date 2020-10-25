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
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/endian.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/types.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/openssl_support.h"
#include "cryptography.h"
#include "crypto_cipher.h"
#include "crypto_defs.h"
#include "crypto_key_factory.h"
#include "crypto_objects.h"
#include "crypto_transform.h"
#include "crypto_utils.h"

#define CRYPTO_ENCRYPTION_MAX_PADDING 32
#define CRYPTO_FOOTER_BASIC_SIZE (CRYPTO_HMAC_SIZE + sizeof(uint32_t))
#define INFO_SRC_SIZE 24
#define INFO_SRC_HDR_SIZE 8
#define RTPS_HEADER_SIZE 20

struct submsg_header
{
  unsigned char id;
  unsigned char flags;
  uint16_t length;
};

struct crypto_header
{
  struct CryptoTransformIdentifier transform_identifier;
  unsigned char session_id[CRYPTO_SESSION_ID_SIZE];
  unsigned char init_vector_suffix[CRYPTO_INIT_VECTOR_SUFFIX_SIZE];
};

struct crypto_contents
{
  uint32_t _length;
  unsigned char _data[];
};

struct crypto_contents_ref
{
  uint32_t _length;
  unsigned char *_data;
};

struct receiver_specific_mac_seq
{
  uint32_t _length;
  struct receiver_specific_mac _buffer[];
};

struct crypto_footer
{
  crypto_hmac_t common_mac;
  struct receiver_specific_mac_seq receiver_specific_macs;
};

struct encrypted_data
{
  uint32_t length;
  unsigned char data[];
};

/*
const DDS_octet INFO_SRC_HDR[] =
   {
         RTPS_Message_Type_INFO_SRC,
         0x00, // BIG ENDIAN
         0x14,
         0x00,
         0x00,
         0x00,
         0x00,
         0x00
   };
*/


/**
 * Implementation structure for storing encapsulated members of the instance
 * while giving only the interface definition to user
 */
typedef struct dds_security_crypto_transform_impl
{
  dds_security_crypto_transform base;
  const dds_security_cryptography *crypto;
} dds_security_crypto_transform_impl;

static bool
is_encryption_required(
    uint32_t transform_kind)
{
  return ((transform_kind == CRYPTO_TRANSFORMATION_KIND_AES256_GCM) ||
          (transform_kind == CRYPTO_TRANSFORMATION_KIND_AES128_GCM));
}

static bool
is_authentication_required(
    uint32_t transform_kind)
{
  return ((transform_kind == CRYPTO_TRANSFORMATION_KIND_AES256_GMAC) ||
          (transform_kind == CRYPTO_TRANSFORMATION_KIND_AES128_GMAC));
}

static bool
is_encryption_expected(
    DDS_Security_ProtectionKind protection_kind)
{
  return ((protection_kind == DDS_SECURITY_PROTECTION_KIND_ENCRYPT) ||
          (protection_kind == DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION));
}

static bool
is_authentication_expected(
    DDS_Security_ProtectionKind protection_kind)
{
  return ((protection_kind == DDS_SECURITY_PROTECTION_KIND_SIGN) ||
          (protection_kind == DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION));
}

static bool
has_origin_authentication(
    DDS_Security_ProtectionKind kind)
{
  return ((kind == DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION) || (kind == DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION));
}

static inline bool
crypto_buffer_read_uint32(
    uint32_t *value,
    unsigned char **ptr,
    uint32_t *remain)
{
  if ((*remain) < sizeof(uint32_t))
    return false;

  *value = ddsrt_fromBE4u(*(uint32_t *)(*ptr));
  (*ptr) += sizeof(uint32_t);
  (*remain) -= (uint32_t)sizeof(uint32_t);

  return true;
}

static inline bool
crypto_buffer_read_bytes(
    unsigned char *bytes,
    uint32_t num,
    unsigned char **ptr,
    uint32_t *remain)
{
  if ((*remain) < num)
    return false;
  memcpy(bytes, *ptr, num);
  (*ptr) += num;
  (*remain) -= num;

  return true;
}


/**
 * Increase the length of the submessage
 * When the buffer does not contain enough memory the
 * buffer is reallocated with the increased size.
 * The function returns the submessage which may
 * be reallocated in memory.
 */
static struct submsg_header *
append_submessage(
    DDS_Security_OctetSeq *seq,
    struct submsg_header *msg,
    size_t size)
{
  assert ((size_t)msg->length + size <= UINT16_MAX);
  if (size + seq->_length > seq->_maximum)
  {
    size_t l = seq->_length + size;
    unsigned char *ptr = seq->_buffer;
    size_t offset = (size_t)(((unsigned char *)msg) - ptr);

    seq->_buffer = ddsrt_realloc(ptr, l);
    seq->_maximum = (DDS_Security_unsigned_long)l;
    msg = (struct submsg_header *)&seq->_buffer[offset];
  }
  seq->_length += (uint32_t)size;
  msg->length = (uint16_t)(msg->length + size);

  return msg;
}

/**
 * Add a new submessage to the tail of the supplied buffer.
 * When the supplied buffer does not contain enough memory
 * it will be reallocated.
 * The buffer's maximum indicated the total size of the
 * buffer. The buffer's length indicates the used size.
 *
 * @param[in] seq   Buffer
 * @param[in] id    Submessage id
 * @param[in] flags Indicates big or little endian
 * @param[in] size  The octetToNextSubmessage
 */
static struct submsg_header *
add_submessage(
    DDS_Security_OctetSeq *seq,
    unsigned char id,
    unsigned char flags,
    size_t size)
{
  struct submsg_header *msg;
  size_t len = sizeof(struct submsg_header) + size;
  assert (size <= UINT16_MAX);

  if (len + seq->_length > seq->_maximum)
  {
    size_t l = len + seq->_length;
    unsigned char *ptr = seq->_buffer;
    seq->_buffer = ddsrt_realloc (ptr, l);
    seq->_maximum = (DDS_Security_unsigned_long)l;
  }

  msg = (struct submsg_header *)&seq->_buffer[seq->_length];
  msg->id = id;
  msg->flags = flags;
  msg->length = (uint16_t)size;
  seq->_length += (uint32_t)len;

  return msg;
}

/**
 * @brief Adds info_src content after info_src header and returns next available location
 */
static bool
add_info_src(
    DDS_Security_OctetSeq *seq,
    unsigned char *rtps_header,
    unsigned char flags)
{
  struct submsg_header *info_src_hdr = add_submessage(seq, SMID_SRTPS_INFO_SRC_KIND,
    flags, INFO_SRC_SIZE - sizeof(struct submsg_header));
  unsigned char *ptr = (unsigned char *)(info_src_hdr + 1);
  memset(ptr, 0, 4); /* skip unused bytes */
  memcpy(ptr + 4, rtps_header + 4, INFO_SRC_SIZE - sizeof(struct submsg_header) - 4);
  return true;
}

static bool
read_rtps_header(
    DDS_Security_OctetSeq *rtps_header,
    unsigned char **ptr,
    uint32_t *remain)
{
  if ((*remain) > RTPS_HEADER_SIZE)
  {
    rtps_header->_buffer = *ptr;
    rtps_header->_length = rtps_header->_maximum = RTPS_HEADER_SIZE;
    (*ptr) += RTPS_HEADER_SIZE;
    (*remain) -= RTPS_HEADER_SIZE;
    return true;
  }

  return false;
}

/**
 * Initialize the remote session info which is used
 * to decode a received message. It will calculate the
 * session key from the received crypto_header.
 *
 * @param[in,out] info                The remote session information which is determined by this function
 * @param[in]     header              The received crypto_header
 * @param[in]     master_salt         The master_salt associated with the remote entity
 * @param[in]     master_key          The master_key associated with the remote entity
 * @param[in]     transformation_kind The transformation kind (to determine key and salt size)
 * @param[in,out] ex                  Security exception
 */
static bool
initialize_remote_session_info(
    remote_session_info *info,
    struct crypto_header *header,
    const unsigned char *master_salt,
    const unsigned char *master_key,
    DDS_Security_CryptoTransformKind_Enum transformation_kind,
    DDS_Security_SecurityException *ex)
{
  info->key_size = crypto_get_key_size (transformation_kind);
  info->id = CRYPTO_TRANSFORM_ID(header->session_id);
  return crypto_calculate_session_key(&info->key, info->id, master_salt, master_key, transformation_kind, ex);
}

static bool transform_kind_valid(DDS_Security_CryptoTransformKind_Enum kind)
{
  return ((kind == CRYPTO_TRANSFORMATION_KIND_AES128_GMAC) ||
          (kind == CRYPTO_TRANSFORMATION_KIND_AES128_GCM) ||
          (kind == CRYPTO_TRANSFORMATION_KIND_AES256_GMAC) ||
          (kind == CRYPTO_TRANSFORMATION_KIND_AES256_GCM));
}

/**
 * @brief Read the crypto_header from the received data buffer
 *
 * @param[in]     header  The returned crypto_header
 * @param[in,out] ptr     Current read pointer in the received buffer
 * @param[in,out] remain  Remaining size in the received buffer
 */
static bool
read_crypto_header(
    struct crypto_header *header,
    unsigned char **ptr,
    uint32_t *remain)
{
  return crypto_buffer_read_bytes((unsigned char *)header, sizeof(*header), ptr, remain);
}

/**
 * @brief Read the payload from the received data buffer
 *
 * @param[in,out] contents  The returned payload
 * @param[in]     ptr       Current read pointer in the received buffer
 * @param[in]     size      Remaining size in the received buffer
 */
static DDS_Security_boolean
read_crypto_contents(
    struct crypto_contents_ref *contents,
    unsigned char *ptr,
    uint32_t size)
{
  bool result = false;

  if (crypto_buffer_read_uint32(&contents->_length, &ptr, &size))
  {
    contents->_data = ptr;
    if (size == contents->_length)
      result = true;
  }
  return result;
}

/**
 * Read the crypto_footer from the received data buffer
 * The size of the footer depends on the number of
 * receiver specific macs present in the message.
 *
 * @param[in,out] footer  The returned crypto_footer
 * @param[in,out] ptr     Current read pointer in the received buffer
 * @param[in,out] remain  Remaining size in the received buffer
 */
static bool
read_crypto_footer(
    struct crypto_footer **footer,
    unsigned char **ptr,
    uint32_t *remain)
{
  uint32_t len;
  size_t sz;
  struct crypto_footer *ft;
  crypto_hmac_t common_mac;

  if (!crypto_buffer_read_bytes(common_mac.data, CRYPTO_HMAC_SIZE, ptr, remain) ||
      !crypto_buffer_read_uint32(&len, ptr, remain))
    return false;

  if (len > (*remain))
    return false;

  sz = CRYPTO_HMAC_SIZE + sizeof(uint32_t) + len * sizeof(struct receiver_specific_mac);
  ft = ddsrt_malloc(sz);

  memcpy(ft->common_mac.data, common_mac.data, CRYPTO_HMAC_SIZE);
  ft->receiver_specific_macs._length = len;

  if (len > 0)
  {
    sz = len * sizeof(struct receiver_specific_mac);
    if (!crypto_buffer_read_bytes((unsigned char *)&ft->receiver_specific_macs._buffer[0], (uint32_t)sz, ptr, remain))
    {
      ddsrt_free(ft);
      *footer = NULL;
      return false;
    }
  }

  *footer = ft;
  return true;
}

/**
 * @brief Read the submessage header from the received data buffer
 *
 * @param[in,out] submsg  The returned submessage header
 * @param[in,out] ptr     Current read pointer in the received buffer
 * @param[in,out] remain  Remaining size in the received buffer
 */
static bool
read_submsg_header(
    struct submsg_header *submsg,
    unsigned char **ptr,
    uint32_t *remain)
{
  int swap;
  bool result = crypto_buffer_read_bytes((unsigned char *)submsg, sizeof(*submsg), ptr, remain);
  if (result)
  {
    if ((submsg->flags & 0x01) == 0)
      swap = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN);
    else
      swap = (DDSRT_ENDIAN != DDSRT_LITTLE_ENDIAN);
    submsg->length = swap ? ddsrt_bswap2u(submsg->length) : submsg->length;
    if ((uint32_t)(submsg->length) > (*remain))
      result = false;
  }

  return result;
}

/**
 * @brief Used by the decode_serialized_payload function to split the received message in the composed components:
 *
 * @param[in]     payload     The received serialized payload
 * @param[in]     header      The crypto_header contained in the payload
 * @param[in,out] payload_ptr The actual payload (either encoded or plain)
 * @param[in,out] payload_len Length of the payload
 * @param[in,out] footer      The crypto_footer contained in the payload
 */
static bool
split_encoded_serialized_payload(
    const DDS_Security_OctetSeq *payload,
    struct crypto_header *header,
    unsigned char **payload_ptr,
    uint32_t *payload_len,
    struct crypto_footer **footer)
{
  /* For data, the footer is always the same length. */
  const uint32_t footer_len = CRYPTO_HMAC_SIZE + sizeof(uint32_t);
  unsigned char *ptr = payload->_buffer;
  uint32_t remain = payload->_length;

  /* Get header. */
  if (read_crypto_header(header, &ptr, &remain))
  {
    if (remain >= footer_len)
    {
      /* Get contents. */
      *payload_ptr = ptr;
      *payload_len = remain - footer_len;
      /* Get footer. */
      ptr = (*payload_ptr) + (*payload_len);
      remain = footer_len;
      if (read_crypto_footer(footer, &ptr, &remain))
        return true;
    }
  }

  return false;
}

/**
 * @brief Read the SEC_PREFIX submessage from a received message
 *
 * @param[in,out] prefix    The contents of the SEC_PREFIX submessage header
 * @param[in,out] header    The crypto_header contained in the SEC_PREFIX submessage
 * @param[in,out] ptr       Current read pointer in the received buffer
 * @param[in,out] remain    Remaining size in the received buffer
 * @param[in]     expected  Expected submessage kind
 */
static bool
read_prefix_submessage(
    struct submsg_header *prefix,
    struct crypto_header *header,
    unsigned char **ptr,
    uint32_t *remain,
    enum SecureSubmsgKind_t expected)
{
  uint32_t datalen;

  if (!read_submsg_header(prefix, ptr, remain))
    return false;

  if (prefix->id != expected)
    return false;

  datalen = prefix->length;
  if (datalen != sizeof(struct crypto_header))
    return false;

  if (!read_crypto_header(header, ptr, remain))
    return false;

  return true;
}

/**
 * @brief Read the SEC_POSTFIX submessage from a received message
 *
 * @param[in,out] postfix   The contents of the SEC_POSTFIX submessage header
 * @param[in,out] footer    The crypto_footer contained in the SEC_POSTFIX submessage
 * @param[in,out] ptr       Current read pointer in the received buffer
 * @param[in,out] remain    Remaining size in the received buffer
 * @param[in]     expected  Expected submessage kind
 */
static bool
read_postfix_submessage(
    struct submsg_header *postfix,
    struct crypto_footer **footer,
    unsigned char **ptr,
    uint32_t *remain,
    enum SecureSubmsgKind_t expected)
{
  uint32_t datalen;
  size_t sz;

  if (!read_submsg_header(postfix, ptr, remain))
    return false;

  if (postfix->id != expected)
    return false;

  datalen = postfix->length;

  if (!read_crypto_footer(footer, ptr, remain))
    return false;

  sz = CRYPTO_FOOTER_BASIC_SIZE + (*footer)->receiver_specific_macs._length * sizeof(struct receiver_specific_mac);

  if (datalen != sz)
  {
    ddsrt_free(*footer);
    *footer = NULL;
    return false;
  }

  return true;
}

/**
 * Read the body submessage from a received message
 * The message received may be encoded or not. When encoded
 * a SEC_BODY submessage is present otherwise the original
 * submessage is present. When a SEC_BODY submessage is present
 * the contents will be set to the contents of the SEC_BODY otherwise
 * the contents will be set to the submessage itself.
 *
 * @param[in,out] body      The contents of the body submessage header
 * @param[in,out] contents  The contents of the SEC_BODY submessage or the submessage itself
 * @param[in,out] ptr       Current read pointer in the received buffer
 * @param[in,out] remain    Remaining size in the received buffer
 */
static bool
read_body_submessage(
    struct submsg_header *body,
    struct crypto_contents_ref *contents,
    unsigned char **ptr,
    uint32_t *remain)
{
  uint32_t datalen;
  struct encrypted_data *encrypted;

  contents->_data = *ptr;

  if (!read_submsg_header(body, ptr, remain))
    return false;

  datalen = body->length;
  if (body->id == SMID_SEC_BODY_KIND)
  {
    encrypted = (struct encrypted_data *)*ptr;
    contents->_length = ddsrt_fromBE4u(encrypted->length);
    contents->_data = encrypted->data;
    if (contents->_length > datalen)
      return false;
  }
  else
  {
    contents->_length = datalen + (uint32_t)sizeof(struct submsg_header);
  }

  if ((*remain) < datalen)
    return false;

  (*ptr) += datalen;
  (*remain) -= datalen;

  return true;
}

/**
 * body is invalid if not encrypted
 * info_src is invalid if encrypted
 */
static bool
read_rtps_body(
    struct submsg_header *body,
    struct crypto_contents_ref *contents,
    DDS_Security_CryptoTransformKind_Enum transformation_kind,
    unsigned char **ptr,
    uint32_t *remain)
{
  uint32_t datalen;
  struct submsg_header submessage_header;

  if (is_encryption_required(transformation_kind))
  { /*read sec body */
    if (!read_body_submessage(body, contents, ptr, remain) || body->id != SMID_SEC_BODY_KIND)
      return false;
  }
  else
  {
    unsigned char *body_start_with_info = *ptr;
    bool arrived_to_postfix = false;

    if (!read_submsg_header(&submessage_header, ptr, remain) || submessage_header.id != SMID_SRTPS_INFO_SRC_KIND)
      return false;

    (*ptr) += submessage_header.length;
    (*remain) -= submessage_header.length;

    while (*remain > sizeof(struct submsg_header) && !arrived_to_postfix)
    {
      if (!read_submsg_header(&submessage_header, ptr, remain))
        return false;

      if (submessage_header.id != SMID_SRTPS_POSTFIX_KIND)
      {
        (*ptr) += submessage_header.length;
        (*remain) -= submessage_header.length;
      }
      else
      {
        /*revert last read*/
        (*ptr) -= sizeof(struct submsg_header);
        (*remain) += (uint32_t)sizeof(struct submsg_header);
        arrived_to_postfix = true;
      }
    }

    if (!arrived_to_postfix)
      return false;

    datalen = (uint32_t)(*ptr - body_start_with_info); /* rtps submessage + info_src + body data*/
    contents->_data = body_start_with_info;
    contents->_length = datalen;
  }

  return true;
}

/**
 * @brief Split a received message is the various components
 *
 * @param[in]     data      Buffer containing the received message
 * @param[in]     prefix    The SEC_PREFIX submessage header
 * @param[in]     body      The body submessage, either SEC_BODY or the not encoded submessage
 * @param[in]     postfix   The SEC_POSTFIX submessage header
 * @param[in]     header    Crypto_header contained in the SEC_PREFIX submessage
 * @param[in]     contents  Crypto contents
 * @param[in,out] footer    Crypto_footer contained in the SEC_POSTFIX submessage
 */
static bool
split_encoded_submessage(
    const DDS_Security_OctetSeq *data,
    struct submsg_header *prefix,
    struct submsg_header *body,
    struct submsg_header *postfix,
    struct crypto_header *header,
    struct crypto_contents_ref *contents,
    struct crypto_footer **footer)
{
  unsigned char *ptr = data->_buffer;
  uint32_t remain = data->_length;
  *footer = NULL;

  return (read_prefix_submessage(prefix, header, &ptr, &remain, SMID_SEC_PREFIX_KIND) &&
      read_body_submessage(body, contents, &ptr, &remain) &&
      read_postfix_submessage(postfix, footer, &ptr, &remain, SMID_SEC_POSTFIX_KIND));
}

static bool
split_encoded_rtps_message(
    const DDS_Security_OctetSeq *data,
    DDS_Security_OctetSeq *rtps_header,
    struct submsg_header *prefix,
    struct submsg_header *body,
    struct submsg_header *postfix,
    struct crypto_header *header,
    struct crypto_contents_ref *contents,
    struct crypto_footer **footer)
{
  unsigned char *ptr = data->_buffer;
  uint32_t remain = data->_length;
  uint32_t transform_kind;
  *footer = NULL;
  if (rtps_header->_buffer)
    return false;
  if (!read_rtps_header(rtps_header, &ptr, &remain))
    return false;
  if (!read_prefix_submessage(prefix, header, &ptr, &remain, SMID_SRTPS_PREFIX_KIND))
    return false;

  transform_kind = CRYPTO_TRANSFORM_KIND(header->transform_identifier.transformation_kind);

  if (!read_rtps_body(body, contents, transform_kind, &ptr, &remain))
    return false;
  if (!read_postfix_submessage(postfix, footer, &ptr, &remain, SMID_SRTPS_POSTFIX_KIND))
    return false;

  return true;
}

/**
 * @brief Initialize the crypto_header
 */
static void
set_crypto_header(
    struct crypto_header *header,
    uint32_t transform_kind,
    uint32_t transform_id,
    uint32_t session_id,
    uint64_t init_vector_suffx)
{
  struct
  {
    uint32_t tkind;
    uint32_t tid;
    uint32_t sid;
    uint32_t ivh;
    uint32_t ivl;
  } s;
  uint64_t ivs = ddsrt_toBE8u(init_vector_suffx);

  s.tkind = ddsrt_toBE4u(transform_kind);
  s.tid = ddsrt_toBE4u(transform_id);
  s.sid = ddsrt_toBE4u(session_id);
  s.ivh = (uint32_t)(ivs >> 32);
  s.ivl = (uint32_t)ivs;

  memcpy(header, &s, sizeof(*header));
}

/*
 * Function implementations
 */

static DDS_Security_boolean
encode_serialized_payload(
    dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *encoded_buffer,
    DDS_Security_OctetSeq *extra_inline_qos,
    const DDS_Security_OctetSeq *plain_buffer,
    const DDS_Security_DatawriterCryptoHandle writer_id,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_transform_impl *impl = (dds_security_crypto_transform_impl *)instance;
  dds_security_crypto_key_factory *factory;
  session_key_material *session;
  struct crypto_header *header;
  struct crypto_contents *contents = NULL;
  struct crypto_footer *footer;
  unsigned char *buffer;
  unsigned char *payload;
  crypto_hmac_t hmac;
  uint32_t payload_len;
  uint32_t transform_kind, transform_id;
  size_t size, offset;

  DDSRT_UNUSED_ARG(extra_inline_qos);

  memset(hmac.data, 0, sizeof(crypto_hmac_t));

  if (!instance || !encoded_buffer || !plain_buffer || plain_buffer->_length == 0 || writer_id == 0)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "encode_serialized_payload: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    goto fail_inv_arg;
  }

  /* check if the payload is aligned on a 4 byte boundary */
  if ((plain_buffer->_length % 4) != 0)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_DATA_NOT_ALIGNED_CODE, 0,
        "encode_serialized_payload: " DDS_SECURITY_ERR_INVALID_CRYPTO_DATA_NOT_ALIGNED_MESSAGE);
    goto fail_inv_arg;
  }

  /* Retrieve key material from sending_datawriter_crypto from factory */
  factory = cryptography_get_crypto_key_factory(impl->crypto);
  if (!crypto_factory_get_writer_key_material(factory, writer_id, 0, true, &session, NULL, ex))
    goto fail_inv_arg;

  if (!session)
  {
    DDS_Security_OctetSeq_copy(encoded_buffer, plain_buffer);
    return true;
  }

  transform_kind = session->master_key_material->transformation_kind;
  transform_id = session->master_key_material->sender_key_id;

  if (!is_encryption_required(transform_kind) && !is_authentication_required(transform_kind))
  {
    DDS_Security_OctetSeq_copy(encoded_buffer, plain_buffer);
    CRYPTO_OBJECT_RELEASE(session);
    return true;
  }

  /* update sessionKey when needed */
  if (!crypto_session_key_material_update(session, plain_buffer->_length, ex))
    goto fail_update_key;

  /* increment init_vector_suffix */
  session->init_vector_suffix++;

  /*
     * allocate buffer for encoded data, size includes:
     * - CryptoHeader
     * if (encrypted) {
     *   - CryptoContents : size of the data + cypher block_size + 1
     * } else {
     *   - Plain payload
     * }
     * - CryptoFooter
     * See spec: 9.5.3.3.4.4 Result from encode_serialized_payload
     * Make sure to allocate enough memory for both options.
     */
  size = sizeof(*header) + sizeof(*contents) + sizeof(*footer) + plain_buffer->_length + session->block_size + 1;
  buffer = ddsrt_malloc(size);
  header = (struct crypto_header *)buffer;
  payload = &buffer[sizeof(*header)];

  /* create CryptoHeader */
  set_crypto_header(header, transform_kind, transform_id, session->id, session->init_vector_suffix);

  /* if the transformation_kind indicates encryption then encrypt the buffer */
  payload_len = 0;
  if (is_encryption_required(transform_kind))
  {
    contents = (struct crypto_contents *)payload;
    if (!crypto_cipher_encrypt_data(&session->key, session->key_size, header->session_id, plain_buffer->_buffer, plain_buffer->_length, NULL, 0, contents->_data, &payload_len, &hmac, ex))
      goto fail_encrypt;
    contents->_length = ddsrt_toBE4u(payload_len);
    payload_len += (uint32_t)sizeof(uint32_t);
  }
  else if (is_authentication_required(transform_kind))
  {
    /* the transformation_kind indicates only indicates authentication the determine HMAC */
    if (!crypto_cipher_encrypt_data(&session->key, session->key_size, header->session_id, NULL, 0, plain_buffer->_buffer, plain_buffer->_length, NULL, NULL, &hmac, ex))
      goto fail_encrypt;
    memcpy(payload, plain_buffer->_buffer, plain_buffer->_length);
    payload_len = plain_buffer->_length;
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "encode_serialized_payload: unknown transform_kind %d", (int)transform_kind);
    goto fail_encrypt;
  }

  /* create CryptoFooter */
  offset = sizeof(*header) + payload_len;
  footer = (struct crypto_footer *)(buffer + offset);
  memset(footer, 0, sizeof(*footer));
  memcpy(footer->common_mac.data, hmac.data, CRYPTO_HMAC_SIZE);

  size = offset + CRYPTO_HMAC_SIZE + sizeof(uint32_t);

  encoded_buffer->_length = encoded_buffer->_maximum = (uint32_t)size;
  encoded_buffer->_buffer = buffer;

  CRYPTO_OBJECT_RELEASE(session);

  return true;

fail_encrypt:
  ddsrt_free(buffer);
fail_update_key:
  CRYPTO_OBJECT_RELEASE(session);
fail_inv_arg:
  return false;
}

static bool
add_reader_specific_mac(
    DDS_Security_OctetSeq *data,
    struct submsg_header *postfix,
    master_key_material *key_material,
    session_key_material *session,
    DDS_Security_ProtectionKind protection_kind,
    DDS_Security_SecurityException *ex)
{
  bool result = true;
  struct submsg_header *prefix;
  struct crypto_header *header;
  struct crypto_footer *footer;
  crypto_session_key_t key;
  crypto_hmac_t hmac;

  if (has_origin_authentication(protection_kind))
  {
    uint32_t index;

    postfix = append_submessage(data, postfix, sizeof(struct receiver_specific_mac));
    /* determine header crypto header because the append operation may have changed the buffer location */
    prefix = (struct submsg_header *)data->_buffer;
    header = (struct crypto_header *)(prefix + 1);
    footer = (struct crypto_footer *)(postfix + 1);
    index = ddsrt_fromBE4u(footer->receiver_specific_macs._length);

    if (!crypto_calculate_receiver_specific_key(&key, session->id, key_material->master_salt, key_material->master_receiver_specific_key, key_material->transformation_kind, ex) ||
        !crypto_cipher_encrypt_data(&key, session->key_size, header->session_id, NULL, 0, footer->common_mac.data, CRYPTO_HMAC_SIZE, NULL, NULL, &hmac, ex))
    {
      result = false;
    }
    else
    {
      uint32_t key_id = ddsrt_toBE4u(key_material->receiver_specific_key_id);
      struct receiver_specific_mac *rcvmac = footer->receiver_specific_macs._buffer + index;
      memcpy(rcvmac->receiver_mac.data, hmac.data, CRYPTO_HMAC_SIZE);
      memcpy(rcvmac->receiver_mac_key_id, &key_id, sizeof(key_id));
      footer->receiver_specific_macs._length = ddsrt_toBE4u(++index);
    }
  }

  return result;
}

static bool
add_crypto_reader_specific_mac(
    dds_security_crypto_key_factory *factory,
    DDS_Security_OctetSeq *data,
    DDS_Security_DatareaderCryptoHandle reader_crypto,
    struct submsg_header *postfix,
    DDS_Security_SecurityException *ex)
{
  bool result = true;
  master_key_material *key_material = NULL;
  session_key_material *session = NULL;
  DDS_Security_ProtectionKind protection_kind;

  if (!crypto_factory_get_remote_reader_sign_key_material(factory, reader_crypto, &key_material, &session, &protection_kind, ex))
    return false;

  result = add_reader_specific_mac(data, postfix, key_material, session, protection_kind, ex);

  CRYPTO_OBJECT_RELEASE(session);
  CRYPTO_OBJECT_RELEASE(key_material);

  return result;
}

static bool
add_receiver_specific_mac(
    dds_security_crypto_key_factory *factory,
    DDS_Security_OctetSeq *data,
    DDS_Security_DatareaderCryptoHandle sending_participant_crypto,
    DDS_Security_DatareaderCryptoHandle receiving_participant_crypto,
    struct submsg_header *postfix,
    DDS_Security_SecurityException *ex)
{
  bool result = true;
  session_key_material *session = NULL;
  struct submsg_header *prefix;
  struct crypto_header *header;
  struct crypto_footer *footer;
  DDS_Security_ProtectionKind local_protection_kind;
  DDS_Security_ProtectionKind remote_protection_kind;
  crypto_session_key_t key;
  crypto_hmac_t hmac;
  participant_key_material *pp_key_material;

  /* get local crypto and session*/
  if (!crypto_factory_get_local_participant_data_key_material(factory, sending_participant_crypto, &session, &local_protection_kind, ex))
    return false;

  /* get remote crypto tokens */
  if (!crypto_factory_get_participant_crypto_tokens(factory, sending_participant_crypto, receiving_participant_crypto, &pp_key_material, NULL, &remote_protection_kind, ex))
  {
    CRYPTO_OBJECT_RELEASE(session);
    return false;
  }

  if (has_origin_authentication(remote_protection_kind))
  {
    uint32_t index;

    postfix = append_submessage(data, postfix, sizeof(struct receiver_specific_mac));
    /* determine header crypto header because the append operation may have changed the buffer location */
    prefix = (struct submsg_header *)(data->_buffer + RTPS_HEADER_SIZE);
    header = (struct crypto_header *)(prefix + 1);
    footer = (struct crypto_footer *)(postfix + 1);
    index = ddsrt_fromBE4u(footer->receiver_specific_macs._length);

    if (!crypto_calculate_receiver_specific_key(&key, session->id, pp_key_material->local_P2P_key_material->master_salt,
            pp_key_material->local_P2P_key_material->master_receiver_specific_key, pp_key_material->local_P2P_key_material->transformation_kind, ex) ||
        !crypto_cipher_encrypt_data(&key, session->key_size, header->session_id, NULL, 0, footer->common_mac.data, CRYPTO_HMAC_SIZE, NULL, NULL, &hmac, ex))
    {
      result = false;
    }
    else
    {
      uint32_t key_id = ddsrt_toBE4u(pp_key_material->local_P2P_key_material->receiver_specific_key_id);
      struct receiver_specific_mac *rcvmac = footer->receiver_specific_macs._buffer + index;
      memcpy(rcvmac->receiver_mac.data, hmac.data, CRYPTO_HMAC_SIZE);
      memcpy(rcvmac->receiver_mac_key_id, &key_id, sizeof(key_id));
      footer->receiver_specific_macs._length = ddsrt_toBE4u(++index);
    }
  }
  CRYPTO_OBJECT_RELEASE(pp_key_material);
  CRYPTO_OBJECT_RELEASE(session);
  return result;
}

static DDS_Security_boolean
encode_datawriter_submessage_sign (
    dds_security_crypto_key_factory *factory,
    DDS_Security_OctetSeq *encoded_submsg,
    const DDS_Security_DatareaderCryptoHandleSeq *reader_crypto_list,
    int32_t *index,
    DDS_Security_SecurityException *ex
)
{
  DDS_Security_boolean result;
  struct submsg_header *prefix = (struct submsg_header *)encoded_submsg->_buffer;
  struct submsg_header *body = (struct submsg_header *)(((unsigned char *)prefix) + prefix->length + sizeof(struct submsg_header));
  struct submsg_header *postfix = (struct submsg_header *)(((unsigned char *)body) + body->length + sizeof(struct submsg_header));

  result = add_crypto_reader_specific_mac(factory, encoded_submsg, reader_crypto_list->_buffer[*index], postfix, ex);
  if (result)
    (*index)++;

  return result;
}

static DDS_Security_boolean
encode_datawriter_submessage_encrypt (
    dds_security_crypto_key_factory *factory,
    DDS_Security_OctetSeq *encoded_submsg,
    const DDS_Security_OctetSeq *plain_submsg,
    const DDS_Security_DatawriterCryptoHandle writer_crypto,
    const DDS_Security_DatareaderCryptoHandleSeq *reader_crypto_list,
    int32_t *index,
    DDS_Security_SecurityException *ex
)
{
  DDS_Security_DatareaderCryptoHandle reader_crypto = 0;
  DDS_Security_OctetSeq data;
  session_key_material *session = NULL;
  DDS_Security_ProtectionKind protection_kind;
  unsigned char flags;
  unsigned char *contents;
  crypto_hmac_t hmac;
  uint32_t payload_len;
  size_t size;
  struct submsg_header *prefix;
  struct crypto_header *header;
  struct submsg_header *postfix;
  struct crypto_footer *footer;
  uint32_t transform_kind, transform_id;
  DDS_Security_boolean result = false;

  if (reader_crypto_list->_length > 0)
    reader_crypto = reader_crypto_list->_buffer[0];

  if (!crypto_factory_get_writer_key_material(factory, writer_crypto, reader_crypto, false, &session, &protection_kind, ex))
    goto enc_dw_submsg_fail_keymat;

  /* Determine the size of the buffer
        * When no encryption
        * - submsg header SEC_PREFIX
        * - crypto header
        * - size of the plain_submsg
        * - submsg header SEC_POSTFIX
        * - crypto footer based on the size of the receiving_datareader_crypto_list
        * When encryption is required:
        * - submsg header SEC_PREFIX
        * - crypto header
        * - submsg header SEC_BODY
        * - estimated size of the encoded submessage (size of the plain_submsg + some extra for possible padding
        * - submsg header SEC_POSTFIX
        * - crypto footer based on the size of the receiving_datareader_crypto_list
        */

  size = 2 * sizeof(struct submsg_header) + sizeof(struct crypto_header) + sizeof(struct crypto_footer) + ALIGN4(plain_submsg->_length);
  size += reader_crypto_list->_length * sizeof(struct receiver_specific_mac);
  /* assure that the buffer contains enough memory to accommodate the encrypted payload */
  if (is_encryption_required(session->master_key_material->transformation_kind))
    size += sizeof(struct submsg_header) + sizeof(uint32_t) + CRYPTO_ENCRYPTION_MAX_PADDING;

  flags = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) ? 0x01 : 0x00;

  /* allocate a buffer to store the encoded submessage */
  data._buffer = ddsrt_malloc(size);
  data._length = 0;
  data._maximum = (uint32_t)size;

  /* Set the SEC_PREFIX and associated CryptoHeader */
  prefix = add_submessage(&data, SMID_SEC_PREFIX_KIND, flags, sizeof(struct crypto_header));
  header = (struct crypto_header *)(prefix + 1);
  contents = (unsigned char *)(header + 1);

  /* update sessionKey when needed */
  if (!crypto_session_key_material_update(session, plain_submsg->_length, ex))
    goto enc_dw_submsg_fail;

  /* increment init_vector_suffix */
  session->init_vector_suffix++;

  transform_kind = session->master_key_material->transformation_kind;
  transform_id = session->master_key_material->sender_key_id;

  set_crypto_header(header, transform_kind, transform_id, session->id, session->init_vector_suffix);

  if (is_encryption_required(transform_kind))
  {
    struct encrypted_data *encrypted;
    size_t submsg_len = plain_submsg->_length + sizeof(uint32_t);
    if (submsg_len > UINT16_MAX)
      goto enc_dw_submsg_fail;

    /* add SEC_BODY submessage */
    struct submsg_header *body = add_submessage(&data, SMID_SEC_BODY_KIND, flags, submsg_len);
    encrypted = (struct encrypted_data *)(body + 1);
    contents = encrypted->data;

    /* encrypt submessage */
    if (!crypto_cipher_encrypt_data(&session->key, session->key_size, header->session_id, plain_submsg->_buffer, plain_submsg->_length, NULL, 0, contents, &payload_len, &hmac, ex))
      goto enc_dw_submsg_fail;

    /* adjust the length of the body submessage when needed */
    encrypted->length = ddsrt_toBE4u(payload_len);
    if (payload_len > plain_submsg->_length)
    {
      size_t inc = payload_len - plain_submsg->_length;
      body->length = (uint16_t)(body->length + inc);
      data._length += (uint32_t)inc;
    }
  }
  else if (is_authentication_required(transform_kind))
  {
    /* the transformation_kind indicates only indicates authentication the determine HMAC */
    if (!crypto_cipher_encrypt_data(&session->key, session->key_size, header->session_id, NULL, 0, plain_submsg->_buffer, plain_submsg->_length, NULL, NULL, &hmac, ex))
      goto enc_dw_submsg_fail;

    /* copy submessage */
    memcpy(contents, plain_submsg->_buffer, plain_submsg->_length);
    payload_len = plain_submsg->_length;
    data._length += payload_len;
  }
  else
  {
    goto enc_dw_submsg_fail;
  }

  postfix = add_submessage(&data, SMID_SEC_POSTFIX_KIND, flags, CRYPTO_FOOTER_BASIC_SIZE);
  footer = (struct crypto_footer *)(postfix + 1);

  /* Set initial SEC_POSTFIX and CryptoFooter containing the common_mac
    * Note that the length of the postfix may increase when reader specific macs are added */
  memcpy(footer->common_mac.data, hmac.data, CRYPTO_HMAC_SIZE);
  footer->receiver_specific_macs._length = 0;

  *encoded_submsg = data;
  if (!has_origin_authentication(protection_kind))
    *index = (int32_t) reader_crypto_list->_length;
  else
  {
    if (reader_crypto_list->_length != 0)
    {
      if (!add_crypto_reader_specific_mac(factory, encoded_submsg, reader_crypto_list->_buffer[0], postfix, ex))
        goto enc_dw_submsg_fail;
      (*index)++;
    }
  }

  result = true;

enc_dw_submsg_fail:
  CRYPTO_OBJECT_RELEASE(session);
  if (!result)
  {
    ddsrt_free(data._buffer);
    encoded_submsg->_buffer = NULL;
    encoded_submsg->_length = 0;
    encoded_submsg->_maximum = 0;
  }
enc_dw_submsg_fail_keymat:
  return result;
}

static DDS_Security_boolean
encode_datawriter_submessage(
    dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *encoded_submsg,
    const DDS_Security_OctetSeq *plain_submsg,
    const DDS_Security_DatawriterCryptoHandle writer_crypto,
    const DDS_Security_DatareaderCryptoHandleSeq *reader_crypto_list,
    int32_t *index,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_transform_impl *impl = (dds_security_crypto_transform_impl *)instance;
  dds_security_crypto_key_factory *factory;
  DDS_Security_boolean result = false;

  /* check arguments */
  if (!instance || !encoded_submsg || (writer_crypto == 0) || !reader_crypto_list ||
      (reader_crypto_list->_length == 0) || (reader_crypto_list->_length > INT32_MAX) ||
      !index || ((*index) >= (int32_t)reader_crypto_list->_length))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "encode_datawriter_submessage: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    goto enc_dw_submsg_inv_args;
  }

  if (*index == 0)
  {
    if (!plain_submsg || (plain_submsg->_length == 0))
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "encode_datawriter_submessage: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
      goto enc_dw_submsg_inv_args;
    }
  }
  else
  {
    if (encoded_submsg->_length == 0)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "encode_datawriter_submessage: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
      goto enc_dw_submsg_inv_args;
    }
  }

  factory = cryptography_get_crypto_key_factory(impl->crypto);

  if (*index == 0)
  {
    /* When the index is 0 then retrieve the key material of the writer */
    result = encode_datawriter_submessage_encrypt (factory, encoded_submsg, plain_submsg,
        writer_crypto, reader_crypto_list, index, ex);
  }
  else
  {
    /* When the index is not 0 then add a signature for the specific reader */
    result = encode_datawriter_submessage_sign (factory, encoded_submsg, reader_crypto_list, index, ex);
  }

enc_dw_submsg_inv_args:
  return result;
}

static bool
add_writer_specific_mac(
    dds_security_crypto_key_factory *factory,
    DDS_Security_OctetSeq *data,
    DDS_Security_DatawriterCryptoHandle writer_crypto,
    struct submsg_header *postfix,
    DDS_Security_SecurityException *ex)
{
  bool result = true;
  master_key_material *key_material = NULL;
  session_key_material *session = NULL;
  DDS_Security_ProtectionKind protection_kind;

  if (!crypto_factory_get_remote_writer_sign_key_material(factory, writer_crypto, &key_material, &session, &protection_kind, ex))
    return false;

  result = add_reader_specific_mac(data, postfix, key_material, session, protection_kind, ex);

  CRYPTO_OBJECT_RELEASE(session);
  CRYPTO_OBJECT_RELEASE(key_material);

  return result;
}

static DDS_Security_boolean
encode_datareader_submessage(
    dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *encoded_submsg,
    const DDS_Security_OctetSeq *plain_submsg,
    const DDS_Security_DatareaderCryptoHandle reader_crypto,
    const DDS_Security_DatawriterCryptoHandleSeq *writer_crypto_list,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_transform_impl *impl = (dds_security_crypto_transform_impl *)instance;
  dds_security_crypto_key_factory *factory;
  DDS_Security_DatawriterCryptoHandle writer_crypto = 0;
  DDS_Security_OctetSeq data;
  struct submsg_header *prefix;
  struct crypto_header *header;
  struct submsg_header *postfix;
  struct crypto_footer *footer;
  session_key_material *session = NULL;
  DDS_Security_ProtectionKind protection_kind;
  unsigned char flags;
  unsigned char *contents;
  crypto_hmac_t hmac;
  uint32_t payload_len;
  size_t size;
  DDS_Security_boolean result = false;
  uint32_t transform_kind, transform_id;

  /* check arguments */
  if (!instance || !encoded_submsg || (reader_crypto == 0) ||
      !plain_submsg || (plain_submsg->_length == 0) ||
      !writer_crypto_list || (writer_crypto_list->_length == 0))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "encode_datawriter_submessage: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    goto enc_dr_submsg_inv_args;
  }

  factory = cryptography_get_crypto_key_factory(impl->crypto);

  if (writer_crypto_list->_length > 0)
    writer_crypto = writer_crypto_list->_buffer[0];

  if (!crypto_factory_get_reader_key_material(factory, reader_crypto, writer_crypto, &session, &protection_kind, ex))
    goto enc_dr_submsg_fail_keymat;

  /* Determine the size of the buffer
     * When no encryption
     * - submsg header SEC_PREFIX
     * - crypto header
     * - size of the plain_submsg
     * - submsg header SEC_POSTFIX
     * - crypto footer based on the size of the receiving_datareader_crypto_list
     * When encryption is required:
     * - submsg header SEC_PREFIX
     * - crypto header
     * - submsg header SEC_BODY
     * - estimated size of the encoded submessage (size of the plain_submsg + some extra for possible padding
     * - submsg header SEC_POSTFIX
     * - crypto footer based on the size of the receiving_datareader_crypto_list
     */

  size = 2 * sizeof(struct submsg_header) + sizeof(struct crypto_header) + sizeof(struct crypto_footer) + ALIGN4(plain_submsg->_length);
  size += writer_crypto_list->_length * sizeof(struct receiver_specific_mac);
  /* assure that the buffer contains enough memory to accommodate the encrypted payload */
  if (is_encryption_required(session->master_key_material->transformation_kind))
    size += sizeof(struct submsg_header) + sizeof(uint32_t) + CRYPTO_ENCRYPTION_MAX_PADDING;

  flags = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) ? 0x01 : 0x00;

  /* allocate a buffer to store the encoded submessage */
  data._buffer = ddsrt_malloc(size);
  data._length = 0;
  data._maximum = (uint32_t)size;

  /* Set the SEC_PREFIX and associated CryptoHeader */

  prefix = add_submessage(&data, SMID_SEC_PREFIX_KIND, flags, sizeof(struct crypto_header));
  header = (struct crypto_header *)(prefix + 1);
  contents = (unsigned char *)(header + 1);

  /* update sessionKey when needed */
  if (!crypto_session_key_material_update(session, plain_submsg->_length, ex))
    goto enc_dr_submsg_fail;

  /* increment init_vector_suffix */
  session->init_vector_suffix++;

  transform_kind = session->master_key_material->transformation_kind;
  transform_id = session->master_key_material->sender_key_id;

  set_crypto_header(header, transform_kind, transform_id, session->id, session->init_vector_suffix);

  if (is_encryption_required(transform_kind))
  {
    struct encrypted_data *encrypted;
    size_t submsg_len = plain_submsg->_length + sizeof (uint32_t);
    if (submsg_len > UINT16_MAX)
      goto enc_dr_submsg_fail;

    /* add SEC_BODY submessage */
    struct submsg_header *body = add_submessage(&data, SMID_SEC_BODY_KIND, flags, submsg_len);
    encrypted = (struct encrypted_data *)(body + 1);
    contents = encrypted->data;

    /* encrypt submessage */
    if (!crypto_cipher_encrypt_data(&session->key, session->key_size, header->session_id, plain_submsg->_buffer, plain_submsg->_length, NULL, 0, contents, &payload_len, &hmac, ex))
      goto enc_dr_submsg_fail;

    /* adjust the length of the body submessage when needed */
    encrypted->length = ddsrt_toBE4u(payload_len);
    if (payload_len > plain_submsg->_length)
    {
      size_t inc = payload_len - plain_submsg->_length;
      body->length = (uint16_t)(body->length + inc);
      data._length += (uint32_t)inc;
    }
  }
  else if (is_authentication_required(transform_kind))
  {
    /* the transformation_kind indicates only indicates authentication the determine HMAC */
    if (!crypto_cipher_encrypt_data(&session->key, session->key_size, header->session_id, NULL, 0, plain_submsg->_buffer, plain_submsg->_length, NULL, NULL, &hmac, ex))
      goto enc_dr_submsg_fail;

    /* copy submessage */
    memcpy(contents, plain_submsg->_buffer, plain_submsg->_length);
    payload_len = plain_submsg->_length;
    data._length += payload_len;
  }
  else
  {
    goto enc_dr_submsg_fail;
  }

  postfix = add_submessage(&data, SMID_SEC_POSTFIX_KIND, flags, CRYPTO_FOOTER_BASIC_SIZE);
  footer = (struct crypto_footer *)(postfix + 1);

  /* Set initial SEC_POSTFIX and CryptoFooter containing the common_mac
    * Note that the length of the postfix may increase when writer specific macs are added */
  memcpy(footer->common_mac.data, hmac.data, CRYPTO_HMAC_SIZE);
  footer->receiver_specific_macs._length = 0;

  if (has_origin_authentication(protection_kind))
  {
    for (uint32_t i = 0; i < writer_crypto_list->_length; i++)
    {
      if (!add_writer_specific_mac(factory, &data, writer_crypto_list->_buffer[i], postfix, ex))
        goto enc_dr_submsg_fail;
    }
  }
  *encoded_submsg = data;
  result = true;

enc_dr_submsg_fail:
  CRYPTO_OBJECT_RELEASE(session);
  if (!result)
  {
    ddsrt_free(data._buffer);
    encoded_submsg->_buffer = NULL;
    encoded_submsg->_maximum = 0;
    encoded_submsg->_length = 0;
  }
enc_dr_submsg_fail_keymat:
enc_dr_submsg_inv_args:
  return result;
}

static bool
check_reader_specific_mac(
    dds_security_crypto_key_factory *factory,
    struct crypto_header *header,
    struct crypto_footer *footer,
    CryptoObjectKind_t kind,
    DDS_Security_Handle rmt_handle,
    const char *context,
    DDS_Security_SecurityException *ex)
{
  bool result = false;
  master_key_material *keymat = NULL;
  uint32_t index;
  uint32_t session_id;
  crypto_session_key_t key;
  crypto_hmac_t *href = NULL;
  crypto_hmac_t hmac;

  if (footer->receiver_specific_macs._length == 0)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_RECEIVER_SIGN_CODE, 0,
                "%s: message does not contain a receiver specific mac", context);
    return false;
  }

  if (!crypto_factory_get_specific_keymat(factory, kind, rmt_handle, &footer->receiver_specific_macs._buffer[0], footer->receiver_specific_macs._length, &index, &keymat))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_RECEIVER_SIGN_CODE, 0,
            "%s: message does not contain a known receiver specific key", context);
    goto check_failed;
  }

  href = &footer->receiver_specific_macs._buffer[index].receiver_mac;
  if (!href)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_RECEIVER_SIGN_CODE, 0,
        "%s: message does not contain receiver specific mac", context);
    goto check_failed;
  }

  session_id = CRYPTO_TRANSFORM_ID(header->session_id);
  if (!crypto_calculate_receiver_specific_key(&key, session_id, keymat->master_salt, keymat->master_receiver_specific_key, keymat->transformation_kind, ex))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_RECEIVER_SIGN_CODE, 0,
        "%s: failed to calculate receiver specific session key", context);
    goto check_failed;
  }

  if (!crypto_cipher_encrypt_data(&key, crypto_get_key_size(keymat->transformation_kind), header->session_id, NULL, 0, footer->common_mac.data, CRYPTO_HMAC_SIZE, NULL, NULL, &hmac, ex))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_RECEIVER_SIGN_CODE, 0,
        "%s: failed to calculate receiver specific hmac", context);
    goto check_failed;
  }

  if (memcmp(hmac.data, href->data, CRYPTO_HMAC_SIZE) != 0)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_RECEIVER_SIGN_CODE, 0,
        "%s: message does not contain a valid receiver specific mac", context);
    goto check_failed;
  }

  result = true;

check_failed:
  CRYPTO_OBJECT_RELEASE(keymat);
  return result;
}

static DDS_Security_boolean encode_rtps_message_sign (
    dds_security_crypto_key_factory *factory,
    DDS_Security_OctetSeq *encoded_rtps_message,
    const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
    int32_t *receiving_participant_crypto_list_index,
    DDS_Security_ParticipantCryptoHandle remote_id,
    DDS_Security_SecurityException *ex
)
{
  DDS_Security_boolean result;
  struct submsg_header *prefix = (struct submsg_header *)(encoded_rtps_message->_buffer + RTPS_HEADER_SIZE);
  struct crypto_header *header = (struct crypto_header *)(prefix + 1);
  struct submsg_header *submessage_header = (struct submsg_header *)(header + 1);
  struct submsg_header *postfix = (struct submsg_header *)(((unsigned char *)submessage_header) + submessage_header->length + sizeof(struct submsg_header));

  if (submessage_header->id == SMID_SRTPS_INFO_SRC_KIND) /* not encrypted */
  {
    /* skip INFO_SRC_HDR to RTPS submessage header */
    unsigned char *ptr = ((unsigned char *)submessage_header + sizeof(struct submsg_header) + submessage_header->length);
    size_t remaining = (size_t) (encoded_rtps_message->_buffer + encoded_rtps_message->_length - ptr);
    /* There may be multiple RTPS submessages until postfix */
    while (remaining - sizeof(struct submsg_header) > 0)
    {
      submessage_header = (struct submsg_header *)ptr;
      size_t length_to_postfix = submessage_header->length + sizeof(struct submsg_header);
      postfix = (struct submsg_header *)(((unsigned char *)submessage_header) + length_to_postfix);
      if (postfix->id == SMID_SRTPS_POSTFIX_KIND)
        break;
      remaining -= length_to_postfix;
      ptr += length_to_postfix;
    }
  }

  result = add_receiver_specific_mac(factory, encoded_rtps_message, sending_participant_crypto, remote_id, postfix, ex);
  if (result)
    (*receiving_participant_crypto_list_index)++;

  return result;
}

static DDS_Security_boolean encode_rtps_message_encrypt (
    dds_security_crypto_key_factory *factory,
    DDS_Security_OctetSeq *encoded_rtps_message,
    const DDS_Security_OctetSeq *plain_rtps_message,
    const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
    const DDS_Security_ParticipantCryptoHandleSeq *receiving_participant_crypto_list,
    int32_t *receiving_participant_crypto_list_index,
    DDS_Security_ParticipantCryptoHandle remote_id,
    DDS_Security_SecurityException *ex
)
{
  session_key_material *session = NULL;
  DDS_Security_ProtectionKind protection_kind;
  DDS_Security_OctetSeq data;
  DDS_Security_OctetSeq secure_body_plain;
  struct submsg_header *prefix;
  struct crypto_header *header;
  struct submsg_header *postfix;
  struct submsg_header *body;
  struct crypto_footer *footer;
  DDS_Security_boolean result = false;
  unsigned char *contents;
  crypto_hmac_t hmac;
  uint32_t payload_len;
  size_t size;
  uint32_t transform_kind, transform_id;
  unsigned char flags = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN) ? 0x01 : 0x00;
  unsigned char *rtps_body = plain_rtps_message->_buffer + RTPS_HEADER_SIZE;
  unsigned rtps_body_size = plain_rtps_message->_length - RTPS_HEADER_SIZE;
  unsigned secure_body_plain_size = rtps_body_size + INFO_SRC_SIZE;

  /* get local crypto and session*/
  if (!crypto_factory_get_local_participant_data_key_material(factory, sending_participant_crypto, &session, &protection_kind, ex))
    goto enc_rtps_inv_keymat;

  secure_body_plain._buffer = ddsrt_malloc(secure_body_plain_size);
  secure_body_plain._maximum = secure_body_plain_size;
  secure_body_plain._length = 0;

  /* info_src and body */
  if (!add_info_src(&secure_body_plain, plain_rtps_message->_buffer, flags))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "encode_rtps_message: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    goto enc_rtps_fail;
  }
  memcpy(secure_body_plain._buffer + secure_body_plain._length, rtps_body, rtps_body_size); /* rtps message body */
  secure_body_plain._length += rtps_body_size;

  /* Determine the size of the buffer
        * When no encryption
        * - rtps header
        * - submsg header SEC_PREFIX
        * - crypto header
        * - INFO_SRC (24)
        * - size of the plain_rtps body
        * - submsg header SEC_POSTFIX
        * - crypto footer based on the size of the receiving_participant_crypto_list
        * When encryption is required:
        * - rtps header
        * - submsg header SEC_PREFIX
        * - crypto header
        * - submsg header SEC_BODY
        * - INFO_SRC (24)
        * - estimated size of the encoded submessage (size of the plain_rtps_message + some extra for possible padding
        * - submsg header SEC_POSTFIX
        * - crypto footer based on the size of the receiving_participant_crypto_list
        */

  size = RTPS_HEADER_SIZE; /* RTPS Header */
  size += 2 * sizeof(struct submsg_header) + sizeof(struct crypto_header) + sizeof(struct crypto_footer) + ALIGN4(plain_rtps_message->_length);
  size += receiving_participant_crypto_list->_length * sizeof(struct receiver_specific_mac);
  size += sizeof(struct submsg_header) + RTPS_HEADER_SIZE; /* INFO_SRC */

  if (is_encryption_required(session->master_key_material->transformation_kind))
    size += sizeof(struct submsg_header) + sizeof(uint32_t);

  /* allocate a buffer to store the encoded message */
  data._buffer = ddsrt_malloc(size);
  data._length = 0;
  data._maximum = (uint32_t)size;

  /* Add RTPS header */
  memcpy(data._buffer, plain_rtps_message->_buffer, RTPS_HEADER_SIZE);
  data._length += RTPS_HEADER_SIZE;

  /* Set the SEC_PREFIX and associated CryptoHeader */
  prefix = add_submessage(&data, SMID_SRTPS_PREFIX_KIND, flags, sizeof(struct crypto_header));
  header = (struct crypto_header *)(prefix + 1);
  contents = (unsigned char *)(header + 1);

  /* update sessionKey when needed */
  if (!crypto_session_key_material_update(session, secure_body_plain_size, ex))
    goto enc_rtps_fail_data;

  /* increment init_vector_suffix */
  session->init_vector_suffix++;

  transform_kind = session->master_key_material->transformation_kind;
  transform_id = session->master_key_material->sender_key_id;
  set_crypto_header(header, transform_kind, transform_id, session->id, session->init_vector_suffix);

  if (is_encryption_required(transform_kind))
  {
    struct encrypted_data *encrypted;
    size_t submsg_len = secure_body_plain_size + sizeof (uint32_t);
    if (submsg_len > UINT16_MAX)
      goto enc_rtps_fail_data;

    /* add SEC_BODY submessage */
    body = add_submessage(&data, SMID_SEC_BODY_KIND, flags, submsg_len);
    encrypted = (struct encrypted_data *)(body + 1);
    contents = encrypted->data;

    /* encrypt message */
    /* FIXME: improve performance by not copying plain_rtps_message to a new buffer (crypto_cipher_encrypt_data should allow encrypting parts of a message) */
    if (!crypto_cipher_encrypt_data(&session->key, session->key_size, header->session_id, secure_body_plain._buffer, secure_body_plain_size, NULL, 0, contents, &payload_len, &hmac, ex))
      goto enc_rtps_fail_data;

    encrypted->length = ddsrt_toBE4u(payload_len);
    if (payload_len + sizeof(encrypted->length) > secure_body_plain_size)
    {
      size_t inc = payload_len + sizeof(encrypted->length) - secure_body_plain_size;
      body->length = (uint16_t)(body->length + inc);
      data._length += (uint32_t)inc;
    }
  }
  else if (is_authentication_required(transform_kind))
  {
    /* the transformation_kind indicates only indicates authentication the determine HMAC */
    if (!crypto_cipher_encrypt_data(&session->key, session->key_size, header->session_id, NULL, 0, secure_body_plain._buffer, secure_body_plain_size, NULL, NULL, &hmac, ex))
      goto enc_rtps_fail_data;

    /* copy submessage */
    memcpy(contents, secure_body_plain._buffer, secure_body_plain_size);
    payload_len = secure_body_plain_size;
    data._length += payload_len;
  }
  else
  {
    goto enc_rtps_fail_data;
  }

  postfix = add_submessage(&data, SMID_SRTPS_POSTFIX_KIND, flags, CRYPTO_FOOTER_BASIC_SIZE);
  footer = (struct crypto_footer *)(postfix + 1);

  /* Set initial SEC_POSTFIX and CryptoFooter containing the common_mac
    * Note that the length of the postfix may increase when reader specific macs are added */
  memcpy(footer->common_mac.data, hmac.data, CRYPTO_HMAC_SIZE);
  footer->receiver_specific_macs._length = 0;
  *encoded_rtps_message = data;
  if (has_origin_authentication(protection_kind))
  {
    if (receiving_participant_crypto_list->_length != 0)
    {
      if (!add_receiver_specific_mac(factory, encoded_rtps_message, sending_participant_crypto, remote_id, postfix, ex))
        goto enc_rtps_fail_data;
      (*receiving_participant_crypto_list_index)++;
    }
  }
  else
  {
    *receiving_participant_crypto_list_index = (int32_t) receiving_participant_crypto_list->_length;
  }
  result = true;

enc_rtps_fail_data:
  if (!result)
  {
    ddsrt_free(data._buffer);
    encoded_rtps_message->_buffer = NULL;
    encoded_rtps_message->_length = 0;
    encoded_rtps_message->_maximum = 0;
  }
enc_rtps_fail:
  CRYPTO_OBJECT_RELEASE(session);
  ddsrt_free(secure_body_plain._buffer);
enc_rtps_inv_keymat:
  return result;
}

static DDS_Security_boolean
encode_rtps_message(dds_security_crypto_transform *instance,
                    DDS_Security_OctetSeq *encoded_rtps_message,
                    const DDS_Security_OctetSeq *plain_rtps_message,
                    const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
                    const DDS_Security_ParticipantCryptoHandleSeq *receiving_participant_crypto_list,
                    int32_t *receiving_participant_crypto_list_index,
                    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_transform_impl *impl = (dds_security_crypto_transform_impl *)instance;
  dds_security_crypto_key_factory *factory;
  DDS_Security_ParticipantCryptoHandle remote_id;
  DDS_Security_boolean result = false;

  /* check arguments */
  if (!instance || !encoded_rtps_message || sending_participant_crypto == 0 || !receiving_participant_crypto_list ||
      !receiving_participant_crypto_list_index || receiving_participant_crypto_list->_length == 0 ||
      (*receiving_participant_crypto_list_index) > (int32_t)receiving_participant_crypto_list->_length ||
      receiving_participant_crypto_list->_length > INT32_MAX)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "encode_rtps_message: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    goto enc_rtps_inv_arg;
  }

  if (*receiving_participant_crypto_list_index == 0)
  {
    if (!plain_rtps_message || plain_rtps_message->_length == 0 || !plain_rtps_message->_buffer)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "encode_rtps_message: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
      goto enc_rtps_inv_arg;
    }
  }
  else
  {
    if (encoded_rtps_message->_length == 0)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "encode_rtps_message: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
      goto enc_rtps_inv_arg;
    }
  }

  factory = cryptography_get_crypto_key_factory(impl->crypto);

  /* get remote participant handle */
  remote_id = receiving_participant_crypto_list->_buffer[*receiving_participant_crypto_list_index];

  /* When the receiving_participant_crypto_list_index is 0 then retrieve the key material of the writer */
  if (*receiving_participant_crypto_list_index == 0)
  {
    result = encode_rtps_message_encrypt (factory, encoded_rtps_message, plain_rtps_message, sending_participant_crypto,
        receiving_participant_crypto_list, receiving_participant_crypto_list_index, remote_id, ex);
  }
  else
  {
    /* When the receiving_participant_crypto_list_index is not 0 then add a signature for the specific reader */
    result = encode_rtps_message_sign (factory, encoded_rtps_message, sending_participant_crypto,
        receiving_participant_crypto_list_index, remote_id, ex);
  }

enc_rtps_inv_arg:
  return result;
}



static DDS_Security_boolean
decode_rtps_message(dds_security_crypto_transform *instance,
                    DDS_Security_OctetSeq *plain_buffer,
                    const DDS_Security_OctetSeq *encoded_buffer,
                    const DDS_Security_ParticipantCryptoHandle receiving_participant_crypto,
                    const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
                    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_transform_impl *impl = (dds_security_crypto_transform_impl *)instance;
  dds_security_crypto_key_factory *factory;
  uint32_t transform_kind;
  remote_session_info remote_session;
  DDS_Security_OctetSeq rtps_header;
  struct submsg_header prefix;
  struct crypto_header header;
  struct submsg_header postfix;
  struct submsg_header body;
  struct crypto_footer *footer;
  struct crypto_contents_ref contents = {0, NULL};
  unsigned char *decoded_body;
  uint32_t decoded_body_size;
  static const char *context = "decode_rtps_message";
  participant_key_material *pp_key_material;
  master_key_material *remote_key_material;
  DDS_Security_ProtectionKind remote_protection_kind;
  bool result = false;

  /* FIXME: when decoding a message the message is split in several parts (header, body, footer, etc) and for this
   * memory is allocated which is probably not necessary. Performance should be improved by removing these allocations
   * and use pointer to the data instead. */

  /* check arguments */
  if (!instance || !encoded_buffer || sending_participant_crypto == 0 || receiving_participant_crypto == 0 ||
      encoded_buffer->_length == 0 || !encoded_buffer->_buffer || !plain_buffer)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "decode_rtps_message: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    goto fail_invalid_input;
  }

  factory = cryptography_get_crypto_key_factory(impl->crypto);

  memset(&rtps_header, 0, sizeof(DDS_Security_OctetSeq));

  /* split the encoded submessage in the corresponding parts */
  if (!split_encoded_rtps_message(encoded_buffer, &rtps_header, &prefix, &body, &postfix, &header, &contents, &footer))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "decode_rtps_message: invalid message");
    goto fail_invalid_input;
  }

  transform_kind = CRYPTO_TRANSFORM_KIND(header.transform_identifier.transformation_kind);

  /* Retrieve key material from sending_participant_crypto and receiving_participant_crypto from factory */
  if (!crypto_factory_get_participant_crypto_tokens(factory, receiving_participant_crypto, sending_participant_crypto, &pp_key_material, &remote_key_material, &remote_protection_kind, ex))
    goto fail_tokens;
  if (remote_key_material == NULL)
    goto fail_remote_keys_not_ready;

  if (has_origin_authentication(remote_protection_kind))
  { /* default governance value */
    if (!check_reader_specific_mac(factory, &header, footer, CRYPTO_OBJECT_KIND_REMOTE_CRYPTO, sending_participant_crypto, context, ex))
      goto fail_reader_mac;
  }

  /* calculate the session key */
  decoded_body = DDS_Security_OctetSeq_allocbuf(contents._length);
  if (!initialize_remote_session_info(&remote_session, &header, remote_key_material->master_salt,
        remote_key_material->master_sender_key, remote_key_material->transformation_kind, ex))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "decode_rtps_message: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    goto fail_decrypt;
  }

  if (is_encryption_required(transform_kind))
  {
    if (!is_encryption_expected(remote_protection_kind))
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "decode_rtps_submessage: message is encrypted, which is unexpected");
      goto fail_decrypt;
    }

    /* When the CryptoHeader indicates that encryption is performed then decrypt the submessage body */
    /* check if the body is a SEC_BODY submessage */
    if (body.id != SMID_SEC_BODY_KIND)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "decode_rtps_submessage: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
      goto fail_decrypt;
    }

    if (!crypto_cipher_decrypt_data(&remote_session, header.session_id, contents._data, contents._length, NULL, 0,
                                    decoded_body, &decoded_body_size, &footer->common_mac, ex))
    {
      goto fail_decrypt;
    }
  }
  else if (is_authentication_required(transform_kind))
  {
    if (!is_authentication_expected(remote_protection_kind))
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "decode_rtps_message: message is signed, which is unexpected");
      goto fail_decrypt;
    }
    /* When the CryptoHeader indicates that authentication is performed then calculate the HMAC */
    if (!crypto_cipher_decrypt_data(&remote_session, header.session_id, NULL, 0, contents._data, contents._length, NULL, 0, &footer->common_mac, ex))
      goto fail_decrypt;
    decoded_body_size = contents._length;
    memcpy(decoded_body, contents._data, contents._length);
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_CODE, 0,
        "decode_rtps_message: " DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_MESSAGE);
    goto fail_decrypt;
  }

  plain_buffer->_buffer = DDS_Security_OctetSeq_allocbuf(decoded_body_size - 4); /* INFO_SRC removed, "RTPS" prefix added */
  plain_buffer->_length = plain_buffer->_maximum = decoded_body_size - 4;        /* INFO_SRC removed, "RTPS" prefix added */
  memcpy(plain_buffer->_buffer, "RTPS", 4);                                      /* ADD RTPS */
  memcpy(plain_buffer->_buffer + 4, decoded_body + 8, decoded_body_size - 8);    /* remove INFO_SRC */
  result = true;

fail_decrypt:
  ddsrt_free(decoded_body);
fail_remote_keys_not_ready:
fail_reader_mac:
  CRYPTO_OBJECT_RELEASE(pp_key_material);
fail_tokens:
  ddsrt_free(footer);
fail_invalid_input:
  return result;
}

static DDS_Security_boolean
preprocess_secure_submsg(
    dds_security_crypto_transform *instance,
    DDS_Security_DatawriterCryptoHandle *datawriter_crypto,
    DDS_Security_DatareaderCryptoHandle *datareader_crypto,
    DDS_Security_SecureSubmessageCategory_t *secure_submessage_category,
    const DDS_Security_OctetSeq *encoded_rtps_submessage,
    const DDS_Security_ParticipantCryptoHandle receiving_participant_crypto,
    const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_transform_impl *impl = (dds_security_crypto_transform_impl *)instance;
  dds_security_crypto_key_factory *factory;
  DDS_Security_Handle remote_handle;
  DDS_Security_Handle local_handle;
  DDS_Security_boolean result;
  struct submsg_header submsg;
  struct crypto_header cheader;
  uint32_t transform_kind, key_id;
  unsigned char *pdata;
  uint32_t remain;

  if (!instance || !datawriter_crypto || !datareader_crypto || sending_participant_crypto == 0 ||
      !secure_submessage_category || !encoded_rtps_submessage || encoded_rtps_submessage->_length == 0)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "preprocess_secure_submsg: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  factory = cryptography_get_crypto_key_factory(impl->crypto);

  pdata = encoded_rtps_submessage->_buffer;
  remain = encoded_rtps_submessage->_length;

  if (!read_submsg_header(&submsg, &pdata, &remain))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "preprocess_secure_submsg: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  if (submsg.id != SMID_SEC_PREFIX_KIND)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "preprocess_secure_submsg: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  if (!read_crypto_header(&cheader, &pdata, &remain))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "preprocess_secure_submsg: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  transform_kind = CRYPTO_TRANSFORM_KIND(cheader.transform_identifier.transformation_kind);
  key_id = CRYPTO_TRANSFORM_ID(cheader.transform_identifier.transformation_key_id);

  if (!transform_kind_valid(transform_kind))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "preprocess_secure_submsg: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  /* Lookup the end point information associated with the transform id */
  result = crypto_factory_get_endpoint_relation(
      factory, receiving_participant_crypto, sending_participant_crypto,
      key_id, &remote_handle, &local_handle, secure_submessage_category, ex);

  if (result)
  {
    if (*secure_submessage_category == DDS_SECURITY_DATAWRITER_SUBMESSAGE)
    {
      *datawriter_crypto = (DDS_Security_DatawriterCryptoHandle)remote_handle;
      *datareader_crypto = (DDS_Security_DatareaderCryptoHandle)local_handle;
    }
    else
    {
      *datareader_crypto = (DDS_Security_DatareaderCryptoHandle)remote_handle;
      *datawriter_crypto = (DDS_Security_DatawriterCryptoHandle)local_handle;
    }
  }

  return result;
}

static DDS_Security_boolean
decode_datawriter_submessage(
    dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *plain_submsg,
    const DDS_Security_OctetSeq *encoded_submsg,
    const DDS_Security_DatareaderCryptoHandle reader_crypto,
    const DDS_Security_DatawriterCryptoHandle writer_crypto,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_transform_impl *impl = (dds_security_crypto_transform_impl *)instance;
  static const char *context = "decode_datawriter_submessage";
  dds_security_crypto_key_factory *factory;
  uint32_t transform_kind, transform_id;
  master_key_material *writer_master_key;
  DDS_Security_ProtectionKind protection_kind;
  remote_session_info remote_session;
  struct submsg_header prefix;
  struct crypto_header header;
  struct submsg_header postfix;
  struct submsg_header body;
  struct crypto_footer *footer;
  struct crypto_contents_ref contents;

  /* check arguments */
  if (!instance || writer_crypto == 0 || reader_crypto == 0 || !encoded_submsg ||
      encoded_submsg->_length == 0 || !encoded_submsg->_buffer || !plain_submsg)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "decode_datawriter_submessage: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  memset(plain_submsg, 0, sizeof(*plain_submsg));

  factory = cryptography_get_crypto_key_factory(impl->crypto);

  /* split the encoded submessage in the corresponding parts */
  if (!split_encoded_submessage(encoded_submsg, &prefix, &body, &postfix, &header, &contents, &footer))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "decode_datawriter_submessage: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  transform_kind = CRYPTO_TRANSFORM_KIND(header.transform_identifier.transformation_kind);
  transform_id = CRYPTO_TRANSFORM_ID(header.transform_identifier.transformation_key_id);

  /* Retrieve key material from sending_datawriter_crypto from factory */
  if (!crypto_factory_get_remote_writer_key_material(factory, reader_crypto, writer_crypto, transform_id, &writer_master_key, &protection_kind, NULL, ex))
    goto fail_invalid_arg;

  if (has_origin_authentication(protection_kind))
  {
    if (!check_reader_specific_mac(factory, &header, footer, CRYPTO_OBJECT_KIND_REMOTE_WRITER_CRYPTO, writer_crypto, context, ex))
      goto fail_reader_mac;
  }

  /* calculate the session key */
  if (!initialize_remote_session_info(&remote_session, &header, writer_master_key->master_salt, writer_master_key->master_sender_key, writer_master_key->transformation_kind, ex))
    goto fail_decrypt;

  if (is_encryption_required(transform_kind))
  {
    if (!is_encryption_expected(protection_kind))
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "decode_datawriter_submessage: submessage is encrypted, which is unexpected (%d vs %d)",
          (int)transform_kind, (int)protection_kind);
      goto fail_decrypt;
    }
    /* When the CryptoHeader indicates that encryption is performed then decrypt the submessage body */
    /* check if the body is a SEC_BODY submessage */
    if (body.id != SMID_SEC_BODY_KIND)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "decode_datawriter_submessage: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
      goto fail_decrypt;
    }

    plain_submsg->_buffer = DDS_Security_OctetSeq_allocbuf(contents._length);
    plain_submsg->_length = plain_submsg->_maximum = contents._length;
    if (!crypto_cipher_decrypt_data(&remote_session, header.session_id, contents._data, contents._length, NULL, 0,
                                    plain_submsg->_buffer, &plain_submsg->_length, &footer->common_mac, ex))
    {
      goto fail_decrypt;
    }
  }
  else if (is_authentication_required(transform_kind))
  {
    if (!is_authentication_expected(protection_kind))
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "decode_datawriter_submessage: submessage is signed, which is unexpected");
      goto fail_decrypt;
    }
    assert(transform_id != 0);
    /* When the CryptoHeader indicates that authentication is performed then calculate the HMAC */
    if (!crypto_cipher_decrypt_data(&remote_session, header.session_id, NULL, 0, contents._data, contents._length, NULL, 0, &footer->common_mac, ex))
      goto fail_decrypt;
    plain_submsg->_buffer = DDS_Security_OctetSeq_allocbuf(contents._length);
    plain_submsg->_length = plain_submsg->_maximum = contents._length;
    memcpy(plain_submsg->_buffer, contents._data, contents._length);
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_CODE, 0,
        "decode_serialized_payload: " DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_MESSAGE);
    goto fail_decrypt;
  }

  ddsrt_free(footer);
  CRYPTO_OBJECT_RELEASE(writer_master_key);
  return true;


fail_decrypt:
  DDS_Security_OctetSeq_deinit(plain_submsg);
fail_reader_mac:
  CRYPTO_OBJECT_RELEASE(writer_master_key);
fail_invalid_arg:
  ddsrt_free(footer);
  return false;
}

static DDS_Security_boolean
decode_datareader_submessage(
    dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *plain_submsg,
    const DDS_Security_OctetSeq *encoded_submsg,
    const DDS_Security_DatawriterCryptoHandle writer_crypto,
    const DDS_Security_DatareaderCryptoHandle reader_crypto,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_transform_impl *impl = (dds_security_crypto_transform_impl *)instance;
  static const char *context = "decode_datareader_submessage";
  dds_security_crypto_key_factory *factory;
  uint32_t transform_kind, transform_id;
  master_key_material *reader_master_key;
  DDS_Security_ProtectionKind protection_kind;
  remote_session_info remote_session;
  struct submsg_header prefix;
  struct crypto_header header;
  struct submsg_header postfix;
  struct submsg_header body;
  struct crypto_footer *footer;
  struct crypto_contents_ref contents;

  /* check arguments */
  if (!instance || writer_crypto == 0 || reader_crypto == 0 || !encoded_submsg ||
      encoded_submsg->_length == 0 || !encoded_submsg->_buffer || !plain_submsg)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "decode_datareader_submessage: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  memset(plain_submsg, 0, sizeof(*plain_submsg));

  factory = cryptography_get_crypto_key_factory(impl->crypto);

  /* split the encoded submessage in the corresponding parts */
  if (!split_encoded_submessage(encoded_submsg, &prefix, &body, &postfix, &header, &contents, &footer))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "decode_datareader_submessage: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  transform_kind = CRYPTO_TRANSFORM_KIND(header.transform_identifier.transformation_kind);
  transform_id = CRYPTO_TRANSFORM_ID(header.transform_identifier.transformation_key_id);

  /* Retrieve key material from sending_datareader_crypto from factory */
  if (!crypto_factory_get_remote_reader_key_material(factory, writer_crypto, reader_crypto, transform_id, &reader_master_key, &protection_kind, ex))
    goto fail_invalid_arg;

  if (has_origin_authentication(protection_kind))
  {
    if (!check_reader_specific_mac(factory, &header, footer, CRYPTO_OBJECT_KIND_REMOTE_READER_CRYPTO, reader_crypto, context, ex))
      goto fail_reader_mac;
  }

  /* calculate the session key */
  if (!initialize_remote_session_info(&remote_session, &header, reader_master_key->master_salt, reader_master_key->master_sender_key, reader_master_key->transformation_kind, ex))
    goto fail_decrypt;

  if (is_encryption_required(transform_kind))
  {
    if (!is_encryption_expected(protection_kind))
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "decode_datareader_submessage: submessage is encrypted, which is unexpected");
      goto fail_decrypt;
    }
    /* When the CryptoHeader indicates that encryption is performed then decrypt the submessage body */
    /* check if the body is a SEC_BODY submessage */
    if (body.id != SMID_SEC_BODY_KIND)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "decode_datareader_submessage: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
      goto fail_decrypt;
    }

    plain_submsg->_buffer = DDS_Security_OctetSeq_allocbuf(contents._length);
    plain_submsg->_length = plain_submsg->_maximum = contents._length;
    if (!crypto_cipher_decrypt_data(&remote_session, header.session_id, contents._data, contents._length, NULL, 0,
                                    plain_submsg->_buffer, &plain_submsg->_length, &footer->common_mac, ex))
    {
      goto fail_decrypt;
    }
  }
  else if (is_authentication_required(transform_kind))
  {
    if (!is_authentication_expected(protection_kind))
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "decode_datareader_submessage: submessage is signed, which is unexpected");
      goto fail_decrypt;
    }
    /* When the CryptoHeader indicates that authentication is performed then calculate the HMAC */
    if (!crypto_cipher_decrypt_data(&remote_session, header.session_id, NULL, 0, contents._data, contents._length, NULL, 0, &footer->common_mac, ex))
      goto fail_decrypt;
    plain_submsg->_buffer = DDS_Security_OctetSeq_allocbuf(contents._length);
    plain_submsg->_length = plain_submsg->_maximum = contents._length;
    memcpy(plain_submsg->_buffer, contents._data, contents._length);
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT,
                               DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_CODE, 0,
                               "decode_datareader_submessage: " DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_MESSAGE);
    goto fail_decrypt;
  }
  ddsrt_free(footer);
  CRYPTO_OBJECT_RELEASE(reader_master_key);
  return true;

fail_decrypt:
  DDS_Security_OctetSeq_deinit(plain_submsg);
fail_reader_mac:
  CRYPTO_OBJECT_RELEASE(reader_master_key);
fail_invalid_arg:
  ddsrt_free(footer);
  return false;
}

static DDS_Security_boolean
decode_serialized_payload(
    dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *plain_buffer,
    const DDS_Security_OctetSeq *encoded_buffer,
    const DDS_Security_OctetSeq *inline_qos,
    const DDS_Security_DatareaderCryptoHandle reader_id,
    const DDS_Security_DatawriterCryptoHandle writer_id,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_transform_impl *impl = (dds_security_crypto_transform_impl *)instance;
  dds_security_crypto_key_factory *factory;
  DDS_Security_BasicProtectionKind basic_protection_kind;
  master_key_material *writer_master_key;
  remote_session_info remote_session;
  uint32_t transform_kind, transform_id;
  struct crypto_header header;
  unsigned char *payload_ptr;
  uint32_t payload_len;
  struct crypto_footer *footer = NULL;

  DDSRT_UNUSED_ARG(inline_qos);

  if (!instance || !encoded_buffer || !plain_buffer || reader_id == 0 || writer_id == 0)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "decode_serialized_payload: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    goto fail_inv_arg;
  }

  if ((plain_buffer->_buffer != NULL) || (plain_buffer->_length != 0))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "decode_serialized_payload: given plain_buffer not empty");
    goto fail_inv_arg;
  }

  /* Retrieve key material from sending_datawriter_crypto from factory */
  factory = cryptography_get_crypto_key_factory(impl->crypto);

  /* determine CryptoHeader, CryptoContent and CryptoFooter*/
  if (!split_encoded_serialized_payload(encoded_buffer, &header, &payload_ptr, &payload_len, &footer))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "decode_serialized_payload: Invalid syntax of encoded payload");
    goto fail_split;
  }

  if (footer->receiver_specific_macs._length != 0)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "decode_serialized_payload: Received specific_macs");
    goto fail_prepare;
  }

  transform_kind = CRYPTO_TRANSFORM_KIND(header.transform_identifier.transformation_kind);
  transform_id = CRYPTO_TRANSFORM_ID(header.transform_identifier.transformation_key_id);

  if (!crypto_factory_get_remote_writer_key_material(factory, reader_id, writer_id, transform_id, &writer_master_key, NULL, &basic_protection_kind, ex))
    goto fail_prepare;

  /* calculate the session key */
  if (!initialize_remote_session_info(&remote_session, &header, writer_master_key->master_salt, writer_master_key->master_sender_key, writer_master_key->transformation_kind, ex))
    goto fail_decrypt;

  /*
     * Depending on encryption, the payload part between Header and Footer is
     * either CryptoContent or the original plain payload.
     * See spec: 9.5.3.3.4.4 Result from encode_serialized_payload
     */
  if (is_encryption_required(transform_kind))
  {
    struct crypto_contents_ref contents;
    /* Is encryption expected? */
    if (basic_protection_kind != DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "decode_serialized_payload: payload is encrypted, which is unexpected");
      goto fail_decrypt;
    }
    /* When encrypted, the content payload starts with a length: update contents. */
    if (read_crypto_contents(&contents, payload_ptr, payload_len))
    {
      /* When the CryptoHeader indicates that encryption is performed then decrypt the payload */
      plain_buffer->_buffer = DDS_Security_OctetSeq_allocbuf(contents._length);
      plain_buffer->_length = plain_buffer->_maximum = contents._length;
      if (!crypto_cipher_decrypt_data(&remote_session, header.session_id, contents._data, contents._length, NULL, 0, plain_buffer->_buffer, &plain_buffer->_length, &footer->common_mac, ex))
      {
        goto fail_decrypt;
      }
    }
    else
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_CODE, 0,
          "decode_serialized_payload: invalid payload format");
      goto fail_decrypt;
    }
  }
  else if (is_authentication_required(transform_kind))
  {
    /* Is signing expected? */
    if (basic_protection_kind != DDS_SECURITY_BASICPROTECTION_KIND_SIGN)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "decode_serialized_payload: payload is signed, which is unexpected");
      goto fail_decrypt;
    }
    /* When the CryptoHeader indicates that authentication is performed then calculate the HMAC */
    if (!crypto_cipher_decrypt_data(&remote_session, header.session_id, NULL, 0, payload_ptr, payload_len, NULL, 0, &footer->common_mac, ex))
    {
      goto fail_decrypt;
    }
    plain_buffer->_buffer = DDS_Security_OctetSeq_allocbuf(payload_len);
    plain_buffer->_length = plain_buffer->_maximum = payload_len;
    memcpy(plain_buffer->_buffer, payload_ptr, payload_len);
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_CODE, 0,
        "decode_serialized_payload: " DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_MESSAGE);
    goto fail_decrypt;
  }
  ddsrt_free(footer);
  CRYPTO_OBJECT_RELEASE(writer_master_key);
  return true;

fail_decrypt:
  DDS_Security_OctetSeq_deinit(plain_buffer);
  CRYPTO_OBJECT_RELEASE(writer_master_key);
fail_prepare:
  ddsrt_free(footer);
fail_split:
fail_inv_arg:
  return false;
}

dds_security_crypto_transform *
dds_security_crypto_transform__alloc(
    const dds_security_cryptography *crypto)
{
  dds_security_crypto_transform_impl *instance;
  instance = (dds_security_crypto_transform_impl *)ddsrt_malloc(
      sizeof(dds_security_crypto_transform_impl));

  instance->crypto = crypto;
  instance->base.encode_datawriter_submessage = &encode_datawriter_submessage;
  instance->base.encode_datareader_submessage = &encode_datareader_submessage;
  instance->base.encode_rtps_message = &encode_rtps_message;
  instance->base.encode_serialized_payload = &encode_serialized_payload;
  instance->base.decode_rtps_message = &decode_rtps_message;
  instance->base.preprocess_secure_submsg = &preprocess_secure_submsg;
  instance->base.decode_datawriter_submessage = &decode_datawriter_submessage;
  instance->base.decode_datareader_submessage = &decode_datareader_submessage;
  instance->base.decode_serialized_payload = &decode_serialized_payload;

  dds_openssl_init ();
  return (dds_security_crypto_transform *)instance;
}

void dds_security_crypto_transform__dealloc(
    dds_security_crypto_transform *instance)
{
  ddsrt_free((dds_security_crypto_transform_impl *)instance);
}
