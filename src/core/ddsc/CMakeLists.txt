#
# Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
PREPEND(srcs_ddsc "${CMAKE_CURRENT_LIST_DIR}/src"
    dds_alloc.c
    dds_builtin.c
    dds_coherent.c
    dds_iid.c
    dds_participant.c
    dds_reader.c
    dds_writer.c
    dds_init.c
    dds_publisher.c
    dds_rhc.c
    dds_time.c
    q_osplser.c
    dds_domain.c
    dds_instance.c
    dds_qos.c
    dds_tkmap.c
    dds_entity.c
    dds_key.c
    dds_querycond.c
    dds_topic.c
    dds_report.c
    dds_err.c
    dds_listener.c
    dds_read.c
    dds_stream.c
    dds_waitset.c
    dds_log.c
    dds_readcond.c
    dds_subscriber.c
    dds_write.c
)
 
PREPEND(hdrs_public_ddsc "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include/ddsc>$<INSTALL_INTERFACE:include/ddsc>"
    dds.h
    dds_public_error.h
    dds_public_impl.h
    dds_public_listener.h
    dds_public_log.h
    dds_public_qos.h
    dds_public_status.h
    dds_public_stream.h
    dds_public_time.h
)

PREPEND(hdrs_private_ddsc "${CMAKE_CURRENT_LIST_DIR}/src"
    dds__alloc.h
    dds__builtin.h
    dds__domain.h
    dds__entity.h
    dds__iid.h
    dds__init.h
    dds__key.h
    dds__listener.h
    dds__participant.h
    dds__publisher.h
    dds__qos.h
    dds__querycond.h
    dds__readcond.h
    dds__reader.h
    dds__report.h
    dds__rhc.h
    dds__stream.h
    dds__subscriber.h
    dds__tkmap.h
    dds__topic.h
    dds__types.h
    dds__write.h
    dds__writer.h
    q__osplser.h
)

configure_file(
    "${CMAKE_CURRENT_LIST_DIR}/cmake/ddsc_project.h.in"
    "include/ddsc/ddsc_project.h")

generate_export_header(
    ddsc
    BASE_NAME DDS
    EXPORT_FILE_NAME "${CMAKE_CURRENT_BINARY_DIR}/include/ddsc/dds_export.h"
)

target_include_directories(ddsc
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")

# Generate builtin-topic sources
set(IDLC_ARGS "-dll" "FOO,ddsc/dds_export.h")
idlc_generate(BuiltinTypes ddsc/src/dds_dcps_builtintopics.idl ddsc/src/dds_builtinTopics.idl)
set(IDLC_ARGS)
target_link_libraries(ddsc PRIVATE BuiltinTypes)

target_include_directories(ddsc
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>")

target_sources(ddsc
    PRIVATE
        ${srcs_ddsc}
        ${hdrs_private_ddsc}
        "include/ddsc/ddsc_project.h"
    PUBLIC
        ${hdrs_public_ddsc}
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>$<INSTALL_INTERFACE:include>/ddsc/dds_export.h"
)

target_include_directories(ddsc
    PUBLIC
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
        "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/../security/include>"
    PRIVATE
        "${CMAKE_CURRENT_LIST_DIR}/src")

install(
  DIRECTORY "${CMAKE_CURRENT_LIST_DIR}/include/ddsc"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev)

install(
  FILES "${CMAKE_CURRENT_BINARY_DIR}/include/ddsc/dds_export.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ddsc"
  COMPONENT dev)

install(
  FILES
    "${CMAKE_CURRENT_BINARY_DIR}/dds_dcps_builtintopics.h"
    "${CMAKE_CURRENT_BINARY_DIR}/dds_builtinTopics.h"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/ddsc"
  COMPONENT dev)

# TODO: improve test inclusion.
if((BUILD_TESTING) AND ((NOT DEFINED MSVC_VERSION) OR (MSVC_VERSION GREATER "1800")))
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/tests")
endif()

