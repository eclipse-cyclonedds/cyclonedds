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
#include "dds/ddsi/q_protocol.h"
#include "cryptography.h"
#include "crypto_cipher.h"
#include "crypto_defs.h"
#include "crypto_key_factory.h"
#include "crypto_objects.h"
#include "crypto_transform.h"
#include "crypto_utils.h"

#define CRYPTO_ENCRYPTION_MAX_PADDING 32

#define INFO_SRC_SIZE sizeof(InfoSRC_t)

struct receiver_specific_mac_seq
{
  uint32_t _length;
  struct receiver_specific_mac _buffer[];
};

struct crypto_prefix {
  struct CryptoTransformIdentifier transform_identifier;
  struct init_vector iv;
};
#define CRYPTO_PREFIX_SIZE sizeof(struct crypto_prefix)

struct crypto_content
{
  uint32_t length;
  unsigned char data[];
};

struct crypto_postfix {
  crypto_hmac_t common_mac;
  struct receiver_specific_mac_seq receiver_specific_macs;
};

struct crypto_header
{
  SubmessageHeader_t header;
  struct crypto_prefix prefix;
};

struct crypto_body
{
  SubmessageHeader_t header;
  struct crypto_content content;
};

struct crypto_footer
{
  SubmessageHeader_t header;
  struct crypto_postfix postfix;
};
#define CRYPTO_FOOTER_BASIC_SIZE (CRYPTO_HMAC_SIZE + sizeof(uint32_t))
#define CRYPTO_FOOTER_MIN_SIZE   (sizeof(struct crypto_footer))

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

static bool is_encryption_required(uint32_t transform_kind)
{
  return ((transform_kind == CRYPTO_TRANSFORMATION_KIND_AES256_GCM) ||
          (transform_kind == CRYPTO_TRANSFORMATION_KIND_AES128_GCM));
}

static bool is_authentication_required(uint32_t transform_kind)
{
  return ((transform_kind == CRYPTO_TRANSFORMATION_KIND_AES256_GMAC) ||
          (transform_kind == CRYPTO_TRANSFORMATION_KIND_AES128_GMAC));
}

static bool is_encryption_expected(DDS_Security_ProtectionKind protection_kind)
{
  return ((protection_kind == DDS_SECURITY_PROTECTION_KIND_ENCRYPT) ||
          (protection_kind == DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION));
}

static bool is_authentication_expected(DDS_Security_ProtectionKind protection_kind)
{
  return ((protection_kind == DDS_SECURITY_PROTECTION_KIND_SIGN) ||
          (protection_kind == DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION));
}

static bool has_origin_authentication(DDS_Security_ProtectionKind kind)
{
  return ((kind == DDS_SECURITY_PROTECTION_KIND_SIGN_WITH_ORIGIN_AUTHENTICATION) || (kind == DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION));
}


typedef struct crypto_buffer {
  unsigned char *contents;
  size_t length;
  unsigned char *inptr;
} crypto_buffer_t;

static void crypto_buffer_init(crypto_buffer_t *buffer, size_t size)
{
  buffer->contents = ddsrt_malloc(size);
  buffer->inptr = buffer->contents;
  buffer->length = size;
}

static void crypto_buffer_from_seq(crypto_buffer_t *buffer, const DDS_Security_OctetSeq *seq)
{
  assert(seq->_length <= seq->_maximum);

  buffer->contents = seq->_buffer;
  buffer->inptr = buffer->contents + seq->_length;
  buffer->length = seq->_maximum;
}

static void crypto_buffer_to_seq(const crypto_buffer_t *buffer, DDS_Security_OctetSeq *seq)
{
  seq->_buffer = buffer->contents;
  seq->_length = (uint32_t)(buffer->inptr - buffer->contents);
  seq->_maximum = (uint32_t)buffer->length;
}

static void *crypto_buffer_expand(crypto_buffer_t *buffer, size_t size)
{
  if ( (size_t)(buffer->inptr - buffer->contents) >= buffer->length - size)
  {
    size_t l = buffer->length + size;
    size_t offset = (size_t)(buffer->inptr - buffer->contents);
    buffer->contents = ddsrt_realloc(buffer->contents, l);
    buffer->length = l;
    buffer->inptr = buffer->contents + offset;
  }
  return buffer->inptr;
}

static void *crypto_buffer_append(crypto_buffer_t *buffer, size_t size)
{
  void *ptr = crypto_buffer_expand(buffer, size);
  buffer->inptr += size;
  return ptr;
}

static void *add_submessage(crypto_buffer_t *buffer, uint8_t submessageId, size_t size)
{
  SubmessageHeader_t *smhdr;
  size_t len = sizeof(SubmessageHeader_t) + size;

  assert (size <= UINT16_MAX);

  smhdr = crypto_buffer_append(buffer, len);
  smhdr->submessageId = submessageId;
  smhdr->flags = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? SMFLAG_ENDIANNESS : 0);
  smhdr->octetsToNextHeader = (uint16_t)size;

  return smhdr;
}

static void set_crypto_prefix(struct crypto_prefix *prefix, uint32_t transform_kind, uint32_t transform_id, uint32_t session_id, uint64_t init_vector_suffix)
{
  struct
  {
    uint32_t tkind;
    uint32_t tid;
    uint32_t sid;
    uint32_t ivh;
    uint32_t ivl;
  } s;
  uint64_t ivs = ddsrt_toBE8u(init_vector_suffix);

  s.tkind = ddsrt_toBE4u(transform_kind);
  s.tid = ddsrt_toBE4u(transform_id);
  s.sid = ddsrt_toBE4u(session_id);
  s.ivh = (uint32_t)(ivs >> 32);
  s.ivl = (uint32_t)ivs;

  memcpy(prefix, &s, CRYPTO_PREFIX_SIZE);
}

static struct crypto_prefix * add_crypto_prefix(crypto_buffer_t *buffer, uint32_t transform_kind, uint32_t transform_id, uint32_t session_id, uint64_t init_vector_suffix)
{
  struct crypto_prefix *prefix = crypto_buffer_append(buffer, sizeof(struct crypto_prefix));
  set_crypto_prefix(prefix, transform_kind, transform_id, session_id, init_vector_suffix);
  return prefix;
}

static struct crypto_content * add_crypto_content(crypto_buffer_t *buffer, size_t size)
{
  struct crypto_content *content = crypto_buffer_append(buffer, size + sizeof(uint32_t));
  return content;
}

static struct crypto_postfix * add_crypto_postfix(crypto_buffer_t *buffer)
{
  struct crypto_postfix *postfix = crypto_buffer_append(buffer, CRYPTO_FOOTER_BASIC_SIZE);
  postfix->receiver_specific_macs._length = 0;
  return postfix;
}

static struct crypto_header * add_crypto_header(crypto_buffer_t *buffer, uint8_t submessageId, uint32_t transform_kind, uint32_t transform_id, uint32_t session_id, uint64_t init_vector_suffix)
{
  struct crypto_header *header = add_submessage(buffer, submessageId, CRYPTO_PREFIX_SIZE);
  set_crypto_prefix(&header->prefix, transform_kind, transform_id, session_id, init_vector_suffix);
  return header;
}

static struct crypto_body * add_crypto_body(crypto_buffer_t *buffer, size_t size)
{
  struct crypto_body *body = add_submessage(buffer, SMID_SEC_BODY, size + sizeof(uint32_t));
  return body;
}

static struct crypto_footer * add_crypto_footer(crypto_buffer_t *buffer, uint8_t submessageId)
{
  struct crypto_footer *footer = add_submessage(buffer, submessageId, CRYPTO_FOOTER_BASIC_SIZE);
  return footer;
}

