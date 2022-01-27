#
# Copyright(c) 2020 ADLINK Technology Limited and others
#
# This program and the accompanying materials are made available under the
# terms of the Eclipse Public License v. 2.0 which is available at
# http://www.eclipse.org/legal/epl-2.0, or the Eclipse Distribution License
# v. 1.0 which is available at
# http://www.eclipse.org/org/documents/edl-v10.php.
#
# SPDX-License-Identifier: EPL-2.0 OR BSD-3-Clause
#

if (BUILD_DDSCONF)
  add_executable(ddsconf
    ddsconf.c rnc.c md.c xsd.c defconfig.c
    ${CMAKE_CURRENT_LIST_DIR}/../../core/ddsi/src/q_config.c
    )

  target_link_libraries(ddsconf PRIVATE ddsrt-internal compat)

  target_include_directories(ddsconf
    PRIVATE
    "${CMAKE_CURRENT_BINARY_DIR}/include"
    "${CMAKE_CURRENT_LIST_DIR}/../../core/ddsi/include")

  add_executable(${PROJECT_NAME}::ddsconf ALIAS ddsconf)

  install(
    TARGETS ddsconf
    EXPORT "${CMAKE_PROJECT_NAME}"
    DESTINATION "${CMAKE_INSTALL_BINDIR}"
    COMPONENT dev)

  if (CMAKE_CROSSCOMPILING)
    find_program(_executable ddsconf REQUIRED NO_CMAKE_FIND_ROOT_PATH)

    # REQUIRED is not working until cmake 3.18 so it might still not exist
    if (NOT _executable)
      message(FATAL_ERROR "Could not find a native ddsconf executable, please make sure it is available on PATH")
    endif()

    add_executable(ddsconf-native IMPORTED GLOBAL)
    set_property(TARGET ddsconf-native PROPERTY IMPORTED_LOCATION "${_executable}")
    message(STATUS "Using imported native ddsconf ${_executable}")
  else()
    add_executable(ddsconf-native ALIAS ddsconf)
  endif()
else()
  # Note, no if crosscompiling statement here, ddsconf is _required_
  # so if you don't BUILD_DDSCONF it has to be found
  find_program(_executable ddsconf REQUIRED NO_CMAKE_FIND_ROOT_PATH)

  # REQUIRED is not working until cmake 3.18 so it might still not exist
  if (NOT _executable)
    message(FATAL_ERROR "Could not find a native ddsconf executable, please make sure it is available on PATH")
  endif()

  add_executable(ddsconf-native IMPORTED GLOBAL)
  set_property(TARGET ddsconf-native PROPERTY IMPORTED_LOCATION "${_executable}")
  message(STATUS "Using imported native ddsconf ${_executable}")
endif()
