// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef SECURITY_CORE_TEST_CRYPTO_WRAPPER_H_
#define SECURITY_CORE_TEST_CRYPTO_WRAPPER_H_

#include "dds/ddsrt/circlist.h"
#include "dds/ddsi/ddsi_domaingv.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/dds_security_api_defs.h"
#include "dds/security/cryptography_wrapper_export.h"

#define CRYPTO_TOKEN_MAXCOUNT 10
#define CRYPTO_TOKEN_SIZE 256

struct dds_security_cryptography_impl;

enum crypto_tokens_type {
  LOCAL_PARTICIPANT_TOKENS,
  LOCAL_WRITER_TOKENS,
  LOCAL_READER_TOKENS,
  REMOTE_PARTICIPANT_TOKENS,
  REMOTE_WRITER_TOKENS,
  REMOTE_READER_TOKENS,
  TOKEN_TYPE_INVALID
};

struct crypto_token_data {
  struct ddsrt_circlist_elem e;
  enum crypto_tokens_type type;
  DDS_Security_ParticipantCryptoHandle local_handle;
  DDS_Security_ParticipantCryptoHandle remote_handle;
  uint32_t n_tokens;
  unsigned char data[CRYPTO_TOKEN_MAXCOUNT][CRYPTO_TOKEN_SIZE];
  size_t data_len[CRYPTO_TOKEN_MAXCOUNT];
};

enum crypto_encode_decode_fn {
  ENCODE_DATAWRITER_SUBMESSAGE,
  ENCODE_DATAREADER_SUBMESSAGE,
  DECODE_DATAWRITER_SUBMESSAGE,
  DECODE_DATAREADER_SUBMESSAGE
};

struct crypto_encode_decode_data {
  struct ddsrt_circlist_elem e;
  enum crypto_encode_decode_fn function;
  DDS_Security_long_long handle;
  uint32_t count;
};

SECURITY_EXPORT void set_protection_kinds(
  struct dds_security_cryptography_impl * impl,
  DDS_Security_ProtectionKind rtps_protection_kind,
  DDS_Security_ProtectionKind metadata_protection_kind,
  DDS_Security_BasicProtectionKind payload_protection_kind);
SECURITY_EXPORT void set_encrypted_secret(struct dds_security_cryptography_impl * impl, const char * secret);
SECURITY_EXPORT void set_disc_protection_kinds(
  struct dds_security_cryptography_impl * impl,
  DDS_Security_ProtectionKind disc_protection_kind,
  DDS_Security_ProtectionKind liveliness_protection_kind);
SECURITY_EXPORT void set_entity_data_secret(struct dds_security_cryptography_impl * impl, const char * pp_secret, const char * groupdata_secret, const char * ep_secret);
SECURITY_EXPORT void set_force_plain_data(struct dds_security_cryptography_impl * impl, DDS_Security_DatawriterCryptoHandle wr_handle, bool plain_rtps, bool plain_submsg, bool plain_payload);

SECURITY_EXPORT const char *get_crypto_token_type_str (enum crypto_tokens_type type);
SECURITY_EXPORT struct ddsrt_circlist * get_crypto_tokens (struct dds_security_cryptography_impl * impl);
SECURITY_EXPORT struct crypto_token_data * find_crypto_token (struct dds_security_cryptography_impl * impl, enum crypto_tokens_type type, unsigned char * data, size_t data_len);
SECURITY_EXPORT struct crypto_encode_decode_data * get_encode_decode_log (struct dds_security_cryptography_impl * impl, enum crypto_encode_decode_fn function, DDS_Security_long_long handle);

/* Init in all-ok mode: all functions return success without calling the actual plugin */
SECURITY_EXPORT int init_test_cryptography_all_ok(const char *argument, void **context, struct ddsi_domaingv *gv);
SECURITY_EXPORT int finalize_test_cryptography_all_ok(void *context);

/* Init in missing function mode: one of the function pointers is null */
SECURITY_EXPORT int init_test_cryptography_missing_func(const char *argument, void **context, struct ddsi_domaingv *gv);
SECURITY_EXPORT int finalize_test_cryptography_missing_func(void *context);

/* Init in wrapper mode */
SECURITY_EXPORT int init_test_cryptography_wrapped(const char *argument, void **context, struct ddsi_domaingv *gv);
SECURITY_EXPORT int finalize_test_cryptography_wrapped(void *context);

/* Init in store-token mode (stores all exchanged security tokens) */
SECURITY_EXPORT int32_t init_test_cryptography_store_tokens(const char *argument, void **context, struct ddsi_domaingv *gv);
SECURITY_EXPORT int32_t finalize_test_cryptography_store_tokens(void *context);

/* Init in plain-data mode (force plain data for payload, submsg and/or rtps) */
SECURITY_EXPORT int init_test_cryptography_plain_data(const char *argument, void **context, struct ddsi_domaingv *gv);
SECURITY_EXPORT int finalize_test_cryptography_plain_data(void *context);

#endif /* SECURITY_CORE_TEST_CRYPTO_WRAPPER_H_ */