static Header_t * add_rtps_header(crypto_buffer_t *buffer, const Header_t *src)
{
  Header_t *dst = crypto_buffer_append(buffer, RTPS_MESSAGE_HEADER_SIZE);
  *dst = *src;
  return dst;
}

static void * crypto_buffer_find_submessage(crypto_buffer_t *buffer, uint8_t submessageId, size_t offset)
{
  unsigned char *ptr = buffer->contents + offset;
  bool found = false;

  while ((size_t)(ptr - buffer->contents) <= buffer->length - sizeof(SubmessageHeader_t))
  {
    SubmessageHeader_t *smhdr = (SubmessageHeader_t *)ptr;
    if (smhdr->submessageId == submessageId)
    {
      found = true;
      break;
    }
    ptr += smhdr->octetsToNextHeader + sizeof(SubmessageHeader_t);
  }

  return (found ? ptr : NULL);
}

static struct crypto_header * crypto_buffer_find_header(crypto_buffer_t *buffer, uint8_t submessageId)
{
  assert(submessageId == SMID_SRTPS_PREFIX || submessageId == SMID_SEC_PREFIX);
  return crypto_buffer_find_submessage(buffer, submessageId, (submessageId == SMID_SRTPS_PREFIX ? RTPS_MESSAGE_HEADER_SIZE : 0));
}

static struct crypto_footer * crypto_buffer_find_footer(crypto_buffer_t *buffer, uint8_t submessageId)
{
  assert(submessageId == SMID_SRTPS_POSTFIX || submessageId == SMID_SEC_POSTFIX);
  return crypto_buffer_find_submessage(buffer, submessageId, (submessageId == SMID_SRTPS_POSTFIX ? RTPS_MESSAGE_HEADER_SIZE : 0));
}

static void init_info_src(InfoSRC_t *info_src, Header_t *rtps_hdr)
{
  info_src->smhdr.submessageId = SMID_INFO_SRC;
  info_src->smhdr.flags = (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN ? SMFLAG_ENDIANNESS : 0);
  info_src->smhdr.octetsToNextHeader = sizeof(*rtps_hdr);
  info_src->unused = 0;
  info_src->version = rtps_hdr->version;
  info_src->vendorid = rtps_hdr->vendorid;
  info_src->guid_prefix = rtps_hdr->guid_prefix;
}


static bool transform_kind_valid(DDS_Security_CryptoTransformKind_Enum kind)
{
  return ((kind == CRYPTO_TRANSFORMATION_KIND_AES128_GMAC) ||
          (kind == CRYPTO_TRANSFORMATION_KIND_AES128_GCM) ||
          (kind == CRYPTO_TRANSFORMATION_KIND_AES256_GMAC) ||
          (kind == CRYPTO_TRANSFORMATION_KIND_AES256_GCM));
}

/**************************************************************************************/

struct secure_prefix {
  uint32_t transform_kind;
  uint32_t transform_id;
  uint32_t session_id;
  struct init_vector iv;
};

struct secure_body {
  uint8_t id;
  crypto_data_t data;
};

struct secure_postfix {
  crypto_hmac_t common_mac;
  uint32_t length;
  struct receiver_specific_mac *recv_spec_mac;
};

struct encrypted_state {
  struct secure_prefix prefix;
  struct secure_body body;
  struct secure_postfix postfix;
};

static bool initialize_remote_session_info(remote_session_info *info, struct secure_prefix *prefix, const unsigned char *master_salt, const unsigned char *master_key, DDS_Security_CryptoTransformKind_Enum transformation_kind, DDS_Security_SecurityException *ex)
{
  info->key_size = crypto_get_key_size (transformation_kind);
  info->id = prefix->session_id;
  return crypto_calculate_session_key(&info->key, info->id, master_salt, master_key, transformation_kind, ex);
}

static bool read_submsg_header(unsigned char **ptr, unsigned char *endp, uint8_t smid, SubmessageHeader_t *hdr, bool *bswap)
{
  SubmessageHeader_t*smhdr = (SubmessageHeader_t *)*ptr;

  if (RTPS_SUBMESSAGE_HEADER_SIZE > (size_t) (endp - *ptr))
    return false;

  if (smid != 0 && smid != smhdr->submessageId)
    return false;

  hdr->submessageId = smhdr->submessageId;
  hdr->flags = smhdr->flags;

  if (smhdr->flags & SMFLAG_ENDIANNESS)
    *bswap = !(DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN);
  else
    *bswap =  (DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN);

  if (*bswap)
    hdr->octetsToNextHeader = ddsrt_bswap2u(smhdr->octetsToNextHeader);
  else
    hdr->octetsToNextHeader = smhdr->octetsToNextHeader;
  *ptr += RTPS_SUBMESSAGE_HEADER_SIZE;
  return true;
}

static bool read_secure_prefix(unsigned char **ptr, unsigned char *endp, uint8_t smid, struct secure_prefix *prefix)
{
  SubmessageHeader_t smhdr;
  bool bswap;

  if (!read_submsg_header(ptr, endp, smid, &smhdr, &bswap))
    return false;
  if (smhdr.octetsToNextHeader > (size_t) (endp - *ptr))
    return false;
  if (smhdr.octetsToNextHeader < sizeof(struct secure_prefix) - sizeof(uint32_t))
    return false;

  prefix->transform_kind = ddsrt_fromBE4u(*(uint32_t *)*ptr);
  *ptr += sizeof(prefix->transform_kind);
  prefix->transform_id = ddsrt_fromBE4u(*(uint32_t *)*ptr);
  *ptr += sizeof(prefix->transform_id);
  prefix->session_id = ddsrt_fromBE4u(*(uint32_t *)(*ptr));
  prefix->iv = *(struct init_vector *)*ptr;
  *ptr += sizeof(prefix->iv);

  if (!transform_kind_valid(prefix->transform_kind))
    return false;

  return true;
}

/* This function retrieves the secure body part of submessage
 *
 * The body part of a secure submessage is either
 * - a plain submessage
 * - a secure body submessage containing a encrypted submessage
 *
 * plain submessage:
 *         ---------------------------
 * body -> | plain submsg hdr        |
 *         ---------------------------
 *         | submessage contents     |
 *         |         .               |
 *         |         .               |
 *         ---------------------------
 *
 * secure body:
 *
 *         ---------------------------
 *         | secure body submsg hdr  |
 *         ---------------------------
 *         | len of encrypted submsg |
 *         ---------------------------
 * body->  | encrypted submsg        |
 *         |         .               |
 *         |         .               |
 *         ---------------------------
 *
 *
 */
static bool read_secure_body(unsigned char **ptr, unsigned char *endp, uint8_t smid, struct secure_body *body)
{
  SubmessageHeader_t smhdr;
  bool bswap;

  body->data.base = *ptr;

  if (!read_submsg_header(ptr, endp, smid, &smhdr, &bswap))
    return false;
  if (smhdr.octetsToNextHeader > (size_t) (endp - *ptr))
    return false;
  body->id = smhdr.submessageId;
  if (smhdr.submessageId == SMID_SEC_BODY)
  {
    if (smhdr.octetsToNextHeader < sizeof(uint32_t))
      return false;

    body->data.length = ddsrt_fromBE4u(*(uint32_t *)(*ptr));
    if (body->data.length > smhdr.octetsToNextHeader - sizeof(uint32_t))
      return false;
    /* set the base address to point to the encrypted submsg */
    body->data.base = *ptr + sizeof(uint32_t);
    *ptr += smhdr.octetsToNextHeader ;
  }
  else
  {
    /* set the length of the body contents to the complete submessage including the header */
    body->data.length = smhdr.octetsToNextHeader + (uint32_t)RTPS_SUBMESSAGE_HEADER_SIZE;
    *ptr += smhdr.octetsToNextHeader;
  }

  return true;
}

