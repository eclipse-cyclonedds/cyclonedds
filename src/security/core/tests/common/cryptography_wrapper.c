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
#include "dds/security/dds_security_api_defs.h"
#include "dds/security/core/dds_security_utils.h"
#include "crypto_tokens.h"
#include "cryptography_wrapper.h"

int32_t init_crypto(const char *argument, void **context, struct ddsi_domaingv *gv);
int32_t finalize_crypto(void *context);

enum crypto_plugin_mode {
  PLUGIN_MODE_ALL_OK,
  PLUGIN_MODE_MISSING_FUNC,
  PLUGIN_MODE_WRAPPED,
  PLUGIN_MODE_TOKEN_LOG,
  PLUGIN_MODE_PLAIN_DATA
};

struct dds_security_crypto_key_exchange_impl {
  struct dds_security_crypto_key_exchange base;
  struct dds_security_crypto_key_exchange *instance;
  struct dds_security_cryptography_impl *parent;
};

struct dds_security_crypto_key_factory_impl {
  struct dds_security_crypto_key_factory base;
  struct dds_security_crypto_key_factory *instance;
  struct dds_security_cryptography_impl *parent;
};

struct dds_security_crypto_transform_impl {
  struct dds_security_crypto_transform base;
  struct dds_security_crypto_transform *instance;
  struct dds_security_cryptography_impl *parent;
};

struct dds_security_cryptography_impl {
  struct dds_security_cryptography base;
  struct dds_security_cryptography *instance;
  struct dds_security_crypto_transform_impl transform_wrap;
  struct dds_security_crypto_key_factory_impl factory_wrap;
  struct dds_security_crypto_key_exchange_impl exchange_wrap;
  enum crypto_plugin_mode mode;
  bool protection_kinds_set;
  bool disc_protection_kinds_set;
  DDS_Security_ProtectionKind rtps_protection_kind;
  DDS_Security_ProtectionKind metadata_protection_kind;
  DDS_Security_BasicProtectionKind payload_protection_kind;
  DDS_Security_ProtectionKind disc_protection_kind;
  DDS_Security_ProtectionKind liveliness_protection_kind;
  const char * pp_secret;
  const char * groupdata_secret;
  const char * ep_secret;
  const char * encrypted_secret;
  ddsrt_mutex_t token_data_lock;
  struct ddsrt_circlist token_data_list;
  ddsrt_mutex_t encode_decode_log_lock;
  struct ddsrt_circlist encode_decode_log;
  bool force_plain_rtps;
  bool force_plain_submsg;
  bool force_plain_payload;
  DDS_Security_DatawriterCryptoHandle force_plain_sender_handle;
};

static DDS_Security_ParticipantCryptoHandle g_local_participant_handle = 0;
static ddsrt_once_t lock_inited = DDSRT_ONCE_INIT;
static ddsrt_mutex_t g_print_token_lock;

void set_protection_kinds(
  struct dds_security_cryptography_impl * impl,
  DDS_Security_ProtectionKind rtps_protection_kind,
  DDS_Security_ProtectionKind metadata_protection_kind,
  DDS_Security_BasicProtectionKind payload_protection_kind)
{
  assert(impl);
  impl->rtps_protection_kind = rtps_protection_kind;
  impl->metadata_protection_kind = metadata_protection_kind;
  impl->payload_protection_kind = payload_protection_kind;
  impl->protection_kinds_set = true;
}

void set_encrypted_secret(struct dds_security_cryptography_impl * impl, const char * secret)
{
  assert(impl);
  impl->encrypted_secret = secret;
}

void set_disc_protection_kinds(
  struct dds_security_cryptography_impl * impl,
  DDS_Security_ProtectionKind disc_protection_kind,
  DDS_Security_ProtectionKind liveliness_protection_kind)
{
  assert(impl);
  impl->disc_protection_kind = disc_protection_kind;
  impl->liveliness_protection_kind = liveliness_protection_kind;
  impl->disc_protection_kinds_set = true;
}

void set_entity_data_secret(struct dds_security_cryptography_impl * impl, const char * pp_secret, const char * groupdata_secret, const char * ep_secret)
{
  assert(impl);
  impl->pp_secret = pp_secret;
  impl->groupdata_secret = groupdata_secret;
  impl->ep_secret = ep_secret;
}

void set_force_plain_data(struct dds_security_cryptography_impl * impl, DDS_Security_DatawriterCryptoHandle handle, bool plain_rtps, bool plain_submsg, bool plain_payload)
{
  assert (impl);
  assert (impl->mode == PLUGIN_MODE_PLAIN_DATA);
  impl->force_plain_rtps = plain_rtps;
  impl->force_plain_submsg = plain_submsg;
  impl->force_plain_payload = plain_payload;
  impl->force_plain_sender_handle = handle;
}

static bool check_crypto_tokens(const DDS_Security_DataHolderSeq *tokens)
{
    bool result = true;

    if (tokens->_length == 0 || tokens->_buffer == NULL)
      return false;

    for (uint32_t i = 0; result && (i < tokens->_length); i++)
    {
      result = (tokens->_buffer[i].class_id != NULL &&
        strcmp(DDS_CRYPTOTOKEN_CLASS_ID, tokens->_buffer[i].class_id) == 0 &&
        tokens->_buffer[i].binary_properties._length == 1 &&
        tokens->_buffer[i].binary_properties._buffer != NULL &&
        tokens->_buffer[i].binary_properties._buffer[0].name != NULL &&
        strcmp(DDS_CRYPTOTOKEN_PROP_KEYMAT, tokens->_buffer[i].binary_properties._buffer[0].name) == 0 &&
        tokens->_buffer[i].binary_properties._buffer[0].value._length > 0 &&
        tokens->_buffer[i].binary_properties._buffer[0].value._buffer != NULL);
    }
    return result;
}

const char *get_crypto_token_type_str (enum crypto_tokens_type type)
{
  switch (type)
  {
  case LOCAL_PARTICIPANT_TOKENS: return "LOCAL_PARTICIPANT_TOKENS";
  case LOCAL_WRITER_TOKENS: return "LOCAL_WRITER_TOKENS";
  case LOCAL_READER_TOKENS: return "LOCAL_READER_TOKENS";
  case REMOTE_PARTICIPANT_TOKENS: return "REMOTE_PARTICIPANT_TOKENS";
  case REMOTE_WRITER_TOKENS: return "REMOTE_WRITER_TOKENS";
  case REMOTE_READER_TOKENS: return "REMOTE_READER_TOKENS";
  default: assert (0); return "";
  }
}

