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
include(Criterion)

idlc_generate(RoundTrip RoundTrip.idl)
idlc_generate(Space Space.idl)
idlc_generate(TypesArrayKey TypesArrayKey.idl)
add_criterion_executable(criterion_ddsc .)
target_include_directories(criterion_ddsc PRIVATE
		"$<BUILD_INTERFACE:${CMAKE_BINARY_DIR}/src/include/>")
target_link_libraries(criterion_ddsc RoundTrip Space TypesArrayKey ddsc OSAPI)

# Setup environment for config-tests
get_test_property(Criterion_ddsc_config_simple_udp ENVIRONMENT ENVIRON)
set(CONFIG_FILE "${CMAKE_CURRENT_LIST_DIR}/config_simple_udp.xml")
set(ENVIRON "${CMAKE_PROJECT_NAME_CAPS}_URI=file://${CONFIG_FILE};MAX_PARTICIPANTS=0;${ENVIRON}")

message(STATUS "Environment for Criterion_ddsc_config_simple_udp: ${ENVIRON}")

set_tests_properties(
	Criterion_ddsc_config_simple_udp
	PROPERTIES
		REQUIRED_FILES ${CONFIG_FILE}
		ENVIRONMENT "${ENVIRON}")

configure_file("config_env.h.in" "config_env.h")
