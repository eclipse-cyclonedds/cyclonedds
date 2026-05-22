#
# Copyright(c) 2026 IntervalZero, Inc
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

###############################################################################
##
## CMake configuration file that describes the RTX64 platform.
##
###############################################################################

##
## Variable used to know that we are building for RTX64
##
set(RTX64 TRUE)

##
## Prevent CMake from using the UNIX prefix "-l" for the linker
##
set(CMAKE_LINK_LIBRARY_FLAG "")

##
## RTX64 file extensions
##
set(CMAKE_LINK_LIBRARY_SUFFIX ".lib")
set(CMAKE_IMPORT_LIBRARY_PREFIX "")
set(CMAKE_IMPORT_LIBRARY_SUFFIX ".lib")
set(CMAKE_STATIC_LIBRARY_PREFIX "")
set(CMAKE_STATIC_LIBRARY_SUFFIX ".lib")
set(CMAKE_SHARED_LIBRARY_PREFIX "")
set(CMAKE_SHARED_LIBRARY_SUFFIX ".rtdll")
set(CMAKE_EXECUTABLE_SUFFIX ".rtss")