static void print_tokens (enum crypto_tokens_type type, const DDS_Security_ParticipantCryptoHandle lch, const DDS_Security_ParticipantCryptoHandle rch,
    const DDS_Security_ParticipantCryptoTokenSeq *tokens)
{
  ddsrt_mutex_lock (&g_print_token_lock);
  printf ("Token type %s, local %"PRIx64" / remote %"PRIx64", count: %u\n", get_crypto_token_type_str (type), lch, rch, tokens->_length);
  for (uint32_t i = 0; i < tokens->_length; i++)
  {
    printf ("- token: ");
    for (uint32_t j = 0; j < tokens->_buffer[i].binary_properties._buffer[0].value._length && j < 32; j++)
      printf ("%02x", tokens->_buffer[i].binary_properties._buffer[0].value._buffer[j]);
    printf ("\n");
  }
  printf ("\n");
  ddsrt_mutex_unlock (&g_print_token_lock);
}

static void add_tokens (struct ddsrt_circlist *list, enum crypto_tokens_type type,
    const DDS_Security_ParticipantCryptoHandle lch, const DDS_Security_ParticipantCryptoHandle rch,
    const DDS_Security_ParticipantCryptoTokenSeq *tokens)
{
  struct crypto_token_data *token_data = ddsrt_malloc (sizeof (*token_data));
  token_data->type = type;
  token_data->local_handle = lch;
  token_data->remote_handle = rch;
  token_data->n_tokens = tokens->_length;
  assert (tokens->_length <= CRYPTO_TOKEN_MAXCOUNT);
  for (uint32_t i = 0; i < tokens->_length; i++)
  {
    size_t len = tokens->_buffer[i].binary_properties._buffer[0].value._length;
    assert (len <= CRYPTO_TOKEN_SIZE);
    memcpy (token_data->data[i], tokens->_buffer[i].binary_properties._buffer[0].value._buffer, len);
    token_data->data_len[i] = len;
  }
  ddsrt_circlist_append(list, &token_data->e);
}

static void store_tokens (struct dds_security_crypto_key_exchange_impl *impl, enum crypto_tokens_type type, const DDS_Security_ParticipantCryptoHandle lch,
    const DDS_Security_ParticipantCryptoHandle rch, const DDS_Security_ParticipantCryptoTokenSeq *tokens)
{
  if (!check_crypto_tokens ((const DDS_Security_DataHolderSeq *) tokens))
  {
    printf ("%d ERROR\n", type);
    return;
  }

  ddsrt_mutex_lock (&impl->parent->token_data_lock);
  add_tokens (&impl->parent->token_data_list, type, lch, rch, tokens);
  ddsrt_mutex_unlock (&impl->parent->token_data_lock);

  print_tokens (type, lch, rch, tokens);
}

struct ddsrt_circlist * get_crypto_tokens (struct dds_security_cryptography_impl * impl)
{
  struct ddsrt_circlist *tokens = ddsrt_malloc (sizeof (*tokens));
  ddsrt_circlist_init (tokens);

  ddsrt_mutex_lock (&impl->token_data_lock);
  if (ddsrt_circlist_isempty (&impl->encode_decode_log))
  {
    ddsrt_mutex_unlock (&impl->token_data_lock);
    return tokens;
  }

  struct ddsrt_circlist_elem *elem0 = ddsrt_circlist_oldest (&impl->token_data_list), *elem = elem0;
  do
  {
    struct crypto_token_data *elem_data = DDSRT_FROM_CIRCLIST (struct crypto_token_data, e, elem);
    struct crypto_token_data *token_data = ddsrt_malloc (sizeof (*token_data));
    memcpy (token_data, elem_data, sizeof (*token_data));
    ddsrt_circlist_append (tokens, &token_data->e);
    elem = elem->next;
  } while (elem != elem0);
  ddsrt_mutex_unlock (&impl->token_data_lock);

  return tokens;
}

struct crypto_token_data * find_crypto_token (struct dds_security_cryptography_impl * impl, enum crypto_tokens_type type, unsigned char * data, size_t data_len)
{
  assert (data_len <= CRYPTO_TOKEN_SIZE);
  ddsrt_mutex_lock (&impl->token_data_lock);
  if (ddsrt_circlist_isempty (&impl->encode_decode_log))
  {
    ddsrt_mutex_unlock (&impl->token_data_lock);
    return NULL;
  }
  struct ddsrt_circlist_elem *elem0 = ddsrt_circlist_oldest (&impl->token_data_list), *elem = elem0;
  do
  {
    struct crypto_token_data *elem_data = DDSRT_FROM_CIRCLIST (struct crypto_token_data, e, elem);
    if (elem_data->type == type)
    {
      for (uint32_t i = 0; i < elem_data->n_tokens; i++)
      {
        size_t len = elem_data->data_len[i];
        assert (len <= CRYPTO_TOKEN_SIZE);
        if (!memcmp (data, elem_data->data[i], data_len < len ? data_len : len))
        {
          ddsrt_mutex_unlock (&impl->token_data_lock);
          return elem_data;
        }
      }
    }
    elem = elem->next;
  } while (elem != elem0);
  ddsrt_mutex_unlock (&impl->token_data_lock);
  return NULL;
}

static void log_encode_decode (struct dds_security_cryptography_impl * impl, enum crypto_encode_decode_fn function, DDS_Security_long_long handle)
{
  ddsrt_mutex_lock (&impl->encode_decode_log_lock);
  if (!ddsrt_circlist_isempty (&impl->encode_decode_log))
  {
    struct ddsrt_circlist_elem *elem0 = ddsrt_circlist_oldest (&impl->encode_decode_log), *elem = elem0;
    do
    {
      struct crypto_encode_decode_data *data = DDSRT_FROM_CIRCLIST (struct crypto_encode_decode_data, e, elem);
      if (data->function == function && data->handle == handle)
      {
        data->count++;
        ddsrt_mutex_unlock (&impl->encode_decode_log_lock);
        return;
      }
      elem = elem->next;
    } while (elem != elem0);
  }
  /* add new entry */
  struct crypto_encode_decode_data *new_data = ddsrt_malloc (sizeof (*new_data));
  new_data->function = function;
  new_data->handle = handle;
  new_data->count = 1;
  ddsrt_circlist_append(&impl->encode_decode_log, &new_data->e);
  ddsrt_mutex_unlock (&impl->encode_decode_log_lock);
}

