#
# Copyright(c) 2021 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
if(CMAKE_CROSSCOMPILING)
  find_program(IDLC_EXECUTABLE idlc REQUIRED)
  add_executable(CycloneDDS::idlc IMPORTED GLOBAL)
  set_property(TARGET CycloneDDS::idlc PROPERTY IMPORTED_LOCATION ${IDLC_EXECUTABLE})
else()
  include(CheckIncludeFile)

  check_include_file(getopt.h HAVE_GETOPT_H)

  configure_file(src/config.h.in config.h)

  set(headers
    src/descriptor.h
    src/generator.h
    src/options.h
    src/plugin.h
    include/idlc/generator.h
    ${CMAKE_CURRENT_BINARY_DIR}/config.h)
  set(sources
    src/idlc.c
    src/plugin.c
    src/options.c
    src/generator.c
    src/descriptor.c
    src/types.c)
  add_executable(idlc ${sources} ${headers})

  if(MSVC)
    # ignore warnings C6255 and 6263 about _alloca
    target_compile_options(idlc PRIVATE /wd6255 /wd6263)
  endif()
  target_link_libraries(idlc PRIVATE idl idlpp ${CMAKE_DL_LIBS})
  target_include_directories(
    idlc PRIVATE
      include
      ${CMAKE_CURRENT_BINARY_DIR}
      $<BUILD_INTERFACE:$<TARGET_PROPERTY:ddsc,INTERFACE_INCLUDE_DIRECTORIES>>)

  if(WIN32 OR NOT HAVE_GETOPT_H)
    # use getopt.h from ddsrt
    file(READ "${CMAKE_SOURCE_DIR}/src/ddsrt/include/getopt.h.in" getopt_h)
    # remove occurrences of DDS_EXPORT
    string(REGEX REPLACE "\n[ \t]*DDS_EXPORT[ \t]+" "\n" getopt_h "${getopt_h}")
    # remove dds/* includes
    string(REGEX REPLACE "\n[ \t]*#[ \t]*include[ \t]+[<\"]dds/[^\n]*" "" getopt_h "${getopt_h}")
    # generate getopt.h
    file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/getopt.h" CONTENT "${getopt_h}")
    target_include_directories(idlc PRIVATE "${CMAKE_CURRENT_BINARY_DIR}")
    # add getopt.c
    configure_file(
      "${CMAKE_SOURCE_DIR}/src/ddsrt/src/getopt.c"
      "${CMAKE_CURRENT_BINARY_DIR}/getopt.c"
      COPYONLY)
    target_sources(idlc PRIVATE ${CMAKE_CURRENT_BINARY_DIR}/getopt.c)
  endif()

  add_executable(${PROJECT_NAME}::idlc ALIAS idlc)

  install(
    DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/idlc"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
    COMPONENT dev
    FILES_MATCHING PATTERN "*.h")

  install(
    TARGETS idlc
    EXPORT "${CMAKE_PROJECT_NAME}"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT dev)
  if (MSVC)
    install(FILES $<TARGET_PDB_FILE:idlc>
      DESTINATION "${CMAKE_INSTALL_BINDIR}"
      COMPONENT dev
      OPTIONAL
    )
  endif()

  install(
    FILES Generate.cmake
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}/idlc"
    COMPONENT dev)
endif()

include(Generate.cmake)
