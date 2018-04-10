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
if (NOT TARGET GetTime)
  add_library(GetTime INTERFACE)
endif()

include(CheckLibraryExists)

# First check whether libc has clock_gettime
check_library_exists(c clock_gettime "" HAVE_CLOCK_GETTIME_IN_C)

if(NOT HAVE_CLOCK_GETTIME_IN_C)
  # Before glibc 2.17, clock_gettime was in librt
  check_library_exists(rt clock_gettime "time.h" HAVE_CLOCK_GETTIME_IN_RT)
  if (HAVE_CLOCK_GETTIME_IN_RT)
    target_link_libraries(GetTime INTERFACE rt)
  endif()
endif()

