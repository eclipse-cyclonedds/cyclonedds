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
set(CMAKE_SYSTEM_NAME VxWorks)
set(CMAKE_SYSTEM_PROCESSOR PENTIUM4)

set(WIND_HOME "/path/to/WindRiver")
set(WIND_PROCESSOR_TYPE "pentium")

# Binaries are named e.g. ccpentium or ccarm
set(CMAKE_C_COMPILER ${WIND_HOME}/gnu/4.3.3-vxworks-6.9/x86-linux2/bin/cc${WIND_PROCESSOR_TYPE})
set(CMAKE_CXX_COMPILER ${WIND_HOME}/gnu/4.3.3-vxworks-6.9/x86-linux2/bin/c++${WIND_PROCESSOR_TYPE})
set(CMAKE_AR ${WIND_HOME}/gnu/4.3.3-vxworks-6.9/x86-linux2/bin/ar${WIND_PROCESSOR_TYPE})

set(WIND_PROGRAM_PATH ${WIND_HOME}/vxworks-6.9/host/x86-linux2/bin;${WIND_BASE}/gnu/4.3.3-vxworks-6.9/x86-linux2/bin)
set(WIND_LIBRARY_PATH ${WIND_HOME}/target/lib/${WIND_PROCESSOR_TYPE}/${CMAKE_SYSTEM_PROCESSOR}/common)
set(WIND_INCLUDE_PATH ${WIND_HOME}/vxworks-6.9/target/h)

set(CMAKE_FIND_ROOT_PATH ${WIND_PROGRAM_PATH};${WIND_LIBRARY_PATH};${WIND_INCLUDE_PATH})

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM BOTH)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