static bool read_secure_postfix(unsigned char **ptr, unsigned char *endp, uint8_t smid, struct secure_postfix *postfix)
{
  static const size_t postfix_min_size = sizeof(postfix->common_mac) + sizeof(postfix->length);
  SubmessageHeader_t smhdr;
  bool bswap;

  if (!read_submsg_header(ptr, endp, smid, &smhdr, &bswap))
    return false;
  if (smhdr.octetsToNextHeader > (size_t) (endp - *ptr))
    return false;
  if (smhdr.octetsToNextHeader < postfix_min_size)
    return false;

  postfix->common_mac = *(crypto_hmac_t *)*ptr;
  *ptr += sizeof(postfix->common_mac);
  postfix->length = ddsrt_fromBE4u(*(uint32_t *)*ptr);

  if (((smhdr.octetsToNextHeader - postfix_min_size) / sizeof(struct receiver_specific_mac) < postfix->length))
    return false;

  *ptr += sizeof(postfix->length);
  postfix->recv_spec_mac = (struct receiver_specific_mac *)*ptr;
  *ptr += postfix->length * sizeof(struct receiver_specific_mac);
  return true;
}

static bool get_secure_prefix(const DDS_Security_OctetSeq *data, struct secure_prefix *prefix)
{
  unsigned char *ptr, *endp;

  ptr = data->_buffer;
  endp = ptr + data->_length;

  return read_secure_prefix(&ptr, endp, SMID_SEC_PREFIX, prefix);
}

static bool split_encoded_serialized_payload(const DDS_Security_OctetSeq *payload, struct encrypted_state *estate)
{
  /* For data, the footer is always the same length. */
  static const size_t header_len = sizeof(estate->prefix.transform_id) + sizeof(estate->prefix.transform_kind) + sizeof(estate->prefix.iv);
  static const size_t footer_len = sizeof(estate->postfix.common_mac) + sizeof(estate->postfix.length);
  size_t min_size = header_len + footer_len;
  struct secure_prefix *prefix = &estate->prefix;
  struct secure_body *body = &estate->body;
  struct secure_postfix *postfix = &estate->postfix;
  unsigned char *ptr = payload->_buffer;

  if (payload->_length < min_size)
    return false;

  prefix->transform_kind = ddsrt_fromBE4u(*(uint32_t *)ptr);
  ptr += sizeof(prefix->transform_kind);
  prefix->transform_id = ddsrt_fromBE4u(*(uint32_t *)ptr);
  ptr += sizeof(prefix->transform_id);
  prefix->session_id = ddsrt_fromBE4u(*(uint32_t *)ptr);
  prefix->iv = *(struct init_vector *)ptr;
  ptr += sizeof(prefix->iv);

  if (is_encryption_required(prefix->transform_kind))
  {
    min_size += sizeof(uint32_t);
    if (payload->_length < min_size)
      return false;
    body->data.length = ddsrt_fromBE4u(*(uint32_t *)ptr);
    if (payload->_length - min_size < body->data.length)
      return false;
    ptr += sizeof(uint32_t);
    body->data.base = ptr;
  }
  else
  {
    body->data.base = ptr;
    body->data.length = payload->_length - (uint32_t)min_size;
  }
  ptr += body->data.length;

  postfix->common_mac = *(crypto_hmac_t *)ptr;
  ptr += sizeof(postfix->common_mac);
  postfix->length = ddsrt_fromBE4u(*(uint32_t *)ptr);

  if ((payload->_length - min_size - body->data.length)/sizeof(struct receiver_specific_mac) < postfix->length)
     return false;

  ptr += sizeof(postfix->length);
  postfix->recv_spec_mac = (struct receiver_specific_mac *)ptr;

  return true;
}

static bool split_encoded_submessage(const DDS_Security_OctetSeq *data, struct encrypted_state *estate)
{
  unsigned char *ptr, *endp;

  ptr = data->_buffer;
  endp = ptr + data->_length;

  if (!read_secure_prefix(&ptr, endp, SMID_SEC_PREFIX, &estate->prefix))
    return false;
  if (!read_secure_body(&ptr, endp, 0, &estate->body))
    return false;
  return read_secure_postfix(&ptr, endp, SMID_SEC_POSTFIX, &estate->postfix);
}

static bool read_secure_rtps_body(unsigned char **ptr, unsigned char *endp, DDS_Security_CryptoTransformKind_Enum transformation_kind, struct secure_body *body)
{
  SubmessageHeader_t smhdr;
  bool bswap;

  if (is_encryption_required(transformation_kind))
  {
    /*read sec body */
    if (!read_secure_body(ptr, endp, SMID_SEC_BODY, body))
      return false;
  }
  else
  {
    body->data.base = *ptr;

    if (!read_submsg_header(ptr, endp, SMID_INFO_SRC, &smhdr, &bswap))
      return false;

    *ptr += smhdr.octetsToNextHeader;

    while (sizeof(smhdr) < (size_t)(endp - *ptr))
    {
      if (!read_submsg_header(ptr, endp, 0, &smhdr, &bswap))
        return false;
      if (smhdr.submessageId == SMID_SRTPS_POSTFIX)
      {
        /* correct for reading a submessage header to far */
        *ptr -= RTPS_SUBMESSAGE_HEADER_SIZE;
        break;
      }
      *ptr += smhdr.octetsToNextHeader;
    }
    if (smhdr.submessageId != SMID_SRTPS_POSTFIX)
      return false;
    body->data.length = (uint32_t)(*ptr - body->data.base);
  }

  return true;
}

static bool split_encoded_rtps_message(const crypto_data_t *data, struct encrypted_state *estate)
{
  unsigned char *ptr, *endp;

  ptr = data->base;
  endp = ptr + data->length;

  if (ptr + RTPS_MESSAGE_HEADER_SIZE > endp)
    return false;
  ptr += RTPS_MESSAGE_HEADER_SIZE;

  if (!read_secure_prefix(&ptr, endp, SMID_SRTPS_PREFIX, &estate->prefix))
    return false;

  if (!read_secure_rtps_body(&ptr, endp, estate->prefix.transform_kind, &estate->body))
    return false;
  return read_secure_postfix(&ptr, endp, SMID_SRTPS_POSTFIX, &estate->postfix);
}




/**************************************************************************************/


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
  dds_security_crypto_key_factory *factory = cryptography_get_crypto_key_factory(impl->crypto);
  crypto_data_t plain_data;
  session_key_material *session;
  struct crypto_prefix *prefix;
  struct crypto_content *content;
  struct crypto_postfix *postfix;
  crypto_buffer_t buffer;
  crypto_hmac_t hmac;
  uint32_t transform_kind, transform_id;
  size_t size;

  DDSRT_UNUSED_ARG(extra_inline_qos);

  assert(encoded_buffer);
  assert(plain_buffer);
  assert(plain_buffer->_length);
  assert(writer_id != 0);
  assert((plain_buffer->_length % 4) == 0);

  if (plain_buffer->_length > INT_MAX)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "encoding payload failed: length exceeds INT_MAX");
    return false;
  }

  /* Retrieve key material from sending_datawriter_crypto from factory */
  if (!crypto_factory_get_writer_key_material(factory, writer_id, 0, true, &session, NULL, ex))
    goto fail_inv_arg;

  if (!session)
  {
    DDS_Security_OctetSeq_copy(encoded_buffer, plain_buffer);
    return true;
  }

  plain_data.base = plain_buffer->_buffer;
  plain_data.length = plain_buffer->_length;

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
  size = sizeof(*prefix) + sizeof(*content) + sizeof(*postfix) + plain_buffer->_length + session->block_size + 1;

  crypto_buffer_init(&buffer, size);
  /* create CryptoHeader */
  prefix = add_crypto_prefix(&buffer, transform_kind, transform_id, session->id, session->init_vector_suffix);

  /* if the transformation_kind indicates encryption then encrypt the buffer */
  if (is_encryption_required(transform_kind))
  {
    crypto_data_t encrypted_data;

    content = add_crypto_content(&buffer, plain_buffer->_length);

    encrypted_data.base = content->data;
    encrypted_data.length = plain_buffer->_length;

    if (!crypto_cipher_encrypt_data(&session->key, session->key_size, &prefix->iv, 1, &plain_data, &encrypted_data, &hmac, ex))
      goto fail_encrypt;
    content->length = ddsrt_toBE4u((uint32_t)encrypted_data.length);
  }
  else if (is_authentication_required(transform_kind))
  {
    /* the transformation_kind indicates only indicates authentication the determine HMAC */
    if (!crypto_cipher_encrypt_data(&session->key, session->key_size, &prefix->iv, 1, &plain_data, NULL, &hmac, ex))
      goto fail_encrypt;
    unsigned char *ptr = crypto_buffer_append(&buffer,  plain_buffer->_length);
    memcpy(ptr, plain_buffer->_buffer, plain_buffer->_length);
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "encode_serialized_payload: unknown transform_kind %d", (int)transform_kind);
    goto fail_encrypt;
  }

  postfix = add_crypto_postfix(&buffer);
  postfix->common_mac = hmac;

  crypto_buffer_to_seq(&buffer, encoded_buffer);

  CRYPTO_OBJECT_RELEASE(session);

  return true;

