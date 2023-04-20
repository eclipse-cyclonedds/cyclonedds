// Copyright(c) 2006 to 2021 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#include "dds/ddsrt/bswap.h"
#include "dds/ddsrt/heap.h"
#include "dds/ddsrt/string.h"
#include "dds/ddsrt/types.h"
#include "dds/ddsrt/environ.h"
#include "dds/security/dds_security_api.h"
#include "dds/security/core/dds_security_serialize.h"
#include "dds/security/core/dds_security_utils.h"
#include "dds/security/core/dds_security_shared_secret.h"
#include "dds/security/openssl_support.h"
#include "CUnit/CUnit.h"
#include "CUnit/Test.h"
#include "common/src/loader.h"
#include "crypto_objects.h"

#define TEST_SHARED_SECRET_SIZE 32

#define VALID_DDSI_RTPS_SMID_SEC_PREFIX       0x31
#define INVALID_DDSI_RTPS_SMID_SEC_PREFIX     0x15


typedef struct SubMessageHeader {
    unsigned char kind;
    unsigned char flags;
    uint16_t octetsToNextSubMsg;
} SubMessageHeader;

typedef struct CryptoHeader {
    DDS_Security_CryptoTransformKind transform_id;
    DDS_Security_CryptoTransformKeyId key_id;
    unsigned char session_id[4];
    unsigned char initVectorSuffix[8];
} CryptoHeader;

static struct plugins_hdl *plugins = NULL;
static dds_security_cryptography *crypto = NULL;

static DDS_Security_IdentityHandle local_participant_identity  = 1;
static DDS_Security_IdentityHandle remote_participant_identity = 2;

static DDS_Security_ParticipantCryptoHandle local_participant_handle = DDS_SECURITY_HANDLE_NIL;
static DDS_Security_ParticipantCryptoHandle remote_participant_handle = DDS_SECURITY_HANDLE_NIL;

static DDS_Security_DatawriterCryptoHandle  local_writer_crypto  = 0;
static DDS_Security_DatawriterCryptoHandle  remote_writer_crypto = 0;
static DDS_Security_DatareaderCryptoHandle  local_reader_crypto  = 0;
static DDS_Security_DatareaderCryptoHandle  remote_reader_crypto = 0;

static DDS_Security_SharedSecretHandleImpl *shared_secret_handle_impl = NULL;
static DDS_Security_SharedSecretHandle shared_secret_handle;

static DDS_Security_KeyMaterial_AES_GCM_GMAC writer_key_message;
static DDS_Security_KeyMaterial_AES_GCM_GMAC writer_key_payload;
static DDS_Security_KeyMaterial_AES_GCM_GMAC reader_key_message;

static void allocate_shared_secret(void)
{
    int32_t i;

    shared_secret_handle_impl = ddsrt_malloc (sizeof(DDS_Security_SharedSecretHandleImpl));

    shared_secret_handle_impl->shared_secret = ddsrt_malloc (TEST_SHARED_SECRET_SIZE * sizeof(unsigned char));
    shared_secret_handle_impl->shared_secret_size = TEST_SHARED_SECRET_SIZE;

    for (i = 0; i < shared_secret_handle_impl->shared_secret_size; i++)
    {
        shared_secret_handle_impl->shared_secret[i] = (unsigned char)(i % 20);
    }
    for (i = 0; i < 32; i++)
    {
        shared_secret_handle_impl->challenge1[i] = (unsigned char)(i % 15);
        shared_secret_handle_impl->challenge2[i] = (unsigned char)(i % 12);
    }

    shared_secret_handle = (DDS_Security_SharedSecretHandle) shared_secret_handle_impl;
}

static void deallocate_shared_secret(void)
{
    ddsrt_free(shared_secret_handle_impl->shared_secret);
    ddsrt_free(shared_secret_handle_impl);
}

