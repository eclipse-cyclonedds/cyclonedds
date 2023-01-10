#
# Copyright(c) 2006 to 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

prepend(srcs_ddsc "${CMAKE_CURRENT_LIST_DIR}/src/"
  dds_alloc.c
  dds_builtin.c
  dds_coherent.c
  dds_participant.c
  dds_reader.c
  dds_writer.c
  dds_init.c
  dds_publisher.c
  dds_rhc.c
  dds_rhc_default.c
  dds_domain.c
  dds_instance.c
  dds_qos.c
  dds_handles.c
  dds_entity.c
  dds_matched.c
  dds_querycond.c
  dds_topic.c
  dds_listener.c
  dds_read.c
  dds_waitset.c
  dds_readcond.c
  dds_guardcond.c
  dds_statistics.c
  dds_subscriber.c
  dds_write.c
  dds_whc.c
  dds_whc_builtintopic.c
  dds_serdata_builtintopic.c
  dds_sertype_builtintopic.c
  dds_serdata_default.c
  dds_sertype_default.c
  dds_data_allocator.c
  dds_loan.c
)

if(ENABLE_TYPE_DISCOVERY)
  list(APPEND srcs_ddsc "${CMAKE_CURRENT_LIST_DIR}/src/dds_dynamic_type.c")
endif()

if(ENABLE_SHM)
  list(APPEND srcs_ddsc
    "${CMAKE_CURRENT_LIST_DIR}/src/dds_shm_monitor.c"
    "${CMAKE_CURRENT_LIST_DIR}/src/dds_shm_qos.c")
endif()

prepend(hdrs_private_ddsc "${CMAKE_CURRENT_LIST_DIR}/src/"
  dds__builtin.h
  dds__domain.h
  dds__handles.h
  dds__entity.h
  dds__init.h
  dds__listener.h
  dds__participant.h
  dds__publisher.h
  dds__qos.h
  dds__readcond.h
  dds__guardcond.h
  dds__reader.h
  dds__rhc_default.h
  dds__statistics.h
  dds__subscriber.h
  dds__topic.h
  dds__types.h
  dds__write.h
  dds__writer.h
  dds__whc.h
  dds__whc_builtintopic.h
  dds__serdata_builtintopic.h
  dds__serdata_default.h
  dds__get_status.h
  dds__data_allocator.h)

prepend(hdrs_public_ddsc "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>/dds/"
  dds.h
  ddsc/dds_public_error.h
  ddsc/dds_public_impl.h
  ddsc/dds_public_listener.h
  ddsc/dds_public_qos.h
  ddsc/dds_public_qosdefs.h
  ddsc/dds_public_status.h
  ddsc/dds_statistics.h
  ddsc/dds_rhc.h
  ddsc/dds_internal_api.h
  ddsc/dds_opcodes.h
  ddsc/dds_data_allocator.h
  ddsc/dds_loan_api.h)

if(ENABLE_TYPE_DISCOVERY)
  list(APPEND hdrs_public_ddsc "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>/dds/ddsc/dds_public_dynamic_type.h")
endif()

if(ENABLE_SHM)
  list(APPEND hdrs_private_ddsc
    "${CMAKE_CURRENT_LIST_DIR}/src/dds__shm_monitor.h"
    "${CMAKE_CURRENT_LIST_DIR}/src/dds__shm_qos.h")
endif()

generate_export_header(
  ddsc BASE_NAME DDS EXPORT_FILE_NAME include/dds/export.h)

target_sources(ddsc
  PRIVATE
    ${srcs_ddsc}
    ${hdrs_private_ddsc}
  PUBLIC
    ${hdrs_public_ddsc}
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>/dds/export.h")

target_include_directories(ddsc
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>"
  PRIVATE
    "${CMAKE_CURRENT_LIST_DIR}/src"
  INTERFACE
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

install(
  DIRECTORY
      "${CMAKE_CURRENT_LIST_DIR}/include/"
      "${CMAKE_CURRENT_BINARY_DIR}/include/"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev)

# TODO: improve test inclusion.
if((BUILD_TESTING) AND (BUILD_IDLC) AND ((NOT DEFINED MSVC_VERSION) OR (MSVC_VERSION GREATER "1800")))
  add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/tests")
endif()
