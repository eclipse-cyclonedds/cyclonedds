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

idlc_generate(RoundTrip RoundTrip.idl)
idlc_generate(Space Space.idl)
idlc_generate(TypesArrayKey TypesArrayKey.idl)

set(ddsc_test_sources
    "basic.c"
    "builtin_topics.c"
    "config.c"
    "dispose.c"
    "entity_api.c"
    "entity_hierarchy.c"
    "entity_status.c"
    "err.c"
    "file_id.c"
    "instance_get_key.c"
    "listener.c"
    "participant.c"
    "publisher.c"
    "qos.c"
    "querycondition.c"
    "readcondition.c"
    "reader.c"
    "reader_iterator.c"
    "read_instance.c"
    "register.c"
    "return_loan.c"
    "subscriber.c"
    "take_instance.c"
    "time.c"
    "topic.c"
    "transientlocal.c"
    "types.c"
    "unregister.c"
    "unsupported.c"
    "waitset.c"
    "write.c"
    "writer.c")

add_cunit_executable(cunit_ddsc ${ddsc_test_sources})
target_include_directories(
  cunit_ddsc PRIVATE
  "$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/include/>")
target_link_libraries(cunit_ddsc RoundTrip Space TypesArrayKey ddsc OSAPI)

# Setup environment for config-tests
get_test_property(CUnit_ddsc_config_simple_udp ENVIRONMENT CUnit_ddsc_config_simple_udp_env)
set(CUnit_ddsc_config_simple_udp_file "${CMAKE_CURRENT_LIST_DIR}/config_simple_udp.xml")
set(CUnit_ddsc_config_simple_udp_uri "file://${CUnit_ddsc_config_simple_udp_file}")
set(CUnit_ddsc_config_simple_udp_max_participants "0")
set(CUnit_ddsc_config_simple_udp_env "${CMAKE_PROJECT_NAME_CAPS}_URI=${CUnit_ddsc_config_simple_udp_uri};MAX_PARTICIPANTS=${CUnit_ddsc_config_simple_udp_max_participants};${CUnit_ddsc_config_simple_udp_env}")

set_tests_properties(
	CUnit_ddsc_config_simple_udp
	PROPERTIES
		REQUIRED_FILES ${CUnit_ddsc_config_simple_udp_file}
		ENVIRONMENT "${CUnit_ddsc_config_simple_udp_env}")

configure_file("config_env.h.in" "config_env.h")
