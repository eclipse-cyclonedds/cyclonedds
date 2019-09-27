#
# Copyright(c) 2006 to 2019 ADLINK Technology Limited and others
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

add_mpt_executable(mpt_discstress
    "procs/createwriter.c"
    "discstress.c")
idlc_generate(mpt_discstress_discstressdata_lib "procs/createwriterdata.idl")
target_link_libraries(mpt_discstress PRIVATE mpt_discstress_discstressdata_lib)
