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
if(CONFIG_CYCLONEDDS)
  # Build a static library only, without host tools and without anything that
  # could pick up OpenSSL or Iceoryx from the host.  Cyclone's configure-time
  # feature checks cannot compile against Zephyr's headers (those need headers
  # that are only generated during the build), so the answers for Zephyr's
  # POSIX layer are given here instead of probed.
  set(settings
    WITH_ZEPHYR ON BUILD_SHARED_LIBS OFF BUILD_IDLC OFF BUILD_DDSPERF OFF
    BUILD_TESTING OFF BUILD_EXAMPLES OFF ENABLE_LTO OFF ENABLE_SSL OFF
    ENABLE_SECURITY OFF ENABLE_ICEORYX OFF ENABLE_ICEORYX2 OFF
    ENABLE_SOURCE_SPECIFIC_MULTICAST OFF ENABLE_IPV6 OFF
    CMAKE_HAVE_LIBC_PTHREAD 1 HAVE_GETOPT_H 1 DDSRT_HAVE_GETHOSTNAME 1
    DDSRT_HAVE_INET_NTOP 1 DDSRT_HAVE_INET_PTON 1 DDSRT_HAVE_GETADDRINFO 1
    DDSRT_HAVE_GETHOSTBYNAME_R 0 DDSRT_HAVE_CONDATTR_SETCLOCK 1)
  if(CONFIG_NET_IPV6)
    list(APPEND settings ENABLE_IPV6 ON HAVE_SIZEOF_SOCKADDR_IN6 TRUE SIZEOF_SOCKADDR_IN6 22)
  endif()
  while(settings)
    list(POP_FRONT settings name value)
    set(${name} ${value} CACHE INTERNAL "")
  endwhile()
  set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
  set(CMAKE_SKIP_INSTALL_RULES ON)

  add_subdirectory(${ZEPHYR_CURRENT_MODULE_DIR} cyclonedds)

  # ddsc is a plain CMake library: give it Zephyr's include paths and compile
  # options, make sure generated headers exist first, and add it to the link.
  # Zephyr enables -Wdouble-promotion (and twister adds -Werror) but Cyclone DDS
  # has a few harmless float promotions.  Interface compile options are added in
  # link order, so linking this after zephyr_interface puts the -Wno after the -W.
  add_library(cyclonedds_zephyr_flags INTERFACE)
  target_compile_options(cyclonedds_zephyr_flags INTERFACE -Wno-double-promotion)
  target_link_libraries(ddsc PRIVATE zephyr_interface cyclonedds_zephyr_flags)
  target_compile_definitions(ddsc PRIVATE _POSIX_C_SOURCE=200809L
    CYCLONEDDS_THREAD_COUNT=${CONFIG_CYCLONEDDS_THREAD_COUNT}
    CYCLONEDDS_THREAD_STACK_SIZE=${CONFIG_CYCLONEDDS_THREAD_STACK_SIZE})
  add_dependencies(ddsc zephyr_generated_headers)
  zephyr_append_cmake_library(ddsc)
  zephyr_include_directories($<TARGET_PROPERTY:ddsc,INTERFACE_INCLUDE_DIRECTORIES>)

  # Host-only helpers and tools that are not buildable for the target.
  set_target_properties(ddsrt-internal dyntypelib xmltype PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()
