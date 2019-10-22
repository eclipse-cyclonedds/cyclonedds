#
# Copyright(c) 2019 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#
include(${MPT_CMAKE})

idlc_generate(mpt_rwdata_lib "procs/rwdata.idl")

set(sources_qosmatch
    "procs/rw.c"
    "qosmatch.c")
add_mpt_executable(mpt_qosmatch ${sources_qosmatch})

target_include_directories(
mpt_qosmatch PRIVATE
"$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../../../core/ddsi/include>")

target_link_libraries(mpt_qosmatch PRIVATE mpt_rwdata_lib)

set(sources_ppuserdata	
    "procs/ppud.c"
    "ppuserdata.c")
add_mpt_executable(mpt_ppuserdata ${sources_ppuserdata})
target_link_libraries(mpt_ppuserdata PRIVATE mpt_rwdata_lib)