fail_encrypt:
  ddsrt_free(buffer.contents);
fail_update_key:
  CRYPTO_OBJECT_RELEASE(session);
fail_inv_arg:
  return false;
}


static bool
add_specific_mac(
    crypto_buffer_t *buffer,
    master_key_material *keymat,
    session_key_material *session,
    bool is_rtps,
    DDS_Security_SecurityException *ex)
{
  uint32_t index;
  const uint8_t prefix_kind = is_rtps ? SMID_SRTPS_PREFIX : SMID_SEC_PREFIX;
  const uint8_t postfix_kind = is_rtps ? SMID_SRTPS_POSTFIX : SMID_SEC_POSTFIX;
  crypto_data_t data;
  struct crypto_header *header;
  struct crypto_footer *footer;
  crypto_session_key_t key;
  crypto_hmac_t hmac;

  crypto_buffer_append(buffer, sizeof(struct receiver_specific_mac));

  if ((header = crypto_buffer_find_header(buffer, prefix_kind)) == NULL)
    return false;
  else if ((footer = crypto_buffer_find_footer(buffer, postfix_kind)) == NULL)
    return false;

  footer->header.octetsToNextHeader = (uint16_t)(footer->header.octetsToNextHeader + sizeof(struct receiver_specific_mac));
  index = ddsrt_fromBE4u(footer->postfix.receiver_specific_macs._length);
  data.base = footer->postfix.common_mac.data;
  data.length = CRYPTO_HMAC_SIZE;

  if (!crypto_calculate_receiver_specific_key(&key, session->id, keymat->master_salt, keymat->master_receiver_specific_key, keymat->transformation_kind, ex) ||
      !crypto_cipher_encrypt_data(&key, session->key_size, &header->prefix.iv, 1, &data, NULL, &hmac, ex))
    return false;

  uint32_t key_id = ddsrt_toBE4u(keymat->receiver_specific_key_id);
  struct crypto_postfix *postfix = &footer->postfix;
  struct receiver_specific_mac *rcvmac = &postfix->receiver_specific_macs._buffer[index];
  rcvmac->receiver_mac = hmac;
  memcpy(rcvmac->receiver_mac_key_id, &key_id, sizeof(key_id));
  postfix->receiver_specific_macs._length = ddsrt_toBE4u(++index);

  return true;
}

static bool
add_reader_specific_mac(
    dds_security_crypto_key_factory *factory,
    crypto_buffer_t *buffer,
    DDS_Security_DatareaderCryptoHandle reader_crypto,
    DDS_Security_SecurityException *ex)
{
  bool result = true;
  master_key_material *keymat = NULL;
  session_key_material *session = NULL;
  DDS_Security_ProtectionKind protection_kind;

  if (!crypto_factory_get_remote_reader_sign_key_material(factory, reader_crypto, &keymat, &session, &protection_kind, ex))
      return false;

  if (has_origin_authentication(protection_kind))
    result = add_specific_mac(buffer, keymat, session, false, ex);

  CRYPTO_OBJECT_RELEASE(session);
  CRYPTO_OBJECT_RELEASE(keymat);
  return result;
}

static bool
add_writer_specific_mac(
    dds_security_crypto_key_factory *factory,
    crypto_buffer_t *buffer,
    DDS_Security_DatawriterCryptoHandle writer_crypto,
    DDS_Security_SecurityException *ex)
{
  bool result = true;
  master_key_material *keymat = NULL;
  session_key_material *session = NULL;
  DDS_Security_ProtectionKind protection_kind;

  if (!crypto_factory_get_remote_writer_sign_key_material(factory, writer_crypto, &keymat, &session, &protection_kind, ex))
    return false;

  if (has_origin_authentication(protection_kind))
    result = add_specific_mac(buffer, keymat, session, false, ex);

  CRYPTO_OBJECT_RELEASE(session);
  CRYPTO_OBJECT_RELEASE(keymat);
  return result;
}

static bool
add_receiver_specific_mac(
    dds_security_crypto_key_factory *factory,
    crypto_buffer_t *buffer,
    DDS_Security_DatareaderCryptoHandle sending_participant_crypto,
    DDS_Security_DatareaderCryptoHandle receiving_participant_crypto,
    DDS_Security_SecurityException *ex)
{
  bool result = true;
  session_key_material *session = NULL;
  DDS_Security_ProtectionKind local_protection_kind;
  DDS_Security_ProtectionKind remote_protection_kind;
  participant_key_material *keymat;

  /* get local crypto and session*/
  if (!crypto_factory_get_local_participant_data_key_material(factory, sending_participant_crypto, &session, &local_protection_kind, ex))
    return false;

  /* get remote crypto tokens */
  if (!crypto_factory_get_participant_crypto_tokens(factory, sending_participant_crypto, receiving_participant_crypto, &keymat, NULL, &remote_protection_kind, ex))
  {
    CRYPTO_OBJECT_RELEASE(session);
    return false;
  }

  if (has_origin_authentication(remote_protection_kind))
    result = add_specific_mac(buffer, keymat->local_P2P_key_material, session, true, ex);

  CRYPTO_OBJECT_RELEASE(keymat);
  CRYPTO_OBJECT_RELEASE(session);
  return result;
}

