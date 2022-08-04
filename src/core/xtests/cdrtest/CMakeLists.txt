#
# Copyright(c) 2019 to 2021 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
cmake_minimum_required(VERSION 3.16)
project(CDRTest LANGUAGES C)
list(INSERT CMAKE_MODULE_PATH 0 ${CMAKE_CURRENT_SOURCE_DIR}/cmake)

find_package(CycloneDDS REQUIRED)
find_package(OpenSplice COMPONENTS kernel sac REQUIRED)
find_package(Perl REQUIRED)

set(PERL_SCRIPT ${CMAKE_SOURCE_DIR}/cdrtest.pl)
set(TEST_MAIN ${CMAKE_CURRENT_BINARY_DIR}/cdrtest-main.c)
set(TEST_IDL ${CMAKE_CURRENT_BINARY_DIR}/cdrtest.idl)

add_custom_command(
    OUTPUT  ${TEST_IDL} ${TEST_MAIN}
    COMMAND ${PERL_EXECUTABLE}
    ARGS    ${PERL_SCRIPT}
    DEPENDS ${PERL_SCRIPT})

# This is a convenience function, provided by the CycloneDDS package,
# that will supply a library target related the the given idl file.
# In short, it takes the idl file, generates the source files with
# the proper data types and compiles them into a library.
idlc_generate(TARGET cdds_type_lib FILES ${TEST_IDL})

#idl compile using ospl idlpp
osplidl_generate(ospl_type_lib ${TEST_IDL})

#transform the generated output (filter out unused code)
set(OSPL_TYPES_C ${CMAKE_CURRENT_BINARY_DIR}/cdrtestSplDcps_s.c)
set(OSPL_TYPES_H ${CMAKE_CURRENT_BINARY_DIR}/cdrtestSplDcps_s.h)

add_custom_command(
    OUTPUT  ${OSPL_TYPES_C}
    COMMAND ${PERL_EXECUTABLE}
    ARGS    ${CMAKE_SOURCE_DIR}/ospl_output_transform_c.pl ${CMAKE_CURRENT_BINARY_DIR}/ospl/cdrtestSplDcps.c > ${OSPL_TYPES_C}
    DEPENDS ospl_type_lib ${CMAKE_SOURCE_DIR}/ospl_output_transform_c.pl)

add_custom_command(
    OUTPUT  ${OSPL_TYPES_H}
    COMMAND ${PERL_EXECUTABLE}
    ARGS    ${CMAKE_SOURCE_DIR}/ospl_output_transform_h.pl ${CMAKE_CURRENT_BINARY_DIR}/ospl/cdrtestSplDcps.h > ${OSPL_TYPES_H}
    DEPENDS ospl_type_lib ${CMAKE_SOURCE_DIR}/ospl_output_transform_h.pl)

add_custom_target(ospl_types DEPENDS ${OSPL_TYPES_C} ${OSPL_TYPES_H})

# Create the main test executable
add_executable(cdrtest ${TEST_MAIN} ${OSPL_TYPES_C})
add_dependencies(cdrtest ospl_types)
target_link_libraries(cdrtest cdds_type_lib OpenSplice::sac CycloneDDS::ddsc)
