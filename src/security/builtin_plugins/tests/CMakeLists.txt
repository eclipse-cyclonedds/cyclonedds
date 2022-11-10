 #
# Copyright(c) 2006 to 2020 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
include(CUnit)

set(security_auth_test_sources
    "common/src/loader.c"
    "common/src/handshake_helper.c"
    "get_authenticated_peer_credential_token/src/get_authenticated_peer_credential_token_utests.c"
    "process_handshake/src/process_handshake_utests.c"
    "validate_begin_handshake_reply/src/validate_begin_handshake_reply_utests.c"
    "validate_begin_handshake_request/src/validate_begin_handshake_request_utests.c"
    "validate_local_identity/src/validate_local_identity_utests.c"
    "validate_remote_identity/src/validate_remote_identity_utests.c"
    "listeners_authentication/src/listeners_authentication_utests.c"
    "listeners_access_control/src/listeners_access_control_utests.c"
)

set(security_ac_test_sources
    "access_control_fnmatch/src/access_control_fnmatch_utests.c"
    "get_permissions_credential_token/src/get_permissions_credential_token_utests.c"
    "get_permissions_token/src/get_permissions_token_utests.c"
    "get_xxx_sec_attributes/src/get_xxx_sec_attributes_utests.c"
    # "listeners_access_control/src/listeners_access_control_utests.c"
    "validate_local_permissions/src/validate_local_permissions_utests.c"
    "validate_remote_permissions/src/validate_remote_permissions_utests.c"
)

set(security_crypto_test_sources
    "common/src/crypto_helper.c"
    "create_local_datareader_crypto_tokens/src/create_local_datareader_crypto_tokens_utests.c"
    "create_local_datawriter_crypto_tokens/src/create_local_datawriter_crypto_tokens_utests.c"
    "create_local_participant_crypto_tokens/src/create_local_participant_crypto_tokens_utests.c"
    "decode_datareader_submessage/src/decode_datareader_submessage_utests.c"
    "decode_datawriter_submessage/src/decode_datawriter_submessage_utests.c"
    "decode_rtps_message/src/decode_rtps_message_utests.c"
    "decode_serialized_payload/src/decode_serialized_payload_utests.c"
    "encode_datareader_submessage/src/encode_datareader_submessage_utests.c"
    "encode_datawriter_submessage/src/encode_datawriter_submessage_utests.c"
    "encode_rtps_message/src/encode_rtps_message_utests.c"
    "encode_serialized_payload/src/encode_serialized_payload_utests.c"
    "preprocess_secure_submsg/src/preprocess_secure_submsg_utests.c"
    "register_local_datareader/src/register_local_datareader_utests.c"
    "register_local_datawriter/src/register_local_datawriter_utests.c"
    "register_local_participant/src/register_local_participant_utests.c"
    "register_matched_remote_datareader/src/register_matched_remote_datareader_utests.c"
    "register_matched_remote_datawriter/src/register_matched_remote_datawriter_utests.c"
    "register_matched_remote_participant/src/register_matched_remote_participant_utests.c"
    "set_remote_datareader_crypto_tokens/src/set_remote_datareader_crypto_tokens_utests.c"
    "set_remote_datawriter_crypto_tokens/src/set_remote_datawriter_crypto_tokens_utests.c"
    "set_remote_participant_crypto_tokens/src/set_remote_participant_crypto_tokens_utests.c"
)

add_cunit_executable(cunit_security_plugins
  ${security_auth_test_sources}
  ${security_ac_test_sources}
  ${security_crypto_test_sources}
  ${CYCLONEDDS_OPENSSL_APPLINK})
add_coverage(cunit_security_plugins)

# applink.c triggers the dreaded This function or variable may be unsafe
if(NOT "${CYCLONEDDS_OPENSSL_APPLINK}" STREQUAL "")
  set_source_files_properties("${CYCLONEDDS_OPENSSL_APPLINK}" PROPERTIES COMPILE_FLAGS "-D_CRT_SECURE_NO_WARNINGS")
endif()

target_include_directories(
  cunit_security_plugins PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../access_control/src/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../cryptographic/src/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../../../core/ddsi/src>"
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/include/>"
  "$<BUILD_INTERFACE:$<TARGET_PROPERTY:security_api,INTERFACE_INCLUDE_DIRECTORIES>>"
  "$<BUILD_INTERFACE:$<TARGET_PROPERTY:security_core,INTERFACE_INCLUDE_DIRECTORIES>>"
  "$<BUILD_INTERFACE:$<TARGET_PROPERTY:security_openssl,INTERFACE_INCLUDE_DIRECTORIES>>"
  "$<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_INCLUDE_DIRECTORIES>>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
)
target_link_libraries(cunit_security_plugins PRIVATE ddsc security_api dds_security_ac dds_security_crypto)
target_link_libraries(cunit_security_plugins PRIVATE security_openssl)
target_link_libraries(cunit_security_plugins PRIVATE OpenSSL::SSL)
if(CMAKE_GENERATOR MATCHES "Visual Studio")
  set_target_properties(cunit_security_plugins PROPERTIES LINK_FLAGS "/ignore:4099")
endif()

set(CUnit_builtin_plugins_tests_dir "${CMAKE_CURRENT_LIST_DIR}")
set(CUnit_build_dir "${CMAKE_CURRENT_BINARY_DIR}")
configure_file("config_env.h.in" "config_env.h")