static int register_local_participant(void)
{
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_PermissionsHandle participant_permissions = 3; //valid dummy value
    DDS_Security_PropertySeq participant_properties;
    DDS_Security_ParticipantSecurityAttributes participant_security_attributes;

    memset(&participant_properties, 0, sizeof(participant_properties));
    memset(&participant_security_attributes, 0, sizeof(participant_security_attributes));

    local_participant_handle =
            crypto->crypto_key_factory->register_local_participant(
                    crypto->crypto_key_factory,
                    local_participant_identity,
                    participant_permissions,
                    &participant_properties,
                    &participant_security_attributes,
                    &exception);

     if (local_participant_handle == DDS_SECURITY_HANDLE_NIL) {
         printf("register_local_participant: %s\n", exception.message ? exception.message : "Error message missing");
     }

     return local_participant_handle ? 0 : -1;
}

static int register_remote_participant(void)
{
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_PermissionsHandle remote_participant_permissions = 5;

    remote_participant_handle =
            crypto->crypto_key_factory->register_matched_remote_participant(
                    crypto->crypto_key_factory,
                    local_participant_handle,
                    remote_participant_identity,
                    remote_participant_permissions,
                    shared_secret_handle,
                    &exception);

     if (remote_participant_handle == DDS_SECURITY_HANDLE_NIL) {
         printf("register_matched_remote_participant: %s\n", exception.message ? exception.message : "Error message missing");
     }

     return remote_participant_handle ? 0 : -1;
}

static void prepare_endpoint_security_attributes( DDS_Security_EndpointSecurityAttributes *attributes){
    memset( attributes, 0 , sizeof(DDS_Security_EndpointSecurityAttributes));
    attributes->is_discovery_protected = true;
    attributes->is_submessage_protected = true;

    attributes->plugin_endpoint_attributes |= DDS_SECURITY_PLUGIN_ENDPOINT_ATTRIBUTES_FLAG_IS_SUBMESSAGE_ENCRYPTED;

}

static int register_local_datareader(void)
{
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_PropertySeq datareader_properties;
    DDS_Security_EndpointSecurityAttributes datareader_security_attributes;

    memset(&datareader_properties, 0, sizeof(datareader_properties));
    prepare_endpoint_security_attributes( &datareader_security_attributes );

    local_reader_crypto =
            crypto->crypto_key_factory->register_local_datareader(
                                        crypto->crypto_key_factory,
                                        local_participant_handle,
                                        &datareader_properties,
                                        &datareader_security_attributes,
                                        &exception);


    if (local_reader_crypto == 0)
        printf("register_local_datawriter: %s\n", exception.message ? exception.message : "Error message missing");

    return local_reader_crypto ? 0 : -1;
}

static int register_remote_datareader(void)
{
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_PropertySeq datawriter_properties;

    memset (&datawriter_properties, 0, sizeof(datawriter_properties));
    remote_reader_crypto =
            crypto->crypto_key_factory->register_matched_remote_datareader(
                    crypto->crypto_key_factory,
                    local_writer_crypto,
                    remote_participant_handle,
                    shared_secret_handle,
                    true,
                    &exception);

    if (remote_reader_crypto == 0)
        printf("register_matched_remote_datareader: %s\n", exception.message ? exception.message : "Error message missing");

    return remote_reader_crypto ? 0 : -1;
}

static int register_local_datawriter(void)
{
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_PropertySeq datawriter_properties;
    DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;

    memset(&datawriter_properties, 0, sizeof(datawriter_properties));
    prepare_endpoint_security_attributes( &datawriter_security_attributes );

    local_writer_crypto =
            crypto->crypto_key_factory->register_local_datawriter(
                    crypto->crypto_key_factory,
                    local_participant_handle,
                    &datawriter_properties,
                    &datawriter_security_attributes,
                    &exception);


    if (local_writer_crypto == 0)
        printf("register_local_datawriter: %s\n", exception.message ? exception.message : "Error message missing");

    return local_writer_crypto ? 0 : -1;
}

static int register_remote_datawriter(void)
{
    DDS_Security_SecurityException exception = {NULL, 0, 0};

    remote_writer_crypto =
            crypto->crypto_key_factory->register_matched_remote_datawriter(
                                        crypto->crypto_key_factory,
                                        local_reader_crypto,
                                        remote_participant_handle,
                                        shared_secret_handle,
                                        &exception);

    if (remote_writer_crypto == 0)
        printf("register_matched_remote_datareader: %s\n", exception.message ? exception.message : "Error message missing");

    return remote_writer_crypto ? 0 : -1;
}