struct crypto_encode_decode_data * get_encode_decode_log (struct dds_security_cryptography_impl * impl, enum crypto_encode_decode_fn function, DDS_Security_long_long handle)
{
  ddsrt_mutex_lock (&impl->encode_decode_log_lock);
  if (!ddsrt_circlist_isempty (&impl->encode_decode_log))
  {
    struct ddsrt_circlist_elem *elem0 = ddsrt_circlist_oldest (&impl->encode_decode_log), *elem = elem0;
    do
    {
      struct crypto_encode_decode_data *data = DDSRT_FROM_CIRCLIST (struct crypto_encode_decode_data, e, elem);
      if (data->function == function && data->handle == handle)
      {
        struct crypto_encode_decode_data *result = ddsrt_malloc (sizeof (*result));
        memcpy (result, data, sizeof (*result));
        ddsrt_mutex_unlock (&impl->encode_decode_log_lock);
        return result;
      }
      elem = elem->next;
    } while (elem != elem0);
  }
  ddsrt_mutex_unlock (&impl->encode_decode_log_lock);
  return NULL;
}

static unsigned char * find_buffer_match(const unsigned char *input, size_t input_len, const unsigned char *match, size_t match_len)
{
  if (match_len <= input_len && match_len > 0 && input_len > 0)
  {
    const unsigned char *match_end = match + match_len;
    unsigned char *i = (unsigned char *) input;
    while (i <= input + input_len - match_len)
    {
      unsigned char *m = (unsigned char *) match, *j = i;
      while (*m == *j && j < input + input_len)
      {
        j++;
        if (++m == match_end)
          return i;
      }
      i++;
    }
  }
  return NULL;
}

static bool check_buffers(const DDS_Security_OctetSeq *encoded_buffer, const DDS_Security_OctetSeq *plain_buffer, bool expect_encrypted, DDS_Security_SecurityException *ex)
{
  unsigned char *m = find_buffer_match (encoded_buffer->_buffer, encoded_buffer->_length,
    plain_buffer->_buffer, plain_buffer->_length);
  if ((m == NULL) != expect_encrypted)
  {
    ex->code = 1;
    ex->message = ddsrt_strdup (expect_encrypted ?
      "Expect encryption, but clear payload found after encoding." : "Expect only signature, but clear payload was not found in source after decoding.");
    return false;
  }
  return true;
}

static DDS_Security_long_long check_handle(DDS_Security_long_long handle)
{
  /* Assume that handle, which actually is a pointer, has a value that is likely to be
     a valid memory address and not a value returned by the mock implementation. */
  CU_ASSERT_FATAL (handle == 0 || handle > 4096);
  return handle;
}

static bool expect_encrypted_buffer (DDS_Security_ProtectionKind pk)
{
  return pk == DDS_SECURITY_PROTECTION_KIND_ENCRYPT || pk == DDS_SECURITY_PROTECTION_KIND_ENCRYPT_WITH_ORIGIN_AUTHENTICATION;
}

static void copy_octetseq(DDS_Security_OctetSeq *encoded_submsg, const DDS_Security_OctetSeq *plain_submsg)
{
  encoded_submsg->_length = encoded_submsg->_maximum = plain_submsg->_length;
  if (plain_submsg->_length > 0)
  {
    encoded_submsg->_buffer = ddsrt_malloc(encoded_submsg->_length);
    memcpy(encoded_submsg->_buffer, plain_submsg->_buffer, encoded_submsg->_length);
  }
  else
  {
    encoded_submsg->_buffer = NULL;
  }
}

/**
 * Crypto key exchange
 */
static DDS_Security_boolean create_local_participant_crypto_tokens(
    dds_security_crypto_key_exchange *instance,
    DDS_Security_ParticipantCryptoTokenSeq *local_participant_crypto_tokens,
    const DDS_Security_ParticipantCryptoHandle local_participant_crypto,
    const DDS_Security_ParticipantCryptoHandle remote_participant_crypto,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_exchange_impl *impl = (struct dds_security_crypto_key_exchange_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
    {
      DDS_Security_boolean ret = impl->instance->create_local_participant_crypto_tokens (impl->instance, local_participant_crypto_tokens,
        local_participant_crypto, remote_participant_crypto, ex);
      if (ret && impl->parent->mode == PLUGIN_MODE_TOKEN_LOG)
        store_tokens (impl, LOCAL_PARTICIPANT_TOKENS, local_participant_crypto, remote_participant_crypto, local_participant_crypto_tokens);
      return ret;
    }
    default:
      return true;
  }
}

static DDS_Security_boolean set_remote_participant_crypto_tokens(
    dds_security_crypto_key_exchange *instance,
    const DDS_Security_ParticipantCryptoHandle local_participant_crypto,
    const DDS_Security_ParticipantCryptoHandle remote_participant_crypto,
    const DDS_Security_ParticipantCryptoTokenSeq *remote_participant_tokens,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_exchange_impl *impl = (struct dds_security_crypto_key_exchange_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
    {
      DDS_Security_boolean ret = impl->instance->set_remote_participant_crypto_tokens (impl->instance, check_handle (local_participant_crypto),
        check_handle (remote_participant_crypto), remote_participant_tokens, ex);
      if (ret && impl->parent->mode == PLUGIN_MODE_TOKEN_LOG)
        store_tokens (impl, REMOTE_PARTICIPANT_TOKENS, local_participant_crypto, remote_participant_crypto, remote_participant_tokens);
      return ret;
    }
    default:
      return true;
  }
}

static DDS_Security_boolean create_local_datawriter_crypto_tokens(
    dds_security_crypto_key_exchange *instance,
    DDS_Security_DatawriterCryptoTokenSeq *local_datawriter_crypto_tokens,
    const DDS_Security_DatawriterCryptoHandle local_datawriter_crypto,
    const DDS_Security_DatareaderCryptoHandle remote_datareader_crypto,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_exchange_impl *impl = (struct dds_security_crypto_key_exchange_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
    {
      DDS_Security_boolean ret = impl->instance->create_local_datawriter_crypto_tokens (impl->instance, local_datawriter_crypto_tokens,
        check_handle (local_datawriter_crypto), check_handle (remote_datareader_crypto), ex);
      if (ret && impl->parent->mode == PLUGIN_MODE_TOKEN_LOG)
        store_tokens (impl, LOCAL_WRITER_TOKENS, local_datawriter_crypto, remote_datareader_crypto, local_datawriter_crypto_tokens);
      return ret;
    }
    default:
      return true;
  }
}

