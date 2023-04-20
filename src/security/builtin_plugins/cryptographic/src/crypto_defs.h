// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef CRYPTO_DEFS_H
#define CRYPTO_DEFS_H

#include "dds/security/core/dds_security_types.h"
#include "dds/security/dds_security_api.h"


#define DDS_CRYPTO_PLUGIN_CONTEXT "Cryptographic"

#define CRYPTO_HMAC_SIZE 16
#define CRYPTO_KEY_SIZE_128 16
#define CRYPTO_KEY_SIZE_256 32
#define CRYPTO_KEY_SIZE_MAX CRYPTO_KEY_SIZE_256

#define CRYPTO_SESSION_ID_SIZE 4
#define CRYPTO_INIT_VECTOR_SUFFIX_SIZE 8
#define CRYPTO_INIT_VECTOR_SIZE (CRYPTO_SESSION_ID_SIZE + CRYPTO_INIT_VECTOR_SUFFIX_SIZE)
#define CRYPTO_CIPHER_BLOCK_SIZE 16


typedef struct crypto_session_key_t
{
  unsigned char data[CRYPTO_KEY_SIZE_MAX];
} crypto_session_key_t;

typedef struct crypto_hmac_t
{
  unsigned char data[CRYPTO_HMAC_SIZE];
} crypto_hmac_t;

typedef enum RTPS_Message_Type
{
  /** The Constant PAD. */
  RTPS_Message_Type_PAD = 0x01,

  /** The Constant ACKNACK. */
  RTPS_Message_Type_ACKNACK = 0x06,

  /** The Constant HEARTBEAT. */
  RTPS_Message_Type_HEARTBEAT = 0x07,

  /** The Constant GAP. */
  RTPS_Message_Type_GAP = 0x08,

  /** The Constant INFO_TS. */
  RTPS_Message_Type_INFO_TS = 0x09,

  /** The Constant INFO_SRC. */
  RTPS_Message_Type_INFO_SRC = 0x0c,

  /** The Constant INFO_REPLY_IP4. */
  RTPS_Message_Type_INFO_REPLY_IP4 = 0x0d,

  /** The Constant INFO_DST. */
  RTPS_Message_Type_INFO_DST = 0x0e,

  /** The Constant INFO_REPLY. */
  RTPS_Message_Type_INFO_REPLY = 0x0f,

  /** The Constant NACK_FRAG. */
  RTPS_Message_Type_NACK_FRAG = 0x12,

  /** The Constant HEARTBEAT_FRAG. */
  RTPS_Message_Type_HEARTBEAT_FRAG = 0x13,

  /** The Constant DATA. */
  RTPS_Message_Type_DATA = 0x15,

  /** The Constant DATA_FRAG. */
  RTPS_Message_Type_DATA_FRAG = 0x16,

  /** The Constant MSG_LENGTH. */
  RTPS_Message_Type_MSG_LEN = 0x81,

  /** The Constant MSG_ENTITY_ID. */
  RTPS_Message_Type_MSG_ENTITY_ID = 0x82,

  /** The Constant INFO_LASTHB. */
  RTPS_Message_Type_INFO_LASTHB = 0x83,

  /** The Constant SEC_PREFIX. */
  RTPS_Message_Type_SEC_BODY = 0x30,

  /** The Constant SEC_PREFIX. */
  RTPS_Message_Type_SEC_PREFIX = 0x31,

  /** The Constant SEC_POSTFIX. */
  RTPS_Message_Type_SEC_POSTFIX = 0x32,

  /** The Constant SRTPS_PREFIX. */
  RTPS_Message_Type_SRTPS_PREFIX = 0x33,

  /** The Constant SRTPS_POSTFIX. */
  RTPS_Message_Type_SRTPS_POSTFIX = 0x34
} RTPS_Message_Type;

struct init_vector {
  unsigned char u[CRYPTO_INIT_VECTOR_SIZE];
};

struct receiver_specific_mac
{
  DDS_Security_CryptoTransformKeyId receiver_mac_key_id;
  crypto_hmac_t receiver_mac;
};

typedef struct const_tainted_crypto_data {
  const unsigned char *base;
  size_t length;
} const_tainted_crypto_data_t;

typedef struct tainted_crypto_data {
  unsigned char *base;
  size_t length;
} tainted_crypto_data_t;

typedef struct trusted_crypto_data {
  tainted_crypto_data_t x;
} trusted_crypto_data_t;

#endif /* CRYPTO_DEFS_H */
