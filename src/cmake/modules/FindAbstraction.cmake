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

if (NOT TARGET Abstraction)
  add_library(Abstraction INTERFACE)
endif()

# Link with the platform-specific threads library that find_package provides us
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
find_package(Threads REQUIRED)
target_link_libraries(Abstraction INTERFACE Threads::Threads)

if(WIN32)
  # Link with WIN32 core-libraries
  target_link_libraries(Abstraction INTERFACE wsock32 ws2_32 iphlpapi)

  # Many of the secure versions provided by Microsoft have failure modes
  # which are not supported by our abstraction layer, so efforts trying
  # to use the _s versions aren't typically the proper solution and C11
  # (which contains most of the secure versions) is 'too new'. So we rely
  # on static detection of misuse instead of runtime detection, so all
  # these warnings can be disabled on Windows.
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_WINSOCK_DEPRECATED_NO_WARNINGS) #Disabled warnings for deprecated Winsock 2 API calls in general
  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE) #Disabled warnings for deprecated POSIX names
elseif(UNIX AND NOT APPLE)
  if("${CMAKE_SYSTEM_PROCESSOR}" STREQUAL "x86_64")
    # Shared libs will have this by default. Static libs need it too on x86_64.
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
  endif()
  find_package(GetTime REQUIRED)
  target_link_libraries(Abstraction INTERFACE GetTime)
endif()
