#
# Copyright(c) 2021 to 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
include(GenerateExportHeader)
include(HashUtilities)

find_package(Threads QUIET REQUIRED)

set(binary_dir "${CMAKE_CURRENT_BINARY_DIR}")
set(source_dir "${CMAKE_CURRENT_SOURCE_DIR}")

configure_file(include/idl/version.h.in include/idl/version.h @ONLY)

check_hashes(
  _bison_hash_correct
  HASH_FILES
    src/parser.y
  APPEND_FILES
    ${CMAKE_CURRENT_LIST_DIR}/src/parser.c
    ${CMAKE_CURRENT_LIST_DIR}/src/parser.h
)

if(NOT ${_bison_hash_correct})
  find_package(BISON 3.0.4)

  if(BISON_FOUND)
    message(STATUS "Running Bison, parser.y was updated.")
    message(STATUS "${source_dir}")

    file(REMOVE src/parser.c)
    file(REMOVE src/parser.h)

    # Run bison targetting an intermediate file
    bison_target(bison_t src/parser.y ${binary_dir}/parser.c DEFINES_FILE ${binary_dir}/parser.h)

    # Run postprocessing (copying/hashing)
    add_custom_command(
      OUTPUT
        ${binary_dir}/src/parser.c
        ${binary_dir}/src/parser.h
      COMMAND ${CMAKE_COMMAND} ARGS
        "-Dsource_dir=${source_dir}"
        "-Dbinary_dir=${binary_dir}"
        "-DMAIN_PROJECT_DIR=${CMAKE_SOURCE_DIR}"
        -P ${CMAKE_CURRENT_LIST_DIR}/postprocess-parser.cmake
      DEPENDS ${binary_dir}/parser.c ${binary_dir}/parser.h
    )
  else()
    message(FATAL_ERROR "parser.y was updated but parser.c not regenerated, and no bison available!")
  endif()
else()
  file(COPY ${source_dir}/src/parser.c DESTINATION ${binary_dir}/src)
  file(COPY ${source_dir}/src/parser.h DESTINATION ${binary_dir}/src)
endif()

set_source_files_properties(${binary_dir}/src/parser.c ${binary_dir}/src/parser.h PROPERTIES GENERATED TRUE)

set(headers
  include/idl/expression.h
  include/idl/file.h
  include/idl/print.h
  include/idl/processor.h
  include/idl/retcode.h
  include/idl/scope.h
  include/idl/stream.h
  include/idl/string.h
  include/idl/symbol.h
  include/idl/tree.h
  include/idl/vector.h
  include/idl/visit.h
  include/idl/heap.h
  ${binary_dir}/include/idl/version.h)

set(sources
  src/symbol.c
  src/directive.c
  src/expression.c
  src/file.c
  src/processor.c
  src/scanner.c
  src/string.c
  src/annotation.c
  src/scope.c
  src/string.c
  src/tree.c
  src/vector.c
  src/visit.c
  src/print.c
  src/keylist.c
  src/hashid.c
  src/parser.c
  src/heap.c
  src/annotation.h
  src/directive.h
  src/expression.h
  src/file.h
  src/hashid.h
  src/parser.h
  src/scanner.h
  src/scope.h
  src/symbol.h
  src/tree.h
  src/keylist.h)

# import generic utility sources from ddsrt to avoid duplication
# NOTE: an additional INTERFACE library defined in ddsrt would be ideal, but
#       the GENERATED property is not propagated before 3.20. using an OBJECT
#       library and propagating the object files through an INTERFACE library
#       can be done, but a proper export header can only be generated from the
#       final library.
#       see: https://gitlab.kitware.com/cmake/cmake/-/issues/18399
set(script "${PROJECT_SOURCE_DIR}/src/ddsrt/rename.cmake")
set(templates
  include/dds/ddsrt/misc.h
  include/dds/ddsrt/attributes.h
  include/dds/ddsrt/endian.h
  include/dds/ddsrt/md5.h
  src/md5.c)
# FIXME: move general functionality to ddsrt

foreach(source ${templates})
  set(input "${PROJECT_SOURCE_DIR}/src/ddsrt/${source}")
  # map include path. i.e. map include/dds/ddsrt to include/idl
  string(REGEX REPLACE "(/dds(rt)?)+" "/idl" source "${source}")
  set(output "${binary_dir}/${source}")
  # process source file. i.e. replace dds and ddsrt with idl
  add_custom_command(
    OUTPUT ${output}
    COMMAND "${CMAKE_COMMAND}" -DNAMESPACE=idl
                               -DINPUT_FILE=${input}
                               -DOUTPUT_DIRECTORY=${binary_dir}
                               -DOUTPUT_FILE=${output}
                               -P ${script}
    DEPENDS ${input} ${script})
  if(source MATCHES "^include/.*\\.h")
    list(APPEND headers ${output})
  else()
    list(APPEND sources ${output})
  endif()
endforeach()

add_library(idl SHARED ${headers} ${sources})

set_target_properties(
  idl PROPERTIES
  OUTPUT_NAME "cycloneddsidl"
  VERSION ${PROJECT_VERSION}
  SOVERSION ${PROJECT_VERSION_MAJOR})

generate_export_header(idl EXPORT_FILE_NAME include/idl/export.h)


target_include_directories(
  idl
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>"
  INTERFACE
    "$<INSTALL_INTERFACE:include>")

target_link_libraries(idl PRIVATE Threads::Threads)

install(
  DIRECTORY
    "${source_dir}/include/idl"
    "${binary_dir}/include/idl"
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  COMPONENT dev
  FILES_MATCHING PATTERN "*.h")

install(
  TARGETS idl
  EXPORT "${CMAKE_PROJECT_NAME}"
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT lib
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT lib)

if (INSTALL_PDB)
  install(FILES $<TARGET_PDB_FILE:idl>
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT dev
    OPTIONAL)
endif()

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
