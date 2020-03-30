# Copyright(c) 2020 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

#[=======================================================================[.rst:
FindTrang
---------
Finds Trang, an xml schema transpiler

Result Variables
^^^^^^^^^^^^^^^^

This will define the following variables:

``Trang_FOUND``
  True if the system has the Foo library.
``Trang_VERSION``
  The version of the Foo library which was found.
``Trang_TRANG_CMD``
  List of command line args to run Trang.
``Trang_TRANG_EXECUTABLE``
  Path to the Trang executable file

#]=======================================================================]

find_program(Trang_TRANG_EXECUTABLE trang)

execute_process(
  COMMAND "${Trang_TRANG_EXECUTABLE}"
  ERROR_VARIABLE trang_output
)
if(trang_output MATCHES "Trang version ([0-9]+)")
  set(TRANG_VERSION "${CMAKE_MATCH_1}")
else()
  message(ERROR "Could not parse version from Trang output: '${trang_output}'")
  set(TRANG_VERSION "0")
endif()

set(Trang_TRANG_CMD "${Trang_TRANG_EXECUTABLE}")

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
  Trang
  REQUIRED_VARS Trang_TRANG_CMD
  VERSION_VAR TRANG_VERSION
)