static DDS_Security_boolean
encode_submmessage_encrypt(
    dds_security_crypto_key_factory *factory,
    DDS_Security_OctetSeq *encoded_submsg,
    const DDS_Security_OctetSeq *plain_submsg,
    session_key_material *session,
    DDS_Security_ProtectionKind protection_kind,
    const DDS_Security_CryptoHandleSeq *crypto_list,
    int32_t *index,
    bool is_writer,
    DDS_Security_SecurityException *ex)
{
  crypto_buffer_t buffer;
  crypto_data_t plain_data;
  crypto_hmac_t hmac;
  size_t size;
  struct crypto_header *header;
  struct crypto_footer *footer;
  uint32_t transform_kind, transform_id;
  DDS_Security_boolean result = false;

  assert(!is_writer || index != NULL);

  if (plain_submsg->_length > INT_MAX)
   {
     DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "encoding submessage failed: length exceeds INT_MAX");
     return false;
   }

  /* update sessionKey when needed */
  if (!crypto_session_key_material_update(session, plain_submsg->_length, ex))
    return false;

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

  size = sizeof(struct crypto_header) + sizeof(struct crypto_footer) + ALIGN4(plain_submsg->_length);
  size += crypto_list->_length * sizeof(struct receiver_specific_mac);
  /* assure that the buffer contains enough memory to accommodate the encrypted payload */
  if (is_encryption_required(session->master_key_material->transformation_kind))
    size += sizeof(struct crypto_body) + CRYPTO_ENCRYPTION_MAX_PADDING;

  /* increment init_vector_suffix */
  session->init_vector_suffix++;

  transform_kind = session->master_key_material->transformation_kind;
  transform_id = session->master_key_material->sender_key_id;

  /* allocate a buffer to store the encoded submessage */
  crypto_buffer_init(&buffer, size);
  /* Add the SEC_PREFIX and associated CryptoHeader */
  header = add_crypto_header(&buffer, SMID_SEC_PREFIX, transform_kind, transform_id, session->id, session->init_vector_suffix);

  plain_data.base = plain_submsg->_buffer;
  plain_data.length = plain_submsg->_length;

  if (is_encryption_required(transform_kind))
  {
    struct crypto_body *body = add_crypto_body(&buffer, plain_submsg->_length);
    crypto_data_t encrypted_data = { .base = body->content.data, .length = plain_submsg->_length };

    /* encrypt submessage */
    if (!crypto_cipher_encrypt_data(&session->key, session->key_size, &header->prefix.iv, 1, &plain_data, &encrypted_data, &hmac, ex))
      goto enc_submsg_fail;

    /* adjust the length of the body submessage when needed */
    body->content.length = ddsrt_toBE4u((uint32_t)encrypted_data.length);
    if (encrypted_data.length > plain_submsg->_length)
    {
      size_t inc = encrypted_data.length - plain_submsg->_length;
      body->header.octetsToNextHeader = (uint16_t)(body->header.octetsToNextHeader + inc);
      (void)crypto_buffer_expand(&buffer, inc);
    }
  }
  else if (is_authentication_required(transform_kind))
  {
    unsigned char *ptr = crypto_buffer_append(&buffer, plain_submsg->_length);
    /* the transformation_kind indicates only indicates authentication the determine HMAC */
    if (!crypto_cipher_encrypt_data(&session->key, session->key_size, &header->prefix.iv, 1, &plain_data, NULL, &hmac, ex))
      goto enc_submsg_fail;

    /* copy submessage */
    memcpy(ptr, plain_submsg->_buffer, plain_submsg->_length);
  }
  else
  {
    goto enc_submsg_fail;
  }

  /* Set initial SEC_POSTFIX and CryptoFooter containing the common_mac
   * Note that the length of the postfix may increase when reader specific macs are added
   */
  footer = add_crypto_footer(&buffer, SMID_SEC_POSTFIX);
  footer->postfix.common_mac = hmac;
  footer->postfix.receiver_specific_macs._length = 0;

  if (is_writer)
  {
    if (!has_origin_authentication(protection_kind))
      *index = (int32_t) crypto_list->_length;
    else
    {
      DDS_Security_CryptoHandle remote_crypto = crypto_list->_buffer[0];

      if (!add_reader_specific_mac(factory, &buffer, remote_crypto, ex))
        goto enc_submsg_fail;
      (*index)++;
    }
  }
  else
  {
    for (uint32_t i = 0; i < crypto_list->_length; i++)
    {
      if (!add_writer_specific_mac(factory, &buffer, crypto_list->_buffer[i], ex))
        goto enc_submsg_fail;
    }
  }

  crypto_buffer_to_seq(&buffer, encoded_submsg);
  result = true;

enc_submsg_fail:
  if (!result)
  {
    ddsrt_free(buffer.contents);
    encoded_submsg->_buffer = NULL;
    encoded_submsg->_length = 0;
    encoded_submsg->_maximum = 0;
  }
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
  session_key_material *session = NULL;
  DDS_Security_ProtectionKind protection_kind;
  DDS_Security_boolean result = false;

  if (reader_crypto_list->_length > 0)
    reader_crypto = reader_crypto_list->_buffer[0];

  if (!crypto_factory_get_writer_key_material(factory, writer_crypto, reader_crypto, false, &session, &protection_kind, ex))
    return false;

  result = encode_submmessage_encrypt(factory, encoded_submsg, plain_submsg, session, protection_kind, reader_crypto_list, index, true, ex);

  CRYPTO_OBJECT_RELEASE(session);

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
  dds_security_crypto_key_factory *factory = cryptography_get_crypto_key_factory(impl->crypto);
  DDS_Security_boolean result = false;

  assert(encoded_submsg);
  assert(writer_crypto != 0);
  assert(reader_crypto_list && reader_crypto_list->_length > 0);
  assert(index);
  assert(*index < (int32_t)reader_crypto_list->_length);
  assert((plain_submsg && plain_submsg->_length > 0) || *index > 0);
  assert(encoded_submsg->_length > 0 || *index == 0);

  if (*index == 0)
  {
    /* When the index is 0 then retrieve the key material of the writer */
    result = encode_datawriter_submessage_encrypt (factory, encoded_submsg, plain_submsg, writer_crypto, reader_crypto_list, index, ex);
  }
  else
  {
    /* When the index is not 0 then add a signature for the specific reader */
    crypto_buffer_t buffer;
    DDS_Security_DatareaderCryptoHandle reader_crypto = reader_crypto_list->_buffer[*index];

    crypto_buffer_from_seq(&buffer, encoded_submsg);
    /* When the receiving_participant_crypto_list_index is not 0 then add a signature for the specific reader */
    result = add_reader_specific_mac(factory, &buffer, reader_crypto, ex);
    if (result)
    {
      crypto_buffer_to_seq(&buffer, encoded_submsg);
      (*index)++;
    }
  }

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
  dds_security_crypto_key_factory *factory = cryptography_get_crypto_key_factory(impl->crypto);
  DDS_Security_DatawriterCryptoHandle writer_crypto = 0;
  session_key_material *session = NULL;
  DDS_Security_ProtectionKind protection_kind;
  DDS_Security_boolean result = false;

  assert(encoded_submsg);
  assert(plain_submsg && plain_submsg->_length > 0 && plain_submsg->_buffer);

  if (writer_crypto_list->_length > 0)
    writer_crypto = writer_crypto_list->_buffer[0];

  if (!crypto_factory_get_reader_key_material(factory, reader_crypto, writer_crypto, &session, &protection_kind, ex))
    return false;

  result = encode_submmessage_encrypt(factory, encoded_submsg, plain_submsg, session, protection_kind, writer_crypto_list, NULL, false, ex);

  CRYPTO_OBJECT_RELEASE(session);

  return result;
}

static bool
check_reader_specific_mac(
    dds_security_crypto_key_factory *factory,
    struct secure_prefix *prefix,
    struct secure_postfix *postfix,
    CryptoObjectKind_t kind,
    DDS_Security_Handle rmt_handle,
    const char *context,
    DDS_Security_SecurityException *ex)
{
  bool result = false;
  master_key_material *keymat = NULL;
  crypto_data_t data = { .base = postfix->common_mac.data, .length = CRYPTO_HMAC_SIZE };
  uint32_t index;
  crypto_session_key_t key;
  crypto_hmac_t *href = NULL;
  crypto_hmac_t hmac;

  if (postfix->length == 0)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_RECEIVER_SIGN_CODE, 0,
                "%s: message does not contain a receiver specific mac", context);
    return false;
  }

  if (!crypto_factory_get_specific_keymat(factory, kind, rmt_handle, postfix->recv_spec_mac, postfix->length, &index, &keymat))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_RECEIVER_SIGN_CODE, 0,
            "%s: message does not contain a known receiver specific key", context);
    goto check_failed;
  }

  href = &postfix->recv_spec_mac[index].receiver_mac;
  if (!href)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_RECEIVER_SIGN_CODE, 0,
        "%s: message does not contain receiver specific mac", context);
    goto check_failed;
  }

  if (!crypto_calculate_receiver_specific_key(&key, prefix->session_id, keymat->master_salt, keymat->master_receiver_specific_key, keymat->transformation_kind, ex))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_RECEIVER_SIGN_CODE, 0,
        "%s: failed to calculate receiver specific session key", context);
    goto check_failed;
  }

  if (!crypto_cipher_encrypt_data(&key, crypto_get_key_size(keymat->transformation_kind), &prefix->iv, 1, &data, NULL, &hmac, ex))
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

