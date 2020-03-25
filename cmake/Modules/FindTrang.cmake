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
``Trang_TRANG_DEPENDS``
  Build dependencies needed to run Trang
``Trang_TRANG_JAR``
  Path to the Trang jar file - may be left unset
``Trang_TRANG_EXECUTABLE``
  Path to the Trang executable file - may be left unset

Cache Variables
^^^^^^^^^^^^^^^

The following cache variables may also be set:

``Trang_DOWNLOAD``
  Whether to download Trang the internet. Default = iff no trang executable found

#]=======================================================================]

find_program(trang_executable trang)

if(${trang_executable})
  set(download_default 0)
else()
  set(download_default 1)
endif()

option(Trang_DOWNLOAD "Should we download Trang?" ${download_default})

if(Trang_DOWNLOAD)
  find_package(Java REQUIRED COMPONENTS Runtime)

  set(TRANG_VERSION 20181222)

  include(ExternalProject)
  ExternalProject_Add(Trang_release
    URL "https://github.com/relaxng/jing-trang/releases/download/V${TRANG_VERSION}/trang-${TRANG_VERSION}.zip"
    URL_HASH MD5=51b4ec4fdcb027cfdb6a43066cf2e7a1
    PREFIX "${CMAKE_CURRENT_BINARY_DIR}/Trang_release"
    CONFIGURE_COMMAND ""
    BUILD_COMMAND ""
    INSTALL_COMMAND ""
    )

  set(Trang_TRANG_EXECUTABLE "")
  set(Trang_TRANG_JAR "${CMAKE_CURRENT_BINARY_DIR}/Trang_release/src/Trang_release/trang.jar")
  set(Trang_TRANG_CMD ${Java_JAVA_EXECUTABLE} -jar ${Trang_TRANG_JAR})
  set(Trang_TRANG_DEPENDS Trang_release)
else()
  execute_process(
    COMMAND "${trang_executable}"
    ERROR_VARIABLE trang_output
  )
  if(trang_output MATCHES "Trang version ([0-9]+)")
    set(TRANG_VERSION "${CMAKE_MATCH_1}")
  else()
    message(ERROR "Could not parse version from Trang output: '${trang_output}'")
    set(TRANG_VERSION "0")
  endif()

  set(Trang_TRANG_EXECUTABLE "${trang_executable}")
  set(Trang_TRANG_JAR "")
  set(Trang_TRANG_CMD "${trang_executable}")
  set(Trang_TRANG_DEPENDS "")
endif()

include(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(
  Trang
  REQUIRED_VARS Trang_TRANG_CMD
  VERSION_VAR TRANG_VERSION
)
