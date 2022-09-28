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

function(filter_files)
    set(one_value_keywords FIND REPLACE)
    set(multi_value_keywords FILES)
    cmake_parse_arguments(_filter_files "" "${one_value_keywords}" "${multi_value_keywords}" "" ${ARGN})

    foreach(_file ${_filter_files_FILES})
        file(READ "${_file}" _file_data)
        string(REPLACE "${_filter_files_FIND}" "${_filter_files_REPLACE}" _file_data "${_file_data}")
        file(WRITE "${_file}" "${_file_data}")
    endforeach()
endfunction()


function(hash_of_file OUTVAR HASH_FILE)
    # Hashing routine, remove \r to not get confused on windows if git changes the line endings.
    file(READ "${_hash_file}" _hash_data)
    string(REPLACE "\r" "" _hash_data "${_hash_data}")
    string(SHA1 _hash "${_hash_data}")
    set(${OUTVAR} "${_hash}" PARENT_SCOPE)
endfunction()


function(generate_hash_text OUTVAR HASH_FILE PREFIX POSTFIX)
    hash_of_file(_hash "${HASH_FILE}")
    get_filename_component(_fname "${_hash_file}" NAME)
    set(${OUTVAR} "${PREFIX} generated from ${_fname}[${_hash}] ${POSTFIX}" PARENT_SCOPE)
endfunction()


function(CHECK_HASH OUTVAR HASH_FILE APPEND_FILE)
    if(NOT EXISTS "${APPEND_FILE}")
        set(${OUTVAR} 0 PARENT_SCOPE)
        return()
    endif()

    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${HASH_FILE} ${APPEND_FILE})

    generate_hash_text(_gen_hash_line "${HASH_FILE}" "" "")

    file(READ "${APPEND_FILE}" _file_data)
    string(FIND "${_file_data}" "${_gen_hash_line}" _position)

    if (${_position} EQUAL -1)
        set(${OUTVAR} 0 PARENT_SCOPE)
    else()
        set(${OUTVAR} 1 PARENT_SCOPE)
    endif()
endfunction()


function(CHECK_HASHES OUTVAR)
    set(multi_value_keywords HASH_FILES APPEND_FILES)
    cmake_parse_arguments(_check_hashes "" "" "${multi_value_keywords}" "" ${ARGN})

    set(${OUTVAR} 1 PARENT_SCOPE)
    foreach(_hash_file ${_check_hashes_HASH_FILES})
        foreach(_append_file ${_check_hashes_APPEND_FILES})
            check_hash(_test ${_hash_file} ${_append_file})
            if (NOT ${_test})
                set(${OUTVAR} 0 PARENT_SCOPE)
            endif()
        endforeach()
    endforeach()
endfunction()


function(APPEND_HASHES)
    set(one_value_keywords PREFIX POSTFIX BEFORE)
    set(multi_value_keywords HASH_FILES APPEND_FILES)
    cmake_parse_arguments(_append_hashes "" "${one_value_keywords}" "${multi_value_keywords}" "" ${ARGN})

    if (NOT "${_append_hashes_BEFORE}" STREQUAL "")
        foreach(_file ${_append_hashes_APPEND_FILES})
            file(APPEND "${_file}" "\n${_append_hashes_BEFORE}\n")
        endforeach()
    endif()

    foreach(_hash_file ${_append_hashes_HASH_FILES})
        generate_hash_text(hash_line "${_hash_file}" "${_append_hashes_PREFIX}" "${_append_hashes_POSTFIX}")

        foreach(_file ${_append_hashes_APPEND_FILES})
            file(APPEND "${_file}" "${hash_line}\n")
        endforeach()
    endforeach()
endfunction()