static DDS_Security_boolean retrieve_datawriter_keys(DDS_Security_DatawriterCryptoTokenSeq *tokens)
{
    DDS_Security_boolean result = true;
    DDS_Security_Deserializer deserializer;
    DDS_Security_KeyMaterial_AES_GCM_GMAC *key_mat = &writer_key_message;
    uint32_t i;

    for (i = 0; result && (i < tokens->_length); i++) {
        const DDS_Security_OctetSeq *tdata = &tokens->_buffer[i].binary_properties._buffer[0].value;

        deserializer = DDS_Security_Deserializer_new(tdata->_buffer, tdata->_length);

        if (!deserializer)
            result = false;
        else if (!DDS_Security_Deserialize_KeyMaterial_AES_GCM_GMAC(deserializer, key_mat))
            result = false;

        DDS_Security_Deserializer_free(deserializer);
        key_mat = &writer_key_payload;
    }

    return result;
}

static DDS_Security_boolean retrieve_datareader_keys(DDS_Security_DatareaderCryptoTokenSeq *tokens)
{
    DDS_Security_boolean result = true;
    const DDS_Security_OctetSeq *tdata = &tokens->_buffer[0].binary_properties._buffer[0].value;
    DDS_Security_Deserializer deserializer;
    DDS_Security_KeyMaterial_AES_GCM_GMAC *key_mat = &reader_key_message;

    deserializer = DDS_Security_Deserializer_new(tdata->_buffer, tdata->_length);

    if (!deserializer)
        result = false;
    else if (!DDS_Security_Deserialize_KeyMaterial_AES_GCM_GMAC(deserializer, key_mat))
        result = false;
    DDS_Security_Deserializer_free(deserializer);

    return result;
}

static int set_remote_datawriter_tokens(void)
{
    DDS_Security_boolean result;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_DatawriterCryptoTokenSeq tokens;

    memset(&tokens, 0, sizeof(tokens));

    /* Now call the function. */
    result = crypto->crypto_key_exchange->create_local_datawriter_crypto_tokens(
                crypto->crypto_key_exchange,
                &tokens,
                local_writer_crypto,
                remote_reader_crypto,
                &exception);

    if (result)
        result = retrieve_datawriter_keys(&tokens);

    if (result) {
        result = crypto->crypto_key_exchange->set_remote_datawriter_crypto_tokens(
                crypto->crypto_key_exchange,
                local_reader_crypto,
                remote_writer_crypto,
                &tokens,
                &exception);
    }

    DDS_Security_DataHolderSeq_deinit((DDS_Security_DataHolderSeq*)&tokens);

    return result ? 0 : -1;
}

static int set_remote_datareader_tokens(void)
{
    DDS_Security_boolean result;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_DatareaderCryptoTokenSeq tokens;

    memset(&tokens, 0, sizeof(tokens));

    /* Now call the function. */
    result = crypto->crypto_key_exchange->create_local_datareader_crypto_tokens(
                crypto->crypto_key_exchange,
                &tokens,
                local_reader_crypto,
                remote_writer_crypto,
                &exception);

    if (result)
        result = retrieve_datareader_keys(&tokens);
    if (result) {
        result = crypto->crypto_key_exchange->set_remote_datareader_crypto_tokens(
                crypto->crypto_key_exchange,
                local_writer_crypto,
                remote_reader_crypto,
                &tokens,
                &exception);
    }

    DDS_Security_DataHolderSeq_deinit((DDS_Security_DataHolderSeq*)&tokens);

    return result ? 0 : -1;
}

static void reset_exception(DDS_Security_SecurityException *ex)
{
    ex->code = 0;
    ex->minor_code = 0;
    ddsrt_free(ex->message);
    ex->message = NULL;
}


