// Copyright(c) 2006 to 2020 ZettaScale Technology and others
//
// This program and the accompanying materials are made available under the
// terms of the Eclipse Public License v. 2.0 which is available at
// http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
// v. 1.0 which is available at
// http://www.eclipse.org/org/documents/edl-v10.php.
//
// SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause

#ifndef CRYPTO_KEY_FACTORY_H
#define CRYPTO_KEY_FACTORY_H

#include "dds/security/dds_security_api.h"
#include "crypto_objects.h"
#include "dds/security/export.h"

/**
 * @brief Allocation function for implementer structure (with internal variables) transparently.
 */
dds_security_crypto_key_factory *
dds_security_crypto_key_factory__alloc(
    const dds_security_cryptography *crypto);

void dds_security_crypto_key_factory__dealloc(
    dds_security_crypto_key_factory *instance);

int generate_key_pairs(
    char **private_key,
    char **public_key);

bool crypto_factory_get_protection_kind(
    const dds_security_crypto_key_factory *factory,
    int64_t handle,
    DDS_Security_ProtectionKind *kind);

bool crypto_factory_get_participant_crypto_tokens(
    const dds_security_crypto_key_factory *factory,
    DDS_Security_ParticipantCryptoHandle local_id,
    DDS_Security_ParticipantCryptoHandle remote_id,
    participant_key_material **pp_key_material,
    master_key_material **remote_key_matarial,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex);

bool crypto_factory_set_participant_crypto_tokens(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_ParticipantCryptoHandle local_id,
    const DDS_Security_ParticipantCryptoHandle remote_id,
    const DDS_Security_KeyMaterial_AES_GCM_GMAC *remote_key_mat,
    DDS_Security_SecurityException *ex);

bool crypto_factory_get_datawriter_crypto_tokens(
    const dds_security_crypto_key_factory *factory,
    DDS_Security_DatawriterCryptoHandle local_writer_handle,
    DDS_Security_DatareaderCryptoHandle remote_reader_handle,
    master_key_material **key_mat,
    uint32_t *num_key_mat,
    DDS_Security_SecurityException *ex);

bool crypto_factory_set_datawriter_crypto_tokens(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatawriterCryptoHandle local_reader_handle,
    const DDS_Security_DatareaderCryptoHandle remote_writer_handle,
    const DDS_Security_KeyMaterial_AES_GCM_GMAC *key_mat,
    const uint32_t num_key_mat,
    DDS_Security_SecurityException *ex);

bool crypto_factory_get_datareader_crypto_tokens(
    const dds_security_crypto_key_factory *factory,
    DDS_Security_DatawriterCryptoHandle local_reader_handle,
    DDS_Security_DatareaderCryptoHandle remote_writer_handle,
    master_key_material **key_mat,
    DDS_Security_SecurityException *ex);

bool crypto_factory_set_datareader_crypto_tokens(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatawriterCryptoHandle local_writer_handle,
    const DDS_Security_DatareaderCryptoHandle remote_reader_handle,
    const DDS_Security_KeyMaterial_AES_GCM_GMAC *key_mat,
    DDS_Security_SecurityException *ex);

bool crypto_factory_get_writer_key_material(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatawriterCryptoHandle writer_id,
    const DDS_Security_DatareaderCryptoHandle reader_id,
    bool payload,
    session_key_material **session_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex);

bool crypto_factory_get_reader_key_material(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatareaderCryptoHandle reader_id,
    const DDS_Security_DatawriterCryptoHandle writer_id,
    session_key_material **session_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex);

bool crypto_factory_get_remote_writer_key_material(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatareaderCryptoHandle reader_id,
    const DDS_Security_DatawriterCryptoHandle writer_id,
    uint32_t key_id,
    master_key_material **master_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_BasicProtectionKind *basic_protection_kind,
    DDS_Security_SecurityException *ex);

bool crypto_factory_get_local_participant_data_key_material(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_ParticipantCryptoHandle local_id,
    session_key_material **session_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex);

bool crypto_factory_get_remote_reader_key_material(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatawriterCryptoHandle writer_id,
    const DDS_Security_DatareaderCryptoHandle reader_id,
    uint32_t key_id,
    master_key_material **master_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex);

bool crypto_factory_get_remote_writer_sign_key_material(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatareaderCryptoHandle writer_id,
    master_key_material **key_material,
    session_key_material **session_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex);

bool crypto_factory_get_remote_reader_sign_key_material(
    const dds_security_crypto_key_factory *factory,
    const DDS_Security_DatareaderCryptoHandle reader_id,
    master_key_material **key_material,
    session_key_material **session_key,
    DDS_Security_ProtectionKind *protection_kind,
    DDS_Security_SecurityException *ex);

bool crypto_factory_get_endpoint_relation(
    const dds_security_crypto_key_factory *factory,
    DDS_Security_ParticipantCryptoHandle local_participant_handle,
    DDS_Security_ParticipantCryptoHandle remote_participant_handle,
    uint32_t key_id,
    DDS_Security_Handle *remote_handle,
    DDS_Security_Handle *local_handle,
    DDS_Security_SecureSubmessageCategory_t *category,
    DDS_Security_SecurityException *ex);

bool
crypto_factory_get_specific_keymat(
    const dds_security_crypto_key_factory *factory,
    CryptoObjectKind_t kind,
    DDS_Security_Handle rmt_handle,
    const struct receiver_specific_mac * const mac_list,
    uint32_t num_mac,
    uint32_t *index,
    master_key_material **key_mat);

SECURITY_EXPORT master_key_material *
crypto_factory_get_master_key_material_for_test(
    const dds_security_crypto_key_factory *factory,
    DDS_Security_ParticipantCryptoHandle local_id,
    DDS_Security_ParticipantCryptoHandle remote_id);

#endif /* CRYPTO_KEY_FACTORY_H */
