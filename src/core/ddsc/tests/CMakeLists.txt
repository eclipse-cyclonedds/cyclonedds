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
include(CUnit)

idlc_generate(TARGET RoundTrip FILES RoundTrip.idl)
idlc_generate(TARGET Space FILES Space.idl)
idlc_generate(TARGET TypesArrayKey FILES TypesArrayKey.idl)
idlc_generate(TARGET WriteTypes FILES WriteTypes.idl)
idlc_generate(TARGET InstanceHandleTypes FILES InstanceHandleTypes.idl)
idlc_generate(TARGET RWData FILES RWData.idl)
idlc_generate(TARGET CreateWriter FILES CreateWriter.idl)

set(ddsc_test_sources
    "basic.c"
    "builtin_topics.c"
    "cdr.c"
    "config.c"
    "data_avail_stress.c"
    "discstress.c"
    "dispose.c"
    "domain.c"
    "domain_torture.c"
    "entity_api.c"
    "entity_hierarchy.c"
    "entity_status.c"
    "err.c"
    "filter.c"
    "instance_get_key.c"
    "instance_handle.c"
    "listener.c"
    "liveliness.c"
    "loan.c"
    "multi_sertopic.c"
    "participant.c"
    "publisher.c"
    "qos.c"
    "qosmatch.c"
    "querycondition.c"
    "guardcondition.c"
    "readcondition.c"
    "reader.c"
    "reader_iterator.c"
    "read_instance.c"
    "register.c"
    "subscriber.c"
    "take_instance.c"
    "time.c"
    "topic.c"
    "topic_find_local.c"
    "transientlocal.c"
    "types.c"
    "unregister.c"
    "unsupported.c"
    "userdata.c"
    "waitset.c"
    "waitset_torture.c"
    "whc.c"
    "write.c"
    "write_various_types.c"
    "writer.c"
    "test_util.c"
    "test_util.h"
    "test_common.h"
    "test_common.c"
    "test_oneliner.c"
    "test_oneliner.h")

if(ENABLE_LIFESPAN)
  list(APPEND ddsc_test_sources "lifespan.c")
endif()

if(ENABLE_DEADLINE_MISSED)
  list(APPEND ddsc_test_sources "deadline.c")
endif()

if(ENABLE_TYPE_DISCOVERY)
  list(APPEND ddsc_test_sources "typelookup.c")
endif()

if(ENABLE_TOPIC_DISCOVERY)
  list(APPEND ddsc_test_sources
    "topic_discovery.c"
    "topic_find_global.c")
endif()

add_cunit_executable(cunit_ddsc ${ddsc_test_sources})
target_include_directories(
  cunit_ddsc PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsc/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/include>")
target_link_libraries(cunit_ddsc PRIVATE
  RoundTrip Space TypesArrayKey WriteTypes InstanceHandleTypes RWData CreateWriter ddsc)

# Setup environment for config-tests
get_test_property(CUnit_ddsc_config_simple_udp ENVIRONMENT CUnit_ddsc_config_simple_udp_env)
set(CUnit_ddsc_config_simple_udp_file "${CMAKE_CURRENT_LIST_DIR}/config_simple_udp.xml")
set(CUnit_ddsc_config_simple_udp_uri "file://${CUnit_ddsc_config_simple_udp_file}")
set(CUnit_ddsc_config_simple_udp_max_participants "0")
set(CUnit_ddsc_config_simple_udp_env "CYCLONEDDS_URI=${CUnit_ddsc_config_simple_udp_uri};MAX_PARTICIPANTS=${CUnit_ddsc_config_simple_udp_max_participants};${CUnit_ddsc_config_simple_udp_env}")
set_tests_properties(
	CUnit_ddsc_config_simple_udp
	PROPERTIES
		REQUIRED_FILES ${CUnit_ddsc_config_simple_udp_file}
		ENVIRONMENT "${CUnit_ddsc_config_simple_udp_env}")

configure_file("config_env.h.in" "config_env.h" @ONLY)

add_executable(oneliner
  "oneliner.c"
  "test_oneliner.c"
  "test_oneliner.h"
  "test_util.c"
  "test_util.h")
target_include_directories(
  oneliner PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/include/>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsc/src>"
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../ddsi/include>")
target_link_libraries(oneliner PRIVATE RoundTrip Space ddsc)