static void suite_preprocess_secure_submsg_init (void)
{
    allocate_shared_secret();
    memset(&writer_key_message, 0, sizeof(DDS_Security_KeyMaterial_AES_GCM_GMAC));
    memset(&writer_key_payload, 0, sizeof(DDS_Security_KeyMaterial_AES_GCM_GMAC));
    memset(&reader_key_message, 0, sizeof(DDS_Security_KeyMaterial_AES_GCM_GMAC));

    CU_ASSERT_FATAL ((plugins = load_plugins(
                            NULL      /* Access Control */,
                            NULL      /* Authentication */,
                            &crypto   /* Cryptography   */,
                            NULL)) != NULL);
    CU_ASSERT_EQUAL_FATAL (register_local_participant(), 0);
    CU_ASSERT_EQUAL_FATAL (register_remote_participant(), 0);
    CU_ASSERT_EQUAL_FATAL (register_local_datawriter(), 0);
    CU_ASSERT_EQUAL_FATAL (register_local_datareader(), 0);
    CU_ASSERT_EQUAL_FATAL (register_remote_datareader(), 0);
    CU_ASSERT_EQUAL_FATAL (register_remote_datawriter(), 0);
    CU_ASSERT_EQUAL_FATAL (set_remote_datawriter_tokens(), 0);
    CU_ASSERT_EQUAL_FATAL (set_remote_datareader_tokens(), 0);
}

static void suite_preprocess_secure_submsg_fini (void)
{
    DDS_Security_SecurityException exception = {NULL, 0, 0};

    if (remote_writer_crypto) {
        crypto->crypto_key_factory->unregister_datawriter(crypto->crypto_key_factory, remote_writer_crypto, &exception);
        reset_exception(&exception);
    }
    if (remote_reader_crypto) {
        crypto->crypto_key_factory->unregister_datareader(crypto->crypto_key_factory, remote_reader_crypto, &exception);
        reset_exception(&exception);
    }
    if (local_reader_crypto) {
        crypto->crypto_key_factory->unregister_datareader(crypto->crypto_key_factory, local_reader_crypto, &exception);
        reset_exception(&exception);
    }
    if (local_writer_crypto) {
        crypto->crypto_key_factory->unregister_datawriter(crypto->crypto_key_factory, local_writer_crypto, &exception);
        reset_exception(&exception);
    }
    if (remote_participant_handle) {
        crypto->crypto_key_factory->unregister_participant(crypto->crypto_key_factory, remote_participant_handle, &exception);
        reset_exception(&exception);
    }
    if (local_participant_handle) {
        crypto->crypto_key_factory->unregister_datareader(crypto->crypto_key_factory, local_participant_handle, &exception);
        reset_exception(&exception);
    }

    DDS_Security_KeyMaterial_AES_GCM_GMAC_deinit(&reader_key_message);
    DDS_Security_KeyMaterial_AES_GCM_GMAC_deinit(&writer_key_message);
    deallocate_shared_secret();
    unload_plugins(plugins);
}

static unsigned char submsg_header_endianness_flag (enum ddsrt_byte_order_selector bo)
{
#if DDSRT_ENDIAN == DDSRT_LITTLE_ENDIAN
  return (unsigned char) ((bo == DDSRT_BOSEL_BE) ? 0 : DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS);
#else
  return (unsigned char) ((bo == DDSRT_BO_LE) ? DDSI_RTPS_SUBMESSAGE_FLAG_ENDIANNESS : 0);
#endif
}

static void create_encoded_submsg(DDS_Security_OctetSeq *msg, DDS_Security_CryptoTransformKeyId key_id, DDS_Security_CryptoTransformKind transform_kind, unsigned char msg_id, enum ddsrt_byte_order_selector bo)
{
    unsigned char *buffer;
    uint32_t length = sizeof(SubMessageHeader) + sizeof(CryptoHeader) + 200;
    SubMessageHeader *submsg;
    CryptoHeader *crpthdr;

    buffer = ddsrt_malloc(length);
    submsg = (SubMessageHeader *) buffer;
    crpthdr = (CryptoHeader *) (submsg + 1);

    submsg->kind = msg_id;
    submsg->flags = submsg_header_endianness_flag(bo);
    submsg->octetsToNextSubMsg = ddsrt_toBO2u(bo, (uint16_t)(length - 24));

    memcpy(crpthdr->key_id, key_id, 4);
    memcpy(crpthdr->transform_id, transform_kind, 4);

    msg->_buffer = buffer;
    msg->_length = msg->_maximum = length;
}

static void clear_encoded_submsg(DDS_Security_OctetSeq *msg)
{
    if (msg) {
        ddsrt_free(msg->_buffer);
        memset(msg, 0, sizeof(*msg));
    }
}