static DDS_Security_boolean encode_rtps_message_encrypt (
    dds_security_crypto_key_factory *factory,
    DDS_Security_OctetSeq *encoded_rtps_message,
    const DDS_Security_OctetSeq *plain_rtps_message,
    const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
    const DDS_Security_ParticipantCryptoHandleSeq *receiving_participant_crypto_list,
    int32_t *receiving_participant_crypto_list_index,
    DDS_Security_ParticipantCryptoHandle remote_id,
    DDS_Security_SecurityException *ex)
{
  session_key_material *session = NULL;
  DDS_Security_ProtectionKind protection_kind;
  crypto_buffer_t buffer;
  crypto_data_t encrypted_data;
  crypto_data_t plain_data[2];
  const size_t num_segs = sizeof(plain_data)/sizeof(plain_data[0]);
  struct crypto_header *header;
  struct crypto_footer *footer;
  Header_t *rtps_header;
  InfoSRC_t info_src;;
  DDS_Security_boolean result = false;
  crypto_hmac_t hmac;
  size_t size;
  uint32_t transform_kind, transform_id;

  DDSRT_STATIC_ASSERT(num_segs == 3);

  size_t rtps_body_size = plain_rtps_message->_length - RTPS_MESSAGE_HEADER_SIZE;
  size_t secure_body_plain_size = rtps_body_size + INFO_SRC_SIZE;

  if (secure_body_plain_size > INT_MAX)
  {
	  DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "encoding rtps message failed: length exceeds INT_MAX");
	  return false;
  }

  /* get local crypto and session*/
  if (!crypto_factory_get_local_participant_data_key_material(factory, sending_participant_crypto, &session, &protection_kind, ex))
    goto enc_rtps_inv_keymat;

  /* info_src and body */
  rtps_header = (Header_t *) plain_rtps_message->_buffer;
  init_info_src(&info_src, rtps_header);

  /* update sessionKey when needed */
  if (!crypto_session_key_material_update(session, (uint32_t)secure_body_plain_size, ex))
    goto enc_rtps_inv_keymat;

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

  size = RTPS_MESSAGE_HEADER_SIZE; /* RTPS Header */
  size += sizeof(struct crypto_header) + sizeof(struct crypto_footer) + ALIGN4(plain_rtps_message->_length);
  size += receiving_participant_crypto_list->_length * sizeof(struct receiver_specific_mac);
  size += sizeof(info_src); /* INFO_SRC */

  if (is_encryption_required(session->master_key_material->transformation_kind))
    size += sizeof(struct crypto_body);

  /* allocate a buffer to store the encoded message */
  crypto_buffer_init(&buffer, size);

  plain_data[0].base = (unsigned char *)&info_src;
  plain_data[0].length = sizeof(info_src);
  plain_data[1].base = (unsigned char *)(rtps_header + 1);
  plain_data[1].length = plain_rtps_message->_length - RTPS_MESSAGE_HEADER_SIZE;

  /* increment init_vector_suffix */
  session->init_vector_suffix++;

  transform_kind = session->master_key_material->transformation_kind;
  transform_id = session->master_key_material->sender_key_id;

  /* copy the rtps header to the encryption buffer */
  (void)add_rtps_header(&buffer, rtps_header);

  header = add_crypto_header(&buffer, SMID_SRTPS_PREFIX, transform_kind, transform_id, session->id, session->init_vector_suffix);

  if (is_encryption_required(transform_kind))
  {
    struct crypto_body *body = add_crypto_body(&buffer, secure_body_plain_size);

    encrypted_data.base = body->content.data;
    encrypted_data.length = secure_body_plain_size;

    /* encrypt message */
    if (!crypto_cipher_encrypt_data(&session->key, session->key_size, &header->prefix.iv, num_segs, plain_data, &encrypted_data, &hmac, ex))
      goto enc_rtps_fail_data;

    body->content.length = ddsrt_toBE4u((uint32_t)encrypted_data.length);
    if (encrypted_data.length > secure_body_plain_size)
    {
      size_t inc = (size_t)(encrypted_data.length - secure_body_plain_size);
      body->header.octetsToNextHeader = (uint16_t)(body->header.octetsToNextHeader + inc);
      crypto_buffer_expand(&buffer, inc);
    }
  }
  else if (is_authentication_required(transform_kind))
  {
    unsigned char *ptr = crypto_buffer_append(&buffer, secure_body_plain_size);
    /* the transformation_kind indicates only indicates authentication the determine HMAC */
    if (!crypto_cipher_encrypt_data(&session->key, session->key_size, &header->prefix.iv, num_segs, plain_data, NULL, &hmac, ex))
      goto enc_rtps_fail_data;

    /* copy submessage */
    memcpy(ptr, plain_data[0].base, plain_data[0].length);
    memcpy(ptr+plain_data[0].length, plain_data[1].base, plain_data[1].length);
  }
  else
  {
    goto enc_rtps_fail_data;
  }

  footer = add_crypto_footer(&buffer, SMID_SRTPS_POSTFIX);

  /* Set initial SEC_POSTFIX and CryptoFooter containing the common_mac
    * Note that the length of the postfix may increase when reader specific macs are added */
  footer->postfix.common_mac = hmac;
  footer->postfix.receiver_specific_macs._length = 0;
  if (has_origin_authentication(protection_kind))
  {
    if (receiving_participant_crypto_list->_length != 0)
    {
      if (!add_receiver_specific_mac(factory, &buffer, sending_participant_crypto, remote_id, ex))
        goto enc_rtps_fail_data;
      (*receiving_participant_crypto_list_index)++;
    }
  }
  else
  {
    *receiving_participant_crypto_list_index = (int32_t) receiving_participant_crypto_list->_length;
  }

  crypto_buffer_to_seq(&buffer, encoded_rtps_message);
  result = true;

enc_rtps_fail_data:
  if (!result)
  {
    ddsrt_free(buffer.contents);
    encoded_rtps_message->_buffer = NULL;
    encoded_rtps_message->_length = 0;
    encoded_rtps_message->_maximum = 0;
  }
  CRYPTO_OBJECT_RELEASE(session);
enc_rtps_inv_keymat:
  return result;
}

static DDS_Security_boolean
encode_rtps_message(dds_security_crypto_transform *instance,
                    DDS_Security_OctetSeq *encoded_message,
                    const DDS_Security_OctetSeq *plain_message,
                    const DDS_Security_ParticipantCryptoHandle remote_crypto,
                    const DDS_Security_ParticipantCryptoHandleSeq *local_crypto_list,
                    int32_t *index,
                    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_transform_impl *impl = (dds_security_crypto_transform_impl *)instance;
  dds_security_crypto_key_factory *factory = cryptography_get_crypto_key_factory(impl->crypto);
  DDS_Security_ParticipantCryptoHandle remote_id = 0;
  DDS_Security_boolean result = false;

  assert(encoded_message);
  assert((plain_message && plain_message->_length > 0 && plain_message->_buffer) || *index > 0);
  assert(encoded_message->_length > 0 || *index == 0);

  /* get remote participant handle */
  if (local_crypto_list->_length > 0)
    remote_id = local_crypto_list->_buffer[*index];

  /* When the receiving_participant_crypto_list_index is 0 then retrieve the key material of the writer */
  if (*index == 0)
    result = encode_rtps_message_encrypt (factory, encoded_message, plain_message, remote_crypto, local_crypto_list, index, remote_id, ex);
  else
  {
    crypto_buffer_t buffer;

    crypto_buffer_from_seq(&buffer, encoded_message);
    /* When the receiving_participant_crypto_list_index is not 0 then add a signature for the specific reader */
    result = add_receiver_specific_mac(factory, &buffer, remote_crypto, remote_id, ex);
    if (result)
    {
       (*index)++;
       crypto_buffer_to_seq(&buffer, encoded_message);
    }
  }

  return result;
}

