 #
# Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
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
    "validate_begin_handshake_reply/src/validate_begin_handshake_reply_utests.c"
    "get_authenticated_peer_credential_token/src/get_authenticated_peer_credential_token_utests.c"
    "validate_local_identity/src/validate_local_identity_utests.c"
    "validate_remote_identity/src/validate_remote_identity_utests.c"
    "validate_begin_handshake_request/src/validate_begin_handshake_request_utests.c"
    "process_handshake/src/process_handshake_utests.c"
    "common/src/handshake_helper.c"
    "common/src/loader.c")

add_cunit_executable(cunit_security_plugins ${security_auth_test_sources})
target_include_directories(
  cunit_security_plugins PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/include/>"
       "$<BUILD_INTERFACE:$<TARGET_PROPERTY:security_api,INTERFACE_INCLUDE_DIRECTORIES>>"
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:security_core,INTERFACE_INCLUDE_DIRECTORIES>>"
        "$<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsrt,INTERFACE_INCLUDE_DIRECTORIES>>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
 )


target_link_libraries(cunit_security_plugins PRIVATE ddsc security_api)


find_package(OpenSSL )
if(OPENSSL_FOUND)
  target_link_libraries(cunit_security_plugins PRIVATE OpenSSL::SSL)

else()
  message(FATAL_ERROR "To build without openssl support, set DDSC_ENABLE_OPENSSL to OFF")
endif()

set(CUnit_builtin_plugins_tests_dir "${CMAKE_CURRENT_LIST_DIR}")

configure_file("config_env.h.in" "config_env.h")