CU_Test(ddssec_builtin_preprocess_secure_submsg, writer_happy_day, .init = suite_preprocess_secure_submsg_init, .fini = suite_preprocess_secure_submsg_fini)
{
    DDS_Security_boolean result;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_DatawriterCryptoHandle writer_crypto;
    DDS_Security_DatareaderCryptoHandle reader_crypto;
    DDS_Security_SecureSubmessageCategory_t category;
    DDS_Security_OctetSeq message;

    CU_ASSERT_FATAL (crypto != NULL);
    assert(crypto != NULL);
    CU_ASSERT_FATAL (crypto->crypto_transform != NULL);
    assert(crypto->crypto_transform != NULL);
    CU_ASSERT_FATAL (crypto->crypto_transform->preprocess_secure_submsg != NULL);
    assert(crypto->crypto_transform->preprocess_secure_submsg != 0);

    create_encoded_submsg(&message, writer_key_message.sender_key_id, writer_key_message.transformation_kind, VALID_DDSI_RTPS_SMID_SEC_PREFIX, DDSRT_BOSEL_NATIVE);

    result = crypto->crypto_transform->preprocess_secure_submsg(
                crypto->crypto_transform,
                &writer_crypto,
                &reader_crypto,
                &category,
                &message,
                local_participant_handle,
                remote_participant_handle,
                &exception);

    if (!result)
        printf("preprocess_secure_submsg: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT_FATAL(result);
    CU_ASSERT(exception.code == 0);
    CU_ASSERT(exception.message == NULL);
    CU_ASSERT(writer_crypto == remote_writer_crypto);
    CU_ASSERT(reader_crypto == local_reader_crypto);
    CU_ASSERT(category == DDS_SECURITY_DATAWRITER_SUBMESSAGE);

    reset_exception(&exception);

    clear_encoded_submsg(&message);

    create_encoded_submsg(&message, writer_key_message.sender_key_id, writer_key_message.transformation_kind, VALID_DDSI_RTPS_SMID_SEC_PREFIX, DDSRT_BOSEL_BE);

    result = crypto->crypto_transform->preprocess_secure_submsg(
                crypto->crypto_transform,
                &writer_crypto,
                &reader_crypto,
                &category,
                &message,
                local_participant_handle,
                remote_participant_handle,
                &exception);

    if (!result)
        printf("preprocess_secure_submsg: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT_FATAL(result);
    CU_ASSERT(exception.code == 0);
    CU_ASSERT(exception.message == NULL);
    CU_ASSERT(writer_crypto == remote_writer_crypto);
    CU_ASSERT(reader_crypto == local_reader_crypto);
    CU_ASSERT(category == DDS_SECURITY_DATAWRITER_SUBMESSAGE);

    reset_exception(&exception);

    clear_encoded_submsg(&message);
}

CU_Test(ddssec_builtin_preprocess_secure_submsg, reader_happy_day, .init = suite_preprocess_secure_submsg_init, .fini = suite_preprocess_secure_submsg_fini)
{
    DDS_Security_boolean result;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_DatawriterCryptoHandle writer_crypto;
    DDS_Security_DatareaderCryptoHandle reader_crypto;
    DDS_Security_SecureSubmessageCategory_t category;
    DDS_Security_OctetSeq message;

    CU_ASSERT_FATAL (crypto != NULL);
    assert(crypto != NULL);
    CU_ASSERT_FATAL (crypto->crypto_transform != NULL);
    assert(crypto->crypto_transform != NULL);
    CU_ASSERT_FATAL (crypto->crypto_transform->preprocess_secure_submsg != NULL);
    assert(crypto->crypto_transform->preprocess_secure_submsg != 0);

    create_encoded_submsg(&message, reader_key_message.sender_key_id, reader_key_message.transformation_kind, VALID_DDSI_RTPS_SMID_SEC_PREFIX, DDSRT_BOSEL_NATIVE);

    result = crypto->crypto_transform->preprocess_secure_submsg(
                crypto->crypto_transform,
                &writer_crypto,
                &reader_crypto,
                &category,
                &message,
                local_participant_handle,
                remote_participant_handle,
                &exception);

    if (!result)
        printf("preprocess_secure_submsg: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT_FATAL(result);
    CU_ASSERT(exception.code == 0);
    CU_ASSERT(exception.message == NULL);
    CU_ASSERT(writer_crypto == local_writer_crypto);
    CU_ASSERT(reader_crypto == remote_reader_crypto);
    CU_ASSERT(category == DDS_SECURITY_DATAREADER_SUBMESSAGE);

    reset_exception(&exception);
    clear_encoded_submsg(&message);
    create_encoded_submsg(&message, reader_key_message.sender_key_id, reader_key_message.transformation_kind, VALID_DDSI_RTPS_SMID_SEC_PREFIX, DDSRT_BOSEL_BE);
    result = crypto->crypto_transform->preprocess_secure_submsg(
                crypto->crypto_transform,
                &writer_crypto,
                &reader_crypto,
                &category,
                &message,
                local_participant_handle,
                remote_participant_handle,
                &exception);

    if (!result)
        printf("preprocess_secure_submsg: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT_FATAL(result);
    CU_ASSERT(exception.code == 0);
    CU_ASSERT(exception.message == NULL);
    CU_ASSERT(writer_crypto == local_writer_crypto);
    CU_ASSERT(reader_crypto == remote_reader_crypto);
    CU_ASSERT(category == DDS_SECURITY_DATAREADER_SUBMESSAGE);

    reset_exception(&exception);
    clear_encoded_submsg(&message);
}


CU_Test(ddssec_builtin_preprocess_secure_submsg, invalid_args, .init = suite_preprocess_secure_submsg_init, .fini = suite_preprocess_secure_submsg_fini)
{
    DDS_Security_boolean result;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_DatawriterCryptoHandle writer_crypto;
    DDS_Security_DatareaderCryptoHandle reader_crypto;
    DDS_Security_SecureSubmessageCategory_t category;
    DDS_Security_OctetSeq message;

    CU_ASSERT_FATAL (crypto != NULL);
    assert(crypto != NULL);
    CU_ASSERT_FATAL (crypto->crypto_transform != NULL);
    assert(crypto->crypto_transform != NULL);
    CU_ASSERT_FATAL (crypto->crypto_transform->preprocess_secure_submsg != NULL);
    assert(crypto->crypto_transform->preprocess_secure_submsg != 0);

    create_encoded_submsg(&message, writer_key_message.sender_key_id, reader_key_message.transformation_kind, VALID_DDSI_RTPS_SMID_SEC_PREFIX, DDSRT_BOSEL_NATIVE);

    /* remote_participant_handle = DDS_SECURITY_HANDLE_NIL */
    result = crypto->crypto_transform->preprocess_secure_submsg(
            crypto->crypto_transform,
            &writer_crypto,
            &reader_crypto,
            &category,
            &message,
            local_participant_handle,
            0,
            &exception);

    if (!result)
        printf("preprocess_secure_submsg: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);

    /* unknown remote_participant_handle  */
    result = crypto->crypto_transform->preprocess_secure_submsg(
            crypto->crypto_transform,
            &writer_crypto,
            &reader_crypto,
            &category,
            &message,
            local_participant_handle,
            1,
            &exception);

    if (!result)
        printf("preprocess_secure_submsg: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    clear_encoded_submsg(&message);
}


CU_Test(ddssec_builtin_preprocess_secure_submsg, invalid_message, .init = suite_preprocess_secure_submsg_init, .fini = suite_preprocess_secure_submsg_fini)
{
    DDS_Security_boolean result;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_DatawriterCryptoHandle writer_crypto;
    DDS_Security_DatareaderCryptoHandle reader_crypto;
    DDS_Security_SecureSubmessageCategory_t category;
    DDS_Security_OctetSeq message;

    CU_ASSERT_FATAL (crypto != NULL);
    assert(crypto != NULL);
    CU_ASSERT_FATAL (crypto->crypto_transform != NULL);
    assert(crypto->crypto_transform != NULL);
    CU_ASSERT_FATAL (crypto->crypto_transform->preprocess_secure_submsg != NULL);
    assert(crypto->crypto_transform->preprocess_secure_submsg != 0);

    /* unknown key id */
    create_encoded_submsg(&message, writer_key_payload.sender_key_id, writer_key_payload.transformation_kind, VALID_DDSI_RTPS_SMID_SEC_PREFIX, DDSRT_BOSEL_NATIVE);
    result = crypto->crypto_transform->preprocess_secure_submsg(
            crypto->crypto_transform,
            &writer_crypto,
            &reader_crypto,
            &category,
            &message,
            local_participant_handle,
            remote_participant_handle,
            &exception);

    if (!result)
        printf("preprocess_secure_submsg: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT(!result);
    CU_ASSERT(exception.code != 0);
    CU_ASSERT(exception.message != NULL);
    reset_exception(&exception);
    clear_encoded_submsg(&message);

    /* invalid transformation kind */
    {
        DDS_Security_CryptoTransformKind kind = {5, 1, 3, 6};

        create_encoded_submsg(&message, writer_key_message.sender_key_id, kind, VALID_DDSI_RTPS_SMID_SEC_PREFIX, DDSRT_BOSEL_NATIVE);

        result = crypto->crypto_transform->preprocess_secure_submsg(
                crypto->crypto_transform,
                &writer_crypto,
                &reader_crypto,
                &category,
                &message,
                local_participant_handle,
                remote_participant_handle,
                &exception);

        if (!result)
            printf("preprocess_secure_submsg: %s\n", exception.message ? exception.message : "Error message missing");

        CU_ASSERT(!result);
        CU_ASSERT(exception.code != 0);
        CU_ASSERT(exception.message != NULL);
        reset_exception(&exception);
        clear_encoded_submsg(&message);
    }

    /* not expected submessage id */
    {
        DDS_Security_CryptoTransformKind kind = {5, 1, 3, 6};
        create_encoded_submsg(&message, writer_key_message.sender_key_id, kind, INVALID_DDSI_RTPS_SMID_SEC_PREFIX, DDSRT_BOSEL_NATIVE);
        result = crypto->crypto_transform->preprocess_secure_submsg(
                crypto->crypto_transform,
                &writer_crypto,
                &reader_crypto,
                &category,
                &message,
                local_participant_handle,
                remote_participant_handle,
                &exception);

        if (!result)
            printf("preprocess_secure_submsg: %s\n", exception.message ? exception.message : "Error message missing");

        CU_ASSERT(!result);
        CU_ASSERT(exception.code != 0);
        CU_ASSERT(exception.message != NULL);
        reset_exception(&exception);
        clear_encoded_submsg(&message);
    }
}


CU_Test(ddssec_builtin_preprocess_secure_submsg, volatile_secure, .init = suite_preprocess_secure_submsg_init, .fini = suite_preprocess_secure_submsg_fini)
{
    DDS_Security_boolean result;
    DDS_Security_SecurityException exception = {NULL, 0, 0};
    DDS_Security_DatareaderCryptoHandle local_reader_crypto_vol;
    DDS_Security_DatawriterCryptoHandle local_writer_crypto_vol;
    DDS_Security_DatareaderCryptoHandle remote_reader_crypto_vol;
    DDS_Security_DatawriterCryptoHandle remote_writer_crypto_vol;
    DDS_Security_PropertySeq datareader_properties;
    DDS_Security_EndpointSecurityAttributes datareader_security_attributes;
    DDS_Security_PropertySeq datawriter_properties;
    DDS_Security_EndpointSecurityAttributes datawriter_security_attributes;
    DDS_Security_DatawriterCryptoHandle writer_crypto;
    DDS_Security_DatareaderCryptoHandle reader_crypto;
    DDS_Security_SecureSubmessageCategory_t category;
    DDS_Security_CryptoTransformKeyId key_id = {0, 0, 0, 0};
    DDS_Security_OctetSeq message;

    CU_ASSERT_FATAL (crypto != NULL);
    assert(crypto != NULL);
    CU_ASSERT_FATAL (crypto->crypto_transform != NULL);
    assert(crypto->crypto_transform != NULL);
    CU_ASSERT_FATAL (crypto->crypto_transform->preprocess_secure_submsg != NULL);
    assert(crypto->crypto_transform->preprocess_secure_submsg != 0);

    datareader_properties._length = datareader_properties._maximum = 1;
    datareader_properties._buffer = DDS_Security_PropertySeq_allocbuf(1);
    datareader_properties._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_BUILTIN_ENDPOINT_NAME);
    datareader_properties._buffer[0].value = ddsrt_strdup("BuiltinParticipantVolatileMessageSecureReader");
    datareader_properties._buffer[0].propagate = false;

    prepare_endpoint_security_attributes( &datareader_security_attributes );
    prepare_endpoint_security_attributes( &datawriter_security_attributes );

    datawriter_properties._length = datawriter_properties._maximum = 1;
    datawriter_properties._buffer = DDS_Security_PropertySeq_allocbuf(1);
    datawriter_properties._buffer[0].name = ddsrt_strdup(DDS_SEC_PROP_BUILTIN_ENDPOINT_NAME);
    datawriter_properties._buffer[0].value = ddsrt_strdup("BuiltinParticipantVolatileMessageSecureWriter");
    datawriter_properties._buffer[0].propagate = false;

    local_writer_crypto_vol =
            crypto->crypto_key_factory->register_local_datawriter(
                    crypto->crypto_key_factory,
                    local_participant_handle,
                    &datawriter_properties,
                    &datawriter_security_attributes,
                    &exception);
    CU_ASSERT_FATAL(local_writer_crypto_vol != 0);

    local_reader_crypto_vol =
            crypto->crypto_key_factory->register_local_datareader(
                    crypto->crypto_key_factory,
                    local_participant_handle,
                    &datareader_properties,
                    &datareader_security_attributes,
                    &exception);
    CU_ASSERT_FATAL(local_reader_crypto_vol != 0);

    remote_writer_crypto_vol =
            crypto->crypto_key_factory->register_matched_remote_datawriter(
                    crypto->crypto_key_factory,
                    local_reader_crypto_vol,
                    remote_participant_handle,
                    shared_secret_handle,
                    &exception);
    CU_ASSERT_FATAL(remote_writer_crypto_vol != 0);

    remote_reader_crypto_vol =
            crypto->crypto_key_factory->register_matched_remote_datareader(
                    crypto->crypto_key_factory,
                    local_writer_crypto_vol,
                    remote_participant_handle,
                    shared_secret_handle,
                    true,
                    &exception);
    CU_ASSERT_FATAL(remote_reader_crypto_vol != 0);

    create_encoded_submsg(&message, key_id, reader_key_message.transformation_kind, VALID_DDSI_RTPS_SMID_SEC_PREFIX, DDSRT_BOSEL_NATIVE);

    result = crypto->crypto_transform->preprocess_secure_submsg(
                crypto->crypto_transform,
                &writer_crypto,
                &reader_crypto,
                &category,
                &message,
                local_participant_handle,
                remote_participant_handle,
                &exception);

    if (!result)
        printf("preprocess_secure_submsg: %s\n", exception.message ? exception.message : "Error message missing");

    CU_ASSERT_FATAL(result);
    CU_ASSERT(exception.code == 0);
    CU_ASSERT(exception.message == NULL);
    CU_ASSERT(((remote_datawriter_crypto *)writer_crypto)->is_builtin_participant_volatile_message_secure_writer);
    CU_ASSERT(((local_datareader_crypto *)reader_crypto)->is_builtin_participant_volatile_message_secure_reader);
    CU_ASSERT(category == DDS_SECURITY_DATAWRITER_SUBMESSAGE);

    reset_exception(&exception);

    if (remote_writer_crypto_vol) {
        crypto->crypto_key_factory->unregister_datawriter(crypto->crypto_key_factory, remote_writer_crypto_vol, &exception);
        reset_exception(&exception);
    }
    if (remote_reader_crypto_vol) {
        crypto->crypto_key_factory->unregister_datareader(crypto->crypto_key_factory, remote_reader_crypto_vol, &exception);
        reset_exception(&exception);
    }
    if (local_reader_crypto_vol) {
        crypto->crypto_key_factory->unregister_datareader(crypto->crypto_key_factory, local_reader_crypto_vol, &exception);
        reset_exception(&exception);
    }
    if (local_writer_crypto_vol) {
        crypto->crypto_key_factory->unregister_datawriter(crypto->crypto_key_factory, local_writer_crypto_vol, &exception);
        reset_exception(&exception);
    }

    clear_encoded_submsg(&message);
    DDS_Security_PropertySeq_deinit(&datareader_properties);
    DDS_Security_PropertySeq_deinit(&datawriter_properties);
}
