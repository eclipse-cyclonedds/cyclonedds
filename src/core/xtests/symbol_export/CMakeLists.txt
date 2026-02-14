#
# Copyright(c) 2022 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

if(WIN32)
    set(DDSC_LIBRARY_PREFIX "")
    set(DDSC_LIBRARY_POSTFIX "dll")
else()
    set(DDSC_LIBRARY_PREFIX "lib")
    set(DDSC_LIBRARY_POSTFIX "so")
endif()

set(DDSC_LIBRARY_FILEPATH ${DDSC_LIBRARY_PREFIX}ddsc.${DDSC_LIBRARY_POSTFIX})

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/ddsc_loader.h.in 
               ${CMAKE_CURRENT_BINARY_DIR}/include/symbol_export/ddsc_loader.h 
               @ONLY
)

add_executable(symbol_export_test symbol_export.c 
    ${CMAKE_CURRENT_BINARY_DIR}/include/symbol_export/ddsc_loader.h
    ${CMAKE_BINARY_DIR}/src/ddsrt/include/dds/config.h # Defines enabled options
    ${CMAKE_BINARY_DIR}/src/ddsrt/include/dds/features.h 
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../ddsrt/include/dds/ddsrt/arch.h # Defines DDSRT_64BIT
)

target_include_directories(symbol_export_test PRIVATE 
    ${CMAKE_CURRENT_BINARY_DIR}/include
    ${CMAKE_BINARY_DIR}/src/ddsrt/include
    ${CMAKE_CURRENT_SOURCE_DIR}/../../../ddsrt/include
)

target_compile_definitions(symbol_export_test PRIVATE
    $<$<BOOL:DDSRT_HAVE_THREAD_SETNAME>:DDSRT_HAVE_THREAD_SETNAME>
    $<$<BOOL:DDSRT_HAVE_THREAD_LIST>:DDSRT_HAVE_THREAD_LIST>
)