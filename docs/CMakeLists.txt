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
include(FindPerl)
if(PERL_FOUND)
  add_custom_command(
    OUTPUT
      options.md cyclonedds.rnc
    COMMAND
      ${PERL_EXECUTABLE} -w "${CMAKE_CURRENT_SOURCE_DIR}/makernc.pl" "${CMAKE_CURRENT_SOURCE_DIR}/../src/core/ddsi/src/q_config.c" options.md cyclonedds.rnc
    COMMAND
      ${PERL_EXECUTABLE} -w "${CMAKE_CURRENT_SOURCE_DIR}/compare.pl" options.md "${CMAKE_CURRENT_SOURCE_DIR}/manual/options.md"
    COMMAND
      ${PERL_EXECUTABLE} -w "${CMAKE_CURRENT_SOURCE_DIR}/compare.pl" cyclonedds.rnc "${CMAKE_CURRENT_SOURCE_DIR}/../etc/cyclonedds.rnc"
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/makernc.pl"
      "${CMAKE_CURRENT_SOURCE_DIR}/../src/core/ddsi/src/q_config.c")
  add_custom_target(options_doc ALL DEPENDS "options.md" "cyclonedds.rnc")

  find_package(Java COMPONENTS Runtime)
  if(JAVA_FOUND AND EXISTS "${TRANG_PATH}" OR EXISTS "$ENV{TRANG}")
    if(NOT EXISTS "${TRANG_PATH}" AND EXISTS "$ENV{TRANG}")
      message(STATUS "Setting TRANG_PATH to $ENV{TRANG}")
      set(TRANG_PATH "$ENV{TRANG}" CACHE FILEPATH "Location of 'trang' for converting XML schemas" FORCE)
    endif()
    add_custom_command(
      OUTPUT
        cyclonedds.xsd
      COMMAND
        ${Java_JAVA_EXECUTABLE} -jar "${TRANG_PATH}" -I rnc -O xsd cyclonedds.rnc cyclonedds.xsd
      COMMAND
        ${PERL_EXECUTABLE} -w "${CMAKE_CURRENT_SOURCE_DIR}/compare.pl" cyclonedds.xsd "${CMAKE_CURRENT_SOURCE_DIR}/../etc/cyclonedds.xsd"
      DEPENDS
        "cyclonedds.rnc")
    add_custom_target(options_xsd ALL DEPENDS "cyclonedds.xsd")
  else()
    message(STATUS "Java or not trang not found: not converting/checking RNC to XSD")
  endif()
else()
  message(STATUS "perl not found: not generating/checking options documentation and RNC")
endif()

add_subdirectory(manual)