static DDS_Security_boolean
decode_rtps_message(dds_security_crypto_transform *instance,
                    DDS_Security_OctetSeq *plain_buffer,
                    const DDS_Security_OctetSeq *encoded_buffer,
                    const DDS_Security_ParticipantCryptoHandle receiving_crypto,
                    const DDS_Security_ParticipantCryptoHandle sending_crypto,
                    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_transform_impl *impl = (dds_security_crypto_transform_impl *)instance;
  dds_security_crypto_key_factory *factory = cryptography_get_crypto_key_factory(impl->crypto);
  remote_session_info remote_session;
  struct encrypted_state estate;
  unsigned char *buffer = NULL;
  size_t buflen;
  crypto_data_t encoded_data = { .base = encoded_buffer->_buffer, .length = encoded_buffer->_length };
  crypto_data_t decoded_body;
  static const char *context = "decode_rtps_message";
  participant_key_material *pp_key_material;
  master_key_material *remote_key_material;
  DDS_Security_ProtectionKind remote_protection_kind;
  DDS_Security_boolean result = false;

  assert(encoded_buffer->_length > 0);
  assert(plain_buffer);

  if (encoded_buffer->_length > INT_MAX)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "decoding rtps message failed: length exceeds INT_MAX");
    return false;
  }

  /* split the encoded submessage in the corresponding parts */
  if (!split_encoded_rtps_message(&encoded_data, &estate))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "%s: invalid message", context);
    return false;
  }

  /* Retrieve key material from sending_participant_crypto and receiving_participant_crypto from factory */
  if (!crypto_factory_get_participant_crypto_tokens(factory, receiving_crypto, sending_crypto, &pp_key_material, &remote_key_material, &remote_protection_kind, ex))
    return false;
  else if (remote_key_material == NULL)
  {
    CRYPTO_OBJECT_RELEASE(pp_key_material);
    return false;
  }

  if (has_origin_authentication(remote_protection_kind))
  { /* default governance value */
    if (!check_reader_specific_mac(factory, &estate.prefix, &estate.postfix, CRYPTO_OBJECT_KIND_REMOTE_CRYPTO, sending_crypto, context, ex))
      goto fail_reader_mac;
  }

  /* calculate the session key */
  if (!initialize_remote_session_info(&remote_session, &estate.prefix, remote_key_material->master_salt, remote_key_material->master_sender_key, remote_key_material->transformation_kind, ex))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "%s: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE, context);
    goto fail_reader_mac;
  }

  buflen = estate.body.data.length + RTPS_MESSAGE_HEADER_SIZE;
  buffer = ddsrt_malloc(buflen);
  memcpy(buffer, encoded_data.base, RTPS_MESSAGE_HEADER_SIZE);

  decoded_body.base = buffer + RTPS_MESSAGE_HEADER_SIZE;
  decoded_body.length = estate.body.data.length;

  if (is_encryption_required(estate.prefix.transform_kind))
  {
    if (!is_encryption_expected(remote_protection_kind))
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "%s: message is encrypted, which is unexpected", context);
      goto fail_decrypt;
    }

    /* When the CryptoHeader indicates that encryption is performed then decrypt the submessage body */
    /* check if the body is a SEC_BODY submessage */
    if (estate.body.id != SMID_SEC_BODY)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "%s: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE, context);
      goto fail_decrypt;
    }

    if (!crypto_cipher_decrypt_data(&remote_session, &estate.prefix.iv, 1, &estate.body.data, &decoded_body, &estate.postfix.common_mac, ex))
      goto fail_decrypt;
  }
  else if (is_authentication_required(estate.prefix.transform_kind))
  {
    if (!is_authentication_expected(remote_protection_kind))
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "%s: message is signed, which is unexpected", context);
      goto fail_decrypt;
    }
    /* When the CryptoHeader indicates that authentication is performed then calculate the HMAC */
    if (!crypto_cipher_decrypt_data(&remote_session, &estate.prefix.iv,  1, &estate.body.data, NULL, &estate.postfix.common_mac, ex))
      goto fail_decrypt;
    memcpy(decoded_body.base, estate.body.data.base, estate.body.data.length);
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_CODE, 0,
        "%s: " DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_MESSAGE, context);
    goto fail_decrypt;
  }

  plain_buffer->_buffer = buffer;
  plain_buffer->_length = plain_buffer->_maximum = (uint32_t)buflen;

  result = true;

fail_decrypt:
  if (!result)
    ddsrt_free(buffer);
fail_reader_mac:
  CRYPTO_OBJECT_RELEASE(pp_key_material);
  return result;
}

static DDS_Security_boolean
preprocess_secure_submsg(
    dds_security_crypto_transform *instance,
    DDS_Security_DatawriterCryptoHandle *writer_crypto,
    DDS_Security_DatareaderCryptoHandle *reader_crypto,
    DDS_Security_SecureSubmessageCategory_t *category,
    const DDS_Security_OctetSeq *encoded_submessage,
    const DDS_Security_ParticipantCryptoHandle receiving_crypto,
    const DDS_Security_ParticipantCryptoHandle sending_crypto,
    DDS_Security_SecurityException *ex)
{
  dds_security_crypto_transform_impl *impl = (dds_security_crypto_transform_impl *)instance;
  dds_security_crypto_key_factory *factory = cryptography_get_crypto_key_factory(impl->crypto);
  DDS_Security_Handle remote_handle;
  DDS_Security_Handle local_handle;
  DDS_Security_boolean result;
  struct secure_prefix prefix;

  assert(writer_crypto);
  assert(reader_crypto);
  assert(category);
  assert(encoded_submessage && encoded_submessage->_length > 0);

  if (!get_secure_prefix(encoded_submessage, &prefix))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "preprocess_secure_submsg: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE);
    return false;
  }

  /* Lookup the end point information associated with the transform id */
  result = crypto_factory_get_endpoint_relation(
      factory, receiving_crypto, sending_crypto,
      prefix.transform_id, &remote_handle, &local_handle, category, ex);

  if (result)
  {
    if (*category == DDS_SECURITY_DATAWRITER_SUBMESSAGE)
    {
      *writer_crypto = (DDS_Security_DatawriterCryptoHandle)remote_handle;
      *reader_crypto = (DDS_Security_DatareaderCryptoHandle)local_handle;
    }
    else
    {
      *reader_crypto = (DDS_Security_DatareaderCryptoHandle)remote_handle;
      *writer_crypto = (DDS_Security_DatawriterCryptoHandle)local_handle;
    }
  }

  return result;
}

