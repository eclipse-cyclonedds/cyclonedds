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
set(Criterion_ddsc_config_simple_udp_file "${CMAKE_CURRENT_LIST_DIR}/config_simple_udp.xml")
set(Criterion_ddsc_config_simple_udp_uri "file://${Criterion_ddsc_config_simple_udp_file}")
set(Criterion_ddsc_config_simple_udp_max_participants "0")
set_tests_properties(
	Criterion_ddsc_config_simple_udp
	PROPERTIES
		REQUIRED_FILES ${Criterion_ddsc_config_simple_udp_file}
		ENVIRONMENT "${CMAKE_PROJECT_NAME_CAPS}_URI=${Criterion_ddsc_config_simple_udp_uri};MAX_PARTICIPANTS=${Criterion_ddsc_config_simple_udp_max_participants}"

)
configure_file("config_env.h.in" "config_env.h")
