#
# Copyright(c) 2006 to 2018 ADLINK Technology Limited and others
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

set(sources
    "procs/rw.c"
    "qosmatch.c")

add_compile_options("-I${PROJECT_SOURCE_DIR}/core/ddsi/include")
add_mpt_executable(mpt_qosmatch ${sources})

idlc_generate(mpt_qosmatch_rwdata_lib "procs/rwdata.idl")
target_link_libraries(mpt_qosmatch PRIVATE mpt_qosmatch_rwdata_lib)

