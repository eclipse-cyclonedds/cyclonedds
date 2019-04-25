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
    "procs/hello.c"
    "helloworld.c"
    "multi.c")

add_mpt_executable(mpt_basic ${sources})

idlc_generate(mpt_basic_helloworlddata_lib "procs/helloworlddata.idl")
target_link_libraries(mpt_basic PRIVATE mpt_basic_helloworlddata_lib)