static DDS_Security_boolean set_remote_datawriter_crypto_tokens(
    dds_security_crypto_key_exchange *instance,
    const DDS_Security_DatareaderCryptoHandle local_datareader_crypto,
    const DDS_Security_DatawriterCryptoHandle remote_datawriter_crypto,
    const DDS_Security_DatawriterCryptoTokenSeq *remote_datawriter_tokens,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_exchange_impl *impl = (struct dds_security_crypto_key_exchange_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
    {
      DDS_Security_boolean ret = impl->instance->set_remote_datawriter_crypto_tokens (impl->instance, check_handle (local_datareader_crypto),
        check_handle (remote_datawriter_crypto), remote_datawriter_tokens, ex);
      if (ret && impl->parent->mode == PLUGIN_MODE_TOKEN_LOG)
        store_tokens (impl, REMOTE_WRITER_TOKENS, local_datareader_crypto, remote_datawriter_crypto, remote_datawriter_tokens);
      return ret;
    }
    default:
      return true;
  }
}

static DDS_Security_boolean create_local_datareader_crypto_tokens(
    dds_security_crypto_key_exchange *instance,
    DDS_Security_DatareaderCryptoTokenSeq *local_datareader_cryto_tokens,
    const DDS_Security_DatareaderCryptoHandle local_datareader_crypto,
    const DDS_Security_DatawriterCryptoHandle remote_datawriter_crypto,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_exchange_impl *impl = (struct dds_security_crypto_key_exchange_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
    {
      DDS_Security_boolean ret = impl->instance->create_local_datareader_crypto_tokens (impl->instance, local_datareader_cryto_tokens,
        check_handle (local_datareader_crypto), check_handle (remote_datawriter_crypto), ex);
      if (ret && impl->parent->mode == PLUGIN_MODE_TOKEN_LOG)
        store_tokens (impl, LOCAL_READER_TOKENS, local_datareader_crypto, remote_datawriter_crypto, local_datareader_cryto_tokens);
      return ret;
    }
    default:
      return true;
  }
}

static DDS_Security_boolean set_remote_datareader_crypto_tokens(
    dds_security_crypto_key_exchange *instance,
    const DDS_Security_DatawriterCryptoHandle local_datawriter_crypto,
    const DDS_Security_DatareaderCryptoHandle remote_datareader_crypto,
    const DDS_Security_DatareaderCryptoTokenSeq *remote_datareader_tokens,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_exchange_impl *impl = (struct dds_security_crypto_key_exchange_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
    {
      DDS_Security_boolean ret = impl->instance->set_remote_datareader_crypto_tokens (impl->instance, check_handle (local_datawriter_crypto),
        check_handle (remote_datareader_crypto), remote_datareader_tokens, ex);
      if (ret && impl->parent->mode == PLUGIN_MODE_TOKEN_LOG)
        store_tokens (impl, REMOTE_READER_TOKENS, local_datawriter_crypto, remote_datareader_crypto, remote_datareader_tokens);
      return ret;
    }
    default:
      return true;
  }
}

static DDS_Security_boolean return_crypto_tokens(
    dds_security_crypto_key_exchange *instance,
    DDS_Security_CryptoTokenSeq *crypto_tokens,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_exchange_impl *impl = (struct dds_security_crypto_key_exchange_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
      return impl->instance->return_crypto_tokens (impl->instance, crypto_tokens, ex);
    default:
      return true;
  }
}

/**
 * Crypto key factory
 */
static DDS_Security_ParticipantCryptoHandle register_local_participant(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_IdentityHandle participant_identity,
    const DDS_Security_PermissionsHandle participant_permissions,
    const DDS_Security_PropertySeq *participant_properties,
    const DDS_Security_ParticipantSecurityAttributes *participant_security_attributes,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_factory_impl *impl = (struct dds_security_crypto_key_factory_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
      return check_handle (impl->instance->register_local_participant (impl->instance, check_handle (participant_identity),
        check_handle (participant_permissions), participant_properties, participant_security_attributes, ex));
    default:
      return ++g_local_participant_handle;
  }
}

static DDS_Security_ParticipantCryptoHandle register_matched_remote_participant(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_ParticipantCryptoHandle local_participant_crypto_handle,
    const DDS_Security_IdentityHandle remote_participant_identity,
    const DDS_Security_PermissionsHandle remote_participant_permissions,
    const DDS_Security_SharedSecretHandle shared_secret,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_factory_impl *impl = (struct dds_security_crypto_key_factory_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
      return check_handle (impl->instance->register_matched_remote_participant (impl->instance, local_participant_crypto_handle,
        remote_participant_identity, remote_participant_permissions, shared_secret, ex));
    default:
      return 0;
  }
}

static DDS_Security_DatawriterCryptoHandle register_local_datawriter(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_ParticipantCryptoHandle participant_crypto,
    const DDS_Security_PropertySeq *datawriter_properties,
    const DDS_Security_EndpointSecurityAttributes *datawriter_security_attributes,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_factory_impl *impl = (struct dds_security_crypto_key_factory_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
      return check_handle (impl->instance->register_local_datawriter (impl->instance, check_handle (participant_crypto),
        datawriter_properties, datawriter_security_attributes, ex));
    default:
      return 0;
  }
}

static DDS_Security_DatareaderCryptoHandle register_matched_remote_datareader(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_DatawriterCryptoHandle local_datawriter_crypto_handle,
    const DDS_Security_ParticipantCryptoHandle remote_participant_crypto,
    const DDS_Security_SharedSecretHandle shared_secret,
    const DDS_Security_boolean relay_only,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_factory_impl *impl = (struct dds_security_crypto_key_factory_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
      return check_handle (impl->instance->register_matched_remote_datareader (impl->instance, check_handle (local_datawriter_crypto_handle),
        check_handle (remote_participant_crypto), check_handle (shared_secret), relay_only, ex));
    default:
      return 0;
  }
}

