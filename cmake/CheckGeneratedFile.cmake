#
# Copyright(c) 2026 ZettaScale Technology and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

if(NOT DEFINED GENERATED_FILE)
  message(FATAL_ERROR "GENERATED_FILE is not set")
endif()
if(NOT DEFINED CHECKED_IN_FILE)
  message(FATAL_ERROR "CHECKED_IN_FILE is not set")
endif()

file(READ "${GENERATED_FILE}" _generated_data)
file(READ "${CHECKED_IN_FILE}" _checked_in_data)
string(REPLACE "\r" "" _generated_data "${_generated_data}")
string(REPLACE "\r" "" _checked_in_data "${_checked_in_data}")

if(NOT _generated_data STREQUAL _checked_in_data)
  message(FATAL_ERROR
    "Generated file differs from checked-in file:\n"
    "  generated:  ${GENERATED_FILE}\n"
    "  checked in: ${CHECKED_IN_FILE}\n"
    "Please regenerate the checked-in file.")
endif()