static DDS_Security_boolean
decode_submessage(
    dds_security_crypto_key_factory *factory,
    DDS_Security_OctetSeq *plain_submsg,
    const DDS_Security_OctetSeq *encoded_submsg,
    const DDS_Security_CryptoHandle local_crypto,
    const DDS_Security_CryptoHandle remote_crypto,
    CryptoObjectKind_t kind,
    const char *context,
    DDS_Security_SecurityException *ex)
{
  DDS_Security_boolean result = false;
  master_key_material *keymat;
  crypto_data_t plain_data;
  DDS_Security_ProtectionKind protection_kind;
  remote_session_info remote_session;
  struct encrypted_state est;

  assert(encoded_submsg && encoded_submsg->_length > 0 && encoded_submsg->_buffer);
  assert(plain_submsg);

  if (encoded_submsg->_length > INT_MAX)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "decoding submessage failed: length exceeds INT_MAX");
    return false;
  }

  /* split the encoded submessage in the corresponding parts */
  if (!split_encoded_submessage(encoded_submsg, &est))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "%s: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE, context);
    return false;
  }

  if (kind == CRYPTO_OBJECT_KIND_REMOTE_WRITER_CRYPTO)
  {
    /* Retrieve key material from sending_datawriter_crypto from factory */
    if (!crypto_factory_get_remote_writer_key_material(factory, local_crypto, remote_crypto, est.prefix.transform_id, &keymat, &protection_kind, NULL, ex))
      return false;
  }
  else
  {
    /* Retrieve key material from sending_datareader_crypto from factory */
     if (!crypto_factory_get_remote_reader_key_material(factory, local_crypto, remote_crypto, est.prefix.transform_id, &keymat, &protection_kind, ex))
       return false;
  }

  if (has_origin_authentication(protection_kind) && !check_reader_specific_mac(factory, &est.prefix, &est.postfix, kind, remote_crypto, context, ex))
    goto fail_mac;

  /* calculate the session key */
  if (!initialize_remote_session_info(&remote_session, &est.prefix, keymat->master_salt, keymat->master_sender_key, keymat->transformation_kind, ex))
    goto fail_mac;

  plain_data.base = ddsrt_malloc(est.body.data.length);
  plain_data.length = est.body.data.length;

  if (is_encryption_required(est.prefix.transform_kind))
  {
    if (!is_encryption_expected(protection_kind))
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "%s: submessage is encrypted, which is unexpected (%d vs %d)",
          context, (int)est.prefix.transform_kind, (int)protection_kind);
      goto fail_decrypt;
    }
    /* When the CryptoHeader indicates that encryption is performed then decrypt the submessage body */
    /* check if the body is a SEC_BODY submessage */
    if (est.body.id!= SMID_SEC_BODY)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "%s: " DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_MESSAGE, context);
      goto fail_decrypt;
    }

    if (!crypto_cipher_decrypt_data(&remote_session, &est.prefix.iv, 1, &est.body.data, &plain_data, &est.postfix.common_mac, ex))
      goto fail_decrypt;
  }
  else if (is_authentication_required(est.prefix.transform_kind))
  {
    if (!is_authentication_expected(protection_kind))
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "%s: submessage is signed, which is unexpected", context);
      goto fail_decrypt;
    }
    assert(est.prefix.transform_id != 0);
    /* When the CryptoHeader indicates that authentication is performed then calculate the HMAC */
    if (!crypto_cipher_decrypt_data(&remote_session, &est.prefix.iv, 1, &est.body.data, NULL, &est.postfix.common_mac, ex))
      goto fail_decrypt;

    memcpy(plain_data.base, est.body.data.base, est.body.data.length);
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_CODE, 0,
        "%s: " DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_MESSAGE, context);
    goto fail_decrypt;
  }

  plain_submsg->_buffer = plain_data.base;
  plain_submsg->_length = plain_submsg->_maximum = (uint32_t)plain_data.length;

  result = true;

fail_decrypt:
  if (!result)
    ddsrt_free(plain_data.base);
fail_mac:
  CRYPTO_OBJECT_RELEASE(keymat);
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
  dds_security_crypto_key_factory *factory = cryptography_get_crypto_key_factory(impl->crypto);

  return decode_submessage(factory, plain_submsg, encoded_submsg, reader_crypto, writer_crypto, CRYPTO_OBJECT_KIND_REMOTE_WRITER_CRYPTO, "decode_datawriter_submessage", ex);
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
  dds_security_crypto_key_factory *factory = cryptography_get_crypto_key_factory(impl->crypto);

  return decode_submessage(factory, plain_submsg, encoded_submsg, writer_crypto, reader_crypto, CRYPTO_OBJECT_KIND_REMOTE_READER_CRYPTO, "decode_datareader_submessage", ex);
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
  dds_security_crypto_key_factory *factory = cryptography_get_crypto_key_factory(impl->crypto);
  crypto_data_t plain_data;
  DDS_Security_BasicProtectionKind basic_protection_kind;
  master_key_material *writer_master_key;
  remote_session_info remote_session;
  struct encrypted_state estate;
  DDS_Security_boolean result = false;

  DDSRT_UNUSED_ARG(inline_qos);

  assert(encoded_buffer && encoded_buffer->_buffer && encoded_buffer->_length > 0);
  assert(plain_buffer);

  if (encoded_buffer->_length > INT_MAX)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_CIPHER_ERROR, 0, "decoding payload failed: length exceeds INT_MAX");
    return false;
  }

  /* determine CryptoHeader, CryptoContent and CryptoFooter*/
  if (!split_encoded_serialized_payload(encoded_buffer, &estate))
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "decode_serialized_payload: Invalid syntax of encoded payload");
    return false;
  }

  if (estate.postfix.length != 0)
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
        "decode_serialized_payload: Received specific_macs");
    return false;
  }

  /* Retrieve key material from sending_datawriter_crypto from factory */
  if (!crypto_factory_get_remote_writer_key_material(factory, reader_id, writer_id, estate.prefix.transform_id, &writer_master_key, NULL, &basic_protection_kind, ex))
    return false;

  plain_data.base = ddsrt_malloc(estate.body.data.length);
  plain_data.length = estate.body.data.length;

  /* calculate the session key */
  if (!initialize_remote_session_info(&remote_session, &estate.prefix, writer_master_key->master_salt, writer_master_key->master_sender_key, writer_master_key->transformation_kind, ex))
    goto fail_decrypt;

  /*
   * Depending on encryption, the payload part between Header and Footer is
   * either CryptoContent or the original plain payload.
   * See spec: 9.5.3.3.4.4 Result from encode_serialized_payload
   */
  if (is_encryption_required(estate.prefix.transform_kind))
  {
    /* Is encryption expected? */
    if (basic_protection_kind != DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "decode_serialized_payload: payload is encrypted, which is unexpected");
      goto fail_decrypt;
    }

    if (!crypto_cipher_decrypt_data(&remote_session, &estate.prefix.iv, 1, &estate.body.data, &plain_data, &estate.postfix.common_mac, ex))
      goto fail_decrypt;
  }
  else if (is_authentication_required(estate.prefix.transform_kind))
  {
    /* Is signing expected? */
    if (basic_protection_kind != DDS_SECURITY_BASICPROTECTION_KIND_SIGN)
    {
      DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_ARGUMENT_CODE, 0,
          "decode_serialized_payload: payload is signed, which is unexpected");
      goto fail_decrypt;
    }
    /* When the CryptoHeader indicates that authentication is performed then calculate the HMAC */
    if (!crypto_cipher_decrypt_data(&remote_session, &estate.prefix.iv, 1, &estate.body.data, NULL, &estate.postfix.common_mac, ex))
      goto fail_decrypt;
    memcpy(plain_data.base, estate.body.data.base,  estate.body.data.length);
  }
  else
  {
    DDS_Security_Exception_set(ex, DDS_CRYPTO_PLUGIN_CONTEXT, DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_CODE, 0,
        "decode_serialized_payload: " DDS_SECURITY_ERR_INVALID_CRYPTO_TRANSFORMATION_MESSAGE);
    goto fail_decrypt;
  }

  plain_buffer->_buffer = plain_data.base;
  plain_buffer->_length = plain_buffer->_maximum = (uint32_t)plain_data.length;

  result =  true;

fail_decrypt:
  if (!result) ddsrt_free(plain_data.base);
  CRYPTO_OBJECT_RELEASE(writer_master_key);
  return result;
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