static DDS_Security_DatareaderCryptoHandle register_local_datareader(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_ParticipantCryptoHandle participant_crypto,
    const DDS_Security_PropertySeq *datareader_properties,
    const DDS_Security_EndpointSecurityAttributes *datareader_security_attributes,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_factory_impl *impl = (struct dds_security_crypto_key_factory_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
      return check_handle (impl->instance->register_local_datareader (impl->instance, check_handle (participant_crypto),
        datareader_properties, datareader_security_attributes, ex));
    default:
      return 0;
  }
}

static DDS_Security_DatawriterCryptoHandle register_matched_remote_datawriter(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_DatareaderCryptoHandle local_datareader_crypto_handle,
    const DDS_Security_ParticipantCryptoHandle remote_participant_crypt,
    const DDS_Security_SharedSecretHandle shared_secret,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_factory_impl *impl = (struct dds_security_crypto_key_factory_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
      return check_handle (impl->instance->register_matched_remote_datawriter (impl->instance, check_handle (local_datareader_crypto_handle),
        check_handle (remote_participant_crypt), shared_secret, ex));
    default:
      return 1;
  }
}

static DDS_Security_boolean unregister_participant(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_ParticipantCryptoHandle participant_crypto_handle,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_factory_impl *impl = (struct dds_security_crypto_key_factory_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
      return impl->instance->unregister_participant (impl->instance, check_handle (participant_crypto_handle), ex);
    default:
      return true;
  }
}

static DDS_Security_boolean unregister_datawriter(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_DatawriterCryptoHandle datawriter_crypto_handle,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_factory_impl *impl = (struct dds_security_crypto_key_factory_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
      return impl->instance->unregister_datawriter (impl->instance, check_handle (datawriter_crypto_handle), ex);
    default:
      return true;
  }
}

static DDS_Security_boolean unregister_datareader(
    dds_security_crypto_key_factory *instance,
    const DDS_Security_DatareaderCryptoHandle datareader_crypto_handle,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_key_factory_impl *impl = (struct dds_security_crypto_key_factory_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
      return impl->instance->unregister_datareader (impl->instance, check_handle (datareader_crypto_handle), ex);
    default:
      return true;
  }
}

/**
 * Crypto transform
 */
static DDS_Security_boolean encode_serialized_payload(
    dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *encoded_buffer,
    DDS_Security_OctetSeq *extra_inline_qos,
    const DDS_Security_OctetSeq *plain_buffer,
    const DDS_Security_DatawriterCryptoHandle sending_datawriter_crypto,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_transform_impl *impl = (struct dds_security_crypto_transform_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_PLAIN_DATA:
      if (impl->parent->force_plain_payload && impl->parent->force_plain_sender_handle == sending_datawriter_crypto)
      {
        copy_octetseq (encoded_buffer, plain_buffer);
        return true;
      }
      /* fall through */
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    {
      if (!impl->instance->encode_serialized_payload (impl->instance, encoded_buffer,
          extra_inline_qos, plain_buffer, check_handle (sending_datawriter_crypto), ex))
        return false;
      if (impl->parent->protection_kinds_set
          && impl->parent->encrypted_secret
          && (impl->parent->payload_protection_kind == DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT
              || expect_encrypted_buffer (impl->parent->metadata_protection_kind)
              || expect_encrypted_buffer (impl->parent->rtps_protection_kind)))
      {
        if (find_buffer_match (encoded_buffer->_buffer, encoded_buffer->_length, (const unsigned char *) impl->parent->encrypted_secret, strlen (impl->parent->encrypted_secret)) != NULL)
        {
          ex->code = 1;
          ex->message = ddsrt_strdup ("Expect encryption, but found secret in payload after encoding");
          return false;
        }
      }
      return !impl->parent->protection_kinds_set || check_buffers (encoded_buffer, plain_buffer, impl->parent->payload_protection_kind == DDS_SECURITY_BASICPROTECTION_KIND_ENCRYPT, ex);
    }
    default:
      return true;
  }
}

static DDS_Security_boolean check_buffer_submsg(
    struct dds_security_crypto_transform_impl *impl,
    DDS_Security_OctetSeq *encoded_rtps_submessage,
    const DDS_Security_OctetSeq *plain_rtps_submessage,
    DDS_Security_SecurityException *ex)
{
  bool exp_enc = impl->parent->protection_kinds_set && (expect_encrypted_buffer (impl->parent->metadata_protection_kind) || expect_encrypted_buffer (impl->parent->rtps_protection_kind));
  if (exp_enc && impl->parent->encrypted_secret && find_buffer_match (encoded_rtps_submessage->_buffer, encoded_rtps_submessage->_length, (const unsigned char *) impl->parent->encrypted_secret, strlen (impl->parent->encrypted_secret)) != NULL)
  {
    ex->code = 1;
    ex->message = ddsrt_strdup ("Expect encryption, but found secret in submessage after encoding");
    return false;
  }

  return impl->parent->protection_kinds_set && expect_encrypted_buffer (impl->parent->metadata_protection_kind) ? check_buffers (encoded_rtps_submessage, plain_rtps_submessage, true, ex) : true;
}

static DDS_Security_boolean encode_datawriter_submessage(
    dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *encoded_rtps_submessage,
    const DDS_Security_OctetSeq *plain_rtps_submessage,
    const DDS_Security_DatawriterCryptoHandle sending_datawriter_crypto,
    const DDS_Security_DatareaderCryptoHandleSeq *receiving_datareader_crypto_list,
    int32_t *receiving_datareader_crypto_list_index,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_transform_impl *impl = (struct dds_security_crypto_transform_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_PLAIN_DATA:
      if (impl->parent->force_plain_submsg && impl->parent->force_plain_sender_handle == sending_datawriter_crypto)
      {
        copy_octetseq (encoded_rtps_submessage, plain_rtps_submessage);
        assert (receiving_datareader_crypto_list->_length <= INT32_MAX);
        *receiving_datareader_crypto_list_index = (int32_t) receiving_datareader_crypto_list->_length;
        return true;
      }
      /* fall through */
    case PLUGIN_MODE_WRAPPED:
      log_encode_decode (impl->parent, ENCODE_DATAWRITER_SUBMESSAGE, sending_datawriter_crypto);
      /* fall-through */
    case PLUGIN_MODE_TOKEN_LOG:
      if (!impl->instance->encode_datawriter_submessage (impl->instance, encoded_rtps_submessage,
        plain_rtps_submessage, check_handle (sending_datawriter_crypto), receiving_datareader_crypto_list, receiving_datareader_crypto_list_index, ex))
        return false;
      if (!check_buffer_submsg(impl, encoded_rtps_submessage, plain_rtps_submessage, ex))
        return false;

      if (impl->parent->disc_protection_kinds_set && expect_encrypted_buffer (impl->parent->disc_protection_kind))
      {
        if (impl->parent->pp_secret && find_buffer_match (plain_rtps_submessage->_buffer, plain_rtps_submessage->_length, (const unsigned char *) impl->parent->pp_secret, strlen (impl->parent->pp_secret)) != NULL)
        {
          ex->code = 1;
          ex->message = ddsrt_strdup ("Expect discovery encryption, but found participant userdata secret in submessage after encoding");
          return false;
        }
        if (impl->parent->groupdata_secret && find_buffer_match (plain_rtps_submessage->_buffer, plain_rtps_submessage->_length, (const unsigned char *) impl->parent->groupdata_secret, strlen (impl->parent->groupdata_secret)) != NULL)
        {
          ex->code = 1;
          ex->message = ddsrt_strdup ("Expect discovery encryption, but found publisher/subscriber groupdata secret in submessage after encoding");
          return false;
        }
        if (impl->parent->ep_secret && find_buffer_match (plain_rtps_submessage->_buffer, plain_rtps_submessage->_length, (const unsigned char *) impl->parent->ep_secret, strlen (impl->parent->ep_secret)) != NULL)
        {
          ex->code = 1;
          ex->message = ddsrt_strdup ("Expect discovery encryption, but found reader/writer userdata secret in submessage after encoding");
          return false;
        }
      }
      return true;


    default:
      return true;
  }
}

static DDS_Security_boolean encode_datareader_submessage(
    dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *encoded_rtps_submessage,
    const DDS_Security_OctetSeq *plain_rtps_submessage,
    const DDS_Security_DatareaderCryptoHandle sending_datareader_crypto,
    const DDS_Security_DatawriterCryptoHandleSeq *receiving_datawriter_crypto_list,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_transform_impl *impl = (struct dds_security_crypto_transform_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_PLAIN_DATA:
      if (impl->parent->force_plain_submsg && impl->parent->force_plain_sender_handle == sending_datareader_crypto)
      {
        copy_octetseq (encoded_rtps_submessage, plain_rtps_submessage);
        return true;
      }
      /* fall through */
    case PLUGIN_MODE_WRAPPED:
      log_encode_decode (impl->parent, ENCODE_DATAREADER_SUBMESSAGE, sending_datareader_crypto);
      /* fall-through */
    case PLUGIN_MODE_TOKEN_LOG:
      if (!impl->instance->encode_datareader_submessage (impl->instance, encoded_rtps_submessage,
        plain_rtps_submessage, check_handle (sending_datareader_crypto), receiving_datawriter_crypto_list, ex))
        return false;
      return check_buffer_submsg(impl, encoded_rtps_submessage, plain_rtps_submessage, ex);
    default:
      return true;
  }
}

static DDS_Security_boolean encode_rtps_message(
    dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *encoded_rtps_message,
    const DDS_Security_OctetSeq *plain_rtps_message,
    const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
    const DDS_Security_ParticipantCryptoHandleSeq *receiving_participant_crypto_list,
    int32_t *receiving_participant_crypto_list_index,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_transform_impl *impl = (struct dds_security_crypto_transform_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_PLAIN_DATA:
      if (impl->parent->force_plain_rtps)
      {
        copy_octetseq (encoded_rtps_message, plain_rtps_message);
        assert (receiving_participant_crypto_list->_length <= INT32_MAX);
        *receiving_participant_crypto_list_index = (int32_t) receiving_participant_crypto_list->_length;
        return true;
      }
      /* fall through */
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
      if (!impl->instance->encode_rtps_message (impl->instance, encoded_rtps_message,
          plain_rtps_message, check_handle (sending_participant_crypto), receiving_participant_crypto_list, receiving_participant_crypto_list_index, ex))
        return false;
      if (impl->parent->protection_kinds_set
          && impl->parent->encrypted_secret
          && expect_encrypted_buffer (impl->parent->rtps_protection_kind)
          && find_buffer_match (encoded_rtps_message->_buffer, encoded_rtps_message->_length, (const unsigned char *) impl->parent->encrypted_secret, strlen (impl->parent->encrypted_secret)) != NULL)
      {
        ex->code = 1;
        ex->message = ddsrt_strdup ("Expect encryption, but found secret in RTPS message after encoding");
        return false;
      }
      return impl->parent->protection_kinds_set && expect_encrypted_buffer (impl->parent->rtps_protection_kind) ?
        check_buffers (encoded_rtps_message, plain_rtps_message, true, ex) : true;
    default:
      return true;
  }
}

static DDS_Security_boolean decode_rtps_message (
    dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *plain_buffer,
    const DDS_Security_OctetSeq *encoded_buffer,
    const DDS_Security_ParticipantCryptoHandle receiving_participant_crypto,
    const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_transform_impl *impl = (struct dds_security_crypto_transform_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
      return impl->instance->decode_rtps_message (impl->instance, plain_buffer, encoded_buffer,
          check_handle (receiving_participant_crypto), check_handle (sending_participant_crypto), ex);
    default:
      return true;
  }
}

static DDS_Security_boolean preprocess_secure_submsg(
    dds_security_crypto_transform *instance,
    DDS_Security_DatawriterCryptoHandle *datawriter_crypto,
    DDS_Security_DatareaderCryptoHandle *datareader_crypto,
    DDS_Security_SecureSubmessageCategory_t *secure_submessage_category,
    const DDS_Security_OctetSeq *encoded_rtps_submessage,
    const DDS_Security_ParticipantCryptoHandle receiving_participant_crypto,
    const DDS_Security_ParticipantCryptoHandle sending_participant_crypto,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_transform_impl *impl = (struct dds_security_crypto_transform_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
      return impl->instance->preprocess_secure_submsg (impl->instance, datawriter_crypto, datareader_crypto,
        secure_submessage_category, encoded_rtps_submessage, check_handle (receiving_participant_crypto), check_handle (sending_participant_crypto), ex);
    default:
      return true;
  }
}

static DDS_Security_boolean decode_datawriter_submessage(
    dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *plain_rtps_submessage,
    const DDS_Security_OctetSeq *encoded_rtps_submessage,
    const DDS_Security_DatareaderCryptoHandle receiving_datareader_crypto,
    const DDS_Security_DatawriterCryptoHandle sending_datawriter_crypto,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_transform_impl *impl = (struct dds_security_crypto_transform_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_PLAIN_DATA:
    case PLUGIN_MODE_WRAPPED:
      log_encode_decode (impl->parent, DECODE_DATAWRITER_SUBMESSAGE, receiving_datareader_crypto);
      /* fall-through */
    case PLUGIN_MODE_TOKEN_LOG:
      return impl->instance->decode_datawriter_submessage (impl->instance, plain_rtps_submessage,
          encoded_rtps_submessage, check_handle (receiving_datareader_crypto), check_handle (sending_datawriter_crypto), ex);
    default:
      return true;
  }
}

static DDS_Security_boolean decode_datareader_submessage(
    dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *plain_rtps_submessage,
    const DDS_Security_OctetSeq *encoded_rtps_submessage,
    const DDS_Security_DatawriterCryptoHandle receiving_datawriter_crypto,
    const DDS_Security_DatareaderCryptoHandle sending_datareader_crypto,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_transform_impl *impl = (struct dds_security_crypto_transform_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_PLAIN_DATA:
    case PLUGIN_MODE_WRAPPED:
      log_encode_decode (impl->parent, DECODE_DATAREADER_SUBMESSAGE, receiving_datawriter_crypto);
      /* fall-through */
    case PLUGIN_MODE_TOKEN_LOG:
      return impl->instance->decode_datareader_submessage (impl->instance, plain_rtps_submessage,
          encoded_rtps_submessage, check_handle (receiving_datawriter_crypto), check_handle (sending_datareader_crypto), ex);
    default:
      return true;
  }
}

static DDS_Security_boolean decode_serialized_payload(
    dds_security_crypto_transform *instance,
    DDS_Security_OctetSeq *plain_buffer,
    const DDS_Security_OctetSeq *encoded_buffer,
    const DDS_Security_OctetSeq *inline_qos,
    const DDS_Security_DatareaderCryptoHandle receiving_datareader_crypto,
    const DDS_Security_DatawriterCryptoHandle sending_datawriter_crypto,
    DDS_Security_SecurityException *ex)
{
  struct dds_security_crypto_transform_impl *impl = (struct dds_security_crypto_transform_impl *)instance;
  switch (impl->parent->mode)
  {
    case PLUGIN_MODE_WRAPPED:
    case PLUGIN_MODE_TOKEN_LOG:
    case PLUGIN_MODE_PLAIN_DATA:
      return impl->instance->decode_serialized_payload(impl->instance, plain_buffer, encoded_buffer,
          inline_qos, check_handle (receiving_datareader_crypto), check_handle (sending_datawriter_crypto), ex);
    default:
      return true;
  }
}

/**
 * Init and finalize functions
 */
static struct dds_security_cryptography_impl * init_test_cryptography_common(const char *argument, bool wrapped, struct ddsi_domaingv *gv)
{
  struct dds_security_cryptography_impl *impl = (struct dds_security_cryptography_impl*) ddsrt_malloc(sizeof(*impl));
  memset(impl, 0, sizeof(*impl));

  if (wrapped)
  {
    if (init_crypto(argument, (void **)&impl->instance, gv) != DDS_SECURITY_SUCCESS)
      return NULL;

    impl->transform_wrap.instance = impl->instance->crypto_transform;
    impl->factory_wrap.instance = impl->instance->crypto_key_factory;
    impl->exchange_wrap.instance = impl->instance->crypto_key_exchange;
  }

  impl->base.crypto_transform = (dds_security_crypto_transform *)&impl->transform_wrap;
  impl->base.crypto_key_factory = (dds_security_crypto_key_factory *)&impl->factory_wrap;
  impl->base.crypto_key_exchange = (dds_security_crypto_key_exchange *)&impl->exchange_wrap;

  impl->transform_wrap.parent = impl;
  impl->factory_wrap.parent = impl;
  impl->exchange_wrap.parent = impl;

  impl->factory_wrap.base.register_local_participant = &register_local_participant;
  impl->factory_wrap.base.register_matched_remote_participant = &register_matched_remote_participant;
  impl->factory_wrap.base.register_local_datawriter = &register_local_datawriter;
  impl->factory_wrap.base.register_matched_remote_datareader = &register_matched_remote_datareader;
  impl->factory_wrap.base.register_local_datareader = &register_local_datareader;
  impl->factory_wrap.base.register_matched_remote_datawriter = &register_matched_remote_datawriter;
  impl->factory_wrap.base.unregister_participant = &unregister_participant;
  impl->factory_wrap.base.unregister_datawriter = &unregister_datawriter;
  impl->factory_wrap.base.unregister_datareader = &unregister_datareader;

  impl->exchange_wrap.base.create_local_participant_crypto_tokens = &create_local_participant_crypto_tokens;
  impl->exchange_wrap.base.set_remote_participant_crypto_tokens = &set_remote_participant_crypto_tokens;
  impl->exchange_wrap.base.create_local_datawriter_crypto_tokens = &create_local_datawriter_crypto_tokens;
  impl->exchange_wrap.base.set_remote_datawriter_crypto_tokens = &set_remote_datawriter_crypto_tokens;
  impl->exchange_wrap.base.create_local_datareader_crypto_tokens = &create_local_datareader_crypto_tokens;
  impl->exchange_wrap.base.set_remote_datareader_crypto_tokens = &set_remote_datareader_crypto_tokens;
  impl->exchange_wrap.base.return_crypto_tokens = &return_crypto_tokens;

  impl->transform_wrap.base.encode_datawriter_submessage = &encode_datawriter_submessage;
  impl->transform_wrap.base.encode_datareader_submessage = &encode_datareader_submessage;
  impl->transform_wrap.base.encode_rtps_message = &encode_rtps_message;
  impl->transform_wrap.base.decode_rtps_message = &decode_rtps_message;
  impl->transform_wrap.base.preprocess_secure_submsg = &preprocess_secure_submsg;
  impl->transform_wrap.base.decode_datawriter_submessage = &decode_datawriter_submessage;
  impl->transform_wrap.base.decode_datareader_submessage = &decode_datareader_submessage;
  impl->transform_wrap.base.decode_serialized_payload = &decode_serialized_payload;
  impl->transform_wrap.base.encode_serialized_payload = &encode_serialized_payload;

  return impl;
}

static int finalize_test_cryptography_common(struct dds_security_cryptography_impl * impl, bool wrapped)
{
  int ret;
  if (wrapped && (ret = finalize_crypto(impl->instance)) != DDS_SECURITY_SUCCESS)
    return ret;
  ddsrt_free(impl);
  return DDS_SECURITY_SUCCESS;
}

int init_test_cryptography_all_ok(const char *argument, void **context, struct ddsi_domaingv *gv)
{
  struct dds_security_cryptography_impl *impl = init_test_cryptography_common(argument, false, gv);
  if (!impl)
    return DDS_SECURITY_FAILED;
  impl->mode = PLUGIN_MODE_ALL_OK;
  *context = impl;
  return DDS_SECURITY_SUCCESS;
}

int finalize_test_cryptography_all_ok(void *context)
{
  struct dds_security_cryptography_impl* impl = (struct dds_security_cryptography_impl*) context;
  assert(impl->mode == PLUGIN_MODE_ALL_OK);
  return finalize_test_cryptography_common(impl, false);
}

int init_test_cryptography_missing_func(const char *argument, void **context, struct ddsi_domaingv *gv)
{
  struct dds_security_cryptography_impl *impl = init_test_cryptography_common(argument, false, gv);
  if (!impl)
    return DDS_SECURITY_FAILED;
  impl->base.crypto_key_exchange->set_remote_participant_crypto_tokens = NULL;
  impl->mode = PLUGIN_MODE_MISSING_FUNC;
  *context = impl;
  return DDS_SECURITY_SUCCESS;
}

int finalize_test_cryptography_missing_func(void *context)
{
  struct dds_security_cryptography_impl* impl = (struct dds_security_cryptography_impl*) context;
  assert(impl->mode == PLUGIN_MODE_MISSING_FUNC);
  return finalize_test_cryptography_common(impl, false);
}

static void init_encode_decode_log(struct dds_security_cryptography_impl *impl)
{
  ddsrt_mutex_init (&impl->encode_decode_log_lock);
  ddsrt_circlist_init (&impl->encode_decode_log);
}

static void fini_encode_decode_log(struct dds_security_cryptography_impl *impl)
{
  ddsrt_mutex_lock (&impl->encode_decode_log_lock);
  while (!ddsrt_circlist_isempty (&impl->encode_decode_log))
  {
    struct ddsrt_circlist_elem *list_elem = ddsrt_circlist_oldest (&impl->encode_decode_log);
    ddsrt_circlist_remove (&impl->encode_decode_log, list_elem);
    ddsrt_free (list_elem);
  }
  ddsrt_mutex_unlock (&impl->encode_decode_log_lock);
  ddsrt_mutex_destroy (&impl->encode_decode_log_lock);
}

int init_test_cryptography_wrapped(const char *argument, void **context, struct ddsi_domaingv *gv)
{
  struct dds_security_cryptography_impl *impl = init_test_cryptography_common(argument, true, gv);
  if (!impl)
    return DDS_SECURITY_FAILED;
  impl->mode = PLUGIN_MODE_WRAPPED;
  init_encode_decode_log(impl);
  *context = impl;
  return DDS_SECURITY_SUCCESS;
}

int finalize_test_cryptography_wrapped(void *context)
{
  struct dds_security_cryptography_impl* impl = (struct dds_security_cryptography_impl*) context;
  assert(impl->mode == PLUGIN_MODE_WRAPPED);
  fini_encode_decode_log(impl);
  return finalize_test_cryptography_common(impl, true);
}

static void init_print_lock(void)
{
  ddsrt_mutex_init (&g_print_token_lock);
}

int32_t init_test_cryptography_store_tokens(const char *argument, void **context, struct ddsi_domaingv *gv)
{
  struct dds_security_cryptography_impl *impl = init_test_cryptography_common(argument, true, gv);
  if (!impl)
    return DDS_SECURITY_FAILED;
  impl->mode = PLUGIN_MODE_TOKEN_LOG;
  ddsrt_once (&lock_inited, &init_print_lock);
  ddsrt_mutex_init (&impl->token_data_lock);
  ddsrt_circlist_init (&impl->token_data_list);
  *context = impl;
  return DDS_SECURITY_SUCCESS;
}

int32_t finalize_test_cryptography_store_tokens(void *context)
{
  struct dds_security_cryptography_impl* impl = (struct dds_security_cryptography_impl*) context;
  assert(impl->mode == PLUGIN_MODE_TOKEN_LOG);
  ddsrt_mutex_lock (&impl->token_data_lock);
  while (!ddsrt_circlist_isempty (&impl->token_data_list))
  {
    struct ddsrt_circlist_elem *list_elem = ddsrt_circlist_oldest (&impl->token_data_list);
    ddsrt_circlist_remove (&impl->token_data_list, list_elem);
    ddsrt_free (list_elem);
  }
  ddsrt_mutex_unlock (&impl->token_data_lock);
  ddsrt_mutex_destroy (&impl->token_data_lock);

  /* don't detroy g_print_token_lock as this will result in multiple
     calls to mutex_destroy for this lock in case of multiple domains */

  return finalize_test_cryptography_common(impl, true);
}

int init_test_cryptography_plain_data(const char *argument, void **context, struct ddsi_domaingv *gv)
{
  struct dds_security_cryptography_impl *impl = init_test_cryptography_common(argument, true, gv);
  if (!impl)
    return DDS_SECURITY_FAILED;
  impl->mode = PLUGIN_MODE_PLAIN_DATA;
  init_encode_decode_log(impl);
  *context = impl;
  return DDS_SECURITY_SUCCESS;
}

int finalize_test_cryptography_plain_data(void *context)
{
  struct dds_security_cryptography_impl* impl = (struct dds_security_cryptography_impl*) context;
  assert(impl->mode == PLUGIN_MODE_PLAIN_DATA);
  fini_encode_decode_log(impl);
  return finalize_test_cryptography_common(impl, true);
}

